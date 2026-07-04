// SPDX-License-Identifier: GPL-2.0
/*
 * md5.cpp  -  MD5 (RFC 1321), Stage 2 shared utility. See include/oa_md5.h
 * for the ground-truthing (context layout, confirmed real symbol names,
 * confirmed-standard padding/finish sequence).
 *
 * The compression function (md5_process here) is written in the standard
 * per-round-table loop form rather than the classic reference's fully
 * unrolled macros -- algorithmically identical (same K[]/shift-amount
 * tables, same round functions F/G/H/I, same message-word selection), just
 * less error-prone to transcribe correctly. Verified against the official
 * RFC 1321 Appendix A.5 test suite in verify/test_crypto.cpp.
 */

#include "oa_md5.h"

static const unsigned int K[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static const unsigned int SHIFT[64] = {
	7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
	5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
	4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
	6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
};

static inline unsigned int rotl32(unsigned int x, unsigned int c)
{
	return (x << c) | (x >> (32 - c));
}

static void md5_process(struct md5_state_t *ctx, const unsigned char block[64])
{
	unsigned int M[16];
	for (int i = 0; i < 16; i++)
		M[i] = (unsigned int)block[i * 4] |
		       ((unsigned int)block[i * 4 + 1] << 8) |
		       ((unsigned int)block[i * 4 + 2] << 16) |
		       ((unsigned int)block[i * 4 + 3] << 24);

	unsigned int A = ctx->abcd[0], B = ctx->abcd[1], C = ctx->abcd[2], D = ctx->abcd[3];

	for (int i = 0; i < 64; i++) {
		unsigned int f, g;

		if (i < 16) {
			f = (B & C) | (~B & D);
			g = (unsigned int)i;
		} else if (i < 32) {
			f = (D & B) | (~D & C);
			g = (unsigned int)(5 * i + 1) % 16;
		} else if (i < 48) {
			f = B ^ C ^ D;
			g = (unsigned int)(3 * i + 5) % 16;
		} else {
			f = C ^ (B | ~D);
			g = (unsigned int)(7 * i) % 16;
		}

		unsigned int tmp = D;
		D = C;
		C = B;
		unsigned int sum = A + f + K[i] + M[g];
		B = B + rotl32(sum, SHIFT[i]);
		A = tmp;
	}

	ctx->abcd[0] += A;
	ctx->abcd[1] += B;
	ctx->abcd[2] += C;
	ctx->abcd[3] += D;
}

extern "C" void md5_init(struct md5_state_t *ctx)
{
	ctx->count[0] = 0;
	ctx->count[1] = 0;
	ctx->abcd[0] = 0x67452301u;
	ctx->abcd[1] = 0xefcdab89u;
	ctx->abcd[2] = 0x98badcfeu;
	ctx->abcd[3] = 0x10325476u;
}

extern "C" void md5_append(struct md5_state_t *ctx, const unsigned char *data, int nbytes)
{
	unsigned int bufIndex = (ctx->count[0] >> 3) & 0x3f;
	unsigned int oldCount0 = ctx->count[0];

	ctx->count[0] += (unsigned int)nbytes << 3;
	if (ctx->count[0] < oldCount0)
		ctx->count[1]++;
	ctx->count[1] += (unsigned int)nbytes >> 29;

	unsigned int i = 0;
	unsigned int space = 64 - bufIndex;

	if ((unsigned int)nbytes >= space) {
		for (unsigned int k = 0; k < space; k++)
			ctx->buf[bufIndex + k] = data[k];
		md5_process(ctx, ctx->buf);

		for (i = space; i + 64 <= (unsigned int)nbytes; i += 64)
			md5_process(ctx, data + i);

		bufIndex = 0;
	}

	for (; i < (unsigned int)nbytes; i++)
		ctx->buf[bufIndex++] = data[i];
}

extern "C" void md5_finish(struct md5_state_t *ctx, unsigned char digest[16])
{
	unsigned char lengthBytes[8];
	for (int i = 0; i < 4; i++)
		lengthBytes[i] = (unsigned char)(ctx->count[0] >> (8 * i));
	for (int i = 0; i < 4; i++)
		lengthBytes[4 + i] = (unsigned char)(ctx->count[1] >> (8 * i));

	unsigned int padLen = ((55u - (ctx->count[0] >> 3)) & 63u) + 1u;

	static const unsigned char PAD[64] = { 0x80 };	/* rest zero-initialized */
	md5_append(ctx, PAD, (int)padLen);
	md5_append(ctx, lengthBytes, 8);

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			digest[i * 4 + j] = (unsigned char)(ctx->abcd[i] >> (8 * j));
}
