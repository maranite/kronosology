// SPDX-License-Identifier: GPL-2.0
/*
 * stg_vpm_pitch_mod_tg92osc_valuegetters.cpp  -  CSTGVPMPitchModTG92Osc's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_vpm_pitch_mod_tg92osc.h for the full class-level
 * derivation notes -- all 5 real weak-symbol ctx-only candidates decoded,
 * zero outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, reusing the stride-5
 * lea-premultiply ctx-index shape first confirmed on CSTGString, and the
 * mask-only single-bit bitfield shape on GetUseCommonMod.
 * verify/test_stg_vpm_pitch_mod_tg92osc_valuegetters.cpp independently
 * re-derives the expected value for every method here from the SAME
 * parsed facts via a separate Python evaluator, not by re-using this
 * file's C output strings.
 */

#include "oa_stg_vpm_pitch_mod_tg92osc.h"

STGConvertedParam &CSTGVPMPitchModTG92Osc::GetAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x16);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMPitchModTG92Osc::GetAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x12);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMPitchModTG92Osc::GetAMSIntensityAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x20);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMPitchModTG92Osc::GetAMSIntensityAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMPitchModTG92Osc::GetUseCommonMod(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x21) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
