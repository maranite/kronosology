// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_OSC_H
#define OA_STG_VPM_OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_osc.h  -  CSTGVPMOsc's value-getter family: 44 of 44 real
 * weak-symbol candidates reconstructed via the STG value-getter family's
 * scripted instruction-pattern decoder -- see include/oa_stg_string.h for
 * the pilot class's full derivation. CSTGVPMOsc is the STG variable-phase-
 * modulation (FM/ring-mod/waveshaper) oscillator patch component -- per
 * its FMInputLevel*, RingMod*, Waveshaper*, Feedback* parameter names.
 *
 * Of the 61 total Get*- and Set*-prefixed symbols this class's own address range
 * carries, 45 are weak/COMDAT with the exact ctx-only mangled suffix; one
 * of those 45, GetSubComponent(unsigned short), is the same
 * `__thiscall`-style sub-object-accessor outlier seen before in
 * CSTGString/CSTGAnalog4PoleBase, excluded up front by its different
 * argument signature. The other 16 candidates are global ('T') linkage
 * (SetOscUsed, SetUsedFlags, SetDCCutCoeffs, SetupComponentOffsets) -- a
 * setter/init mechanism, not part of this family.
 *
 * SIMPLEST dialect: every one of the 44 real candidates is a fixed-offset
 * field read directly off `this` -- zero ctx-dynamic-index methods of any
 * kind, same as CSTGEPModelPatch.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-write (.value AND .displayValue) -- every
 *     Level/Intensity/Cutoff/Drive/Offset/Volume/Crossfade field.
 *   - Plain 8-bit (signed or unsigned) or 16-bit (signed) field: .value
 *     only -- the AMSSource/AMSIntModSource/Mode/Table/EGSelect selector
 *     fields and the coarse/fine tuning fields.
 *   - NEW: bitfield extraction via shift-then-mask, `(byte >> N) & 0x1` --
 *     four independent single-bit boolean flags packed into the SAME
 *     byte at offset 0x1f: bit 0 = OscOnOff (shift 0, i.e. plain
 *     `& 0x1` with no shift instruction at all), bit 1 =
 *     UseCommonPitchMod, bit 2 = WaveshaperDriveKeySlopeHighOnly, bit 3 =
 *     FeedbackPrePost. This extends the family's existing mask-only
 *     bitfield shape (CPianoOsc's `& 0x3`, no shift) with an explicit
 *     `shr al,N` before the `and eax,1` for bits other than 0 -- modeled
 *     directly as `(*(unsigned char *)(base+K) >> N) & 0x1`, no new
 *     helper needed.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGVPMOsc {
	STGConvertedParam &GetFMInputLevel1(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFMInputLevel2(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackPrePost(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqOffsetCoarse(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqOffsetFine(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqRatioCoarse(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqRatioFine(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetInitialPhase(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLPFCutoff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscOnOff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPhaseSync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchAMS1IntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchAMS1IntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchAMS1Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchAMS1Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchAMS2Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchAMS2Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModCrossfade(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModCrossfadeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModCrossfadeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUseCommonPitchMod(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolume(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeEGSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeVelocitySensitivity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperDrive(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperDriveAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperDriveAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperDriveKeySlope(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperDriveKeySlopeHighOnly(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperHPFCutoff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperOffset(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperOffsetAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperOffsetAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperOutputLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveshaperTable(CSTGPatchMessageContext &ctx);
};

#endif
