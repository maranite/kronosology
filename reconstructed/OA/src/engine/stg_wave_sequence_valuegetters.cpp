// SPDX-License-Identifier: GPL-2.0
/*
 * stg_wave_sequence_valuegetters.cpp  -  CSTGWaveSequence's
 * Getter*(CSTGWaveSeqDataMessageContext&) value-getter family, see
 * include/oa_global.h's own header comment above `struct
 * CSTGWaveSequence` for the full class-level derivation notes -- all 34
 * real weak-symbol ctx-only candidates decoded, zero outliers.
 *
 * verify/test_stg_wave_sequence_valuegetters.cpp independently re-derives
 * the expected value for every method here via a separate Python
 * evaluator, not by re-using this file's C output strings.
 */

#include "oa_global.h"

STGConvertedParam &CSTGWaveSequence::GetterRunSequence(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x4) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterNoteOnAdvance(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x4) >> 1) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterTimeTempoMode(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x4) >> 2) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterSwingResolution(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x5);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterStartStep(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x6);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterEndStep(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x7);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterSoloStep(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x8);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterStartStepAMSSource(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x9);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterStartStepAMSIntensity(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xa);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterDurationAMSSource(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xb);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterDurationAMSIntensity(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned short *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterPositionAMSSource(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0xe);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterPositionAMSIntensity(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0xf);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterLoopStart(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x10);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterLoopEnd(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x11);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterLoopRepeat(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(unsigned char *)(base + 0x12);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterLoopDirection(CSTGWaveSeqDataMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x13);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterStepType(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed char *)(base + idx * 0x34 + 0x42);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterMultisampleSelect(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(unsigned short *)(base + idx * 0x34 + 0x3c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterLevel(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0x34 + 0x24);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterTune(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0x34 + 0x28);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterTranspose(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed char *)(base + idx * 0x34 + 0x43);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterReverse(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(unsigned char *)(base + idx * 0x34 + 0x47) & 0x1;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterStartOffset(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(unsigned char *)(base + idx * 0x34 + 0x44);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterAMS1Output(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0x34 + 0x2c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterAMS2Output(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0x34 + 0x30);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterDuration(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed short *)(base + idx * 0x34 + 0x3e);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterTempoBaseNote(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed char *)(base + idx * 0x34 + 0x45);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterTempoMultiplier(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(unsigned char *)(base + idx * 0x34 + 0x46);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterCrossfadeTime(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(signed short *)(base + idx * 0x34 + 0x40);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterFadeOutShape(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0x34 + 0x34);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterFadeInShape(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(int *)(base + idx * 0x34 + 0x38);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

/*
 * GetterBankSelect / GetterBankSelectUUID -- byte-identical bodies,
 * confirmed via two independent isolated re-dumps. Copies a 16-byte
 * record UUID -- 4 sequential dwords at record offset 0x14, 0x18, 0x1c,
 * and 0x20 -- directly into sValueGetterTemp's own +0x0/+0x4/+0x8/+0xc
 * bytes, in place of the usual .value/.displayValue write.
 */
STGConvertedParam &CSTGWaveSequence::GetterBankSelect(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	unsigned char *rec = base + idx * 0x34 + 0x10;
	unsigned char *temp = (unsigned char *)&CSTGParamsOwner::sValueGetterTemp;
	*(unsigned int *)(temp + 0x0) = *(unsigned int *)(rec + 0x4);
	*(unsigned int *)(temp + 0x4) = *(unsigned int *)(rec + 0x8);
	*(unsigned int *)(temp + 0x8) = *(unsigned int *)(rec + 0xc);
	*(unsigned int *)(temp + 0xc) = *(unsigned int *)(rec + 0x10);
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGWaveSequence::GetterBankSelectUUID(CSTGWaveSeqDataMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	unsigned char *rec = base + idx * 0x34 + 0x10;
	unsigned char *temp = (unsigned char *)&CSTGParamsOwner::sValueGetterTemp;
	*(unsigned int *)(temp + 0x0) = *(unsigned int *)(rec + 0x4);
	*(unsigned int *)(temp + 0x4) = *(unsigned int *)(rec + 0x8);
	*(unsigned int *)(temp + 0x8) = *(unsigned int *)(rec + 0xc);
	*(unsigned int *)(temp + 0xc) = *(unsigned int *)(rec + 0x10);
	return CSTGParamsOwner::sValueGetterTemp;
}
