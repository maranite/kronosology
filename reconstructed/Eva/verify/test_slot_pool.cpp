/*
 * test_slot_pool.cpp  -  host-side known-answer test for CSlotPool (slot_pool.h/.cpp)
 * and its CSlotStateFree singleton dependency, reconstructed 2026-07-27 (CPool/CSlotPool
 * follow-up to the same-day fresh broad-survey pass, see slot_pool.h's own file header).
 *
 * Exercises: ctor layout (mCapacity stored, mArray/mFreeListHead NULL until
 * PreKernelConstructor()), PreKernelConstructor()'s real array build + free-list linking
 * (every slot's mNext points at the next slot except the last, which is NULL; every
 * slot's mIndex is its own 0-based position; every slot's mState points at the real
 * CSlotStateFree singleton), and PostKernelDestructor()'s teardown (frees + re-NULLs
 * mArray, safe to call twice unlike CPool's own PostKernelDestructor()).
 */

#include <cstdio>
#include <new>

#include "slot_pool.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main(void)
{
	printf("CSlotPool known-answer test\n");
	printf("============================\n");

	printf("[1] Real object size is 0x10 (16) bytes\n");
	check("sizeof(CSlotPool) == 0x10", sizeof(CSlotPool) == 0x10);

	printf("[2] Real SSlot record size is 0x24 (36) bytes\n");
	check("sizeof(CSlotPool::SSlot) == 0x24", sizeof(CSlotPool::SSlot) == 0x24);

	printf("[3] CSlotStateFree::s_oInstance is a real, already-constructed global "
	       "(GetStateName_debug() returns the real ground-truth string)\n");
	check("g_oSlotStateFreeInstance.GetStateName_debug() == \"CSlotStateFree\"",
	      __builtin_strcmp(g_oSlotStateFreeInstance.GetStateName_debug(), "CSlotStateFree") == 0);

	printf("[4] Ctor stores capacity, does NOT allocate yet\n");
	static unsigned char raw[sizeof(CSlotPool)];
	const unsigned kCapacity = 8;
	CSlotPool *pool = new (raw) CSlotPool(kCapacity);
	unsigned char *b = raw;
	check("mCapacity (+0x4) == 8", *(unsigned *)(b + 4) == kCapacity);
	check("mArray (+0x8) == NULL before PreKernelConstructor()", *(void **)(b + 8) == 0);
	check("mFreeListHead (+0xc) == NULL before PreKernelConstructor()",
	      *(void **)(b + 0xc) == 0);

	printf("[5] PreKernelConstructor() builds the array + free list\n");
	int rc = pool->PreKernelConstructor(0);
	check("PreKernelConstructor() returns 0", rc == 0);
	CSlotPool::SSlot *array = *(CSlotPool::SSlot **)(b + 8);
	check("mArray is now non-NULL", array != 0);
	check("mFreeListHead == mArray (element 0 is the initial free-list head)",
	      *(void **)(b + 0xc) == (void *)array);

	printf("[6] Every slot's own real field values match ground truth's own "
	       "PreKernelConstructor() writes\n");
	bool allLinked = true, allIndexed = true, allStated = true;
	for (unsigned i = 0; i < kCapacity; i++) {
		if (array[i].mState != (void *)&g_oSlotStateFreeInstance)
			allStated = false;
		if (array[i].mIndex != i)
			allIndexed = false;
		CSlotPool::SSlot *expectedNext = (i + 1 < kCapacity) ? &array[i + 1] : 0;
		if (array[i].mNext != expectedNext)
			allLinked = false;
	}
	check("every slot's mState points at the real CSlotStateFree singleton", allStated);
	check("every slot's mIndex is its own 0-based array position", allIndexed);
	check("every slot's mNext free-list link matches (last slot's mNext == NULL)",
	      allLinked);

	printf("[7] PostKernelDestructor() frees + re-NULLs mArray, and is safe to call "
	       "twice (unlike CPool's own PostKernelDestructor())\n");
	rc = pool->PostKernelDestructor(0);
	check("PostKernelDestructor() returns 0", rc == 0);
	check("mArray is now NULL", *(void **)(b + 8) == 0);
	rc = pool->PostKernelDestructor(0);
	check("second PostKernelDestructor() call also returns 0, no crash", rc == 0);

	pool->~CSlotPool();

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
