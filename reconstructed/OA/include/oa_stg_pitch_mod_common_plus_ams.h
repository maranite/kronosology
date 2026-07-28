// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PITCH_MOD_COMMON_PLUS_AMS_H
#define OA_STG_PITCH_MOD_COMMON_PLUS_AMS_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_pitch_mod_common_plus_ams.h  -  CSTGPitchModCommonPlusAMS's
 * value-getter family: both 2 real weak-symbol ctx-only candidates
 * reconstructed, zero outliers -- see include/oa_stg_string.h for the
 * pilot class's full derivation. Picked up alongside the just-modeled
 * CSTGPitchModCommon in the same batch since its own real candidate set
 * fell out of the identical nm sweep -- a distinct, previously-unmodeled
 * sibling class that adds one extra AMS modulation leg on top of
 * CSTGPitchModCommon's own fields, confirmed genuinely fresh via
 * word-boundary grep before starting.
 *
 * Dialect: both candidates fixed-K fields read directly off this, zero
 * ctx-dynamic-index methods.
 *
 * Field-shape summary:
 *   - Plain 8-bit signed field: single-writes value only -- AMSSource.
 *   - Plain 32-bit field: dual-writes value and displayValue --
 *     AMSIntensity.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGPitchModCommonPlusAMS {
	STGConvertedParam &GetAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
