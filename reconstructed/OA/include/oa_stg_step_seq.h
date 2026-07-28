// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_STEP_SEQ_H
#define OA_STG_STEP_SEQ_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_step_seq.h  -  CSTGStepSeq's value-getter family: all 14 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGStepSeq is the STG step-sequencer LFO patch component -- confirmed
 * DISTINCT from CSTGStepSeqBase, an unrelated already-declared stub in
 * oa_engine_init.h, same precedent as CSTGPolysix versus CSTGPolysixModel.
 * 2 of its 16 pending Get symbols are global linkage with a different
 * signature entirely -- Get Key Sync Master Step Seq taking a CSTGVoice
 * pointer, Get Output taking two ints -- excluded up front by the
 * standard weak-linkage plus ctx-only-suffix filter, not fed to the
 * decoder.
 *
 * Dialect: mostly fixed-K fields off this, plus a real ctx-dynamic-index
 * sub-family on the per-step Value slash Duration slash Times group --
 * the family's bare stride-1 SIB shape, ctx's own plus-0x4 field used
 * directly with no lea premultiply and no extra SIB scale, first
 * confirmed on CSTGMS20's own Get Input Jack and reused unchanged here.
 * Modeled via the same CtxIndex helper used across the family.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: dual-writes value and displayValue.
 *   - Plain 8-bit field: signed fields single-write value only, the two
 *     unsigned byte fields End Step and Start Step ALSO single-write --
 *     reconfirms width alone never implies dual-write in this family.
 *   - Byte 0x10 packs two independent single-bit booleans: One Shot bit 0
 *     no shift, Key Sync bit 1.
 * No outliers of any kind in this class.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGStepSeq {
	STGConvertedParam &GetAMSResetSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAttack(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDecay(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEndStep(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeySync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOneShot(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetResetThreshold(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStartStep(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStartStepAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStartStepAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStepDuration(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStepTimes(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetStepValue(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetValueAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
