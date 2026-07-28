// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PITCH_MOD_COMMON_H
#define OA_STG_PITCH_MOD_COMMON_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_pitch_mod_common.h  -  CSTGPitchModCommon's value-getter family:
 * all 5 real weak-symbol ctx-only candidates reconstructed, zero
 * outliers -- see include/oa_stg_string.h for the pilot class's full
 * derivation. CSTGPitchModCommon is a shared LFO-based pitch-modulation
 * building block, distinct from the already-modeled CSTGPitchMod --
 * confirmed via word-boundary grep before starting: only two incidental
 * mentions in the already-modeled include/oa_stg_pitch_mod.h and
 * include/oa_stg_pitch_mod_osc.h header prose, no real struct or ctor
 * anywhere in this project. Method names overlap heavily with
 * CSTGPitchMod's own LFO group -- GetLFOAmount, GetLFOAMSSource,
 * GetLFOAMSIntensity, GetJSYToLFOAmount -- but the field offsets below
 * are entirely different, confirming these are two separate classes
 * sharing a common naming convention rather than one class reused.
 *
 * Dialect: simplest yet -- every candidate a fixed-K field read directly
 * off this, zero ctx-dynamic-index methods, despite CSTGPitchMod's own
 * sibling fields of the same name being ctx-indexed.
 *
 * Field-shape summary:
 *   - Plain 8-bit signed field: single-writes value only -- LFOAMSSource.
 *   - Plain 8-bit unsigned field, no shift/mask: single-writes value
 *     only -- LFOSelect, the by-now-established "unsigned non-bitfield
 *     byte" variant first seen on CSTGPolysixMG::GetMIDITempoSyncTimes.
 *   - Plain 32-bit field: dual-writes value and displayValue --
 *     LFOAmount, JSYToLFOAmount, LFOAMSIntensity.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGPitchModCommon {
	STGConvertedParam &GetJSYToLFOAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLFOSelect(CSTGPatchMessageContext &ctx);
};

#endif
