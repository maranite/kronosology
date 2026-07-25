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
 * task's own period (+0x78) and dispatch CTask's vtable slot+8. CTask itself is not
 * reconstructed as a constructible class here (its real ctor, `CTask@0807ee80.c`, has
 * no caller anywhere in this reconstruction's own call graph -- nothing on the traced
 * boot path ever calls `new CTask(...)`, so the level's own task queue is always empty
 * in practice, same "faithful but currently-empty" status CLevelManagerArray itself
 * had before the prior Stage-6 batch populated it) -- only its real per-tick field
 * layout is modeled, sufficient for this loop.
 *
 * The real function's own tail unconditionally clears this level's missed-tick counter
 * (+0x1c, see CScheduler::Exec(), scheduler.cpp) regardless of what the loop above did.
 */
class CLevelManager {
public:
	static void RunLevel(void *this_);
};

#endif /* LEVEL_MANAGER_ARRAY_H */
