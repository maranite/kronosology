/* SPDX-License-Identifier: GPL-2.0 */
/*
 * heap_alloc.c - the firmware's shared, general-purpose heap allocator: a
 * segregated free-list allocator (small exact-fit bins up to ~504 bytes,
 * indexed logarithmic "tree" bins beyond that), boundary-tag free chunks
 * with immediate coalescing, a page-granular trim-back-to-OS path, and an
 * sbrk-style break-pointer primitive underneath it all.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-18.
 * NOT anchored by a `__FILE__` string (a full image string search found none
 * for this address range) - attributed here on pure code-shape evidence
 * instead, per the discipline clcdc.c's own header comment already
 * established when it first found and excluded this code:
 *
 *   "the address range immediately following [clcdc.cpp] (0xc0015bf8 onward)
 *   turned out NOT to be clcdc.cpp - it's a generic segregated-free-list
 *   heap allocator (malloc/free/sbrk-style, with free-block coalescing and
 *   size-class binning) plus a C++ object destructor that calls into it,
 *   both shared firmware-wide runtime code, not LCD-specific."
 *
 * That destructor is cobjectmgr.c's own cobjectmgr_object_destroy
 * (@0xc0015bf8) - this file covers everything it and cobjectmgr_free_list_
 * recursive (@0xc0015bc8) call into. Confirmed genuinely shared firmware-
 * wide, not cobjectmgr-specific: heap_malloc's own callers alone span at
 * least 5 unrelated functions well outside any subsystem this project has
 * otherwise anchored (FUN_c001aa74, FUN_c001ba38, FUN_c001c98c,
 * FUN_c001a360, FUN_c00169b0 - none investigated this pass).
 *
 * Architecture note: the code shape (chunk header = size|flags word
 * immediately preceding the user pointer, free chunks' fd/bk overlaid into
 * the SAME words used as small-bin array storage via a "bin pointer acts as
 * a virtual chunk pointer minus 8" trick, an identical treebin-index
 * computation ladder duplicated at three separate call sites, a dedicated
 * single "designated victim" slot using the same bin-sentinel trick, and a
 * top/"wilderness" chunk grown via sbrk on cache miss) matches the
 * well-known Doug Lea malloc (dlmalloc) family closely enough that this is
 * almost certainly a compiled dlmalloc derivative, not an original design -
 * noted as an aid to reading this file, NOT as license to substitute real
 * dlmalloc source: everything below is grounded in what Ghidra actually
 * shows for THIS binary, not copied from any reference implementation.
 *
 * Per this project's own established practice for code this dense (see
 * clcdc.c's clcdc_draw_edge/clcdc_blit_glyph, and cobjectmgr.c's own
 * cobjectmgr_handle_type_b), the two densest functions - heap_malloc and
 * heap_free's own internal bin-search/coalescing logic - are documented
 * structurally rather than transcribed line-for-line: the risk of a subtle
 * off-by-one in this much raw pointer arithmetic, with no way to verify
 * against real hardware, outweighs the value of a literal (and possibly
 * subtly wrong) transcription. The simpler, fully-traced support functions
 * (heap_sbrk, heap_trim, the lock/unlock stubs) ARE transcribed in full.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  heap_lock / heap_unlock (FUN_c0016894 / FUN_c0016898) - bracket every
 *  allocator entry point (heap_malloc, heap_free, heap_trim all call both,
 *  in order, around their real work). @0xc0016894, @0xc0016898.
 *
 *  Real bodies are, literally, empty (`{ return; }`) - no observable
 *  side effect in this decompile. Kept as real (if no-op) functions rather
 *  than removed, since every caller genuinely brackets its work with both:
 *  most plausibly a critical-section (interrupt disable/enable) pair that
 *  Ghidra fully collapsed away because this build has no actual contention
 *  to protect against (a single-core, largely single-threaded firmware), or
 *  a lock primitive stubbed out entirely for this configuration. Not
 *  resolved further this pass.
 * ------------------------------------------------------------------------- */
void heap_lock(void)	/* FUN_c0016894 */
{
}

void heap_unlock(void)	/* FUN_c0016898 */
{
}

/* ------------------------------------------------------------------------- *
 *  heap_sbrk (FUN_c0015d4c) - classic sbrk(): rounds the requested increment
 *  up to 4 bytes, lazily initializes the break pointer to a fixed heap-base
 *  constant on first use, and either commits the new break (returning the
 *  OLD break, i.e. the base of the newly available region) or, if it would
 *  exceed the fixed heap-end constant, sets an errno-style global to 0xc
 *  (ENOMEM) and returns (uint32_t)-1. Fully transcribed - simple, and every
 *  branch is unambiguous in the real decompile. @0xc0015d4c.
 *
 *  `unused_handle` is dead - never read in the real body, the same
 *  "phantom forwarded parameter" pattern already found repeatedly elsewhere
 *  in this project (cdix4192.c, eva_board_main.c, cobjectmgr.c's own
 *  cobjectmgr_notify_host).
 * ------------------------------------------------------------------------- */
extern uint32_t  heap_break;		/* *DAT_c0015dac: current program-break value */
extern uint32_t  heap_errno;		/* *DAT_c0015db0: cleared on entry, set to 0xc (ENOMEM) on failure */
extern uint32_t  heap_base_const;	/* DAT_c0015db4: fixed heap base, used to lazily init heap_break */
extern uint32_t  heap_end_const;	/* DAT_c0015db8: fixed heap end - heap_break may never exceed this */

uint32_t heap_sbrk(void *unused_handle, int32_t increment)	/* FUN_c0015d4c */
{
	uint32_t old_break, new_break, rounded;

	(void)unused_handle;

	heap_errno = 0;
	rounded = ((uint32_t)increment + 3U) & 0xfffffffcU;

	if (heap_break == 0)
		heap_break = heap_base_const;

	old_break = heap_break;
	new_break = old_break + rounded;

	if (new_break <= heap_end_const && rounded <= new_break) {
		heap_break = new_break;
		return old_break;
	}

	heap_errno = 0xc;
	return 0xffffffffU;
}

/* ------------------------------------------------------------------------- *
 *  heap_trim (FUN_c0015e2c) - shrinks the heap's top ("wilderness") chunk
 *  back to the OS via a negative heap_sbrk() when more than one page
 *  (0x1000) of free space has accumulated at the very top of the heap.
 *  Fully transcribed - moderate density, but a single linear control path
 *  with no bin-array walking. @0xc0015e2c.
 *
 *  `heap_state` here is the SAME global cobjectmgr.c/heap_malloc/heap_free
 *  all reference (DAT_c0015f24 here resolves to the identical address as
 *  DAT_c00167e8/DAT_c0016154 used elsewhere in this file - confirmed via
 *  direct value comparison of the resolved literal-pool constants, not
 *  assumed). `*(heap_state+8)` is the top/wilderness chunk's own address;
 *  its head word is a 4-byte size|flags field, size in the top 30 bits.
 * ------------------------------------------------------------------------- */
extern int32_t  *heap_state;		/* DAT_c0015f24/DAT_c00167e8/DAT_c0016154 - shared allocator-state pointer */
extern uint32_t  heap_stat_total;	/* *DAT_c0015f28: a running "bytes given back to/taken from the OS" stat, also touched by heap_malloc */
extern uint32_t  heap_stat_base;	/* DAT_c0015f2c: baseline subtracted when recomputing heap_stat_total after a failed trim */

uint32_t heap_trim(void *unused_handle, int32_t pad)	/* FUN_c0015e2c */
{
	int32_t *top_chunk_slot = (int32_t *)((uint8_t *)heap_state + 8);
	uint32_t top_size = (uint32_t)(*(int32_t *)(*top_chunk_slot + 4)) & 0xfffffffcU;
	int32_t  trim_amount = (int32_t)((((top_size - (uint32_t)pad) + 0xfefU) & 0xfffff000U) - 0x1000U);

	heap_lock();

	if (trim_amount > 0xfff) {
		int32_t brk_now = (int32_t)heap_sbrk(unused_handle, 0);

		if (*top_chunk_slot + (int32_t)top_size == brk_now) {
			int32_t sbrk_result = (int32_t)heap_sbrk(unused_handle, -trim_amount);

			if (sbrk_result != -1) {
				uint32_t prev_stat = heap_stat_total;

				*(uint32_t *)(*top_chunk_slot + 4) = (top_size - (uint32_t)trim_amount) | 1;
				heap_stat_total = prev_stat - (uint32_t)trim_amount;
				heap_unlock();
				return 1;
			}

			/* sbrk(-trim_amount) itself failed - re-derive the top
			 * chunk's real size from the actual (unchanged) break
			 * rather than assume the trim happened. */
			brk_now = (int32_t)heap_sbrk(unused_handle, 0);
			int32_t top_addr = *top_chunk_slot;
			uint32_t recomputed = (uint32_t)(brk_now - top_addr);

			if ((int32_t)recomputed > 0xf) {
				heap_stat_total = (uint32_t)brk_now - heap_stat_base;
				*(uint32_t *)(top_addr + 4) = recomputed | 1;
			}
		}
	}

	heap_unlock();
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  heap_malloc (FUN_c0016164) / heap_free (FUN_c0015f30) - the allocator
 *  core. NOT transcribed as executable C (see this file's own header note
 *  on why) - documented structurally:
 *
 *  heap_malloc(handle, size):
 *    - rounds size up to a minimum 16-byte, 8-byte-aligned chunk size
 *      (`size+0xb < 0x17 ? 16 : (size+0xb) & ~7`), rejecting overflow/
 *      too-large requests (returns NULL without touching any lock).
 *    - for requests under 0x1f8 (504) bytes: checks the exact-fit small bin
 *      AND the next-size-up small bin (both 8 bytes apart in a shared array
 *      at heap_state's own base) for a ready entry, taking it immediately
 *      if either has one (dlmalloc's classic small-request fast path - no
 *      splitting needed since adjacent small bins differ by only 8 bytes).
 *    - otherwise (or on small-bin miss): computes a "tree bin" index via a
 *      logarithmic ladder on the rounded size (shifts by 9/6/12/15/18 with
 *      per-range additive offsets 0x38/0x5b/0x6e/0x77/0x7c/0x7e - identical
 *      ladder duplicated at 3 separate sites across malloc/free, a prime
 *      candidate for a single shared helper if this is ever transcribed for
 *      real), checks a single "designated victim" chunk first (via the same
 *      bin-sentinel trick, at a fixed offset from heap_state), then falls
 *      back to walking the computed tree bin and any larger non-empty bin
 *      (via a bitmap at heap_state+4) for a best-fit chunk, splitting off
 *      the remainder as a new free chunk when one is found.
 *    - on total bin-search miss: grows the top/wilderness chunk via
 *      heap_sbrk (page-aligned, with an extra fixed pad constant), updating
 *      running high-water-mark stats (3 separate globals touched:
 *      DAT_c00167f8/fc/c0016800), then carves the request out of the freshly
 *      grown top.
 *    - locks/unlocks via heap_lock/heap_unlock around the whole operation.
 *
 *  heap_free(handle, ptr):
 *    - NULL-safe (returns immediately on a NULL ptr).
 *    - reads the chunk header immediately before `ptr` (chunk = ptr-8, size|
 *      flags word at chunk+4, low bit = "previous chunk in use"), and
 *      special-cases freeing directly below the top/wilderness chunk
 *      (extends top backward, merging with the freed block, then calls
 *      heap_trim if the resulting top size crosses a threshold - the ONLY
 *      call site for heap_trim in the whole allocator).
 *    - otherwise: coalesces with the previous chunk (via prev_foot, the
 *      word stored at the chunk's own start, valid only when the previous
 *      chunk is free) and/or the next chunk (checked via that chunk's own
 *      inuse bit), unlinking any coalesced neighbor from its free-list
 *      first, then reinserts the (possibly now-larger) merged chunk into
 *      the appropriate bin using the SAME small-bin/tree-bin selection
 *      logic as heap_malloc.
 *
 *  Both functions thread a `handle` parameter that, like every other
 *  function in this file, is never actually read - confirmed dead all the
 *  way through heap_sbrk/heap_trim/heap_lock/heap_unlock too. Modeled as
 *  present-but-unused throughout, consistent with the rest of this file and
 *  this project's now-repeated "phantom forwarded parameter" finding.
 *
 *  Genuinely open: the exact struct layout of `heap_state` beyond what's
 *  confirmed above (a binmap word at +4, a designated-victim sentinel
 *  8 bytes past the base, per-bin sentinels at base+index*8); the fixed
 *  constants controlling wilderness growth (DAT_c00167f0/f4); and the three
 *  separate high-water-mark stat globals' individual purposes.
 * ------------------------------------------------------------------------- */
void *heap_malloc(void *handle, uint32_t size);	/* FUN_c0016164, structure only - not transcribed, see above */
void  heap_free(void *handle, void *ptr);		/* FUN_c0015f30, structure only - not transcribed, see above */
