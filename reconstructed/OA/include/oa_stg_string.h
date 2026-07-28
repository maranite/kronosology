// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_STRING_H
#define OA_STG_STRING_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_string.h  -  CSTGString's "value getter" family: 105
 * STGConvertedParam &Get*(CSTGPatchMessageContext &) methods (real ground
 * truth `.text+0x5b0e70`..`.text+0x5b1ae0`, all weak/COMDAT-sectioned --
 * see src/engine/stg_string_valuegetters.cpp for the full derivation).
 *
 * CSTGString is the STG (synth tone generator) "physical-modeled string"
 * patch component (plucked-string/KARMA-adjacent acoustic model, per
 * CSTGString's own many Pluck*, Pickup*, Harmonic*, Dispersion* parameter
 * names below). Declared here as a minimal, deliberately OPAQUE class --
 * same convention already established by CSTGProgramSlot/CSTGToneAdjust
 * (oa_global.h) for classes where only a subset of fields is confirmed:
 * no named members, no base class, no vtable modeled. Every method body
 * accesses its own field(s) via raw `(unsigned char *)this + offset`
 * arithmetic instead.
 *
 * Two DSP-computation outliers in this same real class -- GetPluckDelay/
 * GetPluckDelayAMSIntensity (`.text+0x187b70`/`.text+0x187bd0`, NOT part of
 * this weak/COMDAT cluster -- ordinary global-linkage symbols in the
 * merged plain `.text` section) -- convert a delay
 * parameter to a sample count via CSTGAudioBusManager's live sample rate
 * (`fmul`/`fistp` against a runtime float, not a plain field copy).
 * Deliberately excluded from this batch: genuine audio-DSP computation,
 * out of scope per this project's established DSP-fidelity policy.
 * GetNoiseSaturation (in-range, `.text+0x5b0e40`) is a THIRD outlier for
 * the same reason: a real `fyl2x`-based log2 dB-style conversion, not a
 * plain copy. Also excluded here for the same reason -- left pending.
 *
 * GetSubComponent(unsigned short) (`.text+0x5b0e00`, real __thiscall not
 * __regparm3 -- confirmed a genuinely different calling convention, not
 * just a different body shape) is a real method on this class too but
 * a completely different shape (branchy, returns a sub-object pointer,
 * not a value-getter) -- not part of this family, left pending.
 * GetId/GetName/GetNumParams/GetParamDescriptors/GetMessageHandlers/
 * GetValueGetters/GetNumSubComponents are the generic CSTGParamsOwner
 * reflection-API overrides (base-class virtual slots) -- a different
 * mechanism entirely, also left pending.
 *
 * Field-shape summary (each Get* transcribed via a scripted instruction-
 * pattern decoder, not by hand -- see the .cpp for the full methodology
 * note, same technique as CKGSeqBackupCommonParam/ModuleParam):
 *   - Plain 32-bit field (`mov eax,[eax+K]`): writes BOTH
 *     sValueGetterTemp.value and .displayValue to the same raw 32-bit
 *     value (float params: a raw bit-pattern copy, not a numeric
 *     conversion -- matches CSTGADSRBase's own established convention).
 *   - Plain 8-bit field (`movsx`/`movzx eax, BYTE [eax+K]`): writes only
 *     .value.
 *   - Packed boolean bitfield (`movzx` + `shr`/`and`): writes only
 *     .value. 5 discrete/enum 32-bit fields (PrePost/UseFilter/
 *     TableSelect selectors) are a confirmed real exception to the
 *     "32-bit implies dual-write" pattern -- write only .value too
 *     (ground truth genuinely omits the second store for these).
 *   - "AMS"-suffixed siblings for the Pickup*, MixerPickup* param group
 *     read through a per-call dynamic index (`ctx`'s own +0x4 field,
 *     scaled by a fixed per-group stride) rather than a fixed offset --
 *     modeled via the CtxIndex() helper in the .cpp.
 */

struct CSTGString {
	STGConvertedParam &GetDamping(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDampingAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDampingAMSIntensity1AMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDampingAMSIntensity1AMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDampingAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDampingStringTrackIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecay(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecayAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDispersion(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDispersionAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDispersionAMSIntensity1AMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDispersionAMSIntensity1AMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDispersionAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDispersionStringTrackIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDispersionType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPosition(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPositionAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPositionAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPositionScaling(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPressure(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPressureAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPressureAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPressureAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicPressureAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHarmonicUseExcitationPosition(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerNoiseBalance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerNoiseBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerNoiseBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerNoiseLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerNoiseLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerNoiseLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerNoisePhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPCMBalance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPCMBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPCMBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPCMLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPCMLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPCMLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPCMPhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPickupBalance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPickupBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPickupBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPickupLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPickupLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPickupLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerPickupPhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerStringBalance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerStringBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerStringBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerStringLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerStringLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerStringLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerStringPhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseCutoff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseCutoffAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseCutoffAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevelAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevelPhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevelPrePost(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNonlinearity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNonlinearityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNonlinearityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPCMLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPCMLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPCMLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPCMLevelAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPCMLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPCMLevelPhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPCMLevelPrePost(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickPosition(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickPositionAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickPositionAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickPositionScaling(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickPositionTone(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickupPosition(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickupPositionAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickupPositionAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPickupPositionScaling(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckDelayAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckLevelAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckLevelAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckLevelPhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckLevelPrePost(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckRandomAmt(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckRandomAmtAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckRandomAmtAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckRandomUseFilter(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckTableSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckWidth(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckWidthAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPluckWidthAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRelease(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReleaseAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
