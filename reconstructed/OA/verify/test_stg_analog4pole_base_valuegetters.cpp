// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_analog4pole_base_valuegetters.cpp  -  KAT for CSTGAnalog4PoleBase's Get*() family
 * (74 methods, see ../src/engine/stg_analog4pole_base_valuegetters.cpp).
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed (offset, ctx-index*stride, width, signed, dual) facts the
 * source file's own decoder used -- not by re-using the .cpp file's C
 * output strings -- against the same deterministic non-trivial byte
 * pattern as the CSTGString/CSTGOrganModelPatch/CSTGMS20 KATs (buf[i] =
 * (i*0x9f+0x37) & 0xff). ctx's own dynamic-index field (+0x4) is fixed at
 * 3 for every ctx-indexed getter, matching the established KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_analog4pole_base.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x300
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGAnalog4PoleBase value-getter family known-answer test (74 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGAnalog4PoleBase *s = (CSTGAnalog4PoleBase *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetFilterABandpass1(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterABandpass1 value", CSTGParamsOwner::sValueGetterTemp.value, -2014819926L);
	check_eq("CSTGAnalog4PoleBase::GetFilterABandpass1 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2014819926L);
	s->GetFilterABandpass2(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterABandpass2 value", CSTGParamsOwner::sValueGetterTemp.value, 2010659226L);
	check_eq("CSTGAnalog4PoleBase::GetFilterABandpass2 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2010659226L);
	s->GetFilterABypass(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterABypass value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetFilterADry1(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterADry1 value", CSTGParamsOwner::sValueGetterTemp.value, 56935718L);
	check_eq("CSTGAnalog4PoleBase::GetFilterADry1 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 56935718L);
	s->GetFilterADry2(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterADry2 value", CSTGParamsOwner::sValueGetterTemp.value, -212552426L);
	check_eq("CSTGAnalog4PoleBase::GetFilterADry2 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -212552426L);
	s->GetFilterAEGAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAEGAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -869429521L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAEGAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -869429521L);
	s->GetFilterAEGAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAEGAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 107L);
	s->GetFilterAEGIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAEGIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 579068997L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAEGIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 579068997L);
	s->GetFilterAEGSelect(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAEGSelect value", CSTGParamsOwner::sValueGetterTemp.value, 177L);
	s->GetFilterAEGVelocity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAEGVelocity value", CSTGParamsOwner::sValueGetterTemp.value, -1627430719L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAEGVelocity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1627430719L);
	s->GetFilterAFilterType(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFilterType value", CSTGParamsOwner::sValueGetterTemp.value, 67L);
	s->GetFilterAFilterType1(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFilterType1 value", CSTGParamsOwner::sValueGetterTemp.value, -30L);
	s->GetFilterAFilterType2(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFilterType2 value", CSTGParamsOwner::sValueGetterTemp.value, -127L);
	s->GetFilterAFreqAMS1Intensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS1Intensity value", CSTGParamsOwner::sValueGetterTemp.value, -414668534L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS1Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -414668534L);
	s->GetFilterAFreqAMS1IntensityAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS1IntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 494853952L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS1IntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 494853952L);
	s->GetFilterAFreqAMS1IntensityAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS1IntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -68L);
	s->GetFilterAFreqAMS1Source(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS1Source value", CSTGParamsOwner::sValueGetterTemp.value, -122L);
	s->GetFilterAFreqAMS2Intensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS2Intensity value", CSTGParamsOwner::sValueGetterTemp.value, 40092709L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS2Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 40092709L);
	s->GetFilterAFreqAMS2Source(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFreqAMS2Source value", CSTGParamsOwner::sValueGetterTemp.value, -95L);
	s->GetFilterAFrequency(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFrequency value", CSTGParamsOwner::sValueGetterTemp.value, 848557141L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFrequency displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 848557141L);
	s->GetFilterAFrequencyFine(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFrequencyFine value", CSTGParamsOwner::sValueGetterTemp.value, -1374719791L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAFrequencyFine displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1374719791L);
	s->GetFilterAHighpass1(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAHighpass1 value", CSTGParamsOwner::sValueGetterTemp.value, 191679790L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAHighpass1 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 191679790L);
	s->GetFilterAHighpass2(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAHighpass2 value", CSTGParamsOwner::sValueGetterTemp.value, -77808354L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAHighpass2 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -77808354L);
	s->GetFilterAKeytrackIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAKeytrackIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 309580853L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAKeytrackIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 309580853L);
	s->GetFilterALFOAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterALFOAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1404310902L);
	check_eq("CSTGAnalog4PoleBase::GetFilterALFOAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1404310902L);
	s->GetFilterALFOAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterALFOAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -14L);
	s->GetFilterALFOIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterALFOIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 444324925L);
	check_eq("CSTGAnalog4PoleBase::GetFilterALFOIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 444324925L);
	s->GetFilterALFOJSminusYIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterALFOJSminusYIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1762174791L);
	check_eq("CSTGAnalog4PoleBase::GetFilterALFOJSminusYIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1762174791L);
	s->GetFilterALFOSelect(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterALFOSelect value", CSTGParamsOwner::sValueGetterTemp.value, 80L);
	s->GetFilterALowpass1(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterALowpass1 value", CSTGParamsOwner::sValueGetterTemp.value, -1880075854L);
	check_eq("CSTGAnalog4PoleBase::GetFilterALowpass1 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1880075854L);
	s->GetFilterALowpass2(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterALowpass2 value", CSTGParamsOwner::sValueGetterTemp.value, 2145403298L);
	check_eq("CSTGAnalog4PoleBase::GetFilterALowpass2 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2145403298L);
	s->GetFilterAOutputLevel(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAOutputLevel value", CSTGParamsOwner::sValueGetterTemp.value, -364139507L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAOutputLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -364139507L);
	s->GetFilterAOutputLevelAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAOutputLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1724328073L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAOutputLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1724328073L);
	s->GetFilterAOutputLevelAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAOutputLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 5L);
	s->GetFilterAResonance(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAResonance value", CSTGParamsOwner::sValueGetterTemp.value, 713813069L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAResonance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 713813069L);
	s->GetFilterAResonanceAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAResonanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 949615195L);
	check_eq("CSTGAnalog4PoleBase::GetFilterAResonanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 949615195L);
	s->GetFilterAResonanceAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterAResonanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -41L);
	s->GetFilterATrim(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterATrim value", CSTGParamsOwner::sValueGetterTemp.value, 1859072145L);
	check_eq("CSTGAnalog4PoleBase::GetFilterATrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1859072145L);
	s->GetFilterATypeXfade(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterATypeXfade value", CSTGParamsOwner::sValueGetterTemp.value, -583098624L);
	check_eq("CSTGAnalog4PoleBase::GetFilterATypeXfade displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -583098624L);
	s->GetFilterATypeXfadeAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterATypeXfadeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1505368956L);
	check_eq("CSTGAnalog4PoleBase::GetFilterATypeXfadeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1505368956L);
	s->GetFilterATypeXfadeAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterATypeXfadeAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1960130199L);
	check_eq("CSTGAnalog4PoleBase::GetFilterATypeXfadeAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1960130199L);
	s->GetFilterATypeXfadeAMSIntensityAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterATypeXfadeAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 19L);
	s->GetFilterATypeXfadeAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterATypeXfadeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -8L);
	s->GetFilterB4PoleResType(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterB4PoleResType value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetFilterBBypass(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBBypass value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetFilterBEGAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBEGAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 157993772L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBEGAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 157993772L);
	s->GetFilterBEGAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBEGAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -88L);
	s->GetFilterBEGIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBEGIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1606427010L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBEGIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1606427010L);
	s->GetFilterBEGSelect(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBEGSelect value", CSTGParamsOwner::sValueGetterTemp.value, 238L);
	s->GetFilterBEGVelocity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBEGVelocity value", CSTGParamsOwner::sValueGetterTemp.value, -616784386L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBEGVelocity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -616784386L);
	s->GetFilterBFilterType(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFilterType value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetFilterBFreqAMS1Intensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS1Intensity value", CSTGParamsOwner::sValueGetterTemp.value, 612755015L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS1Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 612755015L);
	s->GetFilterBFreqAMS1IntensityAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS1IntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1522211965L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS1IntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1522211965L);
	s->GetFilterBFreqAMS1IntensityAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS1IntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -7L);
	s->GetFilterBFreqAMS1Source(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS1Source value", CSTGParamsOwner::sValueGetterTemp.value, -61L);
	s->GetFilterBFreqAMS2Intensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS2Intensity value", CSTGParamsOwner::sValueGetterTemp.value, 1067450722L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS2Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1067450722L);
	s->GetFilterBFreqAMS2Source(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFreqAMS2Source value", CSTGParamsOwner::sValueGetterTemp.value, -34L);
	s->GetFilterBFrequency(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFrequency value", CSTGParamsOwner::sValueGetterTemp.value, 1875915154L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFrequency displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1875915154L);
	s->GetFilterBFrequencyFine(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFrequencyFine value", CSTGParamsOwner::sValueGetterTemp.value, -347296498L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBFrequencyFine displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -347296498L);
	s->GetFilterBKeytrackIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBKeytrackIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1336938866L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBKeytrackIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1336938866L);
	s->GetFilterBLFOAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLFOAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1863232845L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLFOAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1863232845L);
	s->GetFilterBLFOAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLFOAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 47L);
	s->GetFilterBLFOIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLFOIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1471682938L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLFOIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1471682938L);
	s->GetFilterBLFOJSminusYIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLFOJSminusYIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -751528458L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLFOJSminusYIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -751528458L);
	s->GetFilterBLFOSelect(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLFOSelect value", CSTGParamsOwner::sValueGetterTemp.value, 141L);
	s->GetFilterBLink(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLink value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetFilterBLinkCutoffOffset(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLinkCutoffOffset value", CSTGParamsOwner::sValueGetterTemp.value, 1572740992L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBLinkCutoffOffset displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1572740992L);
	s->GetFilterBOutputLevel(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBOutputLevel value", CSTGParamsOwner::sValueGetterTemp.value, 663284042L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBOutputLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 663284042L);
	s->GetFilterBOutputLevelAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBOutputLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1559992890L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBOutputLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1559992890L);
	s->GetFilterBOutputLevelAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBOutputLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 66L);
	s->GetFilterBResonance(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBResonance value", CSTGParamsOwner::sValueGetterTemp.value, 1741171082L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBResonance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1741171082L);
	s->GetFilterBResonanceAMSIntensity(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBResonanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1976973208L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBResonanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1976973208L);
	s->GetFilterBResonanceAMSSource(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBResonanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 20L);
	s->GetFilterBTrim(ctx);
	check_eq("CSTGAnalog4PoleBase::GetFilterBTrim value", CSTGParamsOwner::sValueGetterTemp.value, -1425248818L);
	check_eq("CSTGAnalog4PoleBase::GetFilterBTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1425248818L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
