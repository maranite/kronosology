// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PLUCKED_MODEL_PATCH_H
#define OA_STG_PLUCKED_MODEL_PATCH_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_plucked_model_patch.h  -  CSTGPluckedModelPatch's value-getter
 * family: 6 of its real weak-symbol candidates are ctx-only Get*
 * accessors, all 6 reconstructed here, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGPluckedModelPatch is the plucked-string physical-model patch
 * component -- FeedbackDistance and FeedbackOrientation, each with its own
 * AMS source/intensity siblings -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * 5 other pending symbols excluded up front, not fed to the decoder --
 * GetRequiredVoiceInfo and SetupComponentOffsets and
 * GetFeedbackChannelLevels are global ('T') linkage with extra args beyond
 * ctx, GetEG(unsigned int) and GetLFO(unsigned int) are weak but take an
 * explicit index argument instead of the family's plain ctx-only
 * signature -- the standard extra-args/different-signature exclusion.
 *
 * Dialect: simplest fixed-K-off-this shape throughout, zero ctx-index
 * methods.
 *
 * Field-shape summary, all plain 32-bit dual-write fields except the two
 * AMSSource siblings which are plain signed 8-bit single-write fields:
 *   - FeedbackDistance at +0xc, FeedbackDistanceAMSIntensity at +0x10,
 *     FeedbackDistanceAMSSource at +0x14.
 *   - FeedbackOrientation at +0x18, FeedbackOrientationAMSIntensity at
 *     +0x1c, FeedbackOrientationAMSSource at +0x20.
 * No exceptions to the width-vs-dual-write rule found in this class.
 */

struct CSTGPluckedModelPatch {
	STGConvertedParam &GetFeedbackDistance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackDistanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackDistanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackOrientation(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackOrientationAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFeedbackOrientationAMSIntensity(CSTGPatchMessageContext &ctx);
};

#endif
