// SPDX-License-Identifier: GPL-2.0
/*
 * test_keybed_init.cpp  -  host-side known-answer test for
 * CSTGKeybedInterface_Startup()/_Cleanup() (see
 * ../include/oa_keybed_init.h / ../src/init/keybed_init.cpp).
 *
 * `CSTGComPort::Cleanup`/`TriggerInterrupt`/`TransmitFifo::WriteByte`
 * are now REAL (see oa_comport.h/comport.cpp, sec 10.53) and run for
 * real here -- this file mocks their own lower-level hardware I/O
 * dependencies (stg_inb/stg_outb/stg_local_irq_save/
 * stg_local_irq_restore/rtwrap_shutdown_irq/rtwrap_release_irq)
 * instead of mocking CSTGComPort's own methods directly (see
 * test_comport.cpp for those methods' own dedicated KAT). Only
 * `CSTGComPort::Initialize` remains undefined by the real
 * reconstruction (a confirmed-real, deliberately deferred large
 * function -- see oa_comport.h), so this file provides its own mock
 * body for it, matching this project's established pattern.
 *
 * Exercises the confirmed real control flow:
 *   [1] every port's Initialize() fails -> exhausts all 6 ports, all
 *       10 outer retries, returns 0 (failure, matching the INVERTED
 *       success convention: 0 = failure here).
 *   [2] a specific port's Initialize() succeeds and its ACK flag gets
 *       set during the delay-poll loop -> returns 1 (success).
 *   [3] CSTGKeybedInterface_Cleanup() calls Cleanup() and resets state.
 */

#include <cstdio>
#include "oa_keybed_init.h"
#include "oa_setup_global_resources.h" /* STGAPIFrontPanelStatus::sInstance,
					 * referenced by keybed_receive.cpp's
					 * ACK-completion path (sec 10.237) --
					 * not otherwise exercised by this
					 * file's own tests (they set the ACK
					 * flag directly via __const_udelay's
					 * mock instead), so a plain static
					 * buffer is enough here. */

static unsigned char g_frontPanelStub[STGAPI_FRONTPANEL_SIZE];
unsigned char *STGAPIFrontPanelStatus::sInstance = g_frontPanelStub;

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

static int g_initCalls;
static int g_alwaysFail;
static int g_successOnPort = -1; /* -1 = never succeed */
static int g_lastAttemptedPort;

/*
 * Mock body for the one CSTGComPort method this pass doesn't
 * reconstruct. On simulated success, sets irqEnabled so the real
 * Cleanup() below has something observable to tear down, matching
 * what a genuine successful UART init would do (request an IRQ).
 */
char CSTGComPort::Initialize(eComPortId comPortId, eBaudRateCode, eReceiveFifoThresholdCode)
{
	g_initCalls++;
	g_lastAttemptedPort = comPortId;
	if (g_alwaysFail)
		return 0;
	if (comPortId == g_successOnPort) {
		irqEnabled = 1;
		irqNumber = 4;
		return 1;
	}
	return 0;
}

extern "C" {

int printk(const char *, ...) { return 0; } /* real-hardware keybed-debug printk added 2026-07-21 */

static int g_debounceInitCalls;
void CSTGKeybedKeyDebounceFilter_Initialize(unsigned char *) { g_debounceInitCalls++; }

static int g_setAckAfterNCalls = -1; /* set ACK flag once __const_udelay has been called this many times */
static int g_udelayCalls;
void __const_udelay(unsigned long)
{
	g_udelayCalls++;
	if (g_setAckAfterNCalls >= 0 && g_udelayCalls >= g_setAckAfterNCalls)
		CSTGKeybedInterface_sInstance()[KEYBED_OFF_ACK_FLAG] = 1;
}

/* CSTGComPort's own lower-level hardware I/O dependencies (see
 * test_comport.cpp for the dedicated KAT exercising these in
 * isolation). Kept minimal here: TriggerInterrupt is only reached via
 * a specific fifo-state check this test doesn't trigger (a fresh
 * comPort's txFifo always has head==tail==0 immediately after
 * WriteByte queues exactly one byte, so head-tail==1 -- matching the
 * real "trigger immediately" condition -- these mocks just need to
 * make that call terminate promptly, not model real register
 * behavior in depth). */
static int g_shutdownIrqCalls, g_releaseIrqCalls;
void rtwrap_shutdown_irq(unsigned int) { g_shutdownIrqCalls++; }
void rtwrap_release_irq(unsigned int) { g_releaseIrqCalls++; }
unsigned long stg_local_irq_save(void) { return 0; }
void stg_local_irq_restore(unsigned long) {}
unsigned char stg_inb(unsigned int port)
{
	/* This mock CSTGComPort's ioBase is never set (Initialize() is
	 * mocked above and doesn't model real register setup), so it stays
	 * 0 -- these port numbers are that 0 base plus the confirmed real
	 * 16550 register offsets. IIR (port 2): report "no interrupt
	 * pending" immediately so TriggerInterrupt's poll loop exits after
	 * a single pass. LSR (port 5): report neither Data-Ready nor THRE,
	 * so it goes straight to the IIR check without draining/
	 * transmitting anything (this test only cares that TriggerInterrupt
	 * runs without hanging, not its own hardware-level behavior --
	 * see test_comport.cpp for that). */
	if (port == 2)
		return 1;
	return 0;
}
void stg_outb(unsigned int, unsigned char) {}

} /* extern "C" */

static void reset_mocks(void)
{
	g_initCalls = 0;
	g_debounceInitCalls = g_udelayCalls = 0;
	g_shutdownIrqCalls = g_releaseIrqCalls = 0;
	g_alwaysFail = 0;
	g_successOnPort = -1;
	g_setAckAfterNCalls = -1;
	for (int i = 0; i < KEYBED_SINSTANCE_SIZE; i++)
		CSTGKeybedInterface_sInstance()[i] = 0;
}

int main(void)
{
	printf("[1] every port fails: exhausts 6 ports x 10 retries, returns 0:\n");
	reset_mocks();
	g_alwaysFail = 1;
	int rc = CSTGKeybedInterface_Startup();
	check_eq("return value (failure)", rc, 0);
	check_eq("debounce filter initialized once", g_debounceInitCalls, 1);
	/* CONFIRMED via disassembly (not the earlier "up to 7 ports" claim
	 * from a prior session's summary): the real inner loop's own exit
	 * condition (`cmp ebx,7; jne loop_top`, with ebx starting at 1 and
	 * incrementing on each failure) means comPortId=6 gets computed but
	 * the loop exits before ever calling Initialize() with it -- only
	 * ports 0-5 (six) are genuinely attempted per outer round. */
	check_eq("Initialize called 60 times (6 ports x 10 retries)", g_initCalls, 60);
	/* Every port failed, so irqEnabled was never set -- Cleanup()'s
	 * real teardown path is a confirmed no-op here (see
	 * test_comport.cpp's own dedicated coverage of that behavior). */
	check_eq("no IRQ teardown (Initialize always failed)", g_shutdownIrqCalls, 0);
	check_eq("state left at 0 (not started)",
		 CSTGKeybedInterface_sInstance()[KEYBED_OFF_STATE], 0);

	printf("\n[2] port 3 succeeds, ACK arrives during the retry-delay loop:\n");
	reset_mocks();
	g_successOnPort = 3;
	g_setAckAfterNCalls = 3; /* ACK appears on the 3rd __const_udelay call */
	rc = CSTGKeybedInterface_Startup();
	check_eq("return value (success)", rc, 1);
	check_eq("Initialize attempted ports 0..3 (4 calls)", g_initCalls, 4);
	check_eq("last attempted port == 3", g_lastAttemptedPort, 3);
	check_eq("state left at 2 (fully started)",
		 CSTGKeybedInterface_sInstance()[KEYBED_OFF_STATE], 2);
	check_eq("no IRQ teardown on the successful port itself (only ports 0,1,2 failed)",
		 g_shutdownIrqCalls, 0);

	printf("\n[3] CSTGKeybedInterface_Cleanup():\n");
	CSTGKeybedInterface_Cleanup();
	check_eq("IRQ torn down (the successful port's irqEnabled was set)", g_shutdownIrqCalls, 1);
	check_eq("state reset to 0", CSTGKeybedInterface_sInstance()[KEYBED_OFF_STATE], 0);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
