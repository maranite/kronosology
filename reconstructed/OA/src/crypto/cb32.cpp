// SPDX-License-Identifier: GPL-2.0
/*
 * cb32.cpp  -  Crockford Base-32 auth-string decode (Stage 2 shared utility).
 *
 * See include/oa_crypto.h for the full confirmed contract and provenance.
 * Direct port of `UIAppGen/Core/Cb32Codec.cs` (`Decode`), which the file's
 * own doc comment states matches OA.ko's DecodeBytesFromAscii exactly, and
 * independently documented in `docs/confirmed findings/EXs_Auth_Algorithm.md`.
 *
 * Alphabet: 0123456789ACDEFGHJKLMNPQRTUVWXYZ  (Crockford Base-32).
 * Equivalences: B=8, O=0, I=1, S=5 (visually-ambiguous character correction).
 * Quirk: bytes 0..3 of every 5-byte group are stored reversed relative to a
 * plain big-endian bit-stream unpack (i.e. as if each 4-byte run were a
 * little-endian dword) -- byte 4 of each group (if present) is untouched.
 * A 24-character token (this project's fixed auth-string length) decodes to
 * exactly 15 bytes = 3 whole 5-byte groups, so there is never a partial
 * trailing group in practice, but the general case is handled anyway.
 */

#include "oa_crypto.h"

static const char kAlphabet[32] = {
	'0','1','2','3','4','5','6','7','8','9',
	'A','C','D','E','F','G','H','J','K','L','M','N','P','Q','R','T','U','V','W','X','Y','Z',
};

static int cb32_char_value(char c)
{
	if (c >= 'a' && c <= 'z')
		c = (char)(c - 'a' + 'A');
	switch (c) {
	case 'B': c = '8'; break;
	case 'O': c = '0'; break;
	case 'I': c = '1'; break;
	case 'S': c = '5'; break;
	default: break;
	}
	for (int i = 0; i < 32; i++)
		if (kAlphabet[i] == c)
			return i;
	return -1;
}

static void swap_dword_groups(unsigned char *data, unsigned int len)
{
	for (unsigned int g = 0; g < len; g += 5) {
		unsigned int take = (len - g < 5) ? (len - g) : 5;
		if (take < 4)
			continue;
		unsigned char t0 = data[g + 0], t1 = data[g + 1],
			      t2 = data[g + 2], t3 = data[g + 3];
		data[g + 0] = t3;
		data[g + 1] = t2;
		data[g + 2] = t1;
		data[g + 3] = t0;
	}
}

int DecodeBytesFromAscii(unsigned char *out, const char *asciiIn)
{
	unsigned char decoded[32];
	unsigned int decodedLen = 0;
	unsigned long long val = 0;
	int bits = 0;

	for (const char *p = asciiIn; *p; p++) {
		char c = *p;
		int v;

		if (c == '-' || c == ' ')
			continue;
		v = cb32_char_value(c);
		if (v < 0)
			continue;

		val = (val << 5) | (unsigned int)v;
		bits += 5;
		if (bits >= 8) {
			bits -= 8;
			if (decodedLen >= sizeof decoded)
				return -1;
			decoded[decodedLen++] = (unsigned char)((val >> bits) & 0xFF);
		}
	}

	swap_dword_groups(decoded, decodedLen);

	if (decodedLen < 15)
		return -1;

	for (unsigned int i = 0; i < 15; i++)
		out[i] = decoded[i];
	return 0;
}
