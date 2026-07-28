// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_FILTER_H
#define OA_STG_VPM_FILTER_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_filter.h  -  CSTGVPMFilter's value-getter family: all 3 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation. VPM
 * engine dual-filter patch component -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project. Distinct from the already-modeled
 * CSTGMultiFilter2Pole/CSTGSimple2Pole filter classes.
 *
 * Dialect: simplest yet -- every candidate a fixed-K field read directly
 * off this, zero ctx-dynamic-index methods.
 *
 * Field-shape summary:
 *   - Byte-equals-literal-constant boolean: GetRoutingValue -- cmp byte
 *     at plus 0x1f against the literal 3, sete al, movzx eax,al --
 *     single-write only. New variant of the family's established
 *     truth-value-test shape -- prior instances (CSTGAnalogSyncOsc's own
 *     GetRingModModulatorSelect/GetSubOscAudioInModeSelect) compared a
 *     dword against zero; this one compares a byte against a nonzero
 *     literal.
 *   - Plain 8-bit signed fields: single-write value only --
 *     GetFilterAOutputLevelAMSMode, GetFilterBOutputLevelAMSMode.
 * No dword fields in this class, so no dual-write instance either way.
 */

struct CSTGVPMFilter {
	STGConvertedParam &GetRoutingValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAOutputLevelAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBOutputLevelAMSMode(CSTGPatchMessageContext &ctx);
};

#endif
