/*
 * test_pool.cpp  -  host-side known-answer test for CPool (pool.h/.cpp) and the
 * TVector<T,N> template it depends on (tvector.h), reconstructed 2026-07-27
 * (CPool/CSlotPool follow-up to the same-day fresh broad-survey pass, see pool.h's own
 * file header).
 *
 * Exercises: ctor layout (mElementSize/mTotalBytes/initial chunk sizing), Alloc()/Free()
 * round-tripping through the free list, Alloc() growing a brand new chunk (mTotalBytes
 * doubling) once the initial chunk is exhausted, element isolation (writes to one
 * element never bleed into a neighbor), Expand() as a standalone utility, and
 * PostKernelDestructor() freeing every chunk without crashing.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "pool.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main(void)
{
	printf("CPool known-answer test\n");
	printf("========================\n");

	printf("[1] Real object size is 0x24 (36) bytes\n");
	check("sizeof(CPool) == 0x24", sizeof(CPool) == 0x24);

	/* elemSize=200 -> mElementSize = align4(200)+8 = 208; perKPage = 1024/208+1 = 5;
	 * initialCount=1 < perKPage, so the real first chunk holds 5 elements (1040 bytes)
	 * -- small enough to force a real chunk-growth event within a handful of Alloc()s.
	 */
	static unsigned char raw[sizeof(CPool)];
	CPool *pool = new (raw) CPool(200, 1);

	printf("[2] Alloc()/Free() round-trips through the free list (same pointer reused)\n");
	void *a = pool->Alloc();
	check("first Alloc() is non-NULL", a != 0);
	pool->Free(a);
	void *b = pool->Alloc();
	check("Alloc() after Free() reuses the same element", a == b);

	printf("[3] Elements are isolated -- writing 200 bytes to one never bleeds into "
	       "its neighbor\n");
	void *e0 = b; /* still allocated from [2] */
	void *e1 = pool->Alloc();
	void *e2 = pool->Alloc();
	check("3 distinct elements", e0 != e1 && e1 != e2 && e0 != e2);
	memset(e0, 0xaa, 200);
	memset(e1, 0xbb, 200);
	memset(e2, 0xcc, 200);
	check("e0 unaffected by e1/e2 writes", ((unsigned char *)e0)[0] == 0xaa &&
	                                               ((unsigned char *)e0)[199] == 0xaa);
	check("e1 unaffected by e0/e2 writes", ((unsigned char *)e1)[0] == 0xbb &&
	                                               ((unsigned char *)e1)[199] == 0xbb);
	check("e2 unaffected by e0/e1 writes", ((unsigned char *)e2)[0] == 0xcc &&
	                                               ((unsigned char *)e2)[199] == 0xcc);

	printf("[4] Exhausting the initial 5-element chunk forces a real chunk-growth "
	       "event (mTotalBytes doubling) -- allocate past it with no crash\n");
	void *more[16];
	int allocated = 0;
	for (; allocated < 16; allocated++) {
		more[allocated] = pool->Alloc();
		if (more[allocated] == 0)
			break;
	}
	check("all 16 further allocations succeeded (grew past the 5-element chunk)",
	      allocated == 16);
	bool allDistinct = true;
	for (int i = 0; i < allocated && allDistinct; i++) {
		if (more[i] == e0 || more[i] == e1 || more[i] == e2)
			allDistinct = false;
		for (int j = i + 1; j < allocated; j++)
			if (more[i] == more[j])
				allDistinct = false;
	}
	check("every allocated element is a distinct pointer", allDistinct);

	printf("[5] Expand() is a real standalone utility -- fills the caller-owned SPool\n");
	CPool::SPool spoolOut;
	bool expandOk = pool->Expand(spoolOut, 4096);
	check("Expand(spoolOut, 4096) returns true", expandOk);
	check("spoolOut.ptr is non-NULL", spoolOut.ptr != 0);
	check("spoolOut.size == 4096", spoolOut.size == 4096);
	check("spoolOut.used == 0", spoolOut.used == 0);
	free(spoolOut.ptr); /* this chunk was never inserted into mChunks -- ours to free */

	printf("[6] PostKernelDestructor() frees every real chunk without crashing "
	       "(NOTE: real ground truth does not null mChunks[i].ptr afterward, so -- "
	       "faithfully preserved -- calling this a second time would double-free; "
	       "not exercised here, same as ground truth's own single real call site)\n");
	int rc = pool->PostKernelDestructor(0);
	check("PostKernelDestructor() returns 0", rc == 0);

	pool->~CPool();

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
