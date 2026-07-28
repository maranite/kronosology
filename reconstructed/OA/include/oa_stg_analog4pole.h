// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_ANALOG4POLE_H
#define OA_STG_ANALOG4POLE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_analog4pole.h  -  CSTGAnalog4Pole's value-getter family: all 7
 * real weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGAnalog4Pole is the analog-modeled 4-pole dual-filter patch component
 * -- Routing, and the FilterA/FilterB Pan values with their own AMS
 * source/intensity siblings -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project. Confirmed DISTINCT from the already-done
 * CSTGAnalog4PoleBase -- same name-collision precedent as
 * CSTGPolysix/CSTGPolysixModel.
 *
 * Dialect: simplest fixed-K-off-this shape throughout, zero ctx-index
 * methods, all fields at unusually large offsets (up to +0x12c) reflecting
 * this class's own big struct layout.
 *
 * Field-shape summary:
 *   - GetRoutingValue: plain signed 8-bit field at +0x1f, single-write.
 *   - Plain 32-bit fields: dual-write .value and .displayValue --
 *     FilterAPan at +0x11b, FilterAPanAMSIntensity at +0x11f, FilterBPan
 *     at +0x124, FilterBPanAMSIntensity at +0x128.
 *   - Plain signed 8-bit fields: single-write .value only --
 *     FilterAPanAMSSource at +0x123, FilterBPanAMSSource at +0x12c.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGAnalog4Pole {
	STGConvertedParam &GetRoutingValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAPan(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAPanAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterAPanAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBPan(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBPanAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFilterBPanAMSIntensity(CSTGPatchMessageContext &ctx);
};

#endif
