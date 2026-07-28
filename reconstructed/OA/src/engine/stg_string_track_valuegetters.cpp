// SPDX-License-Identifier: GPL-2.0
/*
 * stg_string_track_valuegetters.cpp  -  CSTGStringTrack's single
 * Get*(CSTGPatchMessageContext&) value-getter, see
 * include/oa_stg_string_track.h for the full class-level derivation
 * notes.
 *
 * Transcribed by the STG value-getter family's scripted
 * instruction-pattern decoder, reusing the bare stride-4 SIB-scaled
 * ctx-index shape -- no lea premultiply -- first confirmed on
 * CSTGMultiFilter2Pole.
 * verify/test_stg_string_track_valuegetters.cpp independently re-derives
 * the expected value via a separate Python evaluator, not by re-using
 * this file's C output string.
 */

#include "oa_stg_string_track.h"

STGConvertedParam &CSTGStringTrack::GetStringValue(CSTGPatchMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + CtxIndex(ctx, 0x4, 4) + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
