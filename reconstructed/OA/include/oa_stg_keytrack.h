// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_KEYTRACK_H
#define OA_STG_KEYTRACK_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_keytrack.h  -  CSTGKeyTrack's value-getter family: all 7 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGKeyTrack is the STG key-tracking (keyboard-scaling) patch
 * component -- three key-range breakpoints and four ramp/slope values
 * either side of them -- confirmed genuinely fresh via a word-boundary
 * grep before starting, no pre-existing struct or ctor anywhere in this
 * project.
 *
 * Dialect: the simplest yet -- every candidate a plain fixed-K byte field
 * read directly off this, zero ctx-dynamic-index methods at all. The
 * three key-position fields (LowKey/MidKey/HighKey) are unsigned
 * (movzx, no shift/mask), matching the family's established "unsigned
 * non-bitfield byte" variant first confirmed on CSTGPolysixMG's own
 * GetMIDITempoSyncTimes. The four ramp fields (LowRamp/MidLowRamp/
 * MidHighRamp/HighRamp) are signed (movsx). All seven single-write only,
 * consistent with the family's byte-field convention -- only 32-bit
 * fields dual-write, and this class has none.
 */

struct CSTGKeyTrack {
	STGConvertedParam &GetLowKey(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMidKey(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHighKey(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLowRamp(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMidLowRamp(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMidHighRamp(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetHighRamp(CSTGPatchMessageContext &ctx);
};

#endif
