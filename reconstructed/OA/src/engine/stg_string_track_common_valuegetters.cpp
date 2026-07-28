// SPDX-License-Identifier: GPL-2.0
/*
 * stg_string_track_common_valuegetters.cpp  -  CSTGStringTrackCommon's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_string_track_common.h for the full class-level
 * derivation notes -- all 4 real weak-symbol ctx-only candidates decoded,
 * zero outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, reusing the bare stride-1
 * ctx-index shape -- no lea premultiply, no SIB scale -- first confirmed
 * on CSTGMS20's own GetInputJack. verify/test_stg_string_track_common_
 * valuegetters.cpp independently re-derives the expected value for every
 * method here from the same parsed facts via a separate Python
 * evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_string_track_common.h"

STGConvertedParam &CSTGStringTrackCommon::GetFretPosition(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGStringTrackCommon::GetFretPositionAMSSource(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x11);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGStringTrackCommon::GetFretPositionAMSIntensity(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xd);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGStringTrackCommon::GetStringNoteValue(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + CtxIndex(ctx, 0x4, 1) + 0x12);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
