// SPDX-License-Identifier: GPL-2.0
/*
 * test_b8_handshake.cpp  -  host-side known-answer tests for bignum.cpp
 * (the modexp/IdN->p2 derivation) and b8_handshake.cpp (the two-round $B8
 * challenge/response), plus an end-to-end check tying the handshake to a
 * Zone0 read.
 *
 * Honesty note on what's actually verified here vs. self-consistency only:
 * the bignum/p2 derivation IS checked against an independent oracle (a
 * from-scratch Python port, gen_bignum_vectors.py). The $B8 handshake
 * ROUND-TRIP itself has no independent oracle available -- there's no
 * captured real Nc/Q exchange to compare against, so tests [2]-[4] verify
 * INTERNAL consistency (the handler's verification math matches its own
 * challenge-generation math, exactly mirroring what kronos_extract.c's
 * synth_try() does when talking to a real chip) rather than independent
 * ground truth. Test [5] extends that: after a self-consistent handshake,
 * the resulting cipher state must still correctly decrypt whatever was
 * loaded as Zone0 -- an internal round-trip check, not a comparison
 * against a specific real device's secret.
 *
 * All IdN/config-zone/Zone0 values below are SYNTHETIC (generated for this
 * test, not read from any real device) -- see gen_bignum_vectors.py for
 * the matching independent-oracle input. Real per-device values are
 * intentionally not committed to this repo.
 */

#include <cstdio>
#include <cstring>
#include "../at88_chip.h"
#include "../bignum.h"

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		printf("  ok    %-60s 0x%x\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=0x%x want=0x%x\n", label, got, want);
	g_fail++;
}

int main(void)
{
	printf("AT88VirtualChip bignum + $B8 handshake known-answer test\n");
	printf("==========================================================\n");

	printf("[1] synth_sdflkjsvnd2g vs. independent Python (gen_bignum_vectors.py),\n"
	       "    using a synthetic IdN (not real device data)\n");
	static const unsigned char idn[7] = {0xde,0x00,0xad,0x00,0xbe,0x00,0xef};
	unsigned char p2[8];
	synth_sdflkjsvnd2g(idn, p2);
	static const unsigned char wantP2[8] = {0xbc,0x56,0x9c,0xa3,0xa8,0x62,0x15,0xd8};
	bool p2Ok = memcmp(p2, wantP2, 8) == 0;
	check_eq("p2 == bc569ca3a86215d8", (unsigned int)p2Ok, 1);

	/* Build a chip preloaded with fully synthetic config/zone0 -- this test
	 * never touches a real captured file, so every value here is
	 * self-consistent test data, not real per-device secret material. */
	AT88ChipState chip;
	memset(&chip, 0, sizeof(chip));
	memcpy(&chip.configZone[0x19], idn, 7);
	static const unsigned char cfg50[8] = {0xff,0xcc,0xbb,0xaa,0x99,0x88,0x77,0x66};
	memcpy(&chip.configZone[0x50], cfg50, 8);
	static const unsigned char synthZone0[40] = {
		0xa5,0x92,0x77,0xd3,0xcf,0x9a,0xb2,0x22,0xcc,0x2d,0x7a,0x47,0x6c,0x32,0x8c,0x8b,
		0xee,0xec,0x7e,0x64,0x20,0x8e,0x3d,0xbe,0x61,0x12,0x3c,0xbb,0x53,0xda,0xa9,0x1d,
		0xda,0x29,0xc1,0x02,0x06,0x3a,0x9a,0x93,
	};
	memcpy(chip.zone0, synthZone0, 40);
	deax_init(&chip.session);
	chip.dataLoaded = 1;
	chip.b8RoundsAccepted = 0;

	printf("[2] Round 1 ($B8 zone=0x00): a self-consistent challenge is accepted\n");
	/* "Host" side: compute what a real host would send, using the exact
	 * same algorithm the chip itself uses to verify (there is no other
	 * way to generate a valid challenge -- this is expected self-
	 * consistency, not a weaker test, since the whole point is the CHIP's
	 * verification must accept a correctly-computed challenge). */
	unsigned char hostP2[8], hostP3[8];
	synth_sdflkjsvnd2g(idn, hostP2);
	memcpy(hostP3, cfg50, 8);
	static const unsigned char nc1[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
	unsigned char q1[8], p5_1[8];
	DeaxState hostSession;
	deax_init(&hostSession);
	deax_compute_challenges(&hostSession, nc1, hostP2, hostP3, q1, p5_1);

	/* The synthetic AAC byte (cfg[0x50]) is set to 0xff -- already
	 * saturated at its max, so "increment on accept" correctly no-ops
	 * there rather than overflowing. Check the saturating direction (>=,
	 * not strictly >) so this test is correct regardless of the starting
	 * value. */
	unsigned char aacBefore = chip.configZone[0x50];
	int rc = at88_chip_handle_b8(&chip, 0x00, nc1, q1);
	check_eq("round 1 accepted", (unsigned int)rc, 1);
	check_eq("b8RoundsAccepted == 1", (unsigned int)chip.b8RoundsAccepted, 1);
	check_eq("AAC did not decrease on accept (saturates at 0xff)",
		 (unsigned int)(chip.configZone[0x50] >= aacBefore), 1);

	printf("[3] Round 1 rejects a wrong Q (tampered challenge)\n");
	AT88ChipState chip2 = chip;	/* fresh copy, pre-round-1, to test rejection in isolation */
	chip2.b8RoundsAccepted = 0;
	deax_init(&chip2.session);
	unsigned char badQ[8];
	memcpy(badQ, q1, 8);
	badQ[0] ^= 0xff;	/* corrupt one byte */
	unsigned char aacBefore2 = chip2.configZone[0x50];
	rc = at88_chip_handle_b8(&chip2, 0x00, nc1, badQ);
	check_eq("tampered round 1 rejected", (unsigned int)rc, 0);
	check_eq("b8RoundsAccepted stays 0 on reject", (unsigned int)chip2.b8RoundsAccepted, 0);
	check_eq("AAC decremented on reject", (unsigned int)(chip2.configZone[0x50] < aacBefore2), 1);

	printf("[4] Round 2 ($B8 zone=0x10, chained on round 1's p5) is accepted\n");
	static const unsigned char nc2[8] = {0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x01,0x02};
	unsigned char q2[8], p5_2[8];
	deax_compute_challenges(&hostSession, nc2, p5_1, hostP3, q2, p5_2);	/* p2=p5_1 chained; p3 already updated in place */
	rc = at88_chip_handle_b8(&chip, 0x10, nc2, q2);
	check_eq("round 2 accepted", (unsigned int)rc, 1);
	check_eq("b8RoundsAccepted == 2", (unsigned int)chip.b8RoundsAccepted, 2);

	printf("[5] End-to-end: post-handshake session correctly decrypts the\n"
	       "    synthetic Zone0 data it was loaded with (round-trip check)\n");
	unsigned char cipher[8];
	rc = at88_chip_read_zone0(&chip, &chip.session, 0, 8, cipher);
	check_eq("read_zone0(0,8) after handshake rc==0", (unsigned int)(rc == 0), 1);
	/* Host-side mirrored decrypt, matching kronos_extract.c's
	 * synth_zone0_read() exactly. `hostSession` (from test [4]) is
	 * already sitting right after round 2's own deax_compute_challenges()
	 * call -- matching real synth_try(), which runs the post-$B8
	 * continuation exactly once per round, immediately after that round's
	 * Q computation. So only ONE continuation needs mirroring here (not
	 * one per round), using the AAC value the chip landed on after both
	 * rounds accepted. */
	DeaxState hostZone0State = hostSession;
	unsigned char aacFinal = chip.configZone[0x50];
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0x50);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 1);
	deax_step(&hostZone0State, aacFinal);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	/* Now mirror at88_chip_read_zone0's own 12 pre-steps + per-byte decrypt. */
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);	/* addr=0 */
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 8);	/* len=8 */
	unsigned char recovered[8];
	for (int i = 0; i < 8; i++) {
		recovered[i] = (unsigned char)(cipher[i] ^ hostZone0State.gpa);
		deax_step(&hostZone0State, recovered[i]);
		deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
		deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
		deax_step(&hostZone0State, 0);
	}
	bool zone0Ok = memcmp(recovered, synthZone0, 8) == 0;
	check_eq("recovered bytes match the synthetic Zone0[0..7] loaded above", (unsigned int)zone0Ok, 1);

	printf("==========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
