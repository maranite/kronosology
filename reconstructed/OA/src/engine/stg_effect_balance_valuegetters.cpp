// SPDX-License-Identifier: GPL-2.0
/*
 * stg_effect_balance_valuegetters.cpp  -  CSTGEffectBalance's
 * Get*(CSTGMessageContext&) value-getter family, see
 * include/oa_stg_effect_balance.h for the full class-level derivation
 * notes -- all 3 real strong-linkage ctx-only candidates decoded, zero
 * outliers. Neither `this` nor `ctx` is read by any of these three --
 * each resolves the active CSTGPerformanceVarsManager and reads one
 * fixed field off it.
 *
 * ResolveActiveManager, a static file-local helper below, reproduces
 * the exact same raw selector lookup as `CSTGGlobal::
 * ResolveActivePerformanceVarsManager` in global.cpp -- confirmed via
 * direct disassembly that ground truth INLINES this sequence directly
 * here rather than emitting a real call to that helper, no `call`
 * instruction present, matching the `movzbl eax,[sInstance+8]; mov
 * eax,[eax*4+sInstance]` shape byte-for-byte -- and
 * `ResolveActivePerformanceVarsManager` is itself a private
 * `CSTGGlobal` member anyway, so a real cross-class call would not even
 * compile -- reading the same public
 * `CSTGPerformanceVarsManager::sInstance` directly is both the
 * binary-accurate AND the only accessible translation.
 * verify/test_stg_effect_balance_valuegetters.cpp independently
 * re-derives the expected value for every method here via a separate
 * Python evaluator, not by re-using this file's C output strings.
 */

#include "oa_stg_effect_balance.h"

static CSTGPerformanceVarsManager *ResolveActiveManager()
{
	unsigned char *slots = CSTGPerformanceVarsManager::sInstance;
	unsigned char selector = slots[8];
	unsigned int slotValue = *(unsigned int *)(slots + (unsigned int)selector * 4);
	return (CSTGPerformanceVarsManager *)(unsigned long)slotValue;
}

STGConvertedParam &CSTGEffectBalance::GetIFXBalance(CSTGMessageContext &)
{
	CSTGPerformanceVarsManager *mgr = ResolveActiveManager();
	int v = *(int *)((unsigned char *)mgr + 0x2128);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectBalance::GetMFXBalance(CSTGMessageContext &)
{
	CSTGPerformanceVarsManager *mgr = ResolveActiveManager();
	int v = *(int *)((unsigned char *)mgr + 0x212c);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}

STGConvertedParam &CSTGEffectBalance::GetTFXBalance(CSTGMessageContext &)
{
	CSTGPerformanceVarsManager *mgr = ResolveActiveManager();
	int v = *(int *)((unsigned char *)mgr + 0x2130);
	CSTGParamsOwner::sValueGetterTemp.value = v;
	CSTGParamsOwner::sValueGetterTemp.displayValue = v;
	return CSTGParamsOwner::sValueGetterTemp;
}
