// SPDX-License-Identifier: GPL-2.0
/*
 * stg_string_valuegetters.cpp  -  CSTGString's Get*(CSTGPatchMessageContext&)
 * "value getter" family, see include/oa_stg_string.h for the full class-
 * level derivation notes.
 *
 * All 105 bodies below were transcribed by a scripted instruction-pattern
 * decoder run against `objdump -dr -M intel` output for the real,
 * per-symbol weak/COMDAT `.text._ZN10CSTGString...` sections (same
 * methodology as CKGSeqBackupCommonParam/ModuleParam's own decoder, see
 * src/engine/karma_seq_backup.cpp's own header comment) -- not hand-typed
 * one at a time. The decoder recognizes exactly the instruction
 * vocabulary this family uses (`mov eax,[eax+K]`; `movsx`/`movzx
 * eax,BYTE/WORD [eax+K]`; `mov edx,[edx+0x4]` + `lea edx,[edx+edx*4]` or
 * `shl edx,N` for the per-call dynamic-index Pickup*, MixerPickup* group;
 * `mov eax,[eax+edx*1+K]`; `shr al,N`+`and eax,MASK` for packed boolean
 * bitfields; the fixed `mov ds:0x0,eax` / optional `mov ds:0x18,eax` /
 * `mov eax,0` epilogue that stores into CSTGParamsOwner::sValueGetterTemp
 * and returns its address) and turns each parsed (base, ctx-index*stride,
 * disp, width, signed, shift, mask) tuple into the C expression below.
 * 105 of 107 real weak-symbol candidates in this address range parsed
 * cleanly; the 2 that didn't (GetSubComponent's branchy sub-object
 * lookup, GetNoiseSaturation's fyl2x log2 transform) are NOT part of
 * this family and are left pending (see the header's own note).
 * verify/test_stg_string_valuegetters.cpp independently re-derives the
 * expected value for every method here from the SAME parsed
 * (offset, ctx-index*stride, width, signed, shift, mask) facts via a
 * separate Python evaluator (not by re-using this file's C strings), so
 * it catches rendering bugs in this file's C output, not just decoder
 * bugs shared by both.
 */

#include "oa_stg_string.h"

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

STGConvertedParam &CSTGString::GetDamping(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x130);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDampingAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x134);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDampingAMSIntensity1AMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x143);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDampingAMSIntensity1AMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x147);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDampingAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x138);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDampingStringTrackIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xe4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDecay(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x119);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDecayAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x11d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDecayAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x121);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDispersion(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x148);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDispersionAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x14c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDispersionAMSIntensity1AMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x15b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDispersionAMSIntensity1AMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x15f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDispersionAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x150);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDispersionStringTrackIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x10c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetDispersionType(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x21e) & 0x1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetFeedbackLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x110);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetFeedbackLevelAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x114);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetFeedbackLevelAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x118);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPosition(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1bb);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPositionAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1bf);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPositionAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1c3);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPositionScaling(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = ((*(unsigned char *)(base + 0x21e) >> 2) & 0x1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPressure(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1c4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPressureAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x1c8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPressureAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x1d2);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPressureAMSIntensityAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x1d6);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicPressureAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x1cc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetHarmonicUseExcitationPosition(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = ((*(unsigned char *)(base + 0x21e) >> 3) & 0x1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerNoiseBalance(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x215);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerNoiseBalanceAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x219);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerNoiseBalanceAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x21d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerNoiseLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x208);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerNoiseLevelAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x20c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerNoiseLevelAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x210);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerNoisePhaseInvert(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x211);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPCMBalance(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1ff);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPCMBalanceAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x203);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPCMBalanceAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x207);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPCMLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1f2);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPCMLevelAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1f6);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPCMLevelAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1fa);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPCMPhaseInvert(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1fb);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPickupBalance(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 32) + 0x192);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPickupBalanceAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 32) + 0x196);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPickupBalanceAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 32) + 0x19a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPickupLevel(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 32) + 0x185);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPickupLevelAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 32) + 0x189);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPickupLevelAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 32) + 0x18d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerPickupPhaseInvert(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 32) + 0x18e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerStringBalance(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1e9);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerStringBalanceAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1ed);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerStringBalanceAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1f1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerStringLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1dc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerStringLevelAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1e0);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerStringLevelAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1e4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetMixerStringPhaseInvert(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1e5);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseCutoff(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x10);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseCutoffAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x14);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseCutoffAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x18);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x19);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseLevelAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x21);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x2b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseLevelAMSIntensityAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x2f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseLevelAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x25);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseLevelPhaseInvert(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x1d);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNoiseLevelPrePost(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x30);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNonlinearity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x160);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNonlinearityAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x164);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetNonlinearityAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x168);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPCMLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x34);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPCMLevelAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x3c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPCMLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x46);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPCMLevelAMSIntensityAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x4a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPCMLevelAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x40);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPCMLevelPhaseInvert(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x38);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPCMLevelPrePost(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x4b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickPosition(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x16e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickPositionAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x172);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickPositionAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x176);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickPositionScaling(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = ((*(unsigned char *)(base + 0x21e) >> 1) & 0x1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickPositionTone(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x177);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickupPosition(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 32) + 0x17b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickupPositionAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 32) + 0x17f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickupPositionAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 32) + 0x183);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPickupPositionScaling(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 32) + 0x184);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckDelayAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x91);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckLevel(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x4f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckLevelAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x57);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x61);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckLevelAMSIntensityAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x65);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckLevelAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x5b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckLevelPhaseInvert(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x53);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckLevelPrePost(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x66);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckRandomAmt(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x6a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckRandomAmtAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x6e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckRandomAmtAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x72);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckRandomUseFilter(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x73);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckTableSelect(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x85);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckWidth(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x77);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckWidthAMSIntensity(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 5) + 0x7b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetPluckWidthAMSSource(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + CtxIndex(ctx, 0x4, 5) + 0x7f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetRelease(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x127);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetReleaseAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x12b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGString::GetReleaseAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x12f);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
