/*
 * dump_man_state_machine.h  -  CDumpManStateMachine (abstract per-state dispatch base)
 * and CDumpMachine (its concrete I/O adapter), Stage 6 breadth sweep, 2026-07-25
 * (DumpManager cluster batch). See circ_byte_buffer.h's own header comment for the
 * shared reachability chain (`CDumpManMod::Setup()` -> `CDumpTask::CDumpTask()` ->
 * this class).
 *
 * ARCHITECTURE (a genuine Strategy/Template-method split, confirmed from ground
 * truth, not inferred): `CDumpManStateMachine` (~35 real methods total, `nm -C`-
 * counted: `Init`/`Start`/`Again`/`Error`/`LoopRx`/`LoopTx`/`StartRx`/`StartTx`/
 * `OnTimeout`/`StopByUser`/`SendToSysEx`/`StartLoopRx`/`SendStoppedByUser`, plus ~18
 * `OnRxMsgWhenInXxx`/`OnGetMsgWhenInXxx` per-state handlers) implements the actual
 * dump-transfer PROTOCOL state machine (IDLE/STX/SRX/SENT/WAIT_OK/WAIT_LEN/
 * WAIT_LOAD_END/...) but calls back into a small, abstract I/O surface
 * (`ReadPacket`/`WritePacket`/`SendSexMessage`/`PutMessage`/`IsDumpEnded`/
 * `SetTimeout`) that `CDumpMachine` (9 methods) implements concretely, forwarding to
 * its owning `CDumpTask`'s already-real `CSysExMsgTaskBase::SendMsg()`/`SetTimeout()`
 * (sysex_msg_task_base.h) and its sibling `CBufferingTask`'s own buffer
 * (buffering_task.h). This split is EXACTLY the same "small reconstructed shell around
 * a genuinely deep, out-of-scope algorithm-database family" shape this project already
 * established for `CSTGUnsolMsgHandler`/`CClientCommServer` -- the ~30-method state-
 * handler family stays Tier B (not even declared here, since nothing in this pass's
 * own reconstructed code calls any of them), while the small, self-contained ctor/
 * dtor/Init() and the I/O-adapter methods are Tier A.
 *
 * REAL LAYOUT, CDumpManStateMachine (0x1c bytes, confirmed from
 * CDumpManStateMachine@080cf9b0.c / Init@080cfa30.c):
 *   +0x00  vtbl        installed once at construction (PTR__CDumpManStateMachine_
 *                       08e85ce8, 10 slots -- 10 real named methods this pass didn't
 *                       trace individually beyond the ones listed above, install-only,
 *                       matching this project's usual convention)
 *   +0x04  mUnknown04  zeroed by both the ctor AND Init() (real meaning not decoded --
 *                       part of the out-of-scope state-handler family's own bookkeeping)
 *   +0x08  mUnknown08  zeroed by both the ctor AND Init()
 *   +0x0c  mUnknown0c  zeroed by both the ctor AND Init() (Init() zeroes this FIRST,
 *                       before the virtual dispatch below)
 *   +0x10  mUnknown10  NOT touched by the ctor or by Init()'s own directly-visible
 *                       code -- real ctor/Init() leave it whatever `new[]` returned;
 *                       almost certainly written by the real target of Init()'s own
 *                       virtual dispatch (below), a genuinely different, derived,
 *                       not-reconstructed override (part of the out-of-scope state-
 *                       handler family) -- zeroed explicitly here anyway, for host-KAT
 *                       determinism only, not because ground truth does.
 *   +0x14  mBuffer      `new unsigned char[sm_uiBuffSize]` (214 bytes -- real
 *                       `.data` constant, confirmed via direct byte read;
 *                       `sm_uiPackDataLen = sm_uiBuffSize - 2` is computed by a real
 *                       static initializer, `_GLOBAL__I_sm_uiBuffSize@080d18c0.c`,
 *                       modeled here as a second plain constant rather than
 *                       reproducing a static-init function for a value this pass's
 *                       own code never reads)
 *   +0x18  mUnknown18  zeroed by the ctor only (Init() doesn't touch it)
 *
 * `Init()` (.text+0x080cfa30, 52 bytes) real body: zero mUnknown0c, dispatch a
 * virtual call through this object's OWN vtable slot +0x20 (index 8) with argument 0
 * -- real target not reconstructed (part of the state-handler family), so this lands
 * on `EvaVTableStub` (a safe no-op) via whichever concrete vtable
 * (`PTR__CDumpMachine_08e85c48`) is actually installed by then -- then zero
 * mUnknown08/mUnknown04.
 *
 * REAL LAYOUT, CDumpMachine (0x20 bytes: base 0x1c + 1 own dword, confirmed from
 * CDumpMachine@080cf4d0.c):
 *   +0x1c  mOwnerTask   the ctor's own `CDumpTask&` argument, stored as a raw pointer
 *
 * `ReadPacket()`/`WritePacket()`/`IsDumpEnded()` promoted to real bodies 2026-07-26
 * (re-check of the DumpManager cluster batch): all 3 are trivial forwards --
 * `mOwnerTask->BufferingTask()` (real, `CDumpTask`'s own `+0x94` field, read
 * directly by raw offset in ground truth rather than through the accessor) gives a
 * `CBufferingTask*`; `ReadPacket()`/`WritePacket()` reinterpret that pointer +0x80
 * (where `CDumpBuffer` is always embedded, buffering_task.h) as a `CDumpBuffer*` and
 * tail-call its own (now-real, dump_buffer.h) `Read()`/`Write()`; `IsDumpEnded()`
 * reads `CBufferingTask+0x98` directly, which is the SAME byte as `CDumpBuffer+0x18`
 * (`mRemainingLength`) via that identical `+0x80` embedding -- ground truth computes
 * it as one flat offset rather than two nested ones, but it's the same field either
 * way (RemainingLength(), dump_buffer.h). Still NOT reachable from this
 * reconstruction's own wired call graph -- only called from the genuinely
 * out-of-scope `CDumpManStateMachine` state-handler family -- promoted anyway for the
 * same "small, self-contained, completes the cluster" reasoning as `CDumpBuffer::
 * Read()`/`Write()` (dump_buffer.h's own header comment). Real soft NULL asserts on
 * `BufferingTask()` (Api+0x94) omitted per this project's usual convention; ground
 * truth dereferences the (possibly null) pointer unconditionally afterward either
 * way, so this is not converted into a hard guard.
 * `SetTimeout()`/`SendSexMessage()`/`PutMessage()` ARE Tier A: real, self-contained
 * one-line forwards into already-real code (`CSysExMsgTaskBase::SetTimeout()`/
 * `SendMsg()`) or a Tier-B stub whose OWN forwarding call site is still a real,
 * faithfully-transcribed piece of ground truth (`CBufferingTask::Put()`, same
 * "the forward is real even if its target is a stub" precedent as
 * `CDumpTask::OnGetMessage()` below).
 */

#ifndef DUMP_MAN_STATE_MACHINE_H
#define DUMP_MAN_STATE_MACHINE_H

class CDumpTask;

class CDumpManStateMachine {
public:
	/* .text+0x080cf9b0, 112 bytes. */
	CDumpManStateMachine();

	/* .text+0x080cf950/0x080cf980 (D1/D0 pair). */
	~CDumpManStateMachine();

	/* .text+0x080cfa30, 52 bytes. See header comment. */
	void Init();

	/* .text+0x080d11e0, real signature `OnGetMessage(unsigned char const*,
	 * unsigned char)`. Tier B (real signature, empty body) -- part of the
	 * out-of-scope ~30-method state-handler family; real caller
	 * (`CDumpTask::OnGetMessage()`, dump_task.h) IS a real, faithfully-
	 * transcribed forward, same "the forward is real even if its target is a
	 * stub" precedent as everywhere else in this project.
	 */
	void OnGetMessage(const unsigned char *data, unsigned char len);

	/* .text+0x080d13f0, real signature `OnReceiveMessage(unsigned char const*,
	 * unsigned char)`. Tier B -- see OnGetMessage()'s own comment.
	 */
	void OnReceiveMessage(const unsigned char *data, unsigned char len);

	/* .text+0x080d0150. Tier B -- see OnGetMessage()'s own comment. Real caller
	 * is `CDumpTask::OnTimeout()`'s own tail-call forward (dump_task.h/.cpp,
	 * re-derived from raw `objdump -dr` since Ghidra's own decompile mis-read it
	 * as a double-indirection zero-arg call -- see dump_task.cpp).
	 */
	void OnTimeout();

	/* Real static data members (CDumpManStateMachine's own out-of-scope state-
	 * handler family reads these; this pass's own code doesn't) -- real values
	 * confirmed by direct `.data` byte read, kept for structural completeness.
	 */
	static const unsigned sm_uiBuffSize;
	static const unsigned sm_uiPackDataLen;
	static const unsigned sm_uiMaxRetryCounter;

private:
	/* Declared in real offset order (0x00, 04, 08, 0c, 10, 14, 18); nothing
	 * outside this class ever reaches into these fields by raw offset (unlike
	 * CModule/CTask elsewhere in this project), so exact inter-field ordering is
	 * only a documentation nicety here, not a correctness requirement -- but kept
	 * matching ground truth anyway. `CDumpMachine`'s ctor/dtor install this
	 * class's own +0x00 vtbl slot via a raw `reinterpret_cast<void**>(this)`
	 * write (same "manually-managed raw mVtbl, bypass access control" idiom as
	 * CSysExMsgTaskBase writing into CTask's own private +0x00/+0x08, task.h),
	 * so mVtbl does not need to be protected/friended for that.
	 */
	void          *mVtbl;      /* +0x00 */
	unsigned int   mUnknown04; /* +0x04 */
	unsigned int   mUnknown08; /* +0x08 */
	unsigned int   mUnknown0c; /* +0x0c */
	unsigned int   mUnknown10; /* +0x10 */
	unsigned char *mBuffer;    /* +0x14 */
	unsigned int   mUnknown18; /* +0x18 */

	friend struct DumpManStateMachineTestHooks;
};

class CDumpMachine : public CDumpManStateMachine {
public:
	/* .text+0x080cf4d0, 36 bytes. */
	explicit CDumpMachine(CDumpTask &owner);

	/* .text+0x08186810/0x08186830 (D1/D0 pair). */
	~CDumpMachine();

	/* .text+0x080cf2c0, 38 bytes. Tier A -- forwards to the already-real
	 * CSysExMsgTaskBase::SetTimeout(). Real return value is an unconditional 1.
	 */
	bool SetTimeout(unsigned short milliseconds);

	/* .text+0x080cf4a0, 33 bytes. Tier A -- forwards to the already-real
	 * CSysExMsgTaskBase::SendMsg(), discarding its return value (matches ground
	 * truth -- a void function).
	 */
	void SendSexMessage(const unsigned char *data, unsigned char len);

	/* .text+0x080cf410, 124 bytes. Tier A as a forward -- the real, faithfully-
	 * transcribed call into `mOwnerTask->BufferingTask()->Put()` (a Tier-B stub,
	 * buffering_task.h). Soft NULL assert on the buffering-task pointer omitted.
	 */
	void PutMessage(const unsigned char *data, unsigned char len);

	/* .text+0x080cf2f0, 133 bytes. Tier A (2026-07-26) -- see header comment. */
	void ReadPacket(unsigned char *data, unsigned char len);

	/* .text+0x080cf380, 133 bytes. Tier A (2026-07-26) -- see header comment. */
	void WritePacket(const unsigned char *data, unsigned char len);

	/* .text+0x080cf250, 91 bytes. Tier A (2026-07-26) -- see header comment. */
	bool IsDumpEnded();

private:
	CDumpTask *mOwnerTask; /* +0x1c */
};

#endif /* DUMP_MAN_STATE_MACHINE_H */
