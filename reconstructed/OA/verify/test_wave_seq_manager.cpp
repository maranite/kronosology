// SPDX-License-Identifier: GPL-2.0
/*
 * test_wave_seq_manager.cpp  -  KAT for CSTGWaveSeqManager::
 * CSTGWaveSeqManager()/Initialize() (see src/engine/wave_seq_manager.cpp).
 *
 * Uses MAP_32BIT for the manager's own backing buffer: the real
 * intrusive list bookkeeping truncates addresses to `unsigned int`
 * (matching the real 32-bit target), the same host/target pointer-
 * width hazard hit repeatedly elsewhere in this project.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
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

/* Real definition lives in engine_init.cpp (sec 10.58), not linked by
 * this test -- defined here instead so this test can link standalone. */
CSTGWaveSeqManager *CSTGWaveSeqManager::sInstance;

static int g_genCtorCalls, g_genInitCalls;
CSTGWaveSeqGenerator::CSTGWaveSeqGenerator()
{
	/* The real constructor's own body isn't reconstructed in this pass
	 * (confirmed real, deliberately deferred -- see oa_engine_init.h),
	 * but SOME real constructor's own field-zeroing is a load-bearing
	 * assumption for CSTGWaveSeqManager::Initialize()'s own list-
	 * insertion logic (its "empty list" fast path never explicitly
	 * zeroes a fresh node's own +0x0 "next" field, trusting it's
	 * already 0 -- see wave_seq_manager.cpp's own note). Mocking that
	 * same assumption here rather than leaving the whole object
	 * poisoned, matching how every other node-bearing class's mock in
	 * this project's test suite behaves. */
	memset(this, 0, sizeof(CSTGWaveSeqGenerator));
	g_genCtorCalls++;
}
void CSTGWaveSeqGenerator::Init() { g_genInitCalls++; }

static int g_mutexInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return malloc(size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

int main(void)
{
	printf("CSTGWaveSeqManager known-answer test\n");
	printf("=========================================================\n");

	unsigned char *buf = (unsigned char *)mmap(0, sizeof(CSTGWaveSeqManager),
						    PROT_READ | PROT_WRITE,
						    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	if (buf == MAP_FAILED) { perror("mmap"); return 1; }
	memset(buf, 0xcc, sizeof(CSTGWaveSeqManager));

	printf("[1] CSTGWaveSeqManager::CSTGWaveSeqManager()\n");
	CSTGWaveSeqManager *mgr = new (buf) CSTGWaveSeqManager();
	check_eq("200 generators constructed", g_genCtorCalls, 200);
	check_eq("sInstance == this", (long)(CSTGWaveSeqManager::sInstance == mgr), 1);
	check_eq("+0xe100 zeroed (list head)", *(unsigned int *)(buf + 0xe100), 0);
	check_eq("+0xe104 zeroed (list tail)", *(unsigned int *)(buf + 0xe104), 0);
	check_eq("+0xe108 zeroed (count)", *(unsigned int *)(buf + 0xe108), 0);
	check_eq("+0xe10c zeroed", *(unsigned int *)(buf + 0xe10c), 0);
	check_eq("+0xe120 zeroed", *(unsigned int *)(buf + 0xe120), 0);
	check_eq("+0xe12c mutex ptr non-null", *(unsigned int *)(buf + 0xe12c) != 0, 1);
	check_eq("+0xe130 mutex ptr non-null", *(unsigned int *)(buf + 0xe130) != 0, 1);
	check_eq("+0xe124 zeroed byte", buf[0xe124], 0);
	check_eq("+0xe126 zeroed word", *(unsigned short *)(buf + 0xe126), 0);

	printf("\n[2] CSTGWaveSeqManager::Initialize()\n");
	g_genCtorCalls = 0; /* not re-checked here, just isolating Init()'s own count */
	mgr->Initialize();
	check_eq("200 generators Init()'d", g_genInitCalls, 200);
	check_eq("+0xe108 count == 200", *(unsigned int *)(buf + 0xe108), 200);
	check_eq("+0xe104 (tail) == generator[0]", *(unsigned int *)(buf + 0xe104), ToU32(buf));
	check_eq("+0xe100 (head) == generator[199] (last pushed to front)",
		 *(unsigned int *)(buf + 0xe100), ToU32(buf + 199 * 0x120));

	/* Walk the list from head toward tail via each node's own +0x0
	 * ("next") field -- confirmed real: insertion sets
	 * `newNode->field0 = oldHead`, so the newest (head) node's own
	 * +0x0 points at the previously-inserted node, chaining all the
	 * way back to the very first insertion (the confirmed tail).
	 * (+0x4 ("prev") is NOT a stable reverse-order chain: it gets
	 * overwritten every time a node loses head status to a newer
	 * insertion, so its final per-node value isn't independently
	 * checked here.) */
	printf("\n[3] walk the doubly-linked list head->tail via +0x0 (\"next\")\n");
	{
		unsigned int cur = *(unsigned int *)(buf + 0xe100);
		int walkOk = 1;
		int steps = 0;
		unsigned int expected = ToU32(buf + 199 * 0x120);
		while (cur != 0 && steps < 205) {
			if (cur != expected) { walkOk = 0; break; }
			steps++;
			unsigned int nextExpected = (steps < 200) ?
				ToU32(buf + (199 - steps) * 0x120) : 0;
			cur = *(unsigned int *)((unsigned char *)(unsigned long)cur + 0x0);
			expected = nextExpected;
		}
		check_eq("walked exactly 200 nodes", steps, 200);
		check_eq("walk order matches push-front (199..0)", walkOk, 1);
	}

	printf("\n[4] every node's owner backpointer (+0xc) points at &this->e100field\n");
	{
		int ownerOk = 1;
		unsigned int expectedOwner = ToU32(buf + 0xe100);
		for (int i = 0; i < 200; i++) {
			unsigned char *gen = buf + i * 0x120;
			if (*(unsigned int *)(gen + 0xc) != expectedOwner)
				ownerOk = 0;
		}
		check_eq("all 200 owners correct", ownerOk, 1);
	}

	printf("\n[5] CSTGWaveSeqGenerator::sMutex is &this->e130field (pointer-to-pointer idiom)\n");
	check_eq("sMutex == &buf[0xe130]", (long)(CSTGWaveSeqGenerator::sMutex == (void **)(buf + 0xe130)), 1);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
