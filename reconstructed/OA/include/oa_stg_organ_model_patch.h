// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_ORGAN_MODEL_PATCH_H
#define OA_STG_ORGAN_MODEL_PATCH_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_organ_model_patch.h  -  CSTGOrganModelPatch's "value getter"
 * family: 101 STGConvertedParam &Get*(CSTGPatchMessageContext &) methods,
 * second class reconstructed via the STG value-getter family's scripted
 * instruction-pattern decoder (see include/oa_stg_string.h for the pilot
 * class's full derivation and src/engine/stg_organ_model_patch_valuegetters.cpp
 * for this class's own decoder note).
 *
 * CSTGOrganModelPatch is the STG tonewheel-organ patch component (Hammond-
 * style drawbar/rotary-speaker model, per its many VC*, Rotary*, Perc*, Wheel*
 * parameter names below -- VC = "vibrato/chorus", Perc = percussion click,
 * Rotary/Wheel = Leslie-style rotary speaker simulation). Declared here as a
 * minimal, deliberately OPAQUE class, same convention as CSTGString: no
 * named members, no base class, no vtable modeled. Every method body
 * accesses its own field(s) via raw `(unsigned char *)this + offset`
 * arithmetic instead.
 *
 * Simpler dialect than CSTGString's: `this` stays in eax throughout (no
 * edx-based ctx-dynamic-index AMS sub-family here -- every AMSSource/
 * AMSIntensity/AMSMode sibling in this class reads a fixed per-field offset,
 * confirmed empirically, not assumed). One NEW field-shape not seen in the
 * CSTGString batch: a boolean NOT (`movzx eax,BYTE[eax+K]` + `xor eax,0x1` +
 * `movzx eax,al`, single .value-only write) -- GetPercLevelSwitch.
 *
 * 2 genuine outliers excluded (real ground truth `.text._ZN19CSTGOrganModelPatch...`
 * weak/COMDAT symbols, NOT part of the mechanical-copy decoder vocabulary):
 * GetRotaryHornMicDistance and GetRotaryRotorMicDistance both compute a real
 * `1.0f - field` via x87 (`fld1`; `fsub DWORD PTR [eax+K]`; `fst`/`fstp`), not
 * a plain field copy -- same rationale as CSTGString's GetNoiseSaturation
 * outlier (a numeric transform, however simple, is excluded from a batch
 * meant to be mechanically decoded, not hand-verified per method). Left
 * pending for a future batch.
 *
 * Also left pending (different mechanism, not part of this family, real
 * global-linkage 'T' symbols not weak/COMDAT 'W'): GetRequiredVoiceInfo
 * (CSTGVoiceInitialStateLinkedList* builder), GetVoiceLevelEstimate(CSTGVoice&),
 * GetPatchAMSSourceAddress(CSTGSlotVoiceData*, eAMSSource) -- all take extra
 * arguments beyond ctx and are not value-getters at all.
 *
 * Field-shape summary (each Get* transcribed via the scripted decoder, not
 * by hand):
 *   - Plain 32-bit field (`mov eax,[eax+K]`): writes BOTH .value and
 *     .displayValue (raw bit-pattern copy).
 *   - Plain 8-bit field (`movsx`/`movzx eax, BYTE [eax+K]`): writes only
 *     .value.
 *   - Boolean-NOT 8-bit field (`movzx` + `xor eax,0x1` + `movzx eax,al`):
 *     writes only .value (GetPercLevelSwitch, confirmed real -- ground truth
 *     stores the logical inverse of the raw stored byte).
 */

struct CSTGOrganModelPatch {
	STGConvertedParam &GetAmpGain(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpGainAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpGainAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpRotaryVersion(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpToneBass(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpToneMiddle(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpToneTreble(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEXModeEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExpressionAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExpressionLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExpressionMinimum(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExpressionMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyOffClickLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeyOnClickLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLeakageLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOutputLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOutputLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOutputLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOvertoneLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercDecaySwitch(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercDecaySwitchAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercDecaySwitchAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercEnableAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercEnableAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercFastDecayTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercHarmonicSwitch(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercHarmonicSwitchAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercHarmonicSwitchAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercLevelSwitch(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercLevelSwitchAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercLevelSwitchAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercLoudDBAttenuation(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercLoudLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercSlowDecayTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercSoftLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchBendDown(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPitchBendUp(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryFast(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryFastAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryFastAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryFastOverridesStop(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornDownTransit(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornFastSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornMicSpread(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornRotorBalance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornSlowSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornStartTransit(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornStopPhase(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornStopTransit(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryHornUpTransit(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryOffOutput(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryOn(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryOnAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryOnAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryRotorDownTransit(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryRotorFastSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryRotorMicSpread(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryRotorSlowSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryRotorStartTransit(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryRotorStopPhase(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryRotorStopTransit(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryRotorUpTransit(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotarySpeakerSim(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotarySpeakerSimType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryStop(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryStopAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryStopAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryWetDry(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryWetDryAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRotaryWetDryAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSplitEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSplitEnableAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSplitEnableAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCDepth(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCDepthAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCDepthAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCLowerEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCLowerEnableAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCLowerEnableAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCMix(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCMixAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCMixAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCSpeedAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCSpeedAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCTrim(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCTypeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCTypeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCUpperEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCUpperEnableAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVCUpperEnableAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWheelBrakeEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWheelBrakeEnableAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWheelBrakeEnableAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWheelBrakeSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWheelType(CSTGPatchMessageContext &ctx);
};

#endif
