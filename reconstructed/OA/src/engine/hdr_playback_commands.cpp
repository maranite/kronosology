// SPDX-License-Identifier: GPL-2.0
/*
 * hdr_playback_commands.cpp  -  CSTGHDRManager::ProcessPlaybackCommands()
 * (batch 51).
 *
 * Deliberately a SEPARATE translation unit from managers.cpp/
 * hdr_record_track.cpp/hdr_sampler_commands.cpp, matching the established
 * CSTGStreamingEventManager/CSTGRecordTrack/CSTGSampler precedent (sec
 * 10.145/10.162, batch 50): keeps the pre-existing managers.cpp-linking
 * verify/ binaries untouched by this batch's new symbols. Confirmed via a
 * project-wide grep: no verify/ .cpp file carries a link-satisfying mock
 * for `CSTGHDRManager::ProcessPlaybackCommands` (unlike
 * ProcessSamplerCommands, which had six), so there is nothing to remove.
 *
 * `CSTGHDRManager::ProcessPlaybackCommands()` (`.text+0xd5950`, 453 bytes)
 * confirmed: a ring-buffer consumer loop, same overall shape as
 * `ProcessRecordCommands()`/`ProcessSamplerCommands()`, over its OWN THIRD
 * ring -- `CSTGHDRManager`'s own ctor comment in oa_engine.h flagged three
 * `CSTGBankMemory::AllocAligned()`-backed rings at `+0x18ad8`/`+0x18ae8`/
 * `+0x18af8`; `+0x18ae8` is ProcessRecordCommands()'s own ring and
 * `+0x18af8` is ProcessSamplerCommands()'s (batch 50) -- so `+0x18ad8`,
 * confirmed directly by THIS function's own disassembly, is this one:
 *   +0x18ad8 ring base pointer (packed 32-bit)
 *   +0x18adc producer index (never written here)
 *   +0x18ae0 consumer index (advanced here)
 *   +0x18ae4 capacity (modulus for wraparound)
 * Each entry is 12 bytes (0xc), confirmed via `lea eax,[edx+edx*2];
 * shl eax,2` (`edx*3*4 == edx*12`). `+0x04`/`+0x08` are read UNCONDITIONALLY
 * before the tag branch, matching the real disassembly's own shared
 * prologue:
 *   +0x00  tag (only the low byte read)
 *   +0x04  event      a `CSTGPlaybackEvent*` (packed 32-bit) -- used by
 *                      every tag
 *   +0x08  tag-dependent union: tag 3 = signed loop COUNT; tag 2 = a
 *          `CSTGFileOpener*`; tags 0/1 don't read it at all
 *
 * Dispatch on `tag` (`this` throughout; `event = (CSTGPlaybackEvent*)
 * entry->event`), confirmed via `.rel.text` relocation resolution against
 * ground truth for every call target:
 *
 *   tag==3: for i in [0, entry->count): calls
 *       `((CSTGPlaybackBuffer*)array[i]->field(0x30))->
 *            SetCurrentReadEvent((CSTGPlaybackEvent*)array[i])`
 *     over `this`'s own `+0x18a98` packed-pointer array (real length not
 *     independently confirmed beyond what `entry->count` supplies at
 *     runtime -- this array sits inside the ctor's already-documented
 *     "nine confirmed-zeroed dwords... through +0x18b00" span, oa_engine.h,
 *     now given a specific real use). Unconditionally clears a byte flag
 *     at `+0x18a95` afterward (also within that same span), even when
 *     `count <= 0`.
 *   tag==2: `entry->field8` is a `CSTGFileOpener*` ("opener"). Calls
 *       `opener->AddPlaybackEvent(event, producerIdx)` -- `producerIdx` is
 *       genuinely `*(u32*)(this+0x18adc)`, THIS ring's own current
 *       producer index (confirmed via register-level tracing: the `ecx`
 *       register holding this value is refreshed at every loop
 *       continuation point and never touched between that refresh and this
 *       call -- not a guessed/"probably don't-care" value, an exact
 *       transliteration of what the real disassembly passes). Then clears
 *       `event->fieldC` (`CSTGAudioEvent::fieldC`, ctor default 4 -- see
 *       oa_engine_init.h) to 0, and calls `CSTGFileOpener::sInstance->
 *       AddRecordEvent(event, *(u32*)opener)` -- the SECOND call's own
 *       index argument is a dereference of the SAME `opener` pointer's own
 *       leading field (`CSTGFileOpener::sInstance`, not `opener`, is
 *       `this` for this second call -- a real, confirmed asymmetry, not a
 *       transcription slip). Finally `signal_daemon(0)`.
 *   tag==1: index = `*(u16*)(event+0x2e)` (a field inside the still-opaque
 *       CSTGPlaybackEvent `_unrecovered` tail, within CSTGAudioEvent's own
 *       confirmed-but-unnamed `+0x2c..+0x38` range). Clears `event->fieldC`
 *       to 0, calls `CSTGFileOpener::sInstance->AddRecordEvent(event,
 *       index)`, then `signal_daemon(0)`.
 *   tag==0: walks a singly-linked list of "child" events hanging off
 *       `event->+0x58` (a real, ctor/Reset()-zeroed `CSTGPlaybackEvent`
 *       field -- see playback_event_methods.cpp -- newly identified here
 *       as a "next" pointer, not previously named). Clears `event->+0x16`
 *       (CSTGPlaybackEvent's own already-named flag byte) up front. For
 *       each list node whose own `+0x8` state is neither 0 nor 3
 *       (CSTGPlaybackBuffer::SetCurrentReadEvent/RemoveEvent's own already-
 *       established state-tag semantics): sets state=3, and if the node's
 *       own `+0xc` (CSTGAudioEvent::fieldC) is STILL the ctor-default 4
 *       (i.e. never cleared by a tag==1/2 command above), calls
 *       `node->field(0x30)->RemoveEvent((CSTGPlaybackEvent*)node)`. After
 *       the whole list is walked, applies the SAME "if fieldC==4: RemoveEvent,
 *       else: state=3" test to the TOP-LEVEL event itself.
 *   any other tag: no-op, entry silently consumed (matches every other
 *   ring consumer in this codebase -- consumer index still advances,
 *   computed unconditionally before the tag branch).
 *
 * `signal_daemon()`/`CSTGFileOpener::AddPlaybackEvent`/`AddRecordEvent` are
 * all newly real this same batch -- see oa_daemons.h/src/init/
 * stg_daemons.cpp and oa_engine.h/src/engine/file_opener_events.cpp.
 * `CSTGPlaybackBuffer::SetCurrentReadEvent`/`RemoveEvent` were already real
 * (batch 24/25, playback_buffer_events.cpp).
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_daemons.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

void CSTGHDRManager::ProcessPlaybackCommands()
{
	unsigned char *base = (unsigned char *)this;

	unsigned int consumerIdx = *(unsigned int *)(base + 0x18ae0);
	unsigned int producerIdx = *(unsigned int *)(base + 0x18adc);

	while (consumerIdx != producerIdx) {
		unsigned char *entry = FromU32(*(unsigned int *)(base + 0x18ad8)) + consumerIdx * 0xc;

		unsigned char tag = *(unsigned char *)(entry + 0x00);
		unsigned int  f04 = *(unsigned int *)(entry + 0x04);
		unsigned int  f08 = *(unsigned int *)(entry + 0x08);

		unsigned int nextIdx = (consumerIdx + 1) % *(unsigned int *)(base + 0x18ae4);
		*(unsigned int *)(base + 0x18ae0) = nextIdx;

		CSTGPlaybackEvent *event = (CSTGPlaybackEvent *)FromU32(f04);
		unsigned char *evt = (unsigned char *)event;

		if (tag == 3) {
			int count = (int)f08;
			for (int i = 0; i < count; i++) {
				unsigned char *node = FromU32(*(unsigned int *)(base + 0x18a98 + i * 4));
				CSTGPlaybackBuffer *owner =
					(CSTGPlaybackBuffer *)FromU32(*(unsigned int *)(node + 0x30));
				owner->SetCurrentReadEvent((CSTGPlaybackEvent *)node);
			}
			base[0x18a95] = 0;
		} else if (tag == 2) {
			CSTGFileOpener *opener = (CSTGFileOpener *)FromU32(f08);
			opener->AddPlaybackEvent((CSTGAudioEvent *)evt, producerIdx);

			((CSTGAudioEvent *)evt)->fieldC = 0;
			unsigned int idx2 = *(unsigned int *)FromU32(f08);
			CSTGFileOpener::sInstance->AddRecordEvent((CSTGAudioEvent *)evt, idx2);
			signal_daemon(0);
		} else if (tag == 1) {
			unsigned int idx = *(unsigned short *)(evt + 0x2e);
			((CSTGAudioEvent *)evt)->fieldC = 0;
			CSTGFileOpener::sInstance->AddRecordEvent((CSTGAudioEvent *)evt, idx);
			signal_daemon(0);
		} else if (tag == 0) {
			unsigned char *node = FromU32(*(unsigned int *)(evt + 0x58));
			evt[0x16] = 0;

			while (node != 0) {
				CSTGAudioEvent *nodeBase = (CSTGAudioEvent *)node;
				if (nodeBase->field8 != 0 && nodeBase->field8 != 3) {
					nodeBase->field8 = 3;
					if (nodeBase->fieldC == 4) {
						CSTGPlaybackBuffer *owner =
							(CSTGPlaybackBuffer *)FromU32(*(unsigned int *)(node + 0x30));
						owner->RemoveEvent((CSTGPlaybackEvent *)node);
					}
				}
				node = FromU32(*(unsigned int *)(node + 0x58));
			}

			if (((CSTGAudioEvent *)evt)->fieldC == 4) {
				CSTGPlaybackBuffer *owner =
					(CSTGPlaybackBuffer *)FromU32(*(unsigned int *)(evt + 0x30));
				owner->RemoveEvent(event);
			} else {
				((CSTGAudioEvent *)evt)->field8 = 3;
			}
		}
		/* any other tag: no-op, entry silently consumed. */

		consumerIdx = *(unsigned int *)(base + 0x18ae0);
		producerIdx = *(unsigned int *)(base + 0x18adc);
	}
}
