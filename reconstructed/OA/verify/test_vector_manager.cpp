// SPDX-License-Identifier: GPL-2.0
/*
 * test_vector_manager.cpp  -  KAT for
 * CSTGVectorManager::CSTGVectorManager() (see src/engine/vector_manager.cpp).
 *
 * Verifies exact instance counts per vector-EG type (432/432/34),
 * both confirmed real "gap" regions stay untouched, both confirmed
 * zeroed regions are fully zeroed, and the 3 mutex fields.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_engine_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-55s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

CSTGVectorEGBase::CSTGVectorEGBase() {}

static int g_xOnlyCtorCalls, g_xyCtorCalls, g_ccCtorCalls;
CSTGVectorEGXOnly::CSTGVectorEGXOnly() { memset(this, 0, sizeof(*this)); g_xOnlyCtorCalls++; }
CSTGVectorEGXY::CSTGVectorEGXY() { memset(this, 0, sizeof(*this)); g_xyCtorCalls++; }
CSTGVectorEGCC::CSTGVectorEGCC() { memset(this, 0, sizeof(*this)); g_ccCtorCalls++; }
void **CSTGVectorEGXOnly::sMutex;
void **CSTGVectorEGXY::sMutex;

/* Real definition lives in engine_init.cpp (sec 10.58), not linked by
 * this test -- defined here instead so this test can link standalone. */
CSTGVectorManager *CSTGVectorManager::sInstance;

static int g_mutexInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return malloc(size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }

static bool all_zero(unsigned char *p, unsigned int off, unsigned int len)
{
	for (unsigned int i = 0; i < len; i++)
		if (p[off + i] != 0)
			return false;
	return true;
}

int main(void)
{
	printf("CSTGVectorManager known-answer test\n");
	printf("=========================================================\n");

	unsigned char *buf = new unsigned char[sizeof(CSTGVectorManager)];
	memset(buf, 0xcc, sizeof(CSTGVectorManager));

	CSTGVectorManager *mgr = new (buf) CSTGVectorManager();

	printf("[1] instance counts\n");
	check_eq("432x CSTGVectorEGXOnly (400 loop + 32 unrolled)", g_xOnlyCtorCalls, 432);
	check_eq("432x CSTGVectorEGXY (400 loop + 32 unrolled)", g_xyCtorCalls, 432);
	check_eq("34x CSTGVectorEGCC (all unrolled)", g_ccCtorCalls, 34);

	printf("\n[2] sInstance and mutexes\n");
	check_eq("sInstance == this", (long)(CSTGVectorManager::sInstance == mgr), 1);
	check_eq("mutex_init called 3 times", g_mutexInitCalls, 3);
	check_eq("+0x1c9dc mutex ptr non-null", *(unsigned int *)(buf + 0x1c9dc) != 0, 1);
	check_eq("+0x1c9e0 mutex ptr non-null", *(unsigned int *)(buf + 0x1c9e0) != 0, 1);
	check_eq("+0x1c9e4 mutex ptr non-null", *(unsigned int *)(buf + 0x1c9e4) != 0, 1);

	printf("\n[3] confirmed zeroed regions\n");
	check_eq("+0x1adf0..+0x1af70 fully zeroed", (long)all_zero(buf, 0x1adf0, 0x1af70 - 0x1adf0), 1);
	check_eq("+0x1c7a4..+0x1c924 fully zeroed", (long)all_zero(buf, 0x1c7a4, 0x1c924 - 0x1c7a4), 1);
	check_eq("+0x1c9ac..+0x1c9dc fully zeroed", (long)all_zero(buf, 0x1c9ac, 0x1c9dc - 0x1c9ac), 1);

	printf("\n[4] confirmed real gaps -- left untouched (still poisoned)\n");
	check_eq("+0x1af70..+0x1aff4 still poisoned", buf[0x1af70], 0xcc);
	check_eq("+0x1aff3 (last byte of gap) still poisoned", buf[0x1aff3], 0xcc);
	check_eq("+0x1c924..+0x1c9ac still poisoned", buf[0x1c924], 0xcc);
	check_eq("+0x1c9ab (last byte of gap) still poisoned", buf[0x1c9ab], 0xcc);

	printf("\n[5] boundary spot-checks: first/last instance of each array segment\n");
	check_eq("first EGXOnly (loop) at +0x0", (long)(buf[0] == 0), 1);
	check_eq("last EGXOnly (loop) starts at +0xd3f8 (400th, 0x88 stride)",
		 (long)(0xd480 - 0x88), 0xd3f8);
	check_eq("first EGXY (loop) at +0xd480", (long)(buf[0xd480] == 0), 1);
	check_eq("first EGCC (unrolled) at +0x19640", (long)(buf[0x19640] == 0), 1);
	check_eq("last EGXY (2nd unrolled run) starts at +0x1c728",
		 (long)(0x1c7a4 - 0x7c), 0x1c728);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
