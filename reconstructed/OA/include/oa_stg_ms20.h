// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_MS20_H
#define OA_STG_MS20_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_ms20.h  -  CSTGMS20's "value getter" family:
 * 90 STGConvertedParam &Get*(CSTGPatchMessageContext &) methods, third
 * class reconstructed via the STG value-getter family's scripted
 * instruction-pattern decoder (see include/oa_stg_string.h for the pilot
 * class's full derivation and src/engine/stg_ms20_valuegetters.cpp for
 * this class's own decoder note).
 *
 * CSTGMS20 is the STG analog-modeled MS-20 synth-voice patch component
 * (Korg MS-20-style dual-VCO/VCF/ESP model, per its Vco1*, Vco2*, Hpf*,
 * Lpf*, Esp* parameter names below -- Hpf/Lpf = high/low-pass filter,
 * Esp = the MS-20's External Signal Processor input). Declared here as a
 * minimal, deliberately OPAQUE class, same convention as CSTGString and
 * CSTGOrganModelPatch: no named members, no base class, no vtable modeled.
 * Every method body accesses its own field(s) via raw
 * `(unsigned char *)this + offset` arithmetic instead.
 *
 * Mixed dialect: most methods keep `this` in eax throughout, same as
 * CSTGOrganModelPatch, but this class ALSO has a real ctx-dynamic-index
 * sub-family (CSTGString's own Pickup*, MixerPickup* mechanism), reading
 * ctx's own +0x4 field as a per-call slot index. Two index-scaling shapes
 * confirmed here, both new relative to CSTGString's own stride-5/stride-32
 * cases:
 *   - `mov edx,[edx+0x4]` + `lea edx,[edx+edx*4]` (stride 5) + a SIB-scaled
 *     field load `[eax+edx*2+K]` -- effective stride 10 (the lea's own
 *     stride-5 result gets a SECOND x2 multiply folded into the load's own
 *     addressing mode, not spelled out as a separate instruction). Used by
 *     the Standard*, Mixer* AMSSource/AMSIntensity sibling group.
 *   - `mov edx,[edx+0x4]` with NO lea at all, field load `[eax+edx*1+K]`
 *     -- the raw per-call index used directly as a 1-byte stride (no
 *     premultiply). Used by GetInputJack alone.
 * Modeled uniformly via the same CtxIndex(ctx, off, stride) helper as
 * CSTGString, with the already-fully-reduced per-unit stride (10 or 1)
 * passed in directly -- see the .cpp for the helper.
 *
 * All 90 real weak-symbol candidates in the class's address range parsed
 * cleanly this batch -- no outliers to exclude for CSTGMS20 (unlike
 * CSTGString's 3 and CSTGOrganModelPatch's 2). Left pending (different
 * mechanism, real global-linkage 'T' symbols not weak/COMDAT 'W', extra
 * arguments beyond ctx): GetSubRateExtModSourceAddress(CSTGVoice*,
 * eSTGMS20ExtModSource), GetAudioRateExtModSourceAddress(
 * STGMS20AudioRateParamsSlice*, eSTGMS20ExtModSource).
 *
 * Field-shape summary:
 *   - Plain 32-bit field (`mov eax,[eax+K]`): writes BOTH .value and
 *     .displayValue.
 *   - Plain 8-bit field (`movsx eax, BYTE [eax+K]`, always signed in this
 *     class -- no movzx 8-bit fields observed): writes only .value.
 *   - ctx-indexed 32-bit/8-bit fields (see above): same dual/single-write
 *     rule as the fixed-offset case, by width.
 *   Unlike CSTGOrganModelPatch, no discrete/enum 32-bit single-write
 *   exception and no boolean-NOT shape were found in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGMS20 {
	STGConvertedParam &GetAnalog(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspCVAdjust(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspCVAdjustAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspCVAdjustAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspHighCutFreq(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspHighCutFreqAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspHighCutFreqAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspLowCutFreq(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspLowCutFreqAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspLowCutFreqAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspSignalLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspSignalLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspSignalLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspThreshold(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspThresholdAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEspThresholdAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModASource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModAtoAmp(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModAtoHpfFc(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModAtoLpfFc(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModAtoVco1Pw(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModAtoVco2Pitch(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModBSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModBtoHpfFc(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModBtoHpfTExt(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModBtoLpfFc(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModBtoLpfTExt(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModBtoVcoTExt(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFineTune(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFineTuneAMSIntAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFineTuneAMSIntAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFineTuneAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFineTuneAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqModEg1Ext(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqModMgTExt(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHpfCutoff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHpfEg2Ext(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHpfMgText(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHpfPeak(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetInputJack(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLpfCutoff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLpfEg2Ext(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLpfMgText(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLpfPeak(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixer1LevelA(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixer1LevelB(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixer2LevelA(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixer2LevelB(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerAMSIntAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerAMSIntAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMixerAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMomentarySwAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMomentarySwAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPortamento(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStandardAMSIntAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStandardAMSIntAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStandardAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStandardAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTranspose(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTransposeAMSIntAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTransposeAMSIntAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTransposeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTransposeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTriggerOn(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco1Level(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco1PulseWidth(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco1Scale(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco1ScaleAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco1ScaleAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco1Waveform(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco1WaveformAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco1WaveformAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2Level(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2Pitch(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2PitchAMSIntAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2PitchAMSIntAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2PitchAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2PitchAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2Scale(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2ScaleAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2ScaleAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2Waveform(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2WaveformAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVco2WaveformAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolume(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWheelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWheelAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
