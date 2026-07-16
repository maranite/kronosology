// SPDX-License-Identifier: GPL-2.0
/*
 * pairfact_fixture.h  -  ONE known captured /.pairFact3 blob's key material.
 *
 * SUPERSEDED 2026-07-16 by `pairfact_decrypt.h`, which implements the real,
 * general decrypt -- read that file's header comment for the full story.
 * Short version: there never was a special "RetrieveSecurityICKey wire
 * command" or chip-internal blob decryption to reverse-engineer.
 * `/.pairFact3` (== `.reauth`, byte-for-byte the same format -- confirmed
 * both via `KronosExtract/build/kronos.py`'s own `pf3_decrypt()` and by
 * direct real-hardware testing, renaming one to the other) decrypts with
 * the exact same Blowfish-CFB-64 primitive as an EX-auth string
 * (`moancjsd82`, `oa_crypto.h`), entirely host-side, keyed by nothing more
 * exotic than the target device's own Zone0 data -- the same 24 bytes
 * `KronosExtract`/`AT88VirtualChip` already read via the ordinary
 * authenticated protocol. No chip-internal "RSA + NLFSR" processing of the
 * blob itself; that description conflated the (real, still tamper-resistant)
 * problem of *obtaining Zone0* with the (trivial, already-solved) problem
 * of decrypting `/.pairFact3` once you have it.
 *
 * This file is kept only because it still passes its own KAT and nothing
 * depends on removing it -- `pairfact_fixture_lookup()` below recognizes
 * exactly one specific, already-known blob and returns its (now fully
 * confirmed, not guessed) key material. For any *other* `/.pairFact3` or
 * `.reauth` blob, use `pf3_decrypt()` in `pairfact_decrypt.h` instead --
 * it's the real algorithm, not a fixture, and works for any device you
 * have Zone0 data for.
 *
 * The raw 16-byte-per-volume key material below is now independently
 * confirmed complete (2026-07-16), not just reconstructed from the 31-char
 * ASCII form -- see `cryptoloop_keys.md`'s "`.reauth` IS `.pairFact3`, and
 * it decrypts with plain host-side Blowfish" section. Two real units'
 * `.reauth` files, decrypted with nothing more than their own
 * `KronosExtract.bin` Zone0 data (no chip interaction), yielded these exact
 * 16-byte keys byte-for-byte identical to each other, correcting this
 * file's own previously-arbitrary guess for the 16th byte's low nibble
 * (was `0xa0`/`0xd0`/`0xb0`, confirmed real value `0xa7`/`0xd2`/`0xbe`).
 */

#ifndef AT88_PAIRFACT_FIXTURE_H
#define AT88_PAIRFACT_FIXTURE_H

#define PAIRFACT_BLOB_LEN 80
#define PAIRFACT_KEY_LEN  16	/* raw bytes per volume, pre-hex-encoding */

/*
 * pairfact_fixture_lookup -- recognize a captured /.pairFact3 blob and
 * return the raw 16-byte-per-volume key material.
 *
 * blob/blobLen: the 80-byte file content read from /.pairFact3.
 * outKeys48: 48 bytes out (3 x 16), order Mod / Eva / WaveMotion.
 *
 * Returns 0 if the blob matched the one known captured fixture and
 * outKeys48 was populated; -1 if the blob is unrecognized (this project
 * has no way to derive key material for an unknown blob -- the chip's
 * internal secret is tamper-resistant and was never exposed).
 */
int pairfact_fixture_lookup(const unsigned char *blob, unsigned int blobLen,
			     unsigned char *outKeys48);

/*
 * hexencode_31char -- byte-for-byte replica of loadmod.ko's own HexEncode
 * quirk (confirmed via docs/crypto/cryptoloop_keys.md's "Key format"
 * section): converts 16 raw bytes to hex, but writes only the first 31
 * of the 32 characters a full hex encoding would produce into a
 * pre-zeroed 32-byte buffer -- so out[31] is always 0x00, and the 16th
 * raw byte's low nibble is provably never emitted (confirmed by the
 * "wrong 32nd byte" analysis in that doc: the real device output has
 * the correct first 31 chars but a literal null where a full encoding
 * would place the 32nd hex digit). Exposed here so pairfact_fixture's
 * own KAT can round-trip the known ASCII keys back through the real
 * quirk, and so a future nv2ac_exports.cpp integration doesn't need to
 * re-derive this.
 *
 * raw16: 16 input bytes. out32: 32 bytes out (31 hex chars + one 0x00).
 */
void hexencode_31char(const unsigned char *raw16, unsigned char *out32);

#endif /* AT88_PAIRFACT_FIXTURE_H */
