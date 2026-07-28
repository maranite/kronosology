// SPDX-License-Identifier: GPL-2.0
/*
 * stg_tone_adjust_valuegetters.cpp  -  CSTGToneAdjust's
 * Get*(CSTGToneAdjustMessageContext&) value-getter family, see
 * include/oa_global.h's own header comments above
 * `struct CSTGToneAdjustMessageContext`/`struct CSTGToneAdjust` for the
 * full class-level derivation notes -- all 7 real weak-symbol ctx-only
 * candidates decoded, zero outliers.
 * verify/test_stg_tone_adjust_valuegetters.cpp independently re-derives
 * the expected value for every method here via a separate Python
 * evaluator, not by re-using this file's C output strings.
 */

#include "oa_global.h"

STGConvertedParam &CSTGToneAdjust::GetValueAssignSlider(CSTGToneAdjustMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	unsigned char b = *(unsigned char *)(base + 4 + idx);
	ctx.changedFlag = (unsigned char)(b & 0x1);
	int v = (int)((unsigned char)b >> 1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGToneAdjust::GetValueAssignKnob(CSTGToneAdjustMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	unsigned char b = *(unsigned char *)(base + 0xd + idx);
	ctx.changedFlag = (unsigned char)(b & 0x1);
	int v = (int)((unsigned char)b >> 1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGToneAdjust::GetValueAssignSwitch(CSTGToneAdjustMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	unsigned char b = *(unsigned char *)(base + 0x15 + idx);
	ctx.changedFlag = (unsigned char)(b & 0x1);
	int v = (int)((unsigned char)b >> 1);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGToneAdjust::GetValueAssignSwitchOnValue(CSTGToneAdjustMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(short *)(base + 0x25 + idx * 2);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGToneAdjust::GetValueSliderValue(CSTGToneAdjustMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(short *)(base + 0x45 + idx * 2);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGToneAdjust::GetValueKnobValue(CSTGToneAdjustMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	int idx = (int)ctx.index;
	int v = *(short *)(base + 0x57 + idx * 2);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

/*
 * GetValueSwitchValue: ctx.index is used as a variable BIT-SHIFT AMOUNT
 * (not an array index) applied to a FIXED 16-bit word field at
 * this+0x67 -- ground truth's own `sar %cl,%eax` with cl=ctx.index's
 * low byte. Real target semantics: x86 SAR/SHR mask the shift count to
 * 0..31 (5 bits) for a 32-bit operand -- reproduced here with an
 * explicit `& 0x1f` so this matches on any host, not just x86.
 */
STGConvertedParam &CSTGToneAdjust::GetValueSwitchValue(CSTGToneAdjustMessageContext &ctx)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int shift = ctx.index & 0x1fu;
	unsigned int w = *(unsigned short *)(base + 0x67);
	int v = (int)((w >> shift) & 0x1u);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
