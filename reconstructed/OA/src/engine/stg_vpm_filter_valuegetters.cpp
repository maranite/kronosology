// SPDX-License-Identifier: GPL-2.0
/*
 * stg_vpm_filter_valuegetters.cpp  -  CSTGVPMFilter's
 * Get*(CSTGPatchMessageContext&) value-getter family, see
 * include/oa_stg_vpm_filter.h for the full class-level derivation notes
 * -- all 3 real weak-symbol ctx-only candidates decoded, zero outliers.
 *
 * All bodies below were transcribed by the STG value-getter family's
 * scripted instruction-pattern decoder, extended this batch to recognize
 * a byte-equals-literal-constant boolean test (cmp byte,K2; sete al) as
 * its own case alongside the existing zero/nonzero truth-value shapes.
 * verify/test_stg_vpm_filter_valuegetters.cpp independently re-derives
 * the expected value for every method here from the same parsed facts
 * via a separate Python evaluator, not by re-using this file's C output
 * strings.
 */

#include "oa_stg_vpm_filter.h"

STGConvertedParam &CSTGVPMFilter::GetRoutingValue(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = (*(unsigned char *)(base + 0x1f) == 3) ? 1 : 0;
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMFilter::GetFilterAOutputLevelAMSMode(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x73);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGVPMFilter::GetFilterBOutputLevelAMSMode(CSTGPatchMessageContext &)
{
	unsigned char *base = (unsigned char *)this;
	int v = *(signed char *)(base + 0x116);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
