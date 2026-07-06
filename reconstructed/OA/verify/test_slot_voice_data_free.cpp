// SPDX-License-Identifier: GPL-2.0
/*
 * test_slot_voice_data_free.cpp  -  KAT for batch 17's
 * src/engine/slot_voice_data_free.cpp: CSTGSlotVoiceData::
 * FreeSlotVoiceData(bool), CSTGSlotVoiceData::AreAllKeysAndPedalsReleased()
 * const, CSTGSmoother::CancelAllSlotVoiceDataCCSmoothers(const
 * CSTGSlotVoiceData*), and CSTGPerformanceVars::NotifyAllKeysAndPedalsReleased().
 *
 * Deliberately does NOT link global.cpp: mocks CSTGGlobal::
 * FreeSlotVoiceData()/CLoadBalancer::BalanceStaticLoad() as simple
 * call-recorders instead, keeping this KAT focused on the four new
 * functions' own logic rather than the (already independently tested)
 * rest of CSTGGlobal/CLoadBalancer.
 *
 * Every object whose own address is round-tripped through a packed
 * 32-bit field (the two intrusive-list "identity" tokens, the
 * CSTGGlobal active-voice-data list, etc.) is backed by mmap32()/
 * MAP_32BIT or a plain calloc() within this test's own control -- this
 * is a 64-bit host reconstructing a 32-bit target, the same
 * "pointer-width" discipline established throughout this project.
 */

#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"

#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>

/* No <cstring> here -- it conflicts with oa_internal.h's own `strlen`
 * declaration (different exception specifier), the same reasoning
 * setup_global_resources.cpp/stgheap_init.cpp already established for
 * inlining their own zero-fill rather than using libc memset. */
static void local_zero(void *p, unsigned long n)
{
	unsigned char *b = (unsigned char *)p;
	for (unsigned long i = 0; i < n; i++)
		b[i] = 0;
}

static int g_fail;
static void check_eq(const char *what, long got, long want)
{
	if (got == want) {
		printf("  ok    %-60s %ld\n", what, got);
	} else {
		printf("  FAIL  %-60s got=%ld want=%ld\n", what, got, want);
		g_fail = 1;
	}
}

static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

static unsigned int addr32(void *p) { return (unsigned int)(unsigned long)p; }
static void wr32(unsigned char *p, unsigned long off, unsigned int v) { *(unsigned int *)(p + off) = v; }
static unsigned int rd32(unsigned char *p, unsigned long off) { return *(unsigned int *)(p + off); }

/* ---- mocks ---- */

CSTGSmoother *CSTGSmoother::sInstance;
static int g_finalizeSmootherCalls;
static void *g_lastFinalizedNode;
void CSTGSmoother::FinalizeSmoother(void *node, bool flag)
{
	g_finalizeSmootherCalls++;
	g_lastFinalizedNode = node;
	(void)flag;
}

CSTGGlobal *CSTGGlobal::sInstance;
static int g_globalFreeSlotVoiceDataCalls;
static CSTGSlotVoiceData *g_lastFreedNode;
void CSTGGlobal::FreeSlotVoiceData(CSTGSlotVoiceData *node)
{
	g_globalFreeSlotVoiceDataCalls++;
	g_lastFreedNode = node;
}

CLoadBalancer *CLoadBalancer::sInstance;
static int g_balanceStaticLoadCalls;
void CLoadBalancer::BalanceStaticLoad() { g_balanceStaticLoadCalls++; }

unsigned char CSTGPerformanceVarsManager::sInstance[12];
unsigned char *STGAPIFrontPanelStatus::sInstance;

int main(void)
{
	int rc = 0;

	printf("[1] CSTGSlotVoiceData::AreAllKeysAndPedalsReleased() const -- all 3 branches\n");
	{
		unsigned char buf[0x2900];
		local_zero(buf, sizeof(buf));
		CSTGSlotVoiceData *v = (CSTGSlotVoiceData *)buf;

		buf[0x2888] = 1; buf[0x1790] = 0; buf[0x17a8] = 0;
		check_eq("+0x2888 set -> false", v->AreAllKeysAndPedalsReleased(), 0);

		buf[0x2888] = 0; buf[0x1790] = 0x50; buf[0x17a8] = 0;
		check_eq("+0x1790 > 0x4f -> false", v->AreAllKeysAndPedalsReleased(), 0);

		buf[0x2888] = 0; buf[0x1790] = 0x4f; buf[0x17a8] = 0x40;
		check_eq("+0x1790<=0x4f, +0x17a8 > 0x3f -> false", v->AreAllKeysAndPedalsReleased(), 0);

		buf[0x2888] = 0; buf[0x1790] = 0x4f; buf[0x17a8] = 0x3f;
		check_eq("+0x1790<=0x4f, +0x17a8<=0x3f -> true", v->AreAllKeysAndPedalsReleased(), 1);
	}

	printf("\n[2] CSTGSmoother::CancelAllSlotVoiceDataCCSmoothers -- filters by mapping+0x10==8 AND +0xac==target\n");
	{
		/* Every object below has its own address round-tripped through
		 * a packed 32-bit field (the intrusive list's own next/mapping
		 * pointers) -- on this 64-bit host, a plain stack/heap address
		 * routinely exceeds 32 bits and would silently truncate to a
		 * wild pointer on read-back, so ALL of them must be
		 * mmap32()/MAP_32BIT-backed, not stack arrays. */
		unsigned char *smootherBuf = mmap32(0x10000);
		local_zero(smootherBuf, 0x10000);
		CSTGSmoother *sm = (CSTGSmoother *)smootherBuf;

		unsigned char *targetBuf = mmap32(0x10);
		CSTGSlotVoiceData *target = (CSTGSlotVoiceData *)targetBuf;
		unsigned char *otherBuf = mmap32(0x10);
		CSTGSlotVoiceData *other = (CSTGSlotVoiceData *)otherBuf;

		/* Three list nodes: node0 (mapping type!=8, skipped), node1
		 * (mapping type==8 but different target, skipped), node2
		 * (mapping type==8 AND matches target, finalized). */
		unsigned char *node0 = mmap32(0x10), *node1 = mmap32(0x10), *node2 = mmap32(0x10);
		unsigned char *mapping0 = mmap32(0xb0), *mapping1 = mmap32(0xb0), *mapping2 = mmap32(0xb0);
		local_zero(mapping0, 0xb0);
		local_zero(mapping1, 0xb0);
		local_zero(mapping2, 0xb0);

		wr32(mapping0, 0x10, 2);				/* not 8 */
		wr32(mapping1, 0x10, 8);
		wr32(mapping1, 0xac, addr32(other));		/* different target */
		wr32(mapping2, 0x10, 8);
		wr32(mapping2, 0xac, addr32(target));		/* matches */

		wr32(node0, 0x0, addr32(node1));
		wr32(node0, 0x8, addr32(mapping0));
		wr32(node1, 0x0, addr32(node2));
		wr32(node1, 0x8, addr32(mapping1));
		wr32(node2, 0x0, 0);
		wr32(node2, 0x8, addr32(mapping2));

		wr32(smootherBuf, 0xf010, addr32(node0));

		g_finalizeSmootherCalls = 0;
		g_lastFinalizedNode = 0;
		sm->CancelAllSlotVoiceDataCCSmoothers(target);

		check_eq("exactly one node finalized", g_finalizeSmootherCalls, 1);
		check_eq("the matching node2 was the one finalized",
			 g_lastFinalizedNode == (void *)node2, 1);
	}

	printf("\n[3] CSTGSlotVoiceData::FreeSlotVoiceData(bool) -- full dispatch\n");
	{
		/* CSTGGlobal::sInstance backing: only +0x29c9900 (the active
		 * voice-data list head NotifyAllKeysAndPedalsReleased reads)
		 * matters here -- zero-initialized calloc gives an empty
		 * list "for free", matching this project's own established
		 * ~43.6MB calloc convention for CSTGGlobal-shaped buffers. */
		unsigned char *globalBuf = (unsigned char *)calloc(1, 0x29c9fc0);
		CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;

		CSTGSmoother *smInstanceBuf = (CSTGSmoother *)calloc(1, 0x10000);
		CSTGSmoother::sInstance = smInstanceBuf; /* +0xf010 == 0: empty CC-smoother list */

		CLoadBalancer *lbBuf = (CLoadBalancer *)calloc(1, 0x100);
		CLoadBalancer::sInstance = lbBuf;

		unsigned char *panelBuf = mmap32(0x2000);
		STGAPIFrontPanelStatus::sInstance = panelBuf;

		/* A valid CSTGPerformanceVars target at sInstance-array slot 0,
		 * preset to state 3 (so NotifyAllKeysAndPedalsReleased doesn't
		 * early-return) with a group id (+0x23d0) that won't match
		 * anything in the (empty) CSTGGlobal voice-data list. */
		unsigned char *pvBuf = mmap32(0x3000);
		wr32(CSTGPerformanceVarsManager::sInstance, 0, addr32(pvBuf));
		wr32(CSTGPerformanceVarsManager::sInstance, 4, 0); /* slot 1 unused this test */
		pvBuf[0x23d1] = 3;
		pvBuf[0x23d0] = 7;
		wr32(pvBuf, 0x23e0, 0xdeadbeef);

		unsigned char *voiceBuf = mmap32(0x3000);
		CSTGSlotVoiceData *v = (CSTGSlotVoiceData *)voiceBuf;

		/* List #1 (+0x4/+0x8, owner +0x10): a SINGLE-entry list -- v is
		 * both head and tail. */
		unsigned char *owner1 = mmap32(0x20);
		wr32(owner1, 0x0, addr32(voiceBuf + 0x4));	/* head == v's own identity */
		wr32(owner1, 0x4, addr32(voiceBuf + 0x4));	/* tail == v's own identity */
		wr32(owner1, 0x8, 5);				/* count */
		wr32(voiceBuf, 0x4, 0);
		wr32(voiceBuf, 0x8, 0);
		wr32(voiceBuf, 0x10, addr32(owner1));

		/* List #2 (+0x14/+0x18, owner +0x20): v is a MIDDLE node between
		 * synthetic siblings A and C. */
		unsigned char *owner2 = mmap32(0x20);
		unsigned char *nodeA = mmap32(0x20);
		unsigned char *nodeC = mmap32(0x20);
		wr32(owner2, 0x0, 0x1234);	/* head: unrelated to v, untouched */
		wr32(owner2, 0x4, 0x5678);	/* tail: unrelated to v, untouched */
		wr32(owner2, 0x8, 9);		/* count */
		wr32(voiceBuf, 0x14, addr32(nodeA));	/* v.link1 -> &nodeA[0] */
		wr32(voiceBuf, 0x18, addr32(nodeC));	/* v.link2 -> &nodeC[0] */
		wr32(voiceBuf, 0x20, addr32(owner2));

		/* Program sub-object at +0x34: subType (+0xd) == 1, and
		 * +0x28c4 (locked) == 0 -- so an arg==true call SHOULD reach
		 * CLoadBalancer::BalanceStaticLoad(). */
		unsigned char *programBuf = mmap32(0x20);
		programBuf[0xd] = 1;
		wr32(voiceBuf, 0x34, addr32(programBuf));
		wr32(voiceBuf, 0x28c4, 0);

		/* +0x28c8: perf-vars slot selector -- 0 selects
		 * CSTGPerformanceVarsManager::sInstance[0] (pvBuf above). */
		voiceBuf[0x28c8] = 0;

		/* allKeysReleased == false (so NotifyAllKeysAndPedalsReleased
		 * SHOULD be dispatched): +0x2888 clear, +0x1790<=0x4f,
		 * +0x17a8 > 0x3f. */
		voiceBuf[0x2888] = 0;
		voiceBuf[0x1790] = 0x10;
		voiceBuf[0x17a8] = 0x40;

		g_finalizeSmootherCalls = 0;
		g_globalFreeSlotVoiceDataCalls = 0;
		g_balanceStaticLoadCalls = 0;

		v->FreeSlotVoiceData(true);

		check_eq("CSTGSmoother::CancelAllSlotVoiceDataCCSmoothers reached (empty list, no finalize calls)",
			 g_finalizeSmootherCalls, 0);
		check_eq("CSTGGlobal::FreeSlotVoiceData called exactly once", g_globalFreeSlotVoiceDataCalls, 1);
		check_eq("  ...with this node", g_lastFreedNode == v, 1);
		check_eq("arg==true, subType==1, locked==0 -> BalanceStaticLoad called", g_balanceStaticLoadCalls, 1);

		printf("  -- list #1 (single-entry): owner head/tail/count and node fields --\n");
		check_eq("owner1.head == 0 (was v's own identity, now v.link1==0)", (long)rd32(owner1, 0x0), 0);
		check_eq("owner1.tail == 0 (was v's own identity, now v.link2==0)", (long)rd32(owner1, 0x4), 0);
		check_eq("owner1.count decremented (5 -> 4)", (long)rd32(owner1, 0x8), 4);
		check_eq("v's own +0x4 cleared", (long)rd32(voiceBuf, 0x4), 0);
		check_eq("v's own +0x8 cleared", (long)rd32(voiceBuf, 0x8), 0);
		check_eq("v's own +0x10 (owner ptr) cleared", (long)rd32(voiceBuf, 0x10), 0);

		printf("  -- list #2 (middle-of-3): neighbor propagation, owner head/tail untouched --\n");
		check_eq("owner2.head untouched (v was not head)", (long)rd32(owner2, 0x0), 0x1234);
		check_eq("owner2.tail untouched (v was not tail)", (long)rd32(owner2, 0x4), 0x5678);
		check_eq("owner2.count decremented (9 -> 8)", (long)rd32(owner2, 0x8), 8);
		check_eq("nodeC[0..3] now holds v's own OLD link1 (&nodeA[0])",
			 (long)rd32(nodeC, 0x0), (long)addr32(nodeA));
		check_eq("nodeA[4..7] now holds v's own OLD link2 (&nodeC[0])",
			 (long)rd32(nodeA, 0x4), (long)addr32(nodeC));
		check_eq("v's own +0x14 cleared", (long)rd32(voiceBuf, 0x14), 0);
		check_eq("v's own +0x18 cleared", (long)rd32(voiceBuf, 0x18), 0);
		check_eq("v's own +0x20 (owner ptr) cleared", (long)rd32(voiceBuf, 0x20), 0);

		printf("  -- NotifyAllKeysAndPedalsReleased dispatch (real, not mocked) --\n");
		check_eq("pvBuf's own +0x23d1 committed to 4 (proof it was invoked)", (long)pvBuf[0x23d1], 4);
		check_eq("pvBuf's own +0x23f8 == saved OLD +0x23e0", (long)rd32(pvBuf, 0x23f8), (long)0xdeadbeef);

		printf("\n[3b] FreeSlotVoiceData(false) -- BalanceStaticLoad must NOT be called regardless of program/locked state\n");
		/* Rebuild both lists as empty (owner==0) so this second call
		 * doesn't re-walk already-cleared state. */
		wr32(voiceBuf, 0x10, 0);
		wr32(voiceBuf, 0x20, 0);
		g_balanceStaticLoadCalls = 0;
		g_globalFreeSlotVoiceDataCalls = 0;
		v->FreeSlotVoiceData(false);
		check_eq("arg==false -> BalanceStaticLoad never called", g_balanceStaticLoadCalls, 0);
		check_eq("CSTGGlobal::FreeSlotVoiceData still called", g_globalFreeSlotVoiceDataCalls, 1);

		printf("\n[3c] FreeSlotVoiceData -- allKeysReleased==true skips the notify dispatch entirely\n");
		/* Poison the perf-vars slot: if NotifyAllKeysAndPedalsReleased
		 * were (wrongly) still called, this would dereference a wild
		 * pointer and crash -- the cleanest possible proof of a skip. */
		wr32(CSTGPerformanceVarsManager::sInstance, 0, 0xdeadbeef);
		voiceBuf[0x2888] = 0;
		voiceBuf[0x1790] = 0x4f;
		voiceBuf[0x17a8] = 0x3f; /* -> allKeysReleased == true */
		v->FreeSlotVoiceData(false);
		printf("  ok    (no crash: NotifyAllKeysAndPedalsReleased correctly skipped)\n");
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	rc = g_fail;
	return rc;
}
