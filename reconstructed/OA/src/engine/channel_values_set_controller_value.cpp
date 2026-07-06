// SPDX-License-Identifier: GPL-2.0
/*
 * channel_values_set_controller_value.cpp  -  CSTGChannelValues::
 * SetControllerValue(unsigned char, const CSTGControllerValue&) (batch 15,
 * `.text+0x26df0`, 240 bytes).
 *
 * Deliberately a SEPARATE translation unit from global.cpp (matching the
 * CSTGMidiQueueWriter::Write / WriteSTGMidiOutQueue precedent, sec
 * 10.83/10.145/10.159): `verify/test_engine.cpp` and
 * `verify/test_global_ctor.cpp` both carry pre-existing trivial no-op
 * mocks of this exact symbol, and `verify/test_global.cpp` carries a
 * LOAD-BEARING call-recording mock (`g_lastSetControllerValueThis` etc,
 * used by its own "SetControllerValue called twice per channel" assertion
 * at line ~5177) -- none of those three link this new file, so their
 * mocks are left completely untouched. This real body is instead
 * exercised by its own dedicated `verify/test_channel_values.cpp`.
 *
 * This function was blocked from sec 10.153 through sec 10.160 (see
 * bar2_stubs.cpp's own now-removed comment, and the agent-memory workflow
 * notes) specifically by `CSTGCCInfo::sCCInfoTable` not existing yet --
 * batch 14 (sec 10.161) resolved that table, unblocking this one.
 *
 * Confirmed layout (regparm(3): this=EAX, cc=DL (only low byte read), value=ECX):
 *   - `cc > 0x77` (119): early return, no effect at all (0..119 is the full
 *     120-entry `CSTGCCInfo::sCCInfoTable` domain, confirmed exactly
 *     matching sec 10.161's own 120-entry/10-byte-stride table).
 *   - `this->rawArray[cc] = value` UNCONDITIONALLY (three raw dword copies
 *     of `field0`/`field4`/the packed `field8|fieldA<<16|fieldB<<24`
 *     dword -- i.e. a plain struct-copy of the whole 12-byte
 *     `CSTGControllerValue`, at `this+0 + cc*12`; matches the exact same
 *     3-dword copy shape already confirmed for `SetPitchBend`'s own
 *     `this+0x5a0` slot, sec 10.128 -- strong independent cross-
 *     confirmation this 120-entry array occupies `this+0x000..+0x5a0`
 *     exactly, since `120*12 == 0x5a0`, the SAME offset `SetPitchBend`
 *     already uses for its own single extra slot right after it).
 *   - `value.fieldA` (the "type" byte) selects one of two shapes:
 *     - type IN {3,4,5}: skip the mirror-byte step below entirely.
 *     - type is anything else: mirror `value.field8` (an `unsigned short`,
 *       confirmed via the real `movzx ebx, word[ecx+8]` -- NOT a byte)
 *       LOW BYTE into `this->mirror8[cc]` at `this+0x8b4+cc` (120-byte
 *       array, ending exactly at `this+0x92c`, this class's own already-
 *       confirmed total size, sec 10.151 -- independent confirmation this
 *       array is the LAST field in the class).
 *   - Both shapes then converge on the SAME shared logic (confirmed via
 *     `CSTGCCInfo::sCCInfoTable[cc]`'s own 10-byte-stride fields, using
 *     this project's own already-named `b0`.."b9" convention from sec
 *     10.161):
 *     - `table[cc].b1 == 0`: gate closed, return (no further effect at
 *       all, even though the raw-array copy above already happened).
 *     - otherwise, pick `value` = `value.field4` (a real float) if
 *       `table[cc].b3 == 1`, else `value.field0` (reinterpreted AS a
 *       float bit pattern, matching this project's own already-
 *       established `CSTGControllerValue::field0` "raw bytes, sometimes a
 *       float" convention, sec 10.115).
 *     - ALWAYS store `this->computed[table[cc].b1] = value` (a float,
 *       4-byte-stride array at `this+0x628`, confirmed via the real `fst`
 *       -- non-popping, since the value is still needed below).
 *     - `table[cc].b6 == 1`: ALSO store
 *       `this->computed[table[cc].b7] = value.field0_as_raw_int` (an
 *       INTEGER copy -- re-reads `value.field0` directly, ignoring
 *       whichever of field0/field4 was picked above for the float write).
 *     - `table[cc].b6 == 2`: ALSO store
 *       `this->computed[table[cc].b7] = value + 0.5f * this->fieldAt(0x630)`
 *       (confirmed real float constant `0.5f`, `.rodata.cst4+0x74`;
 *       `this+0x630` is a fixed, NOT per-cc-indexed, scalar field --
 *       coincidentally `computed[2]` if the `computed` array's own
 *       indexing were extended that far, but never written by THIS
 *       function, so left as a raw offset rather than implying it's
 *       really `computed[2]`'s own confirmed real use).
 *     - any other `table[cc].b6` value: no additional store.
 *
 * `computed`/`mirror8` are declared here as raw offsets (not named struct
 * members in oa_engine_init.h) rather than sized arrays, since neither
 * array's own real element COUNT is independently confirmed beyond "big
 * enough to hold whatever `table[cc].b1`/`b7` byte values actually occur"
 * -- matches this project's established convention for partially-modeled
 * classes (e.g. CSTGControllerRTData/CSTGMidiPortManager's own raw-offset
 * fields).
 */

#include "oa_global.h"
#include "oa_engine_init.h"

static inline float SCVFMul(float a, float b)
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

static inline float SCVFAdd(float a, float b)
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

void CSTGChannelValues::SetControllerValue(unsigned char ccNumber, const CSTGControllerValue &value)
{
	if (ccNumber > 0x77)
		return;

	unsigned char *base = (unsigned char *)this;

	/* Unconditional 12-byte raw-array copy: this+0 + cc*12. */
	CSTGControllerValue *raw = (CSTGControllerValue *)(base + (unsigned int)ccNumber * 12);
	*raw = value;

	if (value.fieldA != 3 && value.fieldA != 4 && value.fieldA != 5) {
		base[0x8b4 + ccNumber] = (unsigned char)(value.field8 & 0xff);
	}

	const unsigned char *entry = CSTGCCInfo::sCCInfoTable + (unsigned int)ccNumber * 10;
	unsigned char b1 = entry[1];
	if (b1 == 0)
		return;

	unsigned int rawField0 = value.field0;
	float fval;
	__builtin_memcpy(&fval, &rawField0, sizeof(fval));
	float chosen = (entry[3] == 1) ? value.field4 : fval;

	*(float *)(base + 0x628 + (unsigned int)b1 * 4) = chosen;

	signed char b6 = (signed char)entry[6];
	if (b6 == 1) {
		unsigned char b7 = entry[7];
		*(unsigned int *)(base + 0x628 + (unsigned int)b7 * 4) = value.field0;
	} else if (b6 == 2) {
		unsigned char b7 = entry[7];
		float scale = *(float *)(base + 0x630);
		float result = SCVFAdd(chosen, SCVFMul(0.5f, scale));
		*(float *)(base + 0x628 + (unsigned int)b7 * 4) = result;
	}
}
