// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PITCH_MOD_BASE_H
#define OA_STG_PITCH_MOD_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_pitch_mod_base.h  -  CSTGPitchModBase's value-getter family:
 * both real weak-symbol ctx-only candidates reconstructed, zero outliers
 * -- see include/oa_stg_string.h for the pilot class's full derivation.
 * Base ribbon/aftertouch pitch-bend-slope patch component -- confirmed
 * genuinely fresh via a word-boundary grep before starting (prior
 * appearances of the name in this project were only incidental
 * sibling-class mentions in CSTGPitchMod's own header comments, not real
 * references). Distinct from the already-modeled CSTGPitchMod/
 * CSTGPitchModOsc/CSTGPitchModCommon/CSTGPitchModCommonPlusAMS/
 * CSTGPitchModOscBase siblings.
 *
 * Dialect: both candidates fixed-K fields read directly off this, zero
 * ctx-dynamic-index methods.
 *
 * Field-shape summary:
 *   - Plain 32-bit fields: dual-write value and displayValue --
 *     GetSlope, GetRibbon.
 * No byte fields in this class, so no single-write instance either way.
 */

struct CSTGPitchModBase {
	STGConvertedParam &GetSlope(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRibbon(CSTGPatchMessageContext &ctx);
};

#endif
