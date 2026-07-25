// SPDX-License-Identifier: GPL-2.0
/*
 * test_heap_manager_alloc_static.cpp  -  KAT for
 * src/mem/heap_manager_alloc_static.cpp: CSTGHeapManager::Alloc(unsigned int),
 * this project's own local "static" ecosystem stand-in for the real instance
 * method CSTGHeapManager::Alloc(unsigned long).
 *
 * REWRITTEN (2026-07-24): this file now just delegates to the shared
 * `CSTGHeapManager_BumpAlloc()` core (defined in heap_manager.cpp, whose own
 * dedicated test -- test_heap_manager.cpp -- exercises the bump-allocator
 * logic itself in full). This test only exercises what's specific to THIS
 * file: the `-44` "heap not up yet" sentinel guard, and that the size
 * parameter is correctly forwarded to the shared core.
 */

#include "oa_setup_global_resources.h"
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

char *CSTGHeapManager::sInstance;

static unsigned int g_lastBumpAllocSize;
static unsigned int g_bumpAllocCallCount;
static unsigned int g_mockReturnSlot = 42;
extern "C" unsigned int CSTGHeapManager_BumpAlloc(unsigned int size)
{
	g_lastBumpAllocSize = size;
	g_bumpAllocCallCount++;
	return g_mockReturnSlot;
}

int main(void)
{
	unsigned char dummyHeapObj[4];

	printf("[1] Heap not up yet (-44 sentinel) -> Alloc() fails immediately,\n"
	       "    never reaches the shared bump-allocator core\n");
	CSTGHeapManager::sInstance = (char *)(long)-44;
	unsigned int h0 = CSTGHeapManager::Alloc(12345);
	check("Alloc() on unresolved heap returns -1", h0, 0xffffffffu);
	check("CSTGHeapManager_BumpAlloc NOT called", (long)g_bumpAllocCallCount, 0);

	printf("\n[2] Heap up -> Alloc() delegates to the shared bump-allocator core,\n"
	       "    forwarding the exact size and returning its result unchanged\n");
	CSTGHeapManager::sInstance = (char *)dummyHeapObj;
	unsigned int h1 = CSTGHeapManager::Alloc(0x294fc);
	check("CSTGHeapManager_BumpAlloc called exactly once", (long)g_bumpAllocCallCount, 1);
	check("size forwarded unchanged", (long)g_lastBumpAllocSize, 0x294fc);
	check("Alloc() returns the core's result unchanged", (long)h1, (long)g_mockReturnSlot);

	printf("\n[3] A second call delegates again (no hidden per-call state in this file)\n");
	g_mockReturnSlot = 43;
	unsigned int h2 = CSTGHeapManager::Alloc(0x1000);
	check("CSTGHeapManager_BumpAlloc called again", (long)g_bumpAllocCallCount, 2);
	check("size forwarded unchanged", (long)g_lastBumpAllocSize, 0x1000);
	check("Alloc() returns the core's (new) result unchanged", (long)h2, (long)g_mockReturnSlot);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
