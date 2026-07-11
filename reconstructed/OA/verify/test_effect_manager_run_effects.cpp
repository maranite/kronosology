// SPDX-License-Identifier: GPL-2.0
/*
 * test_effect_manager_run_effects.cpp  -  host-side known-answer test for
 * batch 49: CSTGEffectManager::RunEffects() (see
 * ../src/engine/effect_manager_run_effects.cpp).
 *
 * Links midi_clock_sync.cpp + sk_stg_gate.cpp directly (for the REAL
 * CSTGMIDIClockSync ctor + GetFilteredTempoBPM(), also real now this
 * batch) but deliberately does NOT link global.cpp -- global.cpp's own
 * CSTGPerformanceVarsManager::RunEffects() is a SEPARATE function
 * exercised for real in test_global.cpp's own scenario [55]; here it
 * gets a trivial local call-tracking mock instead, matching this
 * project's established "own dedicated TU keeps its own trivial mocks
 * for functions defined/tested elsewhere" convention (avoids pulling in
 * global.cpp's own large transitive mock ecosystem for an unrelated
 * function).
 *
 * Mock ctor for CSTGAudioBusManager -- matches test_midi_clock_sync.cpp's
 * own established precedent (real ctor lives in managers.cpp, not linked
 * here).
 */

#include <cstdio>
#include <cstring>
#include <new>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_global.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-60s %ld\n", label, got);
		return;
	}
	printf("  FAILED %-60s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}
static void check_float(const char *label, double got, double want)
{
	if (got == want) {
		printf("  ok    %-60s %f\n", label, got);
		return;
	}
	printf("  FAILED %-60s got=%f want=%f\n", label, got, want);
	g_fail++;
}

CSTGAudioBusManager *CSTGAudioBusManager::sInstance;
CSTGAudioBusManager::CSTGAudioBusManager() {}

/* Real storage lives in lfo_stepseq_quad.cpp (not linked here) -- local
 * stand-in, matching test_midi_clock_sync.cpp's own precedent. */
CSTGMIDIClockSync *CSTGMIDIClockSync::sInstance;

/* Real storage (zero-filled placeholder) lives in bar2_stubs.cpp (not
 * linked here) -- local stand-in, same "_ZTVxxx" per-isolated-test
 * convention used throughout this project's own verify/ suite. */
extern "C" unsigned char _ZTV20CSTGIntMIDIClockSync[40];
unsigned char _ZTV20CSTGIntMIDIClockSync[40];

/* Static storage matching test_sk_stg_gate.cpp/test_midi_clock_sync.cpp's
 * own established shape (the real +0x97c750 offset needs a just-under-
 * 9.9MB stand-in). */
static unsigned char bankMgr[0x97c760];

/* Real storage lives in bar2_stubs.cpp (not linked here) -- local
 * stand-in. */
unsigned char CSTGPerformanceVarsManager::sInstance[12];

/* Trivial call-tracking mock -- the REAL body (global.cpp) is exercised
 * separately in test_global.cpp's own scenario [55]; this file only
 * needs to confirm CSTGEffectManager::RunEffects() calls it exactly
 * once, with the correct `this`. */
static int g_perfVarsMgrRunEffectsCalls;
static void *g_lastPerfVarsMgrRunEffectsThis;
void CSTGPerformanceVarsManager::RunEffects()
{
	g_perfVarsMgrRunEffectsCalls++;
	g_lastPerfVarsMgrRunEffectsThis = (void *)this;
}

int main(void)
{
	printf("CSTGEffectManager::RunEffects() known-answer test (batch 49)\n");
	printf("=========================================================\n");

	CSTGAudioBusManager abm;
	abm.busGainReciprocal = 0.0006666666595265269f; /* confirmed real, ~1/1500 */
	abm.busGainScale = 1500.0f;			 /* confirmed real */
	CSTGAudioBusManager::sInstance = &abm;

	for (unsigned int i = 0; i < sizeof(bankMgr); i++)
		bankMgr[i] = 0;
	CKGBankManager::ms_poInstance = bankMgr;

	CTimerManager tm;
	CTimerManager::ms_poInstance = &tm;
	*(int *)(bankMgr + 0x97c750) = 0; /* mode 0 -> ShouldSyncExternalClock() == false */

	unsigned char clockBuf[0x200];
	memset(clockBuf, 0xcc, sizeof(clockBuf));
	CSTGMIDIClockSync *mcs = new (clockBuf) CSTGMIDIClockSync();
	CSTGMIDIClockSync::sInstance = mcs;

	unsigned char emBuf[0xc00];

	printf("\n[1] ctor-default clock state -> defaultTempoA/B == 120.0f, "
	       "_tailZeroed mirrors clockBase+0x90/0x94/0xb0/0xb4\n");
	{
		memset(emBuf, 0xcc, sizeof(emBuf));
		CSTGEffectManager *em = (CSTGEffectManager *)emBuf;
		g_perfVarsMgrRunEffectsCalls = 0;
		g_lastPerfVarsMgrRunEffectsThis = 0;

		*(unsigned int *)(clockBuf + 0x90) = 0x11111111u;
		*(unsigned int *)(clockBuf + 0x94) = 0x22222222u;
		*(unsigned int *)(clockBuf + 0xb0) = 0x33333333u;
		*(unsigned int *)(clockBuf + 0xb4) = 0x44444444u;

		em->RunEffects();

		check_eq("CSTGPerformanceVarsManager::RunEffects() called once",
			 (unsigned int)g_perfVarsMgrRunEffectsCalls, 1u);
		check_eq("  ...this == &CSTGPerformanceVarsManager::sInstance",
			 (unsigned int)(g_lastPerfVarsMgrRunEffectsThis ==
					(void *)CSTGPerformanceVarsManager::sInstance),
			 1u);
		check_float("defaultTempoA == 120.0f (ctor-default cross-check)",
			    em->defaultTempoA, 120.0f);
		check_float("defaultTempoB == 120.0f (ctor-default cross-check)",
			    em->defaultTempoB, 120.0f);
		check_eq("_tailZeroed[0] == clockBase+0x90", em->_tailZeroed[0], 0x11111111);
		check_eq("_tailZeroed[1] == clockBase+0x94", em->_tailZeroed[1], 0x22222222);
		check_eq("_tailZeroed[2] == clockBase+0xb0", em->_tailZeroed[2], 0x33333333);
		check_eq("_tailZeroed[3] == clockBase+0xb4", em->_tailZeroed[3], 0x44444444);
	}

	printf("\n[2] out-of-range smoothed intervals -> defaultTempoA/B clamped "
	       "to [40.0f, 300.0f]\n");
	{
		memset(emBuf, 0xcc, sizeof(emBuf));
		CSTGEffectManager *em = (CSTGEffectManager *)emBuf;

		/* index0's own smoothed interval (+0x98): pick a tiny value so
		 * busGainScale*smoothed*2.5 < 40.0f. */
		*(double *)(clockBuf + 0x98) = 0.001;
		/* index1's own smoothed interval (+0xb8): pick a huge value so
		 * busGainScale*smoothed*2.5 > 300.0f. */
		*(double *)(clockBuf + 0xb8) = 1.0;

		double raw0 = (double)abm.busGainScale * 0.001 * 2.5;
		double raw1 = (double)abm.busGainScale * 1.0 * 2.5;
		check_eq("sanity: raw0 < 40.0f", (unsigned int)(raw0 < 40.0), 1u);
		check_eq("sanity: raw1 > 300.0f", (unsigned int)(raw1 > 300.0), 1u);

		em->RunEffects();

		check_float("defaultTempoA clamped up to 40.0f", em->defaultTempoA, 40.0f);
		check_float("defaultTempoB clamped down to 300.0f", em->defaultTempoB, 300.0f);
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
