/*
 * dump_buffer.h  -  CDumpBuffer : public CCircByteBuffer, Stage 6 breadth sweep,
 * 2026-07-25 (DumpManager cluster batch). See circ_byte_buffer.h's own header comment
 * for the shared reachability chain.
 *
 * REAL LAYOUT (0x20 bytes: CCircByteBuffer's own 0x18 + 2 own dwords, confirmed from
 * CDumpBuffer@080ceff0.c/Reset@080cef70.c):
 *   +0x18  mRemainingLength  a length countdown, decremented as bytes flow through
 *                            Read()/Write() while length-tracking is active (see
 *                            below) -- ctor/Reset() both zero it.
 *   +0x1c  mExpectedLength   the total announced dump length; cleared to 0 the moment
 *                            mRemainingLength reaches 0 -- ctor/Reset() both zero it.
 *                            This is the field `CBufferingTask::GetDumpLength()`
 *                            (buffering_task.h) reads back.
 *
 * A THIRD field this class's own Read()/Write() reference, at relative +0x20 (one
 * dword PAST this class's own 0x20-byte end) is genuinely real but is NOT part of
 * CDumpBuffer's own storage -- confirmed by cross-checking 3 independent call sites
 * that all resolve to the SAME absolute address: `CDumpBuffer::Read()`/`Write()`'s own
 * `this+0x20`, and `CBufferingTask::Exec()`/`Put()`'s own `this+0xa0` (buffering_task.h)
 * -- CDumpBuffer is always embedded at `CBufferingTask+0x80` (buffering_task.h), so
 * `0x80+0x20 == 0xa0` is the identical byte in both views. Ground truth's own
 * `CDumpBuffer::Read()`/`Write()` reach one dword past their own object into their
 * (always-fixed-offset) OWNER's own field -- an unusual but real, deliberate pattern
 * (this class is only ever constructed embedded at this exact spot, never standalone),
 * not a Ghidra artifact (the field is written by `CBufferingTask::Put()`'s own code,
 * a completely separate function, at the exact same absolute address). Modeled here
 * via the same raw-offset-on-`this` idiom this whole project already uses for
 * cross-object field access (module_manager.cpp/task.cpp's own `CModule`/`CTask`
 * "treat as raw blob" convention) -- NOT declared as a member of this class.
 *
 * Read()/Write() promoted to real bodies 2026-07-26 (re-check of the DumpManager
 * cluster batch): still NOT reachable from this reconstruction's own wired call
 * graph -- their only real callers remain `CDumpMachine::ReadPacket()`/
 * `WritePacket()` (dump_man_state_machine.h, themselves only reachable from
 * `CDumpManStateMachine`'s own out-of-scope 30-method state-handler family) plus a
 * SEPARATE real ground-truth path, `CDumpApiInstance::Read()`/`Write()`
 * (`.text+0x080ceb30`/`0x080ce900`, NOT reconstructed -- an install-only "ApiInstance"
 * singleton, `mains.cpp`'s own `DumpApi` global, whose vtable is confirmed via
 * `objdump -dr` to have ZERO real callers anywhere in ground truth's own `.text`
 * beyond its constructor -- genuinely dead in ground truth itself, not just in this
 * reconstruction, matching `mains.cpp`'s own "DumpApi/RMApi are untouched" comment).
 * Promoted anyway because they're small, fully self-contained (zero new
 * dependencies -- `CCircByteBuffer::Read()`/`Write()` are already real, and the
 * `Api+0x94` soft-assert calls both bodies make follow this project's established
 * omit-but-preserve-control-flow convention), and complete this cluster's own
 * `CDumpBuffer`/`CDumpMachine` classes to 100% real -- same "build real
 * infrastructure with no current caller, when cheap and clearly correct" precedent
 * as `CHeap`'s ctor/dtor.
 *
 * REAL ALGORITHM (derived from raw `objdump -dr`, both functions read/write the
 * mystery field one dword past this object's own end -- `CBufferingTask::
 * mLimitActive`, buffering_task.h -- to decide whether the `mRemainingLength`/
 * `mExpectedLength` dump-length countdown applies to this call):
 *
 *   Read(dst, len): if mLimitActive != 0, read exactly `len` bytes straight through
 *     CCircByteBuffer::Read(), no length tracking. If mLimitActive == 0: clamp the
 *     requested length down to `min(len, mRemainingLength)`, do the (possibly
 *     shorter) CCircByteBuffer::Read(), and on success decrement mRemainingLength
 *     by the ACTUAL bytes read (clearing mExpectedLength too once it hits 0); if the
 *     original `len` exceeded what was available, the caller's buffer tail beyond
 *     the actual read is explicitly zero-filled (`memset`) -- a real, confirmed (not
 *     guessed) ground-truth behavior, not a Ghidra artifact.
 *
 *   Write(src, len): if mLimitActive == 1, clamp the requested length down to
 *     `min(len, mRemainingLength)` BEFORE calling CCircByteBuffer::Write(), and on
 *     success decrement mRemainingLength by the actual bytes written (clearing
 *     mExpectedLength too once it hits 0). Any OTHER mLimitActive value (0 included)
 *     writes the full requested length straight through, no tracking.
 *
 *   CONFIRMED, NOT A TRANSCRIPTION ERROR: Read() and Write() check OPPOSITE
 *   polarities of the SAME field (`mLimitActive == 0` engages tracking in Read();
 *   `mLimitActive == 1` engages it in Write()) -- double-checked byte-for-byte on
 *   both functions independently before trusting this, since it looks at first
 *   glance like a copy-paste inversion bug. Given `CBufferingTask`'s own ctor always
 *   zeroes mLimitActive and the only writer (`Put()`'s own type-0x7d branch) stays
 *   Tier B, this reconstruction can't yet observe which polarity ground truth
 *   actually drives Write() through at runtime -- transcribed exactly as read either
 *   way, not reconciled to a single "clean" convention.
 *
 * Both `Api+0x94` soft asserts each function makes (an already-fetched-field
 * re-check, e.g. "mExpectedLength != 0") are omitted per this project's standard
 * convention -- ground truth's own asserts are non-enforcing here too: each one
 * logs, then re-reads the SAME field and falls through to the identical
 * continuation either way, so omitting the call changes no computed value.
 *
 * Ctor/dtor/Reset() were already real -- CDumpBuffer's OWN construction/destruction
 * happens for real every time a `CBufferingTask` is built (buffering_task.h), on the
 * actual boot path.
 */

#ifndef DUMP_BUFFER_H
#define DUMP_BUFFER_H

#include "circ_byte_buffer.h"

class CDumpBuffer : public CCircByteBuffer {
public:
	/* .text+0x080ceff0, 51 bytes. */
	explicit CDumpBuffer(unsigned long requestedCapacity);

	/* .text+0x080cefa0/0x080cefc0 (D1/D0 pair). */
	~CDumpBuffer();

	/* .text+0x080cef70, 37 bytes. */
	void Reset();

	/* .text+0x080cf030, 301 bytes. Tier A (2026-07-26) -- see header comment. */
	bool Read(unsigned char *dst, unsigned long len);

	/* .text+0x080cf170, 208 bytes. Tier A (2026-07-26) -- see header comment. */
	bool Write(const unsigned char *src, unsigned long len);

	/* Not a ground-truth method -- a named accessor standing in for the raw
	 * `*(ulong*)(this+0x1c)` read `CBufferingTask::GetDumpLength()`'s own real
	 * ground-truth body performs (buffering_task.h), same "wrap a raw offset poke
	 * in a small named method" convention as CBufferingTask::LinkDumpTask().
	 */
	unsigned int ExpectedLength() const { return mExpectedLength; }

	/* Not a ground-truth method -- same convention as ExpectedLength() above,
	 * standing in for the raw `*(uint*)(this+0x18)` read `CDumpMachine::
	 * IsDumpEnded()`'s own real ground-truth body performs via the
	 * `CBufferingTask+0x98 == CDumpBuffer+0x18` identity (dump_man_state_machine.h).
	 */
	unsigned int RemainingLength() const { return mRemainingLength; }

private:
	unsigned int mRemainingLength; /* +0x18 */
	unsigned int mExpectedLength;  /* +0x1c */

	friend struct DumpBufferTestHooks;
};

#endif /* DUMP_BUFFER_H */
