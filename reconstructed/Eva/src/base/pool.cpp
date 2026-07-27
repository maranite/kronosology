/*
 * pool.cpp  -  see include/pool.h.
 */

#include "pool.h"

#include <cstdlib>

extern "C" {
void CPool_Dtor(void *self)
{
	((CPool *)self)->~CPool();
}

void CPool_DeletingDtor(void *self)
{
	((CPool *)self)->~CPool();
	free(self);
}

int CPool_PostKernelDestructorVSlot(void *self, unsigned long flags)
{
	return ((CPool *)self)->PostKernelDestructor(flags);
}
} // extern "C"

/* Real 6-slot vtable, confirmed by direct raw-byte read of the real Eva binary at
 * .rodata+0x08e854e8 (vtable base 0x8e854e0, +8 skips the Itanium ABI offset-to-top/RTTI
 * pair). Slot 5 (PostKernelDestructor) is the real override; slots 2-4
 * (PreKernelConstructor/PostKernelConstructor/PreKernelDestructor) stay
 * CGlobalObjectBase's own no-ops -- symbols.csv shows no CPool override for any of them.
 */
extern "C" void *PTR__CPool_08e854e8[6] = {
	(void *)CPool_Dtor,
	(void *)CPool_DeletingDtor,
	(void *)CGlobalObjectBase_PreKernelConstructor,
	(void *)CGlobalObjectBase_PostKernelConstructor,
	(void *)CGlobalObjectBase_PreKernelDestructor,
	(void *)CPool_PostKernelDestructorVSlot,
};

CPool::CPool(unsigned elemSize, int initialCount) : CGlobalObjectBase()
{
	mVtbl = &PTR__CPool_08e854e8[0];

	mFreeListHead = 0;
	mTotalBytes = 0;
	mElementSize = ((elemSize + 3) & ~3u) + 8;

	/* Real: reserve 32 chunk-descriptor slots up front (mChunks.PushBack() below
	 * would grow to the same eventual capacity lazily either way -- this just
	 * avoids the first few reallocations, not externally observable). */
	mChunks.MakeCapacity(32);

	int perKPage = (int)(1024u / mElementSize) + 1;
	int count = (initialCount >= perKPage) ? initialCount : perKPage;
	unsigned chunkBytes = (unsigned)count * mElementSize;

	SPool entry;
	entry.ptr = malloc(chunkBytes);
	entry.size = chunkBytes;
	entry.used = 0;
	mChunks.PushBack(entry);

	mTotalBytes += chunkBytes;
	mCursor = mChunks.Begin();
}

CPool::~CPool()
{
	mVtbl = &PTR__CPool_08e854e8[0];
	/* mChunks (a real TVector<SPool,1> member) destructs itself right after this
	 * body runs, freeing its own backing array -- see file header. */
}

void *CPool::Alloc()
{
	if (mFreeListHead != 0) {
		char *header = (char *)mFreeListHead;
		mFreeListHead = *(void **)(header + 4);
		*(void **)(header + 4) = 0;
		return header + 8;
	}

	for (;;) {
		for (SPool *p = mCursor; p != mChunks.End(); ++p) {
			if (p->used + mElementSize <= p->size) {
				mCursor = p;
				char *header = (char *)p->ptr + p->used;
				p->used += mElementSize;
				*(void **)header = this;
				*(void **)(header + 4) = 0;
				return header + 8;
			}
		}

		/* Real: every known chunk is full -- grow a brand new one (doubling the
		 * pool's total footprint) and resume scanning from it. */
		unsigned newChunkBytes = mTotalBytes;
		SPool entry;
		entry.ptr = malloc(newChunkBytes);
		entry.size = newChunkBytes;
		entry.used = 0;
		mChunks.PushBack(entry);
		mTotalBytes += newChunkBytes;
		mCursor = mChunks.End() - 1;
	}
}

void CPool::Free(void *elem)
{
	if (elem == 0)
		return;

	char *header = (char *)elem - 8;
	void *owner = *(void **)header;
	if (owner != this)
		return; /* real: soft-assert-logs "not owned by this pool" -- omitted */

	void *next = *(void **)(header + 4);
	if (next != 0)
		return; /* real: soft-assert-logs "double free" -- omitted */

	*(void **)(header + 4) = mFreeListHead;
	mFreeListHead = header;
}

bool CPool::Expand(SPool &out, unsigned size)
{
	void *chunk = malloc(size);
	out.ptr = chunk;
	out.size = size;
	out.used = 0;
	if (chunk != 0)
		return true;

	return false; /* real: soft-assert-logs "CPool::Expand - malloc failed" -- omitted */
}

int CPool::PostKernelDestructor(unsigned long)
{
	for (SPool *p = mChunks.Begin(); p != mChunks.End(); ++p) {
		if (p->ptr != 0)
			free(p->ptr);
	}
	return 0;
}
