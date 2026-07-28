// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_vpm_noise_valuegetters.cpp  -  KAT for CSTGVPMNoise's Get*
 * family -- all 7 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_vpm_noise_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own decoder
 * used -- not by re-using the .cpp file's C output strings -- against the
 * same deterministic non-trivial byte pattern as the rest of the STG
 * value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all mod
 * 0x100. This class has no ctx-dynamic-index methods, so ctx's own fields
 * are never read, but the buffer is still filled the same way for
 * consistency with the family's KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_vpm_noise.h"

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
	printf("CSTGVPMNoise value-getter family known-answer test (7 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGVPMNoise *s = (CSTGVPMNoise *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetSaturation(ctx);
	check_eq("CSTGVPMNoise::GetSaturation value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetCutoff(ctx);
	check_eq("CSTGVPMNoise::GetCutoff value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGVPMNoise::GetCutoff displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetVolume(ctx);
	check_eq("CSTGVPMNoise::GetVolume value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGVPMNoise::GetVolume displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetVolumeEGSelect(ctx);
	check_eq("CSTGVPMNoise::GetVolumeEGSelect value", CSTGParamsOwner::sValueGetterTemp.value, 31L);
	s->GetVolumeAMSSource(ctx);
	check_eq("CSTGVPMNoise::GetVolumeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 58L);
	s->GetVolumeAMSIntensity(ctx);
	check_eq("CSTGVPMNoise::GetVolumeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1677959746L);
	check_eq("CSTGVPMNoise::GetVolumeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1677959746L);
	s->GetVolumeAMSMode(ctx);
	check_eq("CSTGVPMNoise::GetVolumeAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -39L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
