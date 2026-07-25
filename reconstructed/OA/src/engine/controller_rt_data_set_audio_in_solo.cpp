// SPDX-License-Identifier: GPL-2.0
/*
 * controller_rt_data_set_audio_in_solo.cpp -- CSTGControllerRTData::
 * SetAudioInSolo(unsigned int, bool) (batch 57).
 *
 * Deliberately a SEPARATE translation unit from global.cpp (its natural
 * home, since it reuses global.cpp's own `ResolveCurrentPerformance()`
 * helper): test_global.cpp carries a load-bearing call-tracking mock of
 * this exact symbol (~2 assertions, "SetAudioInSolo called once"),
 * test_engine.cpp/test_global_ctor.cpp each carry their own trivial
 * link-satisfying empty-body mocks -- all three link global.cpp directly,
 * so none of them link this file, matching this project's established
 * "give it its own TU" technique (WriteSTGMidiOutQueue, sec 10.145;
 * CSTGMidiQueueWriter::Write, sec 10.83).
 */

#include "oa_global.h"

/* Forward declaration only -- real definition (and full derivation
 * comment) lives in global.cpp, no longer `static` there as of this
 * batch specifically so this file can share it instead of duplicating
 * the 3-way mode-dispatch formula. */
CSTGPerformance *ResolveCurrentPerformance(unsigned char *base);

/*
 * CSTGControllerRTData::SetAudioInSolo(unsigned int, bool) (batch 57,
 * .text+0x1d5e0, 303 bytes) -- confirmed real. This class has no
 * confirmed field layout (oa_global.h), so raw offsets are used directly
 * on `this`, matching this project's established convention for classes
 * without a full field layout:
 *   +0x21  bit1 = "single-solo" mode gate (tested only when solo==true)
 *   +0x22/+0x24  two 16-bit fields, cleared to 0 only on the
 *                single-solo-mode assignment path below
 *   +0x26  soloBits -- a per-slot bitmask (1 bit per audio-input slot)
 * If solo==false: soloBits &= ~(1<<slot) (plain bit clear, no other side
 * effects -- confirmed via the real `rol $CL,0xfffffffe` idiom, which is
 * exactly `~(1<<slot)` for slot<32, just computed by rotating an
 * all-ones-except-bit0 mask instead of inverting a shifted 1). If
 * solo==true:
 *   - if +0x21 bit1 is set ("single-solo" mode): zero +0x22/+0x24, then
 *     OVERWRITE soloBits with just (1<<slot) (only one slot can be
 *     soloed at a time in this mode -- an assignment, not an OR).
 *   - otherwise: soloBits |= (1<<slot) (normal multi-solo mode).
 * In both cases, finally notifies the current edit-context performance:
 * resolves it via global.cpp's already-real `ResolveCurrentPerformance()`
 * (same CSTGGlobal +0x684-mode/+0x68c..+0x6a0-index formula as
 * CSTGAudioInput::UpdateBusSelect, global.cpp -- see that function's own
 * header comment for the confirmed 3-branch derivation) and dispatches
 * through its vtable slot 27 (raw byte offset 0x6c past the installed
 * "+8" entries pointer).
 *
 * CONFIRMED via `readelf -r` against ground truth's own
 * `_ZTV15CSTGPerformance` relocation table that this exact slot (raw
 * vtable byte offset 0x74) resolves to `__cxa_pure_virtual` for BOTH real
 * derived types that ever install this vtable (`CSTGProgram`/`CSTGCombi`
 * share the identical base vtable pointer, program_ctor.cpp/
 * combi_ctor.cpp -- neither overrides this slot anywhere in the whole
 * binary). i.e. this dispatch is a confirmed-real, PROVABLY NEVER-TAKEN
 * code path in ground truth itself (a defensive pure-virtual trap, not
 * live behavior) -- so leaving this project's own `_ZTV15CSTGPerformance`
 * placeholder zero-filled (oa_global.h) doesn't introduce any NEW crash
 * risk beyond what the real binary already has at this exact call site;
 * matches the "install vs dispatch" precedent (bar2_stubs.cpp) for a
 * vtable slot confirmed DEAD rather than merely unconfirmed.
 */
void CSTGControllerRTData::SetAudioInSolo(unsigned int slot, bool solo)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int bit = 1u << (slot & 0x1f);

	if (!solo) {
		base[0x26] = (unsigned char)(base[0x26] & ~bit);
	} else if (base[0x21] & 0x2) {
		*(unsigned short *)(base + 0x22) = 0;
		*(unsigned short *)(base + 0x24) = 0;
		base[0x26] = (unsigned char)bit;
	} else {
		base[0x26] = (unsigned char)(base[0x26] | bit);
	}

	CSTGPerformance *perf = ResolveCurrentPerformance((unsigned char *)CSTGGlobal::sInstance);
	void **vtable = *(void ***)perf;
	typedef void (*Fn)(CSTGPerformance *);
	((Fn)vtable[27])(perf);
}
