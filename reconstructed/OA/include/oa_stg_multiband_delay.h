// SPDX-License-Identifier: GPL-2.0
/*
 * oa_stg_multiband_delay.h  -  CSTGMultibandDelay: the 4-band modulated
 * delay effect (GetName() == "Multiband Mod. Delay", GetId() == 0x56).
 *
 * FOUND 2026-07-29 (round 54, solo). 60/85 clean pending methods landed
 * this round (85/88 total pending methods are decompiler-clean; 3 are
 * `in_stack_`/`unaff_`-flagged: the ctor, `Init`, `Run` -- deferred
 * alongside). A striking real-world discovery drives almost the entire
 * landed set: EVERY `UpdateBandXxx`/`UpdateBand{1,2,3,4}Xxx` method's
 * own `this` register is NOT a real object pointer -- ALL real
 * per-instance effect state lives in `*(CSTGEffectMessageContext*+0x18)`
 * (the message context's OWN data sub-object, confirmed via
 * `in_EDX + 0x18` in every single landed method), and `this` instead
 * carries either nothing at all (the 4 hardcoded `UpdateBand{1,2,3,4}Xxx`
 * variants, which the compiler fully specialized per band with a
 * literal baked-in offset) or the runtime band index as a plain `int`
 * (the `UpdateBandXxx(ctx, val, band)` generic 4-arg overloads, which
 * compute `base + band*stride`) -- the SAME "this-smuggled extra
 * argument" idiom already established for CKGParamEdit (round 52), just
 * with a per-band index instead of a small enum/bool.
 *
 * `EffectData(ctx)` below is this round's own helper for the repeated
 * `*(unsigned char**)((char*)&ctx + 0x18)` dereference every landed
 * method performs -- the real `CSTGEffectMessageContext` layout itself
 * is NOT independently confirmed beyond this one confirmed field,
 * modeled as an opaque forward-declared class.
 *
 * Confirmed real per-band field layout (all offsets INTO the
 * `EffectData(ctx)` blob, NOT `this`; `stride` = the generic overload's
 * own `band*N` multiplier, confirmed identical across all 4 hardcoded
 * per-band siblings for every family below):
 *   +0x0f0  mInputSource[4]      stride 4   (bool-negate: -(raw==0))
 *   +0x100  mFeedbackSource[4]   stride 4   (4-way piecewise int-const
 *           write into a 0x10-byte-stride sub-block per band, own
 *           layout confirmed identical across all 4 bands)
 *   +0x180  mLFODepth[4]         stride 4   (DEFERRED -- see below)
 *   +0x190  mTime[4]             stride 4   (DEFERRED -- see below)
 *   +0x1a0  mHighDamping[4]      stride 4   (float: `1.0f - raw`)
 *   +0x1c0  mLowDamping[4]       stride 4   (DEFERRED -- see below)
 *   +0x228  mFeedback[4]         stride 4   (plain `unsigned int` copy)
 *   +0x238  mLFOType[4]          stride 4   (plain `unsigned int` copy)
 *   +0x248  mLFOPhase[4]         stride 4   (DEFERRED -- see below)
 *   +0x258  mLFOFreq[4]          stride 4   (plain `unsigned int` copy)
 *   +0x268  mLevel[4]            stride 4   (plain `unsigned int` copy)
 *   +0x278  mPan[4]              stride 4   (plain `unsigned int` copy)
 *   +0x288  mInputTrimDModSource            (single, no band index)
 *   +0x28c  mInputTrimDModIntensity         (single, no band index)
 *   +0x290/0x298/0x2a0/0x2a8  mFeedbackDModSource[4] (fixed descending
 *           offsets, broadcast-written identically by ONE non-banded
 *           `UpdateFeedbackDModSource`, own stride NOT 4 like every
 *           other family -- confirmed real quirk, not an error)
 *   +0x294  mFeedbackDModIntensity[4]  stride 8  (plain copy)
 *   +0x2b0/0x2b8/0x2c0/0x2c8  mLevelDModSource[4] (same broadcast
 *           shape as mFeedbackDModSource, own separate
 *           `UpdateLevelDModSource`)
 *   +0x2b4  mLevelDModIntensity[4]     stride 8  (plain copy)
 *   +0xd0/+0xd4/+0xe0/+0xe4     band1-2 crossover coeffs (DEFERRED)
 *   +0xb0/+0xb4/+0xc0/+0xc4     band2-3 crossover coeffs (DEFERRED)
 *   +0xd8/+0xdc/+0xe8/+0xec     band3-4 crossover coeffs (DEFERRED)
 *
 * `GetId`/`GetName`/`GetNumParams`/`GetParamDescriptors` -- the SAME
 * `CSTGParamsOwner` framework-metadata family already reconstructed for
 * CSTGGlobal/CSTGLFO/CSTGKeyTrack, all `static`-shaped trivial literal/
 * pointer returns. `GetMessageHandlers()` is the ODD one out: its own
 * real body returns `sMessageHandlers + 0xc` (the SAME shared handler
 * table CSTGKeyTrack's own `GetMessageHandlers()` returns unadorned --
 * this class's own entries evidently start 0xc bytes into that shared
 * table, a genuinely different but equally mechanical real behavior).
 *
 * `~CSTGMultibandDelay()` -- both D0/D2 variants byte-identical (same
 * `&PTR__CSTGParamsOwner_006c04a8` vptr reset), same "no-op beyond
 * vptr reset" quirk and D0/D2-dedup convention already established for
 * CSTGKeyTrack/CSTGPatch.
 *
 * === Deferred, 3 distinct reasons (25/88 methods) ===
 * (1) 3 methods flagged by the decompiler itself (`in_stack_`/
 *     `unaff_`) -- the real ctor (1267B), `Init` (129B), `Run` (1322B,
 *     almost certainly the actual per-sample DSP process function).
 * (2) 24 methods with fully concrete control flow but each reads one
 *     of 6 real, named-but-unrecovered `.rodata` float-literal symbols
 *     (`_DAT_006bbda0`/`_DAT_006bbda4`/`_DAT_006bbda8`/`_DAT_006bbdac`/
 *     `_DAT_006bbdb0..bc`/`_DAT_006bbdc0`) -- the SAME "missing-literal-
 *     value" deferral class already established for CSTGKeyTrack's own
 *     `ConvertIntRampToSlope`/`ConvertSlopeToIntRamp` (round 51):
 *     `UpdateBand{1,2,3,4}LowDamping`+generic (5, a 2-branch piecewise
 *     float remap), `UpdateBand{1,2,3,4}Time`+generic (5, an fp-round
 *     scaled by an unrecovered sample-rate-derived constant),
 *     `UpdateBand{1,2,3,4}LFOPhase`+generic (5), `UpdateBand{1,2,3,4}
 *     LFODepth`+generic (5), `UpdateCrossoverFreq{12,23,34}` (3, an
 *     `fptan`-based bilinear-transform coefficient calculation), and
 *     `CalculateCrossoverCoefficients` (1, the SAME calculation
 *     factored out as its own method -- confirmed via its own `this`
 *     register carrying a smuggled `float` argument, same idiom as the
 *     `UpdateBandXxx` family's smuggled `int band`, but this method's
 *     real 2nd parameter `CrossoverCoeffs*` type is itself unrecovered
 *     beyond the 3 raw `float` fields the body writes).
 */

#ifndef OA_STG_MULTIBAND_DELAY_H
#define OA_STG_MULTIBAND_DELAY_H

#include "oa_global.h"	/* STGConvertedParam */

class CSTGEffectMessageContext; /* opaque, only ever offset-accessed */

class CSTGMultibandDelay {
public:
	~CSTGMultibandDelay();

	static unsigned int GetId();
	static const char *GetName();
	static unsigned int GetNumParams();
	static const void *GetParamDescriptors();
	static const void *GetMessageHandlers();

	static void UpdateBand1Feedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2Feedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3Feedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4Feedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandFeedback(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1FeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2FeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3FeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4FeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandFeedbackDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1LFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2LFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3LFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4LFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandLFOType(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1LFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2LFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3LFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4LFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandLFOFreq(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1Level(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2Level(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3Level(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4Level(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandLevel(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1LevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2LevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3LevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4LevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandLevelDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1Pan(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2Pan(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3Pan(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4Pan(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandPan(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1InputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2InputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3InputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4InputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandInputSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1HighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2HighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3HighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4HighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandHighDamping(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateBand1FeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand2FeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand3FeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBand4FeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateBandFeedbackSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val, int band);
	static void UpdateInputTrimDModSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateInputTrimDModIntensity(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateFeedbackDModSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
	static void UpdateLevelDModSource(CSTGEffectMessageContext &ctx, STGConvertedParam &val);
private:
	unsigned char mVptrPlaceholder[4]; /* +0x00, see header comment */
};

/* Real per-instance data lives at *(EDX+0x18), NOT `this` -- see header
 * comment. `ctx` is passed by reference matching the real signature;
 * this helper reproduces the confirmed `*(int*)(ctx+0x18)` dereference. */
inline unsigned char *EffectData(CSTGEffectMessageContext &ctx)
{
	return *reinterpret_cast<unsigned char **>(reinterpret_cast<unsigned char *>(&ctx) + 0x18);
}

#endif /* OA_STG_MULTIBAND_DELAY_H */
