// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_ORGAN_OSC_H
#define OA_STG_ORGAN_OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_organ_osc.h  -  CSTGOrganOsc's value-getter family: 13 of 36
 * pending Get* candidates reconstructed via the STG value-getter family's
 * scripted instruction-pattern decoder -- see include/oa_stg_string.h for
 * the pilot class's full derivation. CSTGOrganOsc is the STG tonewheel-organ
 * oscillator patch component -- drawbars, percussion assign, EX (extended)
 * upper/lower drawbar mode, per its Drawbar-, EX- and Perc-prefixed parameter names.
 *
 * NOT to be confused with the already-reconstructed CSTGOrganModelPatch
 * (see include/oa_stg_organ_model_patch.h) -- unrelated class with a
 * similar name, confirmed distinct via word-boundary grep before starting.
 *
 * Of the 36 pending Get*- and Set*-prefixed symbols this class's own address range
 * carries, only 13 are real weak/COMDAT ctx-only value getters -- the
 * other 23 are global ('T') linkage and NOT part of this family: most take
 * an extra argument beyond ctx (SetPercEnable/SetSplitEnable/etc, a bool
 * value setter, and SetPercHarmonic/SetEnvelopeIncrement), and the
 * remainder -- GetLowerNoteCount, GetUpperNoteCount, GetLowerDrawbarSum,
 * GetUpperDrawbarSum, GetEXPercDrawbarSum, GetLowerNoteCountCompression,
 * GetUpperNoteCountCompression, GetVoiceLevelEstimate -- despite matching
 * the `Get*(CSTGPatchMessageContext&)` mangled suffix, spot-checked via
 * GetLowerNoteCount's own disassembly and confirmed NOT to follow this
 * family's convention at all: it returns a plain per-voice note count
 * directly in eax (no sValueGetterTemp write, no STGConvertedParam&
 * return), i.e. real runtime state, not a static patch-value accessor.
 * This reconfirms the established "T linkage = different mechanism"
 * heuristic even for candidates whose mangled signature superficially
 * matches -- global linkage is trusted over signature shape.
 *
 * Two field shapes, both already known to this family:
 *   - Plain fixed-offset byte field (`movzx`/`movsx eax, BYTE [eax+K]`):
 *     writes only .value. No word or dword fields in this class's real
 *     candidate set at all.
 *   - ctx-dynamic-index byte field: `mov edx,[edx+0x4]` (ctx's own +0x4
 *     slot index) with NO lea premultiply, bare stride-1 load
 *     `[eax+edx*1+K]` -- same bare/unscaled shape as CSTGMS20's
 *     GetInputJack. Used by the Up/Low-Drawbar and EX-Drawbar-Pitch-Up/Low
 *     and EXPercDrawbar group (5 of the 13). Modeled via the same
 *     CtxIndex(ctx, off, stride) helper as the rest of the family.
 * Zero outliers within the real 13-candidate set -- every genuine
 * exclusion in this class was already filtered out up front by the
 * linkage check above, not discovered mid-decode.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGOrganOsc {
	STGConvertedParam &GetDrawbarLevelCurve(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEXDrawbarModeLow(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEXDrawbarModeUp(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEXDrawbarPitchLow(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEXDrawbarPitchUp(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEXPercDrawbar(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEnvelopeType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLowDrawbar(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLowerOctaveShift(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPercAssignValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSplitPoint(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUpDrawbar(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetUpperOctaveShift(CSTGPatchMessageContext &ctx);
};

#endif
