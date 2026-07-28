// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_AMS_MIXER_BASE_H
#define OA_STG_AMS_MIXER_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_ams_mixer_base.h  -  CSTGAMSMixerBase's value-getter family:
 * 17 of 19 real weak-symbol ctx-only candidates reconstructed, 2 outliers
 * excluded -- see include/oa_stg_string.h for the pilot class's full
 * derivation. CSTGAMSMixerBase is the STG AMS two-input mixer base --
 * source select A/B, amount A/B, mixer type, shape, gate parameters --
 * confirmed genuinely fresh via a word-boundary grep before starting, no
 * pre-existing struct or ctor anywhere in this project.
 *
 * Dialect: the simplest yet, matching CSTGEPModelPatch/CSTGPolysixModelPatch
 * -- every real candidate is a fixed-K field read directly off this, zero
 * ctx-dynamic-index methods at all despite the class name's own mixer
 * framing.
 *
 * 2 outliers, both a real fyl2x log2-style transform -- Get Attack and
 * Get Decay -- same outlier class as CSTGString's own Get Noise Saturation
 * and CSTGAnalogSyncOsc's Get Noise Saturation: fld the field, fld1,
 * fxch, fyl2x, fst plus fstp dual-write. Excluded rather than forced,
 * same rationale as every prior fyl2x instance in this family.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes value and displayValue.
 *   - Plain 8-bit field, always movsx-signed: single-writes value only.
 *   - Byte 0x3c packs FOUR independent single-bit booleans: Gate At Note
 *     On Only bit 0 no shift, Gate Below Threshold Use AMS bit 1, Gate At
 *     Above Threshold Use AMS bit 2, Shape Type bit 3 -- same
 *     shift-then-mask shape as CSTGPolysixModelPatch's own Arpeggiator
 *     group, one more bit-packed byte for the family.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGAMSMixerBase {
	STGConvertedParam &GetAAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetASource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmountForOffset(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetGateAtAboveThresholdFixedValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetGateAtAboveThresholdUseAMS(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetGateAtNoteOnOnly(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetGateBelowThresholdFixedValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetGateBelowThresholdUseAMS(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetGateControlAMSSourceSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetGateThreshold(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOffset(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetQuantizeSteps(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetShape(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetShapeType(CSTGPatchMessageContext &ctx);
};

#endif
