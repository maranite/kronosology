// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_SIMPLE_AMS_MIXER_H
#define OA_STG_SIMPLE_AMS_MIXER_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_simple_ams_mixer.h  -  CSTGSimpleAMSMixer's value-getter family:
 * all 5 real weak-symbol ctx-only candidates reconstructed, zero outliers
 * -- see include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGSimpleAMSMixer is a small two-input AMS modulation mixer patch
 * component -- a mixer type selector plus two independent source/amount
 * modulation legs -- confirmed genuinely fresh via a word-boundary grep
 * before starting, no pre-existing struct or ctor anywhere in this
 * project.
 *
 * Dialect: the simplest yet -- every candidate a fixed-K field read
 * directly off this, zero ctx-dynamic-index methods.
 *
 * Field-shape summary:
 *   - Plain 8-bit signed field: single-writes value only -- Type,
 *     SourceA, SourceB.
 *   - Plain 32-bit field: dual-writes value and displayValue --
 *     AmountA, AmountB.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGSimpleAMSMixer {
	STGConvertedParam &GetAmountA(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAmountB(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSourceA(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSourceB(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetType(CSTGPatchMessageContext &ctx);
};

#endif
