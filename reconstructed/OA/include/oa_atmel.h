// SPDX-License-Identifier: GPL-2.0
/*
 * oa_atmel.h  -  AT88SC CryptoMemory / NV2AC dongle authentication interface.
 *
 * OA proves it is running on genuine Korg hardware by completing a GPA (Gemplus-style)
 * mutual authentication with the Atmel AT88SC "CryptoMemory" dongle (the NV2AC chip)
 * before it will verify any authorization strings.  The low-level chip transport and the
 * GMP big-integer math live in other modules (the OmapNKS4 NV2AC driver and STGGmp); this
 * header just declares the operations the OA auth layer calls.
 *
 * Symbol note: cm_* are the CryptoMemory primitives; the nv2ac_* entries below correspond
 * to OA_322.ko functions still carrying obfuscated names (fFfFfFfFfFfF1F/1G/1H) — see
 * src/auth/atmel_setup.cpp for the mapping.
 */

#ifndef OA_ATMEL_H
#define OA_ATMEL_H

/* Read `len` bytes of chip user-zone `zone` into buf.  0 on success. */
int  cm_ReadUserZone(int zone, int len, unsigned char *buf);

/* Select the active user/password zone for the next operation.  0 on success. */
int  cm_SetUserZone(int zone);

/* Load the GPA Diffie-Hellman parameters (decimal strings) into the GMP context. */
void cm_SetChallengeParams(const char *p, const char *q, const char *g);

/* Run the modular-exponentiation challenge, deriving the GPA cipher key.  0 on success. */
int  cm_ComputeChallenge(void);

/* Fill an 8-byte buffer with fresh random challenge bytes. */
void cm_GetRandomBytes(unsigned char *buf8);

/* GPA key schedule: from challenge c1 + key kin + chip iv, derive 8-byte session keys
 * c2out/c3out (the cipher/encrypt session material). */
void cm_AuthenEncryptMAC(const unsigned char *c1, const unsigned char *kin,
			 const unsigned char *iv,
			 unsigned char *c2out, unsigned char *c3out);

/* Dispatch any pending chip I/O command (binary: fFfFfFfFfFfF1F). */
int  nv2ac_io_request(void);

/* Activate cipher mode for the just-derived session keys (binary: fFfFfFfFfFfF1G). 0 ok. */
int  nv2ac_enable_cipher(void);

/* Activate encrypt mode for the round-2 session keys (binary: fFfFfFfFfFfF1H). 0 ok. */
int  nv2ac_enable_encrypt(void);

/* The OA entry point: complete the two-round dongle handshake. */
int  SetupAtmelForAuthorizations(void);

#endif /* OA_ATMEL_H */
