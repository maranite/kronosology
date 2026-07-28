// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_METRONOME_SETTINGS_H
#define OA_STG_METRONOME_SETTINGS_H

#include "oa_global.h"	/* CSTGMessageContext, STGConvertedParam,
			 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_metronome_settings.h  -  CSTGMetronomeSettings's value-getter
 * family: both 2 real weak-symbol ctx-only candidates reconstructed,
 * zero outliers.
 *
 * Discovered via the broader-discovery-method sweep introduced this
 * batch (batch 18, see stg_value_getter_family.md's "objdump -dr
 * sValueGetterTemp cross-reference" recipe) -- this class takes
 * `CSTGMessageContext&`, the family's plain base context type, so it
 * was invisible to the earlier weak-linkage-plus-
 * `ER23CSTGPatchMessageContext$`-suffix-only sweep despite being
 * weak/COMDAT linkage itself.
 *
 * Genuinely fresh opaque class -- referenced only in comments elsewhere
 * in this project (oa_global.h's own `CSTGHDRTrack`/
 * `CSTGMetronomeSettings` slot-7-dispatch note, sequence_ctor.cpp), no
 * standalone struct or ctor anywhere before this batch.
 *
 * Dialect: the family's simplest -- both candidates are fixed-K field
 * reads directly off `this`, zero ctx-dynamic-index of any kind.
 * GetValueLevel is an unsigned byte (+0x5, single-write); GetValueBusSelect
 * is a signed byte (+0x4, single-write). Confirmed via direct
 * disassembly of the two weak/COMDAT symbols.
 */

struct CSTGMetronomeSettings {
	STGConvertedParam &GetValueBusSelect(CSTGMessageContext &ctx);
	STGConvertedParam &GetValueLevel(CSTGMessageContext &ctx);
};

#endif
