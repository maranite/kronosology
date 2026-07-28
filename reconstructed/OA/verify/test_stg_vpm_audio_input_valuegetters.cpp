// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_vpm_audio_input_valuegetters.cpp  -  KAT for
 * CSTGVPMAudioInput's Get* family -- all 4 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_vpm_audio_input_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest of
 * the STG value-getter family's KATs: buf[i] = i times 0x9f plus 0x37,
 * all mod 0x100.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_vpm_audio_input.h"

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

int main(void)
{
	printf("CSTGVPMAudioInput value-getter family known-answer test (4 methods)\n");
	printf("=====================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);

	CSTGVPMAudioInput *s = (CSTGVPMAudioInput *)g_buf;
	CSTGPatchMessageContext dummy_ctx;
	CSTGPatchMessageContext &ctx = dummy_ctx;

	s->GetLevel(ctx);
	check_eq("CSTGVPMAudioInput::GetLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGVPMAudioInput::GetLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetAMSSource(ctx);
	check_eq("CSTGVPMAudioInput::GetAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -93L);
	s->GetAMSIntensity(ctx);
	check_eq("CSTGVPMAudioInput::GetAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGVPMAudioInput::GetAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetAMSMode(ctx);
	check_eq("CSTGVPMAudioInput::GetAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, 66L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
