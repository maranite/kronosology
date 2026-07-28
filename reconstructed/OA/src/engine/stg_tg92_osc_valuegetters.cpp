// SPDX-License-Identifier: GPL-2.0
/*
 * stg_tg92_osc_valuegetters.cpp  -  CSTGTG92Osc's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_tg92_osc.h for the full class-level derivation notes --
 * both real weak-symbol ctx-only candidates decoded, zero outliers.
 *
 * Both bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, the established unsigned
 * non-bitfield byte shape. verify/test_stg_tg92_osc_valuegetters.cpp
 * independently re-derives the expected value for each method here from
 * the SAME parsed facts via a separate Python evaluator, not by re-using
 * this file's C output strings.
 */

#include "oa_stg_tg92_osc.h"

STGConvertedParam &CSTGTG92Osc::GetOscVelocityZoneLow(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xd8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGTG92Osc::GetOscVelocityZoneHigh(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xd9);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
