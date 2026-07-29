/*
 * test_stg_multiband_delay.cpp  -  host-side known-answer test for
 * CSTGMultibandDelay's 60 real methods landed in round 54 (solo,
 * 2026-07-29). See include/oa_stg_multiband_delay.h for the full
 * derivation.
 */
#include <cstdio>
#include <cstring>
#include "oa_stg_multiband_delay.h"

extern "C" unsigned char STGMultibandDelayParams[4] = {0};
extern "C" unsigned char sMessageHandlers[0x20] = {0};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Opaque CSTGEffectMessageContext, only ever offset-accessed at +0x18
 * (see header comment) -- a raw fake laid out to match. */
struct FakeEffectCtx {
	unsigned char pad[0x18];
	unsigned char *dataPtr;
};

static STGConvertedParam MakeParam(int rawBits)
{
	STGConvertedParam p;
	memset(&p, 0, sizeof(p));
	p.value = rawBits;
	return p;
}

static int FloatBits(float f)
{
	int bits;
	memcpy(&bits, &f, sizeof(bits));
	return bits;
}

int main()
{
	unsigned char data[0x2d0];
	memset(data, 0xcc, sizeof(data));
	FakeEffectCtx fake;
	fake.dataPtr = data;
	CSTGEffectMessageContext &ctx = *reinterpret_cast<CSTGEffectMessageContext *>(&fake);

	/* [1] framework accessors */
	check("GetId() == 0x56", CSTGMultibandDelay::GetId() == 0x56);
	check("GetName() == \"Multiband Mod. Delay\"", strcmp(CSTGMultibandDelay::GetName(), "Multiband Mod. Delay") == 0);
	check("GetNumParams() == 0x44", CSTGMultibandDelay::GetNumParams() == 0x44);
	check("GetParamDescriptors() == STGMultibandDelayParams", CSTGMultibandDelay::GetParamDescriptors() == STGMultibandDelayParams);
	check("GetMessageHandlers() == sMessageHandlers + 0xc", CSTGMultibandDelay::GetMessageHandlers() == sMessageHandlers + 0xc);

	/* [2] dtor: reaches here without crashing */
	{
		CSTGMultibandDelay mbd;
		mbd.~CSTGMultibandDelay();
	}
	check("dtor: no-op beyond vptr reset, reached here", true);

	/* [3] plain u32-copy family (Feedback, stride 4) -- band1 hardcoded + generic */
	{
		STGConvertedParam v = MakeParam(0x12345678);
		CSTGMultibandDelay::UpdateBand1Feedback(ctx, v);
		check("UpdateBand1Feedback writes +0x228", *reinterpret_cast<unsigned int *>(data + 0x228) == 0x12345678u);
		CSTGMultibandDelay::UpdateBand3Feedback(ctx, v);
		check("UpdateBand3Feedback writes +0x230", *reinterpret_cast<unsigned int *>(data + 0x230) == 0x12345678u);
		STGConvertedParam v2 = MakeParam(0xdeadbeef);
		CSTGMultibandDelay::UpdateBandFeedback(ctx, v2, 2);
		check("UpdateBandFeedback(band=2) writes +0x228+2*4=0x230", *reinterpret_cast<unsigned int *>(data + 0x230) == 0xdeadbeefu);
	}

	/* [4] plain u32-copy family, stride 8 (FeedbackDModIntensity) */
	{
		STGConvertedParam v = MakeParam(0x11);
		CSTGMultibandDelay::UpdateBand1FeedbackDModIntensity(ctx, v);
		check("UpdateBand1FeedbackDModIntensity writes +0x294", *reinterpret_cast<unsigned int *>(data + 0x294) == 0x11u);
		STGConvertedParam v2 = MakeParam(0x22);
		CSTGMultibandDelay::UpdateBandFeedbackDModIntensity(ctx, v2, 3);
		check("UpdateBandFeedbackDModIntensity(band=3) writes +0x294+3*8=0x2ac", *reinterpret_cast<unsigned int *>(data + 0x2ac) == 0x22u);
	}

	/* [5] boolneg family (InputSource): raw==0 -> 0xffffffff, else 0 */
	{
		STGConvertedParam vZero = MakeParam(0);
		CSTGMultibandDelay::UpdateBand1InputSource(ctx, vZero);
		check("UpdateBand1InputSource(0) -> 0xffffffff at +0xf0", *reinterpret_cast<unsigned int *>(data + 0xf0) == 0xffffffffu);
		STGConvertedParam vNonZero = MakeParam(5);
		CSTGMultibandDelay::UpdateBand2InputSource(ctx, vNonZero);
		check("UpdateBand2InputSource(5) -> 0 at +0xf4", *reinterpret_cast<unsigned int *>(data + 0xf4) == 0u);
		CSTGMultibandDelay::UpdateBandInputSource(ctx, vZero, 3);
		check("UpdateBandInputSource(0, band=3) -> 0xffffffff at +0xf0+3*4=0xfc", *reinterpret_cast<unsigned int *>(data + 0xfc) == 0xffffffffu);
	}

	/* [6] onemin family (HighDamping): float 1.0f - raw */
	{
		STGConvertedParam v = MakeParam(FloatBits(0.3f));
		CSTGMultibandDelay::UpdateBand1HighDamping(ctx, v);
		float result = *reinterpret_cast<float *>(data + 0x1a0);
		check("UpdateBand1HighDamping(0.3) -> ~0.7 at +0x1a0", result > 0.699f && result < 0.701f);
		CSTGMultibandDelay::UpdateBandHighDamping(ctx, v, 1);
		float result2 = *reinterpret_cast<float *>(data + 0x1a4);
		check("UpdateBandHighDamping(0.3, band=1) -> ~0.7 at +0x1a4", result2 > 0.699f && result2 < 0.701f);
	}

	/* [7] piecewise family (FeedbackSource): 4-way branch, band1 + generic */
	{
		STGConvertedParam v0 = MakeParam(0);
		CSTGMultibandDelay::UpdateBand1FeedbackSource(ctx, v0);
		check("UpdateBand1FeedbackSource(0): +0x100==-1, +0x110==0",
		      *reinterpret_cast<unsigned int *>(data + 0x100) == 0xffffffffu &&
		      *reinterpret_cast<unsigned int *>(data + 0x110) == 0u);

		STGConvertedParam v1 = MakeParam(1);
		CSTGMultibandDelay::UpdateBand1FeedbackSource(ctx, v1);
		check("UpdateBand1FeedbackSource(1): +0x100==0, +0x110==-1, +0x120==0, +0x130==0",
		      *reinterpret_cast<unsigned int *>(data + 0x100) == 0u &&
		      *reinterpret_cast<unsigned int *>(data + 0x110) == 0xffffffffu &&
		      *reinterpret_cast<unsigned int *>(data + 0x120) == 0u &&
		      *reinterpret_cast<unsigned int *>(data + 0x130) == 0u);

		STGConvertedParam v3 = MakeParam(3);
		CSTGMultibandDelay::UpdateBand1FeedbackSource(ctx, v3);
		check("UpdateBand1FeedbackSource(3): default path, +0x130==0xffffffff (source==3)",
		      *reinterpret_cast<unsigned int *>(data + 0x130) == 0xffffffffu);

		STGConvertedParam v99 = MakeParam(99);
		CSTGMultibandDelay::UpdateBand1FeedbackSource(ctx, v99);
		check("UpdateBand1FeedbackSource(99): default path, +0x130==0 (source!=3)",
		      *reinterpret_cast<unsigned int *>(data + 0x130) == 0u);

		CSTGMultibandDelay::UpdateBandFeedbackSource(ctx, v0, 2);
		check("UpdateBandFeedbackSource(0, band=2): +0x108==-1, +0x118==0 (0x100+2*4, 0x110+2*4)",
		      *reinterpret_cast<unsigned int *>(data + 0x108) == 0xffffffffu &&
		      *reinterpret_cast<unsigned int *>(data + 0x118) == 0u);
	}

	/* [8] non-banded singles */
	{
		STGConvertedParam v = MakeParam(0x77);
		CSTGMultibandDelay::UpdateInputTrimDModSource(ctx, v);
		check("UpdateInputTrimDModSource writes +0x288", *reinterpret_cast<unsigned int *>(data + 0x288) == 0x77u);
		CSTGMultibandDelay::UpdateInputTrimDModIntensity(ctx, v);
		check("UpdateInputTrimDModIntensity writes +0x28c", *reinterpret_cast<unsigned int *>(data + 0x28c) == 0x77u);
	}

	/* [9] broadcast-to-4-fixed-offsets singles */
	{
		STGConvertedParam v = MakeParam(0x99);
		CSTGMultibandDelay::UpdateFeedbackDModSource(ctx, v);
		check("UpdateFeedbackDModSource broadcasts to +0x290/298/2a0/2a8",
		      *reinterpret_cast<unsigned int *>(data + 0x290) == 0x99u &&
		      *reinterpret_cast<unsigned int *>(data + 0x298) == 0x99u &&
		      *reinterpret_cast<unsigned int *>(data + 0x2a0) == 0x99u &&
		      *reinterpret_cast<unsigned int *>(data + 0x2a8) == 0x99u);

		STGConvertedParam v2 = MakeParam(0xaa);
		CSTGMultibandDelay::UpdateLevelDModSource(ctx, v2);
		check("UpdateLevelDModSource broadcasts to +0x2b0/2b8/2c0/2c8",
		      *reinterpret_cast<unsigned int *>(data + 0x2b0) == 0xaau &&
		      *reinterpret_cast<unsigned int *>(data + 0x2b8) == 0xaau &&
		      *reinterpret_cast<unsigned int *>(data + 0x2c0) == 0xaau &&
		      *reinterpret_cast<unsigned int *>(data + 0x2c8) == 0xaau);
	}

	/* [10] spot-check the remaining plain-copy families (Level, Pan, LFOType, LFOFreq) */
	{
		STGConvertedParam v = MakeParam(0x42);
		CSTGMultibandDelay::UpdateBand4Level(ctx, v);
		check("UpdateBand4Level writes +0x268+3*4=0x274", *reinterpret_cast<unsigned int *>(data + 0x274) == 0x42u);
		CSTGMultibandDelay::UpdateBand4Pan(ctx, v);
		check("UpdateBand4Pan writes +0x278+3*4=0x284", *reinterpret_cast<unsigned int *>(data + 0x284) == 0x42u);
		CSTGMultibandDelay::UpdateBand4LFOType(ctx, v);
		check("UpdateBand4LFOType writes +0x238+3*4=0x244", *reinterpret_cast<unsigned int *>(data + 0x244) == 0x42u);
		CSTGMultibandDelay::UpdateBand4LFOFreq(ctx, v);
		check("UpdateBand4LFOFreq writes +0x258+3*4=0x264", *reinterpret_cast<unsigned int *>(data + 0x264) == 0x42u);
		CSTGMultibandDelay::UpdateBandLevelDModIntensity(ctx, v, 0);
		check("UpdateBandLevelDModIntensity(band=0) writes +0x2b4", *reinterpret_cast<unsigned int *>(data + 0x2b4) == 0x42u);
	}

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
