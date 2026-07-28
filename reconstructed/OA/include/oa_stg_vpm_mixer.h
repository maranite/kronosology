// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_VPM_MIXER_H
#define OA_STG_VPM_MIXER_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_vpm_mixer.h  -  CSTGVPMMixer's value-getter family: all 4 real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGVPMMixer is the VPM engine's own per-operator level/phase mixer
 * patch component -- confirmed genuinely fresh via a word-boundary grep
 * before starting, no pre-existing struct or ctor anywhere in this
 * project.
 *
 * Dialect: a single ctx-indexed record shared by all four fields --
 * `mov edx,[edx+0x4]; lea edx,[edx+edx*4]` premultiplies ctx's own
 * dynamic-index field by 5, then every field load carries an explicit x2
 * SIB scale on top (`[eax+edx*2+K]`) -- effective stride 10, the same
 * lea-plus-SIB-scale variant first confirmed on CSTGMS20's own
 * Standard- and Mixer-prefixed AMS group. GetLevel and GetLevelAMSIntensity are dual-write
 * dwords; GetLevelAMSSource is a signed byte; GetPhaseInvert is the
 * "unsigned non-bitfield byte" variant -- movzx, no shift/mask -- first
 * confirmed on CSTGPolysixMG's own GetMIDITempoSyncTimes.
 */

static inline int CtxIndex(CSTGPatchMessageContext &ctx, unsigned int off, unsigned int stride)
{
	return *(int *)((unsigned char *)&ctx + off) * (int)stride;
}

struct CSTGVPMMixer {
	STGConvertedParam &GetLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPhaseInvert(CSTGPatchMessageContext &ctx);
};

#endif
