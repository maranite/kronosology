// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_MS20EG_H
#define OA_STG_MS20EG_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_ms20eg.h  -  CSTGMS20EG's value-getter family: all 20 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGMS20EG is a component of the MS-20-style analog voice model, its
 * own dedicated pair of envelope generators -- one hold-time-only EG2
 * and one full delay/attack/release EG1, per its own field names below
 * -- confirmed distinct from the already-done CSTGMS20 and
 * CSTGMS20ModelPatch classes, and genuinely fresh via a word-boundary
 * grep before starting, no pre-existing struct or ctor anywhere in this
 * project.
 *
 * Dialect: the SIMPLEST yet again -- every one of the 20 real candidates
 * is a fixed-K field read directly off this, zero ctx-dynamic-index
 * methods of any kind, matching CSTGEPModelPatch and
 * CSTGPolysixModelPatch's own zero-ctx-index dialects.
 *
 * The class's own naming is notable: each of its 4 EG-time parameters --
 * EG2HoldTime, EG1DelayTime, EG1AttackTime, EG1ReleaseTime -- carries not
 * one but a full FIVE-method AMS sibling group: the base time value,
 * plus AMSSource and AMSIntensity siblings, plus a SECOND-LEVEL
 * AMSIntensityAMSSource and AMSIntensityAMSIntensity pair. Despite the
 * doubled naming this is NOT a real double-modulation indirection --
 * every one of these 20 fields resolves to a plain fixed offset off
 * this, same as any other field in the family; the naming just reflects
 * that AMS intensity itself can be modulated by a second AMS source in
 * this class's own patch-parameter model, without any extra runtime
 * indirection in how the getter reads it.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes .value and .displayValue -- the
 *     4 base time values plus their 4 first-level AMSIntensity and 4
 *     second-level AMSIntensityAMSIntensity siblings, 12 methods total.
 *   - Plain 8-bit field, always movsx-signed in this class: single-writes
 *     .value only -- the 4 first-level AMSSource and 4 second-level
 *     AMSIntensityAMSSource siblings, 8 methods total.
 * No exceptions to the width-vs-dual-write rule found in this class --
 * every dword field dual-writes, every signed byte single-writes, no
 * surprises.
 */

struct CSTGMS20EG {
	STGConvertedParam &GetEG1AttackTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1AttackTimeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1AttackTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1AttackTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1AttackTimeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1DelayTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1DelayTimeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1DelayTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1DelayTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1DelayTimeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1ReleaseTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1ReleaseTimeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1ReleaseTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1ReleaseTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG1ReleaseTimeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG2HoldTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG2HoldTimeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG2HoldTimeAMSIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG2HoldTimeAMSIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEG2HoldTimeAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
