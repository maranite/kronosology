// SPDX-License-Identifier: GPL-2.0
/*
 * test_atmel_setup.cpp  -  host-side end-to-end KAT for
 * SetupAtmelForAuthorizations() (batch 55).
 *
 * THIS is the test that answers the sec 10.206 question directly: does
 * SetupAtmelForAuthorizations() -- with its own five previously-stubbed
 * AT88 GPA-handshake dependencies (cm_SetUserZone/nv2ac_dispatch_cmd/
 * nv2ac_enable_cipher/nv2ac_enable_encrypt/cm_ComputeChallenge, all
 * given real bodies this batch: src/auth/atmel_challenge.cpp,
 * src/auth/nv2ac_handshake.cpp, and the nv2ac_dispatch_cmd addition to
 * src/auth/atmel_zone_io.cpp) -- now genuinely reach `return 0` given a
 * well-behaved chip, using its REAL logic all the way down, not a
 * step-level mock. Links the REAL atmel_setup.cpp, atmel_zone_io.cpp,
 * atmel_deax.cpp, atmel_primitives.cpp, atmel_challenge.cpp,
 * nv2ac_handshake.cpp -- mocks provided ONLY at the genuine hardware/
 * kernel boundary: stgNV2AC_sync_cmd/stgNV2AC_sync_read_cmd (the real
 * AT88/NV2AC driver, exported at insmod time by AT88VirtualChip.ko on
 * real/virtual hardware), msleep, get_random_bytes, and a host-only GMP
 * shim for the nine __gmpz_* symbols cm_ComputeChallenge calls (real
 * ground truth resolves these against STGGmp.ko at insmod time, see
 * include/oa_gmp.h's own header comment).
 *
 * The GMP shim below is a plain fixed-capacity limb-array bignum --
 * CLEARLY NOT a cryptographically faithful GMP port. This is a
 * deliberate scope decision, not an oversight: cm_ComputeChallenge()'s
 * own real control flow (confirmed via full disassembly,
 * atmel_challenge.cpp's own header comment) has NO branch depending on
 * the actual numeric GPA result once its two decimal-string-length
 * checks pass -- so proving this exact call sequence/buffer shape runs
 * to completion and returns 0 does not require bit-exact modular
 * arithmetic, only a bignum implementation correct enough not to
 * crash/hang/overflow its fixed capacity for the specific value ranges
 * this function's own real P/G/Q constants and chipConfig/exponent
 * shapes produce (worst case ~2177 bits, the one-time `1 <<
 * ((sel+17)*128)` initial shift for sel=0 -- BN_LIMBS=40 (2560 bits)
 * below is sized with headroom above that). No real hardware GPA-
 * modexp capture exists to check bit-values against anyway (unlike
 * cm_AuthenEncryptMAC's own hardware-verified vectors, atmel_deax.cpp).
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-55s %ld\n", label, got); return; }
	printf("  FAILED: %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ============================================================
 * Host-only minimal GMP shim (see header comment above).
 * ============================================================ */
#define BN_LIMBS 40
typedef unsigned long limb_t;

static void bn_zero(limb_t *d) { for (int i = 0; i < BN_LIMBS; i++) d[i] = 0; }

static int bn_nonzero(const limb_t *d)
{
	for (int i = 0; i < BN_LIMBS; i++)
		if (d[i]) return 1;
	return 0;
}

static int bn_bitlen(const limb_t *a)
{
	for (int i = BN_LIMBS - 1; i >= 0; i--) {
		if (a[i]) {
			for (int b = 63; b >= 0; b--)
				if ((a[i] >> b) & 1UL) return i * 64 + b + 1;
		}
	}
	return 0;
}

static int bn_cmp(const limb_t *a, const limb_t *b)
{
	for (int i = BN_LIMBS - 1; i >= 0; i--)
		if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
	return 0;
}

static void bn_sub(limb_t *a, const limb_t *b) /* a -= b, assumes a >= b */
{
	__int128 borrow = 0;
	for (int i = 0; i < BN_LIMBS; i++) {
		__int128 v = (__int128)a[i] - (__int128)b[i] - borrow;
		if (v < 0) { v += ((__int128)1 << 64); borrow = 1; } else borrow = 0;
		a[i] = (limb_t)v;
	}
}

static void bn_shl1(limb_t *d)
{
	limb_t carry = 0;
	for (int i = 0; i < BN_LIMBS; i++) {
		limb_t newCarry = d[i] >> 63;
		d[i] = (d[i] << 1) | carry;
		carry = newCarry;
	}
}

static void bn_mod(limb_t *r, const limb_t *n, const limb_t *m) /* r = n mod m */
{
	limb_t rem[BN_LIMBS]; bn_zero(rem);
	int nbits = bn_bitlen(n);
	for (int i = nbits - 1; i >= 0; i--) {
		int limb = i / 64, bit = i % 64;
		bn_shl1(rem);
		rem[0] |= (n[limb] >> bit) & 1UL;
		if (bn_cmp(rem, m) >= 0)
			bn_sub(rem, m);
	}
	for (int i = 0; i < BN_LIMBS; i++) r[i] = rem[i];
}

static void bn_mul(limb_t *r, const limb_t *a, const limb_t *b) /* schoolbook, truncated to BN_LIMBS */
{
	limb_t res[BN_LIMBS];
	for (int i = 0; i < BN_LIMBS; i++) res[i] = 0;
	for (int i = 0; i < BN_LIMBS; i++) {
		if (a[i] == 0) continue;
		unsigned __int128 carry = 0;
		for (int j = 0; j < BN_LIMBS - i; j++) {
			unsigned __int128 p = (unsigned __int128)a[i] * b[j] + res[i + j] + carry;
			res[i + j] = (limb_t)p;
			carry = p >> 64;
		}
	}
	for (int i = 0; i < BN_LIMBS; i++) r[i] = res[i];
}

static void bn_powm(limb_t *r, const limb_t *base, const limb_t *exp, const limb_t *mod)
{
	limb_t result[BN_LIMBS]; bn_zero(result); result[0] = 1;
	limb_t b[BN_LIMBS];
	bn_mod(b, base, mod);

	int ebits = bn_bitlen(exp);
	for (int i = ebits - 1; i >= 0; i--) {
		limb_t sq[BN_LIMBS];
		bn_mul(sq, result, result);
		bn_mod(result, sq, mod);
		int limb = i / 64, bit = i % 64;
		if ((exp[limb] >> bit) & 1UL) {
			limb_t pr[BN_LIMBS];
			bn_mul(pr, result, b);
			bn_mod(result, pr, mod);
		}
	}
	for (int i = 0; i < BN_LIMBS; i++) r[i] = result[i];
}

extern "C" {

struct oa_mpz_struct_local { int _mp_alloc; int _mp_size; unsigned long *_mp_d; };

void __gmpz_init(oa_mpz_struct_local *x)
{
	x->_mp_d = (unsigned long *)malloc(BN_LIMBS * sizeof(unsigned long));
	bn_zero(x->_mp_d);
	x->_mp_alloc = BN_LIMBS;
	x->_mp_size = 0;
}

int __gmpz_init_set_str(oa_mpz_struct_local *x, const char *s, int base)
{
	(void)base;
	__gmpz_init(x);
	limb_t *d = x->_mp_d;
	limb_t ten[BN_LIMBS]; bn_zero(ten); ten[0] = 10;
	for (const char *p = s; *p; p++) {
		if (*p < '0' || *p > '9') continue;
		limb_t tmp[BN_LIMBS];
		bn_mul(tmp, d, ten);
		unsigned __int128 carry = (unsigned)(*p - '0');
		for (int i = 0; i < BN_LIMBS && carry; i++) {
			unsigned __int128 v = (unsigned __int128)tmp[i] + carry;
			tmp[i] = (limb_t)v;
			carry = v >> 64;
		}
		for (int i = 0; i < BN_LIMBS; i++) d[i] = tmp[i];
	}
	x->_mp_size = bn_nonzero(d) ? 1 : 0;
	return 0;
}

void __gmpz_init_set_ui(oa_mpz_struct_local *x, unsigned long v)
{
	__gmpz_init(x);
	x->_mp_d[0] = v;
	x->_mp_size = v ? 1 : 0;
}

void __gmpz_mul_2exp(oa_mpz_struct_local *r, const oa_mpz_struct_local *u, unsigned long cnt)
{
	limb_t tmp[BN_LIMBS];
	for (int i = 0; i < BN_LIMBS; i++) tmp[i] = u->_mp_d[i];
	for (unsigned long i = 0; i < cnt; i++) bn_shl1(tmp);
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}

void __gmpz_add_ui(oa_mpz_struct_local *r, const oa_mpz_struct_local *u, unsigned long v)
{
	limb_t tmp[BN_LIMBS];
	for (int i = 0; i < BN_LIMBS; i++) tmp[i] = u->_mp_d[i];
	unsigned __int128 carry = v;
	for (int i = 0; i < BN_LIMBS && carry; i++) {
		unsigned __int128 s = (unsigned __int128)tmp[i] + carry;
		tmp[i] = (limb_t)s;
		carry = s >> 64;
	}
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}

void __gmpz_mul(oa_mpz_struct_local *r, const oa_mpz_struct_local *u, const oa_mpz_struct_local *v)
{
	limb_t tmp[BN_LIMBS];
	bn_mul(tmp, u->_mp_d, v->_mp_d);
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}

void __gmpz_tdiv_r(oa_mpz_struct_local *r, const oa_mpz_struct_local *n, const oa_mpz_struct_local *d)
{
	limb_t tmp[BN_LIMBS];
	bn_mod(tmp, n->_mp_d, d->_mp_d);
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}

void __gmpz_powm(oa_mpz_struct_local *r, const oa_mpz_struct_local *b, const oa_mpz_struct_local *e, const oa_mpz_struct_local *m)
{
	limb_t tmp[BN_LIMBS];
	bn_powm(tmp, b->_mp_d, e->_mp_d, m->_mp_d);
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}

void __gmpz_clear(oa_mpz_struct_local *x)
{
	free(x->_mp_d);
	x->_mp_d = 0;
	x->_mp_alloc = 0;
	x->_mp_size = 0;
}

} /* extern "C" */

/* ============================================================
 * Hardware/kernel boundary mocks -- same established pattern as
 * verify/test_atmel_zone_io.cpp's own g_fakeChip.
 * ============================================================ */
static int g_readCalls, g_syncCalls;
static int g_forceReadFail, g_forceSyncFail;
static unsigned char g_fakeChip[32];

extern "C" int stgNV2AC_sync_read_cmd(unsigned char *cmd, unsigned char *out, int unused)
{
	(void)cmd; (void)unused;
	g_readCalls++;
	if (g_forceReadFail)
		return -1;
	memcpy(out, g_fakeChip, 32);
	return 0;
}

extern "C" int stgNV2AC_sync_cmd(unsigned char *cmd, int param)
{
	(void)cmd; (void)param;
	g_syncCalls++;
	if (g_forceSyncFail)
		return -1;
	return 0;
}

extern "C" void msleep(unsigned int) { }

extern "C" void get_random_bytes(void *buf, unsigned int len)
{
	unsigned char *p = (unsigned char *)buf;
	for (unsigned int i = 0; i < len; i++)
		p[i] = (unsigned char)(0x11 * (i + 1));
}

int SetupAtmelForAuthorizations(void);

int main()
{
	printf("[1] SetupAtmelForAuthorizations() -- full real chain, well-behaved\n"
	       "    chip (every status byte reads back as 0xff, the real chip's own\n"
	       "    \"verified\" sentinel) -- THE deliverable-#1 question: does this\n"
	       "    now genuinely return 0?\n");
	{
		for (int i = 0; i < 32; i++) g_fakeChip[i] = 0xff;
		g_readCalls = g_syncCalls = g_forceReadFail = g_forceSyncFail = 0;

		int rc = SetupAtmelForAuthorizations();

		check_eq("  SetupAtmelForAuthorizations() returns 0", rc, 0);
		check_eq("  at least one AT88 read happened", g_readCalls > 0, 1);
		check_eq("  at least one AT88 command dispatch happened", g_syncCalls > 0, 1);
	}

	printf("[2] SetupAtmelForAuthorizations() -- config-zone read (0x19)\n"
	       "    driver failure -> rc == -1, never reaches cm_ComputeChallenge\n");
	{
		for (int i = 0; i < 32; i++) g_fakeChip[i] = 0xff;
		g_readCalls = g_syncCalls = 0;
		g_forceReadFail = 1;
		g_forceSyncFail = 0;

		int rc = SetupAtmelForAuthorizations();

		check_eq("  rc == -1", rc, -1);
		g_forceReadFail = 0;
	}

	printf("[3] SetupAtmelForAuthorizations() -- cm_SetUserZone/nv2ac_dispatch_cmd\n"
	       "    command-dispatch driver failure -> rc == -2\n");
	{
		for (int i = 0; i < 32; i++) g_fakeChip[i] = 0xff;
		g_readCalls = 0;
		g_forceReadFail = 0;
		g_forceSyncFail = 1;

		int rc = SetupAtmelForAuthorizations();

		/* cm_ReadUserZone(0x19,...) (a read, opcode 0xb6) succeeds first
		 * (g_forceReadFail is off); nv2ac_dispatch_cmd's own internal
		 * sync_cmd call happens next but its return is deliberately
		 * ignored by ground truth (see atmel_zone_io.cpp's own header
		 * comment) -- the FIRST real failure this chip-misbehavior
		 * actually surfaces is cm_SetUserZone's own direct
		 * stgNV2AC_sync_cmd call, giving rc == -2. */
		check_eq("  rc == -2", rc, -2);
		g_forceSyncFail = 0;
	}

	printf("[4] SetupAtmelForAuthorizations() -- chip never returns the 0xff\n"
	       "    \"verified\" sentinel (round 1, nv2ac_enable_cipher) -> rc == -3\n");
	{
		for (int i = 0; i < 32; i++) g_fakeChip[i] = 0x00; /* never 0xff */
		g_forceReadFail = g_forceSyncFail = 0;

		int rc = SetupAtmelForAuthorizations();

		check_eq("  rc == -3", rc, -3);
	}

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
	return g_fail ? 1 : 0;
}
