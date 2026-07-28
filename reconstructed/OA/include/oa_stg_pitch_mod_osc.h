// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PITCH_MOD_OSC_H
#define OA_STG_PITCH_MOD_OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_pitch_mod_osc.h  -  CSTGPitchModOsc's value-getter family: all 8
 * real weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGPitchModOsc is the per-oscillator pitch-modulation patch component --
 * EGSelect, EGAmount, plus a two-level AMS source/intensity nest for the
 * oscillator's own modulation amount -- confirmed genuinely fresh via a
 * word-boundary grep before starting (one incidental, non-triggering
 * mention of the class name in oa_stg_pitch_mod.h's own prose does not
 * count as a real reference). Confirmed DISTINCT from the already-done
 * CSTGPitchModCommon/CSTGPitchModCommonPlusAMS/CSTGPitchModOscBase and
 * from the unrelated GetEGAddress(CSTGVoice&, int) global-linkage helper
 * on this same class, excluded up front as a different-signature outlier.
 *
 * Dialect: mixed -- EGSelect, EGAmount, EGAMSSource, and EGAMSIntensity
 * are plain fixed-K fields off `this`; the AMSSource/AMSIntensity pair and
 * their own second-level AMSIntensityAMSSource/AMSIntensityAMSIntensity
 * siblings use the established stride-5 lea-premultiply ctx-dynamic-index
 * shape, same as CSTGMOSSAmp's own precedent in this batch.
 *
 * Field-shape summary -- ctx-indexed record base is CtxIndex applied to
 * ctx offset 0x4 with stride 5:
 *   - GetEGSelect at +0x12: plain UNSIGNED byte field, not a bitfield
 *     (`movzx`, no shift/mask) -- same "unsigned non-bitfield byte" variant
 *     first confirmed on CSTGPolysixMG::GetMIDITempoSyncTimes, single-write.
 *   - GetEGAmount at +0x13, GetEGAMSIntensity at +0x17: plain fixed-K
 *     32-bit fields, dual-write, not ctx-indexed.
 *   - GetEGAMSSource at +0x1b: plain fixed-K signed byte, single-write,
 *     not ctx-indexed.
 *   - GetAMSSource at +0x20, GetAMSIntensityAMSSource at +0x2a: signed
 *     byte off the ctx-indexed record base, single-write.
 *   - GetAMSIntensity at +0x1c, GetAMSIntensityAMSIntensity at +0x26:
 *     32-bit field off the ctx-indexed record base, dual-write.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGPitchModOsc {
	STGConvertedParam &GetEGSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
};

#endif
