// SPDX-License-Identifier: GPL-2.0
/*
 * test_cuuid_convert.cpp  -  host-side known-answer test for
 * CUUID::ConvertFromText (see ../src/auth/cuuid_convert.cpp).
 *
 * Host mocks for the two genuine kernel externs this function calls:
 *   - _ctype[256]: real Linux lib/ctype.c bit table, only the two bits
 *     this function reads (_D=0x04, _X=0x40) need to be correct.
 *   - simple_strtoul: real kernel signature: matches libc strtoul's
 *     behavior for base-16, unsigned parsing, close enough for this
 *     function's own always-2-hex-digit-input use.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_types.h"

static int g_fail;
static void check_bool(const char *label, bool got, bool want)
{
	if (got == want) {
		printf("  ok    %-40s %s\n", label, got ? "true" : "false");
		return;
	}
	printf("  FAIL  %-40s got=%s want=%s\n", label, got ? "true" : "false", want ? "true" : "false");
	g_fail++;
}
static void check_bytes(const char *label, const unsigned char *got, const unsigned char *want, int n)
{
	if (memcmp(got, want, n) == 0) {
		printf("  ok    %-40s matches\n", label);
		return;
	}
	printf("  FAIL  %-40s mismatch:\n    got : ", label);
	for (int i = 0; i < n; i++) printf("%02x ", got[i]);
	printf("\n    want: ");
	for (int i = 0; i < n; i++) printf("%02x ", want[i]);
	printf("\n");
	g_fail++;
}

extern "C" {

/* Real Linux lib/ctype.c bit flags -- only _D (0x04) and _X (0x40) matter
 * to this function; the rest of the table is left zero (unused here). */
unsigned char _ctype[256];

unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base)
{
	return strtoul(cp, endp, base);
}

} /* extern "C" */

static void init_ctype_table(void)
{
	memset(_ctype, 0, sizeof(_ctype));
	for (int c = '0'; c <= '9'; c++) _ctype[c] |= 0x04;	/* _D */
	for (int c = 'A'; c <= 'F'; c++) _ctype[c] |= 0x40;	/* _X */
	for (int c = 'a'; c <= 'f'; c++) _ctype[c] |= 0x40;	/* _X */
	/* Non-hex letters get neither bit -- matches real _ctype's own
	 * behavior for this function's purposes (isxdigit semantics). */
}

int main(void)
{
	init_ctype_table();

	printf("[1] Valid canonical UUID (mixed case):\n");
	{
		CUUID uuid;
		memset(&uuid, 0xAA, sizeof(uuid));
		bool ok = uuid.ConvertFromText("01234567-89AB-cdef-0123-456789abcdef");
		check_bool("return value", ok, true);
		unsigned char want[16] = {
			0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xcd, 0xef,
			0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
		};
		check_bytes("parsed bytes", uuid.bytes, want, 16);
	}

	printf("\n[2] Valid UUID with trailing payload (LM/LD/CM/CD grammar tail):\n");
	{
		CUUID uuid;
		bool ok = uuid.ConvertFromText("00000000-0000-0000-0000-000000000001:5:1:2");
		check_bool("return value", ok, true);
		unsigned char want[16] = {
			0,0,0,0, 0,0, 0,0, 0,0, 0,0,0,0,0,1
		};
		check_bytes("parsed bytes (trailer ignored)", uuid.bytes, want, 16);
	}

	printf("\n[3] Too short (35 chars, one short of 36):\n");
	{
		CUUID uuid;
		bool ok = uuid.ConvertFromText("01234567-89ab-cdef-0123-456789abcde");
		check_bool("return value", ok, false);
	}

	printf("\n[4] Exactly 36 chars but malformed (missing a dash):\n");
	{
		CUUID uuid;
		bool ok = uuid.ConvertFromText("0123456789ab-cdef-0123-456789abcdefX");
		check_bool("return value", ok, false);
	}

	printf("\n[5] Non-hex character in a digit position:\n");
	{
		CUUID uuid;
		bool ok = uuid.ConvertFromText("0123456g-89ab-cdef-0123-456789abcdef");
		check_bool("return value", ok, false);
	}

	printf("\n[6] Wrong dash position (dash one char early):\n");
	{
		CUUID uuid;
		bool ok = uuid.ConvertFromText("0123456-789ab-cdef-0123-456789abcdef");
		check_bool("return value", ok, false);
	}

	printf("\n[7] Empty string:\n");
	{
		CUUID uuid;
		bool ok = uuid.ConvertFromText("");
		check_bool("return value", ok, false);
	}

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
