// SPDX-License-Identifier: GPL-2.0
/*
 * stg_common_effect_lfo_valuegetters.cpp  -  CSTGCommonEffectLFO's
 * Get*(CSTGMessageContext&) value-getter family, see
 * include/oa_stg_common_effect_lfo.h for the full class-level
 * derivation notes -- all 8 real weak-symbol ctx-only candidates
 * decoded, zero outliers, zero ctx-dynamic-index methods.
 * verify/test_stg_common_effect_lfo_valuegetters.cpp independently
 * re-derives the expected value for every method here via a separate
 * Python evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_common_effect_lfo.h"

STGConvertedParam &CSTGCommonEffectLFO::GetValueFrequency(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x4);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonEffectLFO::GetValueTempo(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonEffectLFO::GetValueControlChannel(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonEffectLFO::GetValueResetSource(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xd);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonEffectLFO::GetValueTempoTimes(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xe);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonEffectLFO::GetValueTempoBaseNote(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xf);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonEffectLFO::GetValueResetEnable(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x10)) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonEffectLFO::GetValueTempoMIDISync(CSTGMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x10) >> 1) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
