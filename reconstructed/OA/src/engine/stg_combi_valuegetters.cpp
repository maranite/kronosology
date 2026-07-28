// SPDX-License-Identifier: GPL-2.0
/*
 * stg_combi_valuegetters.cpp  -  CSTGCombi's Get*(CSTGMessageContext&)
 * value-getter family, see include/oa_global.h's own header comment
 * above `struct CSTGCombi` for the full class-level derivation notes --
 * all 4 real weak-symbol ctx-only candidates decoded, zero outliers,
 * zero ctx-dynamic-index methods.
 * verify/test_stg_combi_valuegetters.cpp independently re-derives the
 * expected value for every method here via a separate Python evaluator,
 * not by re-using this file's C output strings.
 */

#include "oa_global.h"

STGConvertedParam &CSTGCombi::GetValueScaleType(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x19e3);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCombi::GetValuePitchRandomize(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x19e4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCombi::GetValueScaleKey(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x19e5);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCombi::GetValueAutoLoadToneAdjust(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x19e6)) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
