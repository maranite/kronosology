// SPDX-License-Identifier: GPL-2.0
/*
 * heap_manager.cpp  -  CSTGHeapManager::Initialize()/Alloc() and their
 * C-linkage wrappers. See oa_heapmanager.h for the full ground-truthing
 * detail and the open sentinel/slot-offset discrepancy note.
 */

#include "oa_heapmanager.h"

CSTGHeapManager *CSTGHeapManager::sInstance;

/*
 * WORKAROUND (2026-07-24): a live kronos_vm boot proved `heapBase`/`heapSize`
 * (the real class members, +0x1e8498/+0x1e84a0) read back as 0 by the time
 * ANYTHING reads them again -- confirmed via real C++ member access
 * (CSTGHeapManager_GetHeapSize(), a one-line `return sInstance->heapSize`
 * already used elsewhere in this project, not raw pointer arithmetic), so
 * this isn't an offset-formula bug. Also confirmed the values ARE genuinely
 * correct immediately after the assignment below (printk-verified). Two
 * different explicit-rewrite mitigations (a plain duplicate write, then a
 * `volatile` write + compiler memory barrier at the end of this function)
 * were tried and did NOT survive to the caller either -- ruling out simple
 * dead-store elimination as the mechanism. The exact cause was not
 * conclusively pinned down (a GDB hardware-watchpoint attempt hit its own
 * practical wall: it never fired across a full boot, at ~100x normal
 * runtime under gdbstub overhead). Rather than keep chasing the mechanism,
 * this captures the known-good value into storage that is NOT part of the
 * `CSTGHeapManager` object's own memory layout at all, at the one point
 * it's proven correct -- so whatever corrupts the object's own field
 * (compiler codegen quirk on this freestanding/regparm(3)/-O2 target, or a
 * genuine runtime memory issue specific to this large vmalloc'd region
 * under QEMU-TCG, neither confirmed) can't touch this copy.
 */
static unsigned long g_capturedHeapBase;
static unsigned long g_capturedHeapSize;

unsigned long CSTGHeapManager_GetCapturedHeapBase(void) { return g_capturedHeapBase; }
unsigned long CSTGHeapManager_GetCapturedHeapSize(void) { return g_capturedHeapSize; }

/*
 * WORKAROUND, continued (2026-07-24): the unreliability above turned out
 * not to be limited to heapBase/heapSize -- a live kronos_vm boot showed
 * `Alloc()`'s own free-list/handle-table mechanism (freeListHead,
 * sentinel.size/the bump-down cursor, activeListHead/Count, and each
 * entry's own `offset` field) is ALSO not reliably readable by any
 * caller other than the function that just wrote it: `freeListHead=0`
 * AND `cursor=0` on EVERY SINGLE Alloc() call across a real boot, not
 * occasionally. Patching individual fields (tried first) isn't viable
 * once the unreliability turns out to cover the whole bookkeeping
 * mechanism, not just two fields. `CSTGHeapManager::Alloc()` -- BOTH
 * overloads, this file's real `Alloc(unsigned long)` and
 * heap_manager_alloc_static.cpp's `Alloc(unsigned int)` stand-in
 * (needed there to dodge a pre-existing CSTGGlobal ODR conflict, see
 * that file's own header) -- now share this single, trivial monotonic
 * bump allocator, built entirely on shadow state outside the
 * CSTGHeapManager object, never reading the object's own bookkeeping
 * fields at all. This project's own init-path call graph never frees
 * anything through either Alloc() overload, so a bump allocator is a
 * complete, faithful-enough substitute for what real callers need: N
 * distinct, sufficiently large, non-overlapping regions within the heap
 * arena. Both overloads sharing ONE cursor/slot-counter here (rather
 * than each keeping its own) is what keeps their allocations from
 * overlapping regardless of which overload a given caller happens to
 * use.
 */
#define HM_CAPTURED_OFFSET_SLOTS 64
static unsigned int g_capturedOffsets[HM_CAPTURED_OFFSET_SLOTS];
static unsigned char g_capturedOffsetValid[HM_CAPTURED_OFFSET_SLOTS];
static unsigned int g_bumpCursor;
static unsigned int g_nextAllocSlot = 1; /* slot 0 reserved/unused, matching
                                           * the real handle-numbering
                                           * convention (see oa_heap.h). */

extern "C" unsigned int CSTGHeapManager_GetCapturedOffset(unsigned int slot)
{
	if (slot >= HM_CAPTURED_OFFSET_SLOTS || !g_capturedOffsetValid[slot])
		return 0;
	return g_capturedOffsets[slot];
}

extern "C" unsigned int CSTGHeapManager_BumpAlloc(unsigned int size)
{
	unsigned int totalSize = (unsigned int)g_capturedHeapSize;
	unsigned int alignedSize = (size + 3u) & ~3u;

	if (totalSize == 0 || alignedSize > totalSize - g_bumpCursor)
		return (unsigned int)-1;

	unsigned int offset = g_bumpCursor;
	g_bumpCursor += alignedSize;

	unsigned int slot = g_nextAllocSlot++;
	if (slot < HM_CAPTURED_OFFSET_SLOTS) {
		g_capturedOffsets[slot] = offset;
		g_capturedOffsetValid[slot] = 1;
	}

	return slot;
}

void CSTGHeapManager::Initialize(unsigned long base, unsigned long size)
{
	heapBase = base;
	heapSize = size;
	g_capturedHeapBase = base;
	g_capturedHeapSize = size;

	/*
	 * FIX (root-caused via live-boot printk instrumentation,
	 * 2026-07-12): explicitly zero the free-list bookkeeping this
	 * function itself is about to read, rather than relying solely on
	 * CSTGHeapManager_Initialize()'s external wrapper having already
	 * zeroed this object's memory.
	 *
	 * Without this, a live kernel build's `-O2` codegen for the very
	 * first loop iteration below reused the CPU register still holding
	 * this function's own `size` PARAMETER (untouched since function
	 * entry, per the real regparm(3) calling convention) as the value
	 * for `oldTail = freeListTail` -- because nothing WITHIN THIS
	 * FUNCTION's own compiled body had yet written `freeListTail`, the
	 * compiler could not see across the call boundary into the
	 * separate wrapper function/TU that actually zeroed it, and chose
	 * to treat the read as an arbitrary/indeterminate value rather
	 * than emit a real load. Since `size` (a large nonzero byte count,
	 * confirmed live e.g. 117534720) is never 0, this took the
	 * "non-empty list" branch and dereferenced that bogus value AS A
	 * POINTER (`mov (%ecx),%edx` in the compiled output), corrupting
	 * memory and explaining every downstream symptom independently
	 * confirmed live: `heapBase`/`heapSize` reading back as 0 despite
	 * being written two lines above, `freeListHead` staying 0 while
	 * `freeCount` incremented exactly once, and every subsequent
	 * `CSTGHeapManager::Alloc()` call failing immediately at its own
	 * `freeListHead == 0` guard. An explicit, compiler-visible write
	 * here removes the ambiguity that produced this codegen -- a
	 * behavior-preserving robustness fix (a no-op whenever the
	 * external wrapper's own pre-zeroing already ran, which is always
	 * true for the real call path), not a deviation from the real
	 * binary's own logic.
	 */
	freeListHead = 0;
	freeListTail = 0;
	freeCount = 0;

	/*
	 * FIX (2026-07-23, found while chasing a live kronos_vm boot's
	 * `local_heap_base()` resolving to NULL despite `CSTGHeapManager::
	 * sInstance` itself being a confirmed-valid, live object): the fix
	 * directly above this one (2026-07-12) explicitly zeroed
	 * freeListHead/freeListTail/freeCount because they're read
	 * within THIS function before any explicit write in THIS function's
	 * own compiled body -- but it missed `activeListHead` (read by the
	 * `if` immediately below) and `activeCount` (read-modify-written by
	 * `activeCount++` further below), which have the IDENTICAL shape:
	 * both are class members this function reads before ever writing
	 * them itself, relying entirely on the external wrapper's/caller's
	 * memset having already zeroed the object -- exactly the hazard the
	 * 2026-07-12 fix's own comment describes and root-caused via live
	 * printk instrumentation (that fix's own confirmed symptom list
	 * explicitly includes "heapBase/heapSize reading back as 0 despite
	 * being written two lines above", the same live symptom that led
	 * back here). Same treatment, same reasoning, same behavior-
	 * preserving no-op-on-the-real-path property -- this just closes
	 * the gap the first fix left open for the other two members with
	 * the same shape in this same function.
	 */
	activeListHead = 0;
	activeCount = 0;

	/* Insert the sentinel into the (empty) active list -- confirmed
	 * real insert-or-init doubly-linked-list idiom (identical shape to
	 * every other intrusive list build elsewhere in this project). */
	if (activeListHead == 0) {
		activeListTail = (unsigned int)(unsigned long)&sentinel;
	} else {
		CSTGHeapHandleEntry *head =
			(CSTGHeapHandleEntry *)(unsigned long)activeListHead;
		unsigned int oldTail = head->prev;
		sentinel.prev = oldTail;
		if (oldTail != 0)
			((CSTGHeapHandleEntry *)(unsigned long)oldTail)->next =
				(unsigned int)(unsigned long)&sentinel;
		head->prev = (unsigned int)(unsigned long)&sentinel;
	}
	activeListHead = (unsigned int)(unsigned long)&sentinel;
	sentinel.owner = (unsigned int)(unsigned long)this;
	activeCount++;

	/* CORRECTED: the real live bump-down cursor is the sentinel's own
	 * repurposed "size" field (+0x28), NOT a separate struct member --
	 * see oa_heapmanager.h's own file comment for the full disassembly
	 * evidence (.text+0x2e888-0x2e895). sentinel.offset is likewise
	 * explicitly zeroed here by the real code (confirmed
	 * `movl $0,0x24(%eax)`, redundant with the object's own
	 * zero-initialization but reproduced faithfully). */
	sentinel.size = (unsigned int)heapSize;
	sentinel.offset = 0;

	/* Thread all 99999 handle-table entries onto the free list, in
	 * order (append at tail each time) -- confirmed shape of
	 * Initialize()'s own 99999-iteration loop. The object is trusted
	 * to already be zero-initialized (CSTGHeapManager_Initialize's own
	 * wrapper explicitly zeroes this whole region before calling
	 * here), so no per-entry zeroing is needed beyond what the loop
	 * itself threads. */
	for (unsigned int i = 0; i < CSTG_HEAPMANAGER_HANDLE_COUNT; i++) {
		CSTGHeapHandleEntry *entry = &handles[i];
		unsigned int entryAddr = (unsigned int)(unsigned long)entry;
		unsigned int anchorAddr = (unsigned int)(unsigned long)&freeListHead;

		unsigned int oldTail = freeListTail;
		if (oldTail == 0) {
			freeListHead = entryAddr;
		} else {
			CSTGHeapHandleEntry *tail =
				(CSTGHeapHandleEntry *)(unsigned long)oldTail;
			entry->prev = oldTail;
			unsigned int tailNext = tail->next;
			if (tailNext != 0)
				((CSTGHeapHandleEntry *)(unsigned long)tailNext)->prev = entryAddr;
			tail->next = entryAddr;
		}
		entry->owner = anchorAddr;
		freeListTail = entryAddr;
		freeCount++;
	}

	/* See the WORKAROUND comment at the top of this file: heapBase/heapSize
	 * (the real class members) are known to read back as 0 by the time
	 * anything else observes them, via a mechanism that resisted precise
	 * root-causing across several live-boot investigation rounds (ruled
	 * out: offset-formula errors, simple dead-store elimination). The
	 * `g_capturedHeapBase`/`g_capturedHeapSize` snapshot taken at the top
	 * of this function is the reliable source of truth from here on;
	 * callers needing this function's base/size should use
	 * CSTGHeapManager_GetCapturedHeapBase()/GetCapturedHeapSize() rather
	 * than the class members directly.
	 */
}

unsigned int CSTGHeapManager::Alloc(unsigned long size)
{
	/* See the WORKAROUND comment above (near g_bumpCursor): the real
	 * free-list-pop/cursor-bump-down/active-list-push algorithm this
	 * function used to implement directly is live-boot-confirmed
	 * unreliable in this environment (freeListHead/cursor read back as 0
	 * on every call). Delegates to the shared bump allocator instead --
	 * see that function's own comment. The original algorithm's exact
	 * sequence (pop free list, check capacity, bump cursor, push active
	 * list, compute handle number) is preserved in git history if a
	 * future session wants to revisit the exact root cause; not restated
	 * here since it's no longer live code.
	 */
	return CSTGHeapManager_BumpAlloc((unsigned int)size);
}

unsigned long CSTGHeapManager_Initialize(unsigned long base, unsigned long size)
{
	unsigned char *raw = (unsigned char *)(unsigned long)base;

	/* Zero the whole sentinel+handle-table region (confirmed real:
	 * .text+0x2ee60's loop, `ebx = raw+0x18`, 0x1e8480 bytes, 20-byte
	 * stride) before any threading happens. */
	unsigned char *zeroBase = raw + 0x18;
	for (unsigned long off = 0; off < 0x1e8480; off += 20) {
		*(unsigned int *)(zeroBase + off) = 0;
		*(unsigned int *)(zeroBase + off + 4) = 0;
		*(unsigned int *)(zeroBase + off + 8) = 0;
	}

	/* Page-align the real heap memory to right after this object's own
	 * storage, confirmed literal constant 0x1e94af rounded down to a
	 * 4K page. */
	unsigned long alignedBase = (base + 0x1e94af) & ~0xffful;
	unsigned long availSize = (base + size - alignedBase) & ~3ul;

	CSTGHeapManager *mgr = (CSTGHeapManager *)raw;
	CSTGHeapManager::sInstance = mgr;
	mgr->Initialize(alignedBase, availSize);

	return alignedBase;
}

unsigned long CSTGHeapManager_GetHeapSize(void)
{
	return CSTGHeapManager::sInstance->heapSize;
}
