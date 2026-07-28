// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_MULTI_FILTER2POLE_H
#define OA_STG_MULTI_FILTER2POLE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_multi_filter2pole.h  -  CSTGMultiFilter2Pole's value-getter
 * family: all 23 real weak-symbol ctx-only candidates reconstructed,
 * zero outliers -- see include/oa_stg_string.h for the pilot class's
 * full derivation. CSTGMultiFilter2Pole is the STG multi-mode 2-pole
 * filter patch component -- Frequency/Resonance/Trim/EG/LFO modulation
 * parameters, per its own field names below -- confirmed genuinely
 * fresh via a word-boundary grep before starting, no pre-existing
 * struct or ctor anywhere in this project.
 *
 * Dialect: mostly the simplest fixed-K-off-this shape, same as
 * CSTGEPModelPatch/CSTGPolysixModelPatch -- plus a real ctx-dynamic-index
 * sub-family on the LFO* group, reading ctx's own plus-0x4 field as a
 * per-call slot index. Two index-scaling shapes present, one of them
 * NEW for the family:
 *   - GetLFOAMSSource/GetLFOAMSIntensity use the family's by-now-usual
 *     `mov edx,[edx+0x4]` plus `lea edx,[edx+edx*4]` stride-5
 *     premultiply, bare `[eax+edx*1+K]` addressing on the field load
 *     itself -- effective stride 5, same shape as CPianoOsc's own
 *     single-lea case.
 *   - GetLFOIntensity/GetLFOJSminusYIntensity use `mov edx,[edx+0x4]`
 *     with NO lea premultiply at all, going straight into a SIB-scaled
 *     field load `[eax+edx*4+K]` -- effective stride 4. This is a NEW
 *     variant, distinct from every prior ctx-index shape in the family:
 *     CSTGMS20's own bare-unscaled GetInputJack case has stride 1 with
 *     no SIB scale either, while every other class's ctx-index shape so
 *     far has gone through the stride-5 lea premultiply first. This is
 *     the first confirmed case of a raw per-call index used directly as
 *     a plain array-of-dwords subscript, no slot-record premultiply
 *     involved at all. Modeled via the same CtxIndex(ctx, off, stride)
 *     helper used across the family, just passing 4 directly as the
 *     already-fully-reduced stride -- no helper code changes needed.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes .value and .displayValue.
 *   - Plain 8-bit field, always movsx-signed in this class: single-writes
 *     .value only -- same convention as every prior class's own signed
 *     8-bit fields.
 *   - GetBypass alone is a single-bit boolean extracted from byte 0xd,
 *     bit 1 -- `movzx eax, BYTE [this+0xd]; shr al,1; and eax,1` -- the
 *     shift-then-mask bitfield shape first seen in CSTGVPMOsc/
 *     CSTGMS20ModelPatch, single-write only.
 *   - ctx-indexed 32-bit/8-bit fields: same dual/single-write rule as
 *     the fixed-offset case, by width.
 * No exceptions to the width-vs-dual-write rule found in this class --
 * every dword field dual-writes, every signed byte and the one boolean
 * bit single-write, no surprises.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGMultiFilter2Pole {
	STGConvertedParam &GetBypass(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEGVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS2Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS2Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequency(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeytrackIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOJSminusYIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOutputLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOutputLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOutputLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTrim(CSTGPatchMessageContext &ctx);
};

#endif
