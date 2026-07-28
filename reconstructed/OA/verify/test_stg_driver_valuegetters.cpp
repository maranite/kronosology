// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_driver_valuegetters.cpp  -  KAT for CSTGDriver's Get* family --
 * all 7 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_driver_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/bitfield facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings -- against
 * the same deterministic non-trivial byte pattern as the rest of the STG
 * value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all mod
 * 0x100. This class has no ctx-dynamic-index methods, so ctx's own fields
 * are never read, but the buffer is still filled the same way for
 * consistency with the family's KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_driver.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x900
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGDriver value-getter family known-answer test (7 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGDriver *s = (CSTGDriver *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetBypass(ctx);
	check_eq("CSTGDriver::GetBypass value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetDrive(ctx);
	check_eq("CSTGDriver::GetDrive value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGDriver::GetDrive displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetDriveAMSSource(ctx);
	check_eq("CSTGDriver::GetDriveAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 31L);
	s->GetDriveAMSIntensity(ctx);
	check_eq("CSTGDriver::GetDriveAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGDriver::GetDriveAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetBoost(ctx);
	check_eq("CSTGDriver::GetBoost value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGDriver::GetBoost displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetBoostAMSSource(ctx);
	check_eq("CSTGDriver::GetBoostAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 58L);
	s->GetBoostAMSIntensity(ctx);
	check_eq("CSTGDriver::GetBoostAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1677959746L);
	check_eq("CSTGDriver::GetBoostAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1677959746L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
