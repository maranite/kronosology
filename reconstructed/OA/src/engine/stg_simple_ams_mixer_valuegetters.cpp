// SPDX-License-Identifier: GPL-2.0
/*
 * stg_simple_ams_mixer_valuegetters.cpp  -  CSTGSimpleAMSMixer's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_simple_ams_mixer.h for the full class-level derivation
 * notes -- all 5 real weak-symbol ctx-only candidates decoded, zero
 * outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, same fixed-K plain-field shapes
 * as every simplest-dialect class in the family. verify/test_stg_simple_
 * ams_mixer_valuegetters.cpp independently re-derives the expected value
 * for every method here from the SAME parsed facts via a separate Python
 * evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_simple_ams_mixer.h"

STGConvertedParam &CSTGSimpleAMSMixer::GetType(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimpleAMSMixer::GetSourceA(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xd);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimpleAMSMixer::GetAmountA(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xf);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimpleAMSMixer::GetSourceB(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xe);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimpleAMSMixer::GetAmountB(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x13);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
