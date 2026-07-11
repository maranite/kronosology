/* test_xsize_sweep.c -- STGGmp.ko xsize-patch KAT, part 1/2.
 *
 * Direct numeric safety-margin sweep: for every base GMP defines
 * (2..62 -- table slots for bases 0/1/63..256 have chars_per_limb == 0
 * and are skipped, they're either unused or reserved) and str_size
 * 0..8192 (far beyond the ~63-char practical bound any real caller in
 * this project ever uses -- cm_ComputeChallenge's own catBuf is 0x40
 * bytes), compute both:
 *
 *   (a) the ORIGINAL double-precision formula (mpz/set_str.c:127,
 *       stock unmodified upstream GMP), using this host's own real
 *       libgcc/libm and the REAL mp_bases[] table compiled straight
 *       from the staged gmp/mpn/mp_bases.c (not a hand-copied table --
 *       linked directly, see run_xsize_kat.sh);
 *   (b) our integer-only replacement, stg_gmp_estimate_bits()
 *       (gmp/stg_gmp_xsize.h).
 *
 * Assert (b) >= (a) in EVERY case -- the exact safety property xsize
 * must have, since mpn_set_str() (mpn/set_str.c) writes into a buffer
 * pre-sized by this estimate with no further bounds check. Any failure
 * here is a real potential kernel heap overflow, not a cosmetic
 * mismatch -- this program's exit code is the pass/fail signal.
 */
#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "stg_gmp_xsize.h"

int main(void)
{
	long failures = 0;
	double max_rel_slack = 0.0;
	long checked = 0;

	for (int base = 2; base <= 62; base++) {
		if (mp_bases[base].chars_per_limb == 0)
			continue;
		for (unsigned long str_size = 0; str_size <= 8192; str_size++) {
			mp_size_t orig =
			    (mp_size_t)(str_size / mp_bases[base].chars_per_bit_exactly);
			mp_size_t patched = stg_gmp_estimate_bits(str_size, base);
			checked++;
			if (patched < orig) {
				failures++;
				if (failures <= 10)
					printf("UNDERESTIMATE base=%d str_size=%lu orig=%ld patched=%ld\n",
					       base, str_size, (long)orig, (long)patched);
			} else if (orig > 0) {
				double rel = (double)(patched - orig) / (double)orig;
				if (rel > max_rel_slack)
					max_rel_slack = rel;
			}
		}
	}
	printf("checked=%ld failures=%ld max_rel_slack=%.6f%%\n",
	       checked, failures, max_rel_slack * 100.0);
	return failures ? 1 : 0;
}
