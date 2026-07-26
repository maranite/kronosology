/*
 * test_ev_buffers_pool.cpp  -  host-side known-answer test for CEvBuffersPool
 * (src/base/ev_buffers_pool.cpp, Stage 6 breadth sweep, 2026-07-25 follow-up pass).
 *
 * Exercises the real allocator end to end: small-pool / medium-pool / heap-fallback
 * size routing, refcounted Free() (only actually returns a chunk once its own
 * reference count reaches 0), and Lock()'s copy-on-write duplication (sole owner ->
 * same pointer; shared -> a fresh chunk with the old payload copied across).
 */

#include <cstdio>
#include <cstring>

#include "ev_buffers_pool.h"

/* Friend hook -- reads mAllocCount for a coarse end-to-end leak check. Same
 * convention as client_comm_server.h's own ClientCommServerTestHooks.
 */
struct EvBuffersPoolTestHooks {
	static int AllocCount(const CEvBuffersPool &pool) { return pool.mAllocCount; }
	static void *SmallBase(const CEvBuffersPool &pool) { return pool.mSmallPoolBase; }
	static void *MediumBase(const CEvBuffersPool &pool) { return pool.mMediumPoolBase; }
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	CEvBuffersPool pool;

	/* --- Alloc() size-class routing --- */
	{
		void *small = pool.Alloc(0x10); /* exactly the small-pool cap */
		void *medium = pool.Alloc(0x80); /* exactly the medium-pool cap */
		void *big = pool.Alloc(0x100); /* heap fallback */
		check("Alloc(0x10) non-null", small != 0);
		check("Alloc(0x80) non-null", medium != 0);
		check("Alloc(0x100) non-null", big != 0);
		check("small/medium/big are distinct pointers",
		      small != medium && medium != big && small != big);

		/* Writing the full requested capacity must not corrupt neighbouring
		 * pool metadata -- alloc a second small chunk right after and confirm
		 * it's still independently usable.
		 */
		std::memset(small, 0xaa, 0x10);
		std::memset(medium, 0xbb, 0x80);
		std::memset(big, 0xcc, 0x100);
		void *small2 = pool.Alloc(0x08);
		std::memset(small2, 0x55, 0x08);
		check("first small chunk unaffected by a second alloc+write",
		      static_cast<unsigned char *>(small)[0] == 0xaa &&
		          static_cast<unsigned char *>(small)[0x0f] == 0xaa);
		check("second small chunk holds its own data",
		      static_cast<unsigned char *>(small2)[0] == 0x55 &&
		          static_cast<unsigned char *>(small2)[7] == 0x55);

		pool.Free(small);
		pool.Free(medium);
		pool.Free(big);
		pool.Free(small2);
	}

	/* --- Free() re-issues freed chunks (freelist actually gets reused) --- */
	{
		void *a = pool.Alloc(0x08);
		pool.Free(a);
		void *b = pool.Alloc(0x08);
		check("Free()'d small chunk is reissued by the next same-class Alloc()",
		      a == b);
		pool.Free(b);
	}
	{
		void *a = pool.Alloc(0x40);
		pool.Free(a);
		void *b = pool.Alloc(0x40);
		check("Free()'d medium chunk is reissued by the next same-class Alloc()",
		      a == b);
		pool.Free(b);
	}

	/* --- Lock(): sole owner -> same pointer --- */
	{
		void *p = pool.Alloc(0x08);
		void *locked = pool.Lock(p);
		check("Lock() on a sole-owner (refcount 1) chunk returns the same pointer",
		      locked == p);
		pool.Free(locked);
	}

	/* --- Lock(): shared (refcount > 1) -> copy-on-write duplicate --- */
	{
		void *p = pool.Alloc(0x08);
		std::memset(p, 0x77, 0x08);
		/* Simulate a second outstanding reference the way ground truth's own
		 * code represents one: bump the low 6 bits of the chunk's state byte
		 * (at payload-3) -- the same raw header convention
		 * CClientCommServer's own ctor/TXData() use directly, not a
		 * CEvBuffersPool method (see ev_buffers_pool.h's header comment).
		 */
		static_cast<unsigned char *>(p)[-3] += 1;
		void *locked = pool.Lock(p);
		check("Lock() on a shared (refcount>1) chunk returns a DIFFERENT pointer",
		      locked != p);
		check("Lock()'s duplicate carries the old payload across (copy-on-write)",
		      std::memcmp(locked, p, 0x08) == 0);
		pool.Free(p);      /* drops the last (simulated + real) ref on the OLD chunk */
		pool.Free(locked); /* releases the fresh, sole-owner duplicate */
	}

	/* --- Lock(): NULL is a tolerated no-op --- */
	{
		check("Lock(NULL) returns NULL", pool.Lock(0) == 0);
	}

	/* --- Free(): NULL is a tolerated no-op (does not crash) --- */
	{
		pool.Free(0);
		check("Free(NULL) does not crash", true);
	}

	/* --- Many small allocations exhaust the free list and fall back to the
	 * medium pool, then the heap, without crashing or aliasing.
	 */
	{
		const int kCount = 300; /* > 256 small-pool chunks */
		void *ptrs[kCount];
		for (int i = 0; i < kCount; i++) {
			ptrs[i] = pool.Alloc(0x08);
			static_cast<unsigned char *>(ptrs[i])[0] = static_cast<unsigned char>(i);
		}
		bool allDistinct = true;
		for (int i = 0; i < kCount && allDistinct; i++)
			for (int j = i + 1; j < kCount; j++)
				if (ptrs[i] == ptrs[j]) { allDistinct = false; break; }
		check("300 small allocs (> 256-chunk small pool) are all distinct pointers",
		      allDistinct);
		bool allIntact = true;
		for (int i = 0; i < kCount; i++)
			if (static_cast<unsigned char *>(ptrs[i])[0] != static_cast<unsigned char>(i))
				allIntact = false;
		check("all 300 allocations retain their own written byte", allIntact);
		for (int i = 0; i < kCount; i++)
			pool.Free(ptrs[i]);
	}

	check("mAllocCount returns to 0 after every alloc above was freed (no leaks)",
	      EvBuffersPoolTestHooks::AllocCount(pool) == 0);

	/* --- PostKernelDestructor(): real body, own pool, own scope --- */
	{
		CEvBuffersPool teardownPool;
		void *smallBefore = EvBuffersPoolTestHooks::SmallBase(teardownPool);
		void *mediumBefore = EvBuffersPoolTestHooks::MediumBase(teardownPool);
		check("PostKernelDestructor: arenas allocated before call",
		      smallBefore != 0 && mediumBefore != 0);

		unsigned long rc = teardownPool.PostKernelDestructor(0);
		check("PostKernelDestructor returns 0", rc == 0);

		/* Real ground truth: delete[]s both arenas but never nulls the
		 * pointer fields (see header comment) -- confirm the pointer VALUES
		 * are unchanged (not re-read through, which would be a
		 * use-after-free); the real dtor is also a documented no-op so this
		 * scope exit does not double-free.
		 */
		check("PostKernelDestructor does not null mSmallPoolBase (ground-truth quirk)",
		      EvBuffersPoolTestHooks::SmallBase(teardownPool) == smallBefore);
		check("PostKernelDestructor does not null mMediumPoolBase (ground-truth quirk)",
		      EvBuffersPoolTestHooks::MediumBase(teardownPool) == mediumBefore);
	}

	printf(g_fail ? "%d check(s) FAILED\n" : "all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
