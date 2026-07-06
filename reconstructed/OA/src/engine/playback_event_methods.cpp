// SPDX-License-Identifier: GPL-2.0
/*
 * playback_event_methods.cpp  -  CSTGPlaybackEvent's own remaining small
 * confirmed methods (batch 25), plus CSTGAudioEvent::Reset() (its own
 * newly-discovered dependency). See oa_engine_init.h for each method's
 * own header-comment summary; this file carries the full per-field
 * derivation.
 *
 * Deliberately its own dedicated TU, separate from engine_init.cpp
 * (which owns the two classes' ctors) and from playback_buffer_events.cpp
 * (CSTGPlaybackBuffer's own methods) -- matches this project's established
 * per-concern-file convention. NOTE: this file and
 * playback_buffer_events.cpp have a genuine MUTUAL dependency:
 * HandleFileClosed() below calls CSTGPlaybackBuffer::RemoveEvent()
 * (playback_buffer_events.cpp), and RemoveEvent()/EventFileError() there
 * call CSTGPlaybackEvent::Reset() (here). Both files are linked together
 * in the real .ko build (OA-objs) as a matter of course; any verify/
 * binary that exercises either cross-call must link both (see
 * verify/test_playback_buffer_events.cpp, extended this batch to also
 * link this file).
 *
 * Ground-truthed via objdump -dr against
 * /home/share/Decomp/OA.ko_Decomp/OA.ko:
 *   CSTGAudioEvent::Reset()                        .text+0xd17e0,  70B
 *   CSTGPlaybackEvent::HandleErrorOpening()         .text+0xd6ba0,   1B (bare ret)
 *   CSTGPlaybackEvent::HandleErrorReading()         .text+0xd6bb0,   1B (bare ret)
 *   CSTGPlaybackEvent::Reset()                      .text+0xd6bc0, 119B
 *   CSTGPlaybackEvent::HandleFileClosed()            .text+0xd6c40,  32B
 *   CSTGPlaybackEvent::HandleFileOpened()            .text+0xd6c60,  44B
 *   CSTGPlaybackEvent::IncrementBufferStartLocation() .text+0xd6d10,  29B
 *   CSTGPlaybackEvent::SeekSkipFileBytes()            .text+0xd6d30,  10B
 *   CSTGPlaybackEvent::GetDispositionForReadAttempt() .text+0xd6d40, 117B
 *   CSTGPlaybackEvent::~CSTGPlaybackEvent() (D2Ev/D1Ev, D0Ev)  7B each,
 *     confirmed BYTE-FOR-BYTE IDENTICAL bodies (both just reset the
 *     vtable pointer back to `_ZTV14CSTGAudioEvent+8` and return -- same
 *     "real D0Ev omits the operator-delete call" shape already
 *     documented for CSTGGlobal::~CSTGGlobal(), sec 10.90/global.cpp:
 *     a single C++ `~CSTGPlaybackEvent()` definition here necessarily
 *     generates a compiler D0Ev that WILL call operator delete, a known
 *     harmless divergence since these objects are only ever recycled
 *     through TSTGArrayManager's own free-list, never `delete`d).
 *
 * Confirmed per-event field layout used here (byte offsets into
 * CSTGPlaybackEvent's own `_unrecovered[0x68]`, matching
 * playback_buffer_events.cpp's own already-established field summary
 * for the ones it already uses -- +0x08/+0x1d/+0x24 are strictly BEFORE
 * the CSTGAudioEvent/CSTGPlaybackEvent field-overlap boundary at +0x2c,
 * so reinterpreting `this` as `CSTGAudioEvent*` to reach them by their
 * already-established names is exact, not a re-guess):
 *   +0x08  CSTGAudioEvent::field8  -- state tag (1/2/3), read here by
 *          HandleFileClosed() (only removes the event once state==3).
 *   +0x1d  CSTGAudioEvent::field1d -- per-frame byte-size multiplier
 *          (confirmed real value 2 from the ctor), also used here as
 *          GetDispositionForReadAttempt()'s own divisor.
 *   +0x24  CSTGAudioEvent::field24 -- a `void*` file handle (confirmed
 *          zeroed by the ctor -- set to a real handle elsewhere, outside
 *          this file's own reachable call graph -- before
 *          HandleFileOpened() ever runs), passed directly to the
 *          already-declared `CSTGFile_GetFileSize()` extern.
 *   +0x30  ownerBuffer   -- back-reference to the owning
 *          CSTGPlaybackBuffer*, packed 32-bit (already named in
 *          playback_buffer_events.cpp).
 *   +0x34  fileStartOffset -- the file byte offset this event's own
 *          read window begins at; advanced by SeekSkipFileBytes().
 *   +0x38  windowSize    -- a per-event window/segment size (constant
 *          once established elsewhere -- ctor/Reset() only zero it);
 *          GetDispositionForReadAttempt() uses `windowSize - consumed`
 *          as the effective "bytes still owed within this window".
 *   +0x3c  consumed      -- bytes already skipped/consumed within the
 *          window above; SeekSkipFileBytes() advances this in lockstep
 *          with fileStartOffset.
 *   +0x40  bufferStartLocation -- a `char*` position within the owning
 *          buffer's shared ring (already named in
 *          playback_buffer_events.cpp/EventBufferStartLocationUpdated);
 *          IncrementBufferStartLocation() advances it by `n` (clamped to
 *          the remaining file size) and wraps it modulo the owning
 *          buffer's own embedded `CSTGHDRCircularBuffer::effectiveSize`
 *          once it reaches `bufferEnd` -- pure address arithmetic, never
 *          dereferenced here (matches that class's own established
 *          "arithmetic only" convention).
 *   +0x44  windowThreshold -- already named "remaining/unused frame
 *          count" in playback_buffer_events.cpp/HandleAdvanceCancelledEvent;
 *          GetDispositionForReadAttempt() ALSO reads this same field
 *          directly, as the lower-bound threshold a requested read
 *          position is compared against -- same physical field, a
 *          second confirmed real use, not a naming conflict.
 *   +0x48  maxReadBytes  -- a readable-byte budget/cap, clamped down by
 *          HandleFileOpened() to whatever's actually left in the real
 *          file, and decremented by SeekSkipFileBytes().
 *   +0x4c  cachedFileSize -- the file size HandleFileOpened() caches
 *          from CSTGFile_GetFileSize(); confirmed real gap (matching
 *          the sec 10.158 "don't assume a ctor zeroes a field just
 *          because most are" class of finding): the CTOR never zeroes
 *          this field, but Reset() does -- a real, ctor-vs-Reset()
 *          asymmetry, faithfully NOT "fixed" here.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

/* CSTGFile_GetFileSize -- same handle-based extern already declared
 * locally in init_module.cpp (no shared header carries it); re-declared
 * here per this project's own per-TU-extern convention. Already has a
 * real `{ return 0; }` stub body in bar2_stubs_c.cpp for the production
 * .ko link -- this call resolves within OA.ko itself, not against an
 * external kernel symbol, so it doesn't affect the `nm -u` unresolved-
 * symbol count. */
extern "C" unsigned int CSTGFile_GetFileSize(void *handle);

/*
 * CSTGAudioEvent::Reset() (`.text+0xd17e0`, 70 bytes) -- CONFIRMED
 * byte-for-byte identical to the ctor's own 12 field writes (see
 * oa_engine_init.h), minus the vtable-pointer install. A pure
 * straight-line sequence of immediate stores, no calls/branches.
 */
void CSTGAudioEvent::Reset()
{
	field8 = 0;
	fieldC = 4;
	field10 = 0;
	field1c = 1;
	field18 = 0;
	field24 = 0;
	field28 = 0;
	field14 = 0;
	field15 = 0;
	field16 = 0;
	field1d = 2;
	sampleRate = 0xbb80;
}

/*
 * CSTGPlaybackEvent::Reset() (`.text+0xd6bc0`, 119 bytes): calls
 * CSTGAudioEvent::Reset() on the same storage first (confirmed via
 * relocation to `_ZN14CSTGAudioEvent5ResetEv`), then 14 further
 * confirmed zero-stores in this EXACT order (not alphabetical -- kept
 * to match the real instruction sequence): +0x34/+0x38/+0x3c/+0x40/
 * +0x44/+0x48/+0x4c/+0x50/+0x54/+0x58/+0x30, then the two BYTES +0x61
 * THEN +0x60 (that order, confirmed real), then the dword +0x64. This
 * is also the confirmed real `call *0x1c(%edx)` dispatch target for
 * CSTGPlaybackEvent's own vtable slot 7 -- see
 * CSTGPlaybackBuffer::RemoveEvent()/EventFileError()
 * (playback_buffer_events.cpp) for how that dispatch is reproduced.
 */
void CSTGPlaybackEvent::Reset()
{
	((CSTGAudioEvent *)this)->Reset();

	unsigned char *self = (unsigned char *)this;
	*(unsigned int *)(self + 0x34) = 0;
	*(unsigned int *)(self + 0x38) = 0;
	*(unsigned int *)(self + 0x3c) = 0;
	*(unsigned int *)(self + 0x40) = 0;
	*(unsigned int *)(self + 0x44) = 0;
	*(unsigned int *)(self + 0x48) = 0;
	*(unsigned int *)(self + 0x4c) = 0;
	*(unsigned int *)(self + 0x50) = 0;
	*(unsigned int *)(self + 0x54) = 0;
	*(unsigned int *)(self + 0x58) = 0;
	*(unsigned int *)(self + 0x30) = 0;
	self[0x61] = 0;
	self[0x60] = 0;
	*(unsigned int *)(self + 0x64) = 0;
}

/*
 * CSTGPlaybackEvent::HandleFileOpened() (`.text+0xd6c60`, 44 bytes):
 * caches the real file size (+0x4c), then clamps the readable-byte
 * budget (+0x48) down to whatever's actually left in the file starting
 * from +0x34 -- confirmed plain unsigned subtraction/compare (an
 * underflow here, if +0x34 > fileSize, wraps exactly like the real
 * hardware's own `sub`/`cmp` would; not specially guarded).
 */
void CSTGPlaybackEvent::HandleFileOpened()
{
	unsigned char *self = (unsigned char *)this;
	CSTGAudioEvent *base = (CSTGAudioEvent *)this;

	unsigned int fileSize = CSTGFile_GetFileSize(FromU32(base->field24));
	*(unsigned int *)(self + 0x4c) = fileSize;

	unsigned int remaining = fileSize - *(unsigned int *)(self + 0x34);
	if (remaining < *(unsigned int *)(self + 0x48))
		*(unsigned int *)(self + 0x48) = remaining;
}

/*
 * CSTGPlaybackEvent::HandleFileClosed() (`.text+0xd6c40`, 32 bytes):
 * once this event's own state reaches 3 (finished), removes itself from
 * its owning buffer's tracking via CSTGPlaybackBuffer::RemoveEvent().
 * No-op otherwise.
 */
void CSTGPlaybackEvent::HandleFileClosed()
{
	CSTGAudioEvent *base = (CSTGAudioEvent *)this;
	if (base->field8 != 3)
		return;

	unsigned char *self = (unsigned char *)this;
	CSTGPlaybackBuffer *owner = (CSTGPlaybackBuffer *)FromU32(*(unsigned int *)(self + 0x30));
	owner->RemoveEvent(this);
}

/* CSTGPlaybackEvent::HandleErrorOpening()/HandleErrorReading()
 * (`.text+0xd6ba0`/`.text+0xd6bb0`, 1 byte each): confirmed bare `ret`,
 * no body whatsoever. */
void CSTGPlaybackEvent::HandleErrorOpening()
{
}

void CSTGPlaybackEvent::HandleErrorReading()
{
}

/*
 * CSTGPlaybackEvent::GetDispositionForReadAttempt(pos) const
 * (`.text+0xd6d40`, 117 bytes): confirmed real disposition codes
 * (0/1/2/3), full arithmetic derivation:
 *
 *   threshold = +0x44
 *   if (threshold > pos) return 0;             // before this event's own start
 *
 *   span  = windowSize(+0x38) - consumed(+0x3c)
 *   avail = maxReadBytes(+0x48) - span, clamped to 0 if maxReadBytes <= span
 *
 *   if (threshold == pos)
 *       return (avail < 1) ? 1 : 3;
 *
 *   chunks   = avail / chunkSize(+0x1d)         // unsigned div, real
 *                                                // hardware fault risk
 *                                                // preserved as-is if
 *                                                // chunkSize is ever 0
 *   boundary = threshold + chunks
 *   if (pos < boundary) return 3;
 *   return (avail < 1) ? 1 : 2;
 *
 * Every branch/comparison here (including the `cmovbe`-based avail
 * clamp) is a direct transcription of the real disassembly, not a
 * simplification -- the exact real meaning of dispositions 0-3 isn't
 * independently named (no string table/enum reference found), but
 * every arithmetic step is confirmed byte-for-byte.
 */
unsigned int CSTGPlaybackEvent::GetDispositionForReadAttempt(unsigned int pos) const
{
	const unsigned char *self = (const unsigned char *)this;
	unsigned int threshold = *(const unsigned int *)(self + 0x44);

	if (threshold > pos)
		return 0;

	unsigned int windowSize = *(const unsigned int *)(self + 0x38);
	unsigned int consumed   = *(const unsigned int *)(self + 0x3c);
	unsigned int maxReadBytes = *(const unsigned int *)(self + 0x48);
	unsigned int span  = windowSize - consumed;
	unsigned int avail = maxReadBytes - span;
	if (maxReadBytes <= span)
		avail = 0;

	if (threshold == pos)
		return (avail < 1) ? 1u : 3u;

	unsigned char chunkSize = ((const CSTGAudioEvent *)this)->field1d;
	unsigned int chunks = avail / chunkSize;
	unsigned int boundary = threshold + chunks;
	if (pos < boundary)
		return 3u;
	return (avail < 1) ? 1u : 2u;
}

/*
 * CSTGPlaybackEvent::IncrementBufferStartLocation(n) (`.text+0xd6d10`,
 * 29 bytes): clamps `n` to the cached file size (+0x4c), advances
 * bufferStartLocation (+0x40) by it, then wraps modulo the OWNING
 * buffer's own embedded CSTGHDRCircularBuffer effectiveSize once it
 * reaches bufferEnd. Pure address arithmetic on the owner's fields
 * (bufferEnd/effectiveSize), never dereferenced as real pointers here.
 */
void CSTGPlaybackEvent::IncrementBufferStartLocation(unsigned int n)
{
	unsigned char *self = (unsigned char *)this;

	unsigned int cachedFileSize = *(unsigned int *)(self + 0x4c);
	if (n > cachedFileSize)
		n = cachedFileSize;

	const CSTGHDRCircularBuffer *ring =
		(const CSTGHDRCircularBuffer *)FromU32(*(unsigned int *)(self + 0x30));

	unsigned int pos = *(unsigned int *)(self + 0x40) + n;
	*(unsigned int *)(self + 0x40) = pos;

	if (pos >= ring->bufferEnd)
		*(unsigned int *)(self + 0x40) = pos - ring->effectiveSize;
}

/*
 * CSTGPlaybackEvent::SeekSkipFileBytes(delta) (`.text+0xd6d30`, 10
 * bytes): advances both `consumed` (+0x3c) and `fileStartOffset`
 * (+0x34) by `delta`, and shrinks `maxReadBytes` (+0x48) by the same
 * amount -- three plain adds/subtracts, no branches.
 */
void CSTGPlaybackEvent::SeekSkipFileBytes(unsigned int delta)
{
	unsigned char *self = (unsigned char *)this;
	*(unsigned int *)(self + 0x3c) += delta;
	*(unsigned int *)(self + 0x34) += delta;
	*(unsigned int *)(self + 0x48) -= delta;
}

/*
 * ~CSTGPlaybackEvent() -- see this file's own header comment for why
 * the compiler-generated D0Ev calling operator delete afterward is a
 * known, harmless divergence from the real D0Ev's own confirmed
 * omission of that call (same precedent as CSTGGlobal::~CSTGGlobal(),
 * sec 10.90).
 *
 * NEW gotcha this batch, confirmed via objdump against a real -m32
 * -mregparm=3 compile (matching this project's own Kbuild flags): GCC
 * applies a "destructor purity" dead-store elimination that is UNIQUE
 * to genuine `~ClassName()` destructors (not plain functions with an
 * identical body) whenever the destructor's ONLY effect is a write to
 * `this` with no other calls -- since nothing in the C++ abstract
 * machine can legally observe a write to an object whose lifetime just
 * ended, GCC eliminates the store entirely, compiling this destructor
 * down to a bare `ret` (confirmed: a plain, non-volatile version of
 * this exact body produces ZERO instructions under -O2 -m32
 * -mregparm=3, a real divergence from the confirmed real 7-byte body).
 * Every OTHER destructor already reconstructed in this project
 * (~CSTGVoiceAllocator/~CSTGMessageProcessor/~CSTGVoiceModelManager)
 * happens to call at least one other function in its own body, which
 * suppresses this optimization -- this is the FIRST destructor in this
 * project whose only effect is a raw write, so the first to expose it.
 * Fixed with a `volatile` write (confirmed via the same -m32 objdump
 * check to restore the real write instruction) -- this doesn't change
 * behavior on the real target (a plain write there is already never
 * elided since nothing there does C++-standard lifetime reasoning
 * about kernel memory), it only defeats GCC's C++-abstract-machine-only
 * optimization. Lesson for future batches: any destructor whose ENTIRE
 * body is a memory write (no other calls) needs this same treatment;
 * verify with objdump, don't assume plain code matches the ctor-side
 * precedent.
 */
CSTGPlaybackEvent::~CSTGPlaybackEvent()
{
	*(volatile unsigned int *)this = ToU32(_ZTV14CSTGAudioEvent + 8);
}
