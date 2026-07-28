// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_EG_BASE_H
#define OA_STG_EG_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_eg_base.h  -  CSTGEGBase's value-getter family: only 5 of its 19
 * raw pending weak symbols are real ctx-only candidates -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGEGBase is the shared runtime base class underlying CSTGEG and
 * friends -- confirmed genuinely fresh via a word-boundary grep before
 * starting, no pre-existing struct or ctor anywhere in this project, and
 * confirmed DISTINCT from the already-modeled CSTGEG (a different,
 * unrelated class despite the similar name, same precedent as
 * CSTGPolysix versus CSTGPolysixModel).
 *
 * The other 14 raw pending symbols are NOT this family's convention at
 * all -- confirmed via the standard nm linkage and signature filter, same
 * outcome as CSTGOrganOsc's own per-voice-runtime-state false positives.
 * Ten are global linkage state-machine transition helpers taking a
 * segment-slice pointer and a voice pointer -- attack, decay, sustain,
 * release, hold, slope, free, quick-release, and the generic normal-state
 * dispatcher, plus a filter setup helper taking a segment-slice pointer
 * and a raw integer. Three more are global-linkage setters carrying real
 * extra arguments beyond ctx -- the EG type selector, an explicit control
 * value setter, and a piano half-damper mode flag setter. The last is a
 * global-linkage two-int accumulator query, unrelated to the ctx-based
 * value-getter convention entirely. All 14 excluded up front, none fed to
 * the decoder.
 *
 * Dialect: mixed. Level is ctx-indexed via a bare stride-4 SIB scale on a
 * dword field, dual-write -- the same bare-stride-4 shape confirmed on
 * CSTGMultiFilter2Pole and CSTGAmp. Time and Curve are BOTH ctx-indexed
 * via a bare stride-1 load with no lea premultiply and no extra SIB
 * scale -- the same bare-stride-1 shape first confirmed on CSTGMS20's own
 * GetInputJack -- but differ in field width and sign: Time reads a signed
 * byte, single-write, while Curve reads an UNSIGNED byte, single-write --
 * the first confirmed instance in this family of an unsigned byte load
 * combined with ctx-indexing rather than a plain fixed offset (the prior
 * unsigned-byte precedents, CSTGPolysixMG's MIDITempoSyncTimes and
 * CSTGVPMModelPatch's Algorithm, were both plain fixed-K fields). No new
 * decoder machinery needed -- the sign/width flag is already independent
 * of the ctx-index flag in the shared decoder. AMSResetSource is a plain
 * fixed-K signed byte, single-write; AMSResetThreshold is a plain fixed-K
 * dword, dual-write.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGEGBase {
	STGConvertedParam &GetLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetCurve(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSResetSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetAMSResetThreshold(CSTGPatchMessageContext &ctx);
};

#endif
