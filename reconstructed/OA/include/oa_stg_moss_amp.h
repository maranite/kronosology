// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_MOSS_AMP_H
#define OA_STG_MOSS_AMP_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_moss_amp.h  -  CSTGMOSSAmp's value-getter family: all 6 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGMOSSAmp is the MOSS-algorithm amplifier patch component -- Level,
 * VelocityAmount, plus a two-level AMS source/intensity nest for the
 * amp's own modulation amount -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * 3 other pending symbols excluded up front, not fed to the decoder --
 * GetSubComponent(unsigned short) is the by-now-familiar weak/COMDAT
 * sub-object-accessor outlier (same shape as CSTGString's/
 * CSTGAnalog4PoleBase's own precedent), SetupComponentOffsets and
 * SetOutputLevelMultiplier are global ('T') linkage with extra args beyond
 * ctx -- the standard extra-args exclusion.
 *
 * Dialect: mixed -- Level and VelocityAmount are plain fixed-K fields off
 * `this`; the AMSSource/AMSIntensity pair and their own second-level
 * AMSIntensityAMSSource/AMSIntensityAMSIntensity siblings use the
 * established stride-5 lea-premultiply ctx-dynamic-index shape
 * (`mov edx,[edx+0x4]; lea edx,[edx+edx*4]`, bare `[eax+edx*1+K]` load on
 * top, no additional SIB scale) -- same naming-implies-double-modulation
 * shape as CSTGMS20EG's own precedent, reconfirmed here to be real
 * ctx-indexing rather than a false alarm -- unlike CSTGMS20EG's own case,
 * which turned out to be plain fixed offsets throughout.
 *
 * Field-shape summary -- ctx-indexed record base is CtxIndex applied to
 * ctx offset 0x4 with stride 5:
 *   - GetLevel at +0xc, GetVelocityAmount at +0x10: plain fixed-K 32-bit
 *     fields, dual-write, not ctx-indexed.
 *   - GetAMSSource at +0x18, GetAMSIntensityAMSSource at +0x22: signed
 *     byte off the ctx-indexed record base, single-write.
 *   - GetAMSIntensity at +0x14, GetAMSIntensityAMSIntensity at +0x1e:
 *     32-bit field off the ctx-indexed record base, dual-write.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGMOSSAmp {
	STGConvertedParam &GetLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVelocityAmount(CSTGPatchMessageContext &ctx);
};

#endif
