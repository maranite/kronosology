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
 * Read()/Write() themselves are Tier B this pass (real signature, empty/no-op body):
 * the only real callers are `CDumpMachine::ReadPacket()`/`WritePacket()`
 * (dump_man_state_machine.h), which are themselves only reachable from
 * `CDumpManStateMachine`'s own 30-method OnRx/OnGet-per-state family -- genuinely out
 * of scope for this pass (a CClientCommServer-scale state machine, see that class'
 * own header for the established precedent of leaving a deep dispatch family Tier B
 * around a real, reconstructed shell). Ctor/dtor/Reset() ARE real -- CDumpBuffer's
 * OWN construction/destruction happens for real every time a `CBufferingTask` is
 * built (buffering_task.h), on the actual boot path.
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

	/* .text+0x080cf030, 301 bytes. Tier B -- see header comment. */
	bool Read(unsigned char *dst, unsigned long len);

	/* .text+0x080cf170, 208 bytes. Tier B -- see header comment. */
	bool Write(const unsigned char *src, unsigned long len);

	/* Not a ground-truth method -- a named accessor standing in for the raw
	 * `*(ulong*)(this+0x1c)` read `CBufferingTask::GetDumpLength()`'s own real
	 * ground-truth body performs (buffering_task.h), same "wrap a raw offset poke
	 * in a small named method" convention as CBufferingTask::LinkDumpTask().
	 */
	unsigned int ExpectedLength() const { return mExpectedLength; }

private:
	unsigned int mRemainingLength; /* +0x18 */
	unsigned int mExpectedLength;  /* +0x1c */

	friend struct DumpBufferTestHooks;
};

#endif /* DUMP_BUFFER_H */
