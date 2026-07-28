// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_TG92_OSC_H
#define OA_STG_VPM_TG92_OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_tg92_osc.h  -  CSTGVPMTG92Osc's value-getter family: all
 * 9 real weak-symbol ctx-only candidates reconstructed, zero outliers
 * -- see include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGVPMTG92Osc is the TG92-style FM operator patch component within
 * the VPM engine -- its own Volume group plus OscOnOff -- confirmed
 * genuinely fresh via a word-boundary grep before starting, no
 * pre-existing struct or ctor anywhere in this project.
 *
 * Dialect: the simplest yet -- every one of the 9 real candidates is a
 * fixed-K field read directly off this, zero ctx-dynamic-index methods
 * of any kind.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes .value and .displayValue.
 *   - Plain 8-bit field, always movsx-signed in this class: single-writes
 *     .value only.
 *   - GetOscOnOff alone is a single-bit boolean extracted from byte
 *     0x9b, bit 0 -- no shift needed -- single-write only.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGVPMTG92Osc {
	STGConvertedParam &GetOscOnOff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolume(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeEGSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeVelocitySensitivity(CSTGPatchMessageContext &ctx);
};

#endif
