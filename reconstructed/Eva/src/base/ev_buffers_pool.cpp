/*
 * ev_buffers_pool.cpp  -  see include/ev_buffers_pool.h.
 */

#include "ev_buffers_pool.h"

#include <cstring>

namespace {

const int kSmallChunkSize  = 0x18;  /* 24: 4-byte header + 20 usable, capacity 0x10 */
const int kSmallChunkCap   = 0x10;
const int kSmallArenaBytes = 0x1800; /* 256 chunks */
const int kSmallLinkOffset = 0x14;

const int kMediumChunkSize  = 0x88; /* 136: 4-byte header + 132 usable, capacity 0x80 */
const int kMediumChunkCap   = 0x80;
const int kMediumArenaBytes = 0x4400; /* 128 chunks */
const int kMediumLinkOffset = 0x84;

/* Chunk header helpers -- see ev_buffers_pool.h's own header comment for the byte
 * layout. `payload` is what Alloc()/Lock() return (header+4).
 */
inline unsigned char *HeaderOf(void *payload) { return static_cast<unsigned char *>(payload) - 4; }
inline unsigned char &ClassByteOf(unsigned char *hdr) { return hdr[0]; }
inline unsigned char &StateByteOf(unsigned char *hdr) { return hdr[1]; }
inline unsigned short &FallbackSizeOf(unsigned char *hdr)
{
	return *reinterpret_cast<unsigned short *>(hdr + 2);
}
inline void *&LinkOf(unsigned char *hdr, int linkOffset)
{
	return *reinterpret_cast<void **>(hdr + linkOffset);
}

} // namespace

CEvBuffersPool::CEvBuffersPool()
	: mAllocCount(0), mSmallPoolBase(0), mSmallFreeList(0), mMediumPoolBase(0),
	  mMediumFreeList(0)
{
	mSmallPoolBase = new unsigned char[kSmallArenaBytes];
	for (int off = 0; off < kSmallArenaBytes; off += kSmallChunkSize) {
		unsigned char *hdr = mSmallPoolBase + off;
		ClassByteOf(hdr) = 0;
		StateByteOf(hdr) = 0;
		LinkOf(hdr, kSmallLinkOffset) = 0;
	}

	mMediumPoolBase = new unsigned char[kMediumArenaBytes];
	for (int off = 0; off < kMediumArenaBytes; off += kMediumChunkSize) {
		unsigned char *hdr = mMediumPoolBase + off;
		ClassByteOf(hdr) = 1;
		StateByteOf(hdr) = 1; /* real ctor's own init byte for tier-1 chunks */
		LinkOf(hdr, kMediumLinkOffset) = 0;
	}

	/* Link every chunk in each arena into its own singly-linked free list,
	 * head = arena base (first chunk), matching the real ctor's own two
	 * separate linking passes exactly (only the ORDER differs -- functionally
	 * identical free lists either way, since nothing depends on allocation
	 * order among freshly-constructed chunks).
	 */
	for (int off = 0; off + kSmallChunkSize < kSmallArenaBytes; off += kSmallChunkSize) {
		unsigned char *hdr = mSmallPoolBase + off;
		LinkOf(hdr, kSmallLinkOffset) = mSmallPoolBase + off + kSmallChunkSize;
	}
	mSmallFreeList = mSmallPoolBase;

	for (int off = 0; off + kMediumChunkSize < kMediumArenaBytes; off += kMediumChunkSize) {
		unsigned char *hdr = mMediumPoolBase + off;
		LinkOf(hdr, kMediumLinkOffset) = mMediumPoolBase + off + kMediumChunkSize;
	}
	mMediumFreeList = mMediumPoolBase;
}

CEvBuffersPool::~CEvBuffersPool()
{
	/* Real dtor never frees either arena (this pool is a static-lifetime
	 * singleton, `CEvent::sm_oEvBuffersPool` -- process teardown reclaims the
	 * memory). Not reproduced here for the same reason.
	 */
}

void *CEvBuffersPool::Alloc(int size)
{
	mAllocCount++;

	unsigned char *hdr = 0;
	if (size <= kSmallChunkCap && mSmallFreeList) {
		hdr = static_cast<unsigned char *>(mSmallFreeList);
		mSmallFreeList = LinkOf(hdr, kSmallLinkOffset);
	} else if (size <= kMediumChunkCap && mMediumFreeList) {
		hdr = static_cast<unsigned char *>(mMediumFreeList);
		mMediumFreeList = LinkOf(hdr, kMediumLinkOffset);
	} else {
		/* Real code logs a soft "pool exhausted"/"oversized" diagnostic here
		 * (Api vtable slot 0x90) before falling back to the heap -- omitted,
		 * same non-enforcing-log convention as every other Api+0x90/+0x94
		 * call site in this class (see header comment).
		 */
		hdr = new unsigned char[size + 4];
		ClassByteOf(hdr) = 2;
		FallbackSizeOf(hdr) = static_cast<unsigned short>(size);
	}

	StateByteOf(hdr) = 0x81; /* allocated, refcount 1 */
	return hdr + 4;
}

void CEvBuffersPool::Free(void *p)
{
	if (!p)
		return; /* real code logs then still derefs NULL -- unreachable in
		         * practice, see header comment; guarded here instead. */

	unsigned char *hdr = HeaderOf(p);
	unsigned char st = static_cast<unsigned char>(StateByteOf(hdr) - 1);
	StateByteOf(hdr) = st;

	/* bit 0x40 ("already fully released") soft-assert case omitted -- logged
	 * only in ground truth, then falls through to the same 0x3f check below.
	 */
	if (st & 0x3f)
		return; /* other references still outstanding */

	switch (ClassByteOf(hdr)) {
	case 0:
		LinkOf(hdr, kSmallLinkOffset) = mSmallFreeList;
		mSmallFreeList = hdr;
		mAllocCount--;
		break;
	case 1:
		LinkOf(hdr, kMediumLinkOffset) = mMediumFreeList;
		mMediumFreeList = hdr;
		mAllocCount--;
		break;
	case 2:
		mAllocCount--;
		delete[] hdr;
		break;
	default:
		/* Real code logs an "unknown class" diagnostic then re-dispatches
		 * through this same 3-way switch -- unreachable in practice (Alloc()
		 * only ever produces classes 0/1/2), omitted.
		 */
		break;
	}
}

void *CEvBuffersPool::Lock(void *p)
{
	if (!p)
		return 0; /* real code logs then continues as if a valid chunk were
		           * found -- unreachable in practice, same category as
		           * Free()'s own NULL handling. */

	unsigned char *hdr = HeaderOf(p);
	unsigned char st = StateByteOf(hdr);

	if ((st & 0x3f) == 1) {
		/* Sole owner -- lock in place, same pointer. */
		StateByteOf(hdr) = static_cast<unsigned char>(st | 0x80);
		return p;
	}

	/* Shared -- release one reference on the old chunk, then duplicate into a
	 * fresh one of the same size class (recovering the old byte count).
	 */
	StateByteOf(hdr) = static_cast<unsigned char>(st - 1);
	/* bit 0x40 soft-assert case omitted here too, same reasoning. */

	unsigned char cls = ClassByteOf(hdr);
	int oldSize;
	switch (cls) {
	case 0: oldSize = kSmallChunkCap; break;
	case 1: oldSize = kMediumChunkCap; break;
	default: oldSize = FallbackSizeOf(hdr); break;
	}

	void *fresh = Alloc(oldSize);
	std::memcpy(fresh, p, static_cast<size_t>(oldSize));
	return fresh;
}

unsigned long CEvBuffersPool::PostKernelDestructor(unsigned long)
{
	return 0; /* Tier B -- see header comment. */
}
