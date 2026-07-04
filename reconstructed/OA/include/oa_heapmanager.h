// SPDX-License-Identifier: GPL-2.0
/*
 * oa_heapmanager.h  -  CSTGHeapManager: the real handle-based allocator
 * layered on top of the ioremap'd MMIO region InitializeSTGHeap()
 * (oa_stgheap_init.h) sets up.
 *
 * Ground-truthed via readelf+objdump (`-j .text`) against OA_real.ko:
 *   CSTGHeapManager::Initialize(unsigned long,unsigned long) .text+0x2e850, 213B
 *   CSTGHeapManager::Alloc(unsigned long)                    .text+0x2e970, 220B
 *   CSTGHeapManager_Initialize (C-linkage wrapper)            .text+0x2ee30, 118B
 *   CSTGHeapManager_GetHeapSize (C-linkage wrapper)           .text+0x2eec0,  12B
 *
 * Declared in its OWN header, separate from oa_types.h's/
 * oa_setup_global_resources.h's own minimal `CSTGHeapManager` stand-ins
 * -- this project's established ODR-avoidance pattern (each TU declares
 * a locally-sufficient, mangled-name-matching stand-in rather than
 * sharing one conflicting definition). `sInstance`'s mangled symbol
 * (`_ZN15CSTGHeapManager9sInstanceE`) is only DEFINED once, in
 * heap_manager.cpp; every other TU's `static char *sInstance;` (or this
 * header's own richer type) links against that same 4-byte pointer
 * regardless of the local C++ type used to declare it.
 *
 * CONFIRMED layout (this pass's own direct disassembly trace):
 *   +0x00/+0x04  active-list head/tail (doubly linked; head is set once
 *                to `&sentinel` and never changes again -- confirmed,
 *                every subsequent insertion threads through the
 *                sentinel's OWN next/prev instead)
 *   +0x08        active block count
 *   +0x0c/+0x10  free-handle-list head/tail (doubly linked)
 *   +0x14        free handle count
 *   +0x18        sentinel node -- ONLY its next/prev/owner fields
 *                (12 bytes, +0x18/+0x1c/+0x20) are ever touched; the
 *                handle table below starts immediately after those 12
 *                bytes, at +0x24, with NO gap
 *   +0x24..      99999 x 20-byte free-list entries (see
 *                CSTGHeapHandleEntry below), threaded onto the free
 *                list by Initialize()
 *   +0x1e8498    heapBase (confirmed real, arg1 of Initialize; also
 *                independently confirmed by oa_heap.h's own
 *                oa_heap_base()/oa_heap_region() accessors, sec 10.48)
 *   +0x1e84a0    heapSize (arg2 of Initialize)
 *   +0x1e84a8    reservedSize (SetReservedSize()'s target -- not
 *                reconstructed in this pass; always 0 for a fresh
 *                object, matching Alloc()'s own confirmed
 *                "available = cursor - reservedSize" check)
 *
 * Each CSTGHeapHandleEntry's own fields (confirmed via Alloc()'s
 * disassembly): +0x0/+0x4/+0x8 are the free-list next/prev and an
 * "owner" backpointer while on the free list; ONCE ALLOCATED, +0x0/+0x4
 * are repurposed as the ACTIVE list's own next/prev (re-threaded onto
 * the sentinel), +0x8 becomes the manager backpointer again, +0xc =
 * this allocation's byte OFFSET within the heap (a bump-DOWN cursor:
 * new_offset = align4(old_cursor - size)), +0x10 = the requested size.
 *
 * RESOLVED (sec 10.63, was flagged as an open discrepancy in sec
 * 10.60): oa_heap.h's own oa_heap_region(slot) computes
 * `heap + 0x24 + slot*0x14` and reads THAT address as the region's
 * offset-from-heapBase. This IS consistent with this class's own
 * layout, once slot 0 is understood to mean THE SENTINEL ITSELF (not
 * "the first real handle entry", which was sec 10.60's mistaken
 * assumption): entry(slot) = sentinel_addr + slot*20 =
 * (heap+0x18) + slot*20, and oa_heap_region reads that entry's own
 * +0xc "offset" field: (heap+0x18+slot*20)+0xc = heap+0x24+slot*20 --
 * an EXACT match. Independently reconfirmed from a completely
 * different call site (CSTGMidiDispatcher::Initialize(), sec 10.63),
 * which computes the identical `heap+0x18+slot*0x14` address directly
 * and reads its own +0xc field the same way. This also matches
 * Alloc()'s own handle-number formula: `(entry_addr - sentinel_addr) /
 * 20` naturally gives 0 for the sentinel itself and N for the N-th
 * real handle past it -- no discrepancy, no papering over needed.
 */

#ifndef OA_HEAPMANAGER_H
#define OA_HEAPMANAGER_H

struct CSTGHeapHandleEntry {
	unsigned int next;	/* +0x0 */
	unsigned int prev;	/* +0x4 */
	unsigned int owner;	/* +0x8 */
	unsigned int offset;	/* +0xc, valid once allocated */
	unsigned int size;	/* +0x10, valid once allocated */
};

#define CSTG_HEAPMANAGER_HANDLE_COUNT 99999

class CSTGHeapManager {
public:
	static CSTGHeapManager *sInstance;

	void Initialize(unsigned long base, unsigned long size);
	unsigned int Alloc(unsigned long size);

	unsigned int activeListHead;	/* +0x00 */
	unsigned int activeListTail;	/* +0x04 */
	unsigned int activeCount;	/* +0x08 */
	unsigned int freeListHead;	/* +0x0c */
	unsigned int freeListTail;	/* +0x10 */
	unsigned int freeCount;	/* +0x14 */

	/* Sentinel: only next/prev/owner are real; kept as a full
	 * CSTGHeapHandleEntry purely so `&sentinel` arithmetic matches the
	 * real disassembly's own `lea eax,[this+0x18]` -- its own
	 * offset/size fields are never touched by the real code and are
	 * NOT part of the handle table (the table starts right after, at
	 * +0x24, overlapping where a full 20-byte entry would end -- see
	 * this header's own file comment). */
	CSTGHeapHandleEntry sentinel;			/* +0x18 */
	CSTGHeapHandleEntry handles[CSTG_HEAPMANAGER_HANDLE_COUNT]; /* +0x24 */
	unsigned long heapBase;			/* +0x1e8498 */
	unsigned long heapSize;			/* +0x1e84a0 */
	unsigned long cursor;				/* running bump-down offset */
	unsigned long reservedSize;			/* +0x1e84a8, always 0 here */
};

extern "C" {

/* C-linkage wrappers (confirmed via relocation to be plain unmangled
 * symbols, NOT CSTGHeapManager methods -- see oa_stgheap_init.h's own
 * note on this same naming/class-membership caveat). */
unsigned long CSTGHeapManager_Initialize(unsigned long base, unsigned long size);
unsigned long CSTGHeapManager_GetHeapSize(void);

} /* extern "C" */

#endif /* OA_HEAPMANAGER_H */
