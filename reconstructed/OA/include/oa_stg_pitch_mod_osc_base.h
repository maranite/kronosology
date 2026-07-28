// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PITCH_MOD_OSC_BASE_H
#define OA_STG_PITCH_MOD_OSC_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_pitch_mod_osc_base.h  -  CSTGPitchModOscBase's value-getter
 * family: all 3 real weak-symbol ctx-only candidates reconstructed, zero
 * outliers -- see include/oa_stg_string.h for the pilot class's full
 * derivation. Base transpose/tune/octave oscillator-tuning patch
 * component -- confirmed genuinely fresh via a word-boundary grep before
 * starting (prior appearances of the name in this project were only
 * incidental sibling-class mentions in CSTGPitchMod's and
 * CSTGPitchModOsc's own header comments, not real references). Distinct
 * from the already-modeled CSTGPitchModOsc/CSTGPitchModCommon/
 * CSTGPitchModCommonPlusAMS/CSTGPitchModBase siblings.
 *
 * Dialect: simplest yet -- every candidate a fixed-K field read directly
 * off this, zero ctx-dynamic-index methods.
 *
 * Field-shape summary:
 *   - Plain 8-bit signed fields: single-write value only -- GetTranspose,
 *     GetOctave.
 *   - Plain 32-bit field: dual-writes value and displayValue -- GetTune.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGPitchModOscBase {
	STGConvertedParam &GetTranspose(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTune(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOctave(CSTGPatchMessageContext &ctx);
};

#endif
