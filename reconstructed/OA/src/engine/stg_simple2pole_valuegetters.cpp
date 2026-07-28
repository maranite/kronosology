// SPDX-License-Identifier: GPL-2.0
/*
 * stg_simple2pole_valuegetters.cpp  -  CSTGSimple2Pole's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_simple2pole.h for the full class-level derivation
 * notes -- all 13 real weak-symbol ctx-only candidates decoded, zero
 * outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder. verify/test_stg_simple2pole_
 * valuegetters.cpp independently re-derives the expected value for
 * every method here from the SAME parsed facts via a separate Python
 * evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_simple2pole.h"

STGConvertedParam &CSTGSimple2Pole::GetBypass(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xd) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetFilterType(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetTrim(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x16);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetFrequency(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xe);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetResonance(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x12);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetResonanceAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x2d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetResonanceAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x29);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetFreqAMS1Source(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetFreqAMS1Intensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetFreqAMS2Source(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x23);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetFreqAMS2Intensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetFreqAMS1IntensityAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x28);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGSimple2Pole::GetFreqAMS1IntensityAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x24);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
