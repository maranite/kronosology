// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_STRING_TRACK_H
#define OA_STG_STRING_TRACK_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_string_track.h  -  CSTGStringTrack's value-getter family: its
 * ENTIRE real weak-symbol ctx-only candidate set is a single method,
 * GetStringValue -- see include/oa_stg_string.h for the pilot class's
 * full derivation. CSTGStringTrack is the modeled-string physical-model
 * per-string patch component -- confirmed genuinely fresh via a
 * word-boundary grep before starting, distinct from the already-modeled
 * CSTGStringTrackCommon -- a real name-collision precedent, same shape
 * as the CSTGPolysix/CSTGPolysixModel pair. Found via the whole-binary
 * weak-linkage plus ctx-only-mangled-suffix sweep, batch 12's own
 * methodology.
 *
 * Field-shape: ctx-indexed 32-bit field, bare stride-4 SIB scale, no lea
 * premultiply -- the same shape first confirmed on
 * CSTGMultiFilter2Pole's own GetLFOIntensity/GetLFOJSminusYIntensity --
 * dual-writes value and displayValue.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGStringTrack {
	STGConvertedParam &GetStringValue(CSTGPatchMessageContext &ctx);
};

#endif
