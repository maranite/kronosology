// SPDX-License-Identifier: GPL-2.0
/*
 * test_bank_memory.cpp  -  host-side known-answer tests for CSTGBankMemory
 * (Stage 2, see include/oa_bank_memory.h).
 *
 * Unlike the crypto primitives (Blowfish, Crockford Base-32, MD5), this is
 * Korg-internal bookkeeping with no third-party/published reference to
 * anchor against. Vectors here are hand-traced step by step from the
 * confirmed disassembly algorithm itself (not just re-run through the same
 * implementation, which would only prove self-consistency) -- each expected
 * value is derived arithmetically in the comments and can be checked by a
 * reader independently of this code.
 *
 * All pointer values used are synthetic (never dereferenced) -- AllocAligned
 * only does pointer arithmetic, never touches the memory it "allocates", so
 * this is safe to test without a real backing buffer.
 */

#include <cstdio>
#include <cstring>
#include "oa_bank_memory.h"

static int g_fail;

static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) {
		printf("  ok    %-45s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-45s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

int main(void)
{
	printf("CSTGBankMemory known-answer test\n");
	printf("=================================\n");

	/*
	 * Initialize(base=0x2003, size=1000):
	 *   aligned base = (0x2003 + 0xf) & ~0xf = 0x2012 & ~0xf = 0x2010
	 *   aligned size = 1000 & ~0xf = 0x3e8 & ~0xf = 0x3e0 = 992
	 *   offset reset to 0
	 */
	printf("[1] Initialize rounds base up and size down to 16 bytes\n");
	CSTGBankMemory::Initialize((unsigned char *)0x2003, 1000);
	check_eq("sOurMemoryBase", (unsigned long)CSTGBankMemory::sOurMemoryBase, 0x2010);
	check_eq("sTotalMemoryAvailable", CSTGBankMemory::sTotalMemoryAvailable, 992);
	check_eq("sCurrentAllocationOffset", CSTGBankMemory::sCurrentAllocationOffset, 0);

	/*
	 * AllocAligned(size=100, alignment=16):
	 *   pos = base(0x2010) + offset(0) = 0x2010, already 16-aligned
	 *   ptr = 0x2010
	 *   new offset = (ptr - base) + size = 0 + 100 = 100
	 */
	printf("[2] AllocAligned #1 (size=100, align=16) from a base already aligned\n");
	unsigned char *p1 = CSTGBankMemory::AllocAligned(100, 16);
	check_eq("returned pointer", (unsigned long)p1, 0x2010);
	check_eq("new sCurrentAllocationOffset", CSTGBankMemory::sCurrentAllocationOffset, 100);

	/*
	 * AllocAligned(size=50, alignment=16):
	 *   pos = 0x2010 + 100 = 0x2074 (0x74 mod 16 = 4, needs +12 to align)
	 *   ptr = 0x2080
	 *   new offset = (0x2080 - 0x2010) + 50 = 0x70(112) + 50 = 162
	 */
	printf("[3] AllocAligned #2 (size=50, align=16) exercises alignment padding\n");
	unsigned char *p2 = CSTGBankMemory::AllocAligned(50, 16);
	check_eq("returned pointer", (unsigned long)p2, 0x2080);
	check_eq("new sCurrentAllocationOffset", CSTGBankMemory::sCurrentAllocationOffset, 162);

	/*
	 * AllocAligned(size=8, alignment=4):
	 *   pos = 0x2010 + 162 = 0x2010 + 0xa2 = 0x20b2 (mod 4 = 2, needs +2)
	 *   ptr = 0x20b4
	 *   new offset = (0x20b4 - 0x2010) + 8 = 0xa4(164) + 8 = 172
	 */
	printf("[4] AllocAligned #3 (size=8, align=4) exercises a different alignment\n");
	unsigned char *p3 = CSTGBankMemory::AllocAligned(8, 4);
	check_eq("returned pointer", (unsigned long)p3, 0x20b4);
	check_eq("new sCurrentAllocationOffset", CSTGBankMemory::sCurrentAllocationOffset, 172);

	/*
	 * SetTotalBytesToManage overwrites verbatim, with NO re-alignment --
	 * confirmed distinct from Initialize's own size rounding.
	 */
	printf("[5] SetTotalBytesToManage overwrites verbatim, no rounding\n");
	CSTGBankMemory::SetTotalBytesToManage(12345);
	check_eq("sTotalMemoryAvailable", CSTGBankMemory::sTotalMemoryAvailable, 12345);

	printf("=================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
