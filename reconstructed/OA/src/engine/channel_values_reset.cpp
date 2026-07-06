// SPDX-License-Identifier: GPL-2.0
/*
 * channel_values_reset.cpp  -  CSTGChannelValues::Reset() (batch 18,
 * `.text+0x26aa0`, 569 bytes).
 *
 * Deliberately a SEPARATE translation unit from global.cpp AND from
 * channel_values_set_controller_value.cpp, matching the established
 * per-unit-file convention (sec 10.83/10.145/10.159/10.162): `verify/
 * test_engine.cpp` and `verify/test_global_ctor.cpp` both carry
 * pre-existing trivial no-op mocks of this exact symbol, and `verify/
 * test_global.cpp` carries a LOAD-BEARING call-counting mock
 * (`g_channelValuesResetCalls`/`g_lastChannelValuesResetThis`) -- none of
 * those three link this new file, so their mocks are left completely
 * untouched. This real body is instead exercised by its own dedicated
 * `verify/test_channel_values_reset.cpp`.
 *
 * Confirmed layout (regparm(3): this=EAX, no other args):
 *   (1) Lazy static init, IDENTICAL to the already-real Initialize()
 *       (sec 10.151): if `!sTemplateReady`, call `InitializeLongHand()`
 *       (still a confirmed-real, deliberately deferred no-op stub -- safe
 *       to call into, matches this project's established "calling a
 *       still-deferred stub is fine" precedent) and set `sTemplateReady=1`.
 *       Unconditionally memcpy `sTemplate` (0x92c bytes) over `this`.
 *   (2) Reads `CSTGGlobal::sInstance->fieldAt(0x6c0)`/`+0x6c1` -- the SAME
 *       two per-object "VJSX"/"VJSY assignment selector" signed-byte
 *       fields `CSTGGlobal::UpdateVJSXAssignment()`/`UpdateVJSYAssignment()`
 *       already write (oa_global.h's own confirmed doc, "selector +0x6c0"/
 *       "+0x6c1" -- both ARE plain `CSTGGlobal` fields, not
 *       `CSTGControllerRTData` sub-object fields, confirmed by checking
 *       which `class` block those methods are declared in, so there is NO
 *       offset conflict with `CSTGControllerRTData`'s own embedded `+0x10`
 *       sub-object despite the numeric coincidence at first glance).
 *   (3) For each of the two selector values that is in the valid CC-id
 *       range `[0, 0x77]` (a negative byte, read via `movsbl`, means "no
 *       CC assigned" and is skipped -- confirmed via a real `js`/`ja`
 *       bounds check pair, same idiom as `SetControllerValue`'s own
 *       `cc > 0x77` gate): applies the exact effect
 *       `SetControllerValue(cc, {field0=0.5f, field4=0.0f, field8=0x40,
 *       fieldA=1, fieldB=(origFieldB|1)&~2})` would have (i.e. "reset this
 *       CC to its centered/default value") -- confirmed BYTE-FOR-BYTE
 *       equivalent to that function's own already-reconstructed logic (see
 *       channel_values_set_controller_value.cpp) via full independent FPU-
 *       stack tracing of both the `b3==1`->0.0f / `b3!=1`->0.5f "chosen"
 *       selection and the `b6==1`/`b6==2` computed-array side effects.
 *       The real binary does NOT call `SetControllerValue` from here (no
 *       relocation to that symbol anywhere in `Reset()`'s own disassembly)
 *       -- it is a SEPARATELY inlined duplicate, confirmed via two
 *       independently-disassembled, byte-identical-shaped blocks (one per
 *       selector), matching sec 10.163's "inlined helper is not the same
 *       as a call" finding. Factored into one shared static helper here
 *       anyway (the real binary just duplicates the code twice with no
 *       shared subroutine call either) -- same precedent as
 *       `CSTGSlotVoiceData::FreeSlotVoiceData(bool)`'s shared
 *       `UnlinkFromOwnerList` helper, sec 10.164.
 *   (4) Both selector applications happen unconditionally in sequence --
 *       even when the FIRST selector's own `CSTGCCInfo::sCCInfoTable[cc].b1`
 *       is `0` (gate closed, no `computed[]` write), the raw per-cc struct
 *       write STILL happens for that cc, and the SECOND selector is still
 *       processed regardless (confirmed real: the disassembly's own
 *       "skip to channel B" targets land past the raw-write, not past the
 *       whole per-cc block).
 */

#include "oa_global.h"
#include "oa_engine_init.h"

static inline float CVRFMul(float a, float b)
{
	float result;
	__asm__ __volatile__(
		"flds %1\n\t"
		"flds %2\n\t"
		"fmulp %%st,%%st(1)\n\t"
		"fstps %0"
		: "=m" (result)
		: "m" (a), "m" (b)
	);
	return result;
}

static inline float CVRFAdd(float a, float b)
{
	float result;
	__asm__ __volatile__(
		"flds %1\n\t"
		"flds %2\n\t"
		"faddp %%st,%%st(1)\n\t"
		"fstps %0"
		: "=m" (result)
		: "m" (a), "m" (b)
	);
	return result;
}

/*
 * ApplyDefaultControllerValue() -- shared helper for the per-CC "reset to
 * center" logic Reset() performs identically for both the VJSX and VJSY
 * selector CC ids. See the file header comment above for the full
 * derivation.
 */
static void ApplyDefaultControllerValue(CSTGChannelValues *cv, unsigned char cc)
{
	unsigned char *base = (unsigned char *)cv;
	unsigned char *rec = base + (unsigned int)cc * 12;

	/* Read the ORIGINAL fieldB before any of the writes below (matches
	 * the real disassembly's own instruction order -- no aliasing risk
	 * either way, since field0/field4/field8/fieldA/fieldB are disjoint
	 * byte ranges of the 12-byte CSTGControllerValue record). */
	unsigned char fieldB = (unsigned char)((rec[0xb] | 0x1) & ~0x2);

	*(float *)(rec + 0x0) = 0.5f;
	*(float *)(rec + 0x4) = 0.0f;
	*(unsigned short *)(rec + 0x8) = 0x40;
	rec[0xa] = 1;
	rec[0xb] = fieldB;
	base[0x8b4 + cc] = 0x40;

	const unsigned char *entry = CSTGCCInfo::sCCInfoTable + (unsigned int)cc * 10;
	unsigned char b1 = entry[1];
	if (b1 == 0)
		return;

	float chosen = (entry[3] == 1) ? 0.0f : 0.5f;
	*(float *)(base + 0x628 + (unsigned int)b1 * 4) = chosen;

	signed char b6 = (signed char)entry[6];
	if (b6 == 1) {
		unsigned char b7 = entry[7];
		unsigned int halfBits;
		float half = 0.5f;
		__builtin_memcpy(&halfBits, &half, sizeof(halfBits));
		*(unsigned int *)(base + 0x628 + (unsigned int)b7 * 4) = halfBits;
	} else if (b6 == 2) {
		unsigned char b7 = entry[7];
		float scale = *(float *)(base + 0x630);
		float result = CVRFAdd(chosen, CVRFMul(0.5f, scale));
		*(float *)(base + 0x628 + (unsigned int)b7 * 4) = result;
	}
}

void CSTGChannelValues::Reset()
{
	if (!sTemplateReady) {
		InitializeLongHand();
		sTemplateReady = 1;
	}
	__builtin_memcpy((void *)this, sTemplate, sizeof(sTemplate));

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	signed char vjsX = (signed char)g[0x6c0];
	signed char vjsY = (signed char)g[0x6c1];

	if (vjsX >= 0 && (unsigned char)vjsX <= 0x77)
		ApplyDefaultControllerValue(this, (unsigned char)vjsX);
	if (vjsY >= 0 && (unsigned char)vjsY <= 0x77)
		ApplyDefaultControllerValue(this, (unsigned char)vjsY);
}
