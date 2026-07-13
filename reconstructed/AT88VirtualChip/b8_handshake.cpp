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

/*
 * Real AT88SC AAC decay sequences. Ground truth: Atmel CryptoMemory datasheet
 * (Good Info/Atmel-8664-CryptoMem-Low-Density-Full-Specification-Datasheet.pdf),
 * section 6.3.18 "Authentication Attempts Counters": "The AAC will decrement
 * ($FF, $EE, $CC, $88, $00) with each incorrect attempt to authenticate. The AAC
 * permanently locks the corresponding key set once its value reaches $00." A
 * chip config bit ("ETA") extends this from 4 to 8 consecutive failures, using
 * the sequence $FF,$FE,$FC,$F8,$F0,$E0,$C0,$80,$00 instead. The real Kronos
 * chip's actual ETA setting has not been characterized from a live capture (see
 * README.md's Open Items) -- AT88ChipState::useEightStepAac defaults to 0 (the
 * 4-step/documented-default sequence); set it explicitly if/when this gets
 * confirmed against real hardware.
 */
static const unsigned char kAacSequence4[5] = {0xff, 0xee, 0xcc, 0x88, 0x00};
static const unsigned char kAacSequence8[9] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00};

static void aac_sequence(const struct AT88ChipState *chip, const unsigned char **seq, int *len)
{
	if (chip->useEightStepAac) { *seq = kAacSequence8; *len = 9; }
	else                       { *seq = kAacSequence4; *len = 5; }
}

/*
 * Real chips refuse to even attempt verification once locked (datasheet: "the
 * device will return...to indicate the command is unauthorized" without
 * processing the command) -- session-scoped only, see at88_chip.h's
 * at88_chip_handle_b8() doc comment for what "session-scoped" means here.
 */
int at88_chip_is_locked_out(const struct AT88ChipState *chip)
{
	return chip->configZone[0x50] == 0x00;
}

static void update_aac(struct AT88ChipState *chip, int accepted)
{
	unsigned char *aac = &chip->configZone[0x50];

	if (accepted) {
		/* Datasheet: "any correct attempt to authenticate resets the AAC
		 * value to $FF" -- only reachable prior to lockout, since a locked
		 * chip's $B8 short-circuits before this function is ever called. */
		*aac = 0xff;
		return;
	}

	const unsigned char *seq; int len;
	aac_sequence(chip, &seq, &len);

	int idx = -1;
	for (int i = 0; i < len; i++)
		if (seq[i] == *aac) { idx = i; break; }

	if (idx < 0) {
		/* AAC holds a value outside the known decay sequence (e.g. a
		 * captured chip mid-sequence from real prior use under a config
		 * this emulator doesn't know about) -- fail safe by locking
		 * immediately rather than guessing which step it's really on. */
		*aac = seq[len - 1];
		return;
	}
	if (idx < len - 1)
		*aac = seq[idx + 1];
	/* else: already at the final step ($00) -- stays there. This IS the
	 * per-session-permanent lock: nothing in this function or the $B8
	 * dispatch path (at88_chip_handle_b8(), which short-circuits on lock
	 * before even calling this) can move it off $00 again. Only reloading
	 * the whole AT88ChipState (at88_chip_load_from_extract()/
	 * at88_chip_load_synthetic()) clears it. */
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
	if (at88_chip_is_locked_out(chip))
		return 0;	/* real chips don't even attempt verification once locked */

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
