// SPDX-License-Identifier: GPL-2.0
/*
 * test_param_convertor.cpp  -  host-side known-answer test for
 * USTGParamConvertor (batch 55, src/engine/param_convertor.cpp +
 * am_exp2_ess.cpp).
 *
 * The 23 pure Taper/InverseTaper sample values below are taken
 * DIRECTLY from the ground-truth harness output (real OA.ko machine
 * code executed natively via a throwaway `gcc -m32` relocation-patch
 * harness, not derived from this same reconstruction) -- a handful of
 * spot checks per function spanning at least one full segment and one
 * breakpoint, not exhaustive re-verification of every derived formula.
 */

#include <cstdio>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include "oa_global.h"
#include "oa_engine.h"

/* Linking src/engine/audio_input_mixer.cpp in (needed only for the
 * real CSTGPan::CalculateMonoPanCoeffs that IntToFloatPanConvertor
 * calls) drags in that TU's own static storage requirements -- same
 * per-test-file mock precedent as test_audio_input_mixer.cpp itself
 * (their own header comment explains the rationale for each). None of
 * these are ever touched by anything this test actually exercises. */
unsigned char CSTGPerformanceVarsManager::sInstance[12];
unsigned char CSTGAudioBusManager::sGlobalBusSet[34 * 0x80];
unsigned char CSTGAudioBusManager::sEffectThreadBusSets[240 * 0x80];
unsigned char CSTGAudioBusManager::sSynthesisThreadBusSets[960 * 0x80];
CSTGControllerRTData *CSTGControllerRTData::sInstance;

static int g_fail;

static void check_near(const char *label, float got, float want, float eps = 0.01f)
{
	bool ok = fabsf(got - want) <= eps;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s %.6g\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %.6g, diff %.6g)\n", want, got - want);
}

static void check_eq(const char *label, int got, int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s %d\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %d)\n", want);
}

static void check_true(const char *label, bool got)
{
	if (!got)
		g_fail++;
	printf("  %s  %s\n", got ? "ok  " : "FAIL", label);
}

using U = USTGParamConvertor;

int main(void)
{
	printf("USTGParamConvertor known-answer test\n");
	printf("=====================================\n");

	printf("[1] Taper curve family (ground-truth-harness-derived samples)\n");
	check_near("PiecewiseLinear1Taper(0)", U::PiecewiseLinear1Taper(0.0f), 1.0f);
	check_near("PiecewiseLinear1Taper(91)", U::PiecewiseLinear1Taper(91.0f), 0.0909090936f);
	check_near("PiecewiseLinear1Taper(131)", U::PiecewiseLinear1Taper(131.0f), 0.0f);
	check_near("EffectLFOFreq1Taper(50)", U::EffectLFOFreq1Taper(50.0f), 1.0f);
	check_near("EffectLFOFreq1Taper(210)", U::EffectLFOFreq1Taper(210.0f), 10.0f);
	check_near("EffectLFOFreq1InverseTaper(10)", U::EffectLFOFreq1InverseTaper(10.0f), 210.0f);
	check_near("EffectLFOFreqDModIntensity1Taper(-1000)", U::EffectLFOFreqDModIntensity1Taper(-1000.0f), -905.0f, 1.0f);
	check_near("EffectLFOFreqDModIntensity1Taper(60)", U::EffectLFOFreqDModIntensity1Taper(60.0f), 3.0f);
	check_near("EffectLFOFreqDModIntensity1InverseTaper(-1000)", U::EffectLFOFreqDModIntensity1InverseTaper(-1000.0f), 25000.0f);
	check_near("EffectLFOFreq2Taper(200)", U::EffectLFOFreq2Taper(200.0f), 50.0f);
	check_near("EffectLFOFreqDModIntensity2Taper(-1000)", U::EffectLFOFreqDModIntensity2Taper(-1000.0f), -1850.0f, 1.0f);
	check_near("EffectDelayTime1Taper(1000)", U::EffectDelayTime1Taper(1000.0f), 910.0f);
	check_near("EffectDelayTime2Taper(80)", U::EffectDelayTime2Taper(80.0f), 15.0f);
	check_near("EffectDelayTime3Taper(105)", U::EffectDelayTime3Taper(105.0f), 150.0f);
	check_near("EffectDelayModDepth1Taper(85)", U::EffectDelayModDepth1Taper(85.0f), 8.57440472f);
	check_near("EffectDelayModDepth1Taper(1000)", U::EffectDelayModDepth1Taper(1000.0f), 1500.0f, 0.5f);
	check_near("EffectFixedFreq1Taper(-1000)", U::EffectFixedFreq1Taper(-1000.0f), -89200.0f, 5.0f);
	check_near("EffectFixedFreqDModIntensity1Taper(1000)", U::EffectFixedFreqDModIntensity1Taper(1000.0f), 189200.0f, 5.0f);
	check_near("EffectEQFreq1Taper(-1000)", U::EffectEQFreq1Taper(-1000.0f), -20000.0f, 1.0f);
	check_near("WaveSeqDuration1Taper(160)", U::WaveSeqDuration1Taper(160.0f), 10100.0f);
	check_near("InverseWaveSeqDuration1Taper(72000)", U::InverseWaveSeqDuration1Taper(72000.0f), 146.0f);
	check_near("TaperKeyOnDelay(97)", U::TaperKeyOnDelay(97.0f), 5001.0f);
	check_near("InverseTaperKeyOnDelay(72000)", U::InverseTaperKeyOnDelay(72000.0f), 97.0f);
	check_near("VectorDurationTaper(1000)", U::VectorDurationTaper(1000.0f), 865000.0f, 5.0f);
	check_near("VectorDurationInverseTaper(72000)", U::VectorDurationInverseTaper(72000.0f), 207.0f);
	check_near("PitchSemitoneTaper(-1000)", U::PitchSemitoneTaper(-1000.0f), -897.0f, 1.0f);
	check_near("InversePitchSemitoneTaper(1000)", U::InversePitchSemitoneTaper(1000.0f), 1103.0f, 1.0f);
	check_near("EffectCompAttackTime1Taper(210)", U::EffectCompAttackTime1Taper(210.0f), 200.0f);
	check_near("EffectCompReleaseTime1Taper(200)", U::EffectCompReleaseTime1Taper(200.0f), 200.0f);
	check_near("EffectCompReleaseTime1Taper(210)", U::EffectCompReleaseTime1Taper(210.0f), 300.0f);
	check_near("TaperEQMidFreq(0)", U::TaperEQMidFreq(0.0f), 100.0f);
	check_near("TaperEQMidFreq(-1000)", U::TaperEQMidFreq(-1000.0f), -9900.0f);
	check_near("InverseTaperEQMidFreq(100)", U::InverseTaperEQMidFreq(100.0f), 0.0f);
	check_near("TaperHeadroom(0)", U::TaperHeadroom(0.0f), 1.0f);
	check_near("TaperHeadroom(1)", U::TaperHeadroom(1.0f), 4.0f);
	check_near("TaperHeadroom(3)", U::TaperHeadroom(3.0f), 64.0f);
	check_near("TaperHeadroom(5)", U::TaperHeadroom(5.0f), 1.0f);
	check_near("TaperHeadroom(-10)", U::TaperHeadroom(-10.0f), 1.0f);
	check_near("InverseTaperSmoothingFactor(1)", U::InverseTaperSmoothingFactor(1.0f), 0.0f);
	check_near("InverseTaperSmoothingFactor(0.01)", U::InverseTaperSmoothingFactor(0.01f), 80.5316f, 0.01f);
	check_near("SeamlessHoldTimeTaper(3)", U::SeamlessHoldTimeTaper(3.0f), 2.0f);
	check_near("SeamlessHoldTimeTaper(17)", U::SeamlessHoldTimeTaper(17.0f), 25.0f);

	printf("[2] am_exp2_ess / TaperSmoothingFactor round-trip against its own inverse\n");
	for (float x : {0.0f, 25.0f, 50.0f, 75.0f, 100.0f}) {
		float smoothed = U::TaperSmoothingFactor(x);
		float back = U::InverseTaperSmoothingFactor(smoothed);
		char label[64];
		snprintf(label, sizeof(label), "InverseTaperSmoothingFactor(TaperSmoothingFactor(%.0f))", x);
		check_near(label, back, x, 0.5f);
	}
	check_near("am_exp2_ess(0)", am_exp2_ess(0.0f), 1.0f, 1e-4f);
	check_near("am_exp2_ess(1)", am_exp2_ess(1.0f), 2.0f, 1e-3f);
	check_near("am_exp2_ess(-1)", am_exp2_ess(-1.0f), 0.5f, 1e-3f);
	check_near("am_exp2_ess(3)", am_exp2_ess(3.0f), 8.0f, 1e-2f);

	printf("[3] GetTaperValue/GetInverseTaperValue dispatch through the real tables\n");
	check_near("GetTaperValue(NoTaper)", U::GetTaperValue(42.0f, eSTGTaper_NoTaper), 42.0f);
	check_near("GetTaperValue(PitchSemitone)", U::GetTaperValue(1000.0f, eSTGTaper_PitchSemitone), 897.0f, 1.0f);
	check_near("GetInverseTaperValue(SmoothingFactor)", U::GetInverseTaperValue(1.0f, eSTGTaper_SmoothingFactor), 0.0f);
	check_near("GetInverseTaperValue(EffectLFOFreq2, no inverse)", U::GetInverseTaperValue(5.0f, eSTGTaper_EffectLFOFreq2), 0.0f);

	printf("[4] Convertor family\n");
	{
		CSTGParamDescriptor desc;
		STGConvertedParam out;
		std::memset(&desc, 0, sizeof(desc));
		std::memset(&out, 0, sizeof(out));

		bool ok = U::IntToPercentConvertor(200.0f, desc, out);
		check_true("IntToPercentConvertor returns true", ok);
		float v; std::memcpy(&v, &out.value, 4);
		check_near("IntToPercentConvertor(200) == 2.0", v, 2.0f);
		float dv; std::memcpy(&dv, &out.displayValue, 4);
		check_near("IntToPercentConvertor mirrors to displayValue", dv, 2.0f);

		std::memset(&out, 0, sizeof(out));
		U::IntToTempoConvertor(3000.0f, desc, out);
		std::memcpy(&v, &out.value, 4);
		check_near("IntToTempoConvertor(3000) clamps to 0 below 4000", v, 0.0f);
		U::IntToTempoConvertor(5000.0f, desc, out);
		std::memcpy(&v, &out.value, 4);
		check_near("IntToTempoConvertor(5000) == 50.0", v, 50.0f);

		desc.rawMin = 0; desc.rawMax = 100;
		desc.floatMin = 0.0f; desc.floatMax = 1.0f;
		std::memset(&out, 0, sizeof(out));
		U::IntToFloatConvertor(50.0f, desc, out);
		std::memcpy(&v, &out.value, 4);
		check_near("IntToFloatConvertor midpoint", v, 0.5f);

		std::memset(&out, 0, sizeof(out));
		U::IntCountFromZeroConvertor(30.0f, desc, out);
		check_eq("IntCountFromZeroConvertor(30) - rawMin(0)", out.value, 30);
		int back = U::IntCountFromZeroUnConvertor(desc, out);
		check_eq("IntCountFromZeroUnConvertor round-trip", back, 30);

		std::memset(&out, 0, sizeof(out));
		U::IntToFloatPanRandomConvertor(0.0f, desc, out);
		std::memcpy(&v, &out.value, 4);
		check_near("IntToFloatPanRandomConvertor(0) sentinel", v, -2.0f);
	}

	printf("[5] ConvertParam/UnConvertParam dispatchers (clamp + table indirection)\n");
	{
		CSTGParamDescriptor desc;
		std::memset(&desc, 0, sizeof(desc));
		desc.rawMin = 0;
		desc.rawMax = 100;
		desc.floatMin = 0.0f;
		desc.floatMax = 1.0f;
		desc.convertType = 1;   /* IntToFloatConvertor */
		desc.taperType = 0;     /* NoTaper */

		STGConvertedParam out;
		std::memset(&out, 0, sizeof(out));
		bool ok = U::ConvertParam(150 /* above rawMax */, desc, out);
		check_true("ConvertParam returns true", ok);
		check_eq("ConvertParam clamps rawValue to rawMax", out.clampedRaw, 100);
		float v; std::memcpy(&v, &out.value, 4);
		check_near("ConvertParam(150, clamped to 100) -> 1.0", v, 1.0f);

		std::memcpy(&out.value, &v, 4);
		int back = U::UnConvertParam(out, desc);
		check_eq("UnConvertParam round-trip", back, 100);
	}

	printf("\n%d failure(s)\n", g_fail);
	return g_fail ? 1 : 0;
}
