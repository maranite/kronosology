// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_EP_MODEL_PATCH_H
#define OA_STG_EP_MODEL_PATCH_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_ep_model_patch.h  -  CSTGEPModelPatch's value-getter family --
 * all 42 real weak-symbol Get*(CSTGPatchMessageContext&) candidates
 * reconstructed via the STG value-getter family's scripted
 * instruction-pattern decoder -- see include/oa_stg_string.h for the
 * pilot class's full derivation. CSTGEPModelPatch is the STG
 * electric-piano voice-model patch component: Tine is the Rhodes-style
 * layer, Reed is the Wurlitzer-style physical-model layer, IFX is the
 * model's own internal effect stage.
 *
 * SIMPLEST dialect seen in this family so far: every candidate is a
 * fixed-K field read directly off `this`, zero ctx-dynamic-index methods
 * of any kind -- no AMS-slot-array sub-family like CPianoOsc/CSTGPolysix/
 * CSTGMS20 have. Zero outliers of any kind -- second class in the family
 * with a full clean sweep, CSTGPolysix and CSTGMS20 were the first two.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: writes BOTH .value and .displayValue -- the
 *     *Intensity and main Tine- or Reed-prefixed level/drive/speed
 *     fields.
 *   - Plain 8-bit field, both movsx-signed and movzx-unsigned observed:
 *     writes only .value -- the AMSSource/AMSMode/OnOff/ModelType/
 *     IFXTypeParam/IFXEnable selector fields.
 * No exceptions to the width-vs-dual-write rule found in this class,
 * unlike CPianoOsc/CSTGString/CSTGPolysix, which each had a genuine
 * single-write dword outlier.
 */

struct CSTGEPModelPatch {
	STGConvertedParam &GetIFXEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetIFXTypeParam(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetModelType(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedCabinetDrive(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedCabinetDriveAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedCabinetDriveAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedCabinetOnOff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedCabinetOnOffAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedCabinetOnOffAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedSpeedAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedSpeedAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedVibrato(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedVibratoAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedVibratoAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedVolume(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedVolumeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetReedVolumeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineBass(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineBassAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineBassAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineCabinetDrive(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineCabinetDriveAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineCabinetDriveAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineCabinetOnOff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineCabinetOnOffAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineCabinetOnOffAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTinePreAmpVolume(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTinePreAmpVolumeAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTinePreAmpVolumeAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTreble(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTrebleAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTrebleAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloIntensityAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloIntensityAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloOnOff(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloOnOffAMSMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloOnOffAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloSpeed(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloSpeedAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetTineTremoloSpeedAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
