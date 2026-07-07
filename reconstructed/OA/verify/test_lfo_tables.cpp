// SPDX-License-Identifier: GPL-2.0
/*
 * test_lfo_tables.cpp  -  KAT for ../src/engine/lfo_tables.cpp,
 * CSTGLFOTables::CSTGLFOTables() (batch 28).
 *
 * Two levels of verification:
 *   1. A full byte-for-byte comparison of the constructed object
 *      against `lfo_tables_golden.h`'s 1548 dwords -- an INDEPENDENTLY
 *      generated reference (a from-scratch x87/x86 replay of the real
 *      disassembly, not copy-pasted from the production file's own
 *      literal tables). This is the primary check: it would catch a
 *      transcription slip anywhere in the ~400 embedded literal
 *      floats, not just at hand-picked spot points.
 *   2. A handful of named spot-checks at meaningful boundaries (first/
 *      last entries of each table, the 9 self-referential "dup" fields,
 *      the staircase segment transition points) so a failure report
 *      points at a specific, human-readable field rather than just
 *      "byte 1234 differs".
 */

#include <cstdio>
#include <new>
#include <cstring>
#include <cstdint>
#include "oa_engine_init.h"
#include "lfo_tables_golden.h"

CSTGLFOTables *CSTGLFOTables::sInstance;

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

static unsigned int u32_at(const unsigned char *base, unsigned int off)
{
	unsigned int v;
	memcpy(&v, base + off, 4);
	return v;
}

int main()
{
	/* Placement-new into a plain heap buffer -- this ctor never
	 * dereferences any packed/self-referential pointer field (every
	 * "dup" is a plain same-object memcpy-style read), so no
	 * mmap32()/MAP_32BIT backing is needed here (see the sec 10.156/
	 * 10.157 "does this function reconstitute and dereference a
	 * pointer" test -- it does not). */
	static unsigned char storage[0x1830];
	memset(storage, 0xcc, sizeof(storage));

	CSTGLFOTables *obj = new (storage) CSTGLFOTables();
	const unsigned char *base = (const unsigned char *)obj;

	/* --- 1. Full byte-for-byte comparison against the golden buffer --- */
	int first_mismatch = -1;
	int mismatches = 0;
	for (unsigned int i = 0; i < sizeof(kGoldenLFOTables) / sizeof(kGoldenLFOTables[0]); i++) {
		unsigned int got = u32_at(base, i * 4);
		unsigned int want = kGoldenLFOTables[i];
		if (got != want) {
			mismatches++;
			if (first_mismatch < 0)
				first_mismatch = (int)i;
		}
	}
	check_eq("full-object byte-for-byte match vs golden (mismatch count)", (unsigned int)mismatches, 0);
	if (mismatches) {
		printf("        first mismatch at dword %d (offset 0x%x): got=0x%08x want=0x%08x\n",
		       first_mismatch, first_mismatch * 4,
		       u32_at(base, first_mismatch * 4), kGoldenLFOTables[first_mismatch]);
	}

	/* --- 2. Named spot-checks --- */
	check_eq("rampUp[0] == 0.0", u32_at(base, 0x000), 0x00000000u);
	check_eq("rampUp[31] == 31/32", u32_at(base, 0x000 + 31 * 4), 0x3f780000u);
	check_eq("rampDown[0] == 1.0", u32_at(base, 0x080), 0x3f800000u);
	check_eq("rampDown[31] == 1/32", u32_at(base, 0x080 + 31 * 4), 0x3d000000u);
	check_eq("rampNeg[1] == -1/32", u32_at(base, 0x100 + 1 * 4), 0xbd000000u);
	check_eq("rampMinus1[0] == -1.0", u32_at(base, 0x180), 0xbf800000u);

	check_eq("+0x200 dup == rampUp[0]", u32_at(base, 0x200), u32_at(base, 0x000));

	check_eq("descRamp[0] == 1.0", u32_at(base, 0x204), 0x3f800000u);
	check_eq("descRamp[64] == 0.0", u32_at(base, 0x204 + 64 * 4), 0x00000000u);
	check_eq("descRamp[127] == 1-127/64", u32_at(base, 0x204 + 127 * 4), 0xbf7c0000u);

	check_eq("+0x404 literal == -1.0", u32_at(base, 0x404), 0xbf800000u);

	check_eq("const1.0[0] == 1.0", u32_at(base, 0x408), 0x3f800000u);
	check_eq("const1.0[63] == 1.0", u32_at(base, 0x408 + 63 * 4), 0x3f800000u);
	check_eq("const-1.0[0] == -1.0", u32_at(base, 0x508), 0xbf800000u);
	check_eq("const-1.0[63] (== +0x604) == -1.0", u32_at(base, 0x604), 0xbf800000u);
	check_eq("+0x608 dup == const-1.0[63]", u32_at(base, 0x608), u32_at(base, 0x604));

	check_eq("quarterSine[0] == 0.0", u32_at(base, 0x60c), 0x00000000u);
	check_eq("quarterSine[32] == 1.0", u32_at(base, 0x60c + 32 * 4), 0x3f800000u);
	check_eq("mirror[0] == quarterSine[31]", u32_at(base, 0x690), u32_at(base, 0x60c + 31 * 4));
	check_eq("negQuarter[0] == -0.0", u32_at(base, 0x70c), 0x80000000u);
	check_eq("negMirror[0] == -quarterSine[32] == -1.0", u32_at(base, 0x78c), 0xbf800000u);
	check_eq("negMirror[31] == -quarterSine[1]", u32_at(base, 0x78c + 31 * 4), 0xbd48fb30u);
	check_eq("+0x80c dup == quarterSine[0]", u32_at(base, 0x80c), u32_at(base, 0x60c));

	for (unsigned int off = 0x810; off <= 0x844; off += 4)
		check_eq("zero-fill region [0x810..0x844]", u32_at(base, off), 0u);

	check_eq("envelope[0] small positive", u32_at(base, 0x848), 0x39598440u);
	check_eq("envelope[109] small positive", u32_at(base, 0x848 + 109 * 4), 0x39787319u);

	for (unsigned int off = 0xa00; off <= 0xa10; off += 4)
		check_eq("zero-fill region [0xa00..0xa10]", u32_at(base, off), 0u);

	check_eq("evenFine[0] == -1.0", u32_at(base, 0xa14), 0xbf800000u);
	check_eq("evenFine[63] == fineTable[126]", u32_at(base, 0xa14 + 63 * 4), 0x3f77f7f0u);
	check_eq("oddFineRev[0] == fineTable[127] == 1.0", u32_at(base, 0xb14), 0x3f800000u);
	check_eq("+0xc14 dup == evenFine[0]", u32_at(base, 0xc14), u32_at(base, 0xa14));

	check_eq("fineRev[0] == fineTable[127] == 1.0", u32_at(base, 0xc18), 0x3f800000u);
	check_eq("fineRev[127] == fineTable[0] == -1.0", u32_at(base, 0xc18 + 127 * 4), 0xbf800000u);
	check_eq("+0xe18 dup == fineRev[127]", u32_at(base, 0xe18), u32_at(base, 0xc18 + 127 * 4));

	check_eq("fineFwd[0] == fineTable[0] == -1.0", u32_at(base, 0xe1c), 0xbf800000u);
	check_eq("fineFwd[127] == fineTable[127] == 1.0", u32_at(base, 0xe1c + 127 * 4), 0x3f800000u);
	check_eq("+0x101c dup == fineFwd[127]", u32_at(base, 0x101c), u32_at(base, 0xe1c + 127 * 4));

	/* 4x32-level staircase -1,0,+1,0 */
	check_eq("staircase1 seg0[0] == -1.0", u32_at(base, 0x1020), 0xbf800000u);
	check_eq("staircase1 seg1[0] == 0.0", u32_at(base, 0x1020 + 32 * 4), 0x00000000u);
	check_eq("staircase1 seg2[0] == 1.0", u32_at(base, 0x1020 + 64 * 4), 0x3f800000u);
	check_eq("staircase1 seg3[31] == 0.0", u32_at(base, 0x1020 + 127 * 4), 0x00000000u);
	check_eq("+0x1220 dup == staircase1 last", u32_at(base, 0x1220), u32_at(base, 0x1020 + 127 * 4));

	/* 6-level uneven staircase -1,-1/3,+1/3,+1,+1/3,-1/3 */
	check_eq("staircase2 seg0[0] == -1.0", u32_at(base, 0x1224), 0xbf800000u);
	check_eq("staircase2 seg1[0] (idx22) == -1/3 (a)", u32_at(base, 0x1224 + 22 * 4), 0xbeaaaaaau);
	check_eq("staircase2 seg2[0] (idx43) == +1/3 (a)", u32_at(base, 0x1224 + 43 * 4), 0x3eaaaaacu);
	check_eq("staircase2 seg3[0] (idx65) == +1.0", u32_at(base, 0x1224 + 65 * 4), 0x3f800000u);
	check_eq("staircase2 seg4[0] (idx86) == +1/3 (b)", u32_at(base, 0x1224 + 86 * 4), 0x3eaaaaaau);
	check_eq("staircase2 seg5[0] (idx108) == -1/3 (b)", u32_at(base, 0x1224 + 108 * 4), 0xbeaaaaacu);
	check_eq("staircase2 last (idx127) == -1/3 (b)", u32_at(base, 0x1224 + 127 * 4), 0xbeaaaaacu);
	check_eq("+0x1424 dup == staircase2 last", u32_at(base, 0x1424), u32_at(base, 0x1224 + 127 * 4));

	/* 4x32-level staircase +1,+1/3,-1/3,-1 */
	check_eq("staircase3 seg0[0] == 1.0", u32_at(base, 0x1428), 0x3f800000u);
	check_eq("staircase3 seg1[0] == +1/3 (b)", u32_at(base, 0x1428 + 32 * 4), 0x3eaaaaaau);
	check_eq("staircase3 seg2[0] == -1/3 (b)", u32_at(base, 0x1428 + 64 * 4), 0xbeaaaaacu);
	check_eq("staircase3 seg3[31] == -1.0", u32_at(base, 0x1428 + 127 * 4), 0xbf800000u);
	check_eq("+0x1628 dup == staircase3 last", u32_at(base, 0x1628), u32_at(base, 0x1428 + 127 * 4));

	/* 6-level uneven staircase +1,+0.6,+0.2,-0.2,-0.6,-1 */
	check_eq("staircase4 seg0[0] == 1.0", u32_at(base, 0x162c), 0x3f800000u);
	check_eq("staircase4 seg1[0] (idx22) == 0.6", u32_at(base, 0x162c + 22 * 4), 0x3f19999au);
	check_eq("staircase4 seg2[0] (idx43) == 0.2", u32_at(base, 0x162c + 43 * 4), 0x3e4cccccu);
	check_eq("staircase4 seg3[0] (idx65) == -0.2", u32_at(base, 0x162c + 65 * 4), 0xbe4cccceu);
	check_eq("staircase4 seg4[0] (idx86) == -0.6", u32_at(base, 0x162c + 86 * 4), 0xbf19999au);
	check_eq("staircase4 last (idx127) == -1.0", u32_at(base, 0x162c + 127 * 4), 0xbf800000u);
	check_eq("+0x182c dup == staircase4 last", u32_at(base, 0x182c), u32_at(base, 0x162c + 127 * 4));

	printf("\n%s (%d checks failed)\n", g_fail ? "FAILED:" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
