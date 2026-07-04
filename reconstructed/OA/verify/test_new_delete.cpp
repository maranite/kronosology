// SPDX-License-Identifier: GPL-2.0
/*
 * test_new_delete.cpp  -  host-side known-answer test for the
 * operator-new/delete -> stg_kmalloc/stg_kfree -> __kmalloc/kfree chain
 * (Stage 2, see include/oa_new_delete.h).
 *
 * `__kmalloc`/`kfree` are real Linux kernel APIs -- not host-linkable, so
 * this test supplies its own definitions of them (this TU is never linked
 * against a kernel) purely to observe what stg_kmalloc/stg_kfree pass
 * through. What's actually being verified is the confirmed forwarding
 * contract: the exact GFP_KERNEL flag value (0xd0) stg_kmalloc hands to
 * __kmalloc, and that every layer (operator new/delete down to the mock
 * kernel calls) passes its argument through unmodified -- not any real
 * allocation behavior, which is out of scope (the call contract itself is
 * the entire implementation, same as register_cdrom() in cdrom_check.cpp).
 */

#include <cstdio>
#include <cstdlib>
#include "oa_new_delete.h"

static int g_fail;
static oa_size_t    g_lastKmallocSize;
static unsigned int g_lastKmallocFlags;
static void        *g_lastKfreePtr;
static int          g_kmallocCalls;
static int          g_kfreeCalls;

extern "C" void *__kmalloc(oa_size_t size, unsigned int flags)
{
	g_lastKmallocSize = size;
	g_lastKmallocFlags = flags;
	g_kmallocCalls++;
	return malloc(size);
}

extern "C" void kfree(void *ptr)
{
	g_lastKfreePtr = ptr;
	g_kfreeCalls++;
	free(ptr);
}

static void check_true(const char *label, bool cond)
{
	if (cond) {
		printf("  ok    %s\n", label);
		return;
	}
	printf("  FAIL  %s\n", label);
	g_fail++;
}

int main(void)
{
	printf("operator new/delete -> stg_kmalloc/kfree known-answer test\n");
	printf("============================================================\n");

	printf("[1] stg_kmalloc forwards size and the confirmed GFP_KERNEL flag (0xd0)\n");
	void *p = stg_kmalloc(123);
	check_true("__kmalloc was called exactly once", g_kmallocCalls == 1);
	check_true("size forwarded unmodified", g_lastKmallocSize == 123);
	check_true("flags == confirmed GFP_KERNEL (0xd0)", g_lastKmallocFlags == 0xd0u);
	check_true("returned pointer is non-null", p != 0);
	stg_kfree(p);

	printf("[2] stg_kfree forwards the pointer to kfree() unmodified\n");
	void *p2 = malloc(16);
	stg_kfree(p2);
	check_true("kfree was called exactly once", g_kfreeCalls == 2);	/* once above, once here */
	check_true("pointer forwarded unmodified", g_lastKfreePtr == p2);

	printf("[3] operator new/delete route through stg_kmalloc/stg_kfree\n");
	int callsBefore = g_kmallocCalls;
	int *ip = new int;
	check_true("operator new called __kmalloc", g_kmallocCalls == callsBefore + 1);
	check_true("operator new size == sizeof(int)", g_lastKmallocSize == sizeof(int));
	delete ip;
	check_true("operator delete called kfree", g_kfreeCalls == 3);

	printf("[4] operator new[]/delete[] route through the same pair\n");
	callsBefore = g_kmallocCalls;
	int *arr = new int[10];
	check_true("operator new[] called __kmalloc", g_kmallocCalls == callsBefore + 1);
	check_true("operator new[] size >= 10 ints", g_lastKmallocSize >= 10 * sizeof(int));
	delete[] arr;
	check_true("operator delete[] called kfree", g_kfreeCalls == 4);

	printf("============================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
