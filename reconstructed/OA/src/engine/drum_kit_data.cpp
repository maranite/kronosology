// SPDX-License-Identifier: GPL-2.0
/*
 * drum_kit_data.cpp  -  CSTGDrumKitData::CSTGDrumKitData() (batch 23,
 * `.text+0xa0940`, 925 bytes).
 *
 * Kept in its own dedicated translation unit (not managers.cpp, not
 * bar2_stubs.cpp) since only `verify/test_global_ctor.cpp` mocks this
 * exact symbol (its own `{ g_drumKitCalls++; }` counter mock), and that
 * file does NOT link either of those existing files -- matching the
 * `WriteSTGMidiOutQueue`/`CSTGStreamingEventManager` "give it its own
 * file" precedent (sec 10.145/10.158) so `test_global_ctor.cpp` is left
 * completely untouched.
 *
 * Confirmed via full disassembly + relocation resolution (zero `call`
 * instructions anywhere in the function -- same "big but branch/call-free"
 * safety category as `CSTGSamplingInterface::CSTGSamplingInterface()`,
 * sec 10.160, and `CSTGCCInfo::sCCInfoTable`'s populating function, sec
 * 10.161): a vtable-pointer install, followed by two sequential top-level
 * loops over the SAME flattened 273 (outer) x 129 (inner) space, each
 * "row" a 0x202 (514)-byte "note record" (`this+4 + outerIdx*0x10302 +
 * innerIdx*0x202`, confirmed: `0x10302 == 129*0x202`,
 * `273*0x10302 == 0x1143522` exactly).
 *
 *   Loop 1 (`.text+0xa0959`): zeroes just the 2 bytes at each record's own
 *   `+2`/`+3` -- a pre-pass separate from loop 2's own fuller population,
 *   confirmed via the loop bounds alone (129 inner x 273 outer, same
 *   record stride).
 *
 *   Loop 2 (`.text+0xa09a3`): for every record, sets:
 *     +0x04 = 0                (byte)
 *     +0x05 &= 0xfd             (AND-mask on otherwise-untouched memory --
 *                                 real, faithfully preserved even though
 *                                 it's a no-op on zero-initialized memory)
 *     +0x08 = 1                 (byte -- "populated"/type flag)
 *     +0x09 = (unsigned char)innerIdx   (byte -- the record's own MIDI-note
 *                                         index within its 273-entry bank)
 *     +0x0a..0x0d = 0            (dword)
 *     +0x0e..0x11 = 0            (dword)
 *     +0x12..0x15 = 0            (dword)
 *   then 8 near-identical "velocity zone" sub-records at
 *   `record+0x13e + k*0x19` (k=0..7, confirmed stride 0x19=25 bytes via
 *   the exact relocation-resolved store offsets, not guessed), each:
 *     +0x00..0x0e (15 bytes) = `CSTGMultisampleBankUUIDBase::
 *                  sLegacyBankPrefix` (confirmed real 15-byte `.rodata`
 *                  constant at `.rodata+0x476a8`: "KORG" + 8 zero bytes +
 *                  "MS" + one zero byte -- independently cross-checked
 *                  against `src/auth/klm_manager.cpp`'s own already-
 *                  reconstructed `build_legacy_builtin_uuid()` template,
 *                  whose first 15 bytes are byte-for-byte identical)
 *     +0x0f (1 byte) = NEVER WRITTEN by this ctor -- a real, confirmed
 *                  gap (the 16th "UUID" byte, presumably a per-note/
 *                  per-velocity index filled in by some OTHER,
 *                  unreconstructed method; left as whatever the object's
 *                  own backing memory already held, faithfully NOT
 *                  zeroed here since the real instruction stream never
 *                  touches it)
 *     +0x10..0x13 (4 bytes) = unwritten gap
 *     +0x14..0x15 (word) = 0
 *     +0x16 (1 byte) &= 0xfc   (AND-mask, same "operates on otherwise-
 *                  untouched memory" real quirk as the record-level one
 *                  above)
 *     +0x17..0x18 (2 bytes) = unwritten gap
 *
 * CRITICAL, independently confirmed via a byte-accurate Python replay
 * model (not just hand arithmetic) before writing this file: the 8
 * sub-records' own combined span (8 x 0x19 = 0xc8 bytes, starting at
 * record-relative +0x13e) is 0xc8+0x13e = 0x206 bytes -- 4 bytes WIDER
 * than the record's own 0x202-byte stride. The LAST sub-record's own
 * trailing word/mask/gap (record-relative +0x201..+0x205) therefore
 * spill over into the byte range that is, for every record except the
 * very last one overall, simply the NEXT record's own bytes +0/+1 (never
 * written by anything else, since loop 1 only pre-zeros +2/+3 and loop 2's
 * own per-record fields start at +4) -- a real, confirmed, faithfully-
 * preserved cross-record write, not a reconstruction bug. For the single
 * LAST record (outerIdx==272, innerIdx==128), this same overflow writes 3
 * bytes PAST the nominal array end (`this+4+0x1143522`) -- see
 * oa_global.h's own `CSTGDrumKitData` class comment for the resulting
 * confirmed-minimum real object size (0x1143529, declared here as
 * 0x1143530).
 */

#include "oa_global.h"
#include "oa_internal.h"	/* placement operator new(size_t, void*) */

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/* vtable placeholder -- install only, this ctor never dispatches through
 * it (sec 10.153's "install vs. dispatch" rule), confirmed real size 96
 * bytes (`nm -S -C`: `vtable for CSTGDrumKitData`, 0x60). */
extern "C" unsigned char _ZTV15CSTGDrumKitData[96];
unsigned char _ZTV15CSTGDrumKitData[96];

/*
 * `CSTGMultisampleBankUUIDBase::sLegacyBankPrefix` (`.rodata+0x476a8`, 15
 * bytes, confirmed via `readelf`/`objdump -s`): "KORG" + 8 zero bytes +
 * "MS" + one zero byte. Matches `src/auth/klm_manager.cpp`'s own already-
 * reconstructed `build_legacy_builtin_uuid()` template's first 15 bytes
 * byte-for-byte (that function appends its own extra 16th "index" byte
 * for a different, unrelated purpose -- this table itself is only 15
 * bytes).
 */
static const unsigned char kLegacyBankPrefix[15] = {
	'K', 'O', 'R', 'G', 0, 0, 0, 0, 0, 0, 0, 0, 'M', 'S', 0,
};

CSTGDrumKitData::CSTGDrumKitData()
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned int *)base = ToU32(_ZTV15CSTGDrumKitData + 8);

	unsigned char *arr = base + 4;

	/* Loop 1 (`.text+0xa0959`): pre-zero bytes +2/+3 of every record. */
	for (unsigned int outerOff = 0; outerOff != 0x1143522; outerOff += 0x10302) {
		unsigned char *rec0 = arr + outerOff;
		for (unsigned int innerOff = 0; innerOff != 0x10302; innerOff += 0x202) {
			rec0[innerOff + 2] = 0;
			rec0[innerOff + 3] = 0;
		}
	}

	/* Loop 2 (`.text+0xa09a3`): populate each record. */
	for (unsigned int outerIdx = 0; outerIdx != 0x111; outerIdx++) {
		unsigned int outerOff = outerIdx * 0x10302;
		for (unsigned int innerIdx = 0; innerIdx != 0x81; innerIdx++) {
			unsigned char *rec = arr + outerOff + innerIdx * 0x202;

			rec[8] = 1;
			*(unsigned int *)(rec + 10) = 0;
			rec[9] = (unsigned char)innerIdx;
			*(unsigned int *)(rec + 14) = 0;
			*(unsigned int *)(rec + 18) = 0;
			rec[4] = 0;
			rec[5] &= 0xfd;

			for (int k = 0; k < 8; k++) {
				unsigned char *sub = rec + 0x13e + k * 0x19;

				for (int i = 0; i < 15; i++)
					sub[i] = kLegacyBankPrefix[i];
				/* sub[0xf] deliberately left unwritten -- see
				 * header comment above. */
				*(unsigned short *)(sub + 0x14) = 0;
				sub[0x16] &= 0xfc;
			}
		}
	}
}
