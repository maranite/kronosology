// SPDX-License-Identifier: GPL-2.0
/*
 * pairfact_fixture.cpp  -  see pairfact_fixture.h.
 */

#include "pairfact_fixture.h"

/* The one known captured /.pairFact3 blob (docs/crypto/cryptoloop_keys.md,
 * MD5 817956d550647905828e115f9eae7a0e -- re-verified against the file on
 * disk when this was written). Chip-encrypted; content is opaque to us,
 * used only for exact-match recognition. */
static const unsigned char KNOWN_PAIRFACT3_BLOB[PAIRFACT_BLOB_LEN] = {
	0x0a,0xe6,0xe2,0x57,0x39,0x78,0x4e,0xca,0x47,0x50,0x7e,0x83,0x30,0x0d,0x06,0xfa,
	0xb9,0x6f,0xe5,0x62,0x0b,0xd5,0xd0,0x2b,0x83,0x95,0x6e,0x0a,0xbf,0xdc,0x6c,0x06,
	0x37,0x03,0xc8,0xed,0x11,0xd3,0xc2,0xe1,0x95,0x61,0x63,0x3f,0x95,0x18,0xb8,0xba,
	0x74,0x29,0xeb,0x35,0xc9,0x70,0x9f,0xbe,0x9b,0xd4,0x36,0x8a,0x1d,0xb3,0xf0,0x94,
	0xca,0x9d,0xab,0xb7,0x23,0x9e,0x3d,0x53,0x8f,0xf7,0x1d,0x5e,0xaf,0x98,0x04,0xa2,
};

/*
 * Raw 16-byte-per-volume key material, reconstructed from the known
 * confirmed-universal 31-char ASCII keys (docs/crypto/cryptoloop_keys.md's
 * "The recovered keys" table). Each raw16[i] is 15 full bytes decoded
 * directly from the first 30 hex chars, plus the 16th byte's HIGH nibble
 * from the 31st (last) hex char. The 16th byte's LOW nibble is set to 0 --
 * per that doc's own "Key format" analysis, loadmod.ko's HexEncode()
 * writes only 31 of the 32 hex characters a full 16-byte encoding would
 * produce, so this nibble is provably never emitted regardless of its
 * true value; 0 is as good as any other choice here.
 *
 * Mod:        a336a15cd841ec8926b99e7c3884eaa + high nibble 'a' -> 0xa0
 * Eva:        342ee59d549c7d329d835537be0540d + high nibble 'd' -> 0xd0
 * WaveMotion: 3e72c0e59fc017a9eb7d7e1168a4cdb + high nibble 'b' -> 0xb0
 */
static const unsigned char RAW_KEY_MOD[PAIRFACT_KEY_LEN] = {
	0xa3,0x36,0xa1,0x5c,0xd8,0x41,0xec,0x89,0x26,0xb9,0x9e,0x7c,0x38,0x84,0xea,0xa0,
};
static const unsigned char RAW_KEY_EVA[PAIRFACT_KEY_LEN] = {
	0x34,0x2e,0xe5,0x9d,0x54,0x9c,0x7d,0x32,0x9d,0x83,0x55,0x37,0xbe,0x05,0x40,0xd0,
};
static const unsigned char RAW_KEY_WAVEMOTION[PAIRFACT_KEY_LEN] = {
	0x3e,0x72,0xc0,0xe5,0x9f,0xc0,0x17,0xa9,0xeb,0x7d,0x7e,0x11,0x68,0xa4,0xcd,0xb0,
};

int pairfact_fixture_lookup(const unsigned char *blob, unsigned int blobLen,
			     unsigned char *outKeys48)
{
	if (blobLen != PAIRFACT_BLOB_LEN)
		return -1;

	for (unsigned int i = 0; i < PAIRFACT_BLOB_LEN; i++)
		if (blob[i] != KNOWN_PAIRFACT3_BLOB[i])
			return -1;

	for (int i = 0; i < PAIRFACT_KEY_LEN; i++) {
		outKeys48[i]                        = RAW_KEY_MOD[i];
		outKeys48[PAIRFACT_KEY_LEN + i]      = RAW_KEY_EVA[i];
		outKeys48[2 * PAIRFACT_KEY_LEN + i]  = RAW_KEY_WAVEMOTION[i];
	}
	return 0;
}

static unsigned char hex_nibble_ascii(unsigned char nibble)
{
	nibble &= 0xf;
	return (nibble < 10) ? (unsigned char)('0' + nibble)
			      : (unsigned char)('a' + (nibble - 10));
}

void hexencode_31char(const unsigned char *raw16, unsigned char *out32)
{
	for (int i = 0; i < 32; i++)
		out32[i] = 0;		/* pre-zeroed 32-byte buffer */

	/* Full encoding would be 32 chars (2 per input byte); only write the
	 * first 31 -- confirmed quirk, see pairfact_fixture.h. */
	int written = 0;
	for (int i = 0; i < PAIRFACT_KEY_LEN && written < 31; i++) {
		unsigned char hi = (unsigned char)(raw16[i] >> 4);
		unsigned char lo = (unsigned char)(raw16[i] & 0xf);
		out32[written++] = hex_nibble_ascii(hi);
		if (written < 31)
			out32[written++] = hex_nibble_ascii(lo);
	}
	/* out32[31] is left as the pre-zeroed 0x00. */
}
