// SPDX-License-Identifier: GPL-2.0
/*
 * moancjsd82.cpp  -  Blowfish-CFB-64 keyed decode (Stage 2 shared utility).
 *
 * See include/oa_crypto.h for the full confirmed contract and provenance.
 * This is a direct port of the algorithm proven correct and hardware-verified
 * in `UIAppGen/Core/BlowfishCfb64.cs` (tested against a Python reference,
 * `KronosExtract/build/kronos.py`, and against real K2-73 AT88 chip data) and
 * independently documented byte-for-byte in
 * `docs/confirmed findings/EXs_Auth_Algorithm.md`.
 *
 * CFB-64, full-block feedback: encrypt the current 8-byte feedback register
 * once per 8-byte segment to produce keystream, XOR keystream with
 * ciphertext to recover plaintext, then feed the *ciphertext* bytes just
 * consumed back into the register (any unused tail positions of a final
 * partial segment keep their keystream value, which is discarded once
 * decoding stops).
 */

#include "oa_crypto.h"
#include "oa_blowfish.h"

extern "C" int moancjsd82(const unsigned char *chipKeyMaterial, const unsigned char *ciphertext,
			  unsigned int p3, unsigned char *plainOut)
{
	struct oa_bf_ctx ctx;
	unsigned char feedback[8];
	unsigned int offset;

	/* chipKeyMaterial[0..15] = Blowfish key, [16..23] = CFB feedback IV. */
	oa_bf_setkey(&ctx, chipKeyMaterial, 16);
	for (int i = 0; i < 8; i++)
		feedback[i] = chipKeyMaterial[16 + i];

	offset = 0;
	while (offset < p3) {
		unsigned char keystream[8];
		unsigned int chunk = (p3 - offset < 8) ? (p3 - offset) : 8;
		unsigned int i;

		for (i = 0; i < 8; i++)
			keystream[i] = feedback[i];
		oa_bf_encrypt_block(&ctx, keystream);

		for (i = 0; i < chunk; i++)
			plainOut[offset + i] = (unsigned char)(ciphertext[offset + i] ^ keystream[i]);

		for (i = 0; i < chunk; i++)
			feedback[i] = ciphertext[offset + i];
		for (i = chunk; i < 8; i++)
			feedback[i] = keystream[i];

		offset += chunk;
	}

	return (int)p3;
}
