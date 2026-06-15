// SPDX-License-Identifier: GPL-2.0
/*
 * atmel_setup.cpp  -  SetupAtmelForAuthorizations: the NV2AC dongle handshake.
 *
 * Two-round GPA mutual authentication with the Atmel AT88SC CryptoMemory dongle, run once
 * before any authorization string is verified (it is called from VerifyAuthorizationString).
 * Recovered from OA_322.ko @0x207a50.
 *
 * Sequence:
 *   1. read the chip config zone (zone 0x19, 7 bytes)
 *   2. dispatch pending chip I/O
 *   3. select password/auth zone 0
 *   4. load the hardcoded GPA Diffie-Hellman parameters (p, q, g)
 *   5. run the modexp challenge -> GPA cipher key
 *   6. read the chip IV / public nonce (zone 0x50, 8 bytes)
 *   Round 1 (cipher):   fresh challenge -> key schedule -> enable cipher
 *   Round 2 (encrypt):  fresh challenge -> key schedule (chained on round-1 c3) -> enable encrypt
 *   Finally: scrub all key material from the stack.
 *
 * Return codes (as the binary actually branches — note this differs from the chip's plate
 * comment, which mis-labels the IV-read failure as -3):
 *    0  success
 *   -1  config-zone (0x19) read failed
 *   -2  zone select, challenge compute, or IV-zone (0x50) read failed
 *   -3  round-1 cipher enable failed
 *   -4  round-2 encrypt enable failed
 */

#include "oa_atmel.h"

/* The GPA Diffie-Hellman parameters, baked into OA_322.ko as decimal strings. */
#define GPA_P "2758700829844358015"
#define GPA_Q "41504784743492877417"
#define GPA_G "08285927170653268036"

/* Scrub a key buffer so secrets do not linger on the stack (the binary zeroes them all). */
static inline void secure_zero(unsigned char *p, unsigned int n)
{
	volatile unsigned char *v = p;
	while (n--)
		*v++ = 0;
}

int SetupAtmelForAuthorizations(void)
{
	unsigned char chipConfig[11];	/* config zone (7 bytes used)            */
	unsigned char gpaRandKey[8];	/* GPA key, written by cm_ComputeChallenge*/
	unsigned char challenge[8];	/* per-round random challenge (c1)       */
	unsigned char c2Session[8];	/* derived cipher session key            */
	unsigned char iv[8];		/* chip IV / public nonce                */
	unsigned char c3Session[8];	/* derived encrypt session key           */
	int rc;

	if (cm_ReadUserZone(0x19, 7, chipConfig) != 0) {
		cm_ReadUserZone(0x19, 7, chipConfig);	/* binary re-issues on the error path */
		rc = -1;
		goto wipe;
	}

	nv2ac_dispatch_cmd();

	if (cm_SetUserZone(0) != 0) {
		rc = -2;
		goto wipe;
	}

	cm_SetChallengeParams(GPA_P, GPA_Q, GPA_G);
	if (cm_ComputeChallenge(chipConfig, 0, gpaRandKey) != 0) {
		rc = -2;
		goto wipe;
	}

	if (cm_ReadUserZone(0x50, 8, iv) != 0) {
		rc = -2;
		goto wipe;
	}

	/* Round 1 — cipher authentication (chip auth zone 0x00). */
	cm_GetRandomBytes(challenge, 8);
	cm_AuthenEncryptMAC(challenge, gpaRandKey, iv, c2Session, c3Session);
	if (nv2ac_enable_cipher(0, challenge, c2Session) != 0) {
		rc = -3;
		goto wipe;
	}

	/* Round 2 — encrypt authentication (zone 0x10), chained on the round-1 c3 key. */
	cm_GetRandomBytes(challenge, 8);
	cm_AuthenEncryptMAC(challenge, c3Session, iv, c2Session, c3Session);
	if (nv2ac_enable_encrypt(0, challenge, c2Session) != 0) {
		rc = -4;
		goto wipe;
	}

	rc = 0;

wipe:
	/* The binary scrubs the five 8-byte key buffers (plus its internal GMP scratch); the
	 * config zone is left intact. */
	secure_zero(gpaRandKey, sizeof gpaRandKey);
	secure_zero(challenge,  sizeof challenge);
	secure_zero(c2Session,  sizeof c2Session);
	secure_zero(iv,         sizeof iv);
	secure_zero(c3Session,  sizeof c3Session);
	return rc;
}
