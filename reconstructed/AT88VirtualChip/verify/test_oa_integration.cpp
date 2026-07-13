// SPDX-License-Identifier: GPL-2.0
/*
 * test_oa_integration.cpp  -  REAL cross-module integration test (sec
 * 10.233): links OA.ko's actual reconstructed SetupAtmelForAuthorizations()
 * call chain (reconstructed/OA/src/auth/*.cpp, completely unmodified)
 * directly against this module's actual chip-side logic
 * (chip_state.cpp/b8_handshake.cpp/bignum.cpp/nv2ac_exports.cpp), with
 * NEITHER side mocked at the crypto/protocol level.
 *
 * Why this test exists and what it catches that no other test here does:
 * reconstructed/OA/verify/test_atmel_setup.cpp mocks stgNV2AC_sync_cmd as an
 * unconditional "return 0", so it can never catch a bug in what that
 * function's real implementation (this module's nv2ac_exports.cpp) actually
 * returns. verify/test_nv2ac_exports.cpp (this directory) self-generates its
 * own challenge/response using this module's OWN algorithm, so it can never
 * catch a disagreement between this module and OA.ko's real, independently
 * reconstructed GMP-based cm_ComputeChallenge/cm_AuthenEncryptMAC. Both
 * individually-correct-looking test suites missed a real ABI bug that only
 * surfaces when the two real subsystems are actually linked together and
 * exercised end to end -- exactly the gap this test fills.
 *
 * This test caught TWO real, independently confirmed bugs during
 * development (see MASTER_REFERENCE.md sec 10.233 for the full
 * disassembly-level root-causing of both):
 *
 *   1. nv2ac_exports.cpp's stgNV2AC_sync_cmd() used to be declared `void`,
 *      but OA.ko's real ABI (reconstructed/OA/src/auth/atmel_zone_io.cpp /
 *      nv2ac_handshake.cpp) expects `int`, and checks it. Confirmed via
 *      `objdump -d` on the real target-flags-compiled object that the `void`
 *      version left an essentially-always-nonzero garbage value (a stack
 *      pointer, left over from marshalling the 4th regparm-overflow
 *      argument) in EAX -- OA.ko's caller read that as "driver failed" on
 *      EVERY $B8 dispatch, unconditionally, independent of any chip data.
 *      This alone reproduces the exact live-boot symptom
 *      (`SetupAtmelForAuthorizations failed, result=-3`) on this host, with
 *      the REAL captured KronosExtract.bin loaded -- not a data problem at
 *      all. Fixed by giving the function its real `int` return.
 *
 *   2. Separately, even with bug #1 fixed, a synthetic/all-zero chip (no
 *      real captured blob loaded -- the actual VM/no-hardware scenario)
 *      STILL failed with -3: at88_chip_handle_b8()'s own update_aac() only
 *      ever increments the AAC byte (configZone[0x50]) from whatever it
 *      already was, saturating at 0xff -- it never jumps straight to 0xff.
 *      OA.ko's own Nv2acVerifyRound requires reading back EXACTLY 0xff to
 *      consider a round verified. A brand-new all-zero chip starts this
 *      byte at 0, so its FIRST-EVER handshake bumps it only to 1, which is
 *      not the expected sentinel -- an accidental impedance mismatch
 *      between "AAC as an anti-brute-force decrement-on-reject counter"
 *      (which real hardware's steady state keeps saturated at 0xff after
 *      any history of successful use) and "brand new synthetic chip that
 *      has no such history". Fixed by adding at88_chip_load_synthetic()
 *      (chip_state.cpp), which pre-saturates the AAC bytes at 0x50/0x60/
 *      0x70/0x80 to 0xff, and wiring it into module_main.c's
 *      load_chip_blob_work() as the fallback for every failure path (no
 *      real hardware-extracted blob will ever exist in a VM).
 *
 * Both bugs are exercised below: [1] against the REAL captured
 * KronosExtract.bin (proving the fix works with real per-device data), and
 * [2] against at88_chip_load_synthetic() with NO blob loaded at all (proving
 * the module is self-sufficient in a VM with no real AT88 hardware, per this
 * project's own sec 10.185 RTAI/hardware-substitution policy).
 *
 * ABI notes specific to this test (see nv2ac_exports.cpp's own header
 * comment for the full derivation): stgNV2AC_sync_read_cmd's real ABI packs
 * a pointer into `int`, lossless on the 32-bit target but corrupting on this
 * 64-bit host -- nv2ac_exports.cpp is compiled with
 * `-DstgNV2AC_sync_read_cmd=<renamed>` (see the Makefile rule for this test)
 * so its real (lossy-on-host) wrapper doesn't collide with the pointer-safe
 * bridge below, which calls the real nv2ac_read_cmd_impl() core directly --
 * the same documented workaround test_nv2ac_exports.cpp already uses.
 * cm_ComputeChallenge's GMP calls are served by a host-only limb-array
 * bignum shim (copied from reconstructed/OA/verify/test_atmel_setup.cpp) --
 * unlike that file's own use of the shim (which only needed non-crash
 * behavior, since cm_ComputeChallenge's control flow has no data-dependent
 * branch), THIS test needs the shim to be numerically CORRECT, since it's
 * being cross-checked bit-for-bit against bignum.cpp's independently-ported
 * synth_sdflkjsvnd2g on the chip side.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "../at88_chip.h"

extern "C" int at88_chip_module_init(const unsigned char *blob, unsigned int blobLen);
extern "C" void at88_chip_module_init_synthetic(void);
extern "C" int stgNV2AC_sync_cmd(unsigned char *address, unsigned int data); /* real, fixed */
extern int nv2ac_read_cmd_impl(const unsigned char *cmd, unsigned char *out); /* real, pointer-typed core */

extern "C" int stgNV2AC_sync_read_cmd(unsigned char *cmd, unsigned char *out, int unused)
{
	(void)unused;
	return nv2ac_read_cmd_impl(cmd, out);
}

extern "C" void msleep(unsigned int) { }

extern "C" void get_random_bytes(void *buf, unsigned int len)
{
	/* Deterministic, not cryptographically random -- fine here: the chip
	 * independently verifies whatever nonce it actually receives, it does
	 * not need the nonce to be unpredictable for this test. */
	unsigned char *p = (unsigned char *)buf;
	static unsigned char ctr = 0x37;
	for (unsigned int i = 0; i < len; i++)
		p[i] = (unsigned char)(ctr++ * 0x69 + i);
}

/* ============================================================
 * Host-only GMP shim for cm_ComputeChallenge -- copied from
 * reconstructed/OA/verify/test_atmel_setup.cpp (see that file's own header
 * comment for the general rationale). Needs to be numerically correct here
 * (see this file's own header comment above for why that differs from that
 * other file's own requirement).
 * ============================================================ */
#define BN_LIMBS 40
typedef unsigned long limb_t;

static void bn_zero(limb_t *d) { for (int i = 0; i < BN_LIMBS; i++) d[i] = 0; }
static int bn_nonzero(const limb_t *d) { for (int i = 0; i < BN_LIMBS; i++) if (d[i]) return 1; return 0; }
static int bn_bitlen(const limb_t *a)
{
	for (int i = BN_LIMBS - 1; i >= 0; i--)
		if (a[i])
			for (int b = 63; b >= 0; b--)
				if ((a[i] >> b) & 1UL) return i * 64 + b + 1;
	return 0;
}
static int bn_cmp(const limb_t *a, const limb_t *b)
{
	for (int i = BN_LIMBS - 1; i >= 0; i--)
		if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
	return 0;
}
static void bn_sub(limb_t *a, const limb_t *b)
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
static void bn_mod(limb_t *r, const limb_t *n, const limb_t *m)
{
	limb_t rem[BN_LIMBS]; bn_zero(rem);
	int nbits = bn_bitlen(n);
	for (int i = nbits - 1; i >= 0; i--) {
		int limb = i / 64, bit = i % 64;
		bn_shl1(rem);
		rem[0] |= (n[limb] >> bit) & 1UL;
		if (bn_cmp(rem, m) >= 0) bn_sub(rem, m);
	}
	for (int i = 0; i < BN_LIMBS; i++) r[i] = rem[i];
}
static void bn_mul(limb_t *r, const limb_t *a, const limb_t *b)
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

int SetupAtmelForAuthorizations(void);	/* C++ linkage -- matches oa_atmel.h's own declaration */

static unsigned char *read_file(const char *path, unsigned int *outLen)
{
	FILE *f = fopen(path, "rb");
	if (!f) return nullptr;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char *buf = (unsigned char *)malloc((size_t)sz);
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return nullptr; }
	fclose(f);
	*outLen = (unsigned int)sz;
	return buf;
}

static int g_fail;
static void check(const char *label, int cond)
{
	if (cond) { printf("  ok    %s\n", label); return; }
	printf("  FAIL  %s\n", label);
	g_fail++;
}

int main()
{
	printf("OA.ko <-> AT88VirtualChip.ko real cross-module integration test\n");
	printf("=================================================================\n");

	printf("[1] Real captured chip data (KronosExtract.bin) -- was ALWAYS reproducibly\n"
	       "    broken by bug #1 (stgNV2AC_sync_cmd's void-vs-int ABI mismatch) before\n"
	       "    this fix, independent of any chip-data question\n");
	{
		unsigned int blobLen = 0;
		unsigned char *blob = read_file("/home/share/KronosExtract/build/KronosExtract.bin", &blobLen);
		if (!blob) {
			printf("  FAIL  could not read KronosExtract.bin -- is the path still valid?\n");
			g_fail++;
		} else {
			int initrc = at88_chip_module_init(blob, blobLen);
			free(blob);
			check("at88_chip_module_init(real captured blob) == 0", initrc == 0);
			int rc = SetupAtmelForAuthorizations();
			printf("       SetupAtmelForAuthorizations() = %d\n", rc);
			check("SetupAtmelForAuthorizations() == 0 (real chip data)", rc == 0);
		}
	}

	printf("[2] Synthetic chip, NO real blob loaded (the actual VM/no-hardware\n"
	       "    scenario) -- was broken by bug #2 (AAC byte must start at 0xff)\n"
	       "    even with bug #1 already fixed\n");
	{
		at88_chip_module_init_synthetic();
		int rc = SetupAtmelForAuthorizations();
		printf("       SetupAtmelForAuthorizations() = %d\n", rc);
		check("SetupAtmelForAuthorizations() == 0 (synthetic chip, no real hardware)", rc == 0);
	}

	printf("=================================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
