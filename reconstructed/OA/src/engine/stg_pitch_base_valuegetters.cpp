// SPDX-License-Identifier: GPL-2.0
/*
 * stg_pitch_base_valuegetters.cpp  -  CSTGPitchBase's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_pitch_base.h for the full class-level derivation notes --
 * 3 of its 4 real weak-symbol ctx-only candidates decoded, 1 excluded
 * (HandleVoiceKeyDownTuningOffsetChanged, a bare no-op stub).
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder -- every field here is a plain
 * fixed-K dword read directly off this, no ctx-index arithmetic involved.
 * verify/test_stg_pitch_base_valuegetters.cpp independently re-derives
 * the expected value for every method here from the same parsed facts
 * via a separate Python evaluator, not by re-using this file's C output
 * strings.
 */

#include "oa_stg_pitch_base.h"

STGConvertedParam &CSTGPitchBase::GetBendUp(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchBase::GetBendDown(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0x10);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGPitchBase::GetBendRange(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(int *)(base + 0xc);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
