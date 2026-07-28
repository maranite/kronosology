// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_EFFECT_BALANCE_H
#define OA_STG_EFFECT_BALANCE_H

#include "oa_global.h"	/* CSTGMessageContext, STGConvertedParam,
			 * CSTGParamsOwner::sValueGetterTemp,
			 * CSTGPerformanceVarsManager::sInstance */

/*
 * oa_stg_effect_balance.h  -  CSTGEffectBalance's value-getter family:
 * all 3 real strong-linkage ctx-only candidates reconstructed, zero
 * outliers.
 *
 * Discovered via the broader-discovery-method sweep introduced this
 * batch, batch 18 -- see stg_value_getter_family.md's "objdump -dr
 * sValueGetterTemp cross-reference" recipe -- NOT the earlier
 * `nm ... ER23CSTGPatchMessageContext$` weak-linkage-only sweep, which
 * this class was invisible to on two counts: it takes a
 * `CSTGMessageContext&`, the family's plain base context type, not the
 * STG-patch-specific `CSTGPatchMessageContext&`, and its 3 real
 * candidates are STRONG, T linkage, not weak/COMDAT.
 *
 * CSTGEffectBalance is a genuinely fresh opaque class -- no standalone
 * ctor in ground truth at all, confirmed via a whole-symbol-table grep,
 * zero hits: per oa_global.h's own header comment above
 * `CSTGEffectRack`'s struct, `CSTGEffectBalance` is one of 4 sibling
 * classes -- `CMFXEffectSlot`, `CTFXEffectSlot`, `CSTGEffectBalance`,
 * `CSTGCommonEffectLFO` -- fully inlined as direct field writes inside
 * `CSTGProgram::CSTGProgram`'s own body, embedded at offset 0xa55, see
 * program_ctor.cpp and combi_ctor.cpp.
 *
 * Genuinely new shape for the whole STG value-getter family: none of
 * the 3 methods read `this` OR `ctx` at all. Each instead resolves the
 * process-wide ACTIVE `CSTGPerformanceVarsManager` via the same raw
 * selector-array lookup ground truth's own
 * `CSTGGlobal::ResolveActivePerformanceVarsManager` already establishes
 * elsewhere in this project, see global.cpp -- confirmed via direct
 * disassembly that ground truth genuinely INLINES this lookup at each
 * of the 3 call sites rather than emitting a real call to that helper,
 * and the helper is a private `CSTGGlobal` member anyway, so this
 * file's own static local reimplements the same raw sequence rather
 * than calling it. Reads one fixed dword off the resolved pointer --
 * offset 0x2128 for IFX, 0x212c for MFX, 0x2130 for TFX. All three
 * dual-write, 32-bit fields. Confirmed via direct disassembly of
 * `.text+0xcaed0..0xcaf60`.
 */

struct CSTGEffectBalance {
	STGConvertedParam &GetIFXBalance(CSTGMessageContext &ctx);
	STGConvertedParam &GetMFXBalance(CSTGMessageContext &ctx);
	STGConvertedParam &GetTFXBalance(CSTGMessageContext &ctx);
};

#endif
