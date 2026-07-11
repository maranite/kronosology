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

/*
 * Read `len` bytes of chip user-zone `zone` into buf.  0 on success.
 *
 * UPDATE (batch 46): confirmed real ground-truth identity is
 * `fFfFfFfFfFfF1C` (.text+0x4f4a80) -- `SetupAtmelForAuthorizations`'s
 * own two real call sites (zone 0x19/len 7 for the config zone, zone
 * 0x50/len 8 for the IV) match `fFfFfFfFfFfF1C`'s own confirmed
 * relocations from ground truth's `SetupAtmelForAuthorizations`
 * (.text+0x207a50) exactly (same EAX=zone/EDX=len register values at
 * both call sites). Real body now lives in src/auth/atmel_zone_io.cpp,
 * alongside its sibling `fFfFfFfFfFfF13` (used directly, no friendly
 * alias, by ParseAuths/VerifyAuthorizationString -- see oa_crypto.h) --
 * both share the same AT88 command-dispatch-and-DEAX-decode shape,
 * confirmed via full disassembly of both.
 */
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
 * c2out/c3out (the cipher/encrypt session material). `iv` is NOT const --
 * confirmed real (batch 43, src/auth/atmel_deax.cpp): this call mutates
 * iv in place (iv[0] is forced to 0xff, iv[1..7] are rewritten from the
 * DEAX cipher state), which is why SetupAtmelForAuthorizations() reuses
 * the same `iv` buffer, unmodified by itself, across both handshake
 * rounds -- round 2 depends on round 1's mutation. */
void cm_AuthenEncryptMAC(const unsigned char *c1, const unsigned char *kin,
			 unsigned char *iv,
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

/*
 * bzzzzzzzzzzzt12(in)/DeaxCurrentGpa() (batch 46) -- the DEAX cipher's
 * real single-step primitive (ground truth `.text+0x4f3d00`, confirmed
 * via non-obfuscated `.bss` symbol names in this OA.ko_Decomp image:
 * `gpa_byte`@0x5c90c1, `mode`@0x5c90c0) plus a same-behavior gpa-read
 * accessor. Defined in src/auth/atmel_deax.cpp, operating on the exact
 * persistent `DeaxState` that file's own `cm_AuthenEncryptMAC` uses --
 * called directly by `fFfFfFfFfFfF13`/`cm_ReadUserZone`
 * (src/auth/atmel_zone_io.cpp), matching ground truth's own call graph.
 * `extern "C"` (unlike this header's other declarations) since both
 * sides that need to agree on linkage are plain C++ but declared in two
 * different headers/TUs -- matching this project's own established
 * convention of using extern "C" specifically to pin down a cross-TU
 * contract unambiguously.
 */
extern "C" void bzzzzzzzzzzzt11(void);
extern "C" void bzzzzzzzzzzzt12(unsigned char in);
extern "C" unsigned char DeaxCurrentGpa(void);

/*
 * `mode` (batch 46) -- confirmed real, non-obfuscated ground-truth global
 * name (.bss+0x5c90c0, immediately adjacent to `gpa_byte`@0x5c90c1).
 * Defined in src/auth/atmel_zone_io.cpp, defaults to 0 (no setter
 * reconstructed yet -- almost certainly `nv2ac_enable_cipher`/
 * `nv2ac_enable_encrypt` above, both still stubs). Declared here (not
 * just locally in atmel_zone_io.cpp) so a future batch reconstructing
 * either setter, or a host KAT exercising fFfFfFfFfFfF13/cm_ReadUserZone's
 * mode-dependent behavior, has one shared declaration to use.
 */
extern "C" unsigned char mode;

#endif /* OA_ATMEL_H */
