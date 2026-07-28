// SPDX-License-Identifier: GPL-2.0
/*
 * stg_metronome_settings_valuegetters.cpp  -  CSTGMetronomeSettings's
 * Get*(CSTGMessageContext&) value-getter family, see
 * include/oa_stg_metronome_settings.h for the full class-level
 * derivation notes -- both real weak-symbol ctx-only candidates
 * decoded, zero outliers, zero ctx-dynamic-index methods.
 * verify/test_stg_metronome_settings_valuegetters.cpp independently
 * re-derives the expected value for every method here via a separate
 * Python evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_metronome_settings.h"

STGConvertedParam &CSTGMetronomeSettings::GetValueLevel(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x5);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGMetronomeSettings::GetValueBusSelect(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
