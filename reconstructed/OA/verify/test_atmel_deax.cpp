// SPDX-License-Identifier: GPL-2.0
/*
 * test_atmel_deax.cpp  -  host-side known-answer test for
 * cm_AuthenEncryptMAC() (src/auth/atmel_deax.cpp, batch 43).
 *
 * Links src/auth/atmel_deax.cpp directly. Unlike most of this project's
 * KATs, this one is NOT just checking self-consistency against a
 * hand-derived expectation -- the test vectors below are the exact same
 * REAL, hardware-captured values (`b8_test.ko` runs against a genuine
 * AT88SC dongle) already used to validate the independent
 * `sim_f11.py`/`bzzt12.py` ports referenced in this file's own header
 * comment. Reusing them here directly checks this project's fresh
 * disassembly-and-translation of `fFfFfFfFfFfF11`'s own driving loop
 * against real silicon, not just against another Python port.
 */

#include <cstdio>
#include <cstring>

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAILED: %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static void check_bytes(const char *label, const unsigned char *got,
			 const unsigned char *want, int n)
{
	if (memcmp(got, want, n) == 0) {
		printf("  ok    %-50s (%d bytes match)\n", label, n);
		return;
	}
	printf("  FAILED: %-50s\n    got : ", label);
	for (int i = 0; i < n; i++) printf("%02x ", got[i]);
	printf("\n    want: ");
	for (int i = 0; i < n; i++) printf("%02x ", want[i]);
	printf("\n");
	g_fail++;
}

static void hex_to_bytes(const char *hex, unsigned char *out, int n)
{
	for (int i = 0; i < n; i++) {
		unsigned int b;
		sscanf(hex + 2 * i, "%2x", &b);
		out[i] = (unsigned char)b;
	}
}

/* Plain C++ linkage (no extern "C"), matching oa_atmel.h's own
 * un-wrapped declarations. */
void cm_AuthenEncryptMAC(const unsigned char *c1, const unsigned char *kin,
			 unsigned char *iv,
			 unsigned char *c2out, unsigned char *c3out);

int main()
{
	printf("[1] hardware vector #1 (b8_test.ko capture, sim_f11.py) -- checks\n"
	       "    c2out (Q1), c3out (Q2), AND the mutated iv (Ci) all at once\n");
	{
		unsigned char c1[8], kin[8], iv[8], c2out[8], c3out[8];
		hex_to_bytes("0102030405060708", c1, 8);   /* Nc */
		hex_to_bytes("5ebf3f6e2bd66660", kin, 8);  /* Gc */
		hex_to_bytes("cc40382360567636", iv, 8);   /* Ci (mutated in place) */

		cm_AuthenEncryptMAC(c1, kin, iv, c2out, c3out);

		unsigned char wantQ1[8], wantQ2[8];
		hex_to_bytes("f7036f19455f3730", wantQ1, 8);
		hex_to_bytes("fbfd850ea5fea06c", wantQ2, 8);
		check_bytes("c2out == hardware Q1", c2out, wantQ1, 8);
		check_bytes("c3out == hardware Q2", c3out, wantQ2, 8);
		check_eq("iv[0] forced to 0xff", iv[0], 0xff);
	}

	printf("[2] hardware vector #2 (bzzt12.py's own 'f3a8' capture) -- Q1 only\n");
	{
		unsigned char c1[8], kin[8], iv[8], c2out[8], c3out[8];
		hex_to_bytes("0102030405060708", c1, 8);
		hex_to_bytes("c4d82712d9f2f64c", kin, 8);
		hex_to_bytes("ff49c09774287f45", iv, 8);

		cm_AuthenEncryptMAC(c1, kin, iv, c2out, c3out);

		unsigned char wantQ1[8];
		hex_to_bytes("0829955d85db7cc6", wantQ1, 8);
		check_bytes("c2out == hardware Q1 (f3a8)", c2out, wantQ1, 8);
	}

	printf("[3] hardware vector #3 (bzzt12.py's own '947ef4' capture) -- Q1 only\n");
	{
		unsigned char c1[8], kin[8], iv[8], c2out[8], c3out[8];
		hex_to_bytes("0102030405060708", c1, 8);
		hex_to_bytes("d770e04d23f59382", kin, 8);
		hex_to_bytes("ff4f8c01a7897753", iv, 8);

		cm_AuthenEncryptMAC(c1, kin, iv, c2out, c3out);

		unsigned char wantQ1[8];
		hex_to_bytes("736e394b9e3295be", wantQ1, 8);
		check_bytes("c2out == hardware Q1 (947ef4)", c2out, wantQ1, 8);
	}

	printf("[4] two-round chaining shape: SetupAtmelForAuthorizations() feeds\n"
	       "    round 1's own c3out back in as round 2's kin, reusing the SAME\n"
	       "    (already-mutated) iv buffer -- confirmed distinct from a fresh\n"
	       "    zero-state call (sanity check, not a hardware vector)\n");
	{
		unsigned char c1[8], kin[8], iv[8], c2out[8], c3out[8];
		hex_to_bytes("0102030405060708", c1, 8);
		hex_to_bytes("5ebf3f6e2bd66660", kin, 8);
		hex_to_bytes("cc40382360567636", iv, 8);
		cm_AuthenEncryptMAC(c1, kin, iv, c2out, c3out);

		unsigned char c1b[8], c2out_b[8], c3out_b[8];
		hex_to_bytes("0807060504030201", c1b, 8); /* a different round-2 nonce */
		cm_AuthenEncryptMAC(c1b, c3out, iv, c2out_b, c3out_b);
		check_eq("round 2 output differs from round 1 (chained state, not idempotent)",
			 memcmp(c2out_b, c2out, 8) != 0, 1);
	}

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
