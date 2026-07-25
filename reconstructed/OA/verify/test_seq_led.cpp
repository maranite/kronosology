// SPDX-License-Identifier: GPL-2.0
/*
 * test_seq_led.cpp  -  host-side KAT for TurnOnSeqLed()/SKSTGGate_
 * .../SPROutGate_... (src/engine/seq_led.cpp).
 */

#include <cstdio>
#include "oa_seq_led.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

CSTGFrontPanel *CSTGFrontPanel::sInstance;

static int g_setCalls, g_resetCalls, g_lastCode;
void CSTGFrontPanel::SetLED(unsigned int code) { g_setCalls++; g_lastCode = code; }
void CSTGFrontPanel::ResetLED(unsigned int code) { g_resetCalls++; g_lastCode = code; }

static int g_saveCalls, g_restoreCalls;
extern "C" unsigned long stg_local_irq_save(void) { g_saveCalls++; return 0x246; }
extern "C" void stg_local_irq_restore(unsigned long) { g_restoreCalls++; }

static unsigned char g_panelBuf[64];
static void reset()
{
	CSTGFrontPanel::sInstance = (CSTGFrontPanel *)g_panelBuf;
	g_setCalls = g_resetCalls = g_lastCode = 0;
	g_saveCalls = g_restoreCalls = 0;
}

int main()
{
	reset();
	TurnOnSeqLed(0, true);
	check_eq("id0 (StartStopRed) is a confirmed real no-op", g_setCalls + g_resetCalls, 0);
	check_eq("id0 no-op returns BEFORE irq save (matches real jump-table order)", g_saveCalls, 0);

	reset();
	TurnOnSeqLed(1, false);
	check_eq("id1 (StartStopGreen) is a confirmed real no-op", g_setCalls + g_resetCalls, 0);

	reset();
	TurnOnSeqLed(2, true);
	check_eq("id2 (Rec) on -> SetLED", g_setCalls, 1);
	check_eq("id2 code = 0x17", g_lastCode, 0x17);
	check_eq("irq saved/restored", g_saveCalls, 1);
	check_eq("irq restored", g_restoreCalls, 1);

	reset();
	TurnOnSeqLed(2, false);
	check_eq("id2 (Rec) off -> ResetLED", g_resetCalls, 1);

	reset();
	TurnOnSeqLed(3, true);
	check_eq("id3 (Pause) code = 0x1a", g_lastCode, 0x1a);

	reset();
	TurnOnSeqLed(4, true);
	check_eq("id4 (FF) code = 0x1c", g_lastCode, 0x1c);

	reset();
	TurnOnSeqLed(5, true);
	check_eq("id5 (Rew) code = 0x1b", g_lastCode, 0x1b);

	reset();
	SKSTGGate_TurnOnFFLED(true);
	check_eq("SKSTGGate_TurnOnFFLED forwards id4", g_lastCode, 0x1c);
	check_eq("SKSTGGate_TurnOnFFLED forwards on=true", g_setCalls, 1);

	reset();
	SKSTGGate_TurnOnRecLED(false);
	check_eq("SKSTGGate_TurnOnRecLED forwards id2", g_lastCode, 0x17);
	check_eq("SKSTGGate_TurnOnRecLED forwards on=false", g_resetCalls, 1);

	reset();
	SKSTGGate_TurnOnPauseLED(true);
	check_eq("SKSTGGate_TurnOnPauseLED forwards id3", g_lastCode, 0x1a);

	reset();
	SKSTGGate_TurnOnRewLED(true);
	check_eq("SKSTGGate_TurnOnRewLED forwards id5", g_lastCode, 0x1b);

	reset();
	SKSTGGate_TurnOnStartStopRedLed(true);
	check_eq("SKSTGGate_TurnOnStartStopRedLed still a no-op", g_setCalls + g_resetCalls, 0);

	reset();
	SKSTGGate_TurnOnStartStopGreenLed(true);
	check_eq("SKSTGGate_TurnOnStartStopGreenLed still a no-op", g_setCalls + g_resetCalls, 0);

	/* SPROutGate_* pure trampolines -- one spot check each is enough. */
	reset();
	SPROutGate_TurnOnFFLED(true);
	check_eq("SPROutGate_TurnOnFFLED -> SKSTGGate_TurnOnFFLED -> id4", g_lastCode, 0x1c);
	reset();
	SPROutGate_TurnOnRewLED(false);
	check_eq("SPROutGate_TurnOnRewLED -> id5 off", g_resetCalls, 1);
	reset();
	SPROutGate_TurnOnRecLED(true);
	check_eq("SPROutGate_TurnOnRecLED -> id2", g_lastCode, 0x17);
	reset();
	SPROutGate_TurnOnPauseLED(true);
	check_eq("SPROutGate_TurnOnPauseLED -> id3", g_lastCode, 0x1a);
	reset();
	SPROutGate_TurnOnStartStopRedLed(true);
	check_eq("SPROutGate_TurnOnStartStopRedLed still a no-op", g_setCalls + g_resetCalls, 0);
	reset();
	SPROutGate_TurnOnStartStopGreenLed(true);
	check_eq("SPROutGate_TurnOnStartStopGreenLed still a no-op", g_setCalls + g_resetCalls, 0);

	printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
	return g_fail ? 1 : 0;
}
