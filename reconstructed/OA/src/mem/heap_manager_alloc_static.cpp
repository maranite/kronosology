// SPDX-License-Identifier: GPL-2.0
/*
 * heap_manager_alloc_static.cpp  -  batch 17: CSTGHeapManager::
 * Alloc(unsigned int), this project's own LOCAL "static" stand-in for
 * the real instance method CSTGHeapManager::Alloc(unsigned long) (see
 * oa_heapmanager.h / src/mem/heap_manager.cpp for the full class + real
 * ground-truthing, and oa_setup_global_resources.h's own file comment
 * for why setup_global_resources.cpp can't include oa_heapmanager.h
 * directly -- the pre-existing CSTGGlobal ODR conflict).
 *
 * `oa_setup_global_resources.h`'s own local minimal stand-in:
 *   struct CSTGHeapManager {
 *       static char *sInstance;
 *       static unsigned int Alloc(unsigned int size);
 *   };
 * is only a raw pointer (not the real class layout, again to dodge the
 * ODR conflict) -- `Alloc(unsigned int)` here is therefore a SEPARATE,
 * differently-mangled symbol (`_ZN15CSTGHeapManager5AllocEj`) from the
 * real `_ZN15CSTGHeapManager5AllocEm` (`unsigned long`) already
 * reconstructed in heap_manager.cpp, NOT an overload conflict. `sInstance`
 * itself IS the same shared symbol either way (Itanium ABI static-data-
 * member mangling ignores the declared type), already defined once in
 * heap_manager.cpp.
 *
 * This reproduces heap_manager.cpp's own already-verified Alloc()
 * algorithm via raw offset arithmetic directly on the `char *sInstance`
 * pointer, using the EXACT SAME confirmed field offsets oa_heapmanager.h
 * documents (+0x00/+0x04 active list head/tail, +0x08 active count,
 * +0x0c/+0x10 free list head/tail, +0x14 free count, +0x18 sentinel,
 * +0x28 live cursor (sentinel's repurposed "size" slot), +0x2c.. handle
 * table, +0x1e8498/+0x1e84a0/+0x1e84a8 heapBase/heapSize/reservedSize)
 * -- a mechanical byte-offset transliteration of the ALREADY
 * ground-truthed class body, not a fresh re-disassembly, so it is
 * guaranteed to stay behaviorally identical to it. Every field here is
 * accessed as a plain 4-byte `unsigned int`
 * (never a host `unsigned long`), so this version is actually MORE
 * faithful to the real 32-bit target's field widths than
 * heap_manager.cpp's own `unsigned long`-typed struct members are on
 * this 64-bit host (a pre-existing, unrelated, already-accepted
 * imperfection there -- not touched by this pass).
 *
 * Deliberately its OWN translation unit, not setup_global_resources.cpp
 * itself: test_setup_global_resources.cpp links setup_global_resources.cpp
 * directly and carries its own load-bearing call-counting mock of this
 * exact symbol (deterministic per-call slot assignment) -- giving the
 * real body a separate file keeps that test's mock untouched, matching
 * this project's established "dedicated TU" precedent.
 */

#include "oa_setup_global_resources.h"

#define HM_OFF_HEAP_BASE   0x1e8498ul
#define HM_OFF_HEAP_SIZE   0x1e84a0ul
/* CORRECTED (root-caused via direct disassembly of
 * CSTGHeapManager::Initialize/Alloc/SetReservedSize, .text+0x2e850/
 * 0x2e970/0x2e950): the real live bump-down cursor is NOT a field near
 * heapBase/heapSize -- it's the SENTINEL's own repurposed "size" slot,
 * at +0x28 (12 bytes into the sentinel's 20-byte CSTGHeapHandleEntry,
 * i.e. its next/prev/owner/OFFSET/SIZE layout with `size` reused as the
 * cursor). The old 0x1e84a4 offset is a real, DIFFERENT field
 * (SetLastFixedBlock()'s cached snapshot, never legitimately seeded by
 * this project's call graph) -- reading it here made
 * CSTGHeapManager::Alloc()'s capacity check fail unconditionally
 * regardless of real heap size, the root cause of the CSTGEngine ctor
 * crash chased in sec 10.219-10.221. See oa_heapmanager.h's own file
 * comment for the full three-site disassembly proof. */
#define HM_OFF_CURSOR      0x28ul
#define HM_OFF_RESERVED    0x1e84a8ul
#define HM_HANDLE_STRIDE   20ul

static inline unsigned int rd32(unsigned char *p, unsigned long off)
{
	return *(unsigned int *)(p + off);
}
static inline void wr32(unsigned char *p, unsigned long off, unsigned int v)
{
	*(unsigned int *)(p + off) = v;
}

unsigned int CSTGHeapManager::Alloc(unsigned int size)
{
	unsigned char *heap = (unsigned char *)sInstance;

	unsigned int freeListHead = rd32(heap, 0x0c);
	if (freeListHead == 0)
		return (unsigned int)-1;

	unsigned char *entry = (unsigned char *)(unsigned long)freeListHead;
	unsigned int freeListTail = rd32(heap, 0x10);

	/* Pop from the head of the free list (special-cases the
	 * single-element case, matching heap_manager.cpp's own Alloc()). */
	if (freeListHead == freeListTail)
		wr32(heap, 0x10, 0);

	unsigned int entryNext = rd32(entry, 0x0);
	unsigned int entryPrev = rd32(entry, 0x4);
	wr32(heap, 0x0c, entryNext);
	if (entryNext != 0)
		wr32((unsigned char *)(unsigned long)entryNext, 0x4, entryPrev);
	if (entryPrev != 0)
		wr32((unsigned char *)(unsigned long)entryPrev, 0x0, entryNext);
	wr32(entry, 0x0, 0);
	wr32(entry, 0x4, 0);
	wr32(entry, 0x8, 0);
	wr32(heap, 0x14, rd32(heap, 0x14) - 1);

	unsigned int cursor = rd32(heap, HM_OFF_CURSOR);
	unsigned int reserved = rd32(heap, HM_OFF_RESERVED);
	unsigned int available = cursor - reserved;
	if (size > available)
		return (unsigned int)-1;

	unsigned int newCursor = (cursor - size) & ~3u;
	wr32(heap, HM_OFF_CURSOR, newCursor);
	wr32(entry, 0xc, newCursor);
	wr32(entry, 0x10, size);

	/* Push onto the active list, right after the sentinel. */
	unsigned char *sentinel = heap + 0x18;
	unsigned int sentinelAddr = (unsigned int)(unsigned long)sentinel;
	wr32(entry, 0x4, sentinelAddr);
	unsigned int oldNext = rd32(sentinel, 0x0);
	wr32(entry, 0x0, oldNext);
	if (oldNext != 0)
		wr32((unsigned char *)(unsigned long)oldNext, 0x4, (unsigned int)(unsigned long)entry);
	wr32(sentinel, 0x0, (unsigned int)(unsigned long)entry);
	unsigned int activeListTail = rd32(heap, 0x04);
	if (sentinelAddr == activeListTail)
		wr32(heap, 0x04, (unsigned int)(unsigned long)entry);
	wr32(entry, 0x8, (unsigned int)(unsigned long)heap);
	wr32(heap, 0x08, rd32(heap, 0x08) + 1);

	/* Confirmed real handle-number formula: (entry - sentinel) / 20. */
	unsigned long diff = (unsigned long)entry - (unsigned long)sentinel;
	return (unsigned int)(diff / HM_HANDLE_STRIDE);
}
