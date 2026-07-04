// SPDX-License-Identifier: GPL-2.0
/*
 * at88_chip.h  -  shared types for the AT88VirtualChip software chip
 * emulator. See README.md for the full design and ground-truthing.
 *
 * Two things this header models, deliberately kept separate:
 *
 *   1. The "DEAX" stream cipher (Korg's own internal name for what
 *      docs/crypto/atmel_nv2ac.md calls "GPA" -- same cipher, two names
 *      used in different places in this project's history). This is the
 *      on-wire confidentiality layer between the host and the real chip.
 *      It has NO per-device secret (fixed algorithm, self-synchronizing
 *      from a known-zero state) -- ground-truthed via
 *      `KronosExtract/source/kronos_extract.c`'s own `deax_step()`/
 *      `deax_init()`/`deax_compute_challenges()`, themselves labeled
 *      "software replica of bzzzzzzzzzzzt12"/"fFfFfFfFfFfF11" (the real
 *      OA.ko symbols), i.e. already reverse-engineered and hardware-
 *      validated by an earlier phase of this project. Ported here
 *      directly, not re-derived.
 *
 *   2. The AT88 chip's actual memory: a 128-byte "config zone" (read via
 *      the unencrypted `$B6` command) and the "Zone0" per-device secret
 *      region (`0x00`..`0x27`, 40 bytes -- read via the DEAX-encrypted
 *      `$B2` command, and the *only* part of the chip that's a genuine
 *      per-device secret). Confirmed exact zone numbers OA.ko's
 *      `SetupAtmelForAuthorizations` needs (`0x19`, `0x50`) both fall
 *      inside the 128-byte config zone -- see README.md's Open Item #1
 *      for the full confirmation chain (down to disassembling the real
 *      `cm_ReadUserZone` and confirming its `0xb6` opcode byte).
 *
 * Real captured data for both regions already exists:
 * `KronosExtract/build/KronosExtract.bin` (format below, confirmed against
 * the real 188-byte file -- magic/flags/crc all checked out byte-exact).
 */

#ifndef AT88_CHIP_H
#define AT88_CHIP_H

/* ---- DEAX/GPA stream cipher state ---------------------------------------
 *
 * Exactly the register set kronos_extract.c's deax_step() uses (R/S/T
 * banks + the running `gpa` output byte), just grouped into a struct
 * instead of file-scope globals so a single process can host more than one
 * independent chip session without them fighting over shared state.
 */
struct DeaxState {
	unsigned char gpa;
	unsigned char RA, RB, RC, RD, RE, RF, RG;	/* 5-bit register bank */
	unsigned char SA, SB, SC, SD, SE, SF, SG;	/* 7-bit register bank */
	unsigned char TA, TB, TC, TD, TE;		/* 5-bit register bank */
};

void deax_init(struct DeaxState *d);
void deax_step(struct DeaxState *d, unsigned char in);

/*
 * deax_compute_challenges -- the $B8 challenge/response derivation.
 * Ground-truthed 1:1 from kronos_extract.c's deax_compute_challenges()
 * (itself confirmed against the real fFfFfFfFfFfF11). Resets `d` to zero
 * internally (matches the real function's own behavior -- it's given a
 * fresh DeaxState each call, not threaded through from a prior session).
 *
 *   nc[8]     random nonce (chosen by whichever side initiates the round)
 *   p2[8]     GMP-derived seed for round 1; round 2 uses round 1's p5_out
 *   p3[8]     input: cfg[0x50..0x57]-derived seed; output: updated in place
 *   q_out[8]  the Q challenge bytes sent in the $B8 command
 *   p5_out[8] updated seed, fed back in as p2 for the next round
 */
void deax_compute_challenges(struct DeaxState *d, const unsigned char *nc,
			     const unsigned char *p2, unsigned char *p3,
			     unsigned char *q_out, unsigned char *p5_out);

/* ---- AT88 chip zone storage ---------------------------------------------
 *
 * `configZone[128]`: the plain, unencrypted config zone (`$B6` reads land
 * here directly, addresses 0-127). Confirmed to include both zones
 * `SetupAtmelForAuthorizations` needs: `0x19` (7 bytes, "IdN") and `0x50`
 * (8 bytes, the AAC/IV seed).
 *
 * `zone0[40]`: the real per-device secret, Zone0 `0x00`..`0x27`. This is
 * the ONLY genuinely per-device-secret data on the whole chip (the DEAX
 * cipher itself has no per-device key) -- everything the EX auth-string
 * algorithm and the pairFact/EXs Blowfish key+iv material derive from
 * lives in here. `$B2` reads against zone 0 are DEAX-encrypted per byte
 * (see `at88_chip_read_zone0()`), matching `kronos_extract.c`'s
 * `synth_zone0_read()` exactly (same cipher, opposite role: that function
 * decrypts what the real chip sends; this one encrypts what our emulated
 * chip sends, using the identical step/XOR sequence, so the *already
 * compiled* OA.ko/loadmod.ko decrypt-side logic works against it
 * unmodified).
 */
struct AT88ChipState {
	unsigned char configZone[128];
	unsigned char zone0[40];
	int           dataLoaded;	/* 0 until at88_chip_load_from_extract() succeeds */

	/* $B4 zone-select target -- stored for protocol fidelity only. Every
	 * zone this project has ever needed to emulate is zone 0 (the only
	 * zone SetupAtmelForAuthorizations/the EX auth chain touch), so
	 * nothing currently gates behavior on this value; see
	 * nv2ac_exports.cpp's header comment. */
	unsigned char selectedZone;

	/* ---- $B8 handshake / live session state -----------------------------
	 *
	 * `session` is the ONE persistent DEAX cipher state threaded through
	 * the whole handshake and beyond, confirmed via kronos_extract.c's own
	 * synth_try(): deax_compute_challenges() resets it to zero at the
	 * start of EACH $B8 round, but what's left in it after a round (Q
	 * computed, then `at88_chip_post_b8_steps()`'s 18-step continuation)
	 * carries forward into the NEXT round's reset-and-recompute, and
	 * ultimately into whatever `at88_chip_read_zone0()` calls come after
	 * a successful handshake. Do not re-seed this between rounds or
	 * before zone0 reads -- that's the exact bug kronos_extract.c's own
	 * comments warn "is why gpa is wrong at Zone0 read time."
	 *
	 * `p3` starts as configZone[0x50..0x57] and is updated IN PLACE by
	 * deax_compute_challenges() each round (confirmed: round 2 uses
	 * round 1's already-modified p3, not the original config bytes).
	 *
	 * `p2` is the challenge seed fed to THIS round's
	 * deax_compute_challenges() call: for round 1 it's
	 * synth_sdflkjsvnd2g(IdN) (computed once, at round-1 time); for round
	 * 2 it's round 1's own `p5_out` -- confirmed chaining, not a fresh
	 * modexp per round (`deax_compute_challenges(nc2, p5, p3, q2, p5)` in
	 * kronos_extract.c's synth_try(), p5 reused as both round 2's p2
	 * input and its own output buffer).
	 *
	 * `b8RoundsAccepted` tracks how many of the two rounds have verified
	 * successfully (0, 1, or 2 -- 2 means zone0 reads are now valid).
	 */
	DeaxState     session;
	unsigned char p2[8];
	unsigned char p3[8];
	int           b8RoundsAccepted;
};

/*
 * at88_chip_handle_b8 -- process one $B8 command
 * ({0xb8, zone, 0x00, 0x10, Nc[8], Q[8]}, 20 bytes on the wire).
 *
 * Independently recomputes the expected Q using this chip's own stored
 * IdN/p3 (via synth_sdflkjsvnd2g() + deax_compute_challenges(), exactly
 * mirroring what the real chip does internally, per kronos_extract.c's
 * synth_try()) and compares it against the received Q. Confirmed real
 * per-round zone numbers: round 1 uses zone 0x00 ("cipher" round), round
 * 2 uses zone 0x10 ("encrypt" round, chained on round 1's output) --
 * matches OA.ko's own SetupAtmelForAuthorizations() comments exactly
 * (src/auth/atmel_setup.cpp).
 *
 * On a correct Q (this round accepted): advances `chip->session` by the
 * confirmed 18-step post-$B8 continuation (at88_chip_post_b8_steps(),
 * matching kronos_extract.c's synth_post_b8_steps() -- critical for
 * subsequent zone0 reads to decrypt correctly), bumps the real chip's
 * AAC byte (configZone[0x50]) up by one (saturating), and increments
 * `chip->b8RoundsAccepted`.
 *
 * On a wrong Q (rejected): decrements the AAC byte (saturating at 0) and
 * does NOT advance the session state or round counter -- matches the
 * real chip's confirmed AAC-decrements-on-rejection behavior
 * (kronos_extract.c's synth_try() diagnostic comments).
 *
 * `zone` must be 0x00 for round 1 and 0x10 for round 2 (any other value,
 * or calling round 2 before round 1 has been accepted, is rejected
 * without touching any state). Returns 1 on accept, 0 on reject.
 */
int at88_chip_handle_b8(struct AT88ChipState *chip, unsigned char zone,
			 const unsigned char *nc, const unsigned char *q);

/* The confirmed 18-step post-$B8 cipher continuation: 12 pre-steps (as if
 * reading config addr 0x50, len 1), one step on the (always unencrypted)
 * AAC byte value, and 5 trailing zero-steps. Exposed separately from
 * at88_chip_handle_b8() so it can be unit-tested against
 * kronos_extract.c's synth_post_b8_steps() on its own. */
void at88_chip_post_b8_steps(struct AT88ChipState *chip);

/*
 * Parse a `KronosExtract.bin`-format blob (188 bytes, "KREX" magic,
 * confirmed layout: magic[4] version[1] flags[1] cfg[128] pf3_zone[24]
 * exs_zone[24] crc32[4]) and populate `chip`'s configZone/zone0.
 * pf3_zone (Zone0 0x00-0x17) and exs_zone (Zone0 0x10-0x27) overlap in
 * 0x10-0x17 -- confirmed identical in the real captured file, cross-checked
 * byte-for-byte rather than assumed; this function asserts that overlap
 * matches rather than silently preferring one source over the other.
 *
 * Returns 0 on success, negative on any validation failure (bad magic, bad
 * length, CRC mismatch, or -- the one AT88VirtualChip-specific check --
 * pf3_zone/exs_zone disagreeing on their overlapping 8 bytes).
 */
int at88_chip_load_from_extract(struct AT88ChipState *chip,
				 const unsigned char *blob, unsigned int blobLen);

/* Unencrypted config-zone read ($B6): copies `len` bytes starting at
 * `addr` out of `chip->configZone`. 0 on success, -1 if the read would run
 * past the 128-byte zone. */
int at88_chip_read_config(const struct AT88ChipState *chip,
			   unsigned char addr, unsigned char len,
			   unsigned char *out);

/*
 * DEAX-encrypted Zone0 read ($B2, mode=2): the chip-side counterpart to
 * kronos_extract.c's synth_zone0_read(). `d` must already be in the
 * correct session state (post-$B8 handshake) -- this function only does
 * the per-byte "step forward, XOR the real plaintext byte with the
 * keystream, step again on the *plaintext*" sequence confirmed there;
 * it does not run the $B8 handshake itself (that's
 * deax_compute_challenges(), driven by the not-yet-written nv2ac_exports.cpp).
 * 0 on success, -1 if the read would run past the 40-byte zone.
 */
int at88_chip_read_zone0(struct AT88ChipState *chip, struct DeaxState *d,
			  unsigned char addr, unsigned char len,
			  unsigned char *out);

#endif /* AT88_CHIP_H */
