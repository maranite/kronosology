// SPDX-License-Identifier: GPL-2.0
/*
 * oa_stg_patch.h  -  CSTGPatch: the shared base class for the entire
 * "XModelPatch" family (CSTGOrganModelPatch, CSTGPolysixModelPatch,
 * CSTGMS20ModelPatch, CSTGVPMModelPatch, CSTGPluckedModelPatch,
 * CSTGEPModelPatch, CSTGPCMModelPatch, CSTGAnalogSyncModelPatch,
 * CSTGPianoModelPatch, ...) -- confirmed via the dtor's own
 * `&PTR__CSTGParamsOwner_006c04a8` vptr write (identical pattern to
 * CSTGKeyTrack, oa_stg_key_track.h), i.e. CSTGPatch itself genuinely
 * derives from CSTGParamsOwner, NOT modeled as a real C++ base class
 * here for the same reason (see that header's own comment).
 *
 * FOUND 2026-07-29 (round 53, solo). 34/84 pending methods landed this
 * round -- almost the entire set is this class's own DEFAULT virtual
 * override bodies (meant to be overridden by the concrete "XModelPatch"
 * siblings above): trivial no-ops, or fixed-constant returns (0, 1, 6,
 * 0xffffffff). Confirmed via 7+ spot-checked ground-truth decompiles,
 * all `cc=__cdecl`/`(void)` prototype (Ghidra recovered ZERO real
 * parameters for any of them -- the "richer" signature shown in each
 * one's own C++-demangled comment, e.g.
 * `NotifyNoteOff(CSTGPatchMessageContext&, unsigned char)`, is what the
 * OVERRIDING subclass sees; this base's own default body ignores every
 * argument, so it's reconstructed matching exactly what Ghidra
 * recovered -- zero params -- same "static-shaped trivial accessor"
 * treatment already established for CSTGKeyTrack's own
 * GetId/GetName/GetNumParams).
 *
 * `~CSTGPatch()` -- both D0/D2 variants byte-identical (same
 * `&PTR__CSTGParamsOwner_006c04a8` vptr reset, no free()/
 * HAL_DisableInterrupts() in either), same "no-op beyond vptr reset"
 * quirk already established for CSTGKeyTrack -- landed as ONE real
 * dtor per this project's established D0/D2-dedup convention.
 *
 * `GetDefaultContext()` -- a function-local-static "default patch
 * message context" singleton, lazily vtable-stamped exactly once (real
 * ground truth's own guard is a single byte sharing storage with the
 * vptr field's own low byte -- the same "WARNING: Globals starting
 * with '_' overlap smaller symbols" class of decompiler artifact as
 * this round's own GetVoiceDelay/UpdateVoiceDelay, deferred separately
 * below; reconstructed here with a real, uncorrupted C++ static-init
 * guard instead, which reproduces the INTENDED behavior, not the
 * decompiler's byte-overlap artifact) but with its OTHER fields
 * unconditionally reset to fixed defaults on every single call.
 * Modeled as an opaque `unsigned char[0x31]` blob (real
 * `CSTGPatchMessageContext` layout not independently confirmed beyond
 * these raw offsets); the real vtable-slot value stamped into +0x00 is
 * a genuinely unmodeled external data symbol
 * (`PTR_IsLiveUpdate_006bf728`) -- stored here as an opaque non-null
 * sentinel, never dereferenced by any reconstructed caller, same
 * "opaque placeholder" treatment as this class's own vptr.
 *
 * `CheckMatchingToneAdjustTargetParam` -- static, compares 4 fields of
 * an opaque `CSTGToneAdjustDescriptor`-shaped blob at raw offsets
 * +6/+7/+8/+0xc; the real high-level signature comment's own claimed
 * `unsigned char` 2nd parameter contradicts the decompiled body's
 * actual use of it as a pointer base -- reconstructed matching the
 * ACTUAL decompiled parameter usage (a descriptor pointer), not the
 * possibly-stale/mismatched doc-comment signature.
 *
 * === Deferred, 3 distinct reasons (50/84 methods) ===
 * (1) 16 methods flagged by the decompiler itself (`in_stack_`/
 *     `unaff_`/"Could not recover jumptable") -- SaveParams, HandleCC,
 *     UseDefaults, HandleParamChange (the 158-byte overload, distinct
 *     from this round's landed-nowhere 53-byte one which is itself
 *     deferred under reason (2)), the InitVoiceNotifyXxx pair,
 *     SetupComponents, the 7 UpdateToneAdjustCommonXxx setters,
 *     GetRequiredVoiceInfo, GetMultisampleIds.
 * (2) ~33 methods making a genuine, fully-concrete virtual call through
 *     an UNNAMED vtable slot (`(**(code**)(*in_EAX + 0xNN))()` and
 *     sibling shapes operating on a `CSTGVoice&`'s own vtable, e.g.
 *     `NoteOff`/`Steal`/`FreeVoice`/`UpdateUnisonSpread`/`SetMute`/
 *     `HandleParamChange` (the 53-byte overload)/`HandleThreadIdChanged`,
 *     or the repeated "for each submodule index, call submodule's own
 *     vtable method" loop shape, e.g. `PrecomputeData`/
 *     `UpdateGlobalTune`/`UpdateTrackTune`/`UpdateTrackBendRange`) --
 *     the SAME deferral class already established for CSTGKeyTrack's
 *     own `PrecomputeData`/`UpdateXxxRamp` family (oa_stg_key_track.h):
 *     fully concrete control flow, but no independent confirmation of
 *     which named method occupies the target slot.
 * (3) `GetTransposedNote` -- decompiled body is a bare, unassigned
 *     `undefined4 in_ECX; return in_ECX;` despite `cc=__cdecl`/`(void)`
 *     (Ghidra recovered no real parameters and no assignment to
 *     `in_ECX` anywhere in the function) -- a genuinely unrecoverable
 *     return value (reads whatever happens to be in a register the
 *     `__cdecl` ABI never defines as an argument-passing register at
 *     this call shape), not a vtable-slot-naming problem like (2) --
 *     left undeclared rather than guessing a constant.
 */

#ifndef OA_STG_PATCH_H
#define OA_STG_PATCH_H

class CSTGPatch {
public:
	~CSTGPatch();

	/* --- default virtual overrides: LFO/EG family --- */
	static unsigned int GetNumLFOs();
	static unsigned int GetLFO();
	static unsigned int GetNumEGs();
	static unsigned int GetEG();
	static unsigned int GetEGRemapping();

	/* --- default virtual overrides: note-on/off notification family --- */
	static unsigned int WillHandleNoteOn();
	static void NotifyNoteOff();
	static void NotifyAllNotesOff();
	static void NotifyKeyReleased();

	/* --- default virtual overrides: static/feedback processing --- */
	static void ProcessStaticFront();
	static void ProcessStaticBack();
	static void ProcessFeedback();

	/* --- default virtual overrides: portamento/unison/wave-seq/misc --- */
	static void UpdateSlotPortamento();
	static void UpdateUnisonTuning();
	static unsigned int GetNumStaticAllocatedQuads();
	static unsigned int IsAllPortamentoOff();
	static unsigned int WillHandleUnaCorda();
	static unsigned int ShouldHold();
	static void WaveSequenceVoiceInit();
	static unsigned int GetMaxWaveSeqSwingResolution();
	static void UpdateWaveSeqSwingResolution();
	static unsigned int HasWaveSeqInOscZone();
	static unsigned int GetWaveSeqIdInOscZone();
	static void OverrideWaveform();
	static void ResetWaveform();
	static unsigned int GetExclusiveGroupForNote();
	static void ApplyRestrikeLevelScaling();
	static unsigned int GetRestrikeLimitForNote();
	static void SetOutputLevelMultiplier();
	static void SetDModValues();
	static void ResetDMod();

	static bool CheckMatchingToneAdjustTargetParam(const unsigned char *descriptor,
							char c1, char c2, int v1, int v2);

	static void *GetDefaultContext();

private:
	unsigned char mVptrPlaceholder[4]; /* +0x00, see header comment / oa_stg_key_track.h */
};

#endif /* OA_STG_PATCH_H */
