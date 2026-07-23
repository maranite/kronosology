/*
 * omega_ptr_array.cpp  -  see include/omega_ptr_array.h.
 *
 * Transcribed from the Ghidra decompile export:
 *   COmegaPtrArray::COmegaPtrArray()  .text+0x080a6be0, 46 bytes
 *   COmegaPtrArray::Destroy()         .text+0x080a6ca0, 224 bytes
 *   COmegaPtrArray::FindIndex()       .text+0x080a7200, 227 bytes
 *   COmegaPtrArray::RemoveAtIndex()   .text+0x080a6f20, 331 bytes
 *   COmegaPtrArray::Shrink()          .text+0x080a7310, 356 bytes
 *
 * All 5 real bodies are GCC's classic 4-or-8-way Duff's-device loop unrolling over a
 * flat array walk -- collapsed to plain loops here (same license as eva_main.cpp's
 * inlined-strncmp replacement and lcd_control.cpp's byte-compare collapse); the
 * unrolled and collapsed forms are semantically identical, verified index-by-index
 * against each real decompile while writing this.
 */

#include "omega_ptr_array.h"

#include <cstdlib>
#include <cstring>

/* Real vtable-slot-8 "free element" callback dispatch -- same
 * `(**(code**)(*this+8))(this, elem)` idiom used throughout the project for classes
 * whose real vtable layout isn't a reconstructed C++ virtual table (see
 * omega_ptr_array.h's header comment on why mVtbl stays a raw void*).
 */
namespace {
typedef void (*FreeElementFn)(void *self, void *elem);

inline void CallFreeElement(void *self, void *elem)
{
	void *vtbl = *(void **)self;
	FreeElementFn fn = *(FreeElementFn *)((char *)vtbl + 8);
	fn(self, elem);
}
} // namespace

COmegaPtrArray::COmegaPtrArray()
{
	extern void *PTR__COmegaPtrArray_08e80be0[];
	mVtbl = (void *)PTR__COmegaPtrArray_08e80be0;
	mUnknown04 = 1;
	mCapacity = 0;
	mCount = 0;
	mGrowBy = 1;
	mArray = 0;
}

COmegaPtrArray::COmegaPtrArray(int growBy, int initialCapacity, int ownFlag)
{
	extern void *PTR__COmegaPtrArray_08e80be0[];
	mVtbl = (void *)PTR__COmegaPtrArray_08e80be0;
	mUnknown04 = ownFlag;
	mCapacity = initialCapacity;
	mCount = 0;
	mGrowBy = growBy;
	mArray = 0;
	if (initialCapacity != 0)
		mArray = (void **)malloc(initialCapacity * sizeof(void *));
}

int COmegaPtrArray::Add(const void *item)
{
	void **arr;

	if (mCount < mCapacity) {
		arr = mArray;
	} else {
		if (mGrowBy == 0)
			return 0x7fffffff;
		if (0x7fffffff - mGrowBy <= mCapacity)
			return 0x7fffffff;

		int newCapacity = mCapacity + mGrowBy;
		void **oldArray = mArray;
		mCapacity = newCapacity;
		void **newArray = (void **)malloc(newCapacity * sizeof(void *));
		mArray = newArray;

		if (oldArray != 0) {
			for (int i = 0; i < mCount; i++)
				newArray[i] = oldArray[i];
			free(oldArray);
		}
		arr = mArray;
	}

	arr[mCount] = (void *)item;
	return mCount++;
}

void COmegaPtrArray::Destroy()
{
	if (mUnknown04 != 0) {
		while (mCount != 0) {
			mCount--;
			CallFreeElement(this, mArray[mCount]);
		}
	}

	void *oldArray = mArray;
	mCount = 0;
	mCapacity = 0;
	if (oldArray != 0) {
		free(oldArray);
		mArray = 0;
	}
}

unsigned COmegaPtrArray::FindIndex(const void *item) const
{
	for (int i = 0; i < mCount; i++) {
		if (mArray[i] == item)
			return (unsigned)i;
	}
	return 0x7fffffff;
}

void COmegaPtrArray::RemoveAtIndex(unsigned index, int callDtorCallback)
{
	if ((int)index >= mCount)
		return;

	if (callDtorCallback != 0)
		CallFreeElement(this, mArray[index]);

	/* Shift every element past `index` down by one. */
	for (int i = (int)index; i < mCount - 1; i++)
		mArray[i] = mArray[i + 1];

	mCount--;
}

void COmegaPtrArray::Shrink()
{
	if (mCount >= mCapacity)
		return;

	if (mCount == 0) {
		if (mArray != 0)
			free(mArray);
		mArray = 0;
	} else {
		void **oldArray = mArray;
		void **newArray = (void **)malloc(mCount * sizeof(void *));
		memcpy(newArray, oldArray, mCount * sizeof(void *));
		mArray = newArray;
		if (oldArray != 0)
			free(oldArray);
	}

	mCapacity = mCount;
}
