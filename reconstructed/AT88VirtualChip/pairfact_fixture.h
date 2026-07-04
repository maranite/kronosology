// SPDX-License-Identifier: GPL-2.0
/*
 * pairfact_fixture.h  -  loadmod.ko's RetrieveSecurityICKey response fixture.
 *
 * loadmod.ko reads an 80-byte chip-encrypted blob from `/.pairFact3` and
 * sends it to the real AT88 chip; the chip uses its tamper-resistant
 * internal secret to decrypt it and returns 16 raw bytes of key material
 * per volume (Mod/Eva/WaveMotion), which loadmod.ko then hex-encodes into
 * the actual cryptoloop AES-256 keys (docs/crypto/cryptoloop_keys.md).
 *
 * This chip-internal decryption is NOT reverse-engineered and cannot be
 * (loadmod's own docs describe it as "RSA + NLFSR", genuinely tamper-
 * resistant silicon, never exposing its secret to the host). It doesn't
 * need to be, though: the *correct final output* was already
 * independently recovered via a completely different channel --
 * `LOOP_GET_STATUS64` on a live system, confirmed byte-identical across
 * every unit and firmware version this project has tested. So this is a
 * pure response fixture: recognize the one known captured `/.pairFact3`
 * blob (80 bytes, MD5 `817956d550647905828e115f9eae7a0e`) and return the
 * raw key material that produces the already-known-correct keys, rather
 * than attempting (impossible) chip-internal decryption.
 *
 * IMPORTANT SCOPING NOTE: this file operates at the LOGICAL level (blob
 * in, raw key material out) -- it is deliberately NOT wired into
 * nv2ac_exports.cpp's opcode dispatch, because the actual AT88 wire
 * command RetrieveSecurityICKey uses to move an 80-byte blob to the chip
 * and get 48 bytes back has never been reverse-engineered at the byte
 * level (confirmed: neither cryptoloop_keys.md nor
 * docs/modules/loadmod.ko_analysis.md documents the opcode; the former
 * explicitly says "we have not had occasion to RE the exact plaintext
 * layout" and suggests kprobing stgNV2AC_sync_read_cmd as the way to find
 * out). Guessing that wire format isn't warranted -- this project's
 * standing rule is ground truth over paraphrase, and there is no ground
 * truth for it yet. Wiring this fixture into nv2ac_exports.cpp is a
 * follow-up once that wire format is actually confirmed.
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
