// SPDX-License-Identifier: GPL-2.0
/*
 * comport.cpp  -  CSTGComPort's confirmed real methods (excluding
 * Initialize -- see oa_comport.h for why). See that header for the
 * full ground-truthing details.
 *
 * Faithful, instruction-level reconstruction from a full objdump
 * disassembly + relocation trace of each real method in OA_real.ko.
 */

#include "oa_comport.h"

void CSTGComPort::TransmitFifo::WriteByte(unsigned char value)
{
	unsigned char count = head - tail;
	if (count >= capacity)
		return; /* full, byte dropped -- confirmed real, no error
			 * signaled to the caller */
	buffer[head & 0xf] = value;
	head++;
}

int CSTGComPort::GetByteToTransmit()
{
	if (txFifo.head == txFifo.tail)
		return -1; /* empty */
	unsigned char value = txFifo.buffer[txFifo.tail & 0xf];
	txFifo.tail++;
	return value;
}

void CSTGComPort::Cleanup()
{
	if (irqEnabled) {
		rtwrap_shutdown_irq(irqNumber);
		rtwrap_release_irq(irqNumber);
		irqEnabled = 0;
	}
}

void CSTGComPort::SetDTR(bool assert)
{
	unsigned char mcr = stg_inb(ioBase + 4);
	unsigned char newMcr = (mcr & ~0x1) | (assert ? 0x1 : 0);
	stg_outb(ioBase + 4, newMcr);
}

bool CSTGComPort::IsHardwareTransmitterEmpty() const
{
	return (stg_inb(ioBase + 5) & 0x40) != 0; /* LSR bit 6 = TEMT */
}

void CSTGComPort::SetBaudRate(int baudRateCode)
{
	/* Confirmed real: only cli/sti-guarded if IRQs are currently
	 * enabled on this port -- matches the same `irqEnabled` flag
	 * Cleanup checks. */
	bool needCli = irqEnabled != 0;
	unsigned long savedFlags = 0;
	if (needCli)
		savedFlags = stg_local_irq_save();

	unsigned char lcr = stg_inb(ioBase + 3);
	stg_outb(ioBase + 3, lcr | 0x80); /* set DLAB */
	stg_outb(ioBase + 1, (unsigned char)(baudRateCode >> 8)); /* DLM */
	stg_outb(ioBase + 0, (unsigned char)baudRateCode);	    /* DLL */
	lcr = stg_inb(ioBase + 3);
	stg_outb(ioBase + 3, lcr & 0x7f); /* clear DLAB */

	if (needCli)
		stg_local_irq_restore(savedFlags);
}

/*
 * Confirmed real: both TriggerInterrupt and HandleInterrupt share this
 * EXACT same core poll/drain/transmit loop -- the only confirmed
 * difference between the two real functions is TriggerInterrupt's own
 * cli/sti critical-section wrapping (called from non-ISR context);
 * HandleInterrupt has none (called from within a real ISR, where
 * interrupts are already masked by the CPU). Modeled here as a shared
 * private-equivalent helper rather than duplicating the loop twice.
 */
static void ComPortServiceLoop(CSTGComPort *self)
{
	stg_inb(self->ioBase + 2); /* dummy IIR read: confirmed real,
				     * clears any latched pending flag
				     * before polling starts */
poll_check:
	{
		unsigned char lsr = stg_inb(self->ioBase + 5);
		if (lsr & 0x1) {
			unsigned char rxByte = stg_inb(self->ioBase);
			self->OnByteReceived(rxByte);
			goto poll_check;
		}
		if (!(lsr & 0x20))
			goto check_iir_pending;
	}

	/* THRE set -- transmitter ready, service the TX fifo. */
	if (self->txFifo.capacity == 0)
		goto check_iir_pending;

	{
		int byte = self->GetByteToTransmit();
		unsigned int burstCount = 0;
		while (byte >= 0) {
			stg_outb(self->ioBase, (unsigned char)byte);
			burstCount++;
			if (burstCount >= self->txFifo.capacity)
				break;
			byte = self->GetByteToTransmit();
		}
	}

check_iir_pending:
	if ((stg_inb(self->ioBase + 2) & 0xf) != 1)
		goto poll_check; /* an interrupt is still pending, re-poll */
	/* IIR == 1: no interrupt pending, done. */
}

void CSTGComPort::TriggerInterrupt()
{
	unsigned long savedFlags = stg_local_irq_save();
	ComPortServiceLoop(this);
	stg_local_irq_restore(savedFlags);
}

void CSTGComPort::HandleInterrupt()
{
	ComPortServiceLoop(this);
}

/*
 * Confirmed real: the RTAI IRQ-callback trampoline `rtwrap_request_irq`
 * is given (see oa_comport.h). `irq` is confirmed dead in the real
 * disassembly (loaded, never read again) -- RTAI's own callback ABI
 * requires the parameter even though this handler has no use for it.
 * `dev` is the `this` pointer `Initialize()` originally registered (see
 * comport_init.cpp). Forwards to the already-reconstructed
 * `HandleInterrupt()`/`ComPortServiceLoop` above rather than
 * re-deriving the identical loop a second time -- confirmed
 * byte-for-byte the same body via full disassembly (same dummy-IIR
 * read, same poll/drain/transmit branch shape, same two vtable
 * dispatches).
 */
void CSTGComPort::RTAIInterruptHandler(unsigned int irq, void *dev)
{
	(void)irq;
	static_cast<CSTGComPort *>(dev)->HandleInterrupt();
}
