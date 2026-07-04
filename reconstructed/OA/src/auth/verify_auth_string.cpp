// SPDX-License-Identifier: GPL-2.0
/*
 * verify_auth_string.cpp  -  VerifyAuthorizationString: the AT88-dongle-gated
 * runtime install/validate path (see include/parse_auth.h).
 *
 * Ground-truthed offset (real OA.ko 3.2.1 ELF symbol table; unchanged in
 * 3.2.2, see MASTER_REFERENCE.md sec 9.2):
 *   VerifyAuthorizationString   .text+0x207de0
 *
 * Disassembly-confirmed (not paraphrase -- corrects this file's earlier,
 * speculative version, see MASTER_REFERENCE.md sec 10.1): the function reads
 * the SAME three AT88 zones (0x10, 0x18, 0x20; 8 bytes each) via the SAME
 * fFfFfFfFfFfF13 wrapper as ParseAuths, sums the three read-status codes, and
 * proceeds only if that sum is zero. It then ASCII-decodes `authString`
 * directly (DecodeBytesFromAscii -- no tokenizing needed, the caller already
 * isolated a single string) and calls ParseAuth() with callback=NULL: this
 * path only validates a candidate auth string (and optionally reports the
 * matched product code via `outCode`), it does not itself authorize.
 */

#include "parse_auth.h"
#include "oa_crypto.h"

int VerifyAuthorizationString(const char *authString, char *outCode)
{
	unsigned char chipKeyMaterial[24];
	int r1 = fFfFfFfFfFfF13(0x10, 8, chipKeyMaterial);
	int r2 = fFfFfFfFfFfF13(0x18, 8, chipKeyMaterial + 8);
	int r3 = fFfFfFfFfFfF13(0x20, 8, chipKeyMaterial + 16);
	if (r1 + r2 + r3 != 0)
		return r1 + r2 + r3;

	unsigned char ciphertext[32];	/* see parse_auth.cpp: size not precisely confirmed */
	if (DecodeBytesFromAscii(ciphertext, authString) != 0)
		return -1;

	return ParseAuth(chipKeyMaterial, ciphertext, 0, outCode);
}
