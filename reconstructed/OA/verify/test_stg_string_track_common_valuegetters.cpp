// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_string_track_common_valuegetters.cpp  -  KAT for
 * CSTGStringTrackCommon's Get* family -- all 4 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_string_track_common_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/ctx-index facts the source file's
 * own decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest of
 * the STG value-getter family's KATs: buf[i] = i times 0x9f plus 0x37,
 * all mod 0x100. ctx's own dynamic-index field at +0x4 is fixed at 3,
 * matching the established KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_string_track_common.h"

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
	printf("CSTGStringTrackCommon value-getter family known-answer test (4 methods)\n");
	printf("=========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGStringTrackCommon *s = (CSTGStringTrackCommon *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetFretPosition(ctx);
	check_eq("CSTGStringTrackCommon::GetFretPosition value", CSTGParamsOwner::sValueGetterTemp.value, 171L);
	s->GetFretPositionAMSSource(ctx);
	check_eq("CSTGStringTrackCommon::GetFretPositionAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -58L);
	s->GetFretPositionAMSIntensity(ctx);
	check_eq("CSTGStringTrackCommon::GetFretPositionAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 663284042L);
	check_eq("CSTGStringTrackCommon::GetFretPositionAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 663284042L);
	s->GetStringNoteValue(ctx);
	check_eq("CSTGStringTrackCommon::GetStringNoteValue value", CSTGParamsOwner::sValueGetterTemp.value, 66L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
