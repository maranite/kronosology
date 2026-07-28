// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_DRIVER_H
#define OA_STG_DRIVER_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_driver.h  -  CSTGDriver's value-getter family: all 7 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGDriver is the STG waveshaping-drive patch component -- Bypass,
 * Drive, and Boost, each with the usual AMS source/intensity siblings for
 * Drive and Boost -- confirmed genuinely fresh via a word-boundary grep
 * before starting, no pre-existing struct or ctor anywhere in this
 * project.
 *
 * Dialect: simplest fixed-K-off-this shape throughout, zero ctx-index
 * methods.
 *
 * Field-shape summary:
 *   - GetBypass: mask-only single-bit boolean off byte +0x1e -- bit 0, no
 *     shift instruction, single-write.
 *   - Plain 32-bit fields: dual-write .value and .displayValue -- Drive
 *     at +0xc, DriveAMSIntensity at +0x14, Boost at +0x10, BoostAMSIntensity
 *     at +0x19.
 *   - Plain signed 8-bit fields: single-write .value only -- DriveAMSSource
 *     at +0x18, BoostAMSSource at +0x1d.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGDriver {
	STGConvertedParam &GetBypass(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDrive(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDriveAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDriveAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBoost(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBoostAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBoostAMSIntensity(CSTGPatchMessageContext &ctx);
};

#endif
