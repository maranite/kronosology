// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_AMP_H
#define OA_STG_AMP_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_amp.h  -  CSTGAmp's value-getter family: 7 of its 10 raw pending
 * weak symbols are real ctx-only candidates, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation. CSTGAmp
 * is the STG amplifier patch component -- overall level, velocity
 * sensitivity, and LFO tremolo amount plus their AMS source/intensity
 * siblings -- confirmed genuinely fresh via a word-boundary grep before
 * starting, no pre-existing struct or ctor anywhere in this project.
 *
 * 3 pending symbols excluded up front, not fed to the decoder: a
 * GetSubComponent overload taking an explicit unsigned short is the
 * by-now-familiar sub-object accessor outlier -- weak linkage but a
 * different signature, same exclusion class since CSTGString's own
 * precedent; a SetupComponentOffsets overload and a
 * SetOutputLevelMultiplier overload are both global 'T' linkage with
 * extra args beyond ctx.
 *
 * Dialect: mixed. Level, VelocityAmount, and LevelAMSIntensity are plain
 * fixed-K dwords, dual-writing .value and .displayValue. LevelAMSSource
 * is a plain fixed-K signed byte, single-write. LFOAmount is ctx-indexed
 * via a bare stride-4 SIB scale with no lea premultiply on a dword field,
 * dual-write -- the same bare-stride-4 shape first confirmed on
 * CSTGMultiFilter2Pole. LFOAmountAMSSource and LFOAmountAMSIntensity are
 * ctx-indexed via the usual stride-5 lea premultiply, reading a bare
 * signed byte single-write and a bare dword dual-write respectively off
 * the scaled index -- same shape as CSTGString's own pilot AMS sibling
 * group. No new decoder shapes needed for this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGAmp {
	STGConvertedParam &GetLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVelocityAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAmountAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAmountAMSIntensity(CSTGPatchMessageContext &ctx);
};

#endif
