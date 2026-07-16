// SPDX-License-Identifier: GPL-2.0
/*
 * pairfact_decrypt.cpp  -  see pairfact_decrypt.h.
 */

#include <cstring>
#include "pairfact_decrypt.h"
#include "oa_crypto.h"
#include "oa_md5.h"

/* The confirmed-universal fixed block: 0x03 + Mod/Eva/WaveMotion raw keys,
 * now known complete (including the 16th byte each, previously unknown --
 * see pairfact_fixture.cpp's own updated comment for the same correction). */
static const unsigned char KNOWN_FIXED_BLOCK[49] = {
	0x03,
	0xa3,0x36,0xa1,0x5c,0xd8,0x41,0xec,0x89,0x26,0xb9,0x9e,0x7c,0x38,0x84,0xea,0xa7,
	0x34,0x2e,0xe5,0x9d,0x54,0x9c,0x7d,0x32,0x9d,0x83,0x55,0x37,0xbe,0x05,0x40,0xd2,
	0x3e,0x72,0xc0,0xe5,0x9f,0xc0,0x17,0xa9,0xeb,0x7d,0x7e,0x11,0x68,0xa4,0xcd,0xbe,
};

void pf3_decrypt(const unsigned char *zone0_24, const unsigned char *ciphertext80,
		  struct Pf3Decrypted *out)
{
	unsigned char plain[PF3_BLOB_LEN];

	moancjsd82(zone0_24, ciphertext80, PF3_BLOB_LEN, plain);

	memcpy(out->nonce, plain, 15);
	memcpy(out->fixedBlock, plain + 15, 49);
	out->fixedBlockIsKnownUniversal = memcmp(out->fixedBlock, KNOWN_FIXED_BLOCK, 49) == 0;

	struct md5_state_t md5ctx;
	unsigned char digest[16];
	md5_init(&md5ctx);
	md5_append(&md5ctx, plain, 64);
	md5_finish(&md5ctx, digest);
	out->md5Ok = memcmp(digest, plain + 64, 16) == 0;
}
