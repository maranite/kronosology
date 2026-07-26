/*
 * notify_list.h  -  CNotifyList, a small CGlobalObjectBase-derived free-list-backed
 * change-notification queue.
 *
 * Found 2026-07-26 while rechecking edit_server.cpp's own long-standing "CNotifyList
 * is out of scope (its own subsystem: GrowEventsList/ReleaseList/GetList, none
 * reconstructed)" verdict as part of a broad Tier-B recheck sweep -- another "size is
 * not depth" case: every real method here is a small, self-contained singly-linked
 * free-list operation, no Peg/DSP/CZ dependency whatsoever. `nm -C` on the ground-truth
 * binary shows a real vtable + typeinfo (CNotifyList : public CGlobalObjectBase, same
 * base every XxxApiInstance singleton in this project uses -- global_object_base.h),
 * confirming this is a genuine, previously-unmodeled small class, not just a stub
 * surface.
 *
 * `CEditServer::m_oNotifyList` (edit_server.cpp/.h) is a REAL embedded 0xc-byte static
 * object at .bss+0x0930a248 (confirmed via `nm -C -S`, size exactly matches
 * sizeof(CGlobalObjectBase)+2*sizeof(void*) = 4+4+4 = 0xc) -- NOT a bare pointer as the
 * prior pass modeled it. `PutNotify()`'s own real gating (sm_bNotifyEnabled defaults
 * false) still means this queue is never actually exercised by anything on this pass's
 * own traced boot path, but the class itself is now real rather than a fabricated
 * no-op, matching this project's established "reconstruct real-but-unreached code"
 * convention (CScheduler::Exec()'s own dead timer walk, HAL_GetSystemTime(), etc.).
 *
 * Real layout (CNotifyList::Put()/GetList()/ReleaseList(), .text+0x08071000 and
 * neighbors, cross-checked against CGlobalObjectBase's own +0x00 mVtbl):
 *   +0x00  mVtbl   inherited from CGlobalObjectBase, immediately overwritten with this
 *                   class's own vtable (PTR__CNotifyList_08e81828) by the ctor
 *   +0x04  mFirst  head of this instance's own pending-notification queue (SNotifyEvent
 *                   singly-linked list) -- NULL when empty
 *   +0x08  mLast   tail of the same queue -- Put()'s own fast dedup check only ever
 *                   compares against *this* node (the most-recently-queued one), not
 *                   the whole list
 *
 * SNotifyEvent nodes (8 bytes, malloc(8)'d from a shared static free list,
 * g_poNotifyFreeList/CNotifyList::sm_ptFirstFreeEvent @ .bss+0x0930a260) are never
 * individually freed back to the OS -- only ever recycled between this free list and
 * whichever CNotifyList instance's own mFirst/mLast queue currently holds them.
 */

#ifndef NOTIFY_LIST_H
#define NOTIFY_LIST_H

#include "global_object_base.h"

struct SNotifyEvent {
	unsigned char group;    /* +0x0 */
	unsigned char index;    /* +0x1 */
	unsigned char subIndex; /* +0x2 */
	unsigned char pad;      /* +0x3 -- unused, alignment only */
	SNotifyEvent *next;     /* +0x4 */
};

class CNotifyList : public CGlobalObjectBase {
public:
	/* .text+0x08070c40, 542 bytes. Real body: base-construct CGlobalObjectBase,
	 * install this class's own vtable, then -- if the shared free list
	 * (g_poNotifyFreeList) is empty -- grow it. Ground truth's own GCC inlined
	 * GrowEventsList()'s entire 32-node malloc chain directly into this ctor
	 * (a single call site, small static callee); modeled here as a plain call to
	 * GrowEventsList() instead -- semantically identical, same "collapse an
	 * inlined/unrolled sequence to its equivalent" license used throughout this
	 * project. mFirst/mLast always start NULL (empty queue) either way.
	 */
	CNotifyList();

	/* .text+0x08070b40 (D1) / .text+0x08070b60 (D0, this class's own real, real
	 * `+ free(this)` deleting destructor -- see .cpp for the free-function-thunk
	 * treatment, matching global_object_base.cpp's own CGlobalObjectBase_Dtor/
	 * CGlobalObjectBase_DeletingDtor precedent).
	 */
	~CNotifyList();

	/* .text+0x08071000, 218 bytes. Real "post a change notification" entry point:
	 * dedups against mLast (the most-recently-posted notification only, not the
	 * whole queue) -- if it matches (group,index,subIndex) exactly, returns
	 * without touching anything; otherwise pops a node off the shared free list
	 * (growing it first if empty), fills it in, and appends to this instance's
	 * own mFirst/mLast queue. Every malloc/free/free-list pop is bracketed by its
	 * own HAL_DisableInterrupts()/HAL_EnableInterrupts() pair in the real
	 * disassembly -- dropped here, same established reason as every other
	 * occurrence of that pair in this project (a kernel-side critical-section
	 * shim, not a real userspace primitive, and this reconstruction is
	 * single-threaded).
	 */
	void Put(unsigned char group, unsigned char index, unsigned char subIndex);

	/* .text+0x080710e0, 61 bytes. Real "drain the queue" entry point: if mFirst is
	 * non-NULL, atomically swaps it out (mFirst/mLast reset to NULL) into a
	 * function-local static (CNotifyList::GetList()::ptFirstNotify,
	 * .bss+0x0930a264 -- real ground-truth symbol, kept as a static local here to
	 * match its own real scope) and returns that; otherwise returns NULL.
	 */
	SNotifyEvent *GetList();

	/* .text+0x08071120, 56 bytes. Real "return a whole already-drained chain [first
	 * .. last] back to the shared free list" helper -- prepends it in one shot
	 * (`g_poNotifyFreeList = first; last->next = <old free list head>;`), no walk.
	 */
	static void ReleaseList(SNotifyEvent *first, SNotifyEvent *last);

	/* .text+0x08071160, 126 bytes. Real "release a possibly-multi-node chain given
	 * only its head" overload: real disassembly is a GCC-unrolled 8-way linked-list
	 * walk to find the tail, collapsed here to a plain `while` loop (same license
	 * as every other Duff's-device-style collapse in this project), then delegates
	 * to the 2-argument overload above.
	 */
	static void ReleaseList(SNotifyEvent *list);

	/* .text+0x08070e60, 403 bytes. Real "grow the shared free list by 32 nodes"
	 * helper: mallocs 32 SNotifyEvent-shaped 8-byte nodes (each bracketed by its
	 * own HAL_DisableInterrupts()/HAL_EnableInterrupts(), dropped here), chains
	 * them, then prepends the whole new chain onto g_poNotifyFreeList. Ground
	 * truth's own body is a GCC partial-unroll (9-wide inner block + 1 trailing,
	 * repeated) of this exact 32x pattern -- collapsed to a plain loop here, same
	 * license as omega_ptr_array.cpp's own Duff's-device collapses. Node count (32)
	 * re-derived by hand-tracing the real unroll's own remaining-count arithmetic,
	 * not guessed.
	 */
	static void GrowEventsList();

	/* .text+0x08070bb0, real signature `PostKernelDestructor(unsigned long)`.
	 * Real body: walks BOTH the shared free list and this instance's own mFirst
	 * queue, `free()`ing every node in each (draining both to empty), then returns
	 * 0. The `unsigned long` argument is never read (same "real signature, unused
	 * flag" shape as CGlobalObjectBase's own 4 phase hooks). Not a C++ `virtual`
	 * override -- installed into this class's own manual vtable array like every
	 * other vtable slot in this project (see .cpp).
	 */
	int PostKernelDestructor(unsigned long flags);

private:
	SNotifyEvent *mFirst; /* +0x04 */
	SNotifyEvent *mLast;  /* +0x08 */
};

#endif /* NOTIFY_LIST_H */
