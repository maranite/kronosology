/* test_xsize_patch_driver.c -- STGGmp.ko xsize-patch KAT, part 2/2 (see
 * run_xsize_kat.sh for part 1, the numeric safety-margin sweep).
 *
 * Exercises the REAL, vendor mpz_set_str()/mpn_set_str() code path
 * (unmodified except for the ONE xsize-estimate line under test, see
 * fetch-gmp.sh's "libgcc soft-float" patch section and
 * gmp/stg_gmp_xsize.h) against a battery of decimal (and a few
 * other-base) strings, and prints the resulting mpz_t internals
 * (_mp_size, _mp_alloc, and the raw limb array) in a canonical text
 * form.
 *
 * run_xsize_kat.sh compiles this TWICE -- once against the untouched
 * vendor mpz/set_str.c.orig (float xsize, needs real host libgcc/libm,
 * fine on a host build) and once against the patched mpz/set_str.c
 * (integer xsize) -- and diffs the two outputs. Per-line diffs are
 * expected ONLY in the `alloc=` field (the internal pre-allocation
 * hint this patch changes, always >= the original per
 * gmp/stg_gmp_xsize.h's safety proof); `size=`/`limbs=` (the actual
 * computed bignum result) must be byte-identical in every case, proving
 * the patch has zero effect on any user-visible/cryptographic output.
 */
#include <stdio.h>
#include <string.h>
#include "gmp.h"

extern void mpz_init(mpz_ptr);
extern int mpz_set_str(mpz_ptr, const char *, int);
extern void mpz_clear(mpz_ptr);

static void dump(const char *label, const char *s, int base)
{
	mpz_t x;
	mpz_init(x);
	int rc = mpz_set_str(x, s, base);
	printf("%s base=%d rc=%d size=%d alloc=%d limbs=",
	       label, base, rc, x->_mp_size, x->_mp_alloc);
	int n = x->_mp_size < 0 ? -x->_mp_size : x->_mp_size;
	for (int i = 0; i < n; i++)
		printf("%08lx%s", (unsigned long)x->_mp_d[i], (i + 1 < n) ? "," : "");
	printf("\n");
	mpz_clear(x);
}

int main(void)
{
	/* Base-10 cases -- the ONLY base cm_ComputeChallenge ever actually
	 * passes (reconstructed/OA/src/auth/atmel_challenge.cpp), including
	 * lengths well past the ~63-byte catBuf practical bound for margin. */
	const char *dec[] = {
		"0", "1", "9", "10", "99", "100",
		"12345678901234567890",
		"99999999999999999999999999999999999999",
		"170141183460469231731687303715884105728",  /* 2^127 */
		"340282366920938463463374607431768211456",  /* 2^128 */
		"  42  ",              /* leading/trailing space */
		"-123456789",          /* negative */
		"00000123",             /* leading zeros */
		"3141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909145648566923460348610454326648213393607260249141273724587006606315588174881520920962829254091715364367892590360011330530548820466521384146951941511609",
		"9223372036854775807",
		"18446744073709551616",
		"0",
		NULL
	};
	for (int i = 0; dec[i]; i++)
		dump("DEC", dec[i], 10);

	/* Sanity spread across other bases -- not exercised by any real
	 * caller today, but the stg_gmp_estimate_bits() replacement is
	 * general (uses chars_per_limb, not a base-10-only constant), so
	 * verifying it doesn't change behavior for other bases too is a
	 * free, cheap extra confidence check. */
	dump("HEX", "deadbeefcafebabe1234567890abcdef", 16);
	dump("OCT", "1234567012345670123456701234567", 8);
	dump("BIN", "1010110100110101101001101011010011010110100110101101011", 2);
	dump("B36", "z1y2x3w4v5u6t7s8r9q0", 36);

	return 0;
}
