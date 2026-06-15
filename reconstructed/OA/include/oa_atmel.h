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
 * Symbol note: cm_* are the CryptoMemory primitives; the nv2ac_* entries are the OA_322.ko
 * functions @0x4f4c80/0x4f4cf0/0x4f4d20 (de-obfuscated from fFfFfFfFfFfF1F/1G/1H).
 */

#ifndef OA_ATMEL_H
#define OA_ATMEL_H

/* Read `len` bytes of chip user-zone `zone` into buf.  0 on success. */
int  cm_ReadUserZone(int zone, int len, unsigned char *buf);

/* Select the active user/password zone for the next operation.  0 on success. */
int  cm_SetUserZone(int zone);

/* Load the GPA Diffie-Hellman parameters (decimal strings) into the GMP context. */
void cm_SetChallengeParams(const char *p, const char *q, const char *g);

/* Run the modular-exponentiation challenge over chipConfig, writing the 8-byte GPA cipher
 * key to gpaKeyOut.  `sel` selects the key (0 here).  Returns 0 on success. */
int  cm_ComputeChallenge(const unsigned char *chipConfig, int sel, unsigned char *gpaKeyOut);

/* Fill `len` bytes of buf with fresh random challenge bytes. */
void cm_GetRandomBytes(unsigned char *buf, int len);

/* GPA key schedule: from challenge c1 + key kin + chip iv, derive 8-byte session keys
 * c2out/c3out (the cipher/encrypt session material). */
void cm_AuthenEncryptMAC(const unsigned char *c1, const unsigned char *kin,
			 const unsigned char *iv,
			 unsigned char *c2out, unsigned char *c3out);

/* Dispatch any pending chip I/O command (binary: nv2ac_dispatch_cmd @0x4f4c80). */
int  nv2ac_dispatch_cmd(void);

/*
 * Run the chip challenge/answer for an auth zone and return the chip status (0 == ok); also
 * latches the global cipher `mode`.  `sel` is 0 at the OA call site; challenge/c2 are the
 * round's random challenge and derived cipher session key.  enable_cipher uses chip auth
 * zone 0x00 (binary: nv2ac_enable_cipher @0x4f4cf0), enable_encrypt uses zone 0x10
 * (nv2ac_enable_encrypt @0x4f4d20).
 */
int  nv2ac_enable_cipher(unsigned char sel, const unsigned char *challenge,
			 const unsigned char *c2);
int  nv2ac_enable_encrypt(unsigned char sel, const unsigned char *challenge,
			  const unsigned char *c2);

/* The OA entry point: complete the two-round dongle handshake. */
int  SetupAtmelForAuthorizations(void);

#endif /* OA_ATMEL_H */
