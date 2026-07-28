// SPDX-License-Identifier: GPL-2.0
/*
 * stg_vpm_eg_valuegetters.cpp  -  CSTGVPMEG's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_vpm_eg.h for the full class-level derivation notes --
 * all 5 real weak-symbol ctx-only candidates decoded, zero outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, reusing the bare stride-4
 * SIB-scaled ctx-index shape -- no lea premultiply -- first confirmed on
 * CSTGMultiFilter2Pole, and the mask-only single-bit bitfield shape on
 * GetTriggerAtNoteOn. verify/test_stg_vpm_eg_valuegetters.cpp
 * independently re-derives the expected value for every method here
 * from the SAME parsed facts via a separate Python evaluator, not by
 * re-using this file's C output strings.
 */

#include "oa_stg_vpm_eg.h"

STGConvertedParam &CSTGVPMEG::GetAMS1LevelModSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x3e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMEG::GetAMS1LevelModIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 4) + 0x3f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMEG::GetAMS1TimeModSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x2d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMEG::GetAMS1TimeModIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 4) + 0x2e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMEG::GetTriggerAtNoteOn(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x4f) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
