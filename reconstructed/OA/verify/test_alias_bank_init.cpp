// SPDX-License-Identifier: GPL-2.0
/*
 * test_alias_bank_init.cpp  -  host-side known-answer test for
 * USTGAliasBankTypes::InitializeAliasBanks() (sec 10.85).
 */

#include <cstdio>
#include "oa_global.h"

static int g_fail;
static void check_eq(const char *label, int got, int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s %d\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %d)\n", want);
}

int main(void)
{
	printf("USTGAliasBankTypes::InitializeAliasBanks() known-answer test\n");
	printf("=========================================================\n");

	USTGAliasBankTypes::InitializeAliasBanks();

	printf("[1] 1:1 blocks (0-6) map directly to real banks 0-6\n");
	for (int block = 0; block <= 6; block++) {
		char label[64];
		snprintf(label, sizeof(label), "block %d -> real bank %d", block, block);
		check_eq(label, STGAliasToRealPgmBank[block * 128], block);
	}

	printf("[2] blocks 7-15 (9 distinct alias banks) all alias the SAME real bank 7\n");
	for (int block = 7; block <= 15; block++) {
		char label[64];
		snprintf(label, sizeof(label), "block %d -> real bank 7", block);
		check_eq(label, STGAliasToRealPgmBank[block * 128], 7);
	}

	printf("[3] blocks 16-29 continue 1:1 into real banks 8-21\n");
	for (int block = 16; block <= 29; block++) {
		char label[64];
		int expected = block - 8;
		snprintf(label, sizeof(label), "block %d -> real bank %d", block, expected);
		check_eq(label, STGAliasToRealPgmBank[block * 128], expected);
	}

	printf("[4] STGAliasBankPgmMap is always an identity mapping within each block\n");
	check_eq("block 0, entry 0", STGAliasBankPgmMap[0 * 128 + 0], 0);
	check_eq("block 0, entry 127", STGAliasBankPgmMap[0 * 128 + 127], 127);
	check_eq("block 15, entry 64", STGAliasBankPgmMap[15 * 128 + 64], 64);
	check_eq("block 29, entry 127", STGAliasBankPgmMap[29 * 128 + 127], 127);

	printf("[5] every entry within a block shares the same real bank ID\n");
	{
		bool allSame = true;
		for (int i = 0; i < 128; i++)
			if (STGAliasToRealPgmBank[10 * 128 + i] != 7)
				allSame = false;
		check_eq("block 10, all 128 entries == real bank 7", allSame ? 1 : 0, 1);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
