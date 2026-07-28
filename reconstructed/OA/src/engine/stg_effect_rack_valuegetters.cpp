// SPDX-License-Identifier: GPL-2.0
/*
 * stg_effect_rack_valuegetters.cpp  -  CSTGEffectRack's
 * Get*(CSTGMessageContext&) value-getter family, see
 * include/oa_global.h's own header comment above `struct
 * CSTGEffectRack` for the full class-level derivation notes -- all 13
 * real ctx-only candidates decoded, mixed weak/strong linkage, zero
 * outliers.
 *
 * ResolveEffectSlotRecord, a static file-local helper below, reproduces
 * GetValueAlgorithm/GetValueDModMIDIRouting's shared piecewise 3-bank
 * record resolver branch-for-branch, including ground truth's own
 * unguarded NULL fallback for idx greater than 15 -- dead code in
 * practice, since the real unified slot index is always 0..15: IFX
 * 0..11, MFX 12..13, TFX 14..15.
 *
 * verify/test_stg_effect_rack_valuegetters.cpp independently re-derives
 * the expected value for every method here via a separate Python
 * evaluator, not by re-using this file's C output strings.
 */

#include "oa_global.h"

static unsigned char *ResolveEffectSlotRecord(CSTGEffectRack *self, int idx)
{
	unsigned char *base = (unsigned char *)self;
	if (idx <= 11)
		return base + 4 + idx * 0xa8;
	if (idx <= 13)
		return base + 0x7e4 + (idx - 12) * 0x9c;
	if (idx <= 15)
		return base + 0x91c + (idx - 14) * 0x98;
	return (unsigned char *)0;
}

STGConvertedParam &CSTGEffectRack::GetValueAlgorithm(CSTGMessageContext &ctx)
{
	unsigned char *rec = ResolveEffectSlotRecord(this, (int)ctx.index);
	int v = *(unsigned char *)(rec + 5);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueDModMIDIRouting(CSTGMessageContext &ctx)
{
	unsigned char *rec = ResolveEffectSlotRecord(this, (int)ctx.index);
	int v = *(signed char *)(rec + 6);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueIFXEffectChainIndex(CSTGMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed char *)(base + idx * 0xa8 + 0x9f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueIFXBusIndex(CSTGMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed char *)(base + idx * 0xa8 + 0x9c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueIFXFXControlBusIndex(CSTGMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed char *)(base + idx * 0xa8 + 0x9d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueIFXHDRBusIndex(CSTGMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed char *)(base + idx * 0xa8 + 0x9e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueIFXPan(CSTGMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0xa8 + 0xa0);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueIFXSend1Level(CSTGMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0xa8 + 0xa4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueIFXSend2Level(CSTGMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0xa8 + 0xa8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueMFXReturnLevel(CSTGMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0x9c + 0x87c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueMFXChainDirection(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0xa50)) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueMFXChainLevel(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xa4c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectRack::GetValueMasterVolume(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xb5b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
