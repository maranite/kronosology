// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_pan_output_base_valuegetters.cpp  -  KAT for
 * CSTGPanOutputBase's Get* family -- all 9 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_pan_output_base_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest
 * of the STG value-getter family's KATs: buf[i] = i times 0x9f plus
 * 0x37, all mod 0x100. GetPatchSolo needs no instance data at all (it
 * unconditionally returns 0), so its KAT is a plain literal check.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_pan_output_base.h"

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
	printf("CSTGPanOutputBase value-getter family known-answer test (9 methods)\n");
	printf("=====================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPanOutputBase *s = (CSTGPanOutputBase *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetPanUseDrumkitSetting(ctx);
	check_eq("CSTGPanOutputBase::GetPanUseDrumkitSetting value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetPanAMSSource(ctx);
	check_eq("CSTGPanOutputBase::GetPanAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 39L);
	s->GetPanAMSIntensity(ctx);
	check_eq("CSTGPanOutputBase::GetPanAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGPanOutputBase::GetPanAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetPan(ctx);
	check_eq("CSTGPanOutputBase::GetPan value", CSTGParamsOwner::sValueGetterTemp.value, -1559992890L);
	check_eq("CSTGPanOutputBase::GetPan displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1559992890L);
	s->GetSend1Level(ctx);
	check_eq("CSTGPanOutputBase::GetSend1Level value", CSTGParamsOwner::sValueGetterTemp.value, 528539970L);
	check_eq("CSTGPanOutputBase::GetSend1Level displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 528539970L);
	s->GetSend2Level(ctx);
	check_eq("CSTGPanOutputBase::GetSend2Level value", CSTGParamsOwner::sValueGetterTemp.value, -1677959746L);
	check_eq("CSTGPanOutputBase::GetSend2Level displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1677959746L);
	s->GetPatchLevel(ctx);
	check_eq("CSTGPanOutputBase::GetPatchLevel value", CSTGParamsOwner::sValueGetterTemp.value, 393795898L);
	check_eq("CSTGPanOutputBase::GetPatchLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 393795898L);
	s->GetPatchMute(ctx);
	check_eq("CSTGPanOutputBase::GetPatchMute value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetPatchSolo(ctx);
	check_eq("CSTGPanOutputBase::GetPatchSolo value", CSTGParamsOwner::sValueGetterTemp.value, 0L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
