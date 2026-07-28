// SPDX-License-Identifier: GPL-2.0
/*
 * test_rtparm_ge_table.cpp  -  KAT for InitializegRTParmFunctionTable_GE()
 * (see src/engine/rtparm_ge_table.cpp / include/oa_rtparm_ge_table.h).
 *
 * Ground-truth expected values below were derived by TWO independent
 * decoders run against the raw `objdump -dr -M intel` disassembly of
 * .text+0x56b18d (34435 bytes) in ground-truth OA.ko:
 *   1. an offset-text-driven parser (reads the "ds:0xOFF" operand
 *      objdump prints for each mov and maps it to entry/field via
 *      offset arithmetic), and
 *   2. a purely positional parser (assigns each mov, in raw
 *      instruction ORDER, to the next slot in a fixed 12-field cycle,
 *      asserting -- never trusting -- that the computed offset matches
 *      what (1) read from the text).
 * Both agreed on all 313*12 = 3756 field writes with zero
 * discrepancies. The spot-checked entries below were additionally
 * hand-verified a third time directly against the raw disassembly
 * text (not just the decoder output). The checksum below covers all
 * 313 entries' portable (non-pointer) fields for full-table coverage
 * beyond the spot checks.
 */

#include <cstdio>
#include <cstdint>
#include "oa_rtparm_ge_table.h"

static int g_fail;

static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-55s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static void check_ptr(const char *label, const void *got, const void *want)
{
	if (got == want) {
		printf("  ok    %-55s %p\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=%p want=%p\n", label, got, want);
	g_fail++;
}

struct SpotCheck {
	unsigned int index;
	void *funcPtr;
	unsigned int field04;
	unsigned short field08;
	unsigned char field0a, field0b, field0c;
	unsigned int field10, field14, field18;
};

/* index, expected func, field04, field08(==index), field0a, field0b,
 * field0c, field10, field14, field18 -- field1c/20/24 always 0,
 * checked separately below for all 313 entries. */
static const SpotCheck kSpot[] = {
	{   0, (void *)&RT_ge_mode,             0,   0,  1,  0, 0,   61,    0,   0 },
	{   1, (void *)&RT_ge_gate_type,        0,   1,  1,  1, 0,   62,    0,   0 },
	{   2, (void *)&RT_ge_gate_cc,          1,   2,  1,  2, 0,   63,    0,   0 },
	{   3, (void *)&RT_ge_force_mono,      13,   3,  1,  3, 0,   65,    0,   0 },
	{   4, (void *)&RT_crb_nte_type,        0,   4,  2,  0, 0,   69,    0,   0 },
	{  17, (void *)&RT_crb_filter_fixed,   22,  17,  2, 13, 0,   80,    0,   0 },
	{ 150, (void *)&RT_wav_start_off_1,     9, 150, 10, 21, 0, 1385, 2213,   0 },
	{ 171, (void *)&RT_env_mode,           27, 171, 11,  0, 0,  432,  446, 460 },
	{ 200, (void *)&RT_env_all_time,        3, 200, 11, 29, 0,  434,  448, 462 },
	{ 242, (void *)&RT_bnd_dir,             0, 242, 13, 14, 0,   58,    0,   0 },
	{ 270, (void *)&RT_drm_pat_xpose_oct,   4, 270, 14, 23, 0,  171,  279, 387 },
	{ 300, (void *)&RT_dur_dix_mode,        0, 300, 15,  7, 0,   90,    0,   0 },
	{ 312, (void *)&RT_bnd_dix_width,       0, 312, 15, 19, 0,   46,    0,   0 },
};

/* Independent full-table checksum, matching the standalone Python oracle
 * exactly: acc = sum over i of (i+1) * (sum of all 8 portable numeric
 * fields of entry i), mod 2^64. */
static uint64_t checksum_all_entries()
{
	uint64_t acc = 0;
	for (unsigned int i = 0; i < RTPARM_GE_TABLE_SIZE; ++i) {
		const RTParmFunctionTableEntry_GE &e = gRTParmFunctionTable_GE[i];
		uint64_t s = (uint64_t)e.field04 + e.index + e.field0a + e.field0b +
			     e.field0c + e.field10 + e.field14 + e.field18 +
			     e.field1c + e.field20 + e.field24;
		acc += (uint64_t)(i + 1) * s;
	}
	return acc;
}

int main()
{
	InitializegRTParmFunctionTable_GE();

	printf("-- spot checks (%zu entries, hand-verified against raw disasm) --\n",
	       sizeof(kSpot) / sizeof(kSpot[0]));
	for (size_t k = 0; k < sizeof(kSpot) / sizeof(kSpot[0]); ++k) {
		const SpotCheck &s = kSpot[k];
		const RTParmFunctionTableEntry_GE &e = gRTParmFunctionTable_GE[s.index];
		char label[64];
		snprintf(label, sizeof(label), "[%u].funcPtr", s.index);
		check_ptr(label, e.funcPtr, s.funcPtr);
		snprintf(label, sizeof(label), "[%u].field04", s.index);
		check_eq(label, e.field04, s.field04);
		snprintf(label, sizeof(label), "[%u].index", s.index);
		check_eq(label, e.index, s.field08);
		snprintf(label, sizeof(label), "[%u].field0a", s.index);
		check_eq(label, e.field0a, s.field0a);
		snprintf(label, sizeof(label), "[%u].field0b", s.index);
		check_eq(label, e.field0b, s.field0b);
		snprintf(label, sizeof(label), "[%u].field0c", s.index);
		check_eq(label, e.field0c, s.field0c);
		snprintf(label, sizeof(label), "[%u].field10", s.index);
		check_eq(label, e.field10, s.field10);
		snprintf(label, sizeof(label), "[%u].field14", s.index);
		check_eq(label, e.field14, s.field14);
		snprintf(label, sizeof(label), "[%u].field18", s.index);
		check_eq(label, e.field18, s.field18);
	}

	printf("-- structural invariants, all 313 entries --\n");
	{
		int index_ok = 1, reserved_ok = 1;
		unsigned int nz18 = 0, ones0c = 0;
		for (unsigned int i = 0; i < RTPARM_GE_TABLE_SIZE; ++i) {
			const RTParmFunctionTableEntry_GE &e = gRTParmFunctionTable_GE[i];
			if (e.index != i) index_ok = 0;
			if (e.field1c != 0 || e.field20 != 0 || e.field24 != 0) reserved_ok = 0;
			if (e.field18 != 0) ++nz18;
			if (e.field0c == 1) ++ones0c;
			if (e.funcPtr == 0) {
				printf("  FAIL  [%u].funcPtr is NULL\n", i);
				g_fail++;
			}
		}
		check_eq("index == position for all 313 entries", index_ok, 1);
		check_eq("field1c/20/24 == 0 for all 313 entries", reserved_ok, 1);
		check_eq("count(field18 != 0)", nz18, 72);
		check_eq("count(field0c == 1)", ones0c, 43);
	}

	printf("-- full-table independent checksum (313 entries, all portable numeric fields) --\n");
	{
		uint64_t got = checksum_all_entries();
		uint64_t want = 454190880ULL; /* 0x1b126720, from the standalone Python oracle */
		if (got == want) {
			printf("  ok    checksum_all_entries()%*s%llu\n", 33, "", (unsigned long long)got);
		} else {
			printf("  FAIL  checksum_all_entries() got=%llu want=%llu\n",
			       (unsigned long long)got, (unsigned long long)want);
			g_fail++;
		}
	}

	/* All 313 funcPtr values must be pairwise distinct (confirmed by the
	 * decoder against ground truth: 313 unique RT_* symbols). */
	printf("-- distinctness check --\n");
	{
		int dup = 0;
		for (unsigned int i = 0; i < RTPARM_GE_TABLE_SIZE && !dup; ++i)
			for (unsigned int j = i + 1; j < RTPARM_GE_TABLE_SIZE; ++j)
				if (gRTParmFunctionTable_GE[i].funcPtr == gRTParmFunctionTable_GE[j].funcPtr) {
					printf("  FAIL  funcPtr[%u] == funcPtr[%u]\n", i, j);
					g_fail++;
					dup = 1;
					break;
				}
		if (!dup)
			printf("  ok    all 313 funcPtr values pairwise distinct\n");
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
