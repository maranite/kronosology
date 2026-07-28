// SPDX-License-Identifier: GPL-2.0
/*
 * stg_tg01_filter_valuegetters.cpp  -  CSTGTG01Filter's single
 * Get*(CSTGPatchMessageContext&) value-getter, see
 * include/oa_stg_tg01_filter.h for the full class-level derivation
 * notes.
 *
 * Transcribed by the STG value-getter family's scripted
 * instruction-pattern decoder -- a plain fixed-K signed byte read, the
 * family's simplest shape.
 * verify/test_stg_tg01_filter_valuegetters.cpp independently re-derives
 * the expected value via a separate Python evaluator, not by re-using
 * this file's C output string.
 */

#include "oa_stg_tg01_filter.h"

STGConvertedParam &CSTGTG01Filter::GetRouting(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xf3);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
