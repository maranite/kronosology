// SPDX-License-Identifier: GPL-2.0
/*
 * test_aac_lockout.cpp  -  known-answer test for the real AT88SC authentication
 * lockout behavior (b8_handshake.cpp's update_aac()/at88_chip_is_locked_out()),
 * added per user request to make the emulator match real hardware's documented
 * anti-brute-force behavior instead of a plain saturating ±1 counter.
 *
 * Ground truth: Atmel CryptoMemory datasheet (Good Info/Atmel-8664-CryptoMem-
 * Low-Density-Full-Specification-Datasheet.pdf), section 6.3.18 "Authentication
 * Attempts Counters": AAC decays $FF,$EE,$CC,$88,$00 over 4 consecutive failed
 * $B8 attempts (the default "ETA=1" config this emulator assumes - see
 * at88_chip.h's useEightStepAac field), and $00 PERMANENTLY locks that
 * authentication key set - a locked chip refuses to even attempt verification
 * on any further $B8, correct Q or not.
 *
 * "Permanently" here is scoped to one AT88ChipState's lifetime, matching the
 * user's explicit request: not persisted anywhere beyond process memory, so
 * reloading the chip state (at88_chip_load_from_extract()/
 * at88_chip_load_synthetic()) - the emulator's only reset mechanism - clears
 * it, unlike real silicon which has no such recovery at all. Tests [4]/[5]
 * below cover both halves of that: locked stays locked across repeated
 * attempts (even a correct one), and a fresh load clears it.
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

/* Build a chip like test_b8_handshake.cpp's [1]-[3], plus compute a genuinely
 * valid round-1 Q the same way a real host would (mirroring the chip's own
 * verification math - there is no other way to generate one). */
struct Fixture {
	AT88ChipState chip;
	unsigned char idn[7];
	unsigned char validQ[8];
	unsigned char nc[8];
};

static void build_fixture(Fixture *f)
{
	memset(&f->chip, 0, sizeof(f->chip));
	static const unsigned char idn[7] = {0x66, 0x30, 0x39, 0x84, 0x85, 0xce, 0xf3};
	memcpy(f->idn, idn, 7);
	memcpy(f->chip.configZone + 0x19, idn, 7);
	static const unsigned char cfg50[8] = {0xff, 0x65, 0xff, 0xd9, 0x53, 0xd2, 0x17, 0x6a};
	memcpy(f->chip.configZone + 0x50, cfg50, 8);
	deax_init(&f->chip.session);
	f->chip.dataLoaded = 1;
	f->chip.b8RoundsAccepted = 0;

	static const unsigned char nc1[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
	memcpy(f->nc, nc1, 8);

	unsigned char hostP2[8], hostP3[8], p5[8];
	synth_sdflkjsvnd2g(idn, hostP2);
	memcpy(hostP3, cfg50, 8);
	DeaxState hostSession;
	deax_init(&hostSession);
	deax_compute_challenges(&hostSession, nc1, hostP2, hostP3, f->validQ, p5);
}

int main(void)
{
	printf("AT88VirtualChip AAC lockout known-answer test\n");
	printf("================================================\n");

	printf("[1] Fresh chip starts unlocked, AAC == 0xff\n");
	Fixture f;
	build_fixture(&f);
	check_eq("not locked out initially", (unsigned int)at88_chip_is_locked_out(&f.chip), 0);
	check_eq("AAC == 0xff", f.chip.configZone[0x50], 0xff);

	printf("[2] 4 consecutive tampered round-1 attempts step the AAC through\n"
	       "    the real decay sequence 0xff,0xee,0xcc,0x88,0x00 (not a plain -1)\n");
	static const unsigned int wantAfterEach[4] = {0xee, 0xcc, 0x88, 0x00};
	unsigned char badQ[8];
	memcpy(badQ, f.validQ, 8);
	badQ[0] ^= 0xff; /* corrupt - stays wrong across every attempt */
	for (int i = 0; i < 4; i++) {
		int rc = at88_chip_handle_b8(&f.chip, 0x00, f.nc, badQ);
		char label[64];
		snprintf(label, sizeof(label), "attempt %d: rejected (rc==0)", i + 1);
		check_eq(label, (unsigned int)rc, 0);
		snprintf(label, sizeof(label), "attempt %d: AAC == 0x%02x", i + 1, wantAfterEach[i]);
		check_eq(label, f.chip.configZone[0x50], wantAfterEach[i]);
	}
	check_eq("b8RoundsAccepted still 0 after 4 rejects", (unsigned int)f.chip.b8RoundsAccepted, 0);

	printf("[3] After 4 failures, chip reports locked out (matches AAC == 0x00)\n");
	check_eq("locked out", (unsigned int)at88_chip_is_locked_out(&f.chip), 1);

	printf("[4] A 5th attempt with a GENUINELY VALID Q is STILL rejected -\n"
	       "    real hardware refuses to even evaluate Q once locked\n");
	int rc = at88_chip_handle_b8(&f.chip, 0x00, f.nc, f.validQ);
	check_eq("valid Q rejected while locked", (unsigned int)rc, 0);
	check_eq("b8RoundsAccepted stays 0 while locked", (unsigned int)f.chip.b8RoundsAccepted, 0);
	check_eq("AAC stays 0x00 (does not move once locked)", f.chip.configZone[0x50], 0x00);

	printf("[5] Lockout is session-scoped, NOT permanent across a reload -\n"
	       "    reloading the chip state (the emulator's only reset path,\n"
	       "    unlike real silicon which has none) clears it\n");
	at88_chip_load_synthetic(&f.chip);
	check_eq("unlocked again after reload", (unsigned int)at88_chip_is_locked_out(&f.chip), 0);
	check_eq("AAC reset to 0xff after reload", f.chip.configZone[0x50], 0xff);

	printf("[6] useEightStepAac selects the 8-step sequence instead\n");
	Fixture f2;
	build_fixture(&f2);
	f2.chip.useEightStepAac = 1;
	unsigned char badQ2[8];
	memcpy(badQ2, f2.validQ, 8);
	badQ2[0] ^= 0xff;
	static const unsigned char wantSeq8[8] = {0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};
	for (int i = 0; i < 8; i++) {
		at88_chip_handle_b8(&f2.chip, 0x00, f2.nc, badQ2);
		char label[64];
		snprintf(label, sizeof(label), "8-step attempt %d: AAC == 0x%02x", i + 1, wantSeq8[i]);
		check_eq(label, f2.chip.configZone[0x50], wantSeq8[i]);
	}
	check_eq("locked out after 8 failures (8-step config)",
		 (unsigned int)at88_chip_is_locked_out(&f2.chip), 1);

	printf("================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
