// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_STRING_TRACK_COMMON_H
#define OA_STG_STRING_TRACK_COMMON_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_string_track_common.h  -  CSTGStringTrackCommon's value-getter
 * family: all 4 real weak-symbol ctx-only candidates reconstructed, zero
 * outliers -- see include/oa_stg_string.h for the pilot class's full
 * derivation. CSTGStringTrackCommon is the guitar/string-track fret and
 * note-position patch component -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project. Not the same class as the unrelated
 * CSTGString -- the family's own pilot class from batch 1 -- despite the
 * shared "String" prefix.
 *
 * Dialect: mixed. GetFretPosition is an unsigned byte via movzx with no
 * shift/mask -- the established "unsigned non-bitfield byte" variant.
 * GetFretPositionAMSSource is a signed byte and GetFretPositionAMS-
 * Intensity is a dual-write dword -- both plain fixed-K fields off this,
 * same as GetFretPosition. GetStringNoteValue alone is ctx-indexed --
 * `mov edx,[edx+0x4]` with NO lea premultiply and NO SIB scale at all,
 * straight into `[eax+edx*1+K]` -- the bare stride-1 shape first
 * confirmed on CSTGMS20's own GetInputJack, an unsigned byte load,
 * single-write.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGStringTrackCommon {
	STGConvertedParam &GetFretPosition(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFretPositionAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFretPositionAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStringNoteValue(CSTGPatchMessageContext &ctx);
};

#endif
