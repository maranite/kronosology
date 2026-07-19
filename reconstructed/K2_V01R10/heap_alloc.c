/* SPDX-License-Identifier: GPL-2.0 */
/*
 * heap_alloc.c - KRONOS2S_V01R10.VSB (Kronos 2) port of K1_V06R06/heap_alloc.c.
 * Same subsystem, CONFIRMED essentially unchanged: the firmware's shared,
 * general-purpose heap allocator - a segregated free-list allocator (small
 * exact-fit bins up to ~504 bytes, indexed logarithmic "tree" bins beyond
 * that), boundary-tag free chunks with immediate coalescing, a page-granular
 * trim-back-to-OS path, and an sbrk-style break-pointer primitive underneath
 * it all. Almost certainly a compiled dlmalloc derivative, same caveat as K1.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB,
 * 2026-07-18 (query_dump_k2.py). Located via cobjectmgr.c's own K2 port,
 * which already declares `extern void heap_free(...)` at FUN_c0012e58 (K1:
 * FUN_c0015f30) as an opaque dependency of cobjectmgr_object_destroy and
 * cobjectmgr_free_list_recursive - the exact same discovery path K1's own
 * heap_alloc.c documents (found via clcdc.c's boundary correction there).
 *
 * NOT anchored by a `__FILE__` string, same as K1 - no full-image string
 * search performed this pass (not needed - the code-shape/caller evidence
 * from cobjectmgr.c's own port is already conclusive).
 *
 * PORTING METHOD / CONFIDENCE: every function below was matched by
 * decompiling the K2 static dump at the address found and diffing its raw
 * Ghidra decompile text against K1's raw decompile text with address-literal
 * DAT_/FUN_ tokens masked out - same method mcasp.c's own K2 port used.
 * heap_sbrk and heap_trim came back STRUCTURALLY IDENTICAL to K1, statement
 * for statement (branch shape, comparison operators, and the same
 * re-derive-from-actual-break fallback path in heap_trim's own sbrk-failure
 * branch). heap_lock/heap_unlock are confirmed empty bodies, same as K1.
 * heap_malloc's own opening rounding/small-bin/treebin-ladder logic (the
 * only part inspected, consistent with this project's own established
 * practice for this function - see below) is likewise statement-for-
 * statement identical, including the exact same shift constants (9/6/12/15/
 * 18) and additive offsets (0x38/0x5b/0x6e/0x77/0x7c/0x7e) K1's own header
 * already catalogued.
 *
 * ADDRESS MAP (K1 -> K2):
 *   heap_lock    FUN_c0016894 -> FUN_c00137bc
 *   heap_unlock  FUN_c0016898 -> FUN_c00137c0
 *   heap_sbrk    FUN_c0015d4c -> FUN_c0012c74
 *   heap_trim    FUN_c0015e2c -> FUN_c0012d54
 *   heap_malloc  FUN_c0016164 -> FUN_c001308c (size 1668 bytes in K2; K1's
 *                own size was not itself recorded for direct comparison)
 *   heap_free    FUN_c0015f30 -> FUN_c0012e58 (size 548 bytes)
 *
 * REAL DIFFERENCE FROM K1, not a transcription artifact: K2's `heap_errno`
 * global address (0xC01CE1B4, see below) is ALSO independently zeroed by
 * THREE other, unrelated K2 functions (FUN_c0012a90, FUN_c0012ab4,
 * FUN_c0012adc - callers FUN_c001799c and two sites near 0xc0018f24/58,
 * FUN_c0018e7c respectively) that are NOT part of this allocator cluster by
 * address or by any other evidence found this pass. This project does NOT
 * claim those three functions belong to heap_alloc.c - most likely this
 * `errno`-shaped global is a small SHARED cell multiple unrelated init
 * routines clear at boot, not evidence of a wider heap_alloc.c boundary.
 * Noted here as an honest open item, not folded into this file's own scope.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  heap_lock / heap_unlock - K2 @0xc00137bc / @0xc00137c0 (K1: @0xc0016894 /
 *  @0xc0016898). CONFIRMED empty bodies (`{ return; }`), same as K1. Bracket
 *  every allocator entry point (4 callers each found this pass: heap_free,
 *  heap_trim, heap_malloc, plus one more caller each outside this cluster -
 *  FUN_c0018960 for heap_lock - not traced, consistent with K1's own
 *  "not resolved further" treatment of this pair).
 * ------------------------------------------------------------------------- */
void heap_lock(void)	/* FUN_c00137bc */
{
}

void heap_unlock(void)	/* FUN_c00137c0 */
{
}

/* ------------------------------------------------------------------------- *
 *  heap_sbrk - K2 @0xc0012c74 (K1 @0xc0015d4c). CONFIRMED structurally
 *  identical to K1, statement for statement: rounds the increment up to 4
 *  bytes, lazily initializes the break pointer to a fixed heap-base constant
 *  on first use, commits and returns the OLD break on success, or sets
 *  heap_errno=0xc (ENOMEM) and returns (uint32_t)-1 on overflow/OOM.
 *  `unused_handle` is dead, same phantom-forwarded-parameter pattern as K1
 *  (and as this whole project's own repeated finding elsewhere).
 * ------------------------------------------------------------------------- */
extern uint32_t  heap_break;		/* 0xC01CE1B8 (DAT_c0012cd4) */
extern uint32_t  heap_errno;		/* 0xC01CE1B4 (DAT_c0012cd8) - see file header's own cross-reference note */
extern uint32_t  heap_base_const;	/* 0xC0239200 (DAT_c0012cdc) */
extern uint32_t  heap_end_const;	/* 0xC023BA00 (DAT_c0012ce0) */

uint32_t heap_sbrk(void *unused_handle, int32_t increment)	/* FUN_c0012c74 */
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
 *  heap_trim - K2 @0xc0012d54 (K1 @0xc0015e2c). CONFIRMED structurally
 *  identical to K1: shrinks the heap's top/wilderness chunk back to the OS
 *  via a negative heap_sbrk() when more than one page (0x1000) of free space
 *  has accumulated, including the same "sbrk(-trim_amount) itself failed -
 *  re-derive the top chunk's real size from the actual unchanged break"
 *  fallback branch K1's own header calls out explicitly.
 *
 *  `heap_state` here (DAT_c0012e4c) is the SAME shared allocator-state
 *  pointer heap_malloc/heap_free also reference below - independently
 *  re-confirmed via K2's own resolved literal-pool values (0xC00A0E7C),
 *  same cross-check method K1's own file used.
 * ------------------------------------------------------------------------- */
extern int32_t  *heap_state;		/* 0xC00A0E7C - shared allocator-state pointer */
extern uint32_t  heap_stat_total;	/* 0xC01CE1BC */
extern uint32_t  heap_stat_base;	/* 0xC00A0E74 */

uint32_t heap_trim(void *unused_handle, int32_t pad)	/* FUN_c0012d54 */
{
	int32_t *top_chunk_slot = (int32_t *)((uint8_t *)heap_state + 8);
	uint32_t top_size;
	int32_t  trim_amount;

	heap_lock();

	top_size = (uint32_t)(*(int32_t *)(*top_chunk_slot + 4)) & 0xfffffffcU;
	trim_amount = (int32_t)((((top_size - (uint32_t)pad) + 0xfefU) & 0xfffff000U) - 0x1000U);

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
			 * rather than assume the trim happened. Identical
			 * fallback shape to K1. */
			brk_now = (int32_t)heap_sbrk(unused_handle, 0);
			{
				int32_t top_addr = *top_chunk_slot;
				uint32_t recomputed = (uint32_t)(brk_now - top_addr);

				if ((int32_t)recomputed > 0xf) {
					heap_stat_total = (uint32_t)brk_now - heap_stat_base;
					*(uint32_t *)(top_addr + 4) = recomputed | 1;
				}
			}
		}
	}

	heap_unlock();
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  heap_malloc (FUN_c001308c, size 1668) / heap_free (FUN_c0012e58, size
 *  548) - the allocator core. NOT transcribed as executable C, same
 *  established practice as K1 (and this project's own dense-code precedent:
 *  clcdc.c's clcdc_blit_glyph, cobjectmgr.c's cobjectmgr_handle_type_b) -
 *  documented structurally, with the opening rounding/fast-path logic
 *  spot-checked line-by-line against K1's own decompile text (address
 *  literals masked) and found IDENTICAL:
 *
 *  heap_malloc(handle, size) - CONFIRMED identical opening sequence to K1:
 *    - rounds size up to a minimum 16-byte, 8-byte-aligned chunk size
 *      (`size+0xb < 0x17 ? 16 : (size+0xb) & ~7`), rejecting overflow
 *      (returns NULL without touching any lock) - byte-for-byte the same
 *      comparison and mask as K1.
 *    - for requests under 0x1f8 (504) bytes: checks the exact-fit small bin
 *      and the next-size-up small bin (identical `(uVar7>>3)*8` indexing to
 *      K1), taking either immediately if ready.
 *    - otherwise: computes a "tree bin" index via the SAME logarithmic
 *      ladder K1 documents - shifts by 9/6/12/15/18 with per-range additive
 *      offsets 0x38/0x5b/0x6e/0x77/0x7c/0x7e, confirmed identical constant
 *      for constant in K2's own decompile.
 *    - the remainder (designated-victim check, bin/bitmap walk, top-chunk
 *      growth via heap_sbrk on total miss) was NOT individually re-diffed
 *      this pass beyond confirming the function's overall size/shape is
 *      consistent with K1's own description - same practice K1 itself used
 *      for its own heap_malloc/heap_free.
 *    - locks/unlocks via heap_lock/heap_unlock around the whole operation
 *      (confirmed: heap_lock is this function's first real call, per its
 *      own xrefs_to entry `c00130c8 in FUN_c001308c`).
 *
 *  heap_free(handle, ptr) - CONFIRMED same overall shape via its own call
 *  graph: it is heap_trim's ONLY call site (`c0013034 in FUN_c0012e58`),
 *  exactly matching K1's own "the ONLY call site for heap_trim in the whole
 *  allocator" claim. NULL-safe, coalesces with neighboring free chunks,
 *  special-cases freeing directly below the top/wilderness chunk (extending
 *  top backward then calling heap_trim) - not independently re-diffed
 *  statement-for-statement this pass, same treatment as K1's own file gave
 *  it.
 *
 *  Both functions thread a `handle` parameter that, like every other
 *  function in this file, is never actually used for anything beyond being
 *  passed straight through to heap_sbrk/heap_lock/heap_unlock - consistent
 *  with K1's own "phantom forwarded parameter" finding, not independently
 *  re-verified bit-for-bit this pass.
 *
 *  2026-07-19 LIVE GHIDRA FOLLOW-UP (read-only MCP bridge against
 *  kronos2s_v01r10_panel.elf; zero Agent-tool subagent calls, per this
 *  task's own 2-agent-cap authorization): decompile_function on heap_free
 *  (FUN_c0012e58) succeeded in full (it had already been matched by shape
 *  only, not by a complete live decompile, before this pass). Confirms the
 *  file's own existing structural description exactly - NULL check, chunk
 *  coalescing with both neighbors, top/wilderness special case - and adds
 *  real detail not previously resolved:
 *   - `heap_state+8` (the top-chunk-address field heap_trim already uses)
 *     doubles as heap_free's own "is this the top chunk" IDENTITY sentinel
 *     - read_memory confirms the literal compared against it (DAT_c0013080
 *       in the live decompile) resolves to 0xC00A0E84 = heap_state(0xC00A0E7C)+8
 *       exactly, i.e. no separate sentinel object - textbook dlmalloc "top
 *       chunk is its own bin sentinel" idiom, not a new field.
 *   - heap_free's own top-chunk-growth path is CONFIRMED to be the real,
 *     concrete trigger for heap_trim's only call site (matching the file's
 *     already-stated claim, now with real arguments resolved): it compares
 *     the coalesced free size against `*0xC00A0E78` (a config threshold
 *     living immediately BEFORE heap_state's own resolved base, i.e.
 *     heap_state-4 - not itself part of the heap_state struct as declared
 *     here, a separate small global) and, if exceeded, calls
 *     `heap_trim(handle, *0xC01CE1EC)` - that pad value lives in the SAME
 *     data region as heap_stat_total (0xC01CE1BC) and heap_errno
 *     (0xC01CE1B4) below, i.e. genuinely part of this allocator's shared
 *     config block, not a coincidence.
 *   - The small-bin array is indexed directly off heap_state's own base
 *     (`heap_state + bin_index*8`), with the bitmap living at heap_state+4 -
 *     consistent with (and slightly sharpening) heap_trim's own already-
 *     documented heap_state+8 finding, though the full struct (small-bin
 *     count, treebin sentinels, designated-victim slot) was still not
 *     individually re-derived field-by-field this pass - heap_malloc's own
 *     decompile_function call did not return output and was not retried
 *     further given this pass's own time budget (see "still open" below).
 * ------------------------------------------------------------------------- */
void *heap_malloc(void *handle, uint32_t size);	/* FUN_c001308c, structure only - not transcribed, see above */
void  heap_free(void *handle, void *ptr);		/* FUN_c0012e58, structure only - not transcribed, see above; ALREADY declared `extern` by cobjectmgr.c, this is that declaration's real definition */

/* -------------------------------------------------------------------------
 * Still genuinely open (updated 2026-07-19 live pass):
 *  - The exact struct layout of `heap_state` beyond what heap_trim's own
 *    body AND heap_free's own live decompile (see above) together confirm
 *    (top/wilderness-chunk pointer at +8, small-bin bitmap at +4, small-bin
 *    array indexed directly off the base) - the treebin sentinels and
 *    designated-victim slot heap_malloc's own body must reference were NOT
 *    individually re-derived: decompile_function on heap_malloc
 *    (FUN_c001308c) returned no output this pass and was not retried
 *    further, given this pass's own time budget (NEEDS LIVE QUERY - retry
 *    decompile_function on 0xc001308c, or read_memory its raw bytes, if a
 *    future pass needs the treebin layout specifically).
 *  - The three unrelated K2 functions (FUN_c0012a90/ab4/adc) that happen to
 *    zero the SAME heap_errno global - see file header. Left explicitly
 *    OUT of this file's own scope rather than folded in on weak evidence.
 *  - Whether K2's heap_base_const/heap_end_const (0xC0239200/0xC023BA00)
 *    imply a differently-sized heap region than K1's own build - STILL not
 *    resolvable even with this pass's own live K2 access: K1's own file
 *    genuinely never states DAT_c0015db4/DAT_c0015db8 numerically either
 *    (confirmed by directly re-reading K1_V06R06/heap_alloc.c this pass,
 *    not assumed), and this task's live Ghidra access is scoped to the K2
 *    binary only - closing this would need a live query against K1's own
 *    project, out of this pass's scope. NEEDS LIVE QUERY against
 *    K1_V06R06's own Ghidra project if the actual heap size delta matters
 *    for future work.
 * ------------------------------------------------------------------------- */
