/*
 * level_manager_array.h  -  CLevelManagerArray + CLevelManager, CScheduler's own
 * level bookkeeping (Stage 6 breadth sweep, 2026-07-25). See src/base/scheduler.cpp's
 * own header comment for the full field-offset writeup this was reconstructed from.
 *
 * Broken out into its own header (rather than staying scheduler.cpp-local, its first
 * home) purely so verify/test_level_manager_array.cpp can drive Add()/Find() directly
 * against a synthetic COmegaPtrArray-shaped buffer -- same reason omega_ptr_array.h
 * exists as its own header. Not otherwise included from anywhere but scheduler.cpp.
 *
 * Both classes are raw-offset call-contract shims, not real C++ classes with
 * reconstructed field layouts (matching the manual-vtable-swap idiom used throughout
 * this project) -- callers pass/receive `void*` for the underlying byte buffers, same
 * as the real disassembly's own `this+0xNN` pointer arithmetic.
 */

#ifndef LEVEL_MANAGER_ARRAY_H
#define LEVEL_MANAGER_ARRAY_H

class CLevelManager;

class CLevelManagerArray {
public:
	/* .text+0x0805ee90, 258 bytes. Real signature takes an ETaskLevel; linear scan
	 * over the (now-sorted, see Add()) array, returns the first element whose own
	 * +0xc level field matches, or NULL if none does.
	 */
	static CLevelManager *Find(void *arrayThis, int level);

	/* .text+0x0805ec70, 522 bytes. Appends via the real COmegaPtrArray::Add() base
	 * method, then sifts the new element left while its own +0xc level number is
	 * smaller than its predecessor's -- keeps the array sorted ascending by level.
	 */
	static void Add(void *arrayThis, CLevelManager *level);
};

/* .text+0x0805ea10, 567 bytes -- reconstructed (Stage 6 breadth sweep, 2026-07-25).
 * Real body: calls CTaskBuffer::SendBuffer(this+4) (task_buffer.h -- CLevelManager's
 * own embedded +0x04..+0x0b bytes, zeroed by InsertLevel), then walks the level's own
 * +0x20 embedded array (count/base at the usual COmegaPtrArray-relative +0xc/+0x14,
 * i.e. absolute +0x2c/+0x34).
 *
 * CORRECTION (this pass): this array is TNamedPtrArray<CTask>, NOT
 * TNamedPtrArray<CModule> as a prior pass's own comment claimed -- confirmed by
 * ground-truth field arithmetic (RunLevel@0805ea10.c dereferences +0x4c/+0x78/+0x7a on
 * each element, matching CTask's own real ctor layout byte-for-byte -- CModule's own
 * ctor never touches those offsets, its base object is only 0x2c bytes; see
 * module.h/task_buffer.h). The vtable slot+8 dispatched per element is CTask::Exec()
 * (ground-truth confirmed real base body: `Exec@08180950`, 3 bytes, `return 0;`) --
 * entirely unrelated to CModule's OWN vtable slot+8 ("Setup", module_manager.cpp)
 * despite the coincidental same slot number; a prior pass's comment conflated the two,
 * this corrects it.
 *
 * Per element: skip if the task's own mask/flags byte (+0x4c) has either of its low 2
 * bits set (masked/disabled -- real code short-circuits on this BEFORE touching the
 * countdown at all, so a masked task's countdown stays frozen, not decremented);
 * otherwise decrement the countdown (+0x7a) and, once it reaches 0, reload it from the
 * task's own period (+0x78) and dispatch CTask's vtable slot+8. `CTask` is now a real,
 * reconstructed, constructible class (task.h/task.cpp, Stage 6, 2026-07-25) with a
 * confirmed real caller in ground truth (`CEditor::CPanelIfcTask`'s and `CPoller`'s own
 * ctors) -- **CORRECTION**: this comment's prior claim ("no caller anywhere in this
 * reconstruction's own call graph") is stale, see task.h's header comment for the full
 * writeup. That said, this array specifically (`CLevelManager`'s own per-scheduling-
 * level task queue, distinct from `CModule::mTasks` -- see module.h) is populated by a
 * DIFFERENT real mechanism this batch did not confirm reachable: `CScheduler::
 * InsertTask(CTask const&)` (.text+0x08062d80) exists in ground truth, by name and
 * signature the obvious candidate, but a full disassembly sweep this batch found ZERO
 * direct `call` instructions targeting it anywhere in the binary (only 2 raw 4-byte
 * occurrences of its address at all, neither yet confirmed to sit in an executable
 * dispatch table vs. some other data use) -- left as an open, flagged lead for a future
 * pass, not fabricated or assumed live. So: this array's own "faithful but currently-
 * empty in this reconstruction" status is UNCHANGED by this batch (still real code,
 * still not exercised) -- what changed is `CModule::mTasks` (module.h) and
 * `CModule::AdjustTaskMask()`'s own reachability, a different container.
 *
 * The real function's own tail unconditionally clears this level's missed-tick counter
 * (+0x1c, see CScheduler::Exec(), scheduler.cpp) regardless of what the loop above did.
 */
class CLevelManager {
public:
	static void RunLevel(void *this_);

	/* Added Eva CAlphaKeybCtrl/CAlphaKeybCtrlTask batch, 2026-07-26 --
	 * `COutLinkIfcBase::GetDirectIfcPtr()`'s own real ground-truth body
	 * (.text+0x0807b8e0, out_link_ifc.cpp) calls a virtual method through
	 * `this+0x40`'s own vtbl+0x10 slot whenever that field is non-null; a raw
	 * `nm -C` cross-check confirms the real target is
	 * `CLevelManager::StopForMessage(SStateRegisterForMsg&)`
	 * (.text+0x0805e120-ish region, not individually transcribed -- genuine
	 * scheduler/message-pump depth, same "declare real signature, Tier-B stub
	 * body" convention as `CPoller::InitButtons()`/`InitAnalogs()`, poller.h).
	 * `GetDirectIfcPtr()`'s own `this+0x40` field (`mDirectTarget`) is NEVER
	 * populated by anything in this reconstruction's own call graph (same
	 * "field never populated, real code handles it gracefully" status as
	 * `CPanel::mPoller`, panel.h), so this stub is never actually invoked by
	 * any KAT this batch wrote -- present purely so `GetDirectIfcPtr()`'s own
	 * real, faithfully-transcribed body links and matches ground truth's
	 * call shape exactly.
	 */
	static void StopForMessage(void *this_, void *stateOut);

	/* Companion of StopForMessage() above -- `CAlphaKeybCtrlTask::
	 * ProcessEvent()`'s own real body (alpha_keyb_ctrl_task.cpp) calls this
	 * directly (not through a vtable) whenever `GetDirectIfcPtr()` handed back
	 * a non-null `CLevelManager*`, which -- for the same reason as
	 * StopForMessage() above -- never happens in this reconstruction. Tier-B
	 * stub, same status.
	 */
	static void ResumeAfterMessage(void *this_, void *state);
};

#endif /* LEVEL_MANAGER_ARRAY_H */
