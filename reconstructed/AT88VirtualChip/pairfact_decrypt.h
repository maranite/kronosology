// SPDX-License-Identifier: GPL-2.0
/*
 * pairfact_decrypt.h  -  GENERAL .pairFact3/.reauth decrypt, for real.
 *
 * Added 2026-07-16, replacing what pairfact_fixture.h used to claim was
 * necessary. That file's original framing -- "loadmod.ko sends the 80-byte
 * blob to the real AT88 chip; the chip uses its tamper-resistant internal
 * secret to decrypt it" -- was WRONG, not just imprecise. It was corrected
 * after matching `KronosExtract.bin` + `.reauth` pairs for two real
 * physical units became available, and this file's decrypt (below)
 * reproduced the correct, already-known-universal cryptoloop keys on both,
 * using nothing but each unit's own Zone0 data -- no chip interaction of
 * any kind beyond what `KronosExtract`/`AT88VirtualChip` already fully
 * emulate.
 *
 * The real mechanism was actually already sitting in this repo, just not
 * cross-referenced here: `reconstructed/OA/include/oa_crypto.h`'s own
 * header comment documents that `moancjsd82` (OA.ko's confirmed,
 * hardware-verified Blowfish-CFB-64 primitive, already used for EX-auth
 * strings with p3=15) is called by `loadmod.ko`'s `bbbbbbbba12` with
 * **p3=80 for exactly `.pairFact3`/`.reauth`** -- i.e. this was never a
 * separate, unconfirmed "RetrieveSecurityICKey wire format" question at
 * all. It's the identical CFB-64 decode as EX-auth strings, just with a
 * different key source (Zone0[0x00:0x18], the same 24 bytes
 * KronosExtract/AT88VirtualChip already read) and a different length (80
 * instead of 15). `.reauth` files ARE `.pairFact3` files, byte-for-byte,
 * interchangeable by filename alone -- confirmed independently by
 * `KronosExtract/build/kronos.py`'s own `pf3_decrypt()`/`pf3_generate()`
 * (which this file's algorithm matches) and by direct real-world testing
 * (renaming a captured `.reauth` to `.pairFact3` and using it as such
 * works).
 *
 * This file is deliberately HOST-TEST-ONLY -- not part of
 * `AT88VirtualChip-objs` in the real kernel-module build (unlike
 * `pairfact_fixture.cpp`, which the .ko build already includes). It's a
 * research/verification tool in the same spirit as `kronos.py`, not chip
 * emulation -- the actual decrypt happens entirely host-side (inside
 * `loadmod.ko` on a real device, or here for offline analysis), never on
 * the chip. Reuses `reconstructed/OA/src/crypto/moancjsd82.cpp` +
 * `blowfish.cpp` directly rather than re-implementing Blowfish-CFB-64 a
 * second time in this repo.
 *
 * Plaintext layout (80 bytes total), confirmed against `kronos.py` and
 * against two real decrypted `.reauth` files:
 *   [0:15]   nonce -- random per-generation, per-device
 *   [15:64]  FIXED, identical on EVERY unit (confirmed on 2 real Kronos
 *            units so far): 0x03, then the 3x16-byte cryptoloop keys
 *            (Mod/Eva/WaveMotion) back-to-back, verbatim -- see
 *            `docs/crypto/cryptoloop_keys.md`
 *   [64:80]  MD5(plaintext[0:64]) -- integrity check, not secret material
 */

#ifndef AT88_PAIRFACT_DECRYPT_H
#define AT88_PAIRFACT_DECRYPT_H

#define PF3_BLOB_LEN 80

struct Pf3Decrypted {
	unsigned char nonce[15];
	unsigned char fixedBlock[49];	/* 0x03 + Mod(16) + Eva(16) + WaveMotion(16) */
	int fixedBlockIsKnownUniversal;	/* fixedBlock == the confirmed-universal value */
	int md5Ok;			/* plaintext[64:80] == MD5(plaintext[0:64]) */
};

/*
 * pf3_decrypt -- decrypt an 80-byte .pairFact3/.reauth blob using a
 * specific device's own Zone0 key material.
 *
 * zone0_24: 24 bytes -- Zone0[0x00:0x18] (byte 0..15 = Blowfish key,
 *           16..23 = CFB feedback IV), exactly what KronosExtract.bin's
 *           pf3_zone field (or AT88ChipState::zone0[0:24]) already holds.
 * ciphertext80: the 80-byte blob content (a `.reauth` or `.pairFact3` file
 *               read verbatim -- there is no format difference).
 * out: populated on return; out->md5Ok tells you whether decryption with
 *      this key material actually produced valid plaintext (wrong key
 *      material decrypts to garbage that fails the MD5 check, so this
 *      doubles as an implicit "is this the right device's Zone0" check).
 */
void pf3_decrypt(const unsigned char *zone0_24, const unsigned char *ciphertext80,
		  struct Pf3Decrypted *out);

#endif /* AT88_PAIRFACT_DECRYPT_H */
