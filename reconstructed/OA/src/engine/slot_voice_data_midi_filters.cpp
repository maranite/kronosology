// SPDX-License-Identifier: GPL-2.0
/*
 * slot_voice_data_midi_filters.cpp  -  CSTGSlotVoiceData::
 * UpdateAllActiveMIDIFilters() (batch 16, sec 10.163, `.text+0xb8a50`,
 * 624 bytes), a newly-discovered dependency of `CSTGControllerRTData::
 * SetControllerAssignment()`'s own unconditional commit path.
 *
 * Confirmed real: takes no meaningful argument and ignores its own
 * `this` entirely -- the whole disassembly never references any
 * `this`-derived value (same "operates purely via a global singleton"
 * shape already confirmed for `CSTGControllerRTData::NotifySoloChange()`,
 * sec 10.107) -- modeled `static` for exactly that reason.
 *
 * Walks the SAME 16-entry, 12-byte-stride `CSTGGlobal::sInstance+
 * 0x29c990c` active-voice-data-node table already confirmed for
 * `CSTGProgramSlot::ResolveActiveVoiceDataNode()` (sec 10.142,
 * global.cpp) -- independently re-derived here from this function's own
 * disassembly (16 manually-unrolled `mov eax,[edx+OFFSET]` checks,
 * OFFSET stepping by 0xc from 0x29c990c to 0x29c99c0 inclusive) rather
 * than assumed from that earlier finding, and it matches exactly. For
 * each non-null node: dereferences its own `+0x8` payload field with NO
 * separate null check on the payload itself (`mov eax,[eax+8];
 * cmp byte[eax+0x40],0` -- no intervening `test`/`je`) -- the exact same
 * lack-of-payload-null-check already confirmed for
 * `ResolveActiveVoiceDataNode()`'s own callers (`IsActive()` et al, sec
 * 10.142), so preserved here as a real, confirmed "node non-null implies
 * payload non-null" invariant rather than added defensively. If the
 * payload's `+0x40` byte is 0, calls the payload's own
 * `UpdateMIDIFilterAndResendAllCCs()`.
 *
 * `UpdateMIDIFilterAndResendAllCCs()` itself (`.text+0xb8610`, 1075
 * bytes) is substantially larger and out of scope this pass --
 * deliberately deferred with its own no-op body given directly in the
 * SIBLING file `slot_voice_data_update_midi_filter_resend.cpp` (not
 * bar2_stubs.cpp, which is never linked into any verify/ binary --
 * matching the `CSTGRecordTrack::StandbyRec()` precedent, sec 10.162),
 * deliberately kept in its OWN translation unit rather than this one so
 * `verify/test_slot_voice_data_midi_filters.cpp` can link this file
 * alone and provide its own call-recording MOCK of that sibling to
 * verify the dispatch/skip logic above -- linking both the real no-op
 * and a test mock in the same binary would be a duplicate-definition
 * link error.
 */

#include "oa_global.h"

void CSTGSlotVoiceData::UpdateAllActiveMIDIFilters()
{
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	for (unsigned int idx = 0; idx < 16; idx++) {
		unsigned int nodePacked = *(unsigned int *)(g + 0x29c990c + idx * 12);
		if (!nodePacked)
			continue;
		unsigned char *node = (unsigned char *)(unsigned long)nodePacked;

		/* Confirmed real: no null check on the payload itself. */
		unsigned int payloadPacked = *(unsigned int *)(node + 0x8);
		unsigned char *payload = (unsigned char *)(unsigned long)payloadPacked;

		if (payload[0x40] == 0)
			((CSTGSlotVoiceData *)payload)->UpdateMIDIFilterAndResendAllCCs();
	}
}
