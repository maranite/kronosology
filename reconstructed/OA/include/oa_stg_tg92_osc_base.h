// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_TG92_OSC_BASE_H
#define OA_STG_TG92_OSC_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_tg92_osc_base.h  -  CSTGTG92OscBase's value-getter family: a
 * DELIBERATE PARTIAL reconstruction, 1 of 10 real weak-symbol ctx-only
 * candidates -- see include/oa_stg_string.h for the pilot class's full
 * derivation and the family memory's batch-12/20 notes for the full
 * history of this specific class.
 *
 * This class was first surfaced in batch 12 of the STG value-getter
 * family effort and DELIBERATELY DROPPED that same batch after finding
 * that 9 of its 10 real candidates dispatch through this class's OWN
 * vtable slot at raw offset 0xd4 before doing any field read, and that
 * slot's relocation target is __cxa_pure_virtual -- confirmed again this
 * batch via a direct `objdump -r -j .rodata._ZTV15CSTGTG92OscBase` dump
 * of the whole vtable's own relocations, not re-derived from memory.
 * A whole-symbol-table search for every one of the 9 pure-virtual
 * candidates' own method names -- GetReverse, GetBankType, GetStartOffset,
 * GetBankSelectUUID, GetBottomVelocity, GetCrossfadeCurve,
 * GetCrossfadeRange, GetMultisampleNum, and GetLevel -- across every OTHER
 * class in OA.ko found zero concrete overrides anywhere in this binary --
 * the real behavior depends on a concrete subclass this binary never
 * instantiates through a visible symbol, so these 9 remain a genuine
 * class-level Tier-B scope deferral, not attempted here.
 *
 * The 10th candidate, GetFreqOffset, is a completely ordinary fixed-K
 * signed dword field read directly off `this` -- it does NOT touch the
 * vtable at all, so it is reconstructed normally below.
 *
 * Field-shape summary:
 *   - Plain signed 32-bit field, dual-write -- both value and
 *     displayValue -- GetFreqOffset, this+0xc.
 */

struct CSTGTG92OscBase {
	STGConvertedParam &GetFreqOffset(CSTGPatchMessageContext &ctx);
};

#endif
