/*
 * heap.h  -  CHeap, a standalone binary-heap/priority-queue container. Reconstructed
 * 2026-07-25 to unblock CChkCmdBG (chunk_man.h), the one sibling the earlier
 * small-derived-module batch deferred as "needs a new, unreconstructed CHeap class"
 * (see chunk_man.h's own file header). NOT related to OA.ko's CSTGHeapManager (a
 * completely different, unrelated class in a different binary/project).
 *
 * GROUND TRUTH: `nm -C`/symbols.csv show CHeap is a real binary-heap ADT with ~13
 * methods total (`GetHead`/`RemoveHead`/`TouchHead`/`MoveUp`/`MoveDown`/`TouchAll`/
 * `Insert`/`Realloc`/`StateSize`/`SaveState`/... at 0809cef0-0809d9xx) -- only its 2
 * constructor overloads (0809ccf0 0-arg, 0809cda0 2-arg) and destructor (0809cec0) are
 * reconstructed here, which is everything CChkCmdBG's own ctor/dtor need. The other
 * ~10 methods (the actual sift-up/down heap-queue logic) are genuinely out of scope --
 * nothing on this project's traced boot path calls them (CChkCmdBG's own further
 * methods -- Exec/ExecMsg/AcceptDuplicate -- that would exercise them are themselves
 * already deferred, see chunk_man.h) -- same "ctor/dtor only, rest is a deeper
 * out-of-scope subsystem" precedent as CChkBaseTask's own FindFirstModule/etc.
 *
 * Field layout (confirmed from both ctor bodies): a flat array of 8-byte slots,
 * `mCapacity` slots allocated by `new unsigned char[capacity * 8]` (`operator_new__`
 * in the decompile, i.e. raw non-zeroing array new -- NOT `calloc`/`new[]` with a
 * zeroing constructor). Only the second dword of each slot is zeroed by the ctor's own
 * init loop (GCC's usual 8-way-unrolled zero loop, collapsed to a plain loop here, same
 * license as COmegaPtrArray/CEvBuffersPool); the first dword of every slot is left
 * genuinely uninitialized in the real binary -- preserved verbatim (a real
 * uninitialized-value characteristic of ground truth, not a translation bug -- same
 * "preserve the real uninitialized read" precedent already established elsewhere in
 * this project, e.g. CSTGUnsolMsgHandler's EffectSlotMsgHandler).
 */

#ifndef HEAP_H
#define HEAP_H

class CHeap {
public:
	/* .text+0x0809ccf0, 172 bytes. Real body: equivalent to CHeap(8, 0x10) --
	 * mGrowBy=8, mCapacity=0x10 (16 slots, 0x80 bytes).
	 */
	CHeap();

	/* .text+0x0809cda0, 288 bytes. Real body -- see file header for field
	 * mapping. `growBy` is param_1 (this+0x08), `capacity` is param_2
	 * (this+0x00); a `capacity == 0` call allocates nothing (mSlots stays 0).
	 */
	CHeap(int growBy, int capacity);

	/* .text+0x0809cec0, 33 bytes. Real body: `delete[] mSlots` iff non-null. */
	~CHeap();

private:
	int   mCapacity; /* +0x00 */
	int   mCount;    /* +0x04, always 0 -- no reconstructed Insert() yet */
	int   mGrowBy;   /* +0x08 */
	void *mSlots;    /* +0x0c, array of mCapacity 8-byte slots, or 0 */

	friend struct CHeapTestHooks;
};

#endif /* HEAP_H */
