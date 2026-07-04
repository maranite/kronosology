// SPDX-License-Identifier: GPL-2.0
/*
 * oa_md5.h  -  MD5 (RFC 1321), Stage 2 shared utility.
 *
 * `md5_init`/`md5_append`/`md5_finish`/`md5_process` are real, locally
 * defined symbols in OA.ko (.text+0x4f57d0/0x4f5800/0x4f5900/0x4f4d50) --
 * used by ParseAuth's MD5 cross-check (parse_auth.cpp) but left as
 * unimplemented externs when that file was first reconstructed.
 *
 * Ground-truthed via disassembly to be the well-known, public-domain
 * "L. Peter Deutsch" MD5 reference implementation (originally by Colin
 * Plumb / RSA Data Security -- widely embedded, e.g. in the Independent
 * JPEG Group's libraries and many other public-domain-licensed projects),
 * compiled in unmodified -- same pattern as moancjsd82 turning out to be
 * stock Blowfish (oa_crypto.h). Confirmed exactly, not inferred:
 *   - md5_init writes the canonical RFC 1321 initial ABCD constants
 *     (0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476) at ctx+0x08..0x14,
 *     and zeroes a 2-word bit-count field at ctx+0x00..0x04.
 *   - md5_finish computes the standard pad length
 *     `((55 - (count[0]>>3)) & 63) + 1`, appends 0x80-then-zero padding,
 *     appends the 8-byte little-endian bit-length, and copies abcd[] to the
 *     output digest byte-by-byte in little-endian order -- all exactly the
 *     textbook RFC 1321 finish sequence.
 *
 * Context layout (confirmed from md5_init's write offsets):
 *   +0x00  count[2]   message length in bits, lsw first
 *   +0x08  abcd[4]    running digest state
 *   +0x18  buf[64]    64-byte block accumulator (not touched by md5_init)
 *   total 0x58 = 88 bytes.
 */

#ifndef OA_MD5_H
#define OA_MD5_H

struct md5_state_t {
	unsigned int  count[2];
	unsigned int  abcd[4];
	unsigned char buf[64];
};

extern "C" void md5_init(struct md5_state_t *ctx);
extern "C" void md5_append(struct md5_state_t *ctx, const unsigned char *data, int nbytes);
extern "C" void md5_finish(struct md5_state_t *ctx, unsigned char digest[16]);

#endif /* OA_MD5_H */
