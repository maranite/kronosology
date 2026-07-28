// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_effect_balance_valuegetters.cpp  -  KAT for
 * CSTGEffectBalance's Get* family -- all 3 real strong-linkage
 * ctx-only candidates, see
 * ../src/engine/stg_effect_balance_valuegetters.cpp.
 *
 * Unlike every other class in this family, none of these three read
 * `this` or `ctx` -- each resolves the process-wide active
 * `CSTGPerformanceVarsManager` via the same raw selector-array lookup
 * as `CSTGGlobal::ResolveActivePerformanceVarsManager()` (global.cpp,
 * NOT called directly -- see stg_effect_balance_valuegetters.cpp's own
 * header comment for why) and reads one fixed field off it. This test
 * supplies its own definition of the real, otherwise-external
 * `CSTGPerformanceVarsManager::sInstance` static (same convention as
 * `CSTGParamsOwner::sValueGetterTemp` below) rather than linking
 * global.cpp's whole dependency chain in just for this one array.
 *
 * `mmap32()` matches this project's own established fix (see
 * verify/test_global.cpp's own copy) for pointing
 * `CSTGPerformanceVarsManager::sInstance`'s packed 32-bit pointer slot
 * at a real, controlled buffer on a 64-bit host.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/dual facts the source file's own
 * disassembly-derived translation used -- not by re-using the .cpp
 * file's C output strings -- against the same deterministic non-trivial
 * byte pattern as the rest of the STG value-getter family's KATs:
 * buf[i] = i times 0x9f plus 0x37, all mod 0x100.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_stg_effect_balance.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;
unsigned char CSTGPerformanceVarsManager::sInstance[12];

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

#define BUFSZ 0x2200
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGEffectBalance value-getter family known-answer test (3 methods)\n");
	printf("========================================================================\n");

	unsigned char *mgr = mmap32(BUFSZ);
	for (unsigned int i = 0; i < BUFSZ; i++)
		mgr[i] = (unsigned char)(i*0x9f + 0x37);

	*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgr;
	CSTGPerformanceVarsManager::sInstance[8] = 0;

	CSTGEffectBalance s;
	CSTGMessageContext &ctx = *(CSTGMessageContext *)g_ctxbuf;

	s.GetIFXBalance(ctx);
	check_eq("CSTGEffectBalance::GetIFXBalance value", CSTGParamsOwner::sValueGetterTemp.value, -330453489L);
	check_eq("CSTGEffectBalance::GetIFXBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -330453489L);
	s.GetMFXBalance(ctx);
	check_eq("CSTGEffectBalance::GetMFXBalance value", CSTGParamsOwner::sValueGetterTemp.value, 1758014091L);
	s.GetTFXBalance(ctx);
	check_eq("CSTGEffectBalance::GetTFXBalance value", CSTGParamsOwner::sValueGetterTemp.value, -465197561L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
