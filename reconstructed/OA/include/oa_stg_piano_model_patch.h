// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PIANO_MODEL_PATCH_H
#define OA_STG_PIANO_MODEL_PATCH_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_piano_model_patch.h  -  CSTGPianoModelPatch's value-getter
 * family: 16 of 18 real weak-symbol ctx-only candidates reconstructed,
 * plus 2 small real accessor helpers used by the ctx-indexed group --
 * see include/oa_stg_string.h for the pilot class's full derivation.
 * NOT the same class as CPianoOsc -- CSTGPianoModelPatch is the
 * higher-level acoustic-piano patch component, owning sustain-pedal
 * velocity zones, amp velocity, and release/damper-noise trims, that in
 * turn owns a CPianoOsc, confirmed distinct via word-boundary grep
 * before starting; only appeared in an unrelated stub-file comment
 * before this batch, no real struct.
 *
 * Of the 25 pending Get*-prefixed symbols this class's own address range
 * carries, 18 are real weak/COMDAT ctx-only value getters; the remaining
 * 7 are global 'T' linkage -- SetupExcludeParams, GetVoiceLevelEstimate,
 * SetupComponentOffsets, GetRequiredVoiceInfo, GetRestrikeLimitForNote,
 * SetOutputLevelMultiplier, GetEG -- all take extra arguments beyond ctx
 * or are runtime per-voice accessors, excluded up front by the linkage
 * check.
 *
 * Dialect: mostly the simplest fixed-K-off-`this` shape -- GetPianoType,
 * GetStereoPerspective, GetSustainPedalNoiseEnable,
 * GetSustainPedalNoiseLevel, GetAmpVelocityIntensity, GetReleaseTime,
 * GetDamperDownNoiseTrim, GetDamperUpNoiseTrim, 8 methods -- plus a new
 * ctx-dynamic-index sub-family: the SustainPedalDown- and
 * SustainPedalUp-prefixed multisample-zone group, 8 methods, whose base
 * pointer is NOT `this` directly. It comes from a virtual-dispatch call
 * through `this`'s own vtable, slot 0x170 for "Down" and slot 0x174 for
 * "Up", before the ctx-index arithmetic is applied. Decompiling those
 * two vtable targets directly -- `AccessSustainPedalDownVelocityZones`
 * and `AccessSustainPedalUpVelocityZones`, both real weak pending
 * symbols, 4 bytes each -- shows they are trivial constant-offset
 * accessors: `lea eax,[eax+0x14]; ret` and `lea eax,[eax+0x78]; ret`
 * respectively, i.e. `this + 0x14` and `this + 0x78`. They are safe to
 * reconstruct as real member functions here and call directly, rather
 * than treating the whole ctx-indexed group as an outlier. This is a
 * new shape for the family: a virtual-call-mediated sub-object base
 * pointer whose target turned out to be mechanically trivial once
 * decompiled, distinguishing it from CPianoOsc's own BankSelect outlier,
 * where the delegate target --
 * `CSTGMultisampleBankUUIDAndStereoFlag::GetBankIdAndStereoFlag`, 348
 * bytes -- is genuinely non-trivial and remains unreconstructed.
 * this+0x78 minus this+0x14 equals exactly 0x64, which is 4 times the
 * ctx-index stride of 25, confirming each zone array holds exactly 4
 * velocity-zone records of 25 bytes each -- matching the vtable's own
 * GetNumSustainPedalVelocityZones accessor.
 *
 * The ctx-index field itself is read as a BYTE here -- `movzx ebx, BYTE
 * ptr ctx plus 0x4` -- not the DWORD read the rest of the family's own
 * ctx-index group uses -- same conceptual "ctx's own dynamic index"
 * field, just a narrower load, modeled via a new CtxIndexByte helper
 * alongside the existing CtxIndex. Same effective stride-25 arithmetic
 * as CPianoOsc's own Level/MultisampleNum/BankType/BottomVelocity group,
 * two chained stride-5 `lea` premultiplies, reused directly.
 *
 * 2 genuine outliers excluded, NOT modeled here:
 * GetSustainPedalDownMultisampleBank and
 * GetSustainPedalUpMultisampleBank -- same ctx-index arithmetic as the
 * group above, but then make a REAL call into
 * CSTGMultisampleBankUUIDAndStereoFlag::GetBankIdAndStereoFlag, still
 * itself unreconstructed at 348 bytes -- the same genuine
 * cross-class-delegate outlier CPianoOsc's own BankSelect family hit.
 *
 * No exceptions to the width-vs-dual-write rule found among the decoded
 * candidates -- every dword dual-writes, every byte, whether unsigned
 * or the 2-bit on/off mask, single-writes.
 */

static inline int CtxIndexByte(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return (int)*(unsigned char *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGPianoModelPatch {
	/* Trivial sub-object base-pointer accessors -- real ground-truth
	 * vtable targets (slot 0x170/0x174), confirmed via disassembly to
	 * be exactly `return (unsigned char *)this + K;`, nothing more. */
	unsigned char *AccessSustainPedalDownVelocityZones();
	unsigned char *AccessSustainPedalUpVelocityZones();

	STGConvertedParam &GetAmpVelocityIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDamperDownNoiseTrim(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDamperUpNoiseTrim(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPianoType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStereoPerspective(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalDownBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalDownMultisample(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalDownMultisampleLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalDownMultisampleOnOff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalNoiseEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalNoiseLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalUpBottomVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalUpMultisample(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalUpMultisampleLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSustainPedalUpMultisampleOnOff(CSTGPatchMessageContext &ctx);
};

#endif
