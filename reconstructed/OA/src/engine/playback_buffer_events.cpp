// SPDX-License-Identifier: GPL-2.0
/*
 * playback_buffer_events.cpp  -  CSTGPlaybackBuffer's event-management
 * cluster (batch 24): EventBufferStartLocationUpdated/SetCurrentReadEvent/
 * AdvanceToNextFillEvent/HandleAdvanceCancelledEvent/AddEvent/
 * AdvanceReadPosition, plus the one small dependency they collectively
 * need, CSTGDiskCostManager::UpdateHDRBufferWaterMarks() (implemented in
 * engine_startup_bits2.cpp alongside CSTGDiskCostManager::Initialize()).
 * Batch 25 adds RemoveEvent()/EventFileError() (see their own header
 * comment below) -- now tractable now that CSTGPlaybackEvent::Reset(),
 * their real confirmed vtable-slot-7 dispatch target, is real too (see
 * playback_event_methods.cpp). This file and playback_event_methods.cpp
 * have a genuine mutual dependency (HandleFileClosed() there calls
 * RemoveEvent() here); both are linked together wherever either is
 * needed, including the real .ko build.
 *
 * Deliberately its own dedicated TU (matching the CSTGRecordTrack/
 * CSTGStreamingEventManager/CSTGMIDIClockSync precedent, sec 10.145/
 * 10.162/10.169): these methods dereference the embedded
 * `CSTGHDRCircularBuffer` sub-object's own already-real
 * `AdvanceReadPosition()`/`ReturnUnusedFillBytes()` (managers.cpp) and
 * `TSTGArrayManager<CSTGPlaybackEvent>::sInstance`. NOTE: unlike a
 * verify/-only KAT, this file IS linked into the real .ko alongside
 * engine_init.cpp (OA-objs) -- engine_init.cpp's own generic (non-
 * specialized) `template <typename T> TSTGArrayManager<T> *
 * TSTGArrayManager<T>::sInstance;` line already implicitly instantiates
 * this exact specialization (it explicitly uses
 * `TSTGArrayManager<CSTGPlaybackEvent>::sInstance` itself), so this file
 * must NOT also provide an explicit-specialization definition here --
 * doing so caused a real `ld` "multiple definition" error the first time
 * this was tried (`OA.o` link, batch 24). The sec 10.160 "give it its own
 * local storage" fix applies only to STANDALONE verify/ binaries that
 * don't link engine_init.cpp (see verify/test_playback_buffer_events.cpp,
 * which does carry that line, matching the managers.cpp/
 * TSTGArrayManager<CSTGRecordBuffer> precedent exactly: managers.cpp
 * itself never defines that storage either, only verify/ files that
 * skip engine_init.cpp do).
 *
 * Ground-truthed via objdump -dr against
 * /home/share/Decomp/OA.ko_Decomp/OA.ko (batch 24 local copy, not the
 * 192.168.3.92 build host):
 *   EventBufferStartLocationUpdated  .text+0xd6b90,  12B
 *   SetCurrentReadEvent              .text+0xd6b40,  65B
 *   AdvanceToNextFillEvent           .text+0xd69c0,  65B
 *   HandleAdvanceCancelledEvent      .text+0xd6a10,  67B
 *   AddEvent                         .text+0xd6540,  89B
 *   AdvanceReadPosition(m,b)         .text+0xd6620,  60B
 *
 * Confirmed field layout used here (see oa_engine.h's own
 * CSTGPlaybackBuffer comment for the summary):
 *   +0x00..0x33  embedded CSTGHDRCircularBuffer (offset 0, confirmed via
 *                Initialize()/ctor, hdr_manager_init.cpp/managers.cpp)
 *   +0x0c        (== CSTGHDRCircularBuffer::readPos) doubles as a cached
 *                mirror of the CURRENT READ event's own "+0x40" start-
 *                location pointer
 *   +0x34        unsigned short[0xfa1] ring of CSTGPlaybackEvent array-
 *                indices (the CSTGBankMemory::AllocAligned(0x1f42,0x10)
 *                buffer Initialize() allocates -- 0x1f42/2 == 0xfa1)
 *   +0x38        ring write cursor
 *   +0x3c        ring read cursor
 *   +0x40        ring capacity (constant 0xfa1, set by Initialize())
 *   +0x44        byte flag: "has a pending fill event" (0/1)
 *   +0x48        current FILL CSTGPlaybackEvent* (packed 32-bit)
 *   +0x4c        current READ CSTGPlaybackEvent* (packed 32-bit)
 *
 * CSTGPlaybackEvent itself stays the project's existing opaque
 * `_unrecovered[0x68]` (oa_engine_init.h) -- accessed here via raw byte
 * offsets, same treatment as every other still-opaque class in this
 * codebase. Confirmed per-event fields touched by these methods (byte
 * offsets into the CSTGPlaybackEvent object itself, NOT CSTGPlaybackBuffer):
 *   +0x04  unsigned short, the event's own sequential array-index
 *          (stamped once by BuildArrayManager(), engine_init.cpp -- read
 *          here, never written)
 *   +0x08  state tag (1 = queued/pending, 2 = current, 3 = finished)
 *   +0x16  byte flag, set to 1 when an event is displaced as the current
 *          read event while still mid-flight (real semantics not fully
 *          named -- "advance/cancel requested" is the best fit found)
 *   +0x1d  byte, a per-frame byte-size multiplier (paired with +0x44
 *          below to compute a byte count for ReturnUnusedFillBytes())
 *   +0x2c  byte, mirrors CSTGPlaybackBuffer's own embedded
 *          CSTGHDRCircularBuffer::field00 at the moment the event is
 *          queued (AddEvent)
 *   +0x30  back-reference: the owning CSTGPlaybackBuffer* (packed 32-bit)
 *   +0x40  pointer field: the event's own confirmed "buffer start
 *          location" (a char*, matches EventBufferStartLocationUpdated's
 *          own 2nd argument type)
 *   +0x44  unsigned int, a "remaining/unused frame count" --
 *          HandleAdvanceCancelledEvent multiplies it by the +0x1d
 *          per-frame size and returns the resulting BYTE count to the
 *          circular buffer's free-fill pool, then clears it to 0
 *   +0x54  pointer field: some other associated event (role not
 *          independently named -- SetCurrentReadEvent only ever compares
 *          the new event's own address against it)
 */

#include "oa_engine.h"
#include "oa_engine_init.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

void CSTGPlaybackBuffer::EventBufferStartLocationUpdated(CSTGPlaybackEvent *event, char *newLoc)
{
	unsigned char *base = (unsigned char *)this;

	if (*(unsigned int *)(base + 0x4c) == ToU32(event))
		((CSTGHDRCircularBuffer *)this)->readPos = ToU32(newLoc);
}

void CSTGPlaybackBuffer::SetCurrentReadEvent(CSTGPlaybackEvent *newEvt)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *evtNew = (unsigned char *)newEvt;
	unsigned char *evtOld = FromU32(*(unsigned int *)(base + 0x4c));

	if (evtOld != evtNew) {
		if (evtOld != 0 && *(unsigned int *)(evtOld + 0x8) != 3) {
			*(unsigned int *)(evtOld + 0x8) = 3;
			if (ToU32(evtNew) != *(unsigned int *)(evtOld + 0x54))
				evtOld[0x16] = 1;
		}
		*(unsigned int *)(base + 0x4c) = ToU32(evtNew);
	}

	*(unsigned int *)(evtNew + 0x8) = 2;

	/* Real disassembly reloads this->+0x4c and re-derives its own +0x40
	 * rather than reusing evtNew directly -- both paths above land on
	 * this->+0x4c == evtNew either way, so this is behaviorally identical
	 * to just using evtNew, but kept as a reload to match instruction
	 * order exactly. */
	unsigned char *cur = FromU32(*(unsigned int *)(base + 0x4c));
	unsigned int startLoc = *(unsigned int *)(cur + 0x40);
	if (startLoc != 0)
		((CSTGHDRCircularBuffer *)this)->readPos = startLoc;
}

void CSTGPlaybackBuffer::AdvanceToNextFillEvent()
{
	unsigned char *base = (unsigned char *)this;

	if (*(unsigned int *)(base + 0x48) == 0)
		return;

	unsigned int readIdx = *(unsigned int *)(base + 0x3c);
	if (*(unsigned int *)(base + 0x38) == readIdx) {
		*(unsigned int *)(base + 0x48) = 0;
		return;
	}

	unsigned int capacity = *(unsigned int *)(base + 0x40);
	unsigned int newIdx = (readIdx + 1) % capacity;
	unsigned short *ring = (unsigned short *)FromU32(*(unsigned int *)(base + 0x34));

	*(unsigned int *)(base + 0x3c) = newIdx;

	unsigned short elemIdx = ring[readIdx];
	unsigned int *indexArray = (unsigned int *)FromU32(TSTGArrayManager<CSTGPlaybackEvent>::sInstance->indexArray);
	*(unsigned int *)(base + 0x48) = indexArray[elemIdx];
}

void CSTGPlaybackBuffer::HandleAdvanceCancelledEvent(CSTGPlaybackEvent *event)
{
	unsigned char *evt = (unsigned char *)event;

	if (*(unsigned int *)(evt + 0x8) != 3)
		return;

	unsigned int remainingFrames = *(unsigned int *)(evt + 0x44);
	if (remainingFrames == 0)
		return;

	unsigned int bytesPerFrame = evt[0x1d];
	((CSTGHDRCircularBuffer *)this)->ReturnUnusedFillBytes(remainingFrames * bytesPerFrame);
	*(unsigned int *)(evt + 0x44) = 0;
}

void CSTGPlaybackBuffer::AddEvent(CSTGPlaybackEvent *event)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *evt = (unsigned char *)event;

	if (*(unsigned int *)(base + 0x48) != 0) {
		unsigned int writeIdx = *(unsigned int *)(base + 0x38);
		unsigned short *ring = (unsigned short *)FromU32(*(unsigned int *)(base + 0x34));
		unsigned short elemIdx = *(unsigned short *)(evt + 0x4);

		ring[writeIdx] = elemIdx;

		unsigned int capacity = *(unsigned int *)(base + 0x40);
		*(unsigned int *)(base + 0x38) = (writeIdx + 1) % capacity;
	} else {
		*(unsigned int *)(base + 0x48) = ToU32(event);
		base[0x44] = 1;
	}

	evt[0x2c] = (unsigned char)*(unsigned int *)(base + 0x0);
	*(unsigned int *)(evt + 0x30) = ToU32(this);
	*(unsigned int *)(evt + 0x8) = 1;
}

void CSTGPlaybackBuffer::AdvanceReadPosition(unsigned long n, bool updateWaterMarks)
{
	((CSTGHDRCircularBuffer *)this)->AdvanceReadPosition(n);

	if (updateWaterMarks) {
		unsigned int availableReadBytes = ((CSTGHDRCircularBuffer *)this)->availableReadBytes;
		CSTGDiskCostManager::sInstance->UpdateHDRBufferWaterMarks(availableReadBytes, (CSTGHDRCircularBuffer *)this);
	}
}

/*
 * RemoveEvent()/EventFileError() (batch 25, `.text+0xd65a0`/
 * `.text+0xd6a60`, 121B/212B) -- both confirmed to share a BYTE-FOR-BYTE
 * IDENTICAL tail sequence (return unused fill bytes, retire the current-
 * read-event pointer if it matches, dispatch the event's own vtable slot
 * 7, then push the event back onto TSTGArrayManager<CSTGPlaybackEvent>'s
 * own free list) -- factored into PlaybackEventReclaimAndRecycle() below,
 * matching this project's own sec 10.167 "identical instruction sequence
 * -> shared static helper" technique. `EventFileError()` additionally has
 * its own leading block (only reached when the event argument IS the
 * current FILL event) that is ALSO confirmed byte-for-byte identical to
 * this class's own already-real `AdvanceToNextFillEvent()` -- called
 * directly rather than re-transcribed inline, since the real disassembly
 * duplicating it (rather than emitting a `call`) is just this project's
 * own compiler's inlining choice, not a behavioral requirement to
 * reproduce literally (this project already applies this same "call the
 * existing real method instead of re-deriving an identical block"
 * judgment elsewhere).
 *
 * The event's own vtable slot 7 (`call *0x1c(%edx)`) is confirmed via
 * readelf relocation resolution against
 * `.rodata._ZTV17CSTGPlaybackEvent` to be `CSTGPlaybackEvent::Reset()`
 * -- the ONLY site in the whole real binary that ever installs this
 * exact vtable pointer is CSTGPlaybackEvent's own ctor, and nothing
 * derives from it, so there is no second possible dispatch target to
 * model: reproduced as a direct `event->Reset()` call.
 *
 * Real, confirmed quirk (found by hand-tracing the disassembly, not by
 * a KAT failure): both functions SAVE the event's own `+0x10` field
 * (`CSTGAudioEvent::field10`, a field `CSTGAudioEvent::Reset()` itself
 * unconditionally zeroes as part of the dispatched `Reset()` call)
 * BEFORE the dispatch, and RESTORE the saved value immediately
 * afterward -- net effect, this one field alone survives the Reset()
 * call untouched, even though every other field Reset() zeroes does
 * get cleared. Faithfully reproduced, not "simplified away".
 */
static void PlaybackEventReclaimAndRecycle(CSTGPlaybackBuffer *bufThis, CSTGPlaybackEvent *event)
{
	unsigned char *bufBase = (unsigned char *)bufThis;
	unsigned char *evt = (unsigned char *)event;

	unsigned int bytesPerFrame = evt[0x1d];
	unsigned int remainingFrames = *(unsigned int *)(evt + 0x44);
	((CSTGHDRCircularBuffer *)bufThis)->ReturnUnusedFillBytes(remainingFrames * bytesPerFrame);

	if (*(unsigned int *)(bufBase + 0x4c) == ToU32(event))
		*(unsigned int *)(bufBase + 0x4c) = 0;

	unsigned int savedField10 = *(unsigned int *)(evt + 0x10);
	event->Reset();
	*(unsigned int *)(evt + 0x10) = savedField10;

	TSTGArrayManager<CSTGPlaybackEvent> *mgr = TSTGArrayManager<CSTGPlaybackEvent>::sInstance;
	unsigned int *bucket = (unsigned int *)FromU32(mgr->bucketArray);
	bucket[mgr->writeCursor] = ToU32(event);
	mgr->writeCursor = (mgr->writeCursor + 1) % mgr->modulus;
}

void CSTGPlaybackBuffer::RemoveEvent(CSTGPlaybackEvent *event)
{
	unsigned char *evt = (unsigned char *)event;
	if (evt[0x16] != 0)
		return;
	PlaybackEventReclaimAndRecycle(this, event);
}

void CSTGPlaybackBuffer::EventFileError(CSTGPlaybackEvent *event)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *evt = (unsigned char *)event;

	if (*(unsigned int *)(base + 0x48) == ToU32(event)) {
		if (event != 0)
			AdvanceToNextFillEvent();
	}

	if (evt[0x16] != 0)
		return;

	PlaybackEventReclaimAndRecycle(this, event);
}
