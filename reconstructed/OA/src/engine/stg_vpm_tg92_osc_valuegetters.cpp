// SPDX-License-Identifier: GPL-2.0
/*
 * stg_vpm_tg92_osc_valuegetters.cpp  -  CSTGVPMTG92Osc's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_vpm_tg92_osc.h for the full class-level derivation
 * notes -- all 9 real weak-symbol ctx-only candidates decoded, zero
 * outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder. verify/test_stg_vpm_tg92_osc_
 * valuegetters.cpp independently re-derives the expected value for
 * every method here from the SAME parsed facts via a separate Python
 * evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_vpm_tg92_osc.h"

STGConvertedParam &CSTGVPMTG92Osc::GetVolume(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x87);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMTG92Osc::GetVolumeEGSelect(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x8b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMTG92Osc::GetVolumeVelocitySensitivity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x8c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMTG92Osc::GetVolumeAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x98);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMTG92Osc::GetVolumeAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x90);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMTG92Osc::GetVolumeAMSIntModSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x99);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMTG92Osc::GetVolumeAMSIntModIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x94);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMTG92Osc::GetVolumeAMSMode(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x9a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMTG92Osc::GetOscOnOff(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x9b) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
