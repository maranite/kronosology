// SPDX-License-Identifier: GPL-2.0
/*
 * products.cpp  -  installed-EX-product authorization entry points.
 *
 * These are the glue that turns a 4-character product code (a filename stem from the
 * AuthorizationStrings file, or a CD-ROM command argument) into a call to
 * CSTGKLMManager::AuthorizeProduct: find the matching installed product in the registry,
 * mark it authorized (+0xa2), and authorize all the content it declares.
 *
 * Recovered from OA_322.ko: CSTGInstalledEXProducts::AuthorizeProductByFilename @0x481d0
 * and the C-callable AuthorizeProductCallback @0x47fa0.
 */

#include "auth.h"
#include "oa_types.h"
#include "oa_internal.h"   /* strlen */
#include "oa_heap.h"       /* oa_heap_base(), oa_heap_region() */
#include "parse_auth.h"    /* VerifyAuthorizationString() */

/* Compare two 4-byte product codes (the binary does a 4-byte forward rep-compare). */
static inline bool code4_eq(const char *a, const unsigned char *b)
{
	return a[0] == (char)b[0] && a[1] == (char)b[1] &&
	       a[2] == (char)b[2] && a[3] == (char)b[3];
}

/*
 * Iterate this registry's product array; the first product whose 4-char code matches code4
 * is flagged authorized and run through AuthorizeProduct.  Returns 1 if any matched, else 0.
 * (The binary keeps scanning after a match, so duplicate codes are all authorized.)
 */
unsigned int CSTGInstalledEXProducts::AuthorizeProductByFilename(const char *code4)
{
	struct CSTGEXProductInfo *products =
		(struct CSTGEXProductInfo *)oa_heap_region(slotIndex);
	unsigned int matched = 0;

	for (unsigned int i = 0; i < count; i++) {
		if (!code4_eq(code4, products[i].code))
			continue;
		products[i].authorized = 1;
		CSTGKLMManager::sInstance->AuthorizeProduct(&products[i]);
		matched = 1;
	}
	return matched;
}

/*
 * C-callable authorization callback (registered with the install/CD-ROM path).  The code
 * must be exactly 4 characters.  Returns:
 *    0          -> a product was found and authorized,
 *   -1 (0xff..) -> the code was not 4 characters long,
 *   -2 (0xfe..) -> no installed product matched.
 */
extern "C" unsigned int AuthorizeProductCallback(const char *code4)
{
	if (strlen(code4) != 4)
		return 0xffffffffu;			/* -1: bad length */

	struct CSTGInstalledEXProducts *reg =
		(struct CSTGInstalledEXProducts *)(oa_heap_base() + 0x14);

	return reg->AuthorizeProductByFilename(code4) ? 0u : 0xfffffffeu;	/* 0 / -2 */
}

/*
 * Real file-append target for the "AU:" /proc/.oacmd command
 * (/korg/rw/Startup/AuthorizationStrings) -- a real global, confirmed via
 * relocation, same style as parse_auth.cpp's kOptionsPath (a 4-byte pointer
 * OBJECT, not a char array -- confirmed from the ELF symbol table size).
 */
extern "C" const char *kAuthFileName;

/* Generic file handle I/O, used throughout OA.ko; mode=2/whence=2 are
 * confirmed-from-disassembly magic values whose symbolic names (if any)
 * aren't recovered yet. */
extern "C" void *CSTGFile_Open(const char *path, int mode);
extern "C" int  CSTGFile_Seek(void *handle, int offset, int whence);
extern "C" int  CSTGFile_Write(void *handle, const void *buf, unsigned int len);
extern "C" int  CSTGFile_Close(void *handle);

/*
 * VerifyAndSaveAuthString: the real implementation behind ProcessOACmd's
 * "AU:<24-char-string>" /proc/.oacmd command (process_oacmd.cpp).
 * Disassembly-confirmed, .text+0x48290, 494 bytes.
 *
 * Two-pass structure exactly as compiled (not simplified to one pass,
 * because the two passes behave differently -- see below):
 *   Pass 1: if ANY product whose code matches is already authorized,
 *           return true immediately -- no re-authorization, no file append.
 *           (Idempotent resubmission of an already-applied valid string.)
 *   Pass 2: only reached if pass 1 found no already-authorized match.
 *           Authorizes every NOT-yet-authorized product whose code matches
 *           (duplicates all get authorized, matching
 *           AuthorizeProductByFilename's documented behaviour above), then
 *           appends the raw auth string + "\n" to kAuthFileName.
 *
 * FAITHFUL QUIRK, preserved rather than "fixed": on the append path, the
 * function's own return value ends up being the trailing-newline write's
 * success, not the authorization result. If kAuthFileName can't be opened at
 * all, this returns false even though the in-memory authorization (pass 2)
 * already succeeded. If the auth-string body write is short, the function
 * returns true anyway (only the final single-byte "\n" write's result
 * actually gates the return value).
 */
bool CSTGInstalledEXProducts::VerifyAndSaveAuthString(const char *authString)
{
	char outCode[5];
	if (VerifyAuthorizationString(authString, outCode) != 0)
		return false;
	if (count == 0)
		return false;

	struct CSTGEXProductInfo *products =
		(struct CSTGEXProductInfo *)oa_heap_region(slotIndex);
	if (!products)
		return false;

	for (unsigned int i = 0; i < count; i++) {
		if (code4_eq(outCode, products[i].code) && products[i].authorized)
			return true;
	}

	bool foundAny = false;
	for (unsigned int i = 0; i < count; i++) {
		if (!code4_eq(outCode, products[i].code))
			continue;
		products[i].authorized = 1;
		CSTGKLMManager::sInstance->AuthorizeProduct(&products[i]);
		foundAny = true;
	}
	if (!foundAny)
		return false;

	void *fh = CSTGFile_Open(kAuthFileName, 2);
	if (!fh)
		return false;
	CSTGFile_Seek(fh, 0, 2);		/* SEEK_END */

	unsigned int len = strlen(authString);
	if ((unsigned int)CSTGFile_Write(fh, authString, len) != len) {
		CSTGFile_Close(fh);
		return true;			/* authorization itself already succeeded */
	}
	bool ok = (CSTGFile_Write(fh, "\n", 1) == 1);
	CSTGFile_Close(fh);
	return ok;
}
