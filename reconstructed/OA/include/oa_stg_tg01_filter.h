// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_TG01_FILTER_H
#define OA_STG_TG01_FILTER_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_tg01_filter.h  -  CSTGTG01Filter's value-getter family: its
 * ENTIRE real weak-symbol ctx-only candidate set is a single method,
 * GetRouting -- see include/oa_stg_string.h for the pilot class's full
 * derivation. CSTGTG01Filter is a small TG01-style filter-routing patch
 * component -- confirmed genuinely fresh via a word-boundary grep before
 * starting, no pre-existing struct or ctor anywhere in this project.
 * Found via the whole-binary weak-linkage plus ctx-only-mangled-suffix
 * sweep (batch 12's methodology), grouped by mangled length-prefixed
 * class name -- this class showed exactly one real hit, no other
 * Get*- or Set*-prefixed symbols of any linkage exist for it at all.
 *
 * Field-shape: plain 8-bit signed field, single-write -- value only, no
 * displayValue -- matching the family's standard width-vs-dual-write
 * rule, where byte-width fields (signed or unsigned) always single-write
 * and only dword fields ever carry the second displayValue write.
 */

struct CSTGTG01Filter {
	STGConvertedParam &GetRouting(CSTGPatchMessageContext &ctx);
};

#endif
