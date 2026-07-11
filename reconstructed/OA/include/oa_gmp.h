// SPDX-License-Identifier: GPL-2.0
/*
 * oa_gmp.h  -  minimal GMP mpz_t-ABI-compatible declarations for
 * cm_ComputeChallenge() (batch 55, src/auth/atmel_challenge.cpp).
 *
 * `oa_mpz_struct` is binary-layout compatible with real GMP's own
 * `__mpz_struct` (`{int _mp_alloc; int _mp_size; mp_limb_t *_mp_d;}`,
 * `mp_limb_t` == `unsigned long` on the 32-bit target) -- NOT copied
 * from a real gmp.h (this freestanding build has none), just the three
 * fields cm_ComputeChallenge's own ground-truth disassembly reads
 * directly at offsets +0/+4/+8 (`_mp_alloc`/`_mp_size`/`_mp_d`).
 *
 * The nine `__gmpz_*` symbols below are the SAME real, non-obfuscated
 * GMP internal names ground truth's own `sdflkjsvnd2g` (cm_ComputeChallenge)
 * calls -- confirmed genuinely `U` (external) in ground truth OA.ko's own
 * `nm -u`, resolved at insmod time by `STGGmp.ko`, a real, already-
 * reconstructed companion kernel module wrapping stock GMP 4.2.x for
 * this exact kernel (see docs/modules/STGGmp.ko.md, reconstructed/STGGmp/
 * STGGmpModule.c). Declared here as plain (non-`extern "C"`-wrapped
 * where matching oa_atmel.h's own style) externs relying on this
 * project's Kbuild-wide `-mregparm=3` default for the real `.ko` build
 * (matching ground truth's own EAX/EDX/ECX-then-stack argument-passing
 * shape at every one of these call sites, confirmed via objdump -dr) --
 * no explicit `regparm` attribute needed here, same convention as this
 * header's sibling oa_atmel.h.
 *
 * verify/test_atmel_setup.cpp provides its own minimal, CLEARLY NOT
 * cryptographically-faithful host-only bignum shim implementing these
 * nine symbols against this exact struct layout -- see that file's own
 * header comment for why a full bit-exact GMP port isn't needed for
 * this batch's purposes.
 */

#ifndef OA_GMP_H
#define OA_GMP_H

extern "C" {

typedef struct {
	int _mp_alloc;
	int _mp_size;
	unsigned long *_mp_d;
} oa_mpz_struct;

/* Matches real GMP's own `mpz_t` idiom (an array-of-1 struct) so plain
 * `oa_mpz_t x;` decays to `oa_mpz_struct *` automatically wherever it is
 * passed to one of the functions below. */
typedef oa_mpz_struct oa_mpz_t[1];

void __gmpz_init(oa_mpz_struct *x);
int  __gmpz_init_set_str(oa_mpz_struct *x, const char *s, int base);
void __gmpz_init_set_ui(oa_mpz_struct *x, unsigned long v);
void __gmpz_mul_2exp(oa_mpz_struct *r, const oa_mpz_struct *u, unsigned long cnt);
void __gmpz_add_ui(oa_mpz_struct *r, const oa_mpz_struct *u, unsigned long v);
void __gmpz_mul(oa_mpz_struct *r, const oa_mpz_struct *u, const oa_mpz_struct *v);
void __gmpz_tdiv_r(oa_mpz_struct *r, const oa_mpz_struct *n, const oa_mpz_struct *d);
void __gmpz_powm(oa_mpz_struct *r, const oa_mpz_struct *b, const oa_mpz_struct *e, const oa_mpz_struct *m);
void __gmpz_clear(oa_mpz_struct *x);

} /* extern "C" */

#endif /* OA_GMP_H */
