/*
 * ev_buffers_pool.h  -  CEvBuffersPool, the fixed-size refcounted slab allocator that
 * backs every CEvent/CLinkedEvent payload buffer in this binary -- Stage 6 breadth
 * sweep, 2026-07-25, reconstructed to unblock CClientCommServer (client_comm_server.h)
 * whose own constructor is its single largest caller in this project's traced code so
 * far. Transcribed directly from objdump -dr -M intel disassembly of the real
 * Eva binary's CEvBuffersPool::{CEvBuffersPool,Alloc,Free,Lock}@0807f100/f400/f4b0/f660
 * (Decomp/EVA_Decomp/Eva, ground truth per PLAN.md) -- no Ghidra decompile needed, the
 * asm is small and mechanical enough to transcribe directly, index-by-index.
 *
 * WHAT THIS IS: a two-tier fixed-chunk-size slab allocator (CGlobalObjectBase-derived,
 * confirmed via the ctor's own CGlobalObjectBase::CGlobalObjectBase(this) placement
 * call + real vtable/typeinfo at .rodata+0x08e82220/+0x08e82240) with a heap-`new[]`
 * fallback for anything too big for either tier:
 *
 *   tier 0 ("small")  -- one 0x1800-byte arena (`new unsigned char[0x1800]`), carved
 *     into 256 chunks of 0x18 (24) bytes each: 4-byte header + 20 bytes usable, but
 *     only ever handed out for `size <= 0x10` (16) requests -- the real ctor's own
 *     per-chunk init loop writes a placeholder "next" pointer at header offset 0x14
 *     (i.e. the chunk's own LAST 4 bytes double as the free-list link while unused,
 *     the classic embedded-freelist trick), then a second pass links every chunk into
 *     a singly-linked free list via that same field.
 *   tier 1 ("medium") -- one 0x4400-byte arena, 128 chunks of 0x88 (136) bytes each
 *     (4-byte header + 132 usable), handed out for `0x10 < size <= 0x80` (128), same
 *     shape/free-list convention as tier 0 but with the link field at header offset
 *     0x84 instead of 0x14 (matches the bigger chunk size).
 *   fallback -- `size > 0x80`, or either tier's free list is exhausted: a direct
 *     `new unsigned char[size + 4]` heap chunk, header stores the actual requested
 *     size (a 16-bit word at header+2) so Free()/Lock() can recover it later --
 *     everything else in this class treats a fallback chunk exactly like a pool
 *     chunk except for how it's returned (operator delete[] instead of a free-list
 *     relink) and how many bytes to preserve across a Lock()-triggered copy.
 *
 * CHUNK HEADER (4 bytes, immediately before every pointer this class ever returns --
 * i.e. `((unsigned char*)returned_ptr) - 4`, confirmed by every caller's own `[ptr-3]`/
 * `[ptr-4]` accesses, including CClientCommServer's OWN ctor which pokes this same
 * header directly rather than going through a method -- this is an intentionally raw,
 * not-fully-encapsulated protocol in the real binary, reproduced faithfully here):
 *   +0x00  unsigned char  size class -- 0 (small pool), 1 (medium pool), 2 (heap
 *                          fallback). Never changes once Alloc() sets it.
 *   +0x01  unsigned char  state byte -- low 6 bits (mask 0x3f) are a REFERENCE COUNT
 *                          (Alloc() always sets this to 1); bit 6 (0x40) and bit 7
 *                          (0x80) are set/tested by Lock()/Free() but every real
 *                          branch that inspects them only logs a soft, non-aborting
 *                          diagnostic (`Api` vtable slot 0x94/0x90 -- see
 *                          client_comm_server.h's own established "soft assert, dead
 *                          in practice" precedent, same call shape, same
 *                          non-enforcing behavior every single call site) and then
 *                          falls straight through to the same code that would have
 *                          run anyway -- omitted here for that reason, not guessed
 *                          away.
 *   +0x02  unsigned short  heap-fallback-only: the actual requested size in bytes.
 *                          Unused (left as whatever the pool's own ctor zeroed it to)
 *                          for tier-0/1 chunks -- their capacity is implied by class.
 *   (+0x04  usable payload starts here -- this is the pointer Alloc()/Lock() return)
 *
 * mAllocCount (+0x04 of `this`) is a pool-wide "chunks currently checked out" counter,
 * NOT a per-chunk refcount (that's the header's own low 6 bits) -- incremented once per
 * Alloc() call and once per Lock()-triggered "shared, must duplicate" promotion,
 * decremented only when Free() actually returns a chunk to its free list or deletes a
 * fallback chunk (i.e. when the header's own refcount reaches 0) -- confirmed by
 * reading exactly where each `add/sub DWORD PTR [this+4]` instruction sits relative to
 * the free-list-relink code, not guessed from the name.
 */

#ifndef EV_BUFFERS_POOL_H
#define EV_BUFFERS_POOL_H

#include "global_object_base.h"

class CEvBuffersPool : public CGlobalObjectBase {
public:
	/* .text+0x0807f100, 740 bytes. Builds both arenas + both free lists. */
	CEvBuffersPool();

	/* .text+0x0807f0b0 (D1, 23B) / +0x0807f0d0 (D0, 37B, adds free(this) --
	 * never exercised, this pool is a static-lifetime singleton, installed for
	 * vtable shape-fidelity only, same convention as CGlobalObjectBase's own
	 * D0/D1 split).
	 */
	~CEvBuffersPool();

	/* .text+0x0807f400, 167 bytes. Returns a `size+4`-byte chunk's payload
	 * pointer (i.e. header+4), refcount preset to 1. `size` must be <= 0xff in
	 * practice (every real caller passes an `unsigned char`-derived value) but
	 * the real parameter is a plain `int`.
	 */
	void *Alloc(int size);

	/* .text+0x0807f4b0, 401 bytes. Decrements the chunk's own refcount; only
	 * when it reaches 0 does the chunk actually return to its free list (or,
	 * for a heap-fallback chunk, get `delete[]`d). NULL is a real, silently-
	 * tolerated no-op here (ground truth logs a diagnostic and would then
	 * dereference the null pointer -- an unreachable-in-practice latent bug,
	 * see header comment above; this reconstruction just returns early, same
	 * observable behavior for every real caller since none of them pass NULL).
	 */
	void Free(void *p);

	/* .text+0x0807f660, 487 bytes. "Acquire exclusive access" -- if the
	 * chunk's refcount is already 1 (sole owner), marks it locked in place
	 * (sets header bit 0x80) and returns the SAME pointer. Otherwise releases
	 * one reference on the OLD chunk and allocates+returns a FRESH chunk of
	 * the same size class (or the same heap-fallback size, recovered from the
	 * old header), memcpy'ing the old payload across -- i.e. copy-on-write
	 * duplication when shared, in-place lock when not. NULL in is a real,
	 * tolerated no-op returning NULL (same "unreachable in practice" category
	 * as Free()'s own null handling).
	 */
	void *Lock(void *p);

	/* .text+0x0807f040, 97 bytes. Overrides CGlobalObjectBase's own no-op
	 * PostKernelDestructor hook (vtable slot +0x14) -- real body walks the
	 * medium-pool arena's own already-linked free list and, for every chunk
	 * NOT on it (i.e. still checked out), does nothing observable this pass
	 * needs (a debug leak-report loop, gated on data this reconstruction has
	 * no live caller for -- CKernel's own global-object teardown pass is not
	 * reconstructed, see global_object_base.h). Given as a real, empty-bodied
	 * override for vtable-slot fidelity; Tier B.
	 */
	unsigned long PostKernelDestructor(unsigned long);

private:
	int            mAllocCount;      /* +0x04 */
	unsigned char *mSmallPoolBase;   /* +0x08, 0x1800 bytes, 256 x 24B chunks */
	void          *mSmallFreeList;   /* +0x0c */
	unsigned char *mMediumPoolBase;  /* +0x10, 0x4400 bytes, 128 x 136B chunks */
	void          *mMediumFreeList;  /* +0x14 */

	friend struct EvBuffersPoolTestHooks;
};

#endif /* EV_BUFFERS_POOL_H */
