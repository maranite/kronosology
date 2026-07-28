// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PITCH_BASE_H
#define OA_STG_PITCH_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_pitch_base.h  -  CSTGPitchBase's value-getter family: 3 of its 4
 * real weak-symbol ctx-only candidates reconstructed -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGPitchBase is the base pitch-bend-range patch component -- confirmed
 * genuinely fresh via a word-boundary grep before starting, no
 * pre-existing struct or ctor anywhere in this project.
 *
 * Dialect: the simplest yet -- every real candidate a plain fixed-K
 * signed dword field read directly off this, zero ctx-dynamic-index
 * methods, all dual-write. GetBendUp and GetBendRange read the exact
 * same offset -- byte-identical bodies, confirmed by isolated re-dumps of
 * both COMDAT sections, not a transcription slip -- so the two symbols
 * are modeled as two separate C++ member functions with identical bodies,
 * matching ground truth's own apparent aliasing rather than sharing one
 * implementation.
 *
 * Excluded outlier, a genuinely new shape for the family: HandleVoiceKey-
 * DownTuningOffsetChanged is a bare `ret` with an empty body -- it never
 * touches CSTGParamsOwner::sValueGetterTemp at all despite matching the
 * family's ctx-only mangled suffix and weak linkage. Distinct from the
 * earlier hardcoded-constant-getter shape (CSTGPanOutputBase::GetPatchSolo,
 * which DOES write a literal 0 into sValueGetterTemp.value) -- this one
 * writes nothing and returns nothing meaningful in eax, i.e. a genuine
 * no-op stub, not a value getter under any interpretation. Also excluded
 * because its own name does not fit the Get- and Set-prefixed naming
 * convention at all.
 */

struct CSTGPitchBase {
	STGConvertedParam &GetBendUp(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBendDown(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBendRange(CSTGPatchMessageContext &ctx);
};

#endif
