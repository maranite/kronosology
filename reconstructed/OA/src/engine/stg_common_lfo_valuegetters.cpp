// SPDX-License-Identifier: GPL-2.0
/*
 * stg_common_lfo_valuegetters.cpp  -  CSTGCommonLFO's
 * Get*(CSTGProgramMessageContext&) value-getter family, see
 * include/oa_engine_init.h's own header comments above
 * `struct CSTGProgramMessageContext`/`struct CSTGCommonLFO` for the
 * full class-level derivation notes -- all 17 real weak-symbol
 * ctx-only candidates decoded, zero outliers, zero ctx-dynamic-index
 * methods -- ctx is never dereferenced by any of them, despite several
 * method names implying otherwise, verified via direct disassembly.
 * verify/test_stg_common_lfo_valuegetters.cpp independently re-derives
 * the expected value for every method here via a separate Python
 * evaluator, not by re-using this file's C output strings.
 */

#include "oa_global.h"
#include "oa_engine_init.h"

STGConvertedParam &CSTGCommonLFO::GetWaveform(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1b);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetStartPhase(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetShape(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x9);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetShapeAMSSource(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x11);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetShapeAMSIntensity(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xd);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetOffset(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x12);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetFrequency(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x1c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetFrequencyFine(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x16);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetAMSResetSource(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x1a);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetFrequencyAMSSource(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x26);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetFrequencyAMSIntensity(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x22);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetFrequencyAMSIntensityAMSSource(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x30);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetFrequencyAMSIntensityAMSIntensity(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x2c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetStop(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x1f)) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetMIDITempoSync(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x1f) >> 2) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetMIDITempoSyncBaseNote(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x20);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGCommonLFO::GetMIDITempoSyncTimes(CSTGProgramMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x21);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
