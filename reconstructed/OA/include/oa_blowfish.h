// SPDX-License-Identifier: GPL-2.0
/*
 * oa_blowfish.h  -  standard Blowfish block cipher, single-block encrypt only.
 *
 * Stage 2 (shared utilities) primitive backing moancjsd82() (see oa_crypto.h /
 * src/crypto/moancjsd82.cpp). Confirmed to be UNMODIFIED, textbook Blowfish
 * (pi-digit P/S-box initializers, standard F-function and key schedule) by
 * `docs/confirmed findings/OA_Auth_Analysis_Report.md`, which disassembled
 * moancjsd82's F-function directly and matched it byte-for-byte to the
 * reference algorithm. The P-box/S-box initializer constants here are
 * extracted verbatim (programmatically, not retyped) from this exact kernel
 * tree's own implementation, `linux-2.6.32.11/crypto/blowfish.c` (GPL-2.0,
 * Bruce Schneier's public-domain algorithm) -- using the kernel's own copy
 * both guarantees correctness and keeps this reconstruction self-consistent
 * with the toolchain OA.ko itself was built against.
 *
 * Only encryption is needed: CFB mode (oa_moancjsd82.cpp) always runs the
 * cipher in the encrypt direction to generate keystream, for both encoding
 * and decoding.
 */

#ifndef OA_BLOWFISH_H
#define OA_BLOWFISH_H

typedef unsigned int oa_u32;

struct oa_bf_ctx {
	oa_u32 p[18];
	oa_u32 s[1024];
};

/* Key schedule: keylen must be 4..56 bytes (standard Blowfish key size range). */
void oa_bf_setkey(struct oa_bf_ctx *ctx, const unsigned char *key, unsigned int keylen);

/* Encrypt one 8-byte block in place (big-endian halves, matching the
 * confirmed algorithm's L/R loading in EXs_Auth_Algorithm.md). */
void oa_bf_encrypt_block(const struct oa_bf_ctx *ctx, unsigned char block[8]);

#endif /* OA_BLOWFISH_H */
