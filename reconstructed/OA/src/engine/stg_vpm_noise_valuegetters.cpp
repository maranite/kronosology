// SPDX-License-Identifier: GPL-2.0
/*
 * stg_vpm_noise_valuegetters.cpp  -  CSTGVPMNoise's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_vpm_noise.h for the full class-level derivation notes --
 * all 7 real ctx-only candidates decoded, zero outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, reusing the plain fixed-K field and
 * hardcoded-constant-getter shapes already established from prior classes
 * -- no new decoder shapes needed.
 * verify/test_stg_vpm_noise_valuegetters.cpp independently re-derives the
 * expected value for every method here from the SAME parsed facts via a
 * separate Python evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_vpm_noise.h"

STGConvertedParam &CSTGVPMNoise::GetSaturation(CSTGPatchMessageContext &)
{
	CSTGParamsOwner::sValueGetterTemp.value = 0;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMNoise::GetCutoff(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x10);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMNoise::GetVolume(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x14);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMNoise::GetVolumeEGSelect(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x18);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMNoise::GetVolumeAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMNoise::GetVolumeAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x19);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMNoise::GetVolumeAMSMode(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
