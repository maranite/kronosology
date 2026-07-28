// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_POLYSIX_H
#define OA_STG_POLYSIX_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_polysix.h  -  CSTGPolysix's "value getter" family: 71
 * STGConvertedParam &Get*(CSTGPatchMessageContext &) methods, fifth class
 * reconstructed via the STG value-getter family's scripted
 * instruction-pattern decoder -- see include/oa_stg_string.h for the pilot
 * class's full derivation and src/engine/stg_polysix_valuegetters.cpp for
 * this class's own decoder note. Not to be confused with
 * CSTGPolysixModel in oa_engine_init.h -- an unrelated, already-modeled
 * voice-model wrapper class with a similar name.
 *
 * CSTGPolysix is the STG analog-modeled Korg Polysix synth-voice patch
 * component, per its Osc*, Filter*, MG* and ExtMod* parameter names below
 * -- MG = mod generator, ExtMod = the Polysix's external CV/gate-style
 * modulation input pair. Declared here as a minimal, deliberately OPAQUE
 * class, same convention as the rest of the family: no named members, no
 * base class, no vtable modeled.
 *
 * Mixed dialect: most methods keep this in eax throughout, same as
 * CSTGOrganModelPatch, but the ExtMod*Intensity/ExtModSource group reads
 * a per-call dynamic index from ctx's own plus-0x4 field -- a NEW SIB
 * scale relative to CSTGMS20's own two ctx-index shapes: the usual
 * stride-5 mov edx from edx-plus-0x4 plus lea edx from edx-plus-edx-times-4
 * premultiply, but the field load itself carries a times-4 SIB scale
 * -- eax-plus-edx-times-4-plus-K -- giving effective stride 20, not 10.
 * Modeled via the same CtxIndex helper as CSTGString/CSTGMS20, passing
 * the already-fully-reduced stride.
 *
 * Zero outliers in this class.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: writes BOTH .value and .displayValue.
 *   - Plain 8-bit field (signed or unsigned, both observed): writes only
 *     .value.
 *   - ctx-indexed 32-bit field (the ExtMod*Intensity group, stride 20):
 *     dual write, same as the fixed-offset 32-bit case.
 *   - ctx-indexed 32-bit field (GetExtModSource alone, stride 20): single
 *     write -- a discrete source-selector field, consistent with every
 *     other Source-suffixed method in this family being single-write
 *     regardless of stored width -- GetExtModSource is a confirmed real
 *     exception to the "32-bit implies dual-write" rule, same class of
 *     exception already seen in CSTGString/CSTGOrganModelPatch for other
 *     discrete/enum selector fields.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGPolysix {
	STGConvertedParam &GetAmpAttenuator(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpAttenuatorAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpAttenuatorAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpAttenuatorAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpAttenuatorAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpEGMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpEGModeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmpEGModeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAnalog(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModAmpGainIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModFilterCutoffIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModMGLevelIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModOscPulseWidthIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetExtModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterCutoff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterCutoffAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterCutoffAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterCutoffAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterCutoffAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterEGIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterEGIntensityAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterEGIntensityAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterEGIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterEGIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterKeyboardTrack(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterKeyboardTrackAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterKeyboardTrackAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterKeyboardTrackAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterKeyboardTrackAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterResonance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterResonanceAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterResonanceAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterResonanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterResonanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGDestination(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGDestinationAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGDestinationAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGLevelAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGLevelAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMGLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscOctave(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscOctaveAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscOctaveAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscPulseWidth(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscPulseWidthAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscPulseWidthAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscPulseWidthAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscPulseWidthAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTranspose(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTransposeAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTransposeAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTransposeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTransposeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTune(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTuneAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTuneAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTuneAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscTuneAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscVibratoIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscVibratoIntensityAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscVibratoIntensityAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscVibratoIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscVibratoIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscWaveform(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscWaveformAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscWaveformAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOsc(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
