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
