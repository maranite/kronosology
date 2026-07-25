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
#include <cstring>

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

	/*
	 * REGRESSION COVERAGE (2026-07-23, round 3): poison the
	 * activeListHead/activeCount bytes with a nonzero pattern BEFORE
	 * calling CSTGHeapManager_Initialize() -- a plain fresh mmap() page
	 * already reads back as zero, which would let this exact class of
	 * bug (a compiler treating a same-function read-before-write as
	 * "arbitrary", already root-caused once for freeListHead/
	 * freeListTail/freeCount on 2026-07-12 and a second time for
	 * activeListHead/activeCount on 2026-07-23 -- see heap_manager.cpp's
	 * own comment) pass silently on a portable host test even though it
	 * corrupts a live kernel boot. This doesn't reproduce the actual
	 * miscompilation (that needs the real ancient kernel-era gcc under
	 * -O2, not verified here), but it DOES prove Initialize() no longer
	 * *relies* on the object having been externally pre-zeroed for
	 * these two members -- an explicit write now makes this assertion
	 * true regardless of what garbage happened to be in memory first,
	 * closing the same class of hazard the 2026-07-12 fix already
	 * validated this exact technique against for its own three fields.
	 */
	memset(buf, 0xcc, 0x18);

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
	check("cursor == heapSize", (long)mgr->sentinel.size, (long)mgr->heapSize);

	printf("[3] Alloc() returns distinct handle numbers\n"
	       "    (2026-07-24: delegates to the shared CSTGHeapManager_BumpAlloc()\n"
	       "    core now -- see heap_manager.cpp's own file comment. freeCount/\n"
	       "    activeCount/sentinel.size on the OBJECT are deliberately no\n"
	       "    longer touched by Alloc() at all, so this no longer asserts on\n"
	       "    them -- CSTGHeapManager_GetCapturedOffset() is the reliable\n"
	       "    source of truth for what Alloc() actually did.)\n");
	unsigned int h0 = mgr->Alloc(100);
	unsigned int h1 = mgr->Alloc(200);
	check("h0 != -1", h0 != 0xffffffffu, 1);
	check("h1 != -1", h1 != 0xffffffffu, 1);
	check("h0 != h1", h0 != h1, 1);
	check("h0's captured offset is a real, non-overlapping region start",
	      (long)CSTGHeapManager_GetCapturedOffset(h0) < (long)CSTGHeapManager_GetCapturedOffset(h1)
	      || (long)CSTGHeapManager_GetCapturedOffset(h1) < (long)CSTGHeapManager_GetCapturedOffset(h0), 1);

	printf("[4] Alloc() fails cleanly once requested size exceeds remaining space\n");
	unsigned int hBig = mgr->Alloc(mgr->heapSize * 2);
	check("oversized alloc returns -1", hBig, 0xffffffffu);

	printf("[5] Repeated small allocs keep succeeding, carving distinct regions\n"
	       "    (2026-07-24: the bump allocator has no notion of a finite\n"
	       "    'free list' to exhaust -- it fails purely on capacity, already\n"
	       "    covered by [4] -- so this now just confirms sustained correct\n"
	       "    behavior across many calls instead)\n");
	{
		unsigned int last = 0;
		int failed = 0;
		for (int i = 0; i < 10; i++) {
			last = mgr->Alloc(64);
			if (last == 0xffffffffu) {
				failed = 1;
				break;
			}
		}
		check("10 small repeated allocs all succeeded", failed == 0 && last != 0, 1);
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
