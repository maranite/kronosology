/*
 * pool.h  -  CPool, a fixed-per-element-size chunked memory pool allocator.
 *
 * FOUND 2026-07-27, same fresh broad-survey pass as res_family.h/kernel_death_notifier.h
 * (see that pass's own writeup) -- one of the 4 CGlobalObjectBase-derived classes never
 * touched at all. Deferred that same pass for needing an unmodeled TVector<T,N> template
 * (now real, see tvector.h); reconstructed for real this follow-up pass.
 *
 * CPool's OWNING object (`CSysExSnifferBase::CSampleItem::sm_oPool`, real address
 * ds:0x930a360 -- confirmed by CSysExSniffer::~CSysExSniffer()/CSysExTreeBuilder::
 * ~CSysExTreeBuilder() (.text+0x08182cf0/0x08182d40) both calling `CPool::Free(&sm_oPool,
 * ptr)` directly) is itself a deep, entirely unmodeled subsystem (the SysEx sniffer/tree
 * builder) -- same "owning class out of scope, this class's own construction/destruction
 * lifecycle is not" precedent as CResFamily/CConfigManager::CreateResourceFamilies(). No
 * `extern CPool sm_oPool;` global declared here (its real ctor call-site args, i.e. the
 * real elemSize/initialCount CSampleItem passes, weren't traced this pass).
 *
 * REAL OBJECT LAYOUT (0x24 bytes, confirmed by CPool::CPool()'s/Alloc()'s/Free()'s/
 * Expand()'s own direct field offsets):
 *   +0x00  mVtbl         inherited from CGlobalObjectBase
 *   +0x04  mElementSize  align4(requested per-element payload size) + 8 (an 8-byte
 *                         {ownerPool,freeListNextLink} header goes in front of every
 *                         carved-out element -- see Alloc()/Free())
 *   +0x08  mFreeListHead singly-linked (via each freed element's own header, see below),
 *                         address of the next-to-reuse element's 8-byte header, or NULL
 *   +0x0c  mTotalBytes   running total of every chunk malloc()'d so far; ALSO the byte
 *                         size of the next chunk Alloc() grows when it needs to (i.e.
 *                         pool footprint doubles every time it grows)
 *   +0x10  mChunks       TVector<SPool,1> -- one 12-byte SPool entry per backing chunk
 *                         ever malloc()'d
 *   +0x20  mCursor       SPool* -- last chunk Alloc() successfully carved an element
 *                         from; resumes scanning here next call
 *
 * Element header (8 bytes, immediately BEFORE every pointer Alloc() returns / Free()
 * is passed): word0 = owning CPool* (verified by Free()), word1 = free-list "next" link
 * (0 while the element is live/allocated; Free() soft-asserts and drops the free if this
 * is already non-zero -- a double-free guard). Real ground truth's 2 soft-assert-log
 * branches in Free() (owner mismatch / double-free) are OMITTED here -- same blanket
 * "kernel-side critical-section shim + log-only soft-assert, no control-flow effect,
 * dropped" precedent as limiter_base.cpp/circ_byte_buffer.cpp/task.cpp/module.cpp/
 * out_link.cpp/ev_buffers_pool.cpp (HAL_DisableInterrupts()/HAL_EnableInterrupts()
 * pairs around every malloc/free below are likewise dropped, same precedent).
 */

#ifndef POOL_H
#define POOL_H

#include "global_object_base.h"
#include "tvector.h"

class CPool : public CGlobalObjectBase {
public:
	/* .text+0x080b9210's own real 3-word (12-byte) layout: raw chunk pointer, chunk
	 * byte size, bytes of the chunk already carved into elements ("used"). Public --
	 * Expand()'s real signature takes one by reference.
	 */
	struct SPool {
		void *ptr;
		unsigned size;
		unsigned used;
	};

	/* .text+0x080b92a0, 491 bytes. elemSize = requested per-element payload size
	 * (mElementSize = align4(elemSize)+8, see above); initialCount = requested
	 * element count for the very first backing chunk -- the real first chunk is
	 * actually max(initialCount, floor(1024/mElementSize)+1) elements ("at least
	 * ~1KB worth, or the requested count, whichever is bigger"). Real ground truth's
	 * own malloc-failure soft-assert-and-retry-forever loop is omitted -- same
	 * "malloc failure not specially handled" convention this project already uses
	 * pervasively (e.g. COmegaPtrArray::Add(), omega_ptr_array.cpp).
	 */
	CPool(unsigned elemSize, int initialCount);

	/* .text+0x08182c30 (D1), 43 bytes -- reasserts CPool's own mVtbl, matching this
	 * project's "manual mVtbl field, not a real `virtual`" convention for every
	 * CGlobalObjectBase-derived class. Real ground truth ALSO manually reasserts
	 * mChunks' own TVector vtable field before directly free()ing mChunks' backing
	 * buffer -- equivalent here to simply letting mChunks (a real, ordinary
	 * TVector<SPool,1> member, not manually-dispatched) destruct itself via normal
	 * C++ member-destruction order, which frees the identical buffer. D0 (deleting,
	 * .text+0x08182d90) not given a separate named method -- shape-only in the
	 * vtable array (pool.cpp), same "install-only" precedent as CGlobalObjectBase's
	 * own D0 (global_object_base.h). NOTE: does NOT free any individual chunk's own
	 * memory (mChunks[i].ptr) -- that is PostKernelDestructor()'s real job, below;
	 * faithfully preserved as 2 separate real ground-truth responsibilities.
	 */
	~CPool();

	/* .text+0x080b94c0, 288 bytes. Pops the free list if non-empty; otherwise scans
	 * mChunks from mCursor forward for a chunk with room, carving the next element
	 * out of it; if every known chunk is full, grows a brand new one (byte size ==
	 * mTotalBytes, i.e. doubling the pool's total footprint) and resumes scanning.
	 * Real ground truth's own register/cursor choreography for the "grow, then
	 * resume scanning from the just-appended chunk" case is collapsed to a
	 * behaviorally-identical explicit `mCursor = mChunks.End() - 1` -- same license
	 * this project always takes for GCC-inlined-loop micro-optimizations (see
	 * tvector.h's own header note).
	 */
	void *Alloc();

	/* .text+0x080b9190, 128 bytes. */
	void Free(void *elem);

	/* .text+0x080b9210, 144 bytes -- a real, separate, out-of-line utility.
	 * CPool::CPool()/CPool::Alloc() each hand-inline an equivalent
	 * malloc-and-fill sequence rather than calling this; modeled as a real
	 * standalone method for any other real caller (not traced beyond this file).
	 * Returns false (mallo failed) without touching *out's validity beyond the
	 * fields already written -- matches real ground truth (out.ptr/size/used are
	 * unconditionally written either way, only the return value distinguishes
	 * success).
	 */
	bool Expand(SPool &out, unsigned size);

	/* .text+0x080b9140, 80 bytes -- the real CGlobalObjectBase phase-hook override
	 * that made this class show up in the 2026-07-27 survey. Frees every chunk's
	 * own backing buffer (mChunks[i].ptr) -- but NOT mChunks' own descriptor array
	 * (~CPool()'s job, above). Real ground truth doesn't null out each ptr after
	 * freeing it either (faithfully preserved -- calling this twice would
	 * double-free, exactly like ground truth).
	 */
	int PostKernelDestructor(unsigned long);

private:
	unsigned mElementSize;
	void *mFreeListHead;
	unsigned mTotalBytes;
	TVector<SPool, 1> mChunks;
	SPool *mCursor;
};

#endif /* POOL_H */
