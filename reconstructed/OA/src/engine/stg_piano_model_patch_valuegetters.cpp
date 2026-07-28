// SPDX-License-Identifier: GPL-2.0
/*
 * stg_piano_model_patch_valuegetters.cpp  -  CSTGPianoModelPatch's
 * Get*(CSTGPatchMessageContext&) value-getter family plus its 2 real
 * sub-object accessor helpers, see include/oa_stg_piano_model_patch.h
 * for the full class-level derivation notes -- 16 of 18 real weak-symbol
 * ctx-only candidates decoded, 2 genuine outliers excluded.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, extended this batch to recognize
 * a ctx-dynamic-index group whose base pointer comes from a trivial,
 * once-decompiled virtual-dispatch accessor call rather than `this`
 * directly, and a byte-width ctx-index field load via the new
 * CtxIndexByte helper instead of the family's usual dword-width
 * CtxIndex read. verify/test_stg_piano_model_patch_valuegetters.cpp independently
 * re-derives the expected value for every method here from the SAME
 * parsed facts via a separate Python evaluator, not by re-using this
 * file's C output strings.
 */

#include "oa_stg_piano_model_patch.h"

unsigned char *CSTGPianoModelPatch::AccessSustainPedalDownVelocityZones()
{
	return (unsigned char *)this + 0x14;
}

unsigned char *CSTGPianoModelPatch::AccessSustainPedalUpVelocityZones()
{
	return (unsigned char *)this + 0x78;
}

STGConvertedParam &CSTGPianoModelPatch::GetPianoType(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetStereoPerspective(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xd);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalNoiseEnable(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xe);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalNoiseLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x10);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetAmpVelocityIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xdc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetReleaseTime(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xe0);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetDamperDownNoiseTrim(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xe4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetDamperUpNoiseTrim(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xe8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalDownMultisampleOnOff(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = AccessSustainPedalDownVelocityZones();
	int v = *(unsigned char *)(base + CtxIndexByte(ctx, 0x4, 25) + 0x16) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalDownMultisample(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = AccessSustainPedalDownVelocityZones();
	int v = *(unsigned short *)(base + CtxIndexByte(ctx, 0x4, 25) + 0x14);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalDownMultisampleLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = AccessSustainPedalDownVelocityZones();
	int v = *(int *)(base + CtxIndexByte(ctx, 0x4, 25) + 0x10);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalDownBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = AccessSustainPedalDownVelocityZones();
	int v = *(unsigned char *)(base + CtxIndexByte(ctx, 0x4, 25) + 0x17);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalUpMultisampleOnOff(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = AccessSustainPedalUpVelocityZones();
	int v = *(unsigned char *)(base + CtxIndexByte(ctx, 0x4, 25) + 0x16) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalUpMultisample(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = AccessSustainPedalUpVelocityZones();
	int v = *(unsigned short *)(base + CtxIndexByte(ctx, 0x4, 25) + 0x14);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalUpMultisampleLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = AccessSustainPedalUpVelocityZones();
	int v = *(int *)(base + CtxIndexByte(ctx, 0x4, 25) + 0x10);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPianoModelPatch::GetSustainPedalUpBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = AccessSustainPedalUpVelocityZones();
	int v = *(unsigned char *)(base + CtxIndexByte(ctx, 0x4, 25) + 0x17);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
