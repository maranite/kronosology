// SPDX-License-Identifier: GPL-2.0
/*
 * stg_organ_osc_valuegetters.cpp  -  CSTGOrganOsc's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_organ_osc.h for the full class-level derivation notes --
 * 13 of 36 pending candidates -- the other 23 are a different, global-linkage
 * mechanism, excluded up front.
 *
 * All 13 bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder -- no new instruction shapes needed.
 * verify/test_stg_organ_osc_valuegetters.cpp independently re-derives the
 * expected value for every method here from the SAME parsed facts via a
 * separate Python evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_organ_osc.h"

STGConvertedParam &CSTGOrganOsc::GetUpDrawbar(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 1) + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetLowDrawbar(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 1) + 0x19);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetDrawbarLevelCurve(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x2b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetSplitPoint(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x2c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetUpperOctaveShift(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x2d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetLowerOctaveShift(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x2e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetEXDrawbarModeUp(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x2f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetEXDrawbarModeLow(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x30);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetEXDrawbarPitchUp(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 1) + 0x31);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetEXDrawbarPitchLow(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 1) + 0x35);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetEXPercDrawbar(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 1) + 0x26);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetPercAssignValue(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x39);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGOrganOsc::GetEnvelopeType(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x3a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
