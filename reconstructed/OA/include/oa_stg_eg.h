// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_EG_H
#define OA_STG_EG_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_eg.h  -  CSTGEG's value-getter family: all 10 real weak-symbol
 * ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGEG is the STG general-purpose envelope-generator patch component --
 * per-segment AMS time/level modulation source and intensity fields, per
 * its own method names below -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * Dialect: the AMS1/AMS2/AMS3 TimeModSource and AMS1/AMS2 LevelModSource
 * fields are plain fixed-K signed bytes read directly off this -- despite
 * the naming implying a runtime-selected slot, they are NOT ctx-indexed.
 * The matching *ModIntensity siblings ARE ctx-indexed: `mov edx,[edx+0x4]`
 * with no lea premultiply at all, straight into a SIB-scaled field load
 * `[eax+edx*4+K]` -- effective stride 4, same bare-stride-4 shape as
 * CSTGMultiFilter2Pole's own GetLFOIntensity/GetLFOJSminusYIntensity, no
 * new decoder shape needed.
 *
 * 4 pending symbols excluded up front, not fed to the decoder: this
 * class also exports GetAMSTimeModSource, GetAMSLevelModSource,
 * GetAMSTimeModIntensity, and GetAMSLevelModIntensity -- all real weak
 * symbols carrying an extra explicit slot-index argument, one unsigned
 * char for the Source pair or two unsigned chars for the Intensity
 * pair, instead of the family's plain ctx-only signature -- the same
 * "extra args beyond ctx" exclusion class documented since
 * CSTGOrganModelPatch's own GetRequiredVoiceInfo precedent.
 *
 * Field-shape summary:
 *   - Plain 8-bit signed field: single-writes .value only -- the
 *     TimeModSource/LevelModSource group.
 *   - ctx-indexed 32-bit field, bare stride-4 SIB scale, no lea
 *     premultiply: dual-writes .value and .displayValue -- the
 *     TimeModIntensity/LevelModIntensity group.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGEG {
	STGConvertedParam &GetAMS1LevelModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1LevelModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS2LevelModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS2LevelModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1TimeModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS1TimeModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS2TimeModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS2TimeModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS3TimeModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMS3TimeModIntensity(CSTGPatchMessageContext &ctx);
};

#endif
