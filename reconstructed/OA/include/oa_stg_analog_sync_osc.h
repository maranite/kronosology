// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_ANALOG_SYNC_OSC_H
#define OA_STG_ANALOG_SYNC_OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_analog_sync_osc.h  -  CSTGAnalogSyncOsc's "value getter" family:
 * 63 STGConvertedParam &Get*(CSTGPatchMessageContext &) methods, sixth
 * class reconstructed via the STG value-getter family's scripted
 * instruction-pattern decoder -- see include/oa_stg_string.h for the
 * pilot class's full derivation and
 * src/engine/stg_analog_sync_osc_valuegetters.cpp for this class's own
 * decoder note.
 *
 * CSTGAnalogSyncOsc is the STG analog-modeled oscillator-sync patch
 * component -- master/slave dual-oscillator hard-sync model, per its
 * Waveform*, Master*, RingMod*, SubOsc* and Noise* parameter names below.
 * Declared here as a minimal, deliberately OPAQUE class, same convention
 * as the rest of the family: no named members, no base class, no vtable
 * modeled. No ctx-dynamic-index sub-family in this class -- this stays in
 * eax throughout every method.
 *
 * Two NEW field shapes relative to the earlier classes in this family,
 * both genuine boolean truth-value tests rather than a plain-bit
 * extraction: GetRingModModulatorSelect and GetRingModCarrierSelect each
 * load a dword field, test it against zero, and store 1 if the field is
 * nonzero else 0. GetSubOscAudioInModeSelect does the inverse test --
 * stores 1 only when the field is exactly zero. All three are
 * mechanically decodable integer truth-value tests, not numeric
 * transforms, so all three are included in this batch rather than
 * excluded as outliers.
 *
 * 2 genuine outliers excluded -- real ground truth weak/COMDAT symbols,
 * NOT part of the mechanical-copy decoder vocabulary: GetNoiseSaturation
 * is a real fyl2x-based log2/dB-style conversion, same outlier class
 * already seen on CSTGString's own method of the same name.
 * GetNoiseCutoff computes a real square root of a field via SSE sqrtss --
 * a new but equally genuine numeric-transform outlier, same rationale as
 * CSTGString's GetPluckDelay pair. Both left pending for a future batch.
 *
 * Field-shape summary:
 *   - Plain 32-bit field: writes BOTH .value and .displayValue.
 *   - Plain 8-bit or 16-bit field, signed or unsigned, all four widths
 *     observed in this class: writes only .value.
 *   - Boolean nonzero/zero test on a 32-bit field (see above): writes
 *     only .value.
 */

struct CSTGAnalogSyncOsc {
	STGConvertedParam &GetBalanceM(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetBalanceS(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetEdge(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFMAmount(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFMAmountAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFMAmountAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetInitialPhaseM(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetInitialPhaseS(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevelM(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetLevelS(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterFreqOffset(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterWaveformAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterWaveformAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterWidthAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMasterWidthAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseBalance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoiseLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetNoisePhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPhaseInvertM(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPhaseInvertS(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModBalance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModCarrierSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModMode(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModModulatorSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetRingModPhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveFreqOffset(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveWaveformAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveWaveformAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveWidthAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSlaveWidthAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscAudioInModeSelect(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscBalance(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscBalanceAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscBalanceAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscLevel(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscLevelAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscLevelAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscPhaseInvert(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSubOscWaveform(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetSyncEnable(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveformM(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveformS(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveformSelectM(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWaveformSelectS(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWidthM(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetWidthS(CSTGPatchMessageContext &ctx);
};

#endif
