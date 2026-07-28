// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_POLYSIX_MODEL_PATCH_H
#define OA_STG_POLYSIX_MODEL_PATCH_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_polysix_model_patch.h  -  CSTGPolysixModelPatch's value-getter
 * family: 48 of 48 real weak-symbol candidates reconstructed via the STG
 * value-getter family's scripted instruction-pattern decoder -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 *
 * NOT the same class as the already-reconstructed CSTGPolysix -- see
 * include/oa_stg_polysix.h -- CSTGPolysixModelPatch is the higher-level
 * patch component that owns the PWM/effect/volume/arpeggiator parameters
 * layered on top of the base analog-model oscillator, per its
 * PWMSpeed/EffectMode/Arpeggiator-prefixed parameter names; confirmed
 * distinct via word-boundary grep before starting, same precedent as
 * CSTGProgram/CSTGProgramSlot and CSTGMS20/CSTGMS20ModelPatch.
 *
 * Of the 53 pending Get*- and Set*-prefixed symbols this class's own
 * address range carries, 48 are real weak/COMDAT ctx-only value getters;
 * the remaining 5 are global ('T') linkage -- GetMGAMSSourceAddress,
 * GetVoiceLevelEstimate, SetupComponentOffsets,
 * GetPatchAMSSourceAddress, GetVoiceModelAMSSourceAddress -- all take
 * extra arguments beyond ctx or are runtime per-voice accessors, excluded
 * up front by the linkage check.
 *
 * SIMPLEST dialect: every one of the 48 real candidates is a fixed-offset
 * field read directly off `this` -- zero ctx-dynamic-index methods, same
 * class of dialect as CSTGEPModelPatch/CSTGVPMOsc/CSTGMS20ModelPatch.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-write -- PWMSpeed/EffectSpread/
 *     EffectSpeedIntensity/Volume/ArpeggiatorSpeed and their
 *     AMSIntensity/AMSIntModIntensity siblings.
 *   - Plain 8-bit field, signed: .value only -- almost every
 *     AMSSource/AMSIntModSource selector plus the Arpeggiator
 *     Mode/Range/MIDITempoSyncBaseNote group.
 *   - Plain 8-bit field, unsigned, no mask: .value only --
 *     GetArpeggiatorMIDITempoSyncTimes alone.
 *   - Bitfield extraction, same shift-then-mask shape as CSTGVPMOsc/
 *     CSTGMS20ModelPatch: FOUR independent single-bit booleans packed in
 *     the SAME byte at offset 0x4ac -- bit 0 = ArpeggiatorEnable (no
 *     shift), bit 1 = ArpeggiatorKeySync (`shr al,1`), bit 2 =
 *     ArpeggiatorMIDITempoSync (`shr al,2`), bit 3 = ArpeggiatorLatch
 *     (`shr al,3`) -- one more packed bit than either prior bitfield
 *     class (CSTGVPMOsc/CSTGMS20ModelPatch each had at most 2 in a byte).
 * No exceptions to the width-vs-dual-write rule found in this class --
 * every dword dual-writes, every byte (signed, unsigned, or bitfield)
 * single-writes. Zero outliers of any kind, a first full-clean-sweep
 * result matching CSTGPolysix/CSTGMS20/CSTGEPModelPatch's own earlier
 * zero-outlier batches.
 */

struct CSTGPolysixModelPatch {
	STGConvertedParam &GetArpeggiatorEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorEnableAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorEnableAMSSwitchMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorKeySync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorLatch(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorLatchAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorLatchAMSSwitchMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorMIDITempoSync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorMIDITempoSyncBaseNote(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorMIDITempoSyncTimes(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorMIDITempoSyncTimesAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorMIDITempoSyncTimesAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorMIDITempoSyncTimesAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorMIDITempoSyncTimesAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorModeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorModeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorRange(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorRangeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorRangeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorSpeedAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorSpeedAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorSpeedAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetArpeggiatorSpeedAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectModeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectModeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpeedIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpeedIntensityAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpeedIntensityAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpeedIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpeedIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpread(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpreadAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpreadAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpreadAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEffectSpreadAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPWMSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPWMSpeedAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPWMSpeedAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPWMSpeedAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPWMSpeedAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolume(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
