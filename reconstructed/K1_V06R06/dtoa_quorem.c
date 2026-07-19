/* SPDX-License-Identifier: GPL-2.0 */
/*
 * dtoa_quorem.c - single-function gap-fill: FUN_c0018cb4 (@0xc0018cb4, 496
 * bytes), the David Gay `dtoa.c` Bigint library's own `quorem()` - the
 * digit-generation core's trial-quotient-digit primitive.
 *
 * Assignment context: assigned gap cluster 0xc0018bac-0xc0018cb4 (the
 * second of its "2 fns" - the first, FUN_c0018bac, is
 * newlib_stdio_srefill in newlib_stdio_streams.c; that file's own header
 * explains why the two, despite sharing a gap-cluster label, are
 * unrelated subsystems that happen to sit next to each other in the
 * image). Genuinely uncovered by any existing file (double-checked
 * directly - no grep hit anywhere in K1_V06R06/*.c).
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled.json
 * / all_data.json), queried via query_dump.py, 2026-07-19 pass. No live
 * Ghidra MCP calls this pass.
 *
 * IDENTIFICATION - concrete, not guessed: this function reads/writes
 * fields at offset +0x10 (word count) and +0x14 (digit array base) off
 * BOTH of its two pointer arguments - EXACTLY newlib_dtoa_bigint.c's own
 * already-established `struct dtoa_bigint` layout (`next@0x00, k@0x04,
 * maxwds@0x08, sign@0x0c, wds@0x10, x[]@0x14` - see that file's own
 * header, "a byte-for-byte match to the reference `Bigint` struct's own
 * field order"), and calls `FUN_c001b588` - already reconstructed there
 * as `dtoa_cmp(struct dtoa_bigint *a, struct dtoa_bigint *b)`, "classic
 * `cmp(a, b)`" - directly on its own two arguments. Combined with the
 * function's own real shape (a multiply-by-trial-quotient-digit pass over
 * `S`'s words subtracted from `b`'s words, a carry-propagated 16-bit-half
 * multiply/subtract loop, then a conditional SECOND subtract-and-increment
 * pass gated on `dtoa_cmp(b, S) >= 0`), this is unambiguously David Gay's
 * reference `dtoa.c` `quorem(Bigint *b, Bigint *S)`: computes and returns
 * the next quotient digit of `b / S`, destructively reducing `b` by
 * `digit * S` in place (shrinking `b->wds` if the top word(s) become
 * zero) - the digit-generation engine every `dtoa()`/dtoa-based
 * `%f`/`%e`/`%g` conversion loop calls in a tight loop, one digit per
 * call.
 *
 * `struct dtoa_bigint` is NOT redefined by including newlib_dtoa_bigint.c
 * (translation units in this project don't share headers, matching the
 * real firmware's own separate compilation units) - a local copy with the
 * SAME field names/offsets is declared below instead, exactly this
 * project's established convention for this situation (see
 * newlib_stdio_streams.c's own `struct newlib_file` local copy of
 * newlib_dtoa_bigint.c's struct of the same name, for the identical
 * precedent).
 *
 * FUN_c001e300 (the division helper computing the initial trial digit,
 * `b's top word / (S's top word + 1)`) is task_sched.c's own documented
 * "(C) 0xc001e300-0xc001e608... __udivsi3/__divsi3-shaped restoring-
 * division loop" - out of this file's own scope, cited by address only.
 *
 * Real decompile (query_dump.py func c0018cb4), for reference:
 *
 *     int FUN_c0018cb4(int param_1,int param_2)
 *     {
 *       if (*(int *)(param_1 + 0x10) < *(int *)(param_2 + 0x10)) {
 *         iVar1 = 0;
 *       } else {
 *         iVar9 = *(int *)(param_2 + 0x10) + -1;
 *         puVar6 = (uint *)(param_2 + 0x14);        // S->x
 *         puVar10 = (uint *)(param_1 + 0x14);       // b->x
 *         puVar5 = puVar6 + iVar9;                  // &S->x[n-1]
 *         puVar11 = puVar10 + iVar9;                // &b->x[n-1]
 *         iVar1 = FUN_c001e300(puVar10[iVar9],puVar6[iVar9] + 1);
 *         if (iVar1 != 0) {
 *           ... multiply S by iVar1, subtract from b, word-by-word,
 *               16-bit-half carry propagation ...
 *           if (*puVar11 == 0) { ... shrink b->wds while top word(s) are 0 ... }
 *         }
 *         iVar2 = FUN_c001b588(param_1,param_2);    // dtoa_cmp(b, S)
 *         if (-1 < iVar2) {
 *           iVar1 = iVar1 + 1;
 *           ... subtract S from b ONE more time, same word-by-word shape ...
 *           ... shrink b->wds again if needed ...
 *         }
 *       }
 *       return iVar1;
 *     }
 *
 * Transcribed close to literally below (the 16-bit-half multiply/subtract
 * carry arithmetic is exactly the classic portable-Bigint shape - not
 * simplified into wider arithmetic, to avoid silently changing overflow/
 * carry behavior that's genuinely 16-bit-half-word-at-a-time in the real
 * code).
 */

#include <stdint.h>

struct dtoa_bigint {
	struct dtoa_bigint *next;	/* +0x00 */
	int32_t  k;			/* +0x04 */
	int32_t  maxwds;		/* +0x08 */
	int32_t  sign;			/* +0x0c */
	int32_t  wds;			/* +0x10 */
	uint32_t x[1];			/* +0x14, variable length */
};

extern uint32_t dtoa_quorem_div(uint32_t num, uint32_t denom);	/* FUN_c001e300, task_sched.c's own "(C)" libgcc division-helper block, out of this file's own scope */
extern int32_t  dtoa_cmp(struct dtoa_bigint *a, struct dtoa_bigint *b);	/* FUN_c001b588, newlib_dtoa_bigint.c */

/*
 * dtoa_quorem_msubtract - shared "multiply S by `mult`, subtract from b,
 * one 16-bit half at a time with carry/borrow propagation" pass - both
 * halves of dtoa_quorem's own real body (the first, trial-digit-scaled
 * pass and the second, fixed single-subtract "correction" pass) run this
 * exact same shape, differing only in the multiplier (the trial digit vs.
 * a fixed 1) - factored out here rather than duplicated twice, matching
 * the real decompile's own two, only-multiplier-differing copies.
 * `n` is `S->wds - 1` (index of the top word in both `S->x`/`b->x`).
 */
static void dtoa_quorem_msubtract(uint32_t *bx, uint32_t *sx, int32_t n, uint32_t mult)
{
	int32_t borrow = 0;
	uint32_t carry = 0;
	uint32_t *sp = sx;
	uint32_t *bp = bx;
	uint32_t *sx_top = sx + n;

	do {
		uint32_t yhi, diff_lo, diff_hi;

		carry = mult * (*sp & 0xffff) + carry;
		yhi   = mult * (*sp >> 0x10) + (carry >> 0x10);

		diff_lo = ((*bp & 0xffff) - (carry & 0xffff)) + borrow;
		diff_hi = ((*bp >> 0x10) - (yhi & 0xffff)) + (diff_lo >> 0x10);

		*(int16_t *)((uint8_t *)bp + 2) = (int16_t)diff_hi;
		borrow = diff_hi >> 0x10;
		*(int16_t *)bp = (int16_t)diff_lo;

		carry = yhi >> 0x10;
		sp++;
		bp++;
	} while (sp <= sx_top);
}

/*
 * dtoa_quorem_shrink - shared "b->wds is now stale after a subtract pass
 * that may have zeroed the top word(s) - shrink it back down" helper.
 * Transcribed to match the real decompile's own comma-expression while
 * loop EXACTLY (decrement-then-test every iteration, including the final
 * failing check) rather than a superficially similar but off-by-one-prone
 * "decrement inside the loop body" rewrite - real control flow:
 *   `while ((p = p-1, bx < p && *p == 0)) { n--; }`
 * i.e. `p` is decremented before every single test, unconditionally.
 */
static int32_t dtoa_quorem_shrink(struct dtoa_bigint *b, uint32_t *bx, int32_t n)
{
	uint32_t *p = bx + n;

	for (;;) {
		p--;
		if (!(bx < p && *p == 0))
			break;
		n--;
	}
	b->wds = n;
	return n;
}

/*
 * dtoa_quorem - FUN_c0018cb4. Computes the next base-2^32 (via 16-bit-half
 * carry propagation) trial quotient digit of `b / S`, reducing `b` by
 * `digit * S` in place; returns the digit. 2 real callers, both from
 * FUN_c0018ea4 (the large dtoa-adjacent numeric-formatting driver, out of
 * this file's own scope - not one of this pass's 12 assigned gap
 * clusters). @0xc0018cb4.
 */
int32_t dtoa_quorem(struct dtoa_bigint *b, struct dtoa_bigint *S)	/* FUN_c0018cb4 */
{
	int32_t digit;

	if (b->wds < S->wds)
		return 0;

	{
		/* `n0` is S's own top-word index - FIXED for both
		 * multiply-subtract passes below (S is never modified, and
		 * both passes' own loop bound pointer, `puVar5` in the real
		 * decompile, is computed ONCE from the original index and
		 * reused as-is for the second pass too - NOT recomputed from
		 * a possibly-already-shrunk index). `bn` is b's own top-word
		 * index, which DOES shrink independently after each pass if
		 * that pass zeroed b's top word(s) - tracked separately, per
		 * the real decompile's own two independent shrink checks. */
		int32_t n0 = S->wds - 1;
		int32_t bn = n0;
		uint32_t *sx = S->x;
		uint32_t *bx = b->x;

		digit = (int32_t)dtoa_quorem_div(bx[n0], sx[n0] + 1);

		if (digit != 0) {
			dtoa_quorem_msubtract(bx, sx, n0, (uint32_t)digit);
			if (bx[n0] == 0)
				bn = dtoa_quorem_shrink(b, bx, n0);
		}

		if (dtoa_cmp(b, S) >= 0) {
			digit++;
			dtoa_quorem_msubtract(bx, sx, n0, 1);
			if (bx[bn] == 0)
				dtoa_quorem_shrink(b, bx, bn);
		}
	}

	return digit;
}
