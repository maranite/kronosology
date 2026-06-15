// SPDX-License-Identifier: GPL-2.0
/*
 * test_klm_auth.c  -  host-side known-answer test for the OA copy-protection math.
 *
 * This is the "layer C" (behavioural) verification for the Stage-1 auth cluster: it
 * exercises the *pure* arithmetic recovered from OA_322.ko — oa_fnv1a16() and
 * oa_auth_value() from include/oa_authmath.h — independently of the kernel.
 *
 * It proves three things:
 *
 *   1. CORE CORRECTNESS.  The FNV-1a machinery (xor-then-multiply, prime 0x01000193)
 *      reproduces the *published* FNV-1a-32 test vectors when fed the *standard* offset
 *      basis (0x811c9dc5).  This anchors the algorithm to a third-party reference, so a
 *      transcription error in the loop body cannot pass silently.
 *
 *   2. KORG BASIS.  oa_fnv1a16() uses Korg's custom offset basis (0x050c5d1f) as recovered
 *      from the binary; we anchor a golden hash of a fixed 16-byte identity as a regression
 *      guard so the basis constant can never be edited unnoticed.
 *
 *   3. AUTHORIZATION ROUND-TRIP.  oa_auth_value() is the runtime-keyed stamp.  We model the
 *      manager's authorize→verify cycle and assert: a stamp made on this boot verifies; a
 *      tampered id/extra/stamp fails; and the *same* identity stamped under a different boot
 *      key fails (the property that makes an offline-forged or cross-boot-copied stamp
 *      useless).
 *
 * Build & run:   cc -Wall -Wextra -O2 -I../include test_klm_auth.c -o test_klm_auth && ./test_klm_auth
 */

#include <stdio.h>
#include <string.h>
#include "oa_authmath.h"

static int g_fail;

#define CHECK_EQ(label, got, want)                                              \
	do {                                                                    \
		unsigned int _g = (unsigned int)(got), _w = (unsigned int)(want); \
		if (_g != _w) {                                                 \
			printf("  FAIL  %-34s got=0x%08x want=0x%08x\n",       \
			       (label), _g, _w);                              \
			g_fail++;                                              \
		} else {                                                       \
			printf("  ok    %-34s 0x%08x\n", (label), _g);        \
		}                                                              \
	} while (0)

#define CHECK_TRUE(label, cond)                                                 \
	do {                                                                    \
		if (!(cond)) {                                                  \
			printf("  FAIL  %-34s (expected true)\n", (label));   \
			g_fail++;                                              \
		} else {                                                       \
			printf("  ok    %-34s true\n", (label));              \
		}                                                              \
	} while (0)

#define CHECK_FALSE(label, cond)                                                \
	do {                                                                    \
		if (cond) {                                                     \
			printf("  FAIL  %-34s (expected false)\n", (label));  \
			g_fail++;                                              \
		} else {                                                       \
			printf("  ok    %-34s false\n", (label));             \
		}                                                              \
	} while (0)

/* Generic FNV-1a so we can validate the loop against the *standard* basis/vectors. */
static unsigned int fnv1a(const unsigned char *p, unsigned int n, unsigned int basis)
{
	unsigned int h = basis;
	unsigned int i;
	for (i = 0; i < n; i++)
		h = (h ^ p[i]) * OA_FNV1A_PRIME;
	return h;
}

#define FNV1A_STD_BASIS 0x811c9dc5u

static void test_fnv_core_against_standard(void)
{
	/* Published FNV-1a 32-bit vectors (offset basis 0x811c9dc5). */
	printf("[1] FNV-1a core vs. published standard vectors\n");
	CHECK_EQ("fnv1a(\"\")",       fnv1a((const unsigned char *)"",       0, FNV1A_STD_BASIS), 0x811c9dc5u);
	CHECK_EQ("fnv1a(\"a\")",      fnv1a((const unsigned char *)"a",      1, FNV1A_STD_BASIS), 0xe40c292cu);
	CHECK_EQ("fnv1a(\"foobar\")", fnv1a((const unsigned char *)"foobar", 6, FNV1A_STD_BASIS), 0xbf9cf968u);
}

static void test_korg_basis(void)
{
	/*
	 * oa_fnv1a16() must equal the generic loop over 16 bytes with Korg's basis,
	 * and reproduce a fixed golden value for a fixed identity.
	 */
	unsigned char id[16];
	unsigned int i;
	for (i = 0; i < 16; i++)
		id[i] = (unsigned char)(0x10 + i);	/* 10 11 12 ... 1f */

	printf("[2] oa_fnv1a16() — Korg custom basis 0x%08x\n", OA_FNV1A_OFFSET);
	CHECK_EQ("basis constant",   OA_FNV1A_OFFSET, 0x050c5d1fu);
	CHECK_EQ("prime constant",   OA_FNV1A_PRIME,  0x01000193u);
	CHECK_EQ("oa_fnv1a16==generic", oa_fnv1a16(id), fnv1a(id, 16, OA_FNV1A_OFFSET));
	/* GOLDEN: regression guard for the recovered basis+prime; must stay constant. */
	CHECK_EQ("oa_fnv1a16(10..1f)", oa_fnv1a16(id), 0x5d094a1fu);
}

/* Mirror of CSTGKLMManager's authorize/verify, reduced to its arithmetic. */
static unsigned int authorize(unsigned int id, unsigned int extra, unsigned int bootKey)
{
	return oa_auth_value(id, extra, bootKey);	/* what SET_AUTH would store */
}
static int verify(unsigned int stamped, unsigned int id, unsigned int extra, unsigned int bootKey)
{
	return stamped == oa_auth_value(id, extra, bootKey);
}

static void test_auth_roundtrip(void)
{
	const unsigned int bootKey  = 0x9e3779b1u;	/* a representative per-boot rdtsc low word */
	const unsigned int bootKey2 = 0x12345679u;	/* a *different* boot                        */
	const unsigned int id    = 0x00000007u;		/* e.g. a voice-model object id              */
	const unsigned int extra = 0x00000000u;

	printf("[3] authorize -> verify round-trip\n");
	unsigned int stamp = authorize(id, extra, bootKey);
	CHECK_EQ("stamp=(id+1+extra)*key", stamp, (id + 1u + extra) * bootKey);

	CHECK_TRUE ("verify same boot",        verify(stamp, id,        extra,     bootKey));
	CHECK_FALSE("verify tampered id",      verify(stamp, id + 1u,   extra,     bootKey));
	CHECK_FALSE("verify tampered extra",   verify(stamp, id,        extra + 1u, bootKey));
	CHECK_FALSE("verify tampered stamp",   verify(stamp ^ 1u, id,   extra,     bootKey));
	CHECK_FALSE("verify wrong boot key",   verify(stamp, id,        extra,     bootKey2));

	/* A multisample bank: identity is hashed first, then stamped the same way. */
	unsigned char bankid[16];
	unsigned int i;
	for (i = 0; i < 16; i++)
		bankid[i] = (unsigned char)(0xa0 ^ (i * 7));
	unsigned int hash = oa_fnv1a16(bankid);
	unsigned int bstamp = authorize(hash, extra, bootKey);
	CHECK_TRUE ("bank verify same boot",   verify(bstamp, hash, extra, bootKey));
	CHECK_FALSE("bank verify wrong boot",  verify(bstamp, hash, extra, bootKey2));
}

static void test_legacy_builtin_bank(void)
{
	/*
	 * Pass 3 of AuthorizeBuiltins: the 11 legacy ROM banks share the template
	 * "KORG" + 8x00 + "MS" + 00 + <index>, index stepping 0,2,...,0x14.  Authorized with
	 * extra=0.  We anchor the hash+stamp of bank #0 and confirm the round-trip.
	 */
	const unsigned int bootKey = 0x9e3779b1u;
	unsigned char uuid[16] = { 'K','O','R','G', 0,0,0,0, 0,0,0,0, 'M','S', 0, 0 };

	printf("[4] legacy builtin ROM bank (extra=0)\n");
	uuid[15] = 0x00;	/* bank #0 */
	unsigned int h0 = oa_fnv1a16(uuid);
	CHECK_EQ("fnv1a16(\"KORG..MS\\0\\0\")", h0, 0x1c4e7f2au);	/* golden */
	unsigned int s0 = authorize(h0, 0u, bootKey);
	CHECK_TRUE ("builtin#0 verify",  verify(s0, h0, 0u, bootKey));

	uuid[15] = 0x14;	/* bank #10 (last) */
	unsigned int h10 = oa_fnv1a16(uuid);
	CHECK_TRUE ("index byte changes hash", h10 != h0);
	CHECK_TRUE ("builtin#10 verify", verify(authorize(h10, 0u, bootKey), h10, 0u, bootKey));
}

int main(void)
{
	printf("OA KLM auth known-answer test\n");
	printf("=============================\n");
	test_fnv_core_against_standard();
	test_korg_basis();
	test_auth_roundtrip();
	test_legacy_builtin_bank();
	printf("=============================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
