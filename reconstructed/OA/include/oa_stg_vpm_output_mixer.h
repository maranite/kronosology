// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_OUTPUT_MIXER_H
#define OA_STG_VPM_OUTPUT_MIXER_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_output_mixer.h  -  CSTGVPMOutputMixer's value-getter family:
 * all 7 real weak-symbol ctx-only candidates reconstructed, zero outliers
 * -- see include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGVPMOutputMixer is the VPM engine's per-operator output mixer stage
 * -- level, pan, and phase invert, plus the AMS source/intensity siblings
 * for level and pan -- confirmed genuinely fresh via a word-boundary grep
 * before starting, no pre-existing struct or ctor anywhere in this
 * project.
 *
 * Dialect: a real ctx-dynamic-index sub-family covering six of the seven
 * candidates, using a genuinely NEW premultiply factor for this family --
 * `lea edx,[edx+edx*8]` multiplies ctx's own dynamic-index field by 9
 * -- every prior lea-premultiply shape in the family used factor 5. An
 * additional explicit x2 SIB scale on the field load itself
 * -- `[eax+edx*2+K]` -- combines with the x9 premultiply for an effective
 * stride of 18, per the established "SIB scale multiplies into the
 * existing premultiply stride" decoder rule -- no decoder code change
 * needed, purely a new confirmed stride value -- 18, distinct from the
 * family's prior 10, 20, and 25 SIB-scaled-lea variants.
 *
 * GetPhaseInvert is the by-now-familiar ctx-shift single-bit-boolean
 * shape first confirmed on CSTGVPMModelPatch's GetInterMixerLink/
 * GetOscMacroClass -- ctx's own dynamic-index field used as a variable
 * shift count rather than a record index, selecting one bit of a single
 * fixed byte field.
 *
 * Field-shape summary -- record base is CtxIndex applied to ctx offset
 * 0x4 with stride 18:
 *   - GetLevel at +0xc, GetPan at +0x15: plain 32-bit fields off the
 *     scaled record base, dual-write .value and .displayValue.
 *   - GetLevelAMSIntensity at +0x10, GetPanAMSIntensity at +0x19: same,
 *     dual-write.
 *   - GetLevelAMSSource at +0x14, GetPanAMSSource at +0x1d: signed byte
 *     off the scaled record base, single-write.
 *   - GetPhaseInvert: ctx-shift single-bit boolean off a fixed byte
 *     field at +0x78, not part of the scaled record, single-write.
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

struct CSTGVPMOutputMixer {
	STGConvertedParam &GetLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPan(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPanAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPanAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPhaseInvert(CSTGPatchMessageContext &ctx);
};

#endif
