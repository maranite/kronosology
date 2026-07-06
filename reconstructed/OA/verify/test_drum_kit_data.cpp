// SPDX-License-Identifier: GPL-2.0
/*
 * test_drum_kit_data.cpp  -  host-side known-answer test for
 * CSTGDrumKitData::CSTGDrumKitData() (batch 23, see src/engine/drum_kit_data.cpp).
 *
 * The expected values below were independently cross-checked against a
 * standalone Python replay model of the same disassembly (not just
 * re-derived by hand a second time) before this file was written --
 * see the "hand-trace 2-3 cases independently" discipline this project's
 * own agent-memory workflow file requires for intricate nested-loop
 * ctors like this one.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>
#include "oa_global.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

extern "C" unsigned char _ZTV15CSTGDrumKitData[96];

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		printf("  ok    %-70s 0x%x\n", label, got);
		return;
	}
	printf("  FAIL  %-70s got=0x%x want=0x%x\n", label, got, want);
	g_fail++;
}

static const unsigned char kLegacyBankPrefix[15] = {
	'K', 'O', 'R', 'G', 0, 0, 0, 0, 0, 0, 0, 0, 'M', 'S', 0,
};

/* record_start relative to the array base (this+4). */
static unsigned char *rec_at(unsigned char *arr, unsigned int outerIdx, unsigned int innerIdx)
{
	return arr + (unsigned long)outerIdx * 0x10302 + (unsigned long)innerIdx * 0x202;
}

static void check_record_common_fields(unsigned char *rec, unsigned int innerIdx, const char *tag)
{
	char label[128];

	snprintf(label, sizeof(label), "%s: +2/+3 zeroed (loop 1)", tag);
	check_eq(label, (unsigned int)(rec[2] == 0 && rec[3] == 0), 1);

	snprintf(label, sizeof(label), "%s: +4 == 0", tag);
	check_eq(label, rec[4], 0);

	snprintf(label, sizeof(label), "%s: +8 == 1 (populated flag)", tag);
	check_eq(label, rec[8], 1);

	snprintf(label, sizeof(label), "%s: +9 == innerIdx", tag);
	check_eq(label, rec[9], (unsigned char)innerIdx);

	snprintf(label, sizeof(label), "%s: +10..21 zeroed (3 dwords)", tag);
	check_eq(label, (unsigned int)(
		*(unsigned int *)(rec + 10) == 0 &&
		*(unsigned int *)(rec + 14) == 0 &&
		*(unsigned int *)(rec + 18) == 0), 1);

	for (int k = 0; k < 8; k++) {
		unsigned char *sub = rec + 0x13e + k * 0x19;
		snprintf(label, sizeof(label), "%s: sub[%d] 15-byte UUID prefix matches kLegacyBankPrefix", tag, k);
		check_eq(label, (unsigned int)(memcmp(sub, kLegacyBankPrefix, 15) == 0), 1);

		snprintf(label, sizeof(label), "%s: sub[%d] +0x14 word == 0", tag, k);
		check_eq(label, *(unsigned short *)(sub + 0x14), 0);
	}
}

int main(void)
{
	printf("CSTGDrumKitData::CSTGDrumKitData() KAT\n");
	printf("=======================================\n");

	size_t total = sizeof(CSTGDrumKitData);
	printf("sizeof(CSTGDrumKitData) = 0x%zx\n", total);

	unsigned char *buf = new unsigned char[total];
	memset(buf, 0xcc, total);

	/* [pre] Poke the two bytes that will receive an AND-mask overflow
	 * (record 0's own k=7 sub-record overflowing into record 1's own
	 * +1, arr-relative 0x203; and the LAST record's own k=7 overflowing
	 * 2 bytes past the nominal array end, arr-relative 0x1143523) to a
	 * value whose low 2 bits are SET (0xff, not the blanket 0xcc
	 * poison) -- 0xcc's own low 2 bits are already 0, so `&0xfc` would
	 * be a false-negative-proof check against the blanket poison (same
	 * class of gotcha as sec 10.153's poison-pattern discipline). */
	unsigned char *arr_precheck = buf + 4;
	arr_precheck[0x203] = 0xff;
	arr_precheck[0x1143523] = 0xff;

	printf("[1] construct\n");
	CSTGDrumKitData *dk = new (buf) CSTGDrumKitData();
	(void)dk;

	check_eq("[1] vtable ptr install", *(unsigned int *)buf, ToU32(_ZTV15CSTGDrumKitData + 8));

	unsigned char *arr = buf + 4;

	printf("[2] first record (outerIdx=0, innerIdx=0)\n");
	unsigned char *rec00 = rec_at(arr, 0, 0);
	check_record_common_fields(rec00, 0, "rec(0,0)");
	/* This record's own +0/+1 (nothing precedes it -- no overflow source). */
	check_eq("rec(0,0): +0/+1 never written by anyone, still poisoned",
		 (unsigned int)(rec00[0] == 0xcc && rec00[1] == 0xcc), 1);

	printf("[3] second record (outerIdx=0, innerIdx=1) -- receives rec(0,0)'s\n"
	       "    own k=7 sub-record overflow at its own +0/+1\n");
	unsigned char *rec01 = rec_at(arr, 0, 1);
	check_record_common_fields(rec01, 1, "rec(0,1)");
	check_eq("rec(0,1): +0 == 0 (word half of rec(0,0)'s k=7 overflow, arr+0x202)", rec01[0], 0);
	/* rec01+1 == arr+0x203, pre-poked to 0xff above specifically so this
	 * AND-mask (&0xfc) is provably exercised rather than passing by
	 * coincidence against the default 0xcc poison (whose low 2 bits are
	 * already clear). */
	check_eq("rec(0,1): +1 pre-poked to 0xff, mask really ran -> 0xfc (arr+0x203)",
		 rec01[1], 0xfc);

	printf("[4] mid record (outerIdx=136, innerIdx=64)\n");
	unsigned char *recMid = rec_at(arr, 136, 64);
	check_record_common_fields(recMid, 64, "rec(136,64)");

	printf("[5] last record (outerIdx=272, innerIdx=128)\n");
	unsigned char *recLast = rec_at(arr, 272, 128);
	check_record_common_fields(recLast, 128, "rec(272,128)");
	check_eq("rec(272,128): array end == this+4+0x1143522", (unsigned int)(recLast + 0x202 == arr + 0x1143522), 1);
	/* This record's own k=7 sub-record's trailing word/mask
	 * (rec-relative +0x201/+0x202 word, +0x203 mask) straddle the
	 * nominal array end (`arr+0x1143522`, exclusive): +0x201 (=
	 * arr+0x1143521) is the LAST valid in-bounds byte; +0x202/+0x203
	 * (= arr+0x1143522/+0x1143523) are the first two bytes PAST the
	 * nominal end -- into the declared headroom (class size 0x1143530,
	 * comfortably larger than the confirmed minimum 0x1143529). Confirms
	 * the write lands exactly where the Python replay model predicted. */
	check_eq("last record's own word, low byte (arr+0x1143521, last IN-BOUNDS array byte) == 0",
		 arr[0x1143521], 0);
	check_eq("last record's own word, high byte (arr+0x1143522, 1st byte PAST nominal end) == 0",
		 arr[0x1143522], 0);
	check_eq("last record's own mask byte (arr+0x1143523, 2nd byte PAST nominal end): "
		 "pre-poked to 0xff, mask really ran -> 0xfc",
		 arr[0x1143523], 0xfc);
	check_eq("arr+0x1143524 (3rd byte past nominal end): untouched, still poisoned",
		 arr[0x1143524], 0xcc);

	printf("[6] size sanity: declared struct size fits the real minimum (0x1143529)\n");
	check_eq("sizeof(CSTGDrumKitData) >= 0x1143529", (unsigned int)(total >= 0x1143529UL), 1);

	delete[] buf;

	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
