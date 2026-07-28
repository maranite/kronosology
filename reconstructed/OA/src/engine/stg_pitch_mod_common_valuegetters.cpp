// SPDX-License-Identifier: GPL-2.0
/*
 * stg_pitch_mod_common_valuegetters.cpp  -  CSTGPitchModCommon's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_pitch_mod_common.h for the full class-level derivation
 * notes -- all 5 real weak-symbol ctx-only candidates decoded, zero
 * outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, same fixed-K plain-field shapes
 * as every simplest-dialect class in the family, plus the established
 * unsigned-non-bitfield-byte variant on GetLFOSelect.
 * verify/test_stg_pitch_mod_common_valuegetters.cpp independently
 * re-derives the expected value for every method here from the SAME
 * parsed facts via a separate Python evaluator, not by re-using this
 * file's C output strings.
 */

#include "oa_stg_pitch_mod_common.h"

STGConvertedParam &CSTGPitchModCommon::GetLFOSelect(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x33);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchModCommon::GetLFOAmount(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x26);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchModCommon::GetJSYToLFOAmount(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x2a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchModCommon::GetLFOAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x32);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchModCommon::GetLFOAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x2e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
