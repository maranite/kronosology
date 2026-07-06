// SPDX-License-Identifier: GPL-2.0
/*
 * playback_subrate.cpp  -  CSTGPlaybackBuffer::ProcessSubRate() (batch 27,
 * `.text+0xd6660`, 860 bytes).
 *
 * Pre-scouted since sec 10.173 as CSTGPlaybackBuffer's last remaining
 * method; batches 25/26 resolved its own last two external dependencies
 * (`CSTGPlaybackEvent::GetDispositionForReadAttempt()`/
 * `USTGHDRUtils::ConvertWaveToSTGSamples()`), leaving zero unresolved
 * external calls (all four calls this function makes --
 * `GetDispositionForReadAttempt`/`ConvertWaveToSTGSamples`/the embedded
 * `CSTGHDRCircularBuffer::AdvanceReadPosition`/`CSTGDiskCostManager::
 * UpdateHDRBufferWaterMarks` -- are already real). This batch reconstructs
 * the function's own substantial 860-byte body: a real-time state machine
 * driving how many bytes get pulled from the ring buffer and converted into
 * a fixed per-tick output-sample budget, across however many queued
 * CSTGPlaybackEvent objects it takes to either fill that budget or run out
 * of events.
 *
 * Ground-truthed via objdump -dr against
 * /home/share/Decomp/OA.ko_Decomp/OA.ko (`.text+0xd6660`..`.text+0xd69c0`).
 *
 * ================= CSTGPlaybackBuffer's own additional fields =================
 * Beyond the already-documented +0x00..+0x4c (embedded CSTGHDRCircularBuffer,
 * ring management, fill/read event pointers -- playback_buffer_events.cpp),
 * this function is the first to read/use:
 *   +0x30  (== CSTGHDRCircularBuffer::_unrecovered_tail[0], the LAST of that
 *          embedded sub-object's own 4 unrecovered bytes) -- read here as a
 *          byte flag selecting the output destination: nonzero picks a FIXED
 *          default bus (`&sGlobalBusSet[32]`, i.e. `sGlobalBusSet + 0x1000`
 *          -- confirmed via the real R_386_32 relocation+addend on this
 *          exact immediate, NOT a bare literal 0x1000 as a first read of the
 *          raw hex might suggest), zero uses the per-instance `+0x54`
 *          pointer instead. Neither `CSTGPlaybackBuffer::Initialize()`
 *          overload (hdr_manager_init.cpp) nor `CSTGHDRManager::
 *          Initialize()`'s own per-instance loop (same file) ever sets this
 *          byte for a `CSTGPlaybackBuffer` instance, and the embedded
 *          `CSTGHDRCircularBuffer::CSTGHDRCircularBuffer()` ctor confirmed
 *          does NOT zero it either (only zeroes +0x0/+0x4/+0x8/+0xc/+0x10/
 *          +0x14/+0x18/+0x1c/+0x28/+0x2c) -- so in practice, for every real
 *          `CSTGPlaybackBuffer` instance (all living inside the statically-
 *          allocated `CSTGHDRManager`, zero-initialized by the module
 *          loader's own .bss clearing), this byte reads as 0 and the
 *          `+0x54`-pointer branch is what actually executes on real
 *          hardware -- the `sGlobalBusSet+0x1000` branch is real, reachable
 *          code, just not exercised by any currently-known caller/setup
 *          path. Accessed via raw offset here (not a named
 *          CSTGHDRCircularBuffer field) since `CSTGCDAudioPlay::
 *          Initialize()` already independently confirmed a DIFFERENT
 *          embedding class treats this same shared byte differently (always
 *          zeroes it, never reads it) -- the meaning is per-embedding-class,
 *          not a property of `CSTGHDRCircularBuffer` itself.
 *   +0x50  (byte, ctor-zeroed, managers.cpp) -- passed directly as
 *          `ConvertWaveToSTGSamples()`'s own `needsByteSwap` argument.
 *   +0x51  (byte, ctor-zeroed) -- passed directly as `sourceIsStereo`, AND
 *          selects the per-tick sample `threshold` (0x20 if zero/mono, 0x40
 *          if nonzero/stereo -- exactly double, matching stereo needing
 *          twice the per-frame element count for the same frame count).
 *   +0x52  (byte, ctor-zeroed) -- passed directly as `stereoInterleavedOutput`.
 *   +0x54  (already named/confirmed real, managers.cpp: default
 *          `&sGlobalBusSet[32]`, later overwritten per-instance by
 *          `CSTGHDRManager::Initialize()`'s own loop) -- the per-instance
 *          output bus destination `float*`, used here as `destBase` when
 *          `+0x30 == 0` (the confirmed-real-world case above).
 *
 * ================= CSTGPlaybackEvent's own additional fields =================
 * (raw offsets into `_unrecovered[0x68]`, matching this project's own
 * established "still-opaque class, raw byte offsets" convention --
 * playback_buffer_events.cpp/playback_event_methods.cpp already name
 * several of these; every name below matches THOSE files exactly, no
 * renaming):
 *   +0x08  CSTGAudioEvent::field8 -- state (1=queued, 2=current, 3=finished).
 *   +0x0c  CSTGAudioEvent::fieldC -- confirmed ctor-default value 4; compared
 *          against exactly 4 here to decide whether an outcome==1
 *          "exhausted" event gets immediately recycled onto
 *          `TSTGArrayManager<CSTGPlaybackEvent>`'s free list (fieldC==4, the
 *          common/default case) or instead kept alive to chase its own
 *          `+0x54` link (fieldC != 4 -- some not-yet-reconstructed setter
 *          changes this field away from its ctor default; role of the value
 *          itself not independently named).
 *   +0x16  "advance/cancel requested" byte flag (already named,
 *          playback_buffer_events.cpp) -- written here exactly as that file
 *          already established.
 *   +0x1d  CSTGAudioEvent::field1d -- per-sample byte width (1/2/3 for
 *          8/16/24-bit PCM, already named "bytesPerFrame"/"chunkSize" in
 *          sibling files) -- used here identically, as a byte-count
 *          multiplier.
 *   +0x38  windowSize (already named, playback_event_methods.cpp) --
 *          read/written here in the outcome==2/3 tail bookkeeping.
 *   +0x3c  consumed (already named, playback_event_methods.cpp's own
 *          per-EVENT "consumed" field -- distinct from this function's own
 *          per-CALL `consumed` local below, same name reused for two
 *          different real fields at two different scopes, matching
 *          upstream's own established name).
 *   +0x44  windowThreshold (already named, playback_event_methods.cpp) --
 *          the event's own "remaining/unused frame count"; clamps this
 *          call's `convertCount` down and gets decremented by however many
 *          samples were actually converted.
 *   +0x48  maxReadBytes (already named, playback_event_methods.cpp) -- the
 *          event's own readable-byte budget/cap.
 *   +0x50  eventField50 -- NOT independently named elsewhere; a per-event
 *          "byte budget still available from this event's own file read"
 *          counter, decremented by `grab` (the amount reserved from it)
 *          each chunk.
 *   +0x54  "associated/next event" pointer (already named,
 *          playback_buffer_events.cpp) -- confirmed here to be a genuine
 *          singly-linked "next queued event for this same stream" chain:
 *          once the current event is fully drained (outcome==1) it is
 *          retired (state=3) and, if also due for recycling (fieldC==4),
 *          pushed onto the free list BEFORE its own `+0x54` is followed to
 *          find the next event to make current -- i.e. the link survives
 *          the object being recycled (the free-list push never touches
 *          `+0x54`), and the recycle+advance sequence can repeat across
 *          MULTIPLE already-exhausted-and-recycled events in a single
 *          `ProcessSubRate()` call if they're chained back-to-back.
 *
 * CONFIRMED REAL QUIRK (found by hand-tracing, then verified by a real KAT
 * failure against a first, wrong test expectation -- see
 * verify/test_playback_subrate.cpp section [8]): `recycle_event` sets the
 * just-drained event's own state (`+0x8`) to 0, but `this->+0x4c` (the
 * "current read event" pointer) is NOT updated by recycling -- it still
 * points at that SAME event when `advance_to_next`'s own "retire the old
 * current-read-event" check immediately re-examines it as `curReadEvt`.
 * Since 0 != 3, that check unconditionally forces the state back to 3 a
 * moment later. Net effect: an event that gets recycled AND was also the
 * current read event ends up with `+0x8 == 3` afterward, not 0 -- it
 * visibly passes through 0 in between, but that value never survives to
 * the end of the call. Faithfully reproduced (not "fixed" as a supposed
 * bug): this is exactly what the real instruction sequence does.
 *
 * ================= Per-call control flow =================
 * `destBase`/`consumed`/`grab`/`convertCount`/`copyByteCount`/`outcome`
 * persist across the whole call (matching the real function's own stack
 * slots, which likewise survive across loop iterations and the outcome==2/3
 * "carry" reads below):
 *
 *   destBase = (+0x30 != 0) ? &sGlobalBusSet[32] : (float*)+0x54;
 *   threshold = (+0x51 == 0) ? 0x20 : 0x40;   // per-tick sample budget
 *   needsWaterMarkUpdate = (+0x48 != 0) | (+0x44 != 0);
 *   if (+0x44 != 0) +0x44 = 0;
 *   consumed = 0;
 *
 *   chunk_body:                          // first pass always starts here
 *     zero sConvertBuffer[0..0xFF]
 *     event = this->+0x4c (current read event); if NULL or event->state != 2: return.
 *     budgetRemaining = threshold - consumed;
 *     grab = min(budgetRemaining, event->eventField50);
 *     event->eventField50 -= grab;
 *     budgetAfterGrab = budgetRemaining - grab;
 *     if (budgetAfterGrab == 0) return;    // this tick's budget fully spoken for
 *
 *     outcome = event->GetDispositionForReadAttempt(budgetAfterGrab);
 *
 *     convertCount = budgetAfterGrab;
 *     if (convertCount > event->windowThreshold) {
 *         if (event->windowThreshold == 0) return;
 *         convertCount = event->windowThreshold;
 *     }
 *     copyByteCount = event->field1d * convertCount;
 *     [wraparound copy of copyByteCount bytes: ring->readPos..bufferEnd,
 *      then ring->bufferBase, into sConvertBuffer]
 *
 *     destPtr = destBase + grab;           // float* element arithmetic
 *     result = ConvertWaveToSTGSamples(destPtr, +0x52, [resamplerReservedFlag=]true,
 *                                       sConvertBuffer, +0x51, +0x50,
 *                                       convertCount, event, [reservedArg9=]0);
 *     consumedThisCall = (unsigned char)result;   // confirmed truncated-to-byte quirk
 *     event->windowThreshold -= consumedThisCall;
 *     ring->AdvanceReadPosition(consumedThisCall * event->field1d);
 *     if (needsWaterMarkUpdate)
 *         CSTGDiskCostManager::sInstance->UpdateHDRBufferWaterMarks(ring->availableReadBytes, ring);
 *
 *     consumed += grab + convertCount;      // NOTE: the ARGUMENT values, not
 *                                           // the truncated return value
 *
 *     if (outcome != 1) goto loop_top;
 *
 *     // outcome == 1: this event's own window is exhausted -- retire it.
 *     event->state = 3;
 *     if (event->fieldC == 4) goto recycle_event;
 *
 *   advance_to_next:
 *     nextEvt = event->+0x54;
 *     if (!nextEvt) { this->+0x4c = 0; goto recheck_budget; }
 *     curReadEvt = this->+0x4c;
 *     if (nextEvt != curReadEvt) {
 *         if (curReadEvt && curReadEvt->state != 3) {
 *             curReadEvt->state = 3;
 *             if (nextEvt != curReadEvt->+0x54) curReadEvt->+0x16 = 1;
 *         }
 *         this->+0x4c = nextEvt;
 *     }
 *     nextEvt->state = 2;
 *     startLoc = (this->+0x4c)->+0x40;      // reload, matches real instruction order
 *     if (startLoc == 0) goto recheck_budget;
 *     ring->readPos = startLoc;
 *     if (threshold <= consumed) return;
 *     destBase = destPtr + convertCount; goto chunk_body;
 *
 *   recycle_event:
 *     event->state = 0;                     // OVERWRITES the state=3 just set above
 *     push event onto TSTGArrayManager<CSTGPlaybackEvent>'s free list
 *       (bucketArray[writeCursor] = event; writeCursor = (writeCursor+1) % modulus)
 *     goto advance_to_next;                 // re-chase +0x54 on the SAME (now-recycled) event
 *
 *   recheck_budget:
 *     if (threshold <= consumed) return;
 *     destBase = destPtr + convertCount; goto chunk_body;
 *
 *   loop_top:                               // only reached via the loop-back below,
 *                                            // NEVER on the first pass
 *     if (outcome == 3) goto case_outcome3;
 *     if (outcome == 2) goto case_outcome2;
 *     if (threshold <= consumed) return;
 *     destBase = destPtr + convertCount;
 *     goto chunk_body;
 *
 * outcome==3/2 bookkeeping (case_outcome3/case_outcome2 below) both run
 * using the LAST chunk_body's own final `event`/`consumed`/`convertCount`/
 * `copyByteCount` values (confirmed real: reached directly from `loop_top`'s
 * own check, without any intervening event reload) and both END the whole
 * function afterward (no loop-back) -- reproduced with an algebraically
 * equivalent `carry = consumed - convertCount` standing in for the real
 * function's own stale-register reuse (`consumed - convertCount ==
 * grab + consumed_before_last_chunk`, the exact value the real EAX register
 * still holds at that program point; using a persistent named local instead
 * of relying on C++ local-variable lifetime to mimic register carryover is
 * both simpler and exactly as faithful).
 */

#include "oa_engine.h"
#include "oa_engine_init.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

/* Manual byte copy, NOT __builtin_memcpy/memcpy(): the two copy lengths
 * below (`firstSeg`/`copyByteCount - firstSeg`) are genuine RUNTIME values,
 * not compile-time constants, so GCC cannot inline a `__builtin_memcpy` call
 * for them and instead emits a real call to library `memcpy` -- confirmed
 * via an actual `make ko` run (batch 27): this pushed `nm -u OA.ko` from 32
 * to 33 unresolved symbols (a NEW `memcpy` reference, never previously
 * needed by any other file in this project). This project's own established
 * convention for a fixed COMPILE-TIME-constant-size zero/copy (like this
 * same function's own `sConvertBuffer` zero-fill, sized `sizeof(...)`) is
 * `__builtin_memset`/`__builtin_memcpy`, which GCC happily inlines away with
 * no call at all -- but for a genuinely runtime-sized copy, a plain manual
 * loop is the correct fix, not a workaround. */
static void RawCopy(unsigned char *dst, const unsigned char *src, unsigned int n)
{
	for (unsigned int i = 0; i < n; i++)
		dst[i] = src[i];
}

unsigned char CSTGPlaybackBuffer::sConvertBuffer[0x100];

void CSTGPlaybackBuffer::ProcessSubRate()
{
	unsigned char *base = (unsigned char *)this;
	CSTGHDRCircularBuffer *ring = (CSTGHDRCircularBuffer *)this;

	float *destBase = (base[0x30] != 0)
		? (float *)(CSTGAudioBusManager::sGlobalBusSet + 32 * 0x80)
		: (float *)FromU32(*(unsigned int *)(base + 0x54));
	unsigned int threshold = (base[0x51] == 0) ? 0x20u : 0x40u;
	bool needsWaterMarkUpdate = (*(unsigned int *)(base + 0x48) != 0) || (base[0x44] != 0);
	if (base[0x44] != 0)
		base[0x44] = 0;

	unsigned int consumed = 0;
	unsigned int grab = 0, convertCount = 0, copyByteCount = 0, outcome = 0;
	float *destPtr = destBase;
	CSTGPlaybackEvent *event = 0;
	unsigned char *evt = 0;

	goto chunk_body;

loop_top:
	if (outcome == 3)
		goto case_outcome3;
	if (outcome == 2)
		goto case_outcome2;
	if (threshold <= consumed)
		return;
	destBase = destPtr + convertCount;

chunk_body:
	__builtin_memset(sConvertBuffer, 0, sizeof(sConvertBuffer));

	event = (CSTGPlaybackEvent *)FromU32(*(unsigned int *)(base + 0x4c));
	if (!event)
		return;
	evt = (unsigned char *)event;
	if (*(unsigned int *)(evt + 0x8) != 2)
		return;

	{
		unsigned int budgetRemaining = threshold - consumed;
		unsigned int eventField50 = *(unsigned int *)(evt + 0x50);
		grab = (budgetRemaining <= eventField50) ? budgetRemaining : eventField50;
		*(unsigned int *)(evt + 0x50) = eventField50 - grab;
		unsigned int budgetAfterGrab = budgetRemaining - grab;
		if (budgetAfterGrab == 0)
			return;

		outcome = event->GetDispositionForReadAttempt(budgetAfterGrab);

		convertCount = budgetAfterGrab;
		unsigned int windowThreshold = *(unsigned int *)(evt + 0x44);
		if (convertCount > windowThreshold) {
			if (windowThreshold == 0)
				return;
			convertCount = windowThreshold;
		}

		unsigned int bytesPerFrame = evt[0x1d];
		copyByteCount = bytesPerFrame * convertCount;

		unsigned int bytesUntilWrap = ring->bufferEnd - ring->readPos;
		unsigned int firstSeg = (copyByteCount <= bytesUntilWrap) ? copyByteCount : bytesUntilWrap;
		RawCopy(sConvertBuffer, FromU32(ring->readPos), firstSeg);
		if (copyByteCount > firstSeg)
			RawCopy(sConvertBuffer + firstSeg, FromU32(ring->bufferBase), copyByteCount - firstSeg);

		destPtr = destBase + grab;
		unsigned long result = USTGHDRUtils::ConvertWaveToSTGSamples(
			destPtr, base[0x52] != 0, /*resamplerReservedFlag=*/true,
			(char *)sConvertBuffer, base[0x51] != 0, base[0x50] != 0,
			convertCount, event, /*reservedArg9=*/0);
		unsigned int consumedThisCall = (unsigned char)result;
		*(unsigned int *)(evt + 0x44) = windowThreshold - consumedThisCall;

		ring->AdvanceReadPosition(consumedThisCall * bytesPerFrame);
		if (needsWaterMarkUpdate)
			CSTGDiskCostManager::sInstance->UpdateHDRBufferWaterMarks(ring->availableReadBytes, ring);

		consumed = consumed + grab + convertCount;
	}

	if (outcome != 1)
		goto loop_top;

	/* outcome == 1: this event's own window is exhausted -- retire it. */
	*(unsigned int *)(evt + 0x8) = 3;
	if (((CSTGAudioEvent *)event)->fieldC == 4)
		goto recycle_event;

advance_to_next:
	{
		unsigned char *nextEvt = FromU32(*(unsigned int *)(evt + 0x54));
		if (!nextEvt) {
			*(unsigned int *)(base + 0x4c) = 0;
			goto recheck_budget;
		}
		unsigned char *curReadEvt = FromU32(*(unsigned int *)(base + 0x4c));
		if (nextEvt != curReadEvt) {
			if (curReadEvt != 0 && *(unsigned int *)(curReadEvt + 0x8) != 3) {
				*(unsigned int *)(curReadEvt + 0x8) = 3;
				if (ToU32(nextEvt) != *(unsigned int *)(curReadEvt + 0x54))
					curReadEvt[0x16] = 1;
			}
			*(unsigned int *)(base + 0x4c) = ToU32(nextEvt);
		}
		*(unsigned int *)(nextEvt + 0x8) = 2;

		unsigned char *cur = FromU32(*(unsigned int *)(base + 0x4c));
		unsigned int startLoc = *(unsigned int *)(cur + 0x40);
		if (startLoc == 0)
			goto recheck_budget;
		ring->readPos = startLoc;

		if (threshold <= consumed)
			return;
		destBase = destPtr + convertCount;
		goto chunk_body;
	}

recycle_event:
	*(unsigned int *)(evt + 0x8) = 0;
	{
		TSTGArrayManager<CSTGPlaybackEvent> *mgr = TSTGArrayManager<CSTGPlaybackEvent>::sInstance;
		unsigned int *bucket = (unsigned int *)FromU32(mgr->bucketArray);
		bucket[mgr->writeCursor] = ToU32(event);
		mgr->writeCursor = (mgr->writeCursor + 1) % mgr->modulus;
	}
	goto advance_to_next;

recheck_budget:
	if (threshold <= consumed)
		return;
	destBase = destPtr + convertCount;
	goto chunk_body;

case_outcome3:
	{
		unsigned int carry = consumed - convertCount;
		unsigned int rem = threshold - carry;
		unsigned int bytesPerFrame = evt[0x1d];
		unsigned int windowSize = *(unsigned int *)(evt + 0x38);
		*(unsigned int *)(evt + 0x38) = rem * bytesPerFrame + (windowSize - copyByteCount);
		return;
	}

case_outcome2:
	{
		unsigned char *nextEvt = FromU32(*(unsigned int *)(evt + 0x54));
		unsigned int maxReadBytes = *(unsigned int *)(evt + 0x48);
		unsigned int windowSize = *(unsigned int *)(evt + 0x38);
		unsigned int evtConsumed = *(unsigned int *)(evt + 0x3c);
		*(unsigned int *)(evt + 0x8) = 3;
		unsigned int bytesPerFrame = evt[0x1d];

		if (!nextEvt) {
			*(unsigned int *)(base + 0x4c) = 0;
			return;
		}

		unsigned int span = windowSize - evtConsumed;
		long availSigned = (long)(maxReadBytes - span);
		unsigned int avail = (availSigned >= 0) ? (unsigned int)availSigned : 0;

		*(unsigned int *)(base + 0x4c) = ToU32(nextEvt);
		*(unsigned int *)(nextEvt + 0x8) = 2;

		unsigned int chunks = avail / bytesPerFrame;	/* faithful: real hardware
								 * unsigned div; a zero
								 * bytesPerFrame would fault
								 * on real hardware too,
								 * reproduced as-is */
		unsigned int nextBytesPerFrame = nextEvt[0x1d];
		unsigned int delta = (threshold - chunks - consumed) * nextBytesPerFrame;
		*(unsigned int *)(nextEvt + 0x38) += delta;
		return;
	}
}
