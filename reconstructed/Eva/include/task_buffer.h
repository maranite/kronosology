/*
 * task_buffer.h  -  CTaskBuffer, the per-CLevelManager buffered-message queue
 * (Stage 6 breadth sweep, 2026-07-25). Every CLevelManager embeds one of these at
 * +0x04 (8 bytes, see level_manager_array.h) -- CLevelManager::RunLevel()'s own first
 * statement is `CTaskBuffer::SendBuffer((CTaskBuffer*)(this+4))`.
 *
 * CORRECTION (Stage 6, 2026-07-25, CTask::CTask() reconstruction batch): this file's
 * own prior note ("CTask::CTask() has no caller anywhere in this reconstruction's own
 * call graph") is now stale. `CTask::CTask()` genuinely IS called in ground truth (by
 * `CEditor::CPanelIfcTask::CPanelIfcTask()` and `CPoller::CPoller()`, both confirmed via
 * direct disassembly) and is now reconstructed for real (task.h/task.cpp), along with
 * the actual `mTasks`-populating method, `CModule::Add(CTask*)` (module.h/module.cpp) --
 * itself boot-path-reachable from `CEditor::Setup()` via the already-real
 * `CModuleManager::Setup()` dispatch. `CTaskBuffer` itself is a SEPARATE queue from
 * `CModule::mTasks`/`CLevelManager`'s own task array, though (see below) -- nothing
 * newly reconstructed this batch enqueues into a CTaskBuffer specifically, so THIS
 * class's own producer-side-unreached status is unchanged; see task.h's own header
 * comment for the full writeup of what did change (CModule::AdjustTaskMask() /
 * CLevelManager::RunLevel(), not CTaskBuffer).
 *
 * Real layout confirmed from ~CTaskBuffer@08055ec0.c / SendBuffer@08055f20.c: this
 * class is NOT polymorphic (no vtable -- SendBuffer() dereferences `*this` directly as
 * a linked-list head pointer, never as a vtable pointer):
 *   +0x00  mHead      head of a singly-linked list of pending "buffered message" nodes
 *                       (CBufferedMessage*, itself not reconstructed -- see below)
 *   +0x04  mUnused04  zeroed by CScheduler::InsertLevel() alongside mHead; never read
 *                       back by SendBuffer() or the destructor -- real purpose (a tail
 *                       pointer for O(1) append in AddMessage(), never itself
 *                       reconstructed here) not confirmed
 *
 * SendBuffer() (.text+0x08055f20, 137 bytes) pops and drains the whole list: for each
 * node, dispatches a vtable call through the node's own stored "target" pointer
 * (adjusted by a fixed +8 this-offset -- the classic multiple-inheritance interface-
 * adjustment thunk pattern, matching CTask's own ctor registering itself via
 * `RegisterIfc(this, (CIfcUnknown*)(this+0x60))`; CTask::RegisterIfc/CIfcUnknown
 * themselves are not reconstructed -- genuinely out of scope, see CTask note below),
 * passing the node's own embedded CMessage sub-object (starting right after the "next"
 * link, i.e. the node's own address + 4) as that call's argument. Frees an optional
 * extra payload (node+0x14) if a flag byte at node+0xd has bit 1 set, then returns the
 * node itself to a GLOBAL free-list pool (sm_poPool/sm_wCount) rather than freeing it
 * directly -- a real allocator-reuse detail, preserved faithfully.
 *
 * **Nothing in this reconstruction ever enqueues into a CTaskBuffer** -- AddMessage()/
 * NewBufferMessage()/PurgeMessages()/FreeBufferMessage()/ShrinkPool() (the producer
 * side, .text+0x080561a0/0x08056090/0x08055fc0/0x080561f0/0x08056260) are not called by
 * anything on this pass's own traced call path and are NOT reconstructed here -- so
 * mHead is always null in practice and SendBuffer()'s own while-loop body (including
 * the vtable dispatch whose target-object identity isn't fully decoded) is real but
 * currently unreached, same "faithful but currently-dead branch" license already
 * established for CScheduler::Exec()'s own +0x10/+0x38 bail checks
 * (reconstructed/Eva/src/base/scheduler.cpp).
 *
 * ~CTaskBuffer() (.text+0x08055ec0, 57 bytes) is real but, worth flagging as found: it
 * walks the GLOBAL sm_poPool free-list and frees every node in it -- it does NOT free
 * `this` object's own (possibly still-pending) mHead list. Preserved exactly as found,
 * not "fixed" into freeing the instance's own queue -- this looks like the real
 * program only ever destructs the LAST CTaskBuffer(s) as part of process teardown, at
 * which point releasing the whole shared allocator pool once is the intended effect.
 */

#ifndef TASK_BUFFER_H
#define TASK_BUFFER_H

class CTaskBuffer {
public:
	/* .text+0x08055f20, 137 bytes. See header comment -- real but unreached given
	 * this reconstruction's own data (mHead always null).
	 */
	void SendBuffer();

	/* .text+0x08055ec0, 57 bytes. Frees the GLOBAL free-node pool, not `this`'s own
	 * list -- see header comment.
	 */
	~CTaskBuffer();

private:
	void *mHead;     /* +0x00 */
	int   mUnused04; /* +0x04 */

	/* Real class-static free-list pool + count (.data+0x09309660/0x09309670).
	 * SendBuffer() pushes freed nodes here instead of calling free() directly;
	 * ~CTaskBuffer() drains this pool (not `this`'s own mHead -- see above).
	 * sm_wCount's real width wasn't confirmed from the decompile alone (Ghidra never
	 * showed a narrowing cast on it); `int` is a safe superset, and nothing in this
	 * reconstruction reads it back.
	 */
	static void *sm_poPool;
	static int   sm_wCount;
};

#endif /* TASK_BUFFER_H */
