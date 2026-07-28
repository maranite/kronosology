// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_ANALOG4POLEBASE_H
#define OA_STG_ANALOG4POLEBASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_analog4pole_base.h  -  CSTGAnalog4PoleBase's "value getter"
 * family: 74 STGConvertedParam &Get*(CSTGPatchMessageContext &) methods,
 * fourth class reconstructed via the STG value-getter family's scripted
 * instruction-pattern decoder -- see include/oa_stg_string.h for the pilot
 * class's full derivation and src/engine/stg_analog4pole_base_valuegetters.cpp
 * for this class's own decoder note.
 *
 * CSTGAnalog4PoleBase is the STG analog-modeled 4-pole filter base patch
 * component (dual FilterA/FilterB layout, per its many FilterA* and
 * FilterB* parameter names below). Declared here as a minimal, deliberately OPAQUE
 * class, same convention as CSTGString/CSTGOrganModelPatch/CSTGMS20: no
 * named members, no base class, no vtable modeled. Every method body
 * accesses its own field via raw this-plus-offset arithmetic instead.
 *
 * Simple dialect, no ctx-dynamic-index sub-family in this class -- this
 * stays in eax throughout every method. Field shapes match
 * CSTGOrganModelPatch's vocabulary exactly: plain dword, signed/unsigned
 * byte, and packed bitfield via movzx plus an optional shr plus and mask
 * -- GetFilterBBypass, GetFilterBLink and GetFilterB4PoleResType shift by
 * 1/2/3 bits out of the same packed byte at offset 0x21 that
 * GetFilterABypass masks unshifted.
 *
 * 2 genuine outliers excluded -- real ground truth weak/COMDAT symbols,
 * NOT part of the mechanical-copy decoder vocabulary: GetFilterALeakage
 * and GetFilterBLeakage both compute a real ordered floating-point
 * equal-to-one-point-zero test via x87 fld1 plus fucomip, then negate the
 * result -- effectively "field is not exactly 1.0" as a boolean -- not a
 * plain field copy. New outlier shape for this family, same rationale as
 * CSTGString's GetNoiseSaturation and CSTGOrganModelPatch's rotary-mic
 * distance pair: a numeric comparison, however simple, is excluded from a
 * batch meant to be mechanically decoded, not hand-verified per method.
 * Left pending for a future batch.
 *
 * Also left pending -- different mechanism entirely, real global-linkage
 * symbol not weak/COMDAT, and a completely different signature (takes an
 * unsigned short, not a CSTGPatchMessageContext reference): GetSubComponent.
 *
 * Field-shape summary:
 *   - Plain 32-bit field (mov eax from eax-plus-offset): writes BOTH
 *     .value and .displayValue.
 *   - Plain 8-bit field (movsx/movzx eax, byte from eax-plus-offset):
 *     writes only .value.
 *   - Packed boolean bitfield (movzx plus optional shr plus and): writes
 *     only .value.
 */

struct CSTGAnalog4PoleBase {
	STGConvertedParam &GetFilterABandpass1(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterABandpass2(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterABypass(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterADry1(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterADry2(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAEGAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAEGAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAEGIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAEGSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAEGVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFilterType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFilterType1(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFilterType2(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFreqAMS1Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFreqAMS1IntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFreqAMS1IntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFreqAMS1Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFreqAMS2Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFreqAMS2Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFrequency(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAFrequencyFine(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAHighpass1(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAHighpass2(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAKeytrackIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterALFOAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterALFOAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterALFOIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterALFOJSminusYIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterALFOSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterALowpass1(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterALowpass2(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAOutputLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAOutputLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAOutputLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAResonance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAResonanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAResonanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterATrim(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterATypeXfade(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterATypeXfadeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterATypeXfadeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterATypeXfadeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterATypeXfadeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterB4PoleResType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBBypass(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBEGAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBEGAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBEGIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBEGSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBEGVelocity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFilterType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFreqAMS1Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFreqAMS1IntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFreqAMS1IntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFreqAMS1Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFreqAMS2Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFreqAMS2Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFrequency(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBFrequencyFine(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBKeytrackIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBLFOAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBLFOAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBLFOIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBLFOJSminusYIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBLFOSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBLink(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBLinkCutoffOffset(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBOutputLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBOutputLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBOutputLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBResonance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBResonanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBResonanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBTrim(CSTGPatchMessageContext &ctx);
};

#endif
