// SPDX-License-Identifier: GPL-2.0
/*
 * test_comport.cpp  -  host-side known-answer test for CSTGComPort's
 * confirmed real methods (see ../include/oa_comport.h /
 * ../src/init/comport.cpp). `Initialize` itself is not reconstructed
 * in this pass (see that header's own note) and so isn't exercised
 * here.
 *
 * Mocks the raw hardware I/O layer (stg_inb/stg_outb/
 * stg_local_irq_save/stg_local_irq_restore/rtwrap_shutdown_irq/
 * rtwrap_release_irq), exercising:
 *   [1] TransmitFifo::WriteByte/GetByteToTransmit -- the shared ring
 *       buffer's producer/consumer sides, including the confirmed
 *       real "drop the byte when full" behavior.
 *   [2] Cleanup() -- confirmed real irqEnabled-guarded IRQ teardown.
 *   [3] SetBaudRate()/SetDTR()/IsHardwareTransmitterEmpty() -- confirmed
 *       real 16550 register semantics via the mocked I/O bus.
 *   [4] TriggerInterrupt() -- the confirmed real poll/drain/transmit
 *       loop, including the RX-drain path (dispatching to
 *       OnByteReceived) and the TX-burst path (respecting the
 *       confirmed capacity-as-burst-limit behavior).
 */

#include <cstdio>
#include <cstring>
#include "oa_comport.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-50s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* Mock 16550 register file, indexed by (port - baseIoPort). */
static unsigned char g_regs[8];
static unsigned int g_baseIoPort = 0x3f8;
static int g_inbCalls, g_outbCalls;
static unsigned int g_lastOutbPort;
static unsigned char g_lastOutbValue;

extern "C" {

unsigned char stg_inb(unsigned int port)
{
	g_inbCalls++;
	unsigned int reg = port - g_baseIoPort;
	unsigned char value = g_regs[reg];
	/* Matches real 16550 semantics: reading the RX data register (+0)
	 * clears LSR's Data-Ready bit as a side effect, so a mocked "one
	 * byte pending" scenario terminates instead of looping forever. */
	if (reg == 0)
		g_regs[5] &= (unsigned char)~0x1;
	return value;
}

void stg_outb(unsigned int port, unsigned char value)
{
	g_outbCalls++;
	g_lastOutbPort = port;
	g_lastOutbValue = value;
	g_regs[port - g_baseIoPort] = value;
}

static int g_irqSaveCalls, g_irqRestoreCalls;
unsigned long stg_local_irq_save(void) { g_irqSaveCalls++; return 0xdead; }
void stg_local_irq_restore(unsigned long savedFlags)
{
	g_irqRestoreCalls++;
	check_eq("  (irq_restore) got the same flags irq_save returned", (long)savedFlags, 0xdead);
}

static int g_shutdownIrqCalls, g_releaseIrqCalls;
static unsigned int g_lastShutdownIrq, g_lastReleaseIrq;
void rtwrap_shutdown_irq(unsigned int irq) { g_shutdownIrqCalls++; g_lastShutdownIrq = irq; }
void rtwrap_release_irq(unsigned int irq) { g_releaseIrqCalls++; g_lastReleaseIrq = irq; }

} /* extern "C" */

namespace {
struct TestComPort : CSTGComPort {
	int onByteReceivedCalls = 0;
	unsigned char lastByteReceived = 0;
	/* CSTGComPort itself has no constructor (real objects are
	 * placement-constructed into already-zeroed static storage in the
	 * production reconstruction, see keybed_init.cpp) -- but a
	 * plain stack-local TestComPort here would leave txFifo/irqEnabled/
	 * etc. as uninitialized garbage, not zero. This constructor is a
	 * test-only convenience, not a claim about the real class's own
	 * construction semantics. */
	TestComPort()
	{
		irqEnabled = 0;
		comPortId = 0;
		ioBase = 0;
		irqNumber = 0;
		txFifo.capacity = 0;
		txFifo.head = 0;
		txFifo.tail = 0;
	}
	void OnByteReceived(unsigned char byte) override
	{
		onByteReceivedCalls++;
		lastByteReceived = byte;
	}
};
} // namespace

static void reset(void)
{
	memset(g_regs, 0, sizeof(g_regs));
	g_inbCalls = g_outbCalls = 0;
	g_irqSaveCalls = g_irqRestoreCalls = 0;
	g_shutdownIrqCalls = g_releaseIrqCalls = 0;
}

int main(void)
{
	printf("[1] TransmitFifo::WriteByte / GetByteToTransmit:\n");
	{
		TestComPort port;
		port.txFifo.capacity = 4;
		port.txFifo.head = port.txFifo.tail = 0;

		check_eq("empty fifo: GetByteToTransmit returns -1", port.GetByteToTransmit(), -1);

		port.txFifo.WriteByte(0x11);
		port.txFifo.WriteByte(0x22);
		check_eq("head advanced by 2 writes", port.txFifo.head, 2);
		check_eq("first byte out == 0x11 (FIFO order)", port.GetByteToTransmit(), 0x11);
		check_eq("second byte out == 0x22", port.GetByteToTransmit(), 0x22);
		check_eq("empty again", port.GetByteToTransmit(), -1);

		printf("\n[1b] full FIFO drops the byte (confirmed real behavior):\n");
		port.txFifo.WriteByte(1);
		port.txFifo.WriteByte(2);
		port.txFifo.WriteByte(3);
		port.txFifo.WriteByte(4); /* fills capacity=4 */
		unsigned char headBefore = port.txFifo.head;
		port.txFifo.WriteByte(5); /* should be dropped */
		check_eq("head unchanged after a drop", port.txFifo.head, headBefore);
	}

	printf("\n[2] Cleanup():\n");
	{
		TestComPort port;
		reset();
		port.irqEnabled = 0;
		port.Cleanup();
		check_eq("no-op when irqEnabled == 0", g_shutdownIrqCalls, 0);

		reset();
		port.irqEnabled = 1;
		port.irqNumber = 7;
		port.Cleanup();
		check_eq("shutdown_irq called once", g_shutdownIrqCalls, 1);
		check_eq("release_irq called once", g_releaseIrqCalls, 1);
		check_eq("shutdown_irq got the right IRQ number", (long)g_lastShutdownIrq, 7);
		check_eq("release_irq got the right IRQ number", (long)g_lastReleaseIrq, 7);
		check_eq("irqEnabled cleared afterward", port.irqEnabled, 0);
	}

	printf("\n[3] SetBaudRate() / SetDTR() / IsHardwareTransmitterEmpty():\n");
	{
		TestComPort port;
		reset();
		port.ioBase = g_baseIoPort;
		port.irqEnabled = 1;
		port.SetBaudRate(0x0c); /* an arbitrary confirmed-shaped divisor */
		check_eq("irq_save/restore used (irqEnabled was set)", g_irqSaveCalls, 1);
		check_eq("LCR ends with DLAB cleared", g_regs[3] & 0x80, 0);
		check_eq("DLL got the low byte of the divisor", g_regs[0], 0x0c);
		check_eq("DLM got the high byte of the divisor", g_regs[1], 0);

		reset();
		port.irqEnabled = 0;
		port.SetBaudRate(0x34);
		check_eq("no irq_save/restore when irqEnabled == 0", g_irqSaveCalls, 0);

		reset();
		g_regs[4] = 0; /* MCR */
		port.SetDTR(true);
		check_eq("MCR bit 0 set", g_regs[4] & 0x1, 1);
		port.SetDTR(false);
		check_eq("MCR bit 0 cleared", g_regs[4] & 0x1, 0);

		reset();
		g_regs[5] = 0x40; /* LSR TEMT set */
		check_eq("IsHardwareTransmitterEmpty true when LSR bit 6 set",
			 port.IsHardwareTransmitterEmpty(), true);
		g_regs[5] = 0;
		check_eq("IsHardwareTransmitterEmpty false otherwise",
			 port.IsHardwareTransmitterEmpty(), false);
	}

	printf("\n[4] TriggerInterrupt() -- RX drain path:\n");
	{
		TestComPort port;
		reset();
		port.ioBase = g_baseIoPort;
		port.txFifo.capacity = 4;
		/* LSR: Data Ready set; the mock stg_inb above clears it as a
		 * side effect of reading the RX data register, matching real
		 * 16550 semantics -- so the real drain loop naturally
		 * terminates after exactly one byte instead of looping
		 * forever on a static mock value. */
		g_regs[5] = 0x1;  /* LSR bit0 */
		g_regs[0] = 0x42; /* the RX byte that will be read */
		g_regs[2] = 1;    /* IIR: no interrupt pending, once re-checked */

		port.TriggerInterrupt();
		check_eq("irq_save/restore used", g_irqSaveCalls, 1);
		check_eq("OnByteReceived called at least once", port.onByteReceivedCalls >= 1, true);
		check_eq("received the mocked RX byte", port.lastByteReceived, 0x42);
	}

	printf("\n[4b] TriggerInterrupt() -- TX burst path:\n");
	{
		/* `capacity` is confirmed to double as BOTH the FIFO's own
		 * depth cap (WriteByte drops anything past it, per [1b]
		 * above) AND TriggerInterrupt's burst-per-trigger limit
		 * (same field) -- so a single trigger can never observe
		 * "more queued than the burst limit allows": the queue can
		 * never hold more than `capacity` bytes in the first place.
		 * This scenario instead verifies a full-capacity queue gets
		 * fully drained in one trigger. */
		TestComPort port;
		reset();
		port.ioBase = g_baseIoPort;
		port.txFifo.capacity = 2;
		port.txFifo.WriteByte(0xa1);
		port.txFifo.WriteByte(0xa2);
		g_regs[5] = 0x20; /* LSR: THRE set, Data Ready clear */
		g_regs[2] = 1;    /* IIR: no interrupt pending */

		port.TriggerInterrupt();
		check_eq("both queued bytes transmitted", g_outbCalls, 2);
		check_eq("fifo fully drained", (unsigned char)(port.txFifo.head - port.txFifo.tail), 0);
	}

	printf("\n[4c] TriggerInterrupt() -- empty capacity skips transmit entirely:\n");
	{
		TestComPort port;
		reset();
		port.ioBase = g_baseIoPort;
		port.txFifo.capacity = 0;
		g_regs[5] = 0x20; /* THRE set -- would transmit if capacity allowed it */
		g_regs[2] = 1;
		port.TriggerInterrupt();
		check_eq("no bytes transmitted when capacity == 0", g_outbCalls, 0);
	}

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
