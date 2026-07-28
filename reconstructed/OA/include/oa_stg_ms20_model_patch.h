// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_MS20_MODEL_PATCH_H
#define OA_STG_MS20_MODEL_PATCH_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_ms20_model_patch.h  -  CSTGMS20ModelPatch's value-getter family:
 * 19 of 19 real weak-symbol candidates reconstructed via the STG
 * value-getter family's scripted instruction-pattern decoder -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 *
 * NOT the same class as the already-reconstructed CSTGMS20 (see
 * include/oa_stg_ms20.h) -- CSTGMS20ModelPatch is a distinct, higher-level
 * MS-20 voice-model patch component that owns the model-generator (MG,
 * i.e. LFO-like mod source) and voice-allocator-EG parameters, per its
 * MG- and VoiceAllocatorEG-prefixed parameter names; confirmed distinct via
 * word-boundary grep before starting, same precedent as
 * CSTGPolysix/CSTGPolysixModel and CSTGProgram/CSTGProgramSlot.
 *
 * Of the 26 pending Get*- and Set*-prefixed symbols this class's own address range
 * carries, 19 are real weak/COMDAT ctx-only value getters; the remaining
 * 7 are global ('T') linkage (GetMGAMSSourceAddress x3 overloads,
 * SetupComponentOffsets, GetPatchAMSSourceAddress,
 * GetAudioInputChannelLevels, GetVoiceModelAMSSourceAddress) -- all take
 * extra arguments beyond ctx, excluded up front by the linkage check.
 *
 * SIMPLEST dialect: every one of the 19 real candidates is a fixed-offset
 * field read directly off `this` -- zero ctx-dynamic-index methods, same
 * as CSTGEPModelPatch/CSTGVPMOsc.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-write -- MGFrequency/MGWaveform and their
 *     AMSIntensity/AMSIntModIntensity siblings.
 *   - Plain 8-bit field (signed or unsigned): .value only -- the
 *     AMSSource/AMSIntModSource selectors, MIDITempoSyncBaseNote/Times,
 *     VoiceAllocatorEG.
 *   - Bitfield extraction, same shift-then-mask shape as CSTGVPMOsc: two
 *     independent single-bit booleans packed in the SAME byte at offset
 *     0x6f7 -- bit 0 = MGKeySync (no shift), bit 1 = MGMIDITempoSync
 *     (`shr al,1` then `and eax,1`).
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGMS20ModelPatch {
	STGConvertedParam &GetMGFrequency(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGFrequencyAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGFrequencyAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGFrequencyAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGFrequencyAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGKeySync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGMIDITempoSync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGMIDITempoSyncBaseNote(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGMIDITempoSyncTimes(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGMIDITempoSyncTimesAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGMIDITempoSyncTimesAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGMIDITempoSyncTimesAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGMIDITempoSyncTimesAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGWaveform(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGWaveformAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGWaveformAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGWaveformAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGWaveformAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVoiceAllocatorEG(CSTGPatchMessageContext &ctx);
};

#endif
