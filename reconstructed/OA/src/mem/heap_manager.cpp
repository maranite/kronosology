// SPDX-License-Identifier: GPL-2.0
/*
 * heap_manager.cpp  -  CSTGHeapManager::Initialize()/Alloc() and their
 * C-linkage wrappers. See oa_heapmanager.h for the full ground-truthing
 * detail and the open sentinel/slot-offset discrepancy note.
 */

#include "oa_heapmanager.h"

CSTGHeapManager *CSTGHeapManager::sInstance;

void CSTGHeapManager::Initialize(unsigned long base, unsigned long size)
{
	heapBase = base;
	heapSize = size;

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
	cursor = heapSize;

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
}

unsigned int CSTGHeapManager::Alloc(unsigned long size)
{
	if (freeListHead == 0)
		return (unsigned int)-1;

	CSTGHeapHandleEntry *entry =
		(CSTGHeapHandleEntry *)(unsigned long)freeListHead;

	/* Pop from the head of the free list -- special-cases the
	 * single-element case (freeListTail must also be cleared). */
	if (freeListHead == freeListTail) {
		freeListTail = 0;
	}
	freeListHead = entry->next;
	if (entry->next != 0)
		((CSTGHeapHandleEntry *)(unsigned long)entry->next)->prev = entry->prev;
	if (entry->prev != 0)
		((CSTGHeapHandleEntry *)(unsigned long)entry->prev)->next = entry->next;
	entry->next = 0;
	entry->prev = 0;
	entry->owner = 0;
	freeCount--;

	unsigned long available = cursor - reservedSize;
	if (size > available)
		return (unsigned int)-1;

	unsigned long newCursor = (cursor - size) & ~3ul;
	cursor = newCursor;
	entry->offset = (unsigned int)newCursor;
	entry->size = (unsigned int)size;

	/* Push onto the active list, right after the sentinel (a classic
	 * sentinel-anchored insert-at-front). */
	unsigned int sentinelAddr = (unsigned int)(unsigned long)&sentinel;
	entry->prev = sentinelAddr;
	unsigned int oldNext = sentinel.next;
	entry->next = oldNext;
	if (oldNext != 0)
		((CSTGHeapHandleEntry *)(unsigned long)oldNext)->prev =
			(unsigned int)(unsigned long)entry;
	sentinel.next = (unsigned int)(unsigned long)entry;
	if (sentinelAddr == activeListTail)
		activeListTail = (unsigned int)(unsigned long)entry;
	entry->owner = (unsigned int)(unsigned long)this;
	activeCount++;

	/* Confirmed real handle-number formula: (entry - sentinel) / 20,
	 * computed here directly rather than replicating the target's own
	 * reciprocal-multiplication /20 optimization. */
	unsigned long diff = (unsigned long)entry - (unsigned long)&sentinel;
	return (unsigned int)(diff / sizeof(CSTGHeapHandleEntry));
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
