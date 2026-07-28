// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_COMMON_EFFECT_LFO_H
#define OA_STG_COMMON_EFFECT_LFO_H

#include "oa_global.h"	/* CSTGMessageContext, STGConvertedParam,
			 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_common_effect_lfo.h  -  CSTGCommonEffectLFO's value-getter
 * family: all 8 real weak-symbol ctx-only candidates reconstructed,
 * zero outliers.
 *
 * Discovered via the broader-discovery-method sweep introduced this
 * batch (batch 18, see stg_value_getter_family.md's "objdump -dr
 * sValueGetterTemp cross-reference" recipe) -- takes
 * `CSTGMessageContext&`, the family's plain base context type, hence
 * invisible to the earlier `ER23CSTGPatchMessageContext$`-suffix-only
 * sweep despite being weak/COMDAT itself.
 *
 * Genuinely fresh opaque class -- per oa_global.h's own header comment
 * above `CSTGEffectRack`'s struct, `CSTGCommonEffectLFO` is one of 4
 * sibling classes (`CMFXEffectSlot`/`CTFXEffectSlot`/
 * `CSTGEffectBalance`/`CSTGCommonEffectLFO`) with NO out-of-line ctor
 * in ground truth at all -- fully inlined as direct field writes inside
 * `CSTGProgram::CSTGProgram()`'s own body, embedded TWICE at `+0xa59`/
 * `+0xa6a` (see program_ctor.cpp/combi_ctor.cpp; the effect-rack's own
 * per-slot LFO pair, distinct from the voice-model `CSTGCommonLFO`).
 *
 * Dialect: the family's simplest -- every candidate a fixed-K field
 * read directly off `this`, zero ctx-dynamic-index of any kind, despite
 * the class's own name suggesting a modulation source. Two bits are
 * packed into the same byte at +0x10 (ResetEnable bit 0, TempoMIDISync
 * bit 1), same mask-only/shift-then-mask bitfield shapes used
 * throughout this family. Confirmed via direct disassembly of all 8
 * weak/COMDAT symbols.
 */

struct CSTGCommonEffectLFO {
	STGConvertedParam &GetValueControlChannel(CSTGMessageContext &ctx);
	STGConvertedParam &GetValueFrequency(CSTGMessageContext &ctx);
	STGConvertedParam &GetValueResetEnable(CSTGMessageContext &ctx);
	STGConvertedParam &GetValueResetSource(CSTGMessageContext &ctx);
	STGConvertedParam &GetValueTempo(CSTGMessageContext &ctx);
	STGConvertedParam &GetValueTempoBaseNote(CSTGMessageContext &ctx);
	STGConvertedParam &GetValueTempoMIDISync(CSTGMessageContext &ctx);
	STGConvertedParam &GetValueTempoTimes(CSTGMessageContext &ctx);
};

#endif
