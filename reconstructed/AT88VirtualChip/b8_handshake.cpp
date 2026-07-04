// SPDX-License-Identifier: GPL-2.0
/*
 * b8_handshake.cpp  -  the $B8 verify-crypto challenge/response handshake.
 * See at88_chip.h for the full ground-truthing and state-machine design.
 *
 * Confirmed two-round sequence (kronos_extract.c's synth_try(), matching
 * OA.ko's SetupAtmelForAuthorizations() exactly -- src/auth/atmel_setup.cpp):
 *   Round 1 (zone 0x00, "cipher"):  p2 = synth_sdflkjsvnd2g(IdN)
 *   Round 2 (zone 0x10, "encrypt"): p2 = round 1's p5 output
 * Both rounds: Q = deax_compute_challenges(Nc, p2, p3, ...).p5's Q output;
 * p3 carries forward modified in place between rounds.
 */

#include "at88_chip.h"
#include "bignum.h"

static void update_aac(struct AT88ChipState *chip, int accepted)
{
	unsigned char *aac = &chip->configZone[0x50];
	if (accepted) {
		if (*aac < 0xff)
			(*aac)++;
	} else {
		if (*aac > 0)
			(*aac)--;
	}
}

void at88_chip_post_b8_steps(struct AT88ChipState *chip)
{
	DeaxState *d = &chip->session;
	unsigned char aac = chip->configZone[0x50];

	/* 12 pre-steps: step(0)x5, step(addr=0x50), step(0)x5, step(len=1) --
	 * matching a plain, unencrypted $B6 config read of the AAC byte. */
	deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
	deax_step(d, 0x50);
	deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
	deax_step(d, 1);

	/* step on the response byte (the real AAC value, unencrypted), then
	 * 5 trailing zero-steps. */
	deax_step(d, aac);
	deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
}

int at88_chip_handle_b8(struct AT88ChipState *chip, unsigned char zone,
			 const unsigned char *nc, const unsigned char *q)
{
	if (zone == 0x00) {
		if (chip->b8RoundsAccepted != 0)
			return 0;	/* round 1 must be first */
		synth_sdflkjsvnd2g(&chip->configZone[0x19], chip->p2);
		for (int i = 0; i < 8; i++)
			chip->p3[i] = chip->configZone[0x50 + i];
	} else if (zone == 0x10) {
		if (chip->b8RoundsAccepted != 1)
			return 0;	/* round 2 must follow an accepted round 1 */
	} else {
		return 0;
	}

	unsigned char qExpected[8], p5[8];
	deax_compute_challenges(&chip->session, nc, chip->p2, chip->p3, qExpected, p5);

	int accepted = 1;
	for (int i = 0; i < 8; i++)
		if (qExpected[i] != q[i]) { accepted = 0; break; }

	update_aac(chip, accepted);
	if (!accepted)
		return 0;

	/* Chain p5 forward as the next round's p2 (round 2 only; harmless to
	 * do after round 2 too, since there is no round 3). */
	for (int i = 0; i < 8; i++)
		chip->p2[i] = p5[i];

	at88_chip_post_b8_steps(chip);
	chip->b8RoundsAccepted++;
	return 1;
}
