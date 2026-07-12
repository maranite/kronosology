// SPDX-License-Identifier: GPL-2.0
/*
 * test_heap_manager_alloc_static.cpp  -  KAT for batch 17's
 * src/mem/heap_manager_alloc_static.cpp: CSTGHeapManager::
 * Alloc(unsigned int), this project's own local "static" ecosystem
 * stand-in for the real instance method CSTGHeapManager::
 * Alloc(unsigned long) (already independently verified in
 * test_heap_manager.cpp).
 *
 * Exercises the SAME free-list-pop / cursor-bump-down / active-list-push
 * algorithm, but hand-threads a small 2-3 entry free list directly
 * rather than replaying the real 99999-entry Initialize() loop -- this
 * ecosystem (oa_setup_global_resources.h's own local minimal
 * CSTGHeapManager stand-in) has no Initialize() of its own.
 *
 * MAP_32BIT-backed, same host/target pointer-width reasoning as
 * test_heap_manager.cpp: every address stored through a 32-bit field
 * must itself fit in 32 bits.
 */

#include "oa_setup_global_resources.h"
#include <sys/mman.h>
#include <cstdio>

static int g_fail;
static void check(const char *what, long got, long want)
{
	if (got == want) {
		printf("  ok    %-55s %ld\n", what, got);
	} else {
		printf("  FAIL  %-55s got=%ld want=%ld\n", what, got, want);
		g_fail = 1;
	}
}

static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

#define OFF_ACTIVE_HEAD 0x00ul
#define OFF_ACTIVE_TAIL 0x04ul
#define OFF_ACTIVE_CNT  0x08ul
#define OFF_FREE_HEAD   0x0cul
#define OFF_FREE_TAIL   0x10ul
#define OFF_FREE_CNT    0x14ul
#define OFF_SENTINEL    0x18ul
#define OFF_HEAP_SIZE   0x1e84a0ul
/* CORRECTED: the real live cursor is the sentinel's own repurposed
 * "size" slot at +0x28 (sentinel base +0x18, +0x10 within the entry),
 * not +0x1e84a4 -- see oa_heapmanager.h's file comment and
 * heap_manager_alloc_static.cpp's own HM_OFF_CURSOR for the full
 * disassembly-confirmed evidence. */
#define OFF_CURSOR      0x28ul
#define OFF_RESERVED    0x1e84a8ul

static void wr32(unsigned char *p, unsigned long off, unsigned int v) { *(unsigned int *)(p + off) = v; }
static unsigned int rd32(unsigned char *p, unsigned long off) { return *(unsigned int *)(p + off); }
static unsigned int addr32(void *p) { return (unsigned int)(unsigned long)p; }

char *CSTGHeapManager::sInstance;

int main(void)
{
	unsigned long bufSize = 0x1e8600;
	unsigned char *heap = mmap32(bufSize);
	if (heap == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	CSTGHeapManager::sInstance = (char *)heap;

	wr32(heap, OFF_ACTIVE_HEAD, addr32(heap + OFF_SENTINEL));
	wr32(heap, OFF_ACTIVE_TAIL, addr32(heap + OFF_SENTINEL));
	wr32(heap, OFF_ACTIVE_CNT, 1);
	wr32(heap, OFF_HEAP_SIZE, 0x10000);
	wr32(heap, OFF_CURSOR, 0x10000);
	wr32(heap, OFF_RESERVED, 0);

	/* Hand-thread a 2-entry free list at arbitrary offsets well clear of
	 * the sentinel. */
	unsigned char *e0 = heap + 0x200;
	unsigned char *e1 = heap + 0x220;
	wr32(heap, OFF_FREE_HEAD, addr32(e0));
	wr32(heap, OFF_FREE_TAIL, addr32(e1));
	wr32(e0, 0x0, addr32(e1));
	wr32(e0, 0x4, 0);
	wr32(e1, 0x0, 0);
	wr32(e1, 0x4, addr32(e0));
	wr32(heap, OFF_FREE_CNT, 2);

	printf("[1] Alloc() pops the free list head, decrements cursor, pushes onto active list\n");
	unsigned int h0 = CSTGHeapManager::Alloc(100);
	check("h0 != -1", h0 != 0xffffffffu, 1);
	check("freeCount now 1", (long)rd32(heap, OFF_FREE_CNT), 1);
	check("freeHead now e1", (long)rd32(heap, OFF_FREE_HEAD), (long)addr32(e1));
	check("activeCount now 2", (long)rd32(heap, OFF_ACTIVE_CNT), 2);
	unsigned int cursorAfter1 = rd32(heap, OFF_CURSOR);
	check("cursor decreased by >= 100", (0x10000 - cursorAfter1) >= 100, 1);
	check("e0.offset (+0xc) == new cursor", (long)rd32(e0, 0xc), (long)cursorAfter1);
	check("e0.size (+0x10) == 100", (long)rd32(e0, 0x10), 100);
	check("sentinel.next == e0", (long)rd32(heap + OFF_SENTINEL, 0x0), (long)addr32(e0));

	printf("\n[2] Second Alloc() pops the last remaining free entry\n");
	unsigned int h1 = CSTGHeapManager::Alloc(200);
	check("h1 != -1", h1 != 0xffffffffu, 1);
	check("h0 != h1", h0 != h1, 1);
	check("freeCount now 0", (long)rd32(heap, OFF_FREE_CNT), 0);
	check("freeHead now 0", (long)rd32(heap, OFF_FREE_HEAD), 0);
	check("freeTail now 0", (long)rd32(heap, OFF_FREE_TAIL), 0);
	check("activeCount now 3", (long)rd32(heap, OFF_ACTIVE_CNT), 3);

	printf("\n[3] Free list exhausted -> Alloc() returns -1\n");
	unsigned int h2 = CSTGHeapManager::Alloc(1);
	check("h2 == -1", h2, 0xffffffffu);

	printf("\n[4] Oversized request: entry IS popped (confirmed real quirk -- pop happens\n"
	       "    BEFORE the size check), but alloc still fails and nothing joins the active list\n");
	unsigned char *e2 = heap + 0x240;
	wr32(heap, OFF_FREE_HEAD, addr32(e2));
	wr32(heap, OFF_FREE_TAIL, addr32(e2));
	wr32(e2, 0x0, 0);
	wr32(e2, 0x4, 0);
	wr32(heap, OFF_FREE_CNT, 1);
	unsigned int activeCountBefore = rd32(heap, OFF_ACTIVE_CNT);
	unsigned int remaining = rd32(heap, OFF_CURSOR) - rd32(heap, OFF_RESERVED);
	unsigned int h3 = CSTGHeapManager::Alloc(remaining + 1);
	check("oversized alloc returns -1", h3, 0xffffffffu);
	check("free entry WAS consumed even though alloc failed", (long)rd32(heap, OFF_FREE_CNT), 0);
	check("activeCount unchanged (never reached the active-list push)",
	      (long)rd32(heap, OFF_ACTIVE_CNT), (long)activeCountBefore);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
