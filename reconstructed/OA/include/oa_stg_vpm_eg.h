// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_EG_H
#define OA_STG_VPM_EG_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_eg.h  -  CSTGVPMEG's value-getter family: all 5 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGVPMEG is the VPM engine's own envelope-generator patch component
 * -- confirmed genuinely fresh via a word-boundary grep before starting,
 * no pre-existing struct or ctor anywhere in this project.
 *
 * Dialect: mixed. The AMS1LevelModSource and AMS1TimeModSource fields
 * are plain fixed-K signed bytes read directly off this -- despite the
 * "AMS1" naming implying a runtime-selected slot alongside their own
 * Intensity siblings, they are NOT ctx-indexed. The matching
 * *ModIntensity siblings ARE ctx-indexed: `mov edx,[edx+0x4]` with no
 * lea premultiply at all, straight into a SIB-scaled field load
 * `[eax+edx*4+K]` -- effective stride 4, same bare-stride-4 shape as
 * CSTGMultiFilter2Pole's own GetLFOIntensity/GetLFOJSminusYIntensity, no
 * new decoder shape needed. This is the first class in the family where
 * only the Intensity half of a Source/Intensity AMS pair is ctx-indexed
 * while the Source half stays fixed -- every prior class with this bare
 * stride-4 shape -- CSTGMultiFilter2Pole and CSTGEG -- had BOTH halves
 * of each pair ctx-indexed together.
 *
 * GetTriggerAtNoteOn uses the family's established mask-only single-bit
 * bitfield shape -- no shift instruction, bit 0 -- single-write only.
 *
 * Field-shape summary:
 *   - Plain 8-bit signed field: single-writes value only -- the
 *     AMS1LevelModSource/AMS1TimeModSource pair.
 *   - ctx-indexed 32-bit field, bare stride-4 SIB scale, no lea
 *     premultiply: dual-writes value and displayValue -- the
 *     AMS1LevelModIntensity/AMS1TimeModIntensity pair.
 *   - Mask-only single-bit boolean, bit 0: single-writes value only --
 *     TriggerAtNoteOn.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGVPMEG {
	STGConvertedParam &GetAMS1LevelModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1LevelModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1TimeModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1TimeModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTriggerAtNoteOn(CSTGPatchMessageContext &ctx);
};

#endif
