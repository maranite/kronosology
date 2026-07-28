// SPDX-License-Identifier: GPL-2.0
#ifndef OA_PIANO_OSC_H
#define OA_PIANO_OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_piano_osc.h  -  CPianoOsc's value-getter family -- 46 of 53 real
 * weak-symbol Get*(CSTGPatchMessageContext&) candidates reconstructed via
 * the STG value-getter family's scripted instruction-pattern decoder --
 * see include/oa_stg_string.h for the pilot class's full derivation.
 * CPianoOsc is NOT itself STG-prefixed but participates in the exact same
 * convention -- same sValueGetterTemp sink, same CSTGPatchMessageContext&
 * signature, same weak/COMDAT per-symbol .text sections -- it is the
 * acoustic-piano voice-model patch component: Tine and Reed are the two
 * physical-model layers, UnaCorda/Resonance/KeyOffNoise/ReleaseSample are
 * secondary layered samples.
 *
 * Dialect: mixed, same shape family as CSTGMS20/CSTGPolysix -- most
 * methods keep `this` in eax with a fixed-K offset, but the
 * Level/MultisampleNum/BankType/BottomVelocity group -- 7 named
 * parameter categories times 4 fields, 28 methods -- reads a per-call
 * dynamic index from ctx's own +0x4 field, same CtxIndex helper as the
 * rest of the family. NEW effective stride this batch: 25, produced by
 * TWO consecutive stride-5 `lea edx,[edx+edx*4]` premultiplies chained
 * back to back, 5 times 5, not a single lea plus an extra SIB scale
 * factor like CSTGMS20/CSTGPolysix's stride-10/20 cases -- the
 * addressing mode itself stays a bare `[eax+edx*1+K]` here, all the
 * multiplication happens ahead of the address computation.
 *
 * 7 genuine outliers excluded, NOT modeled here: the parallel
 * Get*BankSelect family -- GetBankSelect, GetResonanceBankSelect,
 * GetUnaCordaResonanceBankSelect, GetKeyOffNoiseBankSelect,
 * GetReleaseSampleBankSelect, GetUnaCordaBankSelect,
 * GetUnaCordaReleaseBankSelect -- computes a sub-object pointer via the
 * same ctx-index arithmetic but then makes a REAL call into
 * CSTGMultisampleBankUUIDAndStereoFlag::GetBankIdAndStereoFlag, still
 * itself unreconstructed at 348 bytes, which itself calls the
 * also-unreconstructed FindBankUUID -- a genuine cross-class delegate
 * call, not a pure field read, so out of scope for this mechanical
 * decoder. A NEW outlier variant for the family: prior batches' outliers
 * were all real DSP/math computation via fyl2x, sqrtss, x87 compares;
 * this is the first "delegates to another undecoded real member
 * function" case.
 *
 * New confirmed exception to "32-bit implies dual-write" this batch:
 * GetKeybedSize -- a fixed dword field that writes .value only, no
 * .displayValue -- a discrete/enum selector despite its 4-byte width,
 * same class of exception as CSTGString/CSTGOrganModelPatch/CSTGPolysix's
 * own discrete-dword cases.
 *
 * 8 excluded up front, not part of this family, never fed to the
 * decoder: GetRequiredVoiceInfo -- T linkage, extra args beyond ctx --
 * GetTransposedNote -- __thiscall, an unrelated helper -- and 6
 * metadata/factory-table cdecl stubs: GetId, GetName, GetNumParams,
 * GetParamDescriptors, GetMessageHandlers, GetValueGetters.
 * No Set* methods exist for this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CPianoOsc {
	STGConvertedParam &GetBankType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDamperResonanceTrim(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyOffNoiseBankType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyOffNoiseBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyOffNoiseEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyOffNoiseLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyOffNoiseMSLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyOffNoiseMultisampleNum(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeybedSize(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMechanicalNoiseTrim(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMultisampleNum(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoteReleaseTrim(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOctave(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseMSLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseMultisampleNum(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseSampleBankType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseSampleEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseSampleLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceAttack(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceBankType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceMSLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceMultisample(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceOn(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceRelease(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceRepedalScale(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTranspose(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaBankType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaMSLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaMultisampleNum(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaOn(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaReleaseBankType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaReleaseBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaReleaseMSLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaReleaseMultisampleNum(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaResonanceBankType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaResonanceBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaResonanceMSLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUnaCordaResonanceMultisample(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVelocityAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVelocityBias(CSTGPatchMessageContext &ctx);
};

#endif
