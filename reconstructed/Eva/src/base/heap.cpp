/*
 * heap.cpp  -  see include/heap.h.
 */

#include "heap.h"

struct HeapSlot {
	unsigned int mUnknown0; /* never zeroed by the ctor -- genuinely uninitialized
	                          * in the real binary, preserved as-is. */
	unsigned int mUnknown4; /* zeroed by the ctor's own init loop. */
};

CHeap::CHeap()
	: mCapacity(0x10), mCount(0), mGrowBy(8), mSlots(0)
{
	HeapSlot *slots = new HeapSlot[0x10];
	for (int i = 0; i < 0x10; ++i)
		slots[i].mUnknown4 = 0;
	mSlots = slots;
}

CHeap::CHeap(int growBy, int capacity)
	: mCapacity(capacity), mCount(0), mGrowBy(growBy), mSlots(0)
{
	if (capacity == 0)
		return;

	HeapSlot *slots = new HeapSlot[capacity];
	for (int i = 0; i < capacity; ++i)
		slots[i].mUnknown4 = 0;
	mSlots = slots;
}

CHeap::~CHeap()
{
	if (mSlots)
		delete[] static_cast<HeapSlot *>(mSlots);
}
