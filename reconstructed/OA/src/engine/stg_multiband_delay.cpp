// SPDX-License-Identifier: GPL-2.0
/*
 * stg_multiband_delay.cpp  -  CSTGMultibandDelay method bodies (round
 * 54, solo). See include/oa_stg_multiband_delay.h for the full
 * object-layout derivation and the deliberately-deferred methods' 2
 * distinct reasons.
 */
#include "oa_stg_multiband_delay.h"

extern "C" unsigned char STGMultibandDelayParams[];
extern "C" unsigned char sMessageHandlers[];

CSTGMultibandDelay::~CSTGMultibandDelay()
{
	/* Real dtor (both D2/D0 variants, identical): resets vptr to
	 * &PTR__CSTGParamsOwner_006c04a8 -- see header comment. Same
	 * "opaque placeholder, no real vtable pointer needed" treatment
	 * as CSTGKeyTrack::~CSTGKeyTrack()/CSTGPatch::~CSTGPatch(). */
	mVptrPlaceholder[0] = mVptrPlaceholder[1] = mVptrPlaceholder[2] = mVptrPlaceholder[3] = 0;
}

unsigned int CSTGMultibandDelay::GetId() { return 0x56; }
const char *CSTGMultibandDelay::GetName() { return "Multiband Mod. Delay"; }
unsigned int CSTGMultibandDelay::GetNumParams() { return 0x44; }
const void *CSTGMultibandDelay::GetParamDescriptors() { return STGMultibandDelayParams; }
/* Real body: `return sMessageHandlers + 0xc` -- this class's own entries
 * start 0xc bytes into the shared table (see header comment), unlike
 * CSTGKeyTrack's own unadorned `return sMessageHandlers`. */
const void *CSTGMultibandDelay::GetMessageHandlers() { return sMessageHandlers + 0xc; }

void CSTGMultibandDelay::UpdateBand1Feedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x228) = value;
}


void CSTGMultibandDelay::UpdateBand2Feedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x22c) = value;
}


void CSTGMultibandDelay::UpdateBand3Feedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x230) = value;
}


void CSTGMultibandDelay::UpdateBand4Feedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x234) = value;
}


void CSTGMultibandDelay::UpdateBandFeedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x228 + band * 4) = value;
}


void CSTGMultibandDelay::UpdateBand1FeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x294) = value;
}


void CSTGMultibandDelay::UpdateBand2FeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x29c) = value;
}


void CSTGMultibandDelay::UpdateBand3FeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x2a4) = value;
}


void CSTGMultibandDelay::UpdateBand4FeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x2ac) = value;
}


void CSTGMultibandDelay::UpdateBandFeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x294 + band * 8) = value;
}


void CSTGMultibandDelay::UpdateBand1LFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x238) = value;
}


void CSTGMultibandDelay::UpdateBand2LFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x23c) = value;
}


void CSTGMultibandDelay::UpdateBand3LFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x240) = value;
}


void CSTGMultibandDelay::UpdateBand4LFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x244) = value;
}


void CSTGMultibandDelay::UpdateBandLFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x238 + band * 4) = value;
}


void CSTGMultibandDelay::UpdateBand1LFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x258) = value;
}


void CSTGMultibandDelay::UpdateBand2LFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x25c) = value;
}


void CSTGMultibandDelay::UpdateBand3LFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x260) = value;
}


void CSTGMultibandDelay::UpdateBand4LFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x264) = value;
}


void CSTGMultibandDelay::UpdateBandLFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x258 + band * 4) = value;
}


void CSTGMultibandDelay::UpdateBand1Level(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x268) = value;
}


void CSTGMultibandDelay::UpdateBand2Level(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x26c) = value;
}


void CSTGMultibandDelay::UpdateBand3Level(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x270) = value;
}


void CSTGMultibandDelay::UpdateBand4Level(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x274) = value;
}


void CSTGMultibandDelay::UpdateBandLevel(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x268 + band * 4) = value;
}


void CSTGMultibandDelay::UpdateBand1LevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x2b4) = value;
}


void CSTGMultibandDelay::UpdateBand2LevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x2bc) = value;
}


void CSTGMultibandDelay::UpdateBand3LevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x2c4) = value;
}


void CSTGMultibandDelay::UpdateBand4LevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x2cc) = value;
}


void CSTGMultibandDelay::UpdateBandLevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x2b4 + band * 8) = value;
}


void CSTGMultibandDelay::UpdateBand1Pan(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x278) = value;
}


void CSTGMultibandDelay::UpdateBand2Pan(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x27c) = value;
}


void CSTGMultibandDelay::UpdateBand3Pan(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x280) = value;
}


void CSTGMultibandDelay::UpdateBand4Pan(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x284) = value;
}


void CSTGMultibandDelay::UpdateBandPan(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x278 + band * 4) = value;
}


void CSTGMultibandDelay::UpdateBand1InputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	int raw = *reinterpret_cast<int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0xf0) = (unsigned int)-(raw == 0);
}


void CSTGMultibandDelay::UpdateBand2InputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	int raw = *reinterpret_cast<int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0xf4) = (unsigned int)-(raw == 0);
}


void CSTGMultibandDelay::UpdateBand3InputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	int raw = *reinterpret_cast<int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0xf8) = (unsigned int)-(raw == 0);
}


void CSTGMultibandDelay::UpdateBand4InputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	int raw = *reinterpret_cast<int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0xfc) = (unsigned int)-(raw == 0);
}


void CSTGMultibandDelay::UpdateBandInputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	int raw = *reinterpret_cast<int *>(&val);
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0xf0 + band * 4) = (unsigned int)-(raw == 0);
}


void CSTGMultibandDelay::UpdateBand1HighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	float raw = *reinterpret_cast<float *>(&val);
	*reinterpret_cast<float *>(EffectData(ctx) + 0x1a0) = 1.0f - raw;
}


void CSTGMultibandDelay::UpdateBand2HighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	float raw = *reinterpret_cast<float *>(&val);
	*reinterpret_cast<float *>(EffectData(ctx) + 0x1a4) = 1.0f - raw;
}


void CSTGMultibandDelay::UpdateBand3HighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	float raw = *reinterpret_cast<float *>(&val);
	*reinterpret_cast<float *>(EffectData(ctx) + 0x1a8) = 1.0f - raw;
}


void CSTGMultibandDelay::UpdateBand4HighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	float raw = *reinterpret_cast<float *>(&val);
	*reinterpret_cast<float *>(EffectData(ctx) + 0x1ac) = 1.0f - raw;
}


void CSTGMultibandDelay::UpdateBandHighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	float raw = *reinterpret_cast<float *>(&val);
	*reinterpret_cast<float *>(EffectData(ctx) + 0x1a0 + band * 4) = 1.0f - raw;
}


void CSTGMultibandDelay::UpdateBand1FeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	int source = *reinterpret_cast<int *>(&val);
	unsigned char *base = EffectData(ctx);
	if (source == 0) {
		*reinterpret_cast<unsigned int *>(base + 0x100) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x110) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x100) = 0;
	if (source == 1) {
		*reinterpret_cast<unsigned int *>(base + 0x110) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x120) = 0;
		*reinterpret_cast<unsigned int *>(base + 0x130) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x110) = 0;
	if (source == 2) {
		*reinterpret_cast<unsigned int *>(base + 0x120) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x130) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x120) = 0;
	*reinterpret_cast<unsigned int *>(base + 0x130) = (unsigned int)((source != 3) - 1);
}


void CSTGMultibandDelay::UpdateBand2FeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	int source = *reinterpret_cast<int *>(&val);
	unsigned char *base = EffectData(ctx);
	if (source == 0) {
		*reinterpret_cast<unsigned int *>(base + 0x104) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x114) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x104) = 0;
	if (source == 1) {
		*reinterpret_cast<unsigned int *>(base + 0x114) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x124) = 0;
		*reinterpret_cast<unsigned int *>(base + 0x134) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x114) = 0;
	if (source == 2) {
		*reinterpret_cast<unsigned int *>(base + 0x124) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x134) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x124) = 0;
	*reinterpret_cast<unsigned int *>(base + 0x134) = (unsigned int)((source != 3) - 1);
}


void CSTGMultibandDelay::UpdateBand3FeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	int source = *reinterpret_cast<int *>(&val);
	unsigned char *base = EffectData(ctx);
	if (source == 0) {
		*reinterpret_cast<unsigned int *>(base + 0x108) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x118) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x108) = 0;
	if (source == 1) {
		*reinterpret_cast<unsigned int *>(base + 0x118) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x128) = 0;
		*reinterpret_cast<unsigned int *>(base + 0x138) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x118) = 0;
	if (source == 2) {
		*reinterpret_cast<unsigned int *>(base + 0x128) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x138) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x128) = 0;
	*reinterpret_cast<unsigned int *>(base + 0x138) = (unsigned int)((source != 3) - 1);
}


void CSTGMultibandDelay::UpdateBand4FeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	int source = *reinterpret_cast<int *>(&val);
	unsigned char *base = EffectData(ctx);
	if (source == 0) {
		*reinterpret_cast<unsigned int *>(base + 0x10c) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x11c) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x10c) = 0;
	if (source == 1) {
		*reinterpret_cast<unsigned int *>(base + 0x11c) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x12c) = 0;
		*reinterpret_cast<unsigned int *>(base + 0x13c) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x11c) = 0;
	if (source == 2) {
		*reinterpret_cast<unsigned int *>(base + 0x12c) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x13c) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x12c) = 0;
	*reinterpret_cast<unsigned int *>(base + 0x13c) = (unsigned int)((source != 3) - 1);
}


void CSTGMultibandDelay::UpdateBandFeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band)
{
	int source = *reinterpret_cast<int *>(&val);
	unsigned char *base = EffectData(ctx) + band * 4;
	if (source == 0) {
		*reinterpret_cast<unsigned int *>(base + 0x100) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x110) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x100) = 0;
	if (source == 1) {
		*reinterpret_cast<unsigned int *>(base + 0x110) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x120) = 0;
		*reinterpret_cast<unsigned int *>(base + 0x130) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x110) = 0;
	if (source == 2) {
		*reinterpret_cast<unsigned int *>(base + 0x120) = 0xffffffffu;
		*reinterpret_cast<unsigned int *>(base + 0x130) = 0;
		return;
	}
	*reinterpret_cast<unsigned int *>(base + 0x120) = 0;
	*reinterpret_cast<unsigned int *>(base + 0x130) = (unsigned int)((source != 3) - 1);
}


void CSTGMultibandDelay::UpdateInputTrimDModSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x288) = *reinterpret_cast<unsigned int *>(&val);
}


void CSTGMultibandDelay::UpdateInputTrimDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	*reinterpret_cast<unsigned int *>(EffectData(ctx) + 0x28c) = *reinterpret_cast<unsigned int *>(&val);
}


void CSTGMultibandDelay::UpdateFeedbackDModSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	unsigned char *base = EffectData(ctx);
	*reinterpret_cast<unsigned int *>(base + 0x2a8) = value;
	*reinterpret_cast<unsigned int *>(base + 0x2a0) = value;
	*reinterpret_cast<unsigned int *>(base + 0x298) = value;
	*reinterpret_cast<unsigned int *>(base + 0x290) = value;
}


void CSTGMultibandDelay::UpdateLevelDModSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val)
{
	unsigned int value = *reinterpret_cast<unsigned int *>(&val);
	unsigned char *base = EffectData(ctx);
	*reinterpret_cast<unsigned int *>(base + 0x2c8) = value;
	*reinterpret_cast<unsigned int *>(base + 0x2c0) = value;
	*reinterpret_cast<unsigned int *>(base + 0x2b8) = value;
	*reinterpret_cast<unsigned int *>(base + 0x2b0) = value;
}
