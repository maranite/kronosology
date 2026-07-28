// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_MODEL_PATCH_H
#define OA_STG_VPM_MODEL_PATCH_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_model_patch.h  -  CSTGVPMModelPatch's value-getter family:
 * all 10 real weak-symbol ctx-only candidates reconstructed, zero
 * outliers -- see include/oa_stg_string.h for the pilot class's full
 * derivation. CSTGVPMModelPatch is the model-generator/algorithm-select
 * patch component that owns the VPM engine's -- FM/ring-mod/waveshaper
 * synthesis -- macro parameters: Algorithm/Analog/InputJack plus the
 * four Macro-prefixed morph controls -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * Dialect: mostly fixed-K-off-this, plus TWO real ctx-dynamic-index
 * sub-families, one of them a genuinely NEW shape for the family:
 *   - GetInputJack uses a bare, unscaled ctx-index load with no lea
 *     premultiply and no SIB scale either -- CtxIndex ctx, 0x4, 1,
 *     same shape as CSTGMS20's own GetInputJack precedent.
 *   - GetInterMixerLink/GetOscMacroClass are a NEW shape not seen
 *     anywhere else in the family so far: `mov ecx,[ctx+0x4]` loads
 *     ctx's own dynamic-index field not as an array index at all, but
 *     as a variable shift count -- `sar eax,cl` -- selecting which
 *     single bit of a fixed byte field to extract. Since the byte is
 *     loaded via movzx -- top 24 bits always zero -- the arithmetic
 *     sar and a logical shr are equivalent here -- modeled as a plain
 *     unsigned right-shift. x86 masks the shift count to 5 bits for a
 *     32-bit operand, so the helper below applies an explicit low-5-bit
 *     mask to match hardware shift-count masking. Distinguish this
 *     shape from the family's usual CtxIndex array-of-records indexing:
 *     here ctx's index field selects a BIT position within ONE fixed
 *     field, not a whole record's base offset.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes .value and .displayValue.
 *   - GetAlgorithm is a plain UNSIGNED byte field -- movzx, no
 *     shift/mask -- single-write only, same "unsigned non-bitfield
 *     byte" variant first confirmed on CSTGPolysixMG's own
 *     GetMIDITempoSyncTimes.
 *   - GetInputJack is a ctx-indexed SIGNED byte field -- single-write.
 *   - GetInterMixerLink/GetOscMacroClass are ctx-shift single-bit
 *     booleans -- single-write.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

static inline unsigned int CtxShift(CSTGPatchMessageContext &ctx, unsigned int off)
{
	return *(unsigned int *)((unsigned char *)&ctx + off) & 0x1f;
}

struct CSTGVPMModelPatch {
	STGConvertedParam &GetAlgorithm(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAnalog(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetInputJack(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetInterMixerLink(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMacroBrightness(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMacroBrightnessVelocitySensitivity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMacroDetune(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMacroFeedback(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMacroTimbre(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscMacroClass(CSTGPatchMessageContext &ctx);
};

#endif
