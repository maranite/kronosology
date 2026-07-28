// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_POLYSIX_MG_H
#define OA_STG_POLYSIX_MG_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_polysix_mg.h  -  CSTGPolysixMG's value-getter family: all 18
 * real weak-symbol ctx-only candidates reconstructed, zero outliers --
 * see include/oa_stg_string.h for the pilot class's full derivation.
 * CSTGPolysixMG is the modulation-generator, LFO-style, sub-component of
 * the Korg Polysix analog voice model -- Frequency, MIDI-tempo-sync,
 * and Delay parameters, per its own field names below -- confirmed
 * distinct from the already-done CSTGPolysix and CSTGPolysixModelPatch
 * classes, and genuinely fresh via a word-boundary grep before starting,
 * no pre-existing struct or ctor anywhere in this project.
 *
 * Dialect: the SIMPLEST yet again -- every one of the 18 real candidates
 * is a fixed-K field read directly off this, zero ctx-dynamic-index
 * methods of any kind, matching CSTGEPModelPatch, CSTGPolysixModelPatch
 * and CSTGMS20EG's own zero-ctx-index dialects.
 *
 * Field-shape summary:
 *   - GetKeySync and GetMIDITempoSync pack two independent single-bit
 *     booleans into one byte at plus-0x27 -- KeySync bit 1, MIDITempoSync
 *     bit 2, both via the shift-then-mask bitfield shape first seen in
 *     CSTGVPMOsc/CSTGMS20ModelPatch -- single-write only.
 *   - GetMIDITempoSyncBaseNote is a plain signed byte, single-write.
 *   - GetMIDITempoSyncTimes is notable: an UNSIGNED byte field --
 *     movzx, no shift or mask -- rather than the family's usual signed
 *     movsx byte read -- still single-write, same as every other byte
 *     field in this class, just zero-extended instead of sign-extended.
 *     A genuinely new width/sign variant for the family, not seen
 *     before on a plain non-bitfield byte read.
 *   - GetMIDITempoSyncTimesAMSSource, GetMIDITempoSyncTimesAMSIntensity,
 *     GetMIDITempoSyncTimesAMSIntModSource and
 *     GetMIDITempoSyncTimesAMSIntModIntensity are ALL plain signed
 *     bytes, single-write -- unlike this class's own Frequency/Delay
 *     AMSIntensity siblings below, which are dwords. Confirmed by
 *     direct disassembly rather than assumed from the field name alone:
 *     the "Intensity"-suffixed MIDITempoSyncTimes siblings do not carry
 *     the usual dword-width, dual-write shape their name might suggest.
 *   - Every other plain 32-bit field -- GetFrequency, GetDelay, and
 *     their first-level AMSIntensity/AMSIntModIntensity siblings --
 *     dual-writes .value and .displayValue.
 *   - Every other plain 8-bit field -- GetFrequencyAMSSource,
 *     GetFrequencyAMSIntModSource, GetDelayAMSSource,
 *     GetDelayAMSIntModSource -- signed, single-write.
 * No exceptions to the width-vs-dual-write rule found beyond the
 * MIDITempoSyncTimes group's own byte-width AMS siblings noted above --
 * every dword field dual-writes, every signed or unsigned byte and the
 * two boolean bits single-write.
 */

struct CSTGPolysixMG {
	STGConvertedParam &GetDelay(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDelayAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDelayAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDelayAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetDelayAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequency(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetFrequencyAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetKeySync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSync(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSyncBaseNote(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSyncTimes(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSyncTimesAMSIntModIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSyncTimesAMSIntModSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSyncTimesAMSIntensity(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetMIDITempoSyncTimesAMSSource(CSTGPatchMessageContext &ctx);
};

#endif
