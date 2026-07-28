// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_PITCH_MOD_TG92OSC_H
#define OA_STG_VPM_PITCH_MOD_TG92OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_pitch_mod_tg92osc.h  -  CSTGVPMPitchModTG92Osc's value-getter
 * family: all 5 real weak-symbol ctx-only candidates reconstructed, zero
 * outliers -- see include/oa_stg_string.h for the pilot class's full
 * derivation. CSTGVPMPitchModTG92Osc is the VPM engine's TG92-oscillator
 * pitch-modulation component -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * Dialect: mixed AMS pair, same asymmetric split first confirmed on
 * CSTGVPMEG -- AMSSource is ctx-indexed via the family's stride-5
 * lea-premultiply shape -- edx times 5, bare +K load, signed byte -- while
 * AMSIntensity uses the SAME stride-5 index against a dword field. The
 * second-level AMSIntensityAMSSource and AMSIntensityAMSIntensity fields
 * are both plain fixed-K reads off this, NOT ctx-indexed at all, matching
 * CSTGVPMEG's own precedent that only the first-level Intensity half of a
 * pair carries the runtime slot index.
 *
 * UseCommonMod uses the family's established mask-only single-bit
 * bitfield shape -- no shift instruction, bit 0 -- single-write only.
 *
 * Field-shape summary:
 *   - ctx-indexed 8-bit signed field, stride-5 lea premultiply: single-
 *     writes value only -- AMSSource.
 *   - ctx-indexed 32-bit field, stride-5 lea premultiply: dual-writes
 *     value and displayValue -- AMSIntensity.
 *   - Plain 8-bit signed field: single-writes value only --
 *     AMSIntensityAMSSource.
 *   - Plain 32-bit field: dual-writes value and displayValue --
 *     AMSIntensityAMSIntensity.
 *   - Mask-only single-bit boolean, bit 0: single-writes value only --
 *     UseCommonMod.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGVPMPitchModTG92Osc {
	STGConvertedParam &GetAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUseCommonMod(CSTGPatchMessageContext &ctx);
};

#endif
