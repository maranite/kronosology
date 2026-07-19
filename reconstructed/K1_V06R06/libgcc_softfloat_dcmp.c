/* SPDX-License-Identifier: GPL-2.0 */
/*
 * libgcc_softfloat_dcmp.c - four-function gap-fill: FUN_c001bf34
 * (@0xc001bf34, 32 bytes) plus FUN_c001ee54/c001ee5c/c001ee64
 * (@0xc001ee54/0xc001ee5c/0xc001ee64, 8/8/148 bytes), a family of libgcc
 * soft-float IEEE-754 double-precision COMPARISON helpers (`__gedf2`/
 * `__ledf2`/`__cmpdf2`-shaped) - two-argument-pair (hi32/lo32 halves) ARM
 * EABI double-compare thunks, the kind GCC emits for any `double`
 * relational operator on a target without hardware double compare.
 *
 * Assignment context: two separate assigned gap clusters that are, per
 * hard evidence, the SAME small function family scattered across two
 * address neighborhoods:
 *   - 0xc001bf34 sits directly inside task_sched.c's own documented
 *     "(A) 0xc001bef8-0xc001bf54: two IEEE-754 double-precision compare
 *     helpers (the `0x7ff00000` exponent-mask idiom is a standard libgcc
 *     __gedf2/__ledf2-family soft-float comparison shape)" - that file
 *     explicitly declines to reconstruct BOTH of the two helpers it
 *     names in that range ("plus one C++-streambuf-shaped virtual
 *     callback... NOT reconstructed here"); this file supplies the
 *     second of the two compare helpers (the first, presumably
 *     0xc001bef8-adjacent, is not itself one of this pass's 12 assigned
 *     gap clusters and is left for a future pass). task_sched.c is NOT
 *     edited here (collision-avoidance rule).
 *   - 0xc001ee54/c001ee5c/c001ee64 sit just past task_sched.c's own last
 *     documented sweep chunk (0xc001e918-0xc001e990) - genuinely outside
 *     every existing file's own claimed range (double-checked directly).
 *
 * All FOUR functions share, almost verbatim, the exact same decompiled
 * control-flow shape (only the two return-value conventions on the
 * NaN/unordered path and one trailing constant differ) - concrete,
 * mechanical evidence they're siblings in one libgcc comparison family,
 * not four independently-evolved functions. FUN_c001ee5c and FUN_c001ee64
 * decompile to LITERALLY IDENTICAL C text despite different real
 * instruction-derived sizes (8 vs 148 bytes) - left as an honest,
 * unresolved observation (see STILL OPEN below) rather than forcing a
 * distinct high-level body neither the decompile nor this project's own
 * tooling can actually tell apart.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled.json
 * / all_data.json), queried via query_dump.py, 2026-07-19 pass. No live
 * Ghidra MCP calls this pass.
 *
 * SHARED SHAPE (transcribed once here, in prose, rather than four times):
 * each function takes a `double` split into two 32-bit halves per
 * argument (`(hi_a, lo_a, hi_b, lo_b)` - standard ARM EABI soft-float
 * double-arg passing: `hi` carries the sign+exponent+top mantissa bits,
 * `lo` the low mantissa word) and:
 *   1. Checks whether either operand's exponent field (`hi & 0x7ff00000`)
 *      is the all-ones NaN/Inf pattern AND that operand's full mantissa
 *      (exponent bits from `hi`, shifted, OR'd with `lo`) is nonzero -
 *      i.e. a real NaN, not just Infinity. If so, returns a FIXED
 *      "unordered" sentinel (-1 for FUN_c001bf34/FUN_c001ee54, +1 for
 *      FUN_c001ee5c/FUN_c001ee64) - the classic per-comparator NaN
 *      contract (`__gedf2`-family returns a negative "false" sentinel for
 *      `>=`-shaped comparisons; `__ledf2`-family returns a positive
 *      "false" sentinel for `<=`-shaped comparisons).
 *   2. Otherwise does a full signed magnitude compare: exact bit-equality
 *      (including the "negative zero == positive zero" case, checked
 *      explicitly) returns 0; otherwise compares sign bits, then (for
 *      same-sign operands) exponent, then mantissa-high, then
 *      mantissa-low, producing a nonzero odd result (always OR'd with 1,
 *      i.e. always odd) whose SIGN encodes less-than vs greater-than,
 *      with the sign of operand `b` used as a tie-break multiplier/
 *      inverter for the final result when `a < b` bit-pattern-wise.
 *
 * This project does not have and does not claim access to the specific
 * ARM EABI libgcc source drop this binary was built from - the structural
 * identification above is grounded entirely in what THIS binary's own
 * decompile shows (the `0x7ff00000` mask, the two distinct NaN return
 * values, the identical shared body across all four sites), per this
 * project's own established discipline for libgcc/dlmalloc/dtoa
 * identification elsewhere (see newlib_dtoa_bigint.c/heap_alloc.c's own
 * header notes on the same standard).
 *
 * STILL OPEN:
 *   - The real GCC export names (`__gedf2` vs `__gtdf2` vs `__cmpdf2`,
 *     etc.) are NOT independently confirmed - no symbol table entry is
 *     available in this static dump, only the NaN-return-value contract,
 *     which is necessary but not sufficient to pick a single exact name
 *     among libgcc's several NaN-return-(-1)/NaN-return-(+1) siblings.
 *     Named descriptively (`_ge_family`/`_le_family`) rather than
 *     asserting a specific mnemonic.
 *   - FUN_c001ee5c and FUN_c001ee64 decompile to identical C text but
 *     have different real sizes (8 and 148 bytes respectively) - this
 *     project's own tooling (Ghidra's decompiler, working from this
 *     static dump) cannot distinguish their real machine-code bodies
 *     beyond that shared high-level shape. Both are defined below with
 *     THE SAME body (matching what's actually shown), rather than
 *     inventing a divergent 148-byte version with no evidence behind it.
 *     A live disassembly diff (out of this pass's own read-only,
 *     2-agent-cap scope) would be needed to resolve the real difference.
 *   - The other "compare helper" task_sched.c's own header names
 *     (somewhere in 0xc001bef8-0xc001bf34) is NOT part of this pass's 12
 *     assigned gap clusters and is not reconstructed here either.
 */

#include <stdint.h>

/*
 * softfloat_dcmp_is_nan_pair - shared NaN/Inf-with-nonzero-mantissa test,
 * factored out since all four real functions in this file open with the
 * exact same check (verified identical across all four independent
 * decompiles) - the only per-function difference is which sentinel gets
 * returned when this is true (-1 for the two "ge-family" sites, +1 for
 * the two "variant" sites), and the shared magnitude-compare tail below.
 */
static int softfloat_dcmp_is_nan_pair(uint32_t a_hi, uint32_t a_lo,
				       uint32_t b_hi, uint32_t b_lo)
{
	uint32_t a_exp = a_hi & 0x7ff00000;
	uint32_t b_exp = b_hi & 0x7ff00000;

	return (a_exp == 0x7ff00000 || b_exp == 0x7ff00000) &&
	       ((a_exp == 0x7ff00000 && ((a_lo | (a_hi << 0xc)) != 0)) ||
		(b_exp == 0x7ff00000 && ((b_lo | (b_hi << 0xc)) != 0)));
}

/*
 * softfloat_dcmp_magnitude - shared post-NaN-check compare core, factored
 * out of both softfloat_dcmp_ge_family and softfloat_dcmp_variant_a below
 * (their real decompiles are identical past the NaN check, differing
 * only in the NaN sentinel itself) rather than duplicated verbatim twice.
 *
 * Real logic, traced precisely from the decompile's own nested-`if`
 * nesting (bVar3/bVar4 reuse) rather than flattened into a ternary chain
 * that risks silently swapping a comparison direction:
 *   - exact equality (including -0.0 == +0.0) -> 0.
 *   - otherwise, opposite signs -> return sign of `a` (as -1/0) | 1.
 *   - otherwise (same sign): compute `ge` = "does `a`'s bit-pattern
 *     magnitude order at or above `b`'s", refined exponent -> mantissa-hi
 *     -> mantissa-lo only as each preceding level ties; fold in `b`'s
 *     sign (inverted if `!ge`) and OR with 1 (result is always odd,
 *     matching every real call site's own use of just the sign bit).
 */
static uint32_t softfloat_dcmp_magnitude(uint32_t a_hi, uint32_t a_lo,
					  uint32_t b_hi, uint32_t b_lo)
{
	int is_zero_a = (a_hi & 0x7fffffff) == 0 && a_lo == 0;
	int equal = is_zero_a ? ((b_hi & 0x7fffffff) == 0 && b_lo == 0)
			      : (a_hi == b_hi);

	if (!equal)
		equal = (a_hi == b_hi);
	if (equal)
		equal = (a_lo == b_lo);

	if (equal)
		return 0;

	if ((int32_t)(a_hi ^ b_hi) < 0)
		return ((int32_t)a_hi >> 0x1f) | 1;

	{
		uint32_t a_exp = a_hi & 0x7ff00000;
		uint32_t b_exp = b_hi & 0x7ff00000;
		int ge;

		if (a_exp != b_exp) {
			ge = (b_exp <= a_exp);
		} else if ((a_hi << 0xc) != (b_hi << 0xc)) {
			ge = (b_hi << 0xc) <= (a_hi << 0xc);
		} else {
			ge = (b_lo <= a_lo);
		}

		{
			int32_t sign = (int32_t)b_hi >> 0x1f;

			if (!ge)
				sign = ~sign;
			return sign | 1;
		}
	}
}

/*
 * softfloat_dcmp_ge_family - FUN_c001bf34. NaN/unordered sentinel: -1
 * (0xffffffff). 1 real caller: FUN_c00169b0 (the shared core formatter,
 * newlib_sprintf.c/newlib_stdio_streams.c - presumably its own `%f`/`%g`
 * conversion path comparing a `double` against some threshold). @0xc001bf34.
 */
uint32_t softfloat_dcmp_ge_family(uint32_t a_hi, uint32_t a_lo,
				   uint32_t b_hi, uint32_t b_lo)	/* FUN_c001bf34 */
{
	if (softfloat_dcmp_is_nan_pair(a_hi, a_lo, b_hi, b_lo))
		return 0xffffffff;
	return softfloat_dcmp_magnitude(a_hi, a_lo, b_hi, b_lo);
}

/*
 * softfloat_dcmp_le_family - FUN_c001ee54. Identical shape to
 * softfloat_dcmp_ge_family above, but with the -1 NaN sentinel (SAME as
 * the `_ge_family` above, not +1 - transcribed exactly as decompiled,
 * not "corrected" to match the +1 sibling below despite the address
 * gap's own name suggesting a `ge`/`le` pairing convention). 5 real
 * callers, all from FUN_c0018ea4 (the large dtoa-adjacent numeric
 * formatting function, out of this file's own scope). @0xc001ee54.
 */
uint32_t softfloat_dcmp_le_family(uint32_t a_hi, uint32_t a_lo,
				   uint32_t b_hi, uint32_t b_lo)	/* FUN_c001ee54 */
{
	return softfloat_dcmp_ge_family(a_hi, a_lo, b_hi, b_lo);
}

/*
 * softfloat_dcmp_variant_a - FUN_c001ee5c. Same shared shape, NaN
 * sentinel +1 this time (matching the real decompile's own `return 1;`
 * on that path, unlike both functions above). 7 real callers (6 from
 * FUN_c0018ea4, 1 from FUN_c00169b0 - the shared core formatter itself).
 * @0xc001ee5c.
 */
uint32_t softfloat_dcmp_variant_a(uint32_t a_hi, uint32_t a_lo,
				   uint32_t b_hi, uint32_t b_lo)	/* FUN_c001ee5c */
{
	if (softfloat_dcmp_is_nan_pair(a_hi, a_lo, b_hi, b_lo))
		return 1;
	return softfloat_dcmp_magnitude(a_hi, a_lo, b_hi, b_lo);
}

/*
 * softfloat_d2iz - FUN_c001eef8, a fifth member of this same libgcc
 * soft-float family, added in a later pass. Classic ARM EABI `__aeabi_d2iz`
 * shape: signed double-to-int32 conversion with round-toward-zero and
 * saturation, not a comparison (unlike the four functions above), but
 * placed here because it shares this file's exact caller (`FUN_c0018ea4`,
 * 4 of its call sites) and the same `0x7ff00000`/exponent-field idiom.
 * Real logic, traced from the decompile:
 *   - +0.0/-0.0 (mantissa and exponent all zero) -> 0.
 *   - exponent field all-ones (NaN or Infinity): NaN (nonzero low mantissa
 *     word) -> 0; Infinity falls through to the saturation tail below.
 *   - exponent below the bias for magnitude >= 1.0 (0x3ff00000) -> 0
 *     (truncates to zero, matching round-toward-zero for |x| < 1).
 *   - exponent below 0x41e00000 (the 2^31 magnitude threshold): shift the
 *     53-bit significand (implicit leading 1 restored) right by the
 *     distance from the max in-range exponent, producing the truncated
 *     integer magnitude; negate if the original sign bit was set.
 *   - otherwise (magnitude >= 2^31, including Infinity): saturate to
 *     INT32_MAX (0x7fffffff) for non-negative inputs, INT32_MIN
 *     (0x80000000, the untouched initial `uVar1` sign-extend value) for
 *     negative inputs - the standard `__aeabi_d2iz` overflow contract.
 * @0xc001eef8.
 */
uint32_t softfloat_d2iz(uint32_t hi, uint32_t lo)	/* FUN_c001eef8 */
{
	/* CONFIRMED via direct decompile re-check: (hi>>31)*-0x80000000 is
	 * exactly 0 (hi>=0) or 0x80000000 (hi<0) - NOT a sign-extended
	 * 0/0xffffffff pattern (an error caught and fixed before finalizing). */
	uint32_t int_min_if_negative = ((int32_t)hi >> 0x1f) * -0x80000000;
	uint32_t exp = hi & 0x7ff00000;

	if (lo == 0 && (hi & 0x7fffffff) == 0)
		return 0;

	if (exp == 0x7ff00000) {
		/* NaN (nonzero mantissa) -> 0; Infinity falls through */
		if (lo != 0 || (hi & 0xfffff) != 0)
			return 0;
	} else {
		if (exp < 0x3ff00000)
			return 0;
		if (exp < 0x41e00000) {
			uint32_t mag = (hi << 0xb | 0x80000000 | lo >> 0x15) >>
				       ((0x41e00000 - exp) >> 0x14 & 0xff);
			return ((int32_t)hi < 0) ? -mag : mag;
		}
	}

	/* saturate: INT32_MAX for >=0, INT32_MIN (int_min_if_negative's own
	 * value, 0x80000000) for negative */
	return ((int32_t)hi < 0) ? int_min_if_negative : 0x7fffffff;
}

/*
 * softfloat_dcmp_variant_b - FUN_c001ee64. Decompiles to LITERALLY THE
 * SAME C text as softfloat_dcmp_variant_a immediately above (both fetched
 * independently via query_dump.py, byte-for-byte identical high-level
 * bodies) despite a real, very different reported size (148 vs 8 bytes) -
 * see this file's own header "STILL OPEN" note. Defined as a thin
 * forwarder to avoid asserting a fabricated 148-byte body with no
 * decompile evidence behind it. 8 real callers (4 from FUN_c0018ea4, 4
 * from FUN_c00169b0). @0xc001ee64.
 */
uint32_t softfloat_dcmp_variant_b(uint32_t a_hi, uint32_t a_lo,
				   uint32_t b_hi, uint32_t b_lo)	/* FUN_c001ee64 */
{
	return softfloat_dcmp_variant_a(a_hi, a_lo, b_hi, b_lo);
}
