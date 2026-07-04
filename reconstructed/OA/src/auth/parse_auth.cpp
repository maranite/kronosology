// SPDX-License-Identifier: GPL-2.0
/*
 * parse_auth.cpp  -  see include/parse_auth.h for the corrected, ground-truthed
 * scheme this implements (this file previously had the argument roles backwards
 * and wrongly modeled a 16-byte "UUID" that does not exist on this call path --
 * see MASTER_REFERENCE.md sec 10.1 for the correction history).
 *
 * Ground-truthed offsets (real OA.ko 3.2.1 ELF symbol table; unchanged in
 * 3.2.2, see MASTER_REFERENCE.md sec 9.2):
 *   ParseAuths   .text+0x207c50
 *   ParseAuth    .text+0x207890
 */

#include "parse_auth.h"
#include "oa_crypto.h"
#include "oa_md5.h"

/* Shared file-IO helpers used throughout OA.ko; not yet reconstructed as their
 * own Stage-2 unit. */
extern "C" int CSTGFile_FileExists(const char *path);
extern "C" unsigned char *CSTGFile_ReadFileIntoNewBuffer(const char *path, unsigned int *outLen);
extern "C" void CSTGFile_FreeReadBuffer(unsigned char *buf);

/* Real global (confirmed via relocation), base of the EXs product options
 * directory: /korg/rw/Options/ (matches Tools/expansion_tools/kronos_auth.py's
 * "--exsins auto-discovers sibling S-file" lookup). NOTE: this is a 4-byte
 * pointer OBJECT in the ELF symbol table, not a char array -- declaring it
 * as `const char kOptionsPath[]` (as an earlier pass here did) would read
 * the pointer variable's own raw bytes as if they were string data. */
extern "C" const char *kOptionsPath;

static inline bool is_token_char(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') || c == '-';
}

/*
 * ParseAuths: read the AT88 dongle once (zones 0x10/0x18/0x20 -> 24-byte
 * key+iv block), then tokenize+decode+dispatch every valid token in `buffer`.
 * Returns the summed AT88 read status (0 = dongle read ok; matches the real
 * function's own return value, which is that same sum, not a separate error
 * code) -- if the dongle read fails, no tokens are processed at all.
 */
int ParseAuths(const char *buffer, unsigned int length, OA_ProductAuthCallback callback)
{
	unsigned char chipKeyMaterial[24];
	int r1 = fFfFfFfFfFfF13(0x10, 8, chipKeyMaterial);
	int r2 = fFfFfFfFfFfF13(0x18, 8, chipKeyMaterial + 8);
	int r3 = fFfFfFfFfFfF13(0x20, 8, chipKeyMaterial + 16);
	if (r1 + r2 + r3 != 0)
		return r1 + r2 + r3;

	unsigned int i = 0;
	while (i < length) {
		while (i < length && !is_token_char((unsigned char)buffer[i]))
			i++;
		unsigned int start = i;
		while (i < length && is_token_char((unsigned char)buffer[i]))
			i++;
		unsigned int tokLen = i - start;
		if (tokLen == 0)
			continue;

		/* NOT YET RECOVERED: exact max token length the real loop enforces
		 * (disassembly shows an internal countdown against the buffer's
		 * remaining length, not a fixed per-token cap) -- 0x100 used here
		 * as a safe bound, matching the stack buffer size observed at the
		 * real call site. */
		char token[0x101];
		if (tokLen > 0x100)
			tokLen = 0x100;
		for (unsigned int k = 0; k < tokLen; k++)
			token[k] = buffer[start + k];
		token[tokLen] = '\0';

		unsigned char ciphertext[32];	/* size not precisely confirmed; DecodeBytesFromAscii's own gate rejects too-short input */
		if (DecodeBytesFromAscii(ciphertext, token) != 0)
			continue;	/* token too short/invalid; keep scanning */

		ParseAuth(chipKeyMaterial, ciphertext, callback, 0);
	}
	return 0;
}

/*
 * ParseAuth: decrypt `ciphertext` with `chipKeyMaterial`, MD5-cross-check
 * against the named product's Options file, and dispatch to `callback` (if
 * non-null) on match.
 */
int ParseAuth(const unsigned char chipKeyMaterial[24], const unsigned char *ciphertext,
	      OA_ProductAuthCallback callback, char *outCode)
{
	unsigned char decoded[15];
	if (moancjsd82(chipKeyMaterial, ciphertext, sizeof decoded, decoded) != (int)sizeof decoded)
		return -1;

	/* path = kOptionsPath + 4-char product code (decoded[8..11]) */
	char path[64];
	unsigned int prefixLen = 0;
	for (; kOptionsPath[prefixLen]; prefixLen++)
		path[prefixLen] = kOptionsPath[prefixLen];
	for (unsigned int k = 0; k < 4; k++)
		path[prefixLen + k] = (char)decoded[8 + k];
	path[prefixLen + 4] = '\0';
	const char *code4 = path + prefixLen;

	if (!CSTGFile_FileExists(path))
		return -1;

	unsigned int fileLen = 0;
	unsigned char *fileData = CSTGFile_ReadFileIntoNewBuffer(path, &fileLen);
	if (!fileData)
		return -1;

	struct md5_state_t ctx;
	unsigned char digest[16];
	md5_init(&ctx);
	md5_append(&ctx, decoded, 12);
	md5_append(&ctx, fileData, (int)fileLen);
	md5_finish(&ctx, digest);
	CSTGFile_FreeReadBuffer(fileData);

	if (digest[3] != decoded[12] || digest[7] != decoded[13] || digest[11] != decoded[14])
		return -1;

	if (outCode) {
		for (unsigned int k = 0; k < 4; k++)
			outCode[k] = code4[k];
		outCode[4] = '\0';
	}

	if (callback)
		return callback(code4) == 0 ? 0 : -1;
	return 0;
}
