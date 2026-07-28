// SPDX-License-Identifier: GPL-2.0
/*
 * test_piano_osc_valuegetters.cpp  -  KAT for CPianoOsc's Get* family
 * -- 46 of 53 real weak-symbol candidates, see
 * ../src/engine/piano_osc_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/ctx-index-times-stride/width/signed/dual facts the
 * source file's own decoder used -- not by re-using the .cpp file's C
 * output strings -- against the same deterministic non-trivial byte
 * pattern as the rest of the STG value-getter family's KATs: buf[i] =
 * i times 0x9f plus 0x37, all mod 0x100. ctx's own dynamic-index field
 * at +0x4 is fixed at 3 for every ctx-indexed getter, matching the
 * established KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_piano_osc.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x900
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CPianoOsc value-getter family known-answer test (46 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CPianoOsc *s = (CPianoOsc *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetTranspose(ctx);
	check_eq("CPianoOsc::GetTranspose value", CSTGParamsOwner::sValueGetterTemp.value, -93L);
	s->GetOctave(ctx);
	check_eq("CPianoOsc::GetOctave value", CSTGParamsOwner::sValueGetterTemp.value, 66L);
	s->GetBankType(ctx);
	check_eq("CPianoOsc::GetBankType value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetMultisampleNum(ctx);
	check_eq("CPianoOsc::GetMultisampleNum value", CSTGParamsOwner::sValueGetterTemp.value, 33250L);
	s->GetLevel(ctx);
	check_eq("CPianoOsc::GetLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1134822758L);
	check_eq("CPianoOsc::GetLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1134822758L);
	s->GetBottomVelocity(ctx);
	check_eq("CPianoOsc::GetBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 191L);
	s->GetResonanceBankType(ctx);
	check_eq("CPianoOsc::GetResonanceBankType value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetResonanceMultisample(ctx);
	check_eq("CPianoOsc::GetResonanceMultisample value", CSTGParamsOwner::sValueGetterTemp.value, 54582L);
	s->GetResonanceMSLevel(ctx);
	check_eq("CPianoOsc::GetResonanceMSLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1745331782L);
	check_eq("CPianoOsc::GetResonanceMSLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1745331782L);
	s->GetResonanceBottomVelocity(ctx);
	check_eq("CPianoOsc::GetResonanceBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 19L);
	s->GetUnaCordaResonanceBankType(ctx);
	check_eq("CPianoOsc::GetUnaCordaResonanceBankType value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetUnaCordaResonanceMultisample(ctx);
	check_eq("CPianoOsc::GetUnaCordaResonanceMultisample value", CSTGParamsOwner::sValueGetterTemp.value, 10634L);
	s->GetUnaCordaResonanceMSLevel(ctx);
	check_eq("CPianoOsc::GetUnaCordaResonanceMSLevel value", CSTGParamsOwner::sValueGetterTemp.value, -347296498L);
	check_eq("CPianoOsc::GetUnaCordaResonanceMSLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -347296498L);
	s->GetUnaCordaResonanceBottomVelocity(ctx);
	check_eq("CPianoOsc::GetUnaCordaResonanceBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 103L);
	s->GetResonanceOn(ctx);
	check_eq("CPianoOsc::GetResonanceOn value", CSTGParamsOwner::sValueGetterTemp.value, 221L);
	s->GetResonanceLevel(ctx);
	check_eq("CPianoOsc::GetResonanceLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1505368956L);
	check_eq("CPianoOsc::GetResonanceLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1505368956L);
	s->GetResonanceRepedalScale(ctx);
	check_eq("CPianoOsc::GetResonanceRepedalScale value", CSTGParamsOwner::sValueGetterTemp.value, -717842440L);
	check_eq("CPianoOsc::GetResonanceRepedalScale displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -717842440L);
	s->GetResonanceAttack(ctx);
	check_eq("CPianoOsc::GetResonanceAttack value", CSTGParamsOwner::sValueGetterTemp.value, 1370624884L);
	check_eq("CPianoOsc::GetResonanceAttack displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1370624884L);
	s->GetResonanceRelease(ctx);
	check_eq("CPianoOsc::GetResonanceRelease value", CSTGParamsOwner::sValueGetterTemp.value, -852586512L);
	check_eq("CPianoOsc::GetResonanceRelease displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -852586512L);
	s->GetUnaCordaOn(ctx);
	check_eq("CPianoOsc::GetUnaCordaOn value", CSTGParamsOwner::sValueGetterTemp.value, 108L);
	s->GetVelocityBias(ctx);
	check_eq("CPianoOsc::GetVelocityBias value", CSTGParamsOwner::sValueGetterTemp.value, -397825525L);
	check_eq("CPianoOsc::GetVelocityBias displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -397825525L);
	s->GetVelocityAmount(ctx);
	check_eq("CPianoOsc::GetVelocityAmount value", CSTGParamsOwner::sValueGetterTemp.value, 1690642055L);
	check_eq("CPianoOsc::GetVelocityAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1690642055L);
	s->GetKeyOffNoiseEnable(ctx);
	check_eq("CPianoOsc::GetKeyOffNoiseEnable value", CSTGParamsOwner::sValueGetterTemp.value, 3L);
	s->GetKeyOffNoiseLevel(ctx);
	check_eq("CPianoOsc::GetKeyOffNoiseLevel value", CSTGParamsOwner::sValueGetterTemp.value, 2145403298L);
	check_eq("CPianoOsc::GetKeyOffNoiseLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2145403298L);
	s->GetReleaseSampleEnable(ctx);
	check_eq("CPianoOsc::GetReleaseSampleEnable value", CSTGParamsOwner::sValueGetterTemp.value, 30L);
	s->GetReleaseSampleLevel(ctx);
	check_eq("CPianoOsc::GetReleaseSampleLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1694802755L);
	check_eq("CPianoOsc::GetReleaseSampleLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1694802755L);
	s->GetKeyOffNoiseBankType(ctx);
	check_eq("CPianoOsc::GetKeyOffNoiseBankType value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetKeyOffNoiseMultisampleNum(ctx);
	check_eq("CPianoOsc::GetKeyOffNoiseMultisampleNum value", CSTGParamsOwner::sValueGetterTemp.value, 15774L);
	s->GetKeyOffNoiseMSLevel(ctx);
	check_eq("CPianoOsc::GetKeyOffNoiseMSLevel value", CSTGParamsOwner::sValueGetterTemp.value, -10436318L);
	check_eq("CPianoOsc::GetKeyOffNoiseMSLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -10436318L);
	s->GetKeyOffNoiseBottomVelocity(ctx);
	check_eq("CPianoOsc::GetKeyOffNoiseBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 123L);
	s->GetReleaseSampleBankType(ctx);
	check_eq("CPianoOsc::GetReleaseSampleBankType value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetReleaseMultisampleNum(ctx);
	check_eq("CPianoOsc::GetReleaseMultisampleNum value", CSTGParamsOwner::sValueGetterTemp.value, 37362L);
	s->GetReleaseMSLevel(ctx);
	check_eq("CPianoOsc::GetReleaseMSLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1404310902L);
	check_eq("CPianoOsc::GetReleaseMSLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1404310902L);
	s->GetReleaseBottomVelocity(ctx);
	check_eq("CPianoOsc::GetReleaseBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 207L);
	s->GetUnaCordaBankType(ctx);
	check_eq("CPianoOsc::GetUnaCordaBankType value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetUnaCordaMultisampleNum(ctx);
	check_eq("CPianoOsc::GetUnaCordaMultisampleNum value", CSTGParamsOwner::sValueGetterTemp.value, 58694L);
	s->GetUnaCordaMSLevel(ctx);
	check_eq("CPianoOsc::GetUnaCordaMSLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1492620854L);
	check_eq("CPianoOsc::GetUnaCordaMSLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1492620854L);
	s->GetUnaCordaBottomVelocity(ctx);
	check_eq("CPianoOsc::GetUnaCordaBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 35L);
	s->GetKeybedSize(ctx);
	check_eq("CPianoOsc::GetKeybedSize value", CSTGParamsOwner::sValueGetterTemp.value, -903115539L);
	s->GetUnaCordaReleaseBankType(ctx);
	check_eq("CPianoOsc::GetUnaCordaReleaseBankType value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetUnaCordaReleaseMultisampleNum(ctx);
	check_eq("CPianoOsc::GetUnaCordaReleaseMultisampleNum value", CSTGParamsOwner::sValueGetterTemp.value, 14746L);
	s->GetUnaCordaReleaseMSLevel(ctx);
	check_eq("CPianoOsc::GetUnaCordaReleaseMSLevel value", CSTGParamsOwner::sValueGetterTemp.value, -77808354L);
	check_eq("CPianoOsc::GetUnaCordaReleaseMSLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -77808354L);
	s->GetUnaCordaReleaseBottomVelocity(ctx);
	check_eq("CPianoOsc::GetUnaCordaReleaseBottomVelocity value", CSTGParamsOwner::sValueGetterTemp.value, 119L);
	s->GetDamperResonanceTrim(ctx);
	check_eq("CPianoOsc::GetDamperResonanceTrim value", CSTGParamsOwner::sValueGetterTemp.value, 1185351785L);
	check_eq("CPianoOsc::GetDamperResonanceTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1185351785L);
	s->GetMechanicalNoiseTrim(ctx);
	check_eq("CPianoOsc::GetMechanicalNoiseTrim value", CSTGParamsOwner::sValueGetterTemp.value, -1037859611L);
	check_eq("CPianoOsc::GetMechanicalNoiseTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1037859611L);
	s->GetNoteReleaseTrim(ctx);
	check_eq("CPianoOsc::GetNoteReleaseTrim value", CSTGParamsOwner::sValueGetterTemp.value, 1050607713L);
	check_eq("CPianoOsc::GetNoteReleaseTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1050607713L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
