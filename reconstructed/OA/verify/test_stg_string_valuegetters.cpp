// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_string_valuegetters.cpp  -  KAT for CSTGString's Get*() family
 * (see ../src/engine/stg_string_valuegetters.cpp).
 *
 * For all 105 methods, the expected value is computed here by a
 * SEPARATE evaluator (embedded as a literal constant per case, generated
 * from the SAME parsed (offset, ctx-index*stride, width, signed, shift,
 * mask) facts the source file's own decoder used -- but via an independent
 * Python arithmetic path, not by re-using stg_string_valuegetters.cpp's C
 * output strings) against a deterministic non-trivial byte pattern in the
 * `this` buffer (buf[i] = (i*0x9f + 0x37) & 0xff -- chosen so every
 * individual bit position is independently distinguishable, unlike an
 * all-same-byte pattern). ctx's own dynamic index field (+0x4) is fixed
 * at 3 for every Pickup*, MixerPickup* indexed getter.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_string.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x230
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGString value-getter family known-answer test (105 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGString *s = (CSTGString *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetDamping(ctx);
	check_eq("CSTGString::GetDamping value", CSTGParamsOwner::sValueGetterTemp.value, -465197561L);
	check_eq("CSTGString::GetDamping displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -465197561L);
	s->GetDampingAMSIntensity(ctx);
	check_eq("CSTGString::GetDampingAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1324190764L);
	check_eq("CSTGString::GetDampingAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1324190764L);
	s->GetDampingAMSIntensity1AMSIntensity(ctx);
	check_eq("CSTGString::GetDampingAMSIntensity1AMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1324190764L);
	check_eq("CSTGString::GetDampingAMSIntensity1AMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1324190764L);
	s->GetDampingAMSIntensity1AMSSource(ctx);
	check_eq("CSTGString::GetDampingAMSIntensity1AMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 80L);
	s->GetDampingAMSSource(ctx);
	check_eq("CSTGString::GetDampingAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 80L);
	s->GetDampingStringTrackIntensity(ctx);
	check_eq("CSTGString::GetDampingStringTrackIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1341033773L);
	check_eq("CSTGString::GetDampingStringTrackIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1341033773L);
	s->GetDecay(ctx);
	check_eq("CSTGString::GetDecay value", CSTGParamsOwner::sValueGetterTemp.value, -1677959746L);
	check_eq("CSTGString::GetDecay displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1677959746L);
	s->GetDecayAMSIntensity(ctx);
	check_eq("CSTGString::GetDecayAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1758014091L);
	check_eq("CSTGString::GetDecayAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1758014091L);
	s->GetDecayAMSSource(ctx);
	check_eq("CSTGString::GetDecayAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 7L);
	s->GetDispersion(ctx);
	check_eq("CSTGString::GetDispersion value", CSTGParamsOwner::sValueGetterTemp.value, -869429521L);
	check_eq("CSTGString::GetDispersion displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -869429521L);
	s->GetDispersionAMSIntensity(ctx);
	check_eq("CSTGString::GetDispersionAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1711645764L);
	check_eq("CSTGString::GetDispersionAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1711645764L);
	s->GetDispersionAMSIntensity1AMSIntensity(ctx);
	check_eq("CSTGString::GetDispersionAMSIntensity1AMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1711645764L);
	check_eq("CSTGString::GetDispersionAMSIntensity1AMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1711645764L);
	s->GetDispersionAMSIntensity1AMSSource(ctx);
	check_eq("CSTGString::GetDispersionAMSIntensity1AMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 56L);
	s->GetDispersionAMSSource(ctx);
	check_eq("CSTGString::GetDispersionAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 56L);
	s->GetDispersionStringTrackIntensity(ctx);
	check_eq("CSTGString::GetDispersionStringTrackIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGString::GetDispersionStringTrackIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetDispersionType(ctx);
	check_eq("CSTGString::GetDispersionType value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetFeedbackLevel(ctx);
	check_eq("CSTGString::GetFeedbackLevel value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGString::GetFeedbackLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetFeedbackLevelAMSIntensity(ctx);
	check_eq("CSTGString::GetFeedbackLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGString::GetFeedbackLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetFeedbackLevelAMSSource(ctx);
	check_eq("CSTGString::GetFeedbackLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 31L);
	s->GetHarmonicPosition(ctx);
	check_eq("CSTGString::GetHarmonicPosition value", CSTGParamsOwner::sValueGetterTemp.value, 966458204L);
	check_eq("CSTGString::GetHarmonicPosition displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 966458204L);
	s->GetHarmonicPositionAMSIntensity(ctx);
	check_eq("CSTGString::GetHarmonicPositionAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1256818728L);
	check_eq("CSTGString::GetHarmonicPositionAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1256818728L);
	s->GetHarmonicPositionAMSSource(ctx);
	check_eq("CSTGString::GetHarmonicPositionAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 84L);
	s->GetHarmonicPositionScaling(ctx);
	check_eq("CSTGString::GetHarmonicPositionScaling value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetHarmonicPressure(ctx);
	check_eq("CSTGString::GetHarmonicPressure value", CSTGParamsOwner::sValueGetterTemp.value, -802057485L);
	check_eq("CSTGString::GetHarmonicPressure displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -802057485L);
	s->GetHarmonicPressureAMSIntensity(ctx);
	check_eq("CSTGString::GetHarmonicPressureAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1644273728L);
	check_eq("CSTGString::GetHarmonicPressureAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1644273728L);
	s->GetHarmonicPressureAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGString::GetHarmonicPressureAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -751528458L);
	check_eq("CSTGString::GetHarmonicPressureAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -751528458L);
	s->GetHarmonicPressureAMSIntensityAMSSource(ctx);
	check_eq("CSTGString::GetHarmonicPressureAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 114L);
	s->GetHarmonicPressureAMSSource(ctx);
	check_eq("CSTGString::GetHarmonicPressureAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 60L);
	s->GetHarmonicUseExcitationPosition(ctx);
	check_eq("CSTGString::GetHarmonicUseExcitationPosition value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetMixerNoiseBalance(ctx);
	check_eq("CSTGString::GetMixerNoiseBalance value", CSTGParamsOwner::sValueGetterTemp.value, 528539970L);
	check_eq("CSTGString::GetMixerNoiseBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 528539970L);
	s->GetMixerNoiseBalanceAMSIntensity(ctx);
	check_eq("CSTGString::GetMixerNoiseBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1677959746L);
	check_eq("CSTGString::GetMixerNoiseBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1677959746L);
	s->GetMixerNoiseBalanceAMSSource(ctx);
	check_eq("CSTGString::GetMixerNoiseBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 58L);
	s->GetMixerNoiseLevel(ctx);
	check_eq("CSTGString::GetMixerNoiseLevel value", CSTGParamsOwner::sValueGetterTemp.value, 208522799L);
	check_eq("CSTGString::GetMixerNoiseLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 208522799L);
	s->GetMixerNoiseLevelAMSIntensity(ctx);
	check_eq("CSTGString::GetMixerNoiseLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGString::GetMixerNoiseLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetMixerNoiseLevelAMSSource(ctx);
	check_eq("CSTGString::GetMixerNoiseLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 39L);
	s->GetMixerNoisePhaseInvert(ctx);
	check_eq("CSTGString::GetMixerNoisePhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, -1559992890L);
	check_eq("CSTGString::GetMixerNoisePhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1559992890L);
	s->GetMixerPCMBalance(ctx);
	check_eq("CSTGString::GetMixerPCMBalance value", CSTGParamsOwner::sValueGetterTemp.value, 1976973208L);
	check_eq("CSTGString::GetMixerPCMBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1976973208L);
	s->GetMixerPCMBalanceAMSIntensity(ctx);
	check_eq("CSTGString::GetMixerPCMBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -246238444L);
	check_eq("CSTGString::GetMixerPCMBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -246238444L);
	s->GetMixerPCMBalanceAMSSource(ctx);
	check_eq("CSTGString::GetMixerPCMBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -112L);
	s->GetMixerPCMLevel(ctx);
	check_eq("CSTGString::GetMixerPCMLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1656956037L);
	check_eq("CSTGString::GetMixerPCMLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1656956037L);
	s->GetMixerPCMLevelAMSIntensity(ctx);
	check_eq("CSTGString::GetMixerPCMLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -566255615L);
	check_eq("CSTGString::GetMixerPCMLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -566255615L);
	s->GetMixerPCMLevelAMSSource(ctx);
	check_eq("CSTGString::GetMixerPCMLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 125L);
	s->GetMixerPCMPhaseInvert(ctx);
	check_eq("CSTGString::GetMixerPCMPhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, -111494372L);
	check_eq("CSTGString::GetMixerPCMPhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -111494372L);
	s->GetMixerPickupBalance(ctx);
	check_eq("CSTGString::GetMixerPickupBalance value", CSTGParamsOwner::sValueGetterTemp.value, 1656956037L);
	check_eq("CSTGString::GetMixerPickupBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1656956037L);
	s->GetMixerPickupBalanceAMSIntensity(ctx);
	check_eq("CSTGString::GetMixerPickupBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -566255615L);
	check_eq("CSTGString::GetMixerPickupBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -566255615L);
	s->GetMixerPickupBalanceAMSSource(ctx);
	check_eq("CSTGString::GetMixerPickupBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 125L);
	s->GetMixerPickupLevel(ctx);
	check_eq("CSTGString::GetMixerPickupLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1336938866L);
	check_eq("CSTGString::GetMixerPickupLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1336938866L);
	s->GetMixerPickupLevelAMSIntensity(ctx);
	check_eq("CSTGString::GetMixerPickupLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -886272530L);
	check_eq("CSTGString::GetMixerPickupLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -886272530L);
	s->GetMixerPickupLevelAMSSource(ctx);
	check_eq("CSTGString::GetMixerPickupLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 106L);
	s->GetMixerPickupPhaseInvert(ctx);
	check_eq("CSTGString::GetMixerPickupPhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, -431511543L);
	check_eq("CSTGString::GetMixerPickupPhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -431511543L);
	s->GetMixerStringBalance(ctx);
	check_eq("CSTGString::GetMixerStringBalance value", CSTGParamsOwner::sValueGetterTemp.value, -886272530L);
	check_eq("CSTGString::GetMixerStringBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -886272530L);
	s->GetMixerStringBalanceAMSIntensity(ctx);
	check_eq("CSTGString::GetMixerStringBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1202194794L);
	check_eq("CSTGString::GetMixerStringBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1202194794L);
	s->GetMixerStringBalanceAMSSource(ctx);
	check_eq("CSTGString::GetMixerStringBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -26L);
	s->GetMixerStringLevel(ctx);
	check_eq("CSTGString::GetMixerStringLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1206289701L);
	check_eq("CSTGString::GetMixerStringLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1206289701L);
	s->GetMixerStringLevelAMSIntensity(ctx);
	check_eq("CSTGString::GetMixerStringLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 882243159L);
	check_eq("CSTGString::GetMixerStringLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 882243159L);
	s->GetMixerStringLevelAMSSource(ctx);
	check_eq("CSTGString::GetMixerStringLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -45L);
	s->GetMixerStringPhaseInvert(ctx);
	check_eq("CSTGString::GetMixerStringPhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, 1336938866L);
	check_eq("CSTGString::GetMixerStringPhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1336938866L);
	s->GetNoiseCutoff(ctx);
	check_eq("CSTGString::GetNoiseCutoff value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGString::GetNoiseCutoff displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetNoiseCutoffAMSIntensity(ctx);
	check_eq("CSTGString::GetNoiseCutoffAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGString::GetNoiseCutoffAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetNoiseCutoffAMSSource(ctx);
	check_eq("CSTGString::GetNoiseCutoffAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 31L);
	s->GetNoiseLevel(ctx);
	check_eq("CSTGString::GetNoiseLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1677959746L);
	check_eq("CSTGString::GetNoiseLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1677959746L);
	s->GetNoiseLevelAMSIntensity(ctx);
	check_eq("CSTGString::GetNoiseLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -465197561L);
	check_eq("CSTGString::GetNoiseLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -465197561L);
	s->GetNoiseLevelAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGString::GetNoiseLevelAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -919958548L);
	check_eq("CSTGString::GetNoiseLevelAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -919958548L);
	s->GetNoiseLevelAMSIntensityAMSSource(ctx);
	check_eq("CSTGString::GetNoiseLevelAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 104L);
	s->GetNoiseLevelAMSSource(ctx);
	check_eq("CSTGString::GetNoiseLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -125L);
	s->GetNoiseLevelPhaseInvert(ctx);
	check_eq("CSTGString::GetNoiseLevelPhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, 393795898L);
	check_eq("CSTGString::GetNoiseLevelPhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 393795898L);
	s->GetNoiseLevelPrePost(ctx);
	check_eq("CSTGString::GetNoiseLevelPrePost value", CSTGParamsOwner::sValueGetterTemp.value, -465197561L);
	s->GetNonlinearity(ctx);
	check_eq("CSTGString::GetNonlinearity value", CSTGParamsOwner::sValueGetterTemp.value, -1273661737L);
	check_eq("CSTGString::GetNonlinearity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1273661737L);
	s->GetNonlinearityAMSIntensity(ctx);
	check_eq("CSTGString::GetNonlinearityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2115877980L);
	check_eq("CSTGString::GetNonlinearityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2115877980L);
	s->GetNonlinearityAMSSource(ctx);
	check_eq("CSTGString::GetNonlinearityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 32L);
	s->GetPCMLevel(ctx);
	check_eq("CSTGString::GetPCMLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1623270019L);
	check_eq("CSTGString::GetPCMLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1623270019L);
	s->GetPCMLevelAMSIntensity(ctx);
	check_eq("CSTGString::GetPCMLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1458934836L);
	check_eq("CSTGString::GetPCMLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1458934836L);
	s->GetPCMLevelAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGString::GetPCMLevelAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1896918863L);
	check_eq("CSTGString::GetPCMLevelAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1896918863L);
	s->GetPCMLevelAMSIntensityAMSSource(ctx);
	check_eq("CSTGString::GetPCMLevelAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 45L);
	s->GetPCMLevelAMSSource(ctx);
	check_eq("CSTGString::GetPCMLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 72L);
	s->GetPCMLevelPhaseInvert(ctx);
	check_eq("CSTGString::GetPCMLevelPhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, -599941377L);
	check_eq("CSTGString::GetPCMLevelPhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -599941377L);
	s->GetPCMLevelPrePost(ctx);
	check_eq("CSTGString::GetPCMLevelPrePost value", CSTGParamsOwner::sValueGetterTemp.value, -1458934836L);
	s->GetPickPosition(ctx);
	check_eq("CSTGString::GetPickPosition value", CSTGParamsOwner::sValueGetterTemp.value, 1724328073L);
	check_eq("CSTGString::GetPickPosition displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1724328073L);
	s->GetPickPositionAMSIntensity(ctx);
	check_eq("CSTGString::GetPickPositionAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -498883579L);
	check_eq("CSTGString::GetPickPositionAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -498883579L);
	s->GetPickPositionAMSSource(ctx);
	check_eq("CSTGString::GetPickPositionAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -127L);
	s->GetPickPositionScaling(ctx);
	check_eq("CSTGString::GetPickPositionScaling value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetPickPositionTone(ctx);
	check_eq("CSTGString::GetPickPositionTone value", CSTGParamsOwner::sValueGetterTemp.value, -44122336L);
	check_eq("CSTGString::GetPickPositionTone displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -44122336L);
	s->GetPickupPosition(ctx);
	check_eq("CSTGString::GetPickupPosition value", CSTGParamsOwner::sValueGetterTemp.value, 427481916L);
	check_eq("CSTGString::GetPickupPosition displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 427481916L);
	s->GetPickupPositionAMSIntensity(ctx);
	check_eq("CSTGString::GetPickupPositionAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1779017800L);
	check_eq("CSTGString::GetPickupPositionAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1779017800L);
	s->GetPickupPositionAMSSource(ctx);
	check_eq("CSTGString::GetPickupPositionAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 52L);
	s->GetPickupPositionScaling(ctx);
	check_eq("CSTGString::GetPickupPositionScaling value", CSTGParamsOwner::sValueGetterTemp.value, 211L);
	s->GetPluckDelayAMSSource(ctx);
	check_eq("CSTGString::GetPluckDelayAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 70L);
	s->GetPluckLevel(ctx);
	check_eq("CSTGString::GetPluckLevel value", CSTGParamsOwner::sValueGetterTemp.value, 629598024L);
	check_eq("CSTGString::GetPluckLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 629598024L);
	s->GetPluckLevelAMSIntensity(ctx);
	check_eq("CSTGString::GetPluckLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1859072145L);
	check_eq("CSTGString::GetPluckLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1859072145L);
	s->GetPluckLevelAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGString::GetPluckLevelAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1404310902L);
	check_eq("CSTGString::GetPluckLevelAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1404310902L);
	s->GetPluckLevelAMSIntensityAMSSource(ctx);
	check_eq("CSTGString::GetPluckLevelAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -14L);
	s->GetPluckLevelAMSSource(ctx);
	check_eq("CSTGString::GetPluckLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 13L);
	s->GetPluckLevelPhaseInvert(ctx);
	check_eq("CSTGString::GetPluckLevelPhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, -1593678908L);
	check_eq("CSTGString::GetPluckLevelPhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1593678908L);
	s->GetPluckLevelPrePost(ctx);
	check_eq("CSTGString::GetPluckLevelPrePost value", CSTGParamsOwner::sValueGetterTemp.value, 1859072145L);
	s->GetPluckRandomAmt(ctx);
	check_eq("CSTGString::GetPluckRandomAmt value", CSTGParamsOwner::sValueGetterTemp.value, -364139507L);
	check_eq("CSTGString::GetPluckRandomAmt displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -364139507L);
	s->GetPluckRandomAmtAMSIntensity(ctx);
	check_eq("CSTGString::GetPluckRandomAmtAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1724328073L);
	check_eq("CSTGString::GetPluckRandomAmtAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1724328073L);
	s->GetPluckRandomAmtAMSSource(ctx);
	check_eq("CSTGString::GetPluckRandomAmtAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 5L);
	s->GetPluckRandomUseFilter(ctx);
	check_eq("CSTGString::GetPluckRandomUseFilter value", CSTGParamsOwner::sValueGetterTemp.value, -2115877980L);
	s->GetPluckTableSelect(ctx);
	check_eq("CSTGString::GetPluckTableSelect value", CSTGParamsOwner::sValueGetterTemp.value, -1357876782L);
	s->GetPluckWidth(ctx);
	check_eq("CSTGString::GetPluckWidth value", CSTGParamsOwner::sValueGetterTemp.value, -44122336L);
	check_eq("CSTGString::GetPluckWidth displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -44122336L);
	s->GetPluckWidthAMSIntensity(ctx);
	check_eq("CSTGString::GetPluckWidthAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -903115539L);
	check_eq("CSTGString::GetPluckWidthAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -903115539L);
	s->GetPluckWidthAMSSource(ctx);
	check_eq("CSTGString::GetPluckWidthAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 105L);
	s->GetRelease(ctx);
	check_eq("CSTGString::GetRelease value", CSTGParamsOwner::sValueGetterTemp.value, 1303252848L);
	check_eq("CSTGString::GetRelease displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1303252848L);
	s->GetReleaseAMSIntensity(ctx);
	check_eq("CSTGString::GetReleaseAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -919958548L);
	check_eq("CSTGString::GetReleaseAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -919958548L);
	s->GetReleaseAMSSource(ctx);
	check_eq("CSTGString::GetReleaseAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 104L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
