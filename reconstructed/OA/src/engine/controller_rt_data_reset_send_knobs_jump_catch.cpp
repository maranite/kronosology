// SPDX-License-Identifier: GPL-2.0
/*
 * controller_rt_data_reset_send_knobs_jump_catch.cpp  -  CSTGControllerRTData::
 * ResetSendKnobsJumpCatch() (batch 57).
 *
 * Deliberately its own dedicated TU (same reason as
 * controller_rt_data_set_audio_in_solo.cpp): test_engine.cpp/
 * test_global.cpp/test_global_ctor.cpp all link global.cpp directly and
 * each carry their own mock for this exact symbol (test_global.cpp's own
 * `g_resetSendKnobsJumpCatchCalls` is load-bearing) -- none of the three
 * link this file.
 *
 * .text+0x16730, 410 bytes, confirmed via full `objdump -dr` + a raw
 * `.rodata+0x1c4` jump-table dump (8 entries, indices 0..7, read from
 * `this->fieldAt(0x2b)` -- a signed byte, range-checked 0..7 via unsigned
 * `jbe`, so any negative byte value there safely no-ops instead of
 * indexing the table):
 *   0,1 -> mode dispatch (CSTGGlobal +0x684 == 0 ? Pgm : Combi)
 *   2,3 -> per-track bus-routing dispatch (STGAPIOutToBusType-gated)
 *   4,5,6 -> no-op (table entries point straight at the function's own
 *            tail/return, no call at all)
 *   7 -> UpdateJumpCatchWithAudioSendKnobValues()
 *
 * Guarded by THREE confirmed real preconditions, all-must-pass:
 *   1. `ResolveCurrentPerformance(CSTGGlobal::sInstance)->
 *      IsCurrentlyActive()` (the SAME resolve formula as
 *      CSTGAudioInput::UpdateBusSelect/CSTGControllerRTData::
 *      SetAudioInSolo, global.cpp/controller_rt_data_set_audio_in_solo.cpp
 *      -- externally linked there for exactly this kind of reuse).
 *   2. The resolved performance's own `+0xad7` byte == 0 (same gate byte
 *      CSTGAudioInput::UpdateBusSelect already reads off a
 *      CSTGControllerRTData-owned table entry -- here read off the
 *      PERFORMANCE object itself instead, a genuinely different object;
 *      not independently named beyond "gate byte", matching this
 *      project's own "reproduce faithfully, name what's confirmed"
 *      convention).
 *   3. `this->fieldAt(0x2b)` (signed byte) in [0,7].
 *
 * Cases 2/3's own per-track routing reuses the SAME `STGAPIOutToBusType`/
 * `STGAPIOutToPhysBusId` 26-entry tables CSTGAudioInputMixerBase::
 * SetOutputBus already established (sec 10.150, audio_input_mixer.cpp) --
 * duplicated here (both are tiny compile-time `.rodata` const arrays,
 * `static` in that file) rather than given external linkage, matching
 * this project's own precedent for small immutable table duplication
 * (as opposed to `ResolveCurrentPerformance`, a real function with actual
 * logic, which got external linkage instead). Two further giant
 * `CSTGGlobal`-relative tables (raw offsets `+0x27cdb08`/`+0x27cea0f`,
 * `0x1cad`-per-sequence-row stride matching `ResolveCurrentPerformance`'s
 * own mode-2 `seqIdx*0x1cad` multiplier -- presumably a sibling
 * per-sequence array with the same row count) are reproduced as raw
 * offset arithmetic ONLY -- their own semantic field names are NOT
 * independently determined, matching this project's established
 * "preserve real offset math faithfully even when the name isn't known"
 * convention (`_unrecoveredNN`-style fields elsewhere).
 *
 * FIVE newly-discovered confirmed-real, deliberately deferred sibling
 * callees (own bodies not reconstructed in this pass -- each is its own
 * substantial audio-routing/DSP-adjacent function, matching the
 * `CSTGControllerInfo::SetPerfSwitch`-class precedent for a real caller
 * dispatching into several not-yet-reconstructed real callees):
 *   UpdateJumpCatchWithPgmSendKnobValues()/
 *   UpdateJumpCatchWithCombiSendKnobValues()/
 *   UpdateJumpCatchWithAudioSendKnobValues()/
 *   UpdateAudioTrackSendJumpCatch(void *track, unsigned int, unsigned int)/
 *   UpdateJumpCatchWithIFXSendKnobValues(CSTGEffectRack&, int) -- `track`
 *   modeled as `void*` (not a real `CSTGHDRTrack&`, a class this project
 *   doesn't declare yet) matching the established "not-yet-modeled type"
 *   convention (e.g. `FinalizeSmoother`'s own `void*` TListLink param).
 */

#include "oa_global.h"
#include "oa_engine_init.h"

extern CSTGPerformance *ResolveCurrentPerformance(unsigned char *base);

extern "C" {
void CSTGControllerRTData_UpdateJumpCatchWithPgmSendKnobValues(void *self);
void CSTGControllerRTData_UpdateJumpCatchWithCombiSendKnobValues(void *self);
void CSTGControllerRTData_UpdateJumpCatchWithAudioSendKnobValues(void *self);
void CSTGControllerRTData_UpdateAudioTrackSendJumpCatch(void *self, void *track, unsigned int a, unsigned int b);
void CSTGControllerRTData_UpdateJumpCatchWithIFXSendKnobValues(void *self, void *effectRack, int busId);
}

static const int kSTGAPIOutToBusType[26] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 4, 3, 4, 3, 4, 3, 4, 2, 2, 2, 2, 0
};
static const int kSTGAPIOutToPhysBusId[26] = {
	48, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76,
	38, 38, 40, 40, 42, 42, 44, 44, 38, 40, 42, 44, 32
};

void CSTGControllerRTData::ResetSendKnobsJumpCatch()
{
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char *self = (unsigned char *)this;

	CSTGPerformance *perf = ResolveCurrentPerformance(g);
	if (!perf->IsCurrentlyActive())
		return;
	if (((unsigned char *)perf)[0xad7] != 0)
		return;

	int caseIdx = (signed char)self[0x2b];
	if ((unsigned int)caseIdx > 7)
		return;

	switch (caseIdx) {
	case 0:
	case 1:
		if (*(unsigned int *)(g + 0x684) == 0)
			CSTGControllerRTData_UpdateJumpCatchWithPgmSendKnobValues(self);
		else
			CSTGControllerRTData_UpdateJumpCatchWithCombiSendKnobValues(self);
		break;

	case 2:
	case 3: {
		unsigned int rowOff = *(unsigned int *)(g + 0x6a0) * 0x1cadu;
		unsigned char trackByte = g[0x27cdb08u + rowOff];
		unsigned int col = trackByte * 0x2cu + rowOff;
		signed char busIdx = (signed char)g[0x27cea0fu + col];

		if (kSTGAPIOutToBusType[busIdx] == 1) {
			void *effectRack = g + 0x27cd028u + rowOff;
			int physBusId = kSTGAPIOutToPhysBusId[busIdx];
			CSTGControllerRTData_UpdateJumpCatchWithIFXSendKnobValues(self, effectRack, physBusId);
		} else {
			void *track = g + 0x27cea0bu + col;
			CSTGControllerRTData_UpdateAudioTrackSendJumpCatch(self, track, trackByte, 0);
			CSTGControllerRTData_UpdateAudioTrackSendJumpCatch(self, track, trackByte, 1);
		}
		break;
	}

	case 4:
	case 5:
	case 6:
		break;

	case 7:
		CSTGControllerRTData_UpdateJumpCatchWithAudioSendKnobValues(self);
		break;
	}
}

/* Five confirmed-real, deliberately deferred siblings -- see this file's
 * own header comment. Flat `extern "C"` free functions (not real
 * CSTGControllerRTData methods) matching this project's established
 * convention for a not-yet-reconstructed real method exposed as a safe
 * link target under a distinct name. Deliberately only DECLARED (not
 * defined) here -- their own safe no-op bodies live in bar2_stubs_c.cpp
 * instead, so this file's own dedicated KAT
 * (test_controller_rt_data_reset_send_knobs_jump_catch.cpp) can supply
 * its own call-counting mocks without a multiple-definition link
 * conflict (same technique as smoother_finalize.cpp/
 * CSTGSmootherMapping_DispatchSmoothedValue). */
