// SPDX-License-Identifier: GPL-2.0
#ifndef OA_WAVE_MOTION_OSC_H
#define OA_WAVE_MOTION_OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_wave_motion_osc.h  -  CWaveMotionOsc's value-getter family: 23 of 23
 * real weak-symbol candidates reconstructed via the STG value-getter
 * family's scripted instruction-pattern decoder -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CWaveMotionOsc is NOT itself STG-prefixed but participates in the
 * exact same convention -- same sValueGetterTemp sink, same
 * CSTGPatchMessageContext& signature, same weak/COMDAT per-symbol .text
 * sections -- same precedent as CPianoOsc. It is the WaveMotion physical
 * -model oscillator patch component: Decay/Release/HammerWidth/Slope/
 * Ribbon are its own physical-model shape parameters, KeyDownNoiseLevel/
 * KeyUpNoiseLevel/NoiseTone are its keyboard-noise-layer parameters.
 *
 * Of the 27 pending Get*- and Set*-prefixed symbols this class's own
 * address range carries, 23 are real weak/COMDAT ctx-only value getters;
 * the remaining 4 are global ('T') linkage -- SetSoundData, SetTableData,
 * SetRelRelease, SetWaveMotionOscData -- all take extra arguments beyond
 * ctx, excluded up front by the linkage check.
 *
 * SIMPLEST dialect: every one of the 23 real candidates is a fixed-offset
 * field read directly off `this` -- zero ctx-dynamic-index methods, same
 * class of dialect as CSTGEPModelPatch/CSTGVPMOsc/CSTGMS20ModelPatch/
 * CSTGPolysixModelPatch. Field-shape summary:
 *   - Plain 32-bit field: dual-write -- OscLevel/Decay/Release/
 *     KeyDownNoiseLevel/KeyUpNoiseLevel/NoiseTone/HammerWidth/Slope/
 *     Ribbon and their AMSIntensity siblings. Several dword fields sit
 *     at unaligned offsets (e.g. Decay at +0x25) -- an ordinary unaligned
 *     32-bit x86 load, no special handling needed.
 *   - Plain 8-bit field, signed: .value only -- every AMSSource selector.
 * No AMSIntModSource/AMSIntModIntensity siblings exist for this class
 * (unlike CSTGPolysixModelPatch/CSTGPolysix) -- only the plain
 * AMSSource/AMSIntensity pair per parameter. No exceptions to the
 * width-vs-dual-write rule found. Zero outliers of any kind, a full
 * clean sweep matching CSTGPolysix/CSTGMS20/CSTGEPModelPatch/
 * CSTGPolysixModelPatch's own zero-outlier batches.
 */

struct CWaveMotionOsc {
	STGConvertedParam &GetDecay(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHammerWidth(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHammerWidthAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHammerWidthAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyDownNoiseLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyDownNoiseLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyDownNoiseLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyUpNoiseLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyUpNoiseLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyUpNoiseLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseTone(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseToneAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseToneAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRelease(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRibbon(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlope(CSTGPatchMessageContext &ctx);
};

#endif
