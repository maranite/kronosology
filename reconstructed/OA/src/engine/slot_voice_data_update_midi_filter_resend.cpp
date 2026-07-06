// SPDX-License-Identifier: GPL-2.0
/*
 * slot_voice_data_update_midi_filter_resend.cpp  -  CSTGSlotVoiceData::
 * UpdateMIDIFilterAndResendAllCCs() (batch 16, sec 10.163, `.text+0xb8610`,
 * 1075 bytes).
 *
 * Confirmed real, deliberately deferred: substantially larger than its
 * own caller (`UpdateAllActiveMIDIFilters()`, src/engine/
 * slot_voice_data_midi_filters.cpp) and out of scope for this pass. Own
 * no-op body given directly here (not bar2_stubs.cpp, which is never
 * linked into any verify/ binary -- matching the `CSTGRecordTrack::
 * StandbyRec()` precedent, sec 10.162), and deliberately kept in its OWN
 * translation unit, separate from `UpdateAllActiveMIDIFilters()` itself,
 * so `verify/test_slot_voice_data_midi_filters.cpp` can link that
 * sibling file alone and provide its own call-recording mock of this
 * function instead of linking this real no-op body.
 */

#include "oa_global.h"

void CSTGSlotVoiceData::UpdateMIDIFilterAndResendAllCCs() {}
