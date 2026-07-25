// SPDX-License-Identifier: GPL-2.0
/*
 * cuuid_convert.cpp  -  CUUID::ConvertFromText real body (see include/oa_types.h).
 *
 * Ground-truthed via full disassembly: .text+0x46570, 1425 bytes (nm -S 0x591).
 * Full disassembly walked end to end (`_ZN5CUUID15ConvertFromTextEPKc`).
 *
 * Confirmed real algorithm:
 *
 *   1. `strlen(text) > 0x23` (i.e. length >= 36), else fail. (Real kernel
 *      `strlen`, R_386_PC32 relocation confirmed.) Extra trailing bytes
 *      (e.g. the ":n1:n2:n3" tail LM/LD/CM/CD append after the UUID field
 *      in process_oacmd.cpp) are simply never read.
 *   2. Standard 8-4-4-4-12 dashed-hex UUID layout: hex digits at text
 *      offsets 0-7, '-' at 8, hex at 9-12, '-' at 13, hex at 14-17, '-' at
 *      18, hex at 19-22, '-' at 23, hex at 24-35 -- 32 hex digits total,
 *      16 bytes, confirmed via four separate `cmp BYTE PTR [.],0x2d`
 *      dash checks at exactly those four offsets (relative to `text`).
 *   3. Each hex digit is validated via `_ctype[c] & 0x44` (real kernel
 *      `_ctype` table, R_386_32 relocation confirmed at every one of the
 *      32 digit checks) -- bit 0x04 is `_D` (decimal digit), bit 0x40 is
 *      `_X` (hex letter A-F/a-f); the OR of the two is exactly libc
 *      `isxdigit()`. Any digit failing this check aborts with `false`.
 *   4. Each validated 2-character pair is copied into a small on-stack
 *      buffer and passed to the real kernel `simple_strtoul(pair, 0, 16)`
 *      (R_386_PC32 relocation confirmed at all 16 call sites) to produce
 *      one output byte.
 *   5. All 16 output bytes are accumulated in a flat on-stack buffer,
 *      strictly in the order they were parsed (no field-reordering), then
 *      copied into `this` as four back-to-back dword stores at the very
 *      end -- confirmed equivalent to a plain 16-byte sequential copy
 *      (source and destination are both byte-contiguous in the same
 *      order; the dword-at-a-time store is just the compiler's own
 *      codegen, not a meaningful reordering).
 *   6. Returns true only if every one of the above checks passed for the
 *      entire 36-character prefix; any single failure (length, a bad hex
 *      digit, or a misplaced dash) returns false immediately without
 *      touching `bytes` at all (confirmed: the return-value byte starts
 *      at 0 and every failure branch jumps straight to the epilogue
 *      without setting it; it's only set to 1 immediately before the
 *      final 4 dword copies).
 *
 * `_ctype`/`simple_strtoul` are both confirmed genuine, undefined (`U`)
 * Linux kernel exports in ground truth OA.ko itself (`nm -u`) -- declared
 * here as real externs, same treatment as this project's other confirmed
 * real-kernel-API call sites (filp_open/vmalloc/mutex_lock/etc). Host KATs
 * supply their own safe mocks (verify/test_cuuid_convert.cpp), same
 * convention as every other genuine-kernel-extern reconstruction here.
 */

#include "oa_types.h"

extern "C" unsigned char _ctype[];
extern "C" unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base);

/* Linux kernel lib/ctype.c bit flags -- only the two this function checks. */
#define OA_CT_DIGIT     0x04u	/* _D */
#define OA_CT_HEXLETTER 0x40u	/* _X */

static inline bool is_hex_digit(char c)
{
	return (_ctype[(unsigned char)c] & (OA_CT_DIGIT | OA_CT_HEXLETTER)) != 0;
}

bool CUUID::ConvertFromText(const char *text)
{
	unsigned int len = 0;
	while (text[len])
		len++;
	if (len <= 0x23)
		return false;

	/* Confirmed dash positions for the 8-4-4-4-12 layout. */
	static const unsigned char kDashOffset[4] = { 8, 13, 18, 23 };

	unsigned char out[16];
	unsigned int outIdx = 0;
	unsigned int pos = 0;
	unsigned int dashIdx = 0;

	while (outIdx < 16) {
		if (dashIdx < 4 && pos == kDashOffset[dashIdx]) {
			if (text[pos] != '-')
				return false;
			pos++;
			dashIdx++;
			continue;
		}

		char hi = text[pos];
		char lo = text[pos + 1];
		if (!is_hex_digit(hi) || !is_hex_digit(lo))
			return false;

		char pair[3] = { hi, lo, 0 };
		out[outIdx++] = (unsigned char)simple_strtoul(pair, 0, 16);
		pos += 2;
	}

	for (int i = 0; i < 16; i++)
		bytes[i] = out[i];
	return true;
}
