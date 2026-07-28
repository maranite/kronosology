// SPDX-License-Identifier: GPL-2.0
/*
 * stg_pitch_mod_osc_base_valuegetters.cpp  -  CSTGPitchModOscBase's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_pitch_mod_osc_base.h for the full class-level
 * derivation notes -- all 3 real weak-symbol ctx-only candidates
 * decoded, zero outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, plain fixed-K plain-field shapes
 * as every simplest-dialect class in the family.
 * verify/test_stg_pitch_mod_osc_base_valuegetters.cpp independently
 * re-derives the expected value for every method here from the same
 * parsed facts via a separate Python evaluator, not by re-using this
 * file's C output strings.
 */

#include "oa_stg_pitch_mod_osc_base.h"

STGConvertedParam &CSTGPitchModOscBase::GetTranspose(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchModOscBase::GetTune(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xe);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchModOscBase::GetOctave(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xd);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
