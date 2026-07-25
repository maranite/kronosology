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

/* .text+0x0805ea10, 567 bytes -- Tier-B link-stub. Real body: calls
 * CTaskBuffer::SendBuffer(this+4) (a wholly unintroduced class -- CLevelManager's own
 * embedded +0x04..+0x0b bytes, zeroed by InsertLevel), then walks the level's own
 * +0x20 embedded TNamedPtrArray<CModule> task queue, decrementing each CModule's own
 * per-task countdown (+0x7a, reload from +0x78) and dispatching through that module's
 * own vtable slot +8 ("Update") when it reaches 0 -- CModule's real per-tick behavior,
 * genuinely out of scope for this pass (would pull in every registered module's own
 * Update() body). The ONE real side effect kept here: the real function's own tail
 * unconditionally clears this level's missed-tick counter (+0x1c, see
 * CScheduler::Exec(), scheduler.cpp) -- trivial, real, and worth preserving even
 * though the task-queue walk above it isn't modeled.
 */
class CLevelManager {
public:
	static void RunLevel(void *this_)
	{
		*(int *)((char *)this_ + 0x1c) = 0;
	}
};

#endif /* LEVEL_MANAGER_ARRAY_H */
