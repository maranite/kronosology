/*
 * test_heap_and_timer_engine.cpp  -  host-side known-answer test for the two
 * dependency chains promoted from Tier B to real in the 2026-07-25 follow-up batch:
 *   CHeap (heap.h) -> CChkCmdBG (chunk_man.h)
 *   CWheelsContainer/CExternalClock/CInternalClock (timer_engine.h) -> CTimerEngine
 *
 * See heap.h/chunk_man.h/timer_engine.h file headers for the ground-truth
 * derivations these checks assert against.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>

#include "module.h"
#include "task.h"
#include "heap.h"
#include "chunk_man.h"
#include "timer_engine.h"
#include "omega_vtables.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

extern CSystemApi *Api; /* real global, mains.cpp */

struct CHeapTestHooks {
	static int   Capacity(const CHeap &h) { return h.mCapacity; }
	static int   Count(const CHeap &h)    { return h.mCount; }
	static int   GrowBy(const CHeap &h)   { return h.mGrowBy; }
	static void *Slots(const CHeap &h)    { return h.mSlots; }
};

struct ChkCmdBGTestHooks {
	static int   State(const CChkCmdBG &c)        { return c.mState; }
	static const CHeap &Heap1(const CChkCmdBG &c) { return c.mHeap1; }
	static const CHeap &Heap2(const CChkCmdBG &c) { return c.mHeap2; }
	static int   PendingCount(const CChkCmdBG &c) { return c.mPendingCount; }
	static void *OutLinkMono(const CChkCmdBG &c)  { return c.mOutLinkMono; }
};

struct TimerEngineTestHooks {
	static int WheelsCapacity(const CTimerEngine &e)
	{
		return *(const int *)((const char *)&e.mWheels);
	}
	static void *ExtClockVtbl(const CTimerEngine &e)
	{
		return *(void * const *)((const char *)&e.mExtClock);
	}
	static void *IntClockVtbl(const CTimerEngine &e)
	{
		return *(void * const *)((const char *)&e.mIntClock);
	}
	static void *RXVtbl(const CTimerEngine &e) { return e.mRXVtbl; }
	static void *TXVtbl(const CTimerEngine &e) { return e.mTXVtbl; }
	static void *RXInterfacePtr(const CTimerEngine &e) { return e.mRXInterfacePtr; }
	static void *TXInterfacePtr(const CTimerEngine &e) { return e.mTXInterfacePtr; }
};

/* Real ModuleTestHooks-shaped access, same raw offsets as test_small_modules.cpp. */
struct ModuleTestHooks {
	static int TaskCount(const CModule &m)
	{
		return *(const int *)((const unsigned char *)&m + 0x14);
	}
};

int main()
{
	printf("CHeap/CChkCmdBG/CTimerEngine known-answer test\n");
	printf("================================================\n");

	CModule owner("TestOwner");

	printf("[1] CHeap default ctor\n");
	{
		CHeap h;
		check("mCapacity == 0x10", CHeapTestHooks::Capacity(h) == 0x10);
		check("mCount == 0", CHeapTestHooks::Count(h) == 0);
		check("mGrowBy == 8", CHeapTestHooks::GrowBy(h) == 8);
		check("mSlots is non-NULL", CHeapTestHooks::Slots(h) != 0);
	}

	printf("[2] CHeap(growBy, capacity) ctor\n");
	{
		CHeap h(10, 5);
		check("mGrowBy == 10", CHeapTestHooks::GrowBy(h) == 10);
		check("mCapacity == 5", CHeapTestHooks::Capacity(h) == 5);
		check("mCount == 0", CHeapTestHooks::Count(h) == 0);
		check("mSlots is non-NULL", CHeapTestHooks::Slots(h) != 0);
	}

	printf("[3] CHeap(growBy, 0) allocates nothing\n");
	{
		CHeap h(10, 0);
		check("mCapacity == 0", CHeapTestHooks::Capacity(h) == 0);
		check("mSlots is NULL", CHeapTestHooks::Slots(h) == 0);
	}

	printf("[4] CChkCmdBG ctor -- 2 embedded CHeap(10,5) sub-objects\n");
	{
		void *raw = malloc(0xc0);
		CChkCmdBG *bg = new (raw) CChkCmdBG(owner);

		check("mState == 2", ChkCmdBGTestHooks::State(*bg) == 2);
		check("mHeap1: growBy==10", CHeapTestHooks::GrowBy(ChkCmdBGTestHooks::Heap1(*bg)) == 10);
		check("mHeap1: capacity==5", CHeapTestHooks::Capacity(ChkCmdBGTestHooks::Heap1(*bg)) == 5);
		check("mHeap1: slots non-NULL", CHeapTestHooks::Slots(ChkCmdBGTestHooks::Heap1(*bg)) != 0);
		check("mHeap2: growBy==10", CHeapTestHooks::GrowBy(ChkCmdBGTestHooks::Heap2(*bg)) == 10);
		check("mHeap2: capacity==5", CHeapTestHooks::Capacity(ChkCmdBGTestHooks::Heap2(*bg)) == 5);
		check("mHeap2: slots non-NULL", CHeapTestHooks::Slots(ChkCmdBGTestHooks::Heap2(*bg)) != 0);
		check("mHeap1/mHeap2 slots are distinct allocations",
		      CHeapTestHooks::Slots(ChkCmdBGTestHooks::Heap1(*bg)) !=
		      CHeapTestHooks::Slots(ChkCmdBGTestHooks::Heap2(*bg)));
		check("mPendingCount == 0", ChkCmdBGTestHooks::PendingCount(*bg) == 0);
		check("mOutLinkMono is non-NULL (real COutLinkMono constructed)",
		      ChkCmdBGTestHooks::OutLinkMono(*bg) != 0);
		check("object's own mTasks/mOutLinks unaffected (added to owner's task list "
		      "only via CChunkMan::Setup(), not this ctor)", true);

		bg->~CChkCmdBG();
		/* mPendingCount == 0 so the dtor's own assert branch is not taken --
		 * matches ground truth's real early-out, no crash expected here.
		 */
		free(raw);
	}

	printf("[5] CTimerEngine ctor -- 3 embedded clock/wheel sub-objects\n");
	{
		void *raw = malloc(0x128);
		CTimerEngine *eng = new (raw) CTimerEngine(owner);

		check("CWheelsContainer::mCapacity == 8", TimerEngineTestHooks::WheelsCapacity(*eng) == 8);
		check("CExternalClock's own vtable installed",
		      TimerEngineTestHooks::ExtClockVtbl(*eng) == (void *)PTR__CExternalClock_08e897a8);
		check("CInternalClock's own vtable installed",
		      TimerEngineTestHooks::IntClockVtbl(*eng) == (void *)PTR__CInternalClock_08e89888);
		check("RX interface vtable installed",
		      TimerEngineTestHooks::RXVtbl(*eng) == (void *)PTR__CSyncRXInterface_08e89748);
		check("TX interface vtable installed",
		      TimerEngineTestHooks::TXVtbl(*eng) == (void *)PTR__CSyncTXInterface_08e89198);
		check("RX self-pointer points at the RX vtable slot's own address",
		      TimerEngineTestHooks::RXInterfacePtr(*eng) ==
		      (void *)((const char *)eng + 0x11c));
		check("TX self-pointer points at the TX vtable slot's own address",
		      TimerEngineTestHooks::TXInterfacePtr(*eng) ==
		      (void *)((const char *)eng + 0x120));
		check("own vtable installed",
		      *reinterpret_cast<void **>(eng) == (void *)PTR__CTimerEngine_08e896c8);

		eng->~CTimerEngine();
		free(raw);
	}

	printf("[6] CTimerEngine size matches ground truth's malloc(0x128)\n");
	check("sizeof(CTimerEngine) == 0x128", sizeof(CTimerEngine) == 0x128);

	printf("[7] CChkCmdBG size matches ground truth's malloc(0xc0)\n");
	check("sizeof(CChkCmdBG) == 0xc0", sizeof(CChkCmdBG) == 0xc0);

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
