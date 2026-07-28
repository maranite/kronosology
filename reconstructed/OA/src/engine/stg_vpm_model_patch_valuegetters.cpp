// SPDX-License-Identifier: GPL-2.0
/*
 * stg_vpm_model_patch_valuegetters.cpp  -  CSTGVPMModelPatch's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_vpm_model_patch.h for the full class-level derivation
 * notes -- all 10 real weak-symbol ctx-only candidates decoded, zero
 * outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, extended this batch to
 * recognize a ctx-index-as-variable-shift-count bit-select shape.
 * verify/test_stg_vpm_model_patch_valuegetters.cpp independently
 * re-derives the expected value for every method here from the SAME
 * parsed facts via a separate Python evaluator, not by re-using this
 * file's C output strings.
 */

#include "oa_stg_vpm_model_patch.h"

STGConvertedParam &CSTGVPMModelPatch::GetAlgorithm(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xd00);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetInputJack(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 1) + 0xd01);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetAnalog(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xce8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetInterMixerLink(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0xd1b) >> CtxShift(ctx, 0x4)) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetMacroBrightness(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xcec);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetMacroBrightnessVelocitySensitivity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xcf0);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetMacroTimbre(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xcf4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetMacroFeedback(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xcf8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetMacroDetune(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xcfc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMModelPatch::GetOscMacroClass(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0xd1c) >> CtxShift(ctx, 0x4)) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
