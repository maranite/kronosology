// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_clock_sync.cpp  -  host-side known-answer test for batch 21:
 * CSTGMIDIClockSync::CSTGMIDIClockSync(), CSTGMIDIClockSyncBase::
 * Initialize(), and the complete CSTGIntMIDIClockSync class, PLUS (later
 * pass) the 10 real (of 13 total) CSTGExtMIDIClockSync methods -- see
 * ../src/engine/midi_clock_sync.cpp.
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
#include <sys/mman.h>
#include "oa_engine_init.h"

/* Local minimal CSTGCPUInfo stand-in (matching test_midi_port_manager.cpp's
 * own established precedent for the exact same situation) rather than
 * `#include "oa_setup_global_resources.h"` directly -- that header pulls
 * in oa_internal.h's placement-`operator new(oa_size_t, void*)`, which
 * conflicts with this file's own `#include <new>` (both define the same
 * placement-new overload, a hard redefinition error). This test only
 * needs `sInstance`/`khz`, never calls the real ctor. */
struct CSTGCPUInfo {
	static CSTGCPUInfo *sInstance;
	unsigned int cpuCount, khz;
	float field8;
	int fieldC;
	float field10, field14, field18, field1c;
	int field20;
};

/* Matches this project's established `mmap(..., MAP_32BIT, ...)` fix
 * (test_global.cpp et al) -- fieldAt(0x60) is a packed 32-bit pointer on
 * the real target; a plain stack/heap address on this 64-bit host would
 * silently truncate when stored there and crash on reconstruction. */
static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

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

/* Same "_ZTVxxx" per-isolated-test convention, for the newly-real
 * CSTGExtMIDIClockSync (this pass) -- real storage lives in
 * bar2_stubs.cpp (not linked here). */
extern "C" unsigned char _ZTV20CSTGExtMIDIClockSync[40];
unsigned char _ZTV20CSTGExtMIDIClockSync[40];

/* CSTGCPUInfo::sInstance -- real storage lives in engine_startup_bits.cpp
 * (not linked here); this test only needs `khz` set directly, matching
 * this file's own established "mock ctor" precedent for
 * CSTGAudioBusManager above. */
CSTGCPUInfo *CSTGCPUInfo::sInstance;

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

	printf("\n[7] GetFilteredTempoBPM(unsigned int) const (batch 49)\n");
	{
		*(int *)(bankMgr + 0x97c750) = 0; /* mode 0 -> ShouldSyncExternalClock() == false */
		unsigned char innerBuf[0x30];
		unsigned char outerBuf[0x8];
		*(int *)(innerBuf + 0x2c) = 0;
		*(unsigned char **)outerBuf = innerBuf;
		CTimerManager::ms_poInstance = (CTimerManager *)outerBuf;

		unsigned char buf[0x200];
		memset(buf, 0xcc, sizeof(buf));
		CSTGMIDIClockSync *mcs = new (buf) CSTGMIDIClockSync();

		printf("  -- ctor-default state, internal sync (mode 0): index 0/1 both "
		       "== EXACTLY 120.0f (1500.0 * (48.0/1500.0) * 2.5) --\n");
		check_float("GetFilteredTempoBPM(0) == 120.0f", mcs->GetFilteredTempoBPM(0), 120.0f);
		check_float("GetFilteredTempoBPM(1) == 120.0f", mcs->GetFilteredTempoBPM(1), 120.0f);

		printf("  -- index >= 2 clamped to 0 (unsigned cmovae) --\n");
		check_float("GetFilteredTempoBPM(2) == GetFilteredTempoBPM(0)",
			    mcs->GetFilteredTempoBPM(2), mcs->GetFilteredTempoBPM(0));
		check_float("GetFilteredTempoBPM(0xffffffff) == GetFilteredTempoBPM(0)",
			    mcs->GetFilteredTempoBPM(0xffffffffu), mcs->GetFilteredTempoBPM(0));

		printf("  -- else-branch formula independently, with a poked non-default "
		       "smoothed interval --\n");
		*(double *)(buf + 0x98) = 0.05; /* index 0's own smoothed interval */
		*(double *)(buf + 0xb8) = 0.10; /* index 1's own smoothed interval */
		double want0 = (double)abm.busGainScale * 0.05 * 2.5;
		double want1 = (double)abm.busGainScale * 0.10 * 2.5;
		check_float("GetFilteredTempoBPM(0) == busGainScale*0.05*2.5",
			    mcs->GetFilteredTempoBPM(0), (float)want0);
		check_float("GetFilteredTempoBPM(1) == busGainScale*0.10*2.5",
			    mcs->GetFilteredTempoBPM(1), (float)want1);

		printf("  -- external sync active (mode 1) but fieldAt(0x60)==0: falls "
		       "through to the SAME else-branch formula --\n");
		*(int *)(bankMgr + 0x97c750) = 1;
		check_eq("fieldAt(0x60) confirmed still 0 (ctor zeroed it)",
			 *(int *)(buf + 0x60), 0);
		check_float("GetFilteredTempoBPM(0) unaffected by mode when fieldAt(0x60)==0",
			    mcs->GetFilteredTempoBPM(0), (float)want0);

		printf("  -- external sync active AND fieldAt(0x60) non-null: returns "
		       "(float)*(int*)(fieldAt(0x60)+0x1c4), formula NOT consulted --\n");
		unsigned char *extObj = mmap32(0x200);
		memset(extObj, 0, 0x200);
		*(int *)(extObj + 0x1c4) = 777;
		*(unsigned int *)(buf + 0x60) = (unsigned int)(unsigned long)extObj;
		check_float("GetFilteredTempoBPM(0) == 777.0f (int-to-float of fieldAt(0x60)+0x1c4)",
			    mcs->GetFilteredTempoBPM(0), 777.0f);
		check_float("GetFilteredTempoBPM(1) -- SAME external path, index irrelevant here",
			    mcs->GetFilteredTempoBPM(1), 777.0f);

		munmap(extObj, 0x200);
		*(unsigned int *)(buf + 0x60) = 0; /* restore for cleanliness */
		(void)mcs;
	}

	printf("\n[8] CSTGExtMIDIClockSync -- newly-real cluster (see oa_engine_init.h)\n");
	{
		CSTGCPUInfo cpu;
		memset(&cpu, 0, sizeof(cpu));
		cpu.khz = 1000000; /* 1000 * 1000000 = 1e9 Hz, exact */
		CSTGCPUInfo::sInstance = &cpu;

		unsigned char buf[0x200];
		memset(buf, 0, sizeof(buf));
		CSTGExtMIDIClockSync *o = (CSTGExtMIDIClockSync *)buf;

		o->Initialize();
		check_float("kSecondsToTimeStamp == 1e9 (1000*khz, exact)",
			    CSTGExtMIDIClockSync::kSecondsToTimeStamp, 1.0e9);
		check_float("kTimeStampToSeconds == 1/1e9",
			    CSTGExtMIDIClockSync::kTimeStampToSeconds, 1.0 / 1.0e9);
		check_float("kClockEarlyThresholdTicks == floor(-0.0004*1500) == -1",
			    CSTGExtMIDIClockSync::kClockEarlyThresholdTicks, -1.0f);
		check_eq("fieldAt(0xa4) zeroed", *(int *)(buf + 0xa4), 0);
		check_eq("fieldAt(0xa8) zeroed", *(int *)(buf + 0xa8), 0);
		check_float("fieldAt(0x1bc) (jitter) zeroed", *(float *)(buf + 0x1bc), 0.0f);
		check_float("fieldAt(0x1d4) == ceil(0.001*1500) == 2",
			    *(float *)(buf + 0x1d4), 2.0f);
		check_eq("fieldAt(0x1c4) (target tempo) == 0x78 (120)",
			 *(int *)(buf + 0x1c4), 0x78);

		printf("  -- second call (different object): static guard held, statics "
		       "unchanged even with a poisoned khz --\n");
		unsigned int savedKhz = cpu.khz;
		cpu.khz = 999;
		unsigned char buf2[0x200];
		memset(buf2, 0, sizeof(buf2));
		((CSTGExtMIDIClockSync *)buf2)->Initialize();
		check_float("kSecondsToTimeStamp unchanged on 2nd Initialize() (guard held)",
			    CSTGExtMIDIClockSync::kSecondsToTimeStamp, 1.0e9);
		cpu.khz = savedKhz;

		printf("  -- GetEventCount/GetEventStatusByte/ConsumeEvent (8-entry ring @ +0x40)\n");
		*(unsigned int *)(buf + 0xa4) = 5; /* write index */
		*(unsigned int *)(buf + 0xa8) = 2; /* read index */
		for (unsigned int i = 0; i < 8; i++)
			buf[0x40 + i * 0xc + 0x4] = (unsigned char)(0x90 + i);
		check_eq("GetEventCount == writeIdx - readIdx", o->GetEventCount(), 3);
		check_eq("GetEventStatusByte == ring[readIdx & 7]", o->GetEventStatusByte(), 0x90 + 2);
		o->ConsumeEvent();
		check_eq("ConsumeEvent advances read index", *(unsigned int *)(buf + 0xa8), 3);
		check_eq("GetEventStatusByte after consume", o->GetEventStatusByte(), 0x90 + 3);
		*(unsigned int *)(buf + 0xa8) = 9; /* wraparound: 9 & 7 == 1 */
		check_eq("GetEventStatusByte wraps mod 8", o->GetEventStatusByte(), 0x90 + 1);

		printf("  -- PrepareForNextTick() -- confirmed real no-op (genuinely "
		       "different from CSTGIntMIDIClockSync's own real override)\n");
		unsigned char before[0x200];
		memcpy(before, buf, sizeof(buf));
		o->PrepareForNextTick();
		check_eq("PrepareForNextTick() touched nothing", memcmp(before, buf, sizeof(buf)), 0);

		printf("  -- NotifySyncDetected() -- zero 2 ring buffers, reset counters, "
		       "recompute fieldAt(0x1d4)\n");
		*(int *)(buf + 0xb4) = 777;
		*(int *)(buf + 0xb8) = 777;
		*(int *)(buf + 0x1c0) = 777;
		memset(buf + 0xbc, 0xcc, 0x80);
		memset(buf + 0x13c, 0xcc, 0x80);
		o->NotifySyncDetected();
		check_eq("fieldAt(0xb4) zeroed", *(int *)(buf + 0xb4), 0);
		check_eq("fieldAt(0xb8) == -1", *(int *)(buf + 0xb8), -1);
		check_eq("fieldAt(0x1c0) zeroed", *(int *)(buf + 0x1c0), 0);
		bool ringsZero = true;
		for (unsigned int i = 0; i < 0x80; i++)
			if (buf[0xbc + i] != 0 || buf[0x13c + i] != 0)
				ringsZero = false;
		check_eq("both 0x80-byte float rings zeroed", ringsZero ? 1 : 0, 1);
		check_float("fieldAt(0x1d4) recomputed == ceil(0.001*1500) == 2",
			    *(float *)(buf + 0x1d4), 2.0f);

		printf("  -- GetClockLateThresholdTicks (dynamic)/GetClockEarlyThresholdTicks "
		       "(static)\n");
		*(float *)(buf + 0x1d4) = 42.0f;
		check_float("GetClockLateThresholdTicks == fieldAt(0x1d4) (dynamic, NOT a "
			    "constant, unlike CSTGIntMIDIClockSync)",
			    o->GetClockLateThresholdTicks(), 42.0f);
		check_float("GetClockEarlyThresholdTicks == static kClockEarlyThresholdTicks",
			    o->GetClockEarlyThresholdTicks(), -1.0f);

		printf("  -- UpdateFilteredTempo(double): debounce -- 32 misses, then adopt "
		       "on the 33rd\n");
		*(int *)(buf + 0x1c4) = 120; /* target */
		*(int *)(buf + 0x1c8) = 0;   /* debounce counter */
		for (int i = 1; i <= 32; i++) {
			o->UpdateFilteredTempo(40.0); /* predicted = 1500*40*2.5 = 150000, exact */
			check_eq("target unchanged during debounce window",
				 *(int *)(buf + 0x1c4), 120);
		}
		check_eq("debounce counter reached 32 after 32 misses", *(int *)(buf + 0x1c8), 32);
		o->UpdateFilteredTempo(40.0); /* 33rd call: 32 > 31 -> adopt */
		check_eq("target adopted on 33rd call", *(int *)(buf + 0x1c4), 150000);
		check_eq("debounce counter reset to 0 on adopt", *(int *)(buf + 0x1c8), 0);

		printf("  -- UpdateFilteredTempo(double): predicted == current target -> "
		       "immediate debounce reset, no re-adoption needed\n");
		*(int *)(buf + 0x1c8) = 17; /* poke a nonzero debounce count */
		o->UpdateFilteredTempo(40.0); /* SAME predicted (150000) == current target */
		check_eq("target still 150000 (already current)", *(int *)(buf + 0x1c4), 150000);
		check_eq("debounce counter reset to 0 immediately", *(int *)(buf + 0x1c8), 0);

		printf("  -- UpdateDynamicThresholds(): clamp(jitter, 0.001, 0.008) * "
		       "busGainScale, ceil\n");
		*(float *)(buf + 0x1bc) = 0.0005f; /* below min -> clamped to 0.001 */
		o->UpdateDynamicThresholds();
		check_float("clamped to min: ceil(0.001*1500) == 2", *(float *)(buf + 0x1d4), 2.0f);

		*(float *)(buf + 0x1bc) = 0.01f; /* above max -> clamped to 0.008 */
		o->UpdateDynamicThresholds();
		/* NOT 12: float32 0.008f's real bit pattern is
		 * 0.00800000037997961, so 0.008f*1500.0f == 12.00000057 (just
		 * over the integer boundary) -> ceil == 13, not the naively
		 * expected exact 12. */
		check_float("clamped to max: ceil(0.008f*1500.0f) == 13 (float32 0.008f is "
			    "slightly > 0.008, confirmed real)",
			    *(float *)(buf + 0x1d4), 13.0f);

		*(float *)(buf + 0x1bc) = 0.003f; /* within range, unclamped */
		o->UpdateDynamicThresholds();
		check_float("unclamped: ceil(0.003*1500) == ceil(4.5) == 5",
			    *(float *)(buf + 0x1d4), 5.0f);
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
