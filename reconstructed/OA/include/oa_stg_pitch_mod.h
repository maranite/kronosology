// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PITCH_MOD_H
#define OA_STG_PITCH_MOD_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_pitch_mod.h  -  CSTGPitchMod's value-getter family: all 12 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGPitchMod is the STG pitch-modulation patch component -- confirmed
 * DISTINCT via word-boundary grep from CSTGPitchModBase,
 * CSTGPitchModCommon, CSTGPitchModCommonPlusAMS, CSTGPitchModOsc, and
 * CSTGPitchModOscBase, all unrelated already-declared classes sharing the
 * same name prefix. 1 of its 13 pending Get symbols, Get Output taking
 * two ints, is global linkage with a different signature -- excluded up
 * front by the standard filter.
 *
 * Dialect: mixed, same two ctx-index shapes as CSTGMultiFilter2Pole's own
 * LFO group -- bare stride-4 SIB with no lea premultiply on Get LFO
 * Amount and Get JSY To LFO Amount, and the family's usual stride-5 lea
 * premultiply on Get LFO AMS Source and Get LFO AMS Intensity. Every
 * other candidate is a fixed-K field read directly off this.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes value and displayValue, including
 *     both ctx-indexed dword fields.
 *   - Plain 8-bit field, always movsx-signed: single-writes value only,
 *     including the ctx-indexed byte field Get LFO AMS Source.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGPitchMod {
	STGConvertedParam &GetEGAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetJSYToLFOAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOctave(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTranspose(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTune(CSTGPatchMessageContext &ctx);
};

#endif
