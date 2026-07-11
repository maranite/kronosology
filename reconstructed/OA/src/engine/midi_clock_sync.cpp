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

extern "C" unsigned char _ZTV20CSTGIntMIDIClockSync[40];

int CSTGMIDIClockSyncBase::kClockTimeOutTicks;
float CSTGMIDIClockSyncBase::kMaxNormalizedTempo;

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
