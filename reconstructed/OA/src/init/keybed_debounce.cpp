// SPDX-License-Identifier: GPL-2.0
/*
 * keybed_debounce.cpp  -  CSTGKeybedKeyDebounceFilter::Initialize().
 *
 * Ground truth: `_ZN27CSTGKeybedKeyDebounceFilter10InitializeEv`
 * (.text+0x33e990, 72 bytes). There is NO plain-C wrapper anywhere in
 * ground truth under either name -- this project's own
 * `CSTGKeybedKeyDebounceFilter_Initialize(unsigned char *filter)` name
 * (declared in oa_keybed_init.h) is simply a C-friendly alias for the
 * real mangled method itself, called with `filter` = `this` (the
 * sub-object embedded at `CSTGKeybedInterface::sInstance +
 * KEYBED_OFF_DEBOUNCE_FILTER`, per keybed_init.cpp's own confirmed
 * storage model). Given its own dedicated TU (rather than folding into
 * keybed_init.cpp) so it can pull in oa_engine.h (CSTGAudioBusManager)
 * without touching keybed_init.cpp's existing includes/tests.
 *
 * Confirmed real body (full objdump -dr trace):
 *   1. A 128-entry array of 20-byte (0x14) per-key debounce-state
 *      records lives at filter+0; this loop zeroes each record's own
 *      byte at +0x10 (the only field this function ever touches --
 *      plausibly a "debounced"/"pending" flag; exact meaning not
 *      independently confirmed, no other method of this filter is
 *      reconstructed in this project to cross-check against).
 *   2. filter+0xa10 (immediately past the 128*0x14=0xa00-byte array,
 *      plus 0x10 bytes of unconfirmed padding/other fields) gets a
 *      computed "debounce window" value:
 *          (int)(CSTGAudioBusManager::sInstance->busGainScale * 50.0f * 0.001f)
 *      -- confirmed real x87 sequence (`fld [.rodata.cst4]=50.0f;
 *      fmul [busManager+4]; fld [.rodata.cst8]=0.001(double); fmulp
 *      st(1),st; fistp`) against the two real constants (decoded via
 *      this project's own symtab-index/section-index method, sec
 *      10.176). Since `busGainScale` is itself a confirmed-constant
 *      1500.0f (see oa_engine.h), this always evaluates to exactly
 *      (int)75 on real hardware -- reusing an otherwise-unrelated
 *      bus-manager scale constant for what reads like "a 50ms window"
 *      is a real, preserved quirk (not a claim about what
 *      `busGainScale` "really" represents); the SAME field is read for
 *      an unrelated formula in `CSTGGlobal::UpdatePerfChangeHoldTime`
 *      (global.cpp). The value 75 has no fractional part after this
 *      exact multiplication chain (1500*50*0.001), so ordinary IEEE
 *      float/double math (no x87 extended-precision or FPU
 *      rounding-mode sensitivity) reproduces it bit-for-bit --
 *      confirmed safe to build this file with `-mhard-float -msse2
 *      -mfpmath=sse` (this project's established per-file override for
 *      exactly this class of formula, see engine/global.cpp et al.)
 *      rather than hand-writing x87 inline asm.
 *   3. No null-check on `CSTGAudioBusManager::sInstance` -- matches
 *      ground truth exactly (unconditional dereference). Relies on
 *      `CSTGAudioManager_StartAudioEngine()` (init_module step 13,
 *      which runs before `CSTGKeybedInterface_Startup()`'s step 14)
 *      having already constructed the bus manager singleton.
 */

#include "oa_keybed_init.h"
#include "oa_engine.h"

extern "C" void CSTGKeybedKeyDebounceFilter_Initialize(unsigned char *filter)
{
	for (int i = 0; i < 128; i++)
		filter[i * 0x14 + 0x10] = 0;

	float scale = CSTGAudioBusManager::sInstance->busGainScale;
	int window = (int)(scale * 50.0f * 0.001f);
	*(int *)(filter + 0xa10) = window;
}
