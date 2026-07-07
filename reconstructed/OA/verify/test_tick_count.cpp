// SPDX-License-Identifier: GPL-2.0
/*
 * test_tick_count.cpp  -  host-side known-answer test for GetSTGTickCount()
 * (src/engine/tick_count.cpp, sec 10.183).
 *
 * Links only src/engine/tick_count.cpp. GetSTGTickCount reads the u32 at
 * CSTGGlobal::sInstance + 0x29c9fa8; rather than allocate a ~44MB fake
 * singleton, point sInstance so that (sInstance + 0x29c9fa8) lands on a
 * small local variable (the project's offset-base trick) and drive that
 * variable directly. sInstance itself is never dereferenced, only the
 * computed field address is -- so the fabricated base is safe.
 *
 * Provides CSTGGlobal::sInstance's storage locally (the real storage lives
 * in global.cpp, which this test does not link).
 */

#include <cstdio>
#include <cstdint>
#include "oa_global.h"

CSTGGlobal *CSTGGlobal::sInstance;

#define STG_TICK_COUNT_OFFSET 0x29c9fa8

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) { printf("  ok    %-40s 0x%lx\n", label, got); return; }
	printf("  FAIL  %-40s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

int main(void)
{
	printf("GetSTGTickCount known-answer test\n");
	printf("=================================\n");

	unsigned int tickField = 0;
	/* Fabricate sInstance so sInstance + 0x29c9fa8 == &tickField. Compute
	 * the base via integer arithmetic (uintptr_t), not pointer arithmetic
	 * off &tickField -- the latter makes GCC's -Warray-bounds flag the
	 * deliberately-out-of-object base as if it were a real overrun. */
	CSTGGlobal::sInstance =
		(CSTGGlobal *)((uintptr_t)&tickField - (uintptr_t)STG_TICK_COUNT_OFFSET);

	printf("[1] reads the u32 at sInstance + 0x29c9fa8 verbatim\n");
	tickField = 0;
	check_eq("tick == 0", GetSTGTickCount(), 0);
	tickField = 0x12345678;
	check_eq("tick == 0x12345678", GetSTGTickCount(), 0x12345678);
	tickField = 0xFFFFFFFFu;
	check_eq("tick == 0xFFFFFFFF (full range)", GetSTGTickCount(), 0xFFFFFFFFu);
	tickField = 1000;
	check_eq("tick tracks the field live", GetSTGTickCount(), 1000);

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
