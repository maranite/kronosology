// SPDX-License-Identifier: GPL-2.0
/*
 * bignum.h  -  the 130-bit modular-exponentiation primitive OA.ko/loadmod.ko
 * use to derive the GPA challenge seed ("p2") from the chip's IdN, and the
 * IdN-squaring wrapper around it.
 *
 * Ported directly from `KronosExtract/source/kronos_extract.c`'s own
 * `ke_bn5_*`/`ke_synth_sdflkjsvnd2g` -- a self-contained, GMP-free
 * reimplementation of loadmod.ko's `sdflkjsvnd2g`, already confirmed
 * against real hardware by an earlier phase of this project (see that
 * file's own comment: "Algorithm (from Ghidra disassembly)"). Not
 * re-derived; just carried over so this module has no GMP dependency
 * either (`STGGmp.ko` is a separate, already-reconstructed module -- this
 * chip emulator deliberately doesn't need it).
 *
 * All numbers are 5x32-bit words, little-endian word order (word[0] =
 * least significant). N1/N2/e0 are fixed constants baked into loadmod.ko
 * (decimal strings in the real binary, same ones atmel_setup.cpp's
 * GPA_P/GPA_Q/GPA_G reference) -- not per-device secrets.
 */

#ifndef AT88_BIGNUM_H
#define AT88_BIGNUM_H

#define BN_WORDS5 5
#define BN_WORDS9 9

/* r[5] = base^exp[4] mod n -- right-to-left binary, 128-bit array exponent. */
void bn5_modexp_arr(unsigned int *r, const unsigned int *base,
		    const unsigned int *exp, const unsigned int *n);

/*
 * synth_sdflkjsvnd2g -- exact replica of loadmod.ko's sdflkjsvnd2g / OA.ko's
 * cm_ComputeChallenge. Derives the 8-byte GPA challenge seed ("p2") from the
 * chip's 7-byte IdN (config zone 0x19..0x1f).
 */
void synth_sdflkjsvnd2g(const unsigned char *idn, unsigned char *p2);

#endif /* AT88_BIGNUM_H */
