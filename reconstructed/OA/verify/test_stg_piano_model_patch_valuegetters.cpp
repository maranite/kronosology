// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_piano_model_patch_valuegetters.cpp  -  KAT for
 * CSTGPianoModelPatch's Get* family plus its 2 sub-object accessor
 * helpers -- 16 of 18 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_piano_model_patch_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/ctx-index facts the source
 * file's own decoder used -- not by re-using the .cpp file's C output
 * strings -- against the same deterministic non-trivial byte pattern as
 * the rest of the STG value-getter family's KATs: buf[i] = i times 0x9f
 * plus 0x37, all mod 0x100. ctx's own dynamic-index field at +0x4 is
 * fixed at 3, matching the established KAT convention -- note this
 * class reads that field as a plain BYTE (CtxIndexByte), so only the
 * low byte of the stored int actually matters here.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_piano_model_patch.h"

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
	printf("CSTGPianoModelPatch value-getter family known-answer test (16 methods + 2 accessors)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPianoModelPatch *s = (CSTGPianoModelPatch *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	check_eq("CSTGPianoModelPatch::AccessSustainPedalDownVelocityZones offset",
		 (long)(s->AccessSustainPedalDownVelocityZones() - g_buf), 0x14L);
	check_eq("CSTGPianoModelPatch::AccessSustainPedalUpVelocityZones offset",
		 (long)(s->AccessSustainPedalUpVelocityZones() - g_buf), 0x78L);

	s->GetPianoType(ctx);
	check_eq("CSTGPianoModelPatch::GetPianoType value", CSTGParamsOwner::sValueGetterTemp.value, 171L);
	s->GetStereoPerspective(ctx);
	check_eq("CSTGPianoModelPatch::GetStereoPerspective value", CSTGParamsOwner::sValueGetterTemp.value, 74L);
	s->GetSustainPedalNoiseEnable(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalNoiseEnable value", CSTGParamsOwner::sValueGetterTemp.value, 233L);
	s->GetSustainPedalNoiseLevel(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalNoiseLevel value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGPianoModelPatch::GetSustainPedalNoiseLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetAmpVelocityIntensity(ctx);
	check_eq("CSTGPianoModelPatch::GetAmpVelocityIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1206289701L);
	check_eq("CSTGPianoModelPatch::GetAmpVelocityIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1206289701L);
	s->GetReleaseTime(ctx);
	check_eq("CSTGPianoModelPatch::GetReleaseTime value", CSTGParamsOwner::sValueGetterTemp.value, 882243159L);
	check_eq("CSTGPianoModelPatch::GetReleaseTime displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 882243159L);
	s->GetDamperDownNoiseTrim(ctx);
	check_eq("CSTGPianoModelPatch::GetDamperDownNoiseTrim value", CSTGParamsOwner::sValueGetterTemp.value, -1341033773L);
	check_eq("CSTGPianoModelPatch::GetDamperDownNoiseTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1341033773L);
	s->GetDamperUpNoiseTrim(ctx);
	check_eq("CSTGPianoModelPatch::GetDamperUpNoiseTrim value", CSTGParamsOwner::sValueGetterTemp.value, 747499087L);
	check_eq("CSTGPianoModelPatch::GetDamperUpNoiseTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 747499087L);
	s->GetSustainPedalDownMultisampleOnOff(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalDownMultisampleOnOff value", CSTGParamsOwner::sValueGetterTemp.value, 2L);
	s->GetSustainPedalDownMultisample(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalDownMultisample value", CSTGParamsOwner::sValueGetterTemp.value, 17316L);
	s->GetSustainPedalDownMultisampleLevel(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalDownMultisampleLevel value", CSTGParamsOwner::sValueGetterTemp.value, 90621736L);
	check_eq("CSTGPianoModelPatch::GetSustainPedalDownMultisampleLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 90621736L);
	s->GetSustainPedalDownBottomVelocity(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalDownBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 129L);
	s->GetSustainPedalUpMultisampleOnOff(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalUpMultisampleOnOff value", CSTGParamsOwner::sValueGetterTemp.value, 2L);
	s->GetSustainPedalUpMultisample(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalUpMultisample value", CSTGParamsOwner::sValueGetterTemp.value, 24512L);
	s->GetSustainPedalUpMultisampleLevel(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalUpMultisampleLevel value", CSTGParamsOwner::sValueGetterTemp.value, 562225988L);
	check_eq("CSTGPianoModelPatch::GetSustainPedalUpMultisampleLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 562225988L);
	s->GetSustainPedalUpBottomVelocity(ctx);
	check_eq("CSTGPianoModelPatch::GetSustainPedalUpBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 157L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
