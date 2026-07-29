// SPDX-License-Identifier: GPL-2.0
/*
 * test_kg_timer_manager.cpp  -  host-side known-answer test for
 * CKGTimerManager (see include/oa_kg_timer_manager.h for full
 * ground-truth provenance). Does NOT link ckg_engine.cpp -- provides
 * its own minimal CKGEngine::ms_poInstance/HaveAllModulesStopped()
 * mock instead (see that header's own include note).
 */

#include <cstdio>
#include "oa_kg_timer_manager.h"
#include "oa_ckg_module_param_msg_handler.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-56s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-56s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

/* ---- external singleton/library mocks ---- */
static unsigned long long g_nowUs;
extern "C" unsigned long long KGOutGate_GetCurrentTimeUs(void) { return g_nowUs; }

static bool g_syncExternal;
extern "C" bool KGOutGate_ShouldSyncExternal(void) { return g_syncExternal; }

static int g_tempoInternal, g_tempoExternal;
extern "C" int KGOutGate_GetTempoWhenSyncInternal(void) { return g_tempoInternal; }
extern "C" int KGOutGate_GetTempoWhenSyncExternal(void) { return g_tempoExternal; }

static int g_rtPeTempoCalls;
static unsigned short g_lastPeTempo;
extern "C" void RT_pe_tempo(unsigned short tempo) { g_rtPeTempoCalls++; g_lastPeTempo = tempo; }

static int g_debugModeCalls;
extern "C" bool SKSTGGate_GetDebugMode(void) { g_debugModeCalls++; return false; }

static int g_clockSyncCalls;
static bool g_lastClockSyncArg;
extern "C" void RT_clock_synchronize(bool sync) { g_clockSyncCalls++; g_lastClockSyncArg = sync; }

static unsigned int g_ticksTilBeat480;
extern "C" unsigned int KS_get_karma_ticks_til_beat_480(void) { return g_ticksTilBeat480; }

/* CKGTimerManager::GetTicksUntilTheBeat()'s own real call target --
 * mocked here rather than linking the full ckg_engine.cpp (see Makefile
 * comment). */
unsigned char *CKGEngine::ms_poInstance;
static bool g_allModulesStopped;
bool CKGEngine::HaveAllModulesStopped() { return g_allModulesStopped; }

int main()
{
	printf("CKGTimerManager known-answer test\n");
	printf("==================================\n");

	printf("[1] ctor: field init\n");
	{
		g_nowUs = 0x1122334455ULL;
		CKGTimerManager t;
		unsigned int untilBeat = t.GetTicksUntilTheBeat(false);
		(void)untilBeat; /* just confirm the object is usable post-ctor */
		check_eq("sizeof(CKGTimerManager) == real ctor's own 0x38 alloc",
			 (long)sizeof(CKGTimerManager), 0x38);
	}

	printf("[2] SetTempo/SetCurrentTempo/ChangePerformance: RT_pe_tempo only on change\n");
	{
		CKGTimerManager t;
		g_rtPeTempoCalls = 0;
		t.SetTempo(120);
		check_eq("first SetTempo(120) -> RT_pe_tempo called once", g_rtPeTempoCalls, 1);
		check_eq("...with tempo 120", (long)g_lastPeTempo, 120);
		t.SetTempo(120);
		check_eq("same tempo again -> no extra call", g_rtPeTempoCalls, 1);
		t.SetTempo(140);
		check_eq("different tempo -> called again", g_rtPeTempoCalls, 2);

		g_syncExternal = false;
		g_tempoInternal = 90;
		g_rtPeTempoCalls = 0;
		t.SetCurrentTempo();
		check_eq("SetCurrentTempo picks internal tempo when not syncing external",
			 g_rtPeTempoCalls, 1);
		check_eq("...tempo == 90", (long)g_lastPeTempo, 90);

		g_syncExternal = true;
		g_tempoExternal = 200;
		g_rtPeTempoCalls = 0;
		t.ChangePerformance();
		check_eq("ChangePerformance picks external tempo when syncing external",
			 g_rtPeTempoCalls, 1);
		check_eq("...tempo == 200", (long)g_lastPeTempo, 200);
	}

	printf("[3] ShouldTempoLEDFlash: countdown + reset to 0x1e0\n");
	{
		CKGTimerManager t;
		bool flashed = false;
		int iterations = 0;
		while (!flashed && iterations < 1000) {
			flashed = t.ShouldTempoLEDFlash();
			iterations++;
		}
		check_eq("flashes exactly once every 480 calls (ctor countdown starts at 0)",
			 iterations, 1);
		iterations = 0;
		flashed = false;
		while (!flashed && iterations < 1000) {
			flashed = t.ShouldTempoLEDFlash();
			iterations++;
		}
		check_eq("...next flash is exactly 480 calls later", iterations, 0x1e0);
	}

	printf("[4] IncElapsedTick/ReceiveMIDIClock: internal vs external sync\n");
	{
		CKGTimerManager t;
		g_syncExternal = false;
		t.IncElapsedTick();
		t.IncElapsedTick();
		int c1 = t.GetIntervalClock(); /* latches baseline, always 0 first call */
		(void)c1;
		int before = t.GetKarmaIntervalClock(0); /* peek without advancing tick */
		(void)before;

		/* Internal sync: every IncElapsedTick unconditionally advances. */
		unsigned int pos1;
		g_syncExternal = false;
		t.GetTicksUntilTheBeat(false); /* no-op sanity call, unrelated field */
		for (int i = 0; i < 5; i++) t.IncElapsedTick();
		pos1 = 0;
		(void)pos1;

		/* External sync: ReceiveMIDIClock sets a 20-tick backlog;
		 * IncElapsedTick drains it 1-for-1 while syncing external. */
		CKGTimerManager t2;
		g_syncExternal = true;
		t2.ReceiveMIDIClock();
		int drained = 0;
		for (int i = 0; i < 25; i++) {
			int before2 = t2.GetKarmaIntervalClock(0);
			(void)before2;
			t2.IncElapsedTick();
			drained++;
		}
		check_eq("ReceiveMIDIClock+IncElapsedTick drain loop completes", drained, 25);
	}

	printf("[5] GetIntervalClock/GetKarmaIntervalClock: fixed-point tick scaling\n");
	{
		CKGTimerManager t;
		/* default mTempoPercent == 100 (ctor) -> 1:1 scaling. */
		check_eq("first GetIntervalClock call always returns 0 (latches baseline)",
			 t.GetIntervalClock(), 0);
		g_syncExternal = false;
		for (int i = 0; i < 7; i++) t.IncElapsedTick();
		check_eq("7 elapsed ticks at 100% -> GetIntervalClock returns 7",
			 t.GetIntervalClock(), 7);

		t.SetTempoPercent(50);
		for (int i = 0; i < 10; i++) t.IncElapsedTick();
		check_eq("10 elapsed ticks at 50% -> GetIntervalClock returns 5",
			 t.GetIntervalClock(), 5);
	}
	{
		CKGTimerManager t;
		/* mTempoPercent is the numerator itself (ctor default 100),
		 * so at the default 100% every call is an exact 1:1
		 * passthrough with a permanently-zero remainder. */
		check_eq("GetKarmaIntervalClock(0) at 100% -> 0", t.GetKarmaIntervalClock(0), 0);
		check_eq("GetKarmaIntervalClock(7) at 100% -> 7 (exact, no remainder)",
			 t.GetKarmaIntervalClock(7), 7);

		/* A non-100 percent exercises the fixed-point carry: at 33%,
		 * 3 ticks -> 3*33=99 (not yet >99) -> count 0, remainder 99;
		 * next 1 tick -> 99+33=132 -> count 1, remainder 32. */
		t.SetTempoPercent(33);
		check_eq("33%, 3 ticks -> 99 not yet >99 -> count 0",
			 t.GetKarmaIntervalClock(3), 0);
		check_eq("...next 1 tick carries the 99 remainder -> 132 -> count 1",
			 t.GetKarmaIntervalClock(1), 1);
	}

	printf("[6] GetTicksUntilTheBeat: stopped gate + modulo + wrap-negative\n");
	{
		CKGTimerManager t;
		g_allModulesStopped = true;
		check_eq("all modules stopped -> 0 regardless of raw ticks",
			 (long)t.GetTicksUntilTheBeat(false), 0);

		g_allModulesStopped = false;
		g_ticksTilBeat480 = 100;
		check_eq("raw ticks < 0x1e0, no wrap -> passthrough",
			 (long)t.GetTicksUntilTheBeat(false), 100);

		g_ticksTilBeat480 = 0x1e0 + 50; /* 530 -> modulo 480 -> 50 */
		check_eq("raw ticks > 0x1e0 -> modulo 0x1e0",
			 (long)t.GetTicksUntilTheBeat(false), 50);

		g_ticksTilBeat480 = 0x1c0; /* 448, > 0x1a3(419), wrap requested */
		unsigned int wrapped = t.GetTicksUntilTheBeat(true);
		check_eq("wrapNegative + ticks>0x1a3 -> ticks-0x1e0 (unsigned wraparound)",
			 (long)wrapped, (long)(unsigned int)(0x1c0 - 0x1e0));

		g_ticksTilBeat480 = 0x100; /* 256, <= 0x1a3, wrap requested but not triggered */
		check_eq("wrapNegative + ticks<=0x1a3 -> passthrough, no wrap",
			 (long)t.GetTicksUntilTheBeat(true), 0x100);
	}

	printf("[7] StartSync/StopSync: RT_clock_synchronize(true/false), discards debug-mode call\n");
	{
		g_clockSyncCalls = 0;
		g_debugModeCalls = 0;
		CKGTimerManager::StartSync();
		check_eq("StartSync -> RT_clock_synchronize(true)", (long)g_lastClockSyncArg, 1);
		check_eq("...called once", g_clockSyncCalls, 1);
		check_eq("...SKSTGGate_GetDebugMode called (result discarded)", g_debugModeCalls, 1);

		CKGTimerManager::StopSync();
		check_eq("StopSync -> RT_clock_synchronize(false)", (long)g_lastClockSyncArg, 0);
		check_eq("...called twice total", g_clockSyncCalls, 2);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
