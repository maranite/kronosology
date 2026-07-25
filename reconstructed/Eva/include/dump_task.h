/*
 * dump_task.h  -  CDumpTask : public CSysExMsgTaskBase, Stage 6 breadth sweep,
 * 2026-07-25 (DumpManager cluster batch). See circ_byte_buffer.h's own header comment
 * for the shared reachability chain, and sysex_msg_task_base.h's own "GROUND TRUTH
 * REACHABILITY" note -- THIS class's ctor is the real, direct caller of
 * `CSysExMsgTaskBase::CSysExMsgTaskBase()` that note already documented (that header
 * called this class out of scope at the time; now reconstructed).
 *
 * This is the FIRST time this reconstruction's own wired call graph genuinely
 * exercises `CSysExMsgTaskBase`'s `ECanTransmit==1` branch end to end: `CDumpTask`'s
 * ctor passes `canTransmit=1` (see below), so constructing a `CDumpTask` for real
 * (via `CDumpManMod::Setup()`, dump_man_mod.h, itself dispatched by the already-real
 * `CModuleManager::Setup()` on the actual boot path) genuinely mallocs and registers a
 * real `CSysExMsgClientOutLink` through the already-real `CTask::Add(COutLink*)` --
 * previously "ground-truth reachable but dead in this reconstruction's own current
 * call graph" per both task.h's and sysex_msg_task_base.h's own header comments.
 *
 * REAL LAYOUT (0x98 bytes malloc'd; own fields start at CSysExMsgTaskBase's own 0x8c
 * end, confirmed from CDumpTask@080d1b10.c):
 *   +0x90  mMachine        `CDumpMachine*` (dump_man_state_machine.h) -- the real
 *                          constructed type, even though ground truth's own decompile
 *                          types the malloc loosely as `CDumpManStateMachine*`
 *                          (Ghidra couldn't distinguish the two; the actual ctor
 *                          CALLED is `CDumpMachine::CDumpMachine`).
 *   +0x94  mBufferingTask  `CBufferingTask*` (buffering_task.h) -- ctor sets 0;
 *                          `CDumpManMod::Setup()` sets the real value via
 *                          `LinkBufferingTask()` right after both sibling tasks exist
 *                          (the cross-link `CDumpMachine::PutMessage()`, dump_man_
 *                          state_machine.h, reads back via `BufferingTask()`).
 *
 * `OnGetMessage()`/`OnReceiveMessage()`/`OnTimeout()` are all real, one-line forwards
 * into `mMachine`'s own (Tier-B stub) `CDumpManStateMachine` methods -- Tier A here
 * even though their targets are stubs, same "the forward is real" precedent as
 * `CDumpMachine::PutMessage()`. `OnTimeout()` specifically required re-deriving from
 * raw `objdump -dr` rather than trusting Ghidra's own decompile, which mis-resolved a
 * genuine tail call as a nonsensical zero-argument double-indirection (Ghidra's own
 * "Could not recover jumptable... Treating indirect jump as call" warning on this
 * function) -- same class of Ghidra artifact this project already hit for
 * `CTask::Add(COutLink*)` (task.h).
 */

#ifndef DUMP_TASK_H
#define DUMP_TASK_H

#include "sysex_msg_task_base.h"

class CModule;
class CDumpMachine;
class CBufferingTask;

class CDumpTask : public CSysExMsgTaskBase {
public:
	/* .text+0x080d1b10, 197 bytes. */
	explicit CDumpTask(const CModule &owner);

	/* .text+0x080d19d0/0x080d1a60 (D1/D0 pair). */
	~CDumpTask();

	/* Real: sets mBufferingTask (+0x94) -- named wrapper for the raw poke
	 * `CDumpManMod::Setup()`'s own ground-truth body performs.
	 */
	void LinkBufferingTask(CBufferingTask *task) { mBufferingTask = task; }

	/* Read back by `CDumpMachine::PutMessage()` (dump_man_state_machine.h). */
	CBufferingTask *BufferingTask() const { return mBufferingTask; }

	/* .text+0x080d1c20, 119 bytes. Tier A -- see header comment. */
	void OnGetMessage(const unsigned char *data, unsigned char len);

	/* .text+0x080d18d0, 244 bytes. Tier A -- see header comment. Real 3 soft
	 * asserts (mCommId != 0xff, param_1 == mCommId, data != 0) omitted, same
	 * convention as every other soft assert in this project.
	 */
	void OnReceiveMessage(unsigned char commId, const unsigned char *data, unsigned char len);

	/* .text+0x08186880, 28 bytes. Tier A -- see header comment (re-derived from
	 * raw disassembly, not Ghidra's own mis-decompiled pseudocode).
	 */
	void OnTimeout();

private:
	CDumpMachine    *mMachine;       /* +0x90 */
	CBufferingTask  *mBufferingTask; /* +0x94 */

	friend struct DumpTaskTestHooks;
};

#endif /* DUMP_TASK_H */
