// SPDX-License-Identifier: GPL-2.0
/*
 * test_vector_manager_init.cpp  -  KAT for CSTGVectorManager::Initialize()
 * (see src/engine/vector_manager_init.cpp).
 *
 * Verifies: exact vtable-slot-0 dispatch counts per phase (400/400/17/16/16),
 * exact index stamps (including the shared-index-per-pair property of phase
 * 5), the two intrusive lists' head/tail/count and full walkability, the
 * confirmed marker write, the 4 confirmed zeroed tables, the two sMutex
 * assignments, and -- the key real-quirk check -- that "batch2" objects
 * (never touched by this function) are NOT dispatched and NOT index-stamped.
 *
 * Uses MAP_32BIT for the manager's own backing buffer: the intrusive
 * list-node fields truncate addresses to `unsigned int`, matching the
 * real 32-bit target -- the same host/target pointer-width hazard hit
 * repeatedly elsewhere in this project (e.g. CSTGWaveSeqManager,
 * sec 10.62).
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
		printf("  ok    %-60s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static int g_xOnlyCalls, g_xyCalls, g_ccCalls;
static void *g_xOnlyVtable[1];
static void *g_xyVtable[1];
static void *g_ccVtable[1];
static void XOnlySlot0(void *) { g_xOnlyCalls++; }
static void XYSlot0(void *) { g_xyCalls++; }
static void CCSlot0(void *) { g_ccCalls++; }

/* sec 10.227: CSTGVectorEGBase/XOnly/XY/CC are now genuinely C++-
 * polymorphic (real `virtual void Init()`, see oa_engine_init.h). This
 * test target doesn't link the real vector_eg_ctors.cpp (it provides
 * its own mock constructors below, to precisely count vtable-slot-0
 * dispatches instead of running the real bodies), so the linker still
 * needs SOME definition for each class's own compiler-generated
 * `Init()` -- these trivial bodies are never actually reached: each
 * mock constructor below immediately overwrites its object's real
 * vtable pointer with a fake one (g_xOnlyVtable/g_xyVtable/g_ccVtable)
 * pointing at XOnlySlot0/XYSlot0/CCSlot0 instead, which is what
 * CSTGVectorManager::Initialize()'s own generic vtable-slot-0 dispatch
 * (vector_manager_init.cpp's DispatchSlot0) actually calls through. */
void CSTGVectorEGBase::Init() {}
void CSTGVectorEGXOnly::Init() {}
void CSTGVectorEGXY::Init() {}
void CSTGVectorEGCC::Init() {}

CSTGVectorEGBase::CSTGVectorEGBase() {}

CSTGVectorEGXOnly::CSTGVectorEGXOnly()
{
	memset(this, 0, sizeof(*this));
	*(void ***)this = g_xOnlyVtable;
}
CSTGVectorEGXY::CSTGVectorEGXY()
{
	memset(this, 0, sizeof(*this));
	*(void ***)this = g_xyVtable;
}
CSTGVectorEGCC::CSTGVectorEGCC()
{
	memset(this, 0, sizeof(*this));
	*(void ***)this = g_ccVtable;
}

CSTGVectorManager *CSTGVectorManager::sInstance;

static int g_mutexInitCalls;
extern "C" unsigned int get_sizeof_rtwrap_pthread_mutex(void) { return 24; }
extern "C" void *rtwrap_malloc(unsigned int size) { return malloc(size); }
extern "C" void rtwrap_pthread_mutex_init(void *, void *) { g_mutexInitCalls++; }

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned short u16at(unsigned char *p, unsigned int off)
{
	return *(unsigned short *)(p + off);
}
static unsigned int u32at(unsigned char *p, unsigned int off)
{
	return *(unsigned int *)(p + off);
}

int main(void)
{
	printf("CSTGVectorManager::Initialize() known-answer test\n");
	printf("=========================================================\n");

	g_xOnlyVtable[0] = (void *)&XOnlySlot0;
	g_xyVtable[0] = (void *)&XYSlot0;
	g_ccVtable[0] = (void *)&CCSlot0;

	unsigned char *buf = (unsigned char *)mmap(0, sizeof(CSTGVectorManager),
						    PROT_READ | PROT_WRITE,
						    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	if (buf == MAP_FAILED) { perror("mmap"); return 1; }
	memset(buf, 0xcc, sizeof(CSTGVectorManager));
	CSTGVectorManager *mgr = new (buf) CSTGVectorManager();

	/* The real constructor already ran (via the mocks above) and
	 * zeroed EVERY object it constructs, including the "batch2" ones
	 * -- so their own +0x4 index field reads back as 0 post-
	 * construction, not the 0xcc poison. To actually prove
	 * Initialize() never touches them, re-poison just those three
	 * index fields to a distinct sentinel now, then confirm they're
	 * still that sentinel after Initialize() runs. */
	*(unsigned short *)(buf + 0x1aff4 + 0x4) = 0xdead;
	*(unsigned short *)(buf + 0x1b764 + 0x4) = 0xdead;
	*(unsigned short *)(buf + 0x1bfe4 + 0x4) = 0xdead;

	g_xOnlyCalls = g_xyCalls = g_ccCalls = 0;
	mgr->Initialize();

	printf("[1] vtable-slot-0 dispatch counts per phase\n");
	check_eq("EGXOnly total dispatches (400 main + 16 batch1)", g_xOnlyCalls, 416);
	check_eq("EGXY total dispatches (400 main + 16 batch1)", g_xyCalls, 416);
	check_eq("EGCC total dispatches (17 batch1 only)", g_ccCalls, 17);

	printf("\n[2] index stamps, phase 1 (EGXOnly main, i=0..399 at +0x4)\n");
	check_eq("EGXOnly[0] index", u16at(buf, 0x0 + 0x4), 0);
	check_eq("EGXOnly[399] index", u16at(buf, 399 * 0x88 + 0x4), 399);

	printf("\n[3] index stamps, phase 2 (EGXY main, i=0..399 at +0x4)\n");
	check_eq("EGXY[0] index", u16at(buf, 0xd480 + 0x4), 0);
	check_eq("EGXY[399] index", u16at(buf, 0xd480 + 399 * 0x7c + 0x4), 399);

	printf("\n[4] confirmed real marker write\n");
	check_eq("+0x1aff0 (word) == 0", u16at(buf, 0x1aff0), 0);
	check_eq("+0x1aff2 (byte) == 0", buf[0x1aff2], 0);
	check_eq("+0x1aff3 (byte) == 1", buf[0x1aff3], 1);

	printf("\n[5] EGCC batch1 literal index stamps (i=0..16 at +0x4)\n");
	check_eq("EGCC[0] index", u16at(buf, 0x19640 + 0x4), 0);
	check_eq("EGCC[16] index", u16at(buf, 0x19640 + 16 * 0x70 + 0x4), 16);

	printf("\n[6] batch1 EGXOnly/EGXY pairs share the SAME index (400+i)\n");
	check_eq("EGXOnly-batch1[0] index == 400", u16at(buf, 0x19db0 + 0x4), 400);
	check_eq("EGXY-batch1[0] index == 400 (shared, not independent)", u16at(buf, 0x1a630 + 0x4), 400);
	check_eq("EGXOnly-batch1[15] index == 415", u16at(buf, 0x19db0 + 15 * 0x88 + 0x4), 415);
	check_eq("EGXY-batch1[15] index == 415", u16at(buf, 0x1a630 + 15 * 0x7c + 0x4), 415);

	printf("\n[7] the 4 confirmed zeroed tables (spot-check nonzero poison cleared)\n");
	check_eq("table0[5] == 0", u16at(buf, 0x1af70 + 5 * 2), 0);
	check_eq("table1[5] == 0", u16at(buf, 0x1af90 + 5 * 2), 0);
	check_eq("table2[5] == 0", u16at(buf, 0x1afb0 + 5 * 2), 0);
	check_eq("table3[5] == 0", u16at(buf, 0x1afd0 + 5 * 2), 0);

	printf("\n[8] CONFIRMED REAL QUIRK: batch2 objects are never touched\n");
	check_eq("EGCC-batch2[0] (+0x1aff4) index still sentinel (never written)",
		 u16at(buf, 0x1aff4 + 0x4), 0xdead);
	check_eq("EGXOnly-batch2[0] (+0x1b764) index still sentinel",
		 u16at(buf, 0x1b764 + 0x4), 0xdead);
	check_eq("EGXY-batch2[0] (+0x1bfe4) index still sentinel",
		 u16at(buf, 0x1bfe4 + 0x4), 0xdead);

	printf("\n[9] intrusive lists: head/tail/count and full walkability\n");
	check_eq("EGXOnly list count == 400", u32at(buf, 0x1c9b4), 400);
	check_eq("EGXY list count == 400", u32at(buf, 0x1c9c0), 400);
	check_eq("EGXOnly list head == EGXOnly[399] (last pushed to front)",
		 u32at(buf, 0x1c9ac), ToU32(buf + 399 * 0x88 + 0x3c));
	check_eq("EGXOnly list tail == EGXOnly[0]'s own node",
		 u32at(buf, 0x1c9b0), ToU32(buf + 0x0 + 0x3c));
	{
		/* Walk EGXOnly's list from head via each node's own +0x0
		 * ("next"), matching CSTGWaveSeqManager's own confirmed
		 * push-front traversal direction (sec 10.62). */
		unsigned int cur = u32at(buf, 0x1c9ac);
		int steps = 0;
		while (cur != 0 && steps < 405) {
			steps++;
			cur = *(unsigned int *)((unsigned char *)(unsigned long)cur + 0x0);
		}
		check_eq("EGXOnly list walk reaches all 400 nodes", steps, 400);
	}
	{
		unsigned int cur = u32at(buf, 0x1c9b8);
		int steps = 0;
		while (cur != 0 && steps < 405) {
			steps++;
			cur = *(unsigned int *)((unsigned char *)(unsigned long)cur + 0x0);
		}
		check_eq("EGXY list walk reaches all 400 nodes", steps, 400);
	}

	printf("\n[10] batch1 objects are NOT list-linked (owner field never written)\n");
	check_eq("EGXOnly-batch1[0] owner (+0x48) still 0 (ctor-zeroed, never written)",
		 u32at(buf, 0x19db0 + 0x48), 0);
	check_eq("EGCC[0] owner-equivalent (+0x48) still 0 (EGCC never list-linked)",
		 u32at(buf, 0x19640 + 0x48), 0);

	printf("\n[11] sMutex tail assignments\n");
	check_eq("CSTGVectorEGXOnly::sMutex == &mgr[+0x1c9e0]",
		 (long)(CSTGVectorEGXOnly::sMutex == (void **)(buf + 0x1c9e0)), 1);
	check_eq("CSTGVectorEGXY::sMutex == &mgr[+0x1c9e4]",
		 (long)(CSTGVectorEGXY::sMutex == (void **)(buf + 0x1c9e4)), 1);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
