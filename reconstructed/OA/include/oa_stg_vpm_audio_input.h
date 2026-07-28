// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_AUDIO_INPUT_H
#define OA_STG_VPM_AUDIO_INPUT_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_audio_input.h  -  CSTGVPMAudioInput's value-getter family:
 * all 4 real weak-symbol ctx-only candidates reconstructed, zero
 * outliers -- see include/oa_stg_string.h for the pilot class's full
 * derivation. CSTGVPMAudioInput is the VPM engine's live-audio-input
 * mixer-leg patch component -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * Dialect: the simplest yet -- every candidate a plain fixed-K field read
 * directly off this, zero ctx-dynamic-index methods, despite the class's
 * own field layout being byte-identical (same relative offsets 0xc/0x10/
 * 0x14/0x15) to CSTGVPMMixer's own ctx-indexed record -- confirmed via
 * disassembly that this class's own fields are NOT ctx-indexed, only
 * CSTGVPMMixer's per-operator array is. GetLevel and GetAMSIntensity are
 * dual-write dwords; GetAMSSource and GetAMSMode are both signed bytes
 * (movsx).
 */

struct CSTGVPMAudioInput {
	STGConvertedParam &GetLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSMode(CSTGPatchMessageContext &ctx);
};

#endif
