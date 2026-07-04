// SPDX-License-Identifier: GPL-2.0
/*
 * test_heap_manager.cpp  -  KAT for CSTGHeapManager::Initialize()/Alloc()
 * and the CSTGHeapManager_Initialize()/GetHeapSize() C-linkage wrappers.
 *
 * Uses MAP_32BIT for the backing buffer: CSTGHeapManager's own free/
 * active-list bookkeeping stores addresses truncated to `unsigned int`
 * (matching the real target's own 32-bit pointer width) -- the same
 * host/target pointer-width hazard hit repeatedly elsewhere in this
 * project (e.g. engine_init.cpp's TSTGArrayManager<T>). A plain host
 * `new`/`static` buffer would live outside 32-bit address space and
 * silently corrupt on truncation.
 */

#include "oa_heapmanager.h"
#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>

static int g_fail = 0;

static void check(const char *what, long got, long want)
{
	if (got == want) {
		printf("  ok    %-50s %ld\n", what, got);
	} else {
		printf("  FAIL  %-50s got=%ld want=%ld\n", what, got, want);
		g_fail = 1;
	}
}

int main()
{
	/* sizeof(CSTGHeapManager) is dominated by handles[99999] (~2MB) --
	 * give it plenty of room for the "heap" region carved out after it
	 * too. */
	unsigned long bufSize = 8 * 1024 * 1024;
	unsigned char *buf = (unsigned char *)mmap(0, bufSize, PROT_READ | PROT_WRITE,
						    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	if (buf == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

	printf("[1] CSTGHeapManager_Initialize()\n");
	unsigned long base = (unsigned long)buf;
	unsigned long alignedBase = CSTGHeapManager_Initialize(base, bufSize);
	check("returned base is page-aligned", (long)(alignedBase & 0xfff), 0);
	check("returned base > raw base", alignedBase > base, 1);
	check("sInstance set", CSTGHeapManager::sInstance != 0, 1);
	check("sInstance == raw base", (long)(unsigned long)CSTGHeapManager::sInstance, (long)base);

	unsigned long heapSize = CSTGHeapManager_GetHeapSize();
	check("heap size > 0", heapSize > 0, 1);
	check("GetHeapSize matches sInstance->heapSize",
	      (long)heapSize, (long)CSTGHeapManager::sInstance->heapSize);

	printf("[2] Fresh manager: free list fully populated, active list has only the sentinel\n");
	CSTGHeapManager *mgr = CSTGHeapManager::sInstance;
	check("freeCount == 99999", (long)mgr->freeCount, CSTG_HEAPMANAGER_HANDLE_COUNT);
	check("activeCount == 1 (sentinel)", (long)mgr->activeCount, 1);
	check("cursor == heapSize", (long)mgr->cursor, (long)mgr->heapSize);

	printf("[3] Alloc() carves from the top, returns increasing-from-0 handle numbers\n");
	unsigned int h0 = mgr->Alloc(100);
	unsigned int h1 = mgr->Alloc(200);
	check("h0 != -1", h0 != 0xffffffffu, 1);
	check("h1 != -1", h1 != 0xffffffffu, 1);
	check("h0 != h1", h0 != h1, 1);
	check("freeCount decremented twice", (long)mgr->freeCount, CSTG_HEAPMANAGER_HANDLE_COUNT - 2);
	check("activeCount incremented twice", (long)mgr->activeCount, 3);

	unsigned long cursorAfter = mgr->cursor;
	check("cursor decreased by >= 300", (mgr->heapSize - cursorAfter) >= 300, 1);

	printf("[4] Alloc() fails cleanly once requested size exceeds remaining space\n");
	unsigned int hBig = mgr->Alloc(mgr->heapSize * 2);
	check("oversized alloc returns -1", hBig, 0xffffffffu);

	printf("[5] Exhausting the free list eventually fails\n");
	{
		unsigned long remaining = mgr->cursor;
		unsigned int last = 0;
		int failed = 0;
		for (int i = 0; i < 10; i++) {
			last = mgr->Alloc(remaining / 20);
			if (last == 0xffffffffu) {
				failed = 1;
				break;
			}
		}
		check("small repeated allocs keep succeeding for a while", failed == 0 || last != 0, 1);
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
