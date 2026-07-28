// SPDX-License-Identifier: GPL-2.0
/*
 * stg_pitch_mod_common_plus_ams_valuegetters.cpp  -
 * CSTGPitchModCommonPlusAMS's Get*(CSTGPatchMessageContext&) value-getter
 * family, see include/oa_stg_pitch_mod_common_plus_ams.h for the full
 * class-level derivation notes -- both real weak-symbol ctx-only
 * candidates decoded, zero outliers.
 *
 * Both bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, same fixed-K plain-field shapes
 * as every simplest-dialect class in the family.
 * verify/test_stg_pitch_mod_common_plus_ams_valuegetters.cpp
 * independently re-derives the expected value for each method here from
 * the SAME parsed facts via a separate Python evaluator, not by re-using
 * this file's C output strings.
 */

#include "oa_stg_pitch_mod_common_plus_ams.h"

STGConvertedParam &CSTGPitchModCommonPlusAMS::GetAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x38);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchModCommonPlusAMS::GetAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x34);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
