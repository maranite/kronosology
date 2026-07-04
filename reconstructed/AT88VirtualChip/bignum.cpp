// SPDX-License-Identifier: GPL-2.0
/*
 * bignum.cpp  -  see bignum.h. Ported 1:1 from kronos_extract.c's own
 * ke_bn5_ helpers and ke_synth_sdflkjsvnd2g, renamed to this project's
 * naming only.
 */

#include "bignum.h"

/* N1 = int("275870082984435801508285927170653268036") */
static const unsigned int BN_N1[BN_WORDS5] = {
	0xbe7ba844u, 0x7089aef5u, 0x2f182eeeu, 0xcf8aa536u, 0x0u
};
/* N2 = int("275870082984435801541504784743492877417") */
static const unsigned int BN_N2[BN_WORDS5] = {
	0xdfe1fc69u, 0x3d8ac5ebu, 0x2f182ef0u, 0xcf8aa536u, 0x0u
};
/* e0 = pow(2, 2176, N1) -- initial exponent before the first doubling */
static const unsigned int BN_E0[BN_WORDS5] = {
	0xfd6d0f6cu, 0xcf14b069u, 0xcff9149au, 0xc3a25b8du, 0x0u
};

/* -1 / 0 / +1 comparison */
static int bn5_cmp(const unsigned int *a, const unsigned int *b)
{
	for (int i = 4; i >= 0; i--) {
		if (a[i] > b[i]) return  1;
		if (a[i] < b[i]) return -1;
	}
	return 0;
}

/* a -= b in-place; caller guarantees a >= b */
static void bn5_sub(unsigned int *a, const unsigned int *b)
{
	unsigned long long borrow = 0;
	for (int i = 0; i < BN_WORDS5; i++) {
		unsigned long long d = (unsigned long long)a[i] - (unsigned long long)b[i] - borrow;
		a[i] = (unsigned int)d;
		borrow = d >> 63;
	}
}

/* r <<= 1 */
static void bn5_shl1(unsigned int *r)
{
	for (int i = 4; i > 0; i--)
		r[i] = (r[i] << 1) | (r[i-1] >> 31);
	r[0] <<= 1;
}

/* bit 'pos' of x[9] */
static unsigned int bn9_bit(const unsigned int *x, int pos)
{
	return (x[pos >> 5] >> (pos & 31)) & 1u;
}

/* r[5] = x[9] mod n[5] -- binary long division (288 iterations) */
static void bn5_mod(unsigned int *r, const unsigned int *x, const unsigned int *n)
{
	for (int i = 0; i < BN_WORDS5; i++)
		r[i] = 0;
	for (int i = 287; i >= 0; i--) {
		bn5_shl1(r);
		r[0] |= bn9_bit(x, i);
		if (bn5_cmp(r, n) >= 0)
			bn5_sub(r, n);
	}
}

/* dst[9] = a[5] * b[5] -- schoolbook */
static void bn5_mul(unsigned int *dst, const unsigned int *a, const unsigned int *b)
{
	for (int i = 0; i < BN_WORDS9; i++)
		dst[i] = 0;
	for (int i = 0; i < BN_WORDS5; i++) {
		unsigned long long carry = 0;
		for (int j = 0; j < BN_WORDS5; j++) {
			unsigned long long t = (unsigned long long)a[i] * (unsigned long long)b[j]
					      + (unsigned long long)dst[i+j] + carry;
			dst[i+j] = (unsigned int)t;
			carry    = t >> 32;
		}
		dst[i + BN_WORDS5] += (unsigned int)carry;
	}
}

/* r[5] = (a * b) mod n -- safe even when r aliases a or b */
static void bn5_modmul(unsigned int *r, const unsigned int *a, const unsigned int *b, const unsigned int *n)
{
	unsigned int tmp[BN_WORDS9];
	bn5_mul(tmp, a, b);
	bn5_mod(r, tmp, n);
}

void bn5_modexp_arr(unsigned int *r, const unsigned int *base,
		    const unsigned int *exp, const unsigned int *n)
{
	unsigned int cur[BN_WORDS5];
	r[0] = 1;
	for (int i = 1; i < BN_WORDS5; i++)
		r[i] = 0;
	for (int i = 0; i < BN_WORDS5; i++)
		cur[i] = base[i];
	for (int i = 0; i < 128; i++) {
		if ((exp[i >> 5] >> (i & 31)) & 1)
			bn5_modmul(r, r, cur, n);
		if (i < 127)
			bn5_modmul(cur, cur, cur, n);
	}
}

void synth_sdflkjsvnd2g(const unsigned char *idn, unsigned char *p2)
{
	unsigned int m[BN_WORDS5], e[BN_WORDS5], result[BN_WORDS5];
	unsigned int tmp[BN_WORDS9];
	unsigned char buf[16];

	/* Build idn as 56-bit LE integer: idn[0]=LSB, idn[6]=MSB */
	for (int i = 0; i < BN_WORDS5; i++)
		m[i] = 0;
	m[0] = ((unsigned int)idn[3] << 24) | ((unsigned int)idn[2] << 16) |
	       ((unsigned int)idn[1] << 8)  | idn[0];
	m[1] = ((unsigned int)idn[6] << 16) | ((unsigned int)idn[5] << 8) | idn[4];

	/* m = m^2 (result < 2^112 < N2, no modular reduction needed) */
	bn5_mul(tmp, m, m);
	for (int i = 0; i < BN_WORDS5; i++)
		m[i] = tmp[i];

	/* e starts at 2^2176 mod N1; the loop doubles before each use */
	for (int i = 0; i < BN_WORDS5; i++)
		e[i] = BN_E0[i];

	for (int i = 0; i < 16; i++)
		buf[i] = 0;

	for (int outer = 0; outer < 16; outer++) {
		for (int inner = 0; inner < 8; inner++) {
			/* e = (e * 2) mod N1 -- invariant: e < N1 < 2^128 -> e[4] stays 0 */
			bn5_shl1(e);
			if (bn5_cmp(e, BN_N1) >= 0)
				bn5_sub(e, BN_N1);
			/* result = m^e mod N2 */
			bn5_modexp_arr(result, m, e, BN_N2);
			if (result[0] & 1)
				buf[outer] |= (unsigned char)(1u << inner);
		}
	}

	/* Select output bytes at indices {1,2,3,5,7,11,13,15} */
	p2[0] = buf[1];  p2[1] = buf[2];  p2[2] = buf[3];
	p2[3] = buf[5];  p2[4] = buf[7];  p2[5] = buf[11];
	p2[6] = buf[13]; p2[7] = buf[15];
}
