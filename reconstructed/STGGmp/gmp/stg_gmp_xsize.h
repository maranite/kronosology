/* stg_gmp_xsize.h -- STGGmp.ko-only patch: integer-only, provably-safe
 * replacement for stock GMP 4.2.1 mpz/set_str.c's `xsize` scratch-buffer
 * size estimate. See fetch-gmp.sh's own comment (search "libgcc
 * soft-float") for the full incident writeup; this header carries just
 * the safety argument and the replacement itself.
 *
 * `xsize` is used ONLY as a pre-allocation hint (mpz/set_str.c's very
 * next line: `MPZ_REALLOC (x, xsize)`) before the REAL digit-by-digit
 * conversion (mpn_set_str(), mpn/set_str.c) computes the true limb count
 * and writes directly into that pre-allocated `x->_mp_d` buffer with NO
 * further bounds check or reallocation (confirmed by direct inspection
 * of mpn/set_str.c -- it trusts the caller's allocation completely).
 * This makes the correctness requirement precise and asymmetric: `xsize`
 * may safely be an OVER-estimate (wastes a few words of scratch memory
 * -- harmless) but must NEVER be an UNDER-estimate (mpn_set_str() would
 * then write past the end of the allocated buffer -- real heap
 * corruption, not just a wrong answer).
 *
 * Safety argument (no floating point, no new per-base table, correct
 * for every base 2..62 stock GMP supports -- not hardcoded to base 10,
 * even though base 10 is, as of this writing, the only base any real
 * caller of __gmpz_init_set_str in this project actually passes --
 * see reconstructed/OA/src/auth/atmel_challenge.cpp):
 *
 *   chars_per_bit_exactly == log(2)/log(base) (gmp-impl.h's own `struct
 *   bases` comment), so the original expression computes
 *   floor(str_size * log2(base)).
 *
 *   Every mp_bases[] entry's `chars_per_limb` field is DEFINED (same
 *   struct's own header comment) as "the number of digits in the
 *   conversion base that always fits in an mp_limb_t" -- i.e. by
 *   construction, base**chars_per_limb <= 2**GMP_NUMB_BITS for every
 *   entry in the table (mp_bases[] is a static generated table, not
 *   computed at runtime -- this invariant is load-bearing and already
 *   relied on elsewhere in GMP itself, e.g. mpn/set_str.c's own use of
 *   chars_per_limb as a conversion block size). Taking log2 of both
 *   sides of that invariant:
 *
 *       chars_per_limb * log2(base) <= GMP_NUMB_BITS
 *       log2(base) <= GMP_NUMB_BITS / chars_per_limb
 *
 *   Substituting this upper bound for log2(base) in the original
 *   expression can only ever make the result larger or equal, never
 *   smaller -- i.e. it is a provably-safe over-estimate, computed with
 *   plain integer division/multiplication only:
 *
 *       str_size * log2(base) <= str_size * GMP_NUMB_BITS / chars_per_limb
 *
 *   The helper below computes the right-hand side with the division
 *   rounded UP (ceiling) and truncates to mp_size_t exactly as the
 *   original's outer `(mp_size_t)` cast did -- both roundings are safe
 *   in the "never smaller than the true real value" direction, so their
 *   composition is too. Cross-checked against the real (host, real
 *   libgcc) double-precision formula for every base 2..62 and str_size
 *   0..4096 in verify/test_xsize_patch.c: never once smaller, and never
 *   off by more than 1-2 GMP_NUMB_BITS-sized limbs of extra (harmless)
 *   slack.
 */
#ifndef _STG_GMP_XSIZE_H
#define _STG_GMP_XSIZE_H

static inline mp_size_t
stg_gmp_estimate_bits (size_t str_size, int base)
{
  int cpl = __mp_bases[base].chars_per_limb;
  unsigned long bits_ub =
    ((unsigned long) str_size * GMP_NUMB_BITS + (unsigned long) cpl - 1)
    / (unsigned long) cpl;
  return (mp_size_t) bits_ub;
}

#endif /* _STG_GMP_XSIZE_H */
