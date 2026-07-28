// SPDX-License-Identifier: GPL-2.0
/*
 * stg_tg92_osc_base_valuegetters.cpp  -  CSTGTG92OscBase's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_tg92_osc_base.h for the full class-level derivation
 * notes -- 1 of 10 real weak-symbol ctx-only candidates decoded, 9
 * deliberately deferred as a class-level pure-virtual-dispatch scope
 * exclusion, not attempted here.
 *
 * The single body below was transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, the established plain fixed-K
 * dual-write dword shape. verify/test_stg_tg92_osc_base_valuegetters.cpp
 * independently re-derives the expected value from the SAME parsed facts
 * via a separate Python evaluator, not by re-using this file's C output
 * string.
 */

#include "oa_stg_tg92_osc_base.h"

STGConvertedParam &CSTGTG92OscBase::GetFreqOffset(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
