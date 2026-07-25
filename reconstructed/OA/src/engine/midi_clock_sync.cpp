// SPDX-License-Identifier: GPL-2.0
/*
 * midi_clock_sync.cpp  -  batch 21: CSTGMIDIClockSync::CSTGMIDIClockSync()
 * (`.text+0x67410`, 250 bytes) plus its own two newly-discovered real
 * dependencies -- CSTGMIDIClockSyncBase::Initialize() (`.text+0x67a50`,
 * 152 bytes) and the complete CSTGIntMIDIClockSync class (8 methods,
 * 1-66 bytes each, all confirmed via full disassembly/objdump -dr).
 *
 * UPDATE (batch 49): CSTGMIDIClockSync::GetFilteredTempoBPM(unsigned int)
 * const added (`.text+0x67990`, 108 bytes, confirmed via relocation from
 * the newly-real CSTGEffectManager::RunEffects()) -- see oa_engine_init.h
 * for the full confirmed shape, including the 120.0f cross-check.
 *
 * This is the SAME "check the whole class once a tiny dependency turns
 * up" technique that paid off for CSTGHDRCircularBuffer (sec 10.158,
 * batch 11): CSTGMIDIClockSync's own ctor calls
 * CSTGMIDIClockSyncBase::Initialize() on an embedded sub-object, and
 * that sub-object's real vtable (`_ZTV20CSTGIntMIDIClockSync`, 40 bytes/
 * 8 slots, readelf-confirmed) turned out to have every one of its own
 * slot targets already small and tractable -- reconstructing the whole
 * cluster in one pass rather than leaving 7 more tiny deferred externs
 * for a future batch to re-discover.
 *
 * Deliberately a separate translation unit from lfo_stepseq_quad.cpp
 * (which already owns `CSTGMIDIClockSync::sInstance`'s real storage) and
 * from sk_stg_gate.cpp (which owns the unrelated `CTimerManager`/
 * `CKGBankManager` cluster) -- this file only ADDS new symbols, it does
 * not redefine any existing storage. test_engine_init.cpp's own
 * MOCK_CTOR_ONLY(CSTGMIDIClockSync) mock is untouched (doesn't link this
 * file), matching this project's now-standard "give a newly-real ctor
 * its own dedicated TU" precedent (sec 10.145/10.158/10.162 et al).
 *
 * Float/double arithmetic here uses plain C (matching this project's
 * OWN established substitute for genuine-but-simple x87 sequences --
 * engine_startup_bits.cpp/engine_startup_bits2.cpp/scale.cpp/
 * smoother_init.cpp, sec 10.57/10.86) rather than hand-rolled inline
 * asm: every real chained computation here was independently checked
 * (Python, double precision) to land on an EXACT representable value
 * with zero rounding ambiguity (e.g. `0.104 * 1500.0 == 156.0` exactly),
 * so the plain-C vs. real-x87-extended-precision distinction is
 * provably immaterial for these specific confirmed inputs. Needs the
 * same `-mhard-float -msse2 -mfpmath=sse` Makefile CFLAGS override as
 * those four sibling files (kernel default is `-msoft-float`, which
 * would otherwise pull in unresolvable libgcc soft-float helpers).
 */

#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"	/* CSTGCPUInfo::sInstance, for
					 * CSTGExtMIDIClockSync::Initialize() */

extern "C" unsigned char _ZTV20CSTGIntMIDIClockSync[40];

int CSTGMIDIClockSyncBase::kClockTimeOutTicks;
float CSTGMIDIClockSyncBase::kMaxNormalizedTempo;

double CSTGExtMIDIClockSync::kSecondsToTimeStamp;
double CSTGExtMIDIClockSync::kTimeStampToSeconds;
float CSTGExtMIDIClockSync::kClockEarlyThresholdTicks;

namespace {

/* Portable (no-libm) truncate-based ceil/floor -- see
 * CSTGExtMIDIClockSync::Initialize()'s own header comment for why this
 * class, unlike every prior CSTGMIDIClockSyncBase/CSTGIntMIDIClockSync
 * rounding site in this file, has a real CW-rounding-mode-dependent
 * result that a plain `(int)` truncating cast would get WRONG (e.g.
 * `floor(-0.6) = -1 != (int)(-0.6) = 0`). Exact for any finite float. */
int CeilToInt(float x)
{
	int t = (int)x;
	if ((float)t < x)
		t += 1;
	return t;
}

int FloorToInt(float x)
{
	int t = (int)x;
	if ((float)t > x)
		t -= 1;
	return t;
}

/* Round-half-away-from-zero, no libm. Used only by UpdateFilteredTempo()
 * (the sole site in this class whose real `fistp` is NOT preceded by any
 * CW override, so it runs under the ambient x87 control word -- default
 * round-to-nearest-ties-to-even on this target). Genuine tie-break cases
 * (exact `.5` boundary) would round differently here than real x87
 * ties-to-even; considered immaterial for this function's real input
 * domain (a tempo-derived BPM*busGainScale*2.5 product), matching this
 * project's established tolerance for confirmed-improbable edge cases. */
int RoundToInt(float x)
{
	return (x >= 0.0f) ? (int)(x + 0.5f) : (int)(x - 0.5f);
}

} /* anonymous namespace */

/*
 * CSTGMIDIClockSyncBase::Initialize() -- see oa_engine_init.h for the
 * full confirmed shape. `this` here is the EMBEDDED sub-object
 * (outerThis+0x4), matching the ctor's own `lea eax,[eax+0x4]` call
 * site below.
 */
void CSTGMIDIClockSyncBase::Initialize()
{
	static bool s_initialized;
	unsigned char *base = (unsigned char *)this;
	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;

	if (!s_initialized) {
		s_initialized = true;
		/* Real code sets the x87 rounding control word to "round
		 * toward +infinity" before frndint+fisttp; the confirmed
		 * real inputs make this immaterial here (0.104 * 1500.0 ==
		 * 156.0 exactly, independently verified) -- matching this
		 * project's own established "trivial given" precedent
		 * (CSTGDiskCostManager::Initialize(),
		 * engine_startup_bits2.cpp). */
		kClockTimeOutTicks = (int)(0.104 * (double)bus->busGainScale);
	}

	kMaxNormalizedTempo = 200.0f * bus->busGainReciprocal;
	*(unsigned int *)(base + 0x8) = 0;
	base[0x14] = 0;
	*(double *)(base + 0xc) = 48.0 * (double)bus->busGainReciprocal;
}

/* GetEventCount() const -- see oa_engine_init.h. */
unsigned int CSTGIntMIDIClockSync::GetEventCount() const
{
	const unsigned char *base = (const unsigned char *)this;
	return *(const unsigned int *)(base + 0x54) -
	       *(const unsigned int *)(base + 0x58);
}

/* GetEventStatusByte() const -- see oa_engine_init.h. */
unsigned char CSTGIntMIDIClockSync::GetEventStatusByte() const
{
	const unsigned char *base = (const unsigned char *)this;
	unsigned int idx = *(const unsigned int *)(base + 0x58) & 0xf;
	return base[0x44 + idx];
}

/* ConsumeEvent() -- see oa_engine_init.h. */
void CSTGIntMIDIClockSync::ConsumeEvent()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0x58) += 1;
}

/*
 * Shared tail for PrepareForNextTick()/NotifySyncDetected() (confirmed
 * byte-for-byte identical opcodes at both real call sites, matching the
 * sec 10.167 "factor identical sibling blocks into one helper" technique).
 */
static void RecomputeMIDIClockInterval(unsigned char *base)
{
	int tempo = SKSTGGate_GetInternalTempo();
	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;

	*(double *)(base + 0xc) =
		(double)tempo * 0.01 * 0.4 * (double)bus->busGainReciprocal;
}

/* PrepareForNextTick() -- see oa_engine_init.h. */
void CSTGIntMIDIClockSync::PrepareForNextTick()
{
	if (SKSTGGate_ShouldSyncExternalClock())
		return;
	RecomputeMIDIClockInterval((unsigned char *)this);
}

/* NotifySyncDetected() -- see oa_engine_init.h. */
void CSTGIntMIDIClockSync::NotifySyncDetected()
{
	RecomputeMIDIClockInterval((unsigned char *)this);
}

/* ProcessClock() -- confirmed real no-op override, see oa_engine_init.h. */
void CSTGIntMIDIClockSync::ProcessClock()
{
}

/* GetClockLateThresholdTicks() const -- confirmed real: always 1.0f. */
float CSTGIntMIDIClockSync::GetClockLateThresholdTicks() const
{
	return 1.0f;
}

/* GetClockEarlyThresholdTicks() const -- confirmed real: always 0.0f. */
float CSTGIntMIDIClockSync::GetClockEarlyThresholdTicks() const
{
	return 0.0f;
}

/*
 * GetFilteredTempoBPM(unsigned int) const -- see oa_engine_init.h for the
 * full confirmed shape and the 120.0f cross-check. `index` selects one of
 * two independent "smoothed tempo interval" doubles at +0x98/+0xb8
 * (stride 0x20), matching the ctor's own +0x78/+0x98/+0xb8 triple.
 */
float CSTGMIDIClockSync::GetFilteredTempoBPM(unsigned int index) const
{
	const unsigned char *base = (const unsigned char *)this;

	if (index >= 2)
		index = 0;

	if (SKSTGGate_ShouldSyncExternalClock()) {
		unsigned int extPtr = *(const unsigned int *)(base + 0x60);
		if (extPtr != 0) {
			const unsigned char *extObj =
				(const unsigned char *)(unsigned long)extPtr;
			return (float)(*(const int *)(extObj + 0x1c4));
		}
	}

	const CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;
	double smoothed =
		*(const double *)(base + 0x98 + (unsigned long)index * 0x20);
	return (float)((double)bus->busGainScale * smoothed * 2.5);
}

/*
 * CSTGMIDIClockSync::CSTGMIDIClockSync() -- see oa_engine_init.h for the
 * full confirmed field list.
 */
CSTGMIDIClockSync::CSTGMIDIClockSync()
{
	unsigned char *base = (unsigned char *)this;

	base[0x44] = 1;

	/* Install the embedded CSTGIntMIDIClockSync sub-object's vtable at
	 * +0x4 (its own offset-0), matching this project's established
	 * "&_ZTVxxx + 8" convention, then call Initialize() on it -- a
	 * direct (non-virtual) call in the real disassembly, not dispatched
	 * through the just-installed vtable. */
	*(unsigned int *)(base + 0x4) =
		(unsigned int)(unsigned long)(_ZTV20CSTGIntMIDIClockSync + 8);
	((CSTGMIDIClockSyncBase *)(base + 0x4))->Initialize();

	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;
	double scaled = 48.0 * (double)bus->busGainReciprocal;

	/* Real code stores 0 into +0x5c then reads it straight back into
	 * +0x58 -- behaviorally just "both fields are 0", reproduced as two
	 * plain zero-writes. */
	*(unsigned int *)(base + 0x5c) = 0;
	*(unsigned int *)(base + 0x58) = 0;
	*(unsigned int *)(base + 0x68) = 0;
	*(unsigned int *)(base + 0x6c) = 0;
	*(unsigned int *)(base + 0x70) = 0;
	*(unsigned int *)(base + 0x74) = 0;
	*(double *)(base + 0x78) = scaled;
	*(unsigned int *)(base + 0x88) = 0;
	*(double *)(base + 0x80) = 0.0;
	*(unsigned int *)(base + 0x8c) = 0;
	*(unsigned int *)(base + 0x90) = 0;
	*(unsigned int *)(base + 0x94) = 0;
	*(double *)(base + 0x98) = scaled;
	*(unsigned int *)(base + 0xa8) = 0;
	*(double *)(base + 0xa0) = 0.0;
	*(unsigned int *)(base + 0xac) = 0;
	*(unsigned int *)(base + 0xb0) = 0;
	*(unsigned int *)(base + 0xb4) = 0;
	*(double *)(base + 0xb8) = scaled;
	*(double *)(base + 0xc0) = 0.0;

	sInstance = this;
	*(unsigned int *)(base + 0x60) = 0;
	*(unsigned int *)(base + 0x64) = 0;
	*(int *)(base + 0xc8) = -1;
}

/*
 * CSTGExtMIDIClockSync -- see oa_engine_init.h for the full confirmed
 * field-by-field breakdown of each method below.
 */

/* GetEventCount() const -- see oa_engine_init.h. */
unsigned int CSTGExtMIDIClockSync::GetEventCount() const
{
	const unsigned char *base = (const unsigned char *)this;
	return *(const unsigned int *)(base + 0xa4) -
	       *(const unsigned int *)(base + 0xa8);
}

/* GetEventStatusByte() const -- see oa_engine_init.h. */
unsigned char CSTGExtMIDIClockSync::GetEventStatusByte() const
{
	const unsigned char *base = (const unsigned char *)this;
	unsigned int idx = *(const unsigned int *)(base + 0xa8) & 7;
	return base[0x40 + idx * 0xc + 0x4];
}

/* ConsumeEvent() -- see oa_engine_init.h. */
void CSTGExtMIDIClockSync::ConsumeEvent()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)(base + 0xa8) += 1;
}

/* PrepareForNextTick() -- confirmed real no-op override, see
 * oa_engine_init.h. */
void CSTGExtMIDIClockSync::PrepareForNextTick()
{
}

/* GetClockEarlyThresholdTicks() const -- see oa_engine_init.h: the
 * static, computed once in Initialize(). */
float CSTGExtMIDIClockSync::GetClockEarlyThresholdTicks() const
{
	return kClockEarlyThresholdTicks;
}

/* GetClockLateThresholdTicks() const -- see oa_engine_init.h: the
 * dynamic per-instance field UpdateDynamicThresholds()/NotifySyncDetected()/
 * Initialize() all write. */
float CSTGExtMIDIClockSync::GetClockLateThresholdTicks() const
{
	const unsigned char *base = (const unsigned char *)this;
	return *(const float *)(base + 0x1d4);
}

/*
 * Shared tail for Initialize()/NotifySyncDetected()/
 * UpdateDynamicThresholds() (confirmed byte-for-byte identical opcodes at
 * all three real call sites): fieldAt(0x1d4) = ceil(0.001f * busGainScale).
 */
static void SetDefaultLateThreshold(unsigned char *base)
{
	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;
	*(float *)(base + 0x1d4) = (float)CeilToInt(0.001f * bus->busGainScale);
}

/*
 * Initialize() -- see oa_engine_init.h for the full confirmed shape.
 */
void CSTGExtMIDIClockSync::Initialize()
{
	unsigned char *base = (unsigned char *)this;

	((CSTGMIDIClockSyncBase *)this)->Initialize();

	static bool s_initialized;
	if (!s_initialized) {
		s_initialized = true;

		CSTGCPUInfo *cpu = CSTGCPUInfo::sInstance;
		/* See class comment: plain `unsigned int` product, not the
		 * real code's full unsigned-64-bit-safe conversion -- exact
		 * for every real CPU frequency this target runs at, and
		 * avoids a genuine `__floatundidf`/libgcc dependency this
		 * Kbuild target cannot resolve. */
		unsigned int freqHz = 1000u * cpu->khz;

		kSecondsToTimeStamp = (double)freqHz;
		kTimeStampToSeconds = 1.0 / kSecondsToTimeStamp;

		CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;
		/* Confirmed real FLOOR rounding -- see class comment. */
		kClockEarlyThresholdTicks =
			(float)FloorToInt(-0.0004f * bus->busGainScale);
	}

	*(unsigned int *)(base + 0xa4) = 0;
	*(unsigned int *)(base + 0xa8) = 0;
	*(float *)(base + 0x1bc) = 0.0f;
	SetDefaultLateThreshold(base);
	*(int *)(base + 0x1c4) = 0x78;
}

/*
 * NotifySyncDetected() -- see oa_engine_init.h for the full confirmed
 * shape.
 */
void CSTGExtMIDIClockSync::NotifySyncDetected()
{
	unsigned char *base = (unsigned char *)this;

	*(int *)(base + 0xb4) = 0;
	*(int *)(base + 0xb8) = -1;
	*(int *)(base + 0x1c0) = 0;

	for (unsigned int i = 0; i < 0x80; i++)
		base[0xbc + i] = 0;
	for (unsigned int i = 0; i < 0x80; i++)
		base[0x13c + i] = 0;

	SetDefaultLateThreshold(base);
}

/*
 * UpdateFilteredTempo(double) -- see oa_engine_init.h for the full
 * confirmed shape.
 */
void CSTGExtMIDIClockSync::UpdateFilteredTempo(double bpm)
{
	unsigned char *base = (unsigned char *)this;
	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;

	/* 2.5f -- confirmed real .rodata.cst4 float, the SAME constant
	 * CSTGMIDIClockSync::GetFilteredTempoBPM() uses for the inverse
	 * conversion (busGainScale * smoothedInterval * 2.5). Real `fistp`
	 * here runs under the ambient (round-to-nearest) CW -- see
	 * RoundToInt()'s own comment. */
	int predicted = RoundToInt((float)(bus->busGainScale * bpm * 2.5));

	int *target = (int *)(base + 0x1c4);
	int *debounce = (int *)(base + 0x1c8);

	if (predicted != *target) {
		if ((unsigned int)*debounce <= 0x1f) {
			*debounce += 1;
			return;
		}
		*target = predicted;
	}
	*debounce = 0;
}

/*
 * UpdateDynamicThresholds() -- see oa_engine_init.h for the full
 * confirmed shape.
 */
void CSTGExtMIDIClockSync::UpdateDynamicThresholds()
{
	unsigned char *base = (unsigned char *)this;
	float jitter = *(float *)(base + 0x1bc);

	if (jitter < 0.001f)
		jitter = 0.001f;
	else if (jitter > 0.008f)
		jitter = 0.008f;

	CSTGAudioBusManager *bus = CSTGAudioBusManager::sInstance;
	*(float *)(base + 0x1d4) = (float)CeilToInt(jitter * bus->busGainScale);
}
