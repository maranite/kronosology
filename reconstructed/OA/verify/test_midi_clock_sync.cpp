// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_clock_sync.cpp  -  host-side known-answer test for batch 21:
 * CSTGMIDIClockSync::CSTGMIDIClockSync(), CSTGMIDIClockSyncBase::
 * Initialize(), and the complete CSTGIntMIDIClockSync class (see
 * ../src/engine/midi_clock_sync.cpp).
 *
 * Links sk_stg_gate.cpp directly (its SKSTGGate_ShouldSyncExternalClock()/
 * SKSTGGate_GetInternalTempo() are real dependencies of
 * PrepareForNextTick()/NotifySyncDetected()) -- so this test also owns
 * CTimerManager::ms_poInstance/CKGBankManager::ms_poInstance setup,
 * matching test_sk_stg_gate.cpp's own established buffer shapes.
 *
 * Mock ctor for CSTGAudioBusManager -- the real ctor lives in
 * managers.cpp (deliberately not linked here, matching
 * test_audio_bus_manager.cpp's own precedent); this test only needs
 * busGainReciprocal/busGainScale set directly.
 */

#include <cstdio>
#include <cstring>
#include <new>
#include "oa_engine_init.h"

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

/* Real storage lives in lfo_stepseq_quad.cpp (not linked here) --
 * local stand-in, matching this project's established per-isolated-test
 * convention. */
CSTGMIDIClockSync *CSTGMIDIClockSync::sInstance;

/* Real storage (zero-filled placeholder) lives in bar2_stubs.cpp (not
 * linked here) -- local stand-in, matching the same
 * "_ZTVxxx" per-isolated-test convention already used throughout this
 * project's own verify/ suite (sec 10.158). */
extern "C" unsigned char _ZTV20CSTGIntMIDIClockSync[40];
unsigned char _ZTV20CSTGIntMIDIClockSync[40];

/* Static storage matching test_sk_stg_gate.cpp's own established shape
 * (the real +0x97c750 offset needs a just-under-9.9MB stand-in). */
static unsigned char bankMgr[0x97c760];

int main(void)
{
	printf("CSTGMIDIClockSync cluster known-answer test (batch 21)\n");
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

	printf("\n[1] CSTGMIDIClockSyncBase::Initialize() -- direct (not via ctor)\n");
	{
		unsigned char buf[0x200];
		memset(buf, 0xcc, sizeof(buf));
		CSTGMIDIClockSyncBase *base = (CSTGMIDIClockSyncBase *)buf;

		base->Initialize();

		check_eq("kClockTimeOutTicks == 156 (0.104*1500.0, exact)",
			 CSTGMIDIClockSyncBase::kClockTimeOutTicks, 156);
		check_float("kMaxNormalizedTempo == 200*busGainReciprocal",
			    CSTGMIDIClockSyncBase::kMaxNormalizedTempo,
			    200.0f * abm.busGainReciprocal);
		check_eq("fieldAt(0x8) zeroed", *(int *)(buf + 0x8), 0);
		check_eq("fieldAt(0x14) zeroed", buf[0x14], 0);
		check_float("fieldAt(0xc) == 48*busGainReciprocal",
			    *(double *)(buf + 0xc), 48.0 * (double)abm.busGainReciprocal);

		printf("  -- second call: kClockTimeOutTicks must NOT recompute (own "
		       "guard) even with a poisoned busGainScale\n");
		float savedScale = abm.busGainScale;
		abm.busGainScale = 999999.0f;
		base->Initialize();
		check_eq("kClockTimeOutTicks unchanged on 2nd call (guard held)",
			 CSTGMIDIClockSyncBase::kClockTimeOutTicks, 156);
		abm.busGainScale = savedScale;
	}

	printf("\n[2] CSTGIntMIDIClockSync ring: GetEventCount/GetEventStatusByte/ConsumeEvent\n");
	{
		unsigned char buf[0x200];
		memset(buf, 0, sizeof(buf));
		CSTGIntMIDIClockSync *ring = (CSTGIntMIDIClockSync *)buf;

		*(unsigned int *)(buf + 0x54) = 5; /* write index */
		*(unsigned int *)(buf + 0x58) = 2; /* read index */
		for (unsigned int i = 0; i < 16; i++)
			buf[0x44 + i] = (unsigned char)(0x90 + i);

		check_eq("GetEventCount == writeIdx - readIdx", ring->GetEventCount(), 3);
		check_eq("GetEventStatusByte == ring[readIdx & 0xf]",
			 ring->GetEventStatusByte(), 0x90 + 2);

		ring->ConsumeEvent();
		check_eq("ConsumeEvent advances read index", *(unsigned int *)(buf + 0x58), 3);
		check_eq("GetEventCount after consume", ring->GetEventCount(), 2);
		check_eq("GetEventStatusByte after consume", ring->GetEventStatusByte(), 0x90 + 3);

		printf("  -- wraparound: readIdx=17 -> ring index 1\n");
		*(unsigned int *)(buf + 0x58) = 17;
		check_eq("GetEventStatusByte wraps mod 16", ring->GetEventStatusByte(), 0x90 + 1);
	}

	printf("\n[3] CSTGIntMIDIClockSync::ProcessClock/GetClockLateThresholdTicks/"
	       "GetClockEarlyThresholdTicks -- trivial confirmed constants\n");
	{
		unsigned char buf[0x200];
		memset(buf, 0xcc, sizeof(buf));
		CSTGIntMIDIClockSync *o = (CSTGIntMIDIClockSync *)buf;

		o->ProcessClock(); /* confirmed real no-op; just must not crash */
		check_float("GetClockLateThresholdTicks == 1.0f", o->GetClockLateThresholdTicks(), 1.0f);
		check_float("GetClockEarlyThresholdTicks == 0.0f", o->GetClockEarlyThresholdTicks(), 0.0f);
	}

	printf("\n[4] PrepareForNextTick() -- gated by SKSTGGate_ShouldSyncExternalClock()\n");
	{
		unsigned char buf[0x200];
		memset(buf, 0xcc, sizeof(buf));
		CSTGIntMIDIClockSync *o = (CSTGIntMIDIClockSync *)buf;

		*(int *)(bankMgr + 0x97c750) = 1; /* mode 1 -> ShouldSync == true */
		double sentinel = 12345.0;
		*(double *)(buf + 0xc) = sentinel;
		o->PrepareForNextTick();
		check_float("external sync active -> fieldAt(0xc) untouched",
			    *(double *)(buf + 0xc), sentinel);

		*(int *)(bankMgr + 0x97c750) = 0; /* mode 0 -> ShouldSync == false */
		unsigned char innerBuf[0x30];
		unsigned char outerBuf[0x8];
		*(int *)(innerBuf + 0x2c) = 12000; /* SKSTGGate_GetInternalTempo() result */
		*(unsigned char **)outerBuf = innerBuf;
		CTimerManager::ms_poInstance = (CTimerManager *)outerBuf;

		o->PrepareForNextTick();
		double want = 12000.0 * 0.01 * 0.4 * (double)abm.busGainReciprocal;
		check_float("internal sync -> fieldAt(0xc) == tempo*0.01*0.4*busGainReciprocal",
			    *(double *)(buf + 0xc), want);
	}

	printf("\n[5] NotifySyncDetected() -- SAME computation, unconditional\n");
	{
		unsigned char buf[0x200];
		memset(buf, 0xcc, sizeof(buf));
		CSTGIntMIDIClockSync *o = (CSTGIntMIDIClockSync *)buf;

		unsigned char innerBuf[0x30];
		unsigned char outerBuf[0x8];
		*(int *)(innerBuf + 0x2c) = 24000;
		*(unsigned char **)outerBuf = innerBuf;
		CTimerManager::ms_poInstance = (CTimerManager *)outerBuf;

		/* Even with external sync ACTIVE, NotifySyncDetected must still
		 * recompute (unconditional, unlike PrepareForNextTick). */
		*(int *)(bankMgr + 0x97c750) = 1;
		o->NotifySyncDetected();
		double want = 24000.0 * 0.01 * 0.4 * (double)abm.busGainReciprocal;
		check_float("NotifySyncDetected recomputes regardless of ShouldSyncExternalClock",
			    *(double *)(buf + 0xc), want);
	}

	printf("\n[6] CSTGMIDIClockSync::CSTGMIDIClockSync() -- full ctor field sweep\n");
	{
		*(int *)(bankMgr + 0x97c750) = 0;
		unsigned char innerBuf[0x30];
		unsigned char outerBuf[0x8];
		*(int *)(innerBuf + 0x2c) = 0;
		*(unsigned char **)outerBuf = innerBuf;
		CTimerManager::ms_poInstance = (CTimerManager *)outerBuf;

		unsigned char buf[0x200];
		memset(buf, 0xcc, sizeof(buf));
		CSTGMIDIClockSync *mcs = new (buf) CSTGMIDIClockSync();

		check_eq("fieldAt(0x44) == 1", buf[0x44], 1);
		check_eq("embedded vtable ptr installed at +0x4",
			 *(unsigned int *)(buf + 0x4) != 0xccccccccu, 1);
		check_eq("fieldAt(0x5c) == 0", *(int *)(buf + 0x5c), 0);
		check_eq("fieldAt(0x58) == 0", *(int *)(buf + 0x58), 0);
		check_eq("fieldAt(0x68) == 0", *(int *)(buf + 0x68), 0);
		check_eq("fieldAt(0x6c) == 0", *(int *)(buf + 0x6c), 0);
		check_eq("fieldAt(0x70) == 0", *(int *)(buf + 0x70), 0);
		check_eq("fieldAt(0x74) == 0", *(int *)(buf + 0x74), 0);
		double scaled = 48.0 * (double)abm.busGainReciprocal;
		check_float("fieldAt(0x78) == 48*busGainReciprocal", *(double *)(buf + 0x78), scaled);
		check_eq("fieldAt(0x88) == 0", *(int *)(buf + 0x88), 0);
		check_float("fieldAt(0x80) == 0.0", *(double *)(buf + 0x80), 0.0);
		check_eq("fieldAt(0x8c) == 0", *(int *)(buf + 0x8c), 0);
		check_eq("fieldAt(0x90) == 0", *(int *)(buf + 0x90), 0);
		check_eq("fieldAt(0x94) == 0", *(int *)(buf + 0x94), 0);
		check_float("fieldAt(0x98) == 48*busGainReciprocal", *(double *)(buf + 0x98), scaled);
		check_eq("fieldAt(0xa8) == 0", *(int *)(buf + 0xa8), 0);
		check_float("fieldAt(0xa0) == 0.0", *(double *)(buf + 0xa0), 0.0);
		check_eq("fieldAt(0xac) == 0", *(int *)(buf + 0xac), 0);
		check_eq("fieldAt(0xb0) == 0", *(int *)(buf + 0xb0), 0);
		check_eq("fieldAt(0xb4) == 0", *(int *)(buf + 0xb4), 0);
		check_float("fieldAt(0xb8) == 48*busGainReciprocal", *(double *)(buf + 0xb8), scaled);
		check_float("fieldAt(0xc0) == 0.0", *(double *)(buf + 0xc0), 0.0);
		check_eq("sInstance == this", (long)(CSTGMIDIClockSync::sInstance == mcs), 1);
		check_eq("fieldAt(0x60) == 0", *(int *)(buf + 0x60), 0);
		check_eq("fieldAt(0x64) == 0", *(int *)(buf + 0x64), 0);
		check_eq("fieldAt(0xc8) == -1", *(int *)(buf + 0xc8), -1);

		/* Base::Initialize() ran as part of the ctor too (called on the
		 * embedded sub-object at outer+0x4) -- confirm its own three
		 * effects landed at their OWN absolute offsets (embedded+0x8 ==
		 * outer+0xc, embedded+0xc == outer+0x10, embedded+0x14 ==
		 * outer+0x18) -- distinct fields from the ctor's own explicit
		 * list above, not a duplicate check. */
		check_eq("embedded Base::Initialize(): fieldAt(embed+0x8)==0 (outer+0xc)",
			 *(int *)(buf + 0xc), 0);
		check_float("embedded Base::Initialize(): fieldAt(embed+0xc)==48*busGainReciprocal (outer+0x10)",
			    *(double *)(buf + 0x10), scaled);
		check_eq("embedded Base::Initialize(): fieldAt(embed+0x14)==0 (outer+0x18)",
			 buf[0x18], 0);
		check_float("embedded Base::Initialize(): kMaxNormalizedTempo == 200*busGainReciprocal",
			    CSTGMIDIClockSyncBase::kMaxNormalizedTempo, 200.0f * abm.busGainReciprocal);
		(void)mcs;
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
