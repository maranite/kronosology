// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_3BAND_EQ_BASE_H
#define OA_STG_3BAND_EQ_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_3band_eq_base.h  -  CSTG3BandEQBase's value-getter family: all 6
 * of its raw pending weak symbols are real ctx-only candidates, a 100%
 * hit rate and zero outliers -- see include/oa_stg_string.h for the pilot
 * class's full derivation. CSTG3BandEQBase is the STG 3-band parametric
 * EQ patch component base -- input trim, bypass, and the low/mid/high
 * band gain plus mid frequency controls -- confirmed genuinely fresh via
 * a word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * Dialect: the simplest yet -- zero ctx-dynamic-index methods at all.
 * InputTrimValue/LowGainValue/MidFreqValue/MidGainValue/HighGainValue are
 * all plain fixed-K dwords, dual-writing .value and .displayValue.
 * BypassValue is a plain fixed-K mask-only bitfield -- bit 0 of one byte,
 * no shift instruction -- unsigned, single-write only, the family's
 * established "32-bit does not always imply dual-write, discrete/enum
 * fields single-write" rule extended to a boolean bit test. No new
 * decoder shapes needed for this class.
 */

struct CSTG3BandEQBase {
	STGConvertedParam &GetBypassValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetInputTrimValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLowGainValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMidFreqValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMidGainValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHighGainValue(CSTGPatchMessageContext &ctx);
};

#endif
