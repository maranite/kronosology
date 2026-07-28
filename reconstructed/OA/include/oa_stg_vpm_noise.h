// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_NOISE_H
#define OA_STG_VPM_NOISE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_noise.h  -  CSTGVPMNoise's value-getter family: all 7 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGVPMNoise is the VPM engine's noise-generator patch component --
 * Saturation, Cutoff, Volume, plus the volume EG-select and AMS source/
 * intensity/mode siblings -- confirmed genuinely fresh via a word-boundary
 * grep before starting, no pre-existing struct or ctor anywhere in this
 * project.
 *
 * Dialect: simplest fixed-K-off-this shape throughout, zero ctx-index
 * methods.
 *
 * Field-shape summary:
 *   - GetSaturation is the hardcoded-constant-getter shape first confirmed
 *     on CSTGPanOutputBase::GetPatchSolo -- `this` is never dereferenced,
 *     the body unconditionally stores literal 0 into .value only and
 *     returns sValueGetterTemp.
 *   - Plain 32-bit fields: dual-write .value and .displayValue -- Cutoff
 *     at +0x10, Volume at +0x14, VolumeAMSIntensity at +0x19.
 *   - Plain signed 8-bit fields: single-write .value only --
 *     VolumeEGSelect at +0x18, VolumeAMSSource at +0x1d, VolumeAMSMode
 *     at +0x1e.
 * No exceptions to the width-vs-dual-write rule found among the
 * field-backed methods in this class.
 */

struct CSTGVPMNoise {
	STGConvertedParam &GetSaturation(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetCutoff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolume(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeEGSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetVolumeAMSMode(CSTGPatchMessageContext &ctx);
};

#endif
