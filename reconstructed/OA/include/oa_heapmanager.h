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
 *   +0x18        sentinel node, declared as a FULL 20-byte
 *                CSTGHeapHandleEntry (next/prev/owner at +0x18/+0x1c/
 *                +0x20 used normally for list-threading; its own
 *                "offset"/"size" fields at +0x24/+0x28 are REPURPOSED,
 *                see below -- NOT unused padding as an earlier pass of
 *                this header claimed)
 *   +0x24        sentinel's "offset" slot: always zeroed by
 *                Initialize() (`movl $0,0x24(%eax)`, .text+0x2e895) and
 *                never read anywhere else -- genuinely unused, but part
 *                of the real, fully-20-byte sentinel struct.
 *   +0x28        **the real live bump-down cursor** (sentinel's "size"
 *                slot, repurposed). CONFIRMED via THREE independent
 *                direct-disassembly sites: CSTGHeapManager::Initialize
 *                (.text+0x2e850) seeds it to heapSize
 *                (`mov 0x1e84a0(%eax),%edx; ... mov %edx,0x28(%eax)`),
 *                CSTGHeapManager::Alloc (.text+0x2e970) both reads it
 *                for the capacity check (`mov 0x28(%eax),%esi; ...
 *                sub 0x1e84a8(%eax),%edi` i.e. `available = cursor -
 *                reservedSize`) and writes the decremented value back
 *                (`mov %ecx,0x28(%eax)`), and
 *                CSTGHeapManager::SetReservedSize (.text+0x2e950) reads
 *                it as the ceiling a new reservedSize may not exceed
 *                (`mov 0x28(%eax),%ecx; cmp %ecx,%edx; ja <fail>`).
 *                GetHeapFreeSize (.text+0x2eed0, not reconstructed)
 *                also returns this same field, consistent with it
 *                being "remaining capacity". THIS IS THE FIELD A PRIOR
 *                RECONSTRUCTION PASS MISPLACED at +0x1e84a4 (see
 *                heap_manager.cpp/heap_manager_alloc_static.cpp
 *                history) -- that offset holds a DIFFERENT field, see
 *                below.
 *   +0x2c..      99999 x 20-byte free-list entries (see
 *                CSTGHeapHandleEntry below), threaded onto the free
 *                list by Initialize(). (CORRECTED: an earlier pass of
 *                this header claimed the table starts at +0x24 "with
 *                no gap" -- that was wrong; the sentinel genuinely
 *                consumes the full 20 bytes, +0x18..+0x2c, and the
 *                real handle table starts right after, at +0x2c. Proof:
 *                Initialize()'s own free-list-threading loop sets
 *                freeListHead to `this+0x2c` for the very first
 *                threaded entry [.text+0x2e8da/0x2e918], and this is
 *                the ONLY layout under which the class's own already-
 *                confirmed heapBase offset [+0x1e8498] is arithmetically
 *                consistent with 99999 x 20-byte entries following the
 *                sentinel: 0x2c + 99999*0x14 = 0x1e8498 exactly. Under
 *                the old "+0x24" claim the table would end 8 bytes
 *                short of the confirmed heapBase offset.)
 *   +0x1e8498    heapBase (confirmed real, arg1 of Initialize; also
 *                independently confirmed by oa_heap.h's own
 *                oa_heap_base()/oa_heap_region() accessors, sec 10.48)
 *   +0x1e84a0    heapSize (arg2 of Initialize)
 *   +0x1e84a4    lastFixedBlockCursor -- NOT the live cursor (see +0x28
 *                above). A cached snapshot only written by
 *                SetLastFixedBlock() (.text+0x2e930, not reconstructed:
 *                `mov 0x28(%eax),%edx; mov %edx,0x1e84a4(%eax)` --
 *                literally copies the live +0x28 cursor here at the
 *                moment a block is marked "fixed"). A prior
 *                reconstruction pass mistook THIS field for the live
 *                cursor and had Initialize()/Alloc() read/write it
 *                instead of +0x28 -- since nothing in this project's
 *                call graph exercises SetLastFixedBlock(), that
 *                mistaken field silently stayed at/near zero, making
 *                every CSTGHeapManager::Alloc() capacity check fail
 *                regardless of real heap size (root cause of the
 *                CSTGEngine ctor crash chased in sec 10.219-10.221).
 *   +0x1e84a8    reservedSize (SetReservedSize()'s target, confirmed
 *                via .text+0x2e950/0x2e961; always 0 for a fresh
 *                object, matching Alloc()'s own confirmed
 *                "available = cursor - reservedSize" check)
 *   +0x1e84ac    lastFixedBlock -- pointer to the active-list entry
 *                most recently marked "fixed" (SetLastFixedBlock,
 *                .text+0x2e930/0x2e939; 0 until first use). Not
 *                reconstructed/used by this project's call graph.
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
 * 10.60; offsets below updated to the corrected +0x2c handle-table
 * base derived above -- the ADDRESS ARITHMETIC was always right,
 * only the "table starts at +0x24" prose framing was wrong): the
 * region-resolution formula used throughout this project (see
 * oa_setup_global_resources.h's local_heap_region()) computes
 * `heap + 0x24 + slot*0x14` and reads THAT address as the region's
 * offset-from-heapBase. This IS consistent with this class's own
 * layout, once slot 0 is understood to mean THE SENTINEL ITSELF:
 * entry(slot) = sentinel_addr + slot*20 = (heap+0x18) + slot*20 (slot
 * 1 = the first real handle, physically at heap+0x2c, matching the
 * corrected table-base above), and the region resolver reads that
 * entry's own +0xc "offset" field: (heap+0x18+slot*20)+0xc =
 * heap+0x24+slot*20 -- an EXACT match, independently reconfirmed from
 * a completely different call site (CSTGMidiDispatcher::Initialize(),
 * sec 10.63) and again directly against setup_global_resources'
 * panelSlot-resolution disassembly (.text+0x116cbe/0x116cc6). This
 * also matches Alloc()'s own handle-number formula: `(entry_addr -
 * sentinel_addr) / 20` naturally gives 0 for the sentinel itself and N
 * for the N-th real handle past it -- no discrepancy, no papering over
 * needed. NOTE: the real disassembly's own "is this slot valid" guard
 * (.text+0x116cc2, `test %ecx,%ecx` on the LEA'd entry ADDRESS) never
 * dereferences memory -- it is a pointer-nullity check that is
 * effectively always true once `heap` itself is non-null. A prior
 * reconstruction pass turned this into a dereferencing read
 * (`*(rec+0x18)`), which is wrong (and, for slot 1 specifically, would
 * spuriously return 0 on the very first-ever allocation, whose
 * `entry.next` legitimately reads 0 right after insertion) -- fixed in
 * setup_global_resources.cpp's local_heap_region() to match the real,
 * non-dereferencing check.
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

	/* Sentinel: a genuine full 20-byte CSTGHeapHandleEntry (matches the
	 * real disassembly's own `lea eax,[this+0x18]` for `&sentinel`
	 * arithmetic). next/prev/owner (+0x18/+0x1c/+0x20) are the normal
	 * active-list link fields; its own offset/size fields
	 * (+0x24/+0x28) are REPURPOSED by the real code rather than left
	 * unused -- offset(+0x24) is always zeroed and otherwise untouched,
	 * but size(+0x28) is the real, live bump-down cursor (see this
	 * header's own file comment for the full disassembly-confirmed
	 * evidence). The handle table proper starts immediately after this
	 * full struct, at +0x2c, with NO gap. */
	CSTGHeapHandleEntry sentinel;			/* +0x18 */
	CSTGHeapHandleEntry handles[CSTG_HEAPMANAGER_HANDLE_COUNT]; /* +0x2c */
	unsigned long heapBase;			/* +0x1e8498 */
	/*
	 * CONFIRMED real 4-byte gap (live-boot-diagnosed 2026-07-12): the
	 * real disassembly of CSTGHeapManager::Initialize itself writes
	 * heapBase to +0x1e8498 (`mov %edx,0x1e8498(%eax)`) and heapSize to
	 * +0x1e84a0 (`mov %ecx,0x1e84a0(%eax)`) -- 8 bytes apart, not 4.
	 * Declaring heapBase/heapSize back-to-back (as this struct did
	 * before this fix) compiles heapSize to +0x1e849c instead, silently
	 * misaligning it and every field after it (lastFixedBlockCursor/
	 * reservedSize/lastFixedBlock) by 4 bytes relative to the hardcoded
	 * offset constants used elsewhere (heap_manager_alloc_static.cpp's
	 * HM_OFF_*). This is what made a live kernel boot's
	 * CSTGHeapManager_Initialize() write heapBase/heapSize/cursor
	 * correctly (self-consistently, via the C++ field names) while
	 * heap_manager_alloc_static.cpp's raw-offset Alloc() read back all
	 * zeros for every one of them -- the two TUs were talking past each
	 * other by one dword. Purpose of the real field living in this gap
	 * is not yet identified (no reconstructed method reads/writes it);
	 * kept as an explicit placeholder so subsequent fields land at
	 * their confirmed real offsets.
	 */
	unsigned long unknown_1e849c;
	unsigned long heapSize;			/* +0x1e84a0 */
	unsigned long lastFixedBlockCursor;		/* +0x1e84a4, SetLastFixedBlock()'s
							 * cached snapshot -- NOT the live
							 * cursor (that's sentinel.size,
							 * +0x28). Not written by any path
							 * this project's call graph
							 * exercises; kept only so
							 * reservedSize below lands at its
							 * confirmed real offset. */
	unsigned long reservedSize;			/* +0x1e84a8, always 0 here */
	unsigned long lastFixedBlock;			/* +0x1e84ac, SetLastFixedBlock()'s
							 * target pointer; unused here. */
};

extern "C" {

/* C-linkage wrappers (confirmed via relocation to be plain unmangled
 * symbols, NOT CSTGHeapManager methods -- see oa_stgheap_init.h's own
 * note on this same naming/class-membership caveat). */
unsigned long CSTGHeapManager_Initialize(unsigned long base, unsigned long size);
unsigned long CSTGHeapManager_GetHeapSize(void);

} /* extern "C" */

#endif /* OA_HEAPMANAGER_H */
