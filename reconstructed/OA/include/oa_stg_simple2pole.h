// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_SIMPLE2POLE_H
#define OA_STG_SIMPLE2POLE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_simple2pole.h  -  CSTGSimple2Pole's value-getter family: all
 * 13 real weak-symbol ctx-only candidates reconstructed, zero outliers
 * -- see include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGSimple2Pole is the STG simple 2-pole filter patch component --
 * Frequency/Resonance/Trim plus a two-level Freq AMS modulation chain,
 * per its own field names below -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * Dialect: the simplest yet -- every one of the 13 real candidates is a
 * fixed-K field read directly off this, zero ctx-dynamic-index methods
 * of any kind despite the FreqAMS1IntensityAMSSource/AMSIntensity
 * naming implying a second level of modulation nesting -- reconfirmed
 * via direct disassembly to be a plain fixed offset, same lesson as
 * CSTGMS20EG's own naming quirk.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes .value and .displayValue.
 *   - Plain 8-bit field, always movsx-signed in this class: single-writes
 *     .value only.
 *   - GetBypass alone is a single-bit boolean extracted from byte 0xd,
 *     bit 0 (no shift needed) -- `movzx eax, BYTE [this+0xd]; and eax,1`
 *     -- single-write only.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGSimple2Pole {
	STGConvertedParam &GetBypass(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1IntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1IntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS1Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS2Intensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFreqAMS2Source(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequency(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResonanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTrim(CSTGPatchMessageContext &ctx);
};

#endif
