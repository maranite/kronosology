// SPDX-License-Identifier: GPL-2.0
/*
 * piano_osc_valuegetters.cpp  -  CPianoOsc's Get*(CSTGPatchMessageContext&)
 * value-getter family, see include/oa_piano_osc.h for the full
 * class-level derivation notes -- 46 of 53 real weak-symbol candidates
 * decoded, 7 genuine outliers excluded, see header for details.
 *
 * All 46 bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, extended this batch to recognize
 * a ctx-dynamic-index field load with TWO chained stride-5 lea
 * premultiplies giving an effective stride of 25, instead of a single lea
 * plus an extra SIB scale factor. verify/test_piano_osc_valuegetters.cpp
 * independently re-derives the expected value for every method here from
 * the SAME parsed facts via a separate Python evaluator, not by re-using
 * this file's C output strings.
 */

#include "oa_piano_osc.h"

STGConvertedParam &CPianoOsc::GetTranspose(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x14);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetOctave(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x15);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetBankType(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x2c) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetMultisampleNum(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned short *)(base + CtxIndex(ctx, 0x4, 25) + 0x2a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 25) + 0x26);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x2d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceBankType(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x158) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceMultisample(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned short *)(base + CtxIndex(ctx, 0x4, 25) + 0x156);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceMSLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 25) + 0x152);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x159);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaResonanceBankType(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x284) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaResonanceMultisample(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned short *)(base + CtxIndex(ctx, 0x4, 25) + 0x282);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaResonanceMSLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 25) + 0x27e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaResonanceBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x285);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceOn(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x39a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x39b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceRepedalScale(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x39f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceAttack(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x3a3);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetResonanceRelease(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x3a7);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaOn(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x3ab);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetVelocityBias(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x3ac);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetVelocityAmount(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x3b0);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetKeyOffNoiseEnable(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x3b4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetKeyOffNoiseLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x3b5);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetReleaseSampleEnable(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x3b9);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetReleaseSampleLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x3ba);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetKeyOffNoiseBankType(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x3f0) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetKeyOffNoiseMultisampleNum(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned short *)(base + CtxIndex(ctx, 0x4, 25) + 0x3ee);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetKeyOffNoiseMSLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 25) + 0x3ea);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetKeyOffNoiseBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x3f1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetReleaseSampleBankType(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x51c) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetReleaseMultisampleNum(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned short *)(base + CtxIndex(ctx, 0x4, 25) + 0x51a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetReleaseMSLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 25) + 0x516);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetReleaseBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x51d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaBankType(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x648) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaMultisampleNum(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned short *)(base + CtxIndex(ctx, 0x4, 25) + 0x646);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaMSLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 25) + 0x642);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x649);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetKeybedSize(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x88a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaReleaseBankType(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x774) & 0x3;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaReleaseMultisampleNum(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned short *)(base + CtxIndex(ctx, 0x4, 25) + 0x772);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaReleaseMSLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 25) + 0x76e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetUnaCordaReleaseBottomVelocity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 25) + 0x775);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetDamperResonanceTrim(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x88e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetMechanicalNoiseTrim(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x892);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CPianoOsc::GetNoteReleaseTrim(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x896);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
