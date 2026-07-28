// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PIANO_LPF_H
#define OA_STG_PIANO_LPF_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_piano_lpf.h  -  CSTGPianoLPF's value-getter family: all 9 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGPianoLPF is the acoustic-piano voice-model's lowpass-filter patch
 * component -- Frequency, LidPosition, KeyTrackIntensity, and a
 * two-level AMS modulation group, per its own method names below --
 * confirmed genuinely fresh via a word-boundary grep before starting, no
 * pre-existing struct or ctor anywhere in this project.
 *
 * Dialect: simplest fixed-K-off-this shape throughout, zero ctx-index
 * methods, despite GetFreqAMS1IntensityAMSSource/
 * GetFreqAMS1IntensityAMSIntensity's own naming implying a second
 * modulation level -- reconfirmed via direct disassembly to be plain
 * fixed offsets, same lesson as CSTGMS20EG's and CSTGSimple2Pole's own
 * earlier naming quirks.
 *
 * 2 pending symbols excluded up front, not fed to the decoder:
 * GetSubComponent(unsigned short) is the same different-signature
 * sub-object-accessor outlier shape as CSTGString's/
 * CSTGAnalog4PoleBase's own. SetupComponentOffsets(CSTGPatch*, ...) is
 * global ('T') linkage with extra args beyond ctx -- the standard
 * extra-args exclusion.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes .value and .displayValue --
 *     Frequency, FreqKeyTrackIntensity, FreqAMS1Intensity,
 *     FreqAMS2Intensity, FreqAMS1IntensityAMSIntensity, LidPosition.
 *   - Plain 8-bit signed field: single-writes .value only --
 *     FreqAMS1Source, FreqAMS2Source, FreqAMS1IntensityAMSSource.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGPianoLPF {
	STGConvertedParam &GetFrequency(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLidPosition(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS2Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS2Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqKeyTrackIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1IntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1IntensityAMSIntensity(CSTGPatchMessageContext &ctx);
};

#endif
