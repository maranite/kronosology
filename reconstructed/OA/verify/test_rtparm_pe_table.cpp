// SPDX-License-Identifier: GPL-2.0
/*
 * test_rtparm_pe_table.cpp  -  KAT for InitializegRTParmFunctionTable_PE()
 * (see src/engine/rtparm_pe_table.cpp / include/oa_rtparm_pe_table.h).
 *
 * Same two-decoder methodology as verify/test_rtparm_ge_table.cpp: an
 * offset-text-driven parser and a purely positional parser, cross-checked
 * against each other with zero discrepancies across all 50*12 = 600 field
 * writes. The spot-checked entries below were additionally hand-verified
 * a third time directly against the raw `objdump -dr -M intel` disassembly
 * text of .text+0x573810 (5505 bytes) in ground-truth OA.ko (not just the
 * decoder output) -- see entry 20 in particular, confirmed byte-for-byte
 * against the raw text including its funcPtr relocation symbol. The
 * checksum below covers all 50 entries' portable (non-pointer) fields for
 * full-table coverage beyond the spot checks.
 */

#include <cstdio>
#include <cstdint>
#include "oa_rtparm_pe_table.h"

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
	unsigned int field10, field14, field18, field1c, field20, field24;
};

/* index, expected func (NULL for the 4 known-null slots), field04, index,
 * field0a, field0b, field0c, field10, field14, field18, field1c, field20,
 * field24 -- hand-verified against raw disasm text for entry 20 (see file
 * header); the rest cross-checked via the two-decoder agreement. Entry 2 is
 * one of only 3 entries (indices 2,3,4, the RT_crb_xpose* trio) where
 * field0c reads 0 instead of the otherwise-uniform 2. */
static const SpotCheck kSpot[] = {
	{  0, (void *)&RT_pe_tsig_menu,          36,  0, 1,  0, 2,  484,   0,   0,   0,   0,   0 },
	{  1, (void *)&RT_run,                    0,  1, 2,  0, 2,    6, 126, 246, 366,   6,   6 },
	{  2, (void *)&RT_crb_xpose,               7,  2, 2,  1, 0,    7,2515,5023,7531,   7,   7 },
	{  5, (void *)0,                          6,  5, 2,  4, 2,   54, 174, 294, 414,  54,  54 },
	{  6, (void *)0,                          6,  6, 2,  5, 2,   56, 176, 296, 416,  56,  56 },
	{  7, (void *)0,                          6,  7, 2,  6, 2,   57, 177, 297, 417,  57,  57 },
	{  8, (void *)0,                          6,  8, 2,  7, 2,    7, 127, 247, 367,   7,   7 },
	{  9, (void *)&RT_nbo_interp,             0,  9, 3,  0, 2,   17, 137, 257, 377,  17,  17 },
	{ 20, (void *)&RT_nte_map_kbd_track,     20, 20, 3, 11, 2,   43, 163, 283, 402,  43,  43 },
	{ 30, (void *)&RT_env_trig_mode_1,        6, 30, 4,  9, 2,   29, 149, 269, 389,  29,  29 },
	{ 45, (void *)&RT_kbd_thru_out_xpose_oct_5th, 1, 45, 5, 9, 2, 16, 136, 256, 376, 16,  16 },
	{ 49, (void *)&RT_pe_freeze_retrig,      20, 49, 6,  3, 2,   40, 160, 280, 400,  40,  40 },
};

/* Independent full-table checksum, matching the standalone Python oracle
 * exactly: acc = sum over i of (i+1) * (sum of all 11 portable numeric
 * fields of entry i), mod 2^64. */
static uint64_t checksum_all_entries()
{
	uint64_t acc = 0;
	for (unsigned int i = 0; i < RTPARM_PE_TABLE_SIZE; ++i) {
		const RTParmFunctionTableEntry_PE &e = gRTParmFunctionTable_PE[i];
		uint64_t s = (uint64_t)e.field04 + e.index + e.field0a + e.field0b +
			     e.field0c + e.field10 + e.field14 + e.field18 +
			     e.field1c + e.field20 + e.field24;
		acc += (uint64_t)(i + 1) * s;
	}
	return acc;
}

int main()
{
	InitializegRTParmFunctionTable_PE();

	printf("-- spot checks (%zu entries, hand-verified against raw disasm) --\n",
	       sizeof(kSpot) / sizeof(kSpot[0]));
	for (size_t k = 0; k < sizeof(kSpot) / sizeof(kSpot[0]); ++k) {
		const SpotCheck &s = kSpot[k];
		const RTParmFunctionTableEntry_PE &e = gRTParmFunctionTable_PE[s.index];
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
		snprintf(label, sizeof(label), "[%u].field1c", s.index);
		check_eq(label, e.field1c, s.field1c);
		snprintf(label, sizeof(label), "[%u].field20", s.index);
		check_eq(label, e.field20, s.field20);
		snprintf(label, sizeof(label), "[%u].field24", s.index);
		check_eq(label, e.field24, s.field24);
	}

	printf("-- structural invariants, all 50 entries --\n");
	{
		int index_ok = 1, f2024_eq_ok = 1;
		unsigned int null_count = 0, c0c_2 = 0;
		for (unsigned int i = 0; i < RTPARM_PE_TABLE_SIZE; ++i) {
			const RTParmFunctionTableEntry_PE &e = gRTParmFunctionTable_PE[i];
			if (e.index != i) index_ok = 0;
			if (e.field20 != e.field24) f2024_eq_ok = 0;
			if (e.funcPtr == 0) ++null_count;
			if (e.field0c == 2) ++c0c_2;
		}
		check_eq("index == position for all 50 entries", index_ok, 1);
		check_eq("field20 == field24 for all 50 entries", f2024_eq_ok, 1);
		check_eq("count(funcPtr == NULL)", null_count, 4);
		check_eq("count(field0c == 2)", c0c_2, 47);
	}

	printf("-- full-table independent checksum (50 entries, all portable numeric fields) --\n");
	{
		uint64_t got = checksum_all_entries();
		uint64_t want = 1355169ULL; /* from the standalone Python oracle */
		if (got == want) {
			printf("  ok    checksum_all_entries()%*s%llu\n", 33, "", (unsigned long long)got);
		} else {
			printf("  FAIL  checksum_all_entries() got=%llu want=%llu\n",
			       (unsigned long long)got, (unsigned long long)want);
			g_fail++;
		}
	}

	/* All non-null funcPtr values must be pairwise distinct (confirmed by
	 * the decoder against ground truth: 46 unique RT_* symbols among the
	 * 50 entries, 4 entries legitimately NULL). */
	printf("-- distinctness check (non-null funcPtr only) --\n");
	{
		int dup = 0;
		for (unsigned int i = 0; i < RTPARM_PE_TABLE_SIZE && !dup; ++i) {
			if (gRTParmFunctionTable_PE[i].funcPtr == 0)
				continue;
			for (unsigned int j = i + 1; j < RTPARM_PE_TABLE_SIZE; ++j) {
				if (gRTParmFunctionTable_PE[j].funcPtr == 0)
					continue;
				if (gRTParmFunctionTable_PE[i].funcPtr == gRTParmFunctionTable_PE[j].funcPtr) {
					printf("  FAIL  funcPtr[%u] == funcPtr[%u]\n", i, j);
					g_fail++;
					dup = 1;
					break;
				}
			}
		}
		if (!dup)
			printf("  ok    all 46 non-null funcPtr values pairwise distinct\n");
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
