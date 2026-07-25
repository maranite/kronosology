/*
 * buffering_task.h  -  CBufferingTask : public CTask, Stage 6 breadth sweep,
 * 2026-07-25 (DumpManager cluster batch). See circ_byte_buffer.h's own header comment
 * for the shared reachability chain -- constructed as the SECOND of the two sibling
 * tasks `CDumpManMod::Setup()` (dump_man_mod.h) builds, right after `CDumpTask`
 * (dump_task.h), and cross-linked to it there.
 *
 * REAL LAYOUT (0xac bytes: CTask's own 0x7c + 0x30 own bytes, confirmed from
 * CBufferingTask@080cdd50.c):
 *   +0x7c  mDumpTask     the sibling `CDumpTask*` this task buffers for -- ctor sets
 *                        0; `CDumpManMod::Setup()` (dump_man_mod.h) sets the real
 *                        value via `LinkDumpTask()` right after both siblings exist.
 *   +0x80  mBuffer       embedded `CDumpBuffer` (dump_buffer.h, 0x20 bytes),
 *                        constructed with capacity 0x800 (an already-power-of-two
 *                        request, so `CCircByteBuffer`'s own round-down loop is a
 *                        no-op here)
 *   +0xa0  mLimitActive  NOT part of `CDumpBuffer` itself -- see dump_buffer.h's own
 *                        header comment for why this field, despite sitting
 *                        immediately after the embedded `mBuffer`, is genuinely
 *                        `CBufferingTask`'s OWN field that `CDumpBuffer::Read()`/
 *                        `Write()` reach into via a fixed `this+0x20` offset. NOT
 *                        touched by this class's own ctor (a real gap -- populated
 *                        externally by `Put()`'s own type-0x7d branch, Tier B here)
 *                        -- zeroed explicitly for host-KAT determinism only.
 *   +0xa4  mChunkClient  `CChunkClient*` -- a genuinely separate, un-reconstructed
 *                        chunk-transfer client class (out of scope). NOT touched by
 *                        this class's own ctor either (same real-gap status as
 *                        mLimitActive) -- left null here.
 *   +0xa8  mScratchBuf   `new unsigned char[0x100]` -- a 256-byte scratch buffer this
 *                        pass's own Tier-B `Exec()`/`Put()` would use for chunked
 *                        packet assembly (real meaning not fully decoded).
 *
 * `Exec(CMessage&)` (.text+0x080cd930, 685 bytes) and `Put(unsigned char const*,
 * unsigned char)` (.text+0x080cde50, 651 bytes) are Tier B this pass (real signature,
 * minimal body): both pull in `CChunkClient`/`CDumpReqDescr`/`CDumpHeaderDescr`, a
 * genuinely separate, deep dump-file-transfer serialization subsystem this project
 * hasn't reconstructed anywhere -- same "small reconstructed shell, deep out-of-scope
 * algorithm family" boundary as `CDumpManStateMachine`'s own state-handler family
 * (dump_man_state_machine.h). Ctor/dtor/`GetDumpLength()` ARE Tier A -- all 3 are
 * self-contained and genuinely exercised the moment `CDumpManMod::Setup()` runs.
 */

#ifndef BUFFERING_TASK_H
#define BUFFERING_TASK_H

#include "task.h"
#include "dump_buffer.h"

class CModule;
class CDumpTask;
class CMessage;

class CBufferingTask : public CTask {
public:
	/* .text+0x080cdd50, 204 bytes. */
	explicit CBufferingTask(const CModule &owner);

	/* .text+0x080cdc10/0x080cdcb0 (D1/D0 pair). */
	~CBufferingTask();

	/* Real: sets mDumpTask (+0x7c) -- the actual raw-offset poke ground truth's
	 * own `CDumpManMod::Setup()` performs, named here for clarity rather than
	 * reproduced as a bare cast at the call site.
	 */
	void LinkDumpTask(CDumpTask *task) { mDumpTask = task; }

	/* .text+0x080ce120, 25 bytes. Reads CDumpBuffer's own mExpectedLength
	 * (dump_buffer.h) -- real, self-contained.
	 */
	bool GetDumpLength(unsigned long &out) const;

	/* .text+0x080cd930, 685 bytes. Tier B -- see header comment. */
	int Exec(CMessage &msg);

	/* .text+0x080cde50, 651 bytes. Tier B -- see header comment. */
	bool Put(const unsigned char *data, unsigned char len);

private:
	CDumpTask    *mDumpTask;    /* +0x7c */
	CDumpBuffer   mBuffer;      /* +0x80, 0x20 bytes */
	unsigned int  mLimitActive; /* +0xa0 -- see header comment */
	void         *mChunkClient; /* +0xa4 -- opaque CChunkClient*, out of scope */
	unsigned char *mScratchBuf; /* +0xa8 */

	friend struct BufferingTaskTestHooks;
};

#endif /* BUFFERING_TASK_H */
