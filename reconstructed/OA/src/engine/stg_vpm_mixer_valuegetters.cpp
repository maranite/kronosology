// SPDX-License-Identifier: GPL-2.0
/*
 * stg_vpm_mixer_valuegetters.cpp  -  CSTGVPMMixer's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_vpm_mixer.h for the full class-level derivation notes --
 * all 4 real weak-symbol ctx-only candidates decoded, zero outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, reusing the stride-10
 * lea-premultiply-plus-SIB-scale ctx-index shape first confirmed on
 * CSTGMS20. verify/test_stg_vpm_mixer_valuegetters.cpp independently
 * re-derives the expected value for every method here from the same
 * parsed facts via a separate Python evaluator, not by re-using this
 * file's C output strings.
 */

#include "oa_stg_vpm_mixer.h"

STGConvertedParam &CSTGVPMMixer::GetLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 10) + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMMixer::GetLevelAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 10) + 0x14);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMMixer::GetLevelAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 10) + 0x10);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMMixer::GetPhaseInvert(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 10) + 0x15);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
