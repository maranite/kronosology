// SPDX-License-Identifier: GPL-2.0
/*
 * parse_auth.h  -  AuthorizationStrings boot-time parser + runtime auth-string
 * install path. Ground-truthed by direct disassembly of the real OA.ko 3.2.1
 * binary (not paraphrase) -- see MASTER_REFERENCE.md sec 8/10.1.
 *
 *   CSTGInstalledEXProducts::Initialize (.text+0x048620)
 *     -> reads /korg/rw/Startup/AuthorizationStrings
 *     -> ParseAuths()  (.text+0x207c50), callback = AuthorizeProductCallback
 *          -> per token: ParseAuth()  (.text+0x207890)
 *
 *   VerifyAuthorizationString (.text+0x207de0) -- the front-panel-UI runtime
 *   install path -- performs the identical AT88-read + decode + ParseAuth
 *   sequence on a single already-isolated string, with callback = NULL (it
 *   only validates, it does not authorize).
 *
 * CORRECTED FINDINGS (superseding earlier notes in this file and in
 * OA.ko_auth.md, resolved by disassembling ParseAuth/ParseAuths/
 * VerifyAuthorizationString and moancjsd82 directly rather than trusting the
 * doc's paraphrase):
 *
 *   1. There is no 16-byte "UUID". ParseAuth decodes to exactly 15 plaintext
 *      bytes and its callback receives a plain 4-character product code
 *      pointer -- the SAME callback signature as products.cpp's
 *      AuthorizeProductCallback(const char *code4), which the disassembly
 *      confirms IS the callback ParseAuths is called with (relocation in
 *      CSTGInstalledEXProducts::Initialize). This is not
 *      CSTGKLMManager::AuthorizeMultisampleBank -- that call never happens on
 *      this path.
 *
 *   2. ParseAuths ALSO requires the AT88 dongle. It reads AT88 zones 0x10,
 *      0x18, 0x20 (24 bytes: Blowfish key+iv) via fFfFfFfFfFfF13 at its very
 *      entry, before touching any token, and returns immediately (processing
 *      nothing) if that read fails. The "boot-time path does not require the
 *      dongle" claim (from OA.ko_auth.md and this project's own earlier
 *      notes) does not hold up against the disassembly.
 *
 *   3. ParseAuth's real parameters are NOT (token, tokenLen). They are the
 *      24-byte AT88 key material and a SEPARATELY ascii-decoded ciphertext
 *      pointer (via DecodeBytesFromAscii, oa_crypto.h) -- see ParseAuth()'s
 *      declaration below.
 *
 * Decoded 15-byte plaintext layout (hardware-verified,
 * Tools/expansion_tools/kronos_auth.py):
 *   [0..7]    rand8
 *   [8..11]   4-char product code (used to build the /korg/rw/Options/<code>
 *             path via the real `kOptionsPath` global, and passed to the
 *             callback)
 *   [12..14]  MD5 check bytes: compared against digest[3], digest[7],
 *             digest[11] of MD5(plaintext[0:12] + optionsFileContent)
 */

#ifndef OA_PARSE_AUTH_H
#define OA_PARSE_AUTH_H

/* Matches products.cpp's AuthorizeProductCallback / AuthorizeProductByFilename
 * signature -- confirmed to be the actual callback wired up at the real call
 * site (CSTGInstalledEXProducts::Initialize). */
typedef unsigned int (*OA_ProductAuthCallback)(const char *code4);

/*
 * Read the AT88 dongle (zones 0x10/0x18/0x20), tokenize `buffer` (the raw
 * contents of .../Startup/AuthorizationStrings) on whitespace, ASCII-decode
 * and process each token via ParseAuth(), invoking `callback` for every
 * decode+MD5 match. Returns 0 if the dongle read succeeded (regardless of how
 * many/few tokens matched), nonzero if the dongle read itself failed -- in
 * which case no tokens are processed at all.
 */
int ParseAuths(const char *buffer, unsigned int length, OA_ProductAuthCallback callback);

/*
 * Decrypt one already-ASCII-decoded token (`ciphertext`, produced by
 * DecodeBytesFromAscii) using the 24-byte AT88 `chipKeyMaterial` (key[16] +
 * iv[8]), MD5-cross-check the result against the product's own Options file,
 * and -- only on a match, and only if `callback` is non-null -- invoke
 * `callback` with the matched 4-character product code. If `outCode` is
 * non-null, the matched code + a null terminator (5 bytes) are copied there
 * regardless of whether a callback was invoked. Returns 0 on success.
 */
int ParseAuth(const unsigned char chipKeyMaterial[24], const unsigned char *ciphertext,
	      OA_ProductAuthCallback callback, char *outCode);

/*
 * Runtime, front-panel-UI install/validate path: reads the AT88 dongle itself
 * (same zones as ParseAuths), ASCII-decodes `authString` directly (no
 * tokenizing -- it is already a single isolated string), and calls
 * ParseAuth() with callback=NULL (validate only, does not authorize). If
 * `outCode` is non-null, receives the matched product code on success.
 * Returns 0 on success.
 */
int VerifyAuthorizationString(const char *authString, char *outCode);

#endif /* OA_PARSE_AUTH_H */
