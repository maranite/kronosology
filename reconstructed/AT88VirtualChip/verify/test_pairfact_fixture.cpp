// SPDX-License-Identifier: GPL-2.0
/*
 * test_pairfact_fixture.cpp  -  known-answer tests for pairfact_fixture.cpp.
 *
 * Ground truth used here: the REAL captured /.pairFact3 blob (loaded from
 * disk, MD5-verified against docs/crypto/cryptoloop_keys.md's documented
 * value) as input, and the REAL confirmed-universal 31-char ASCII keys
 * (recovered independently via LOOP_GET_STATUS64 on live hardware, per
 * that same doc) as the expected output -- checked via a full round trip
 * through hexencode_31char(), the same quirky truncation loadmod.ko's own
 * HexEncode() performs. This is the strongest test available for this
 * fixture: it doesn't just check "does the code run", it confirms the
 * fixture's raw key material, run through the REAL quirk, reproduces the
 * REAL independently-recovered keys exactly.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "../pairfact_fixture.h"

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		printf("  ok    %-55s 0x%x\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=0x%x want=0x%x\n", label, got, want);
	g_fail++;
}

static unsigned char *read_file(const char *path, unsigned int *outLen)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return nullptr;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char *buf = (unsigned char *)malloc((size_t)sz);
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		fclose(f);
		free(buf);
		return nullptr;
	}
	fclose(f);
	*outLen = (unsigned int)sz;
	return buf;
}

static bool check_volume(const char *name, const unsigned char *raw16,
			  const char *want31Chars)
{
	unsigned char buf32[32];
	hexencode_31char(raw16, buf32);

	bool charsOk = memcmp(buf32, want31Chars, 31) == 0;
	bool nullOk  = buf32[31] == 0x00;

	char label[80];
	snprintf(label, sizeof label, "%s: hex-encoded key == real confirmed key", name);
	check_eq(label, (unsigned int)(charsOk && nullOk), 1);
	return charsOk && nullOk;
}

int main(void)
{
	printf("AT88VirtualChip pairfact_fixture known-answer test\n");
	printf("=====================================================\n");

	printf("[1] Load the REAL captured /.pairFact3 blob\n");
	unsigned int blobLen = 0;
	unsigned char *blob = read_file(
		"/home/share/kronosology/docs/crypto/pairFact3.bin", &blobLen);
	if (!blob) {
		printf("  FAIL  could not read pairFact3.bin -- is the path still valid?\n");
		return 1;
	}
	check_eq("captured blob length == 80", blobLen, PAIRFACT_BLOB_LEN);

	printf("[2] pairfact_fixture_lookup recognizes the real blob\n");
	unsigned char keys48[48];
	int rc = pairfact_fixture_lookup(blob, blobLen, keys48);
	check_eq("lookup succeeds (rc==0)", (unsigned int)(rc == 0), 1);
	free(blob);

	printf("[3] Each volume's raw key, hex-encoded with the REAL quirky\n"
	       "    truncation, reproduces the REAL independently-recovered key\n"
	       "    (docs/crypto/cryptoloop_keys.md, recovered via LOOP_GET_STATUS64)\n");
	check_volume("Mod",        keys48,      "a336a15cd841ec8926b99e7c3884eaa");
	check_volume("Eva",        keys48 + 16, "342ee59d549c7d329d835537be0540d");
	check_volume("WaveMotion", keys48 + 32, "3e72c0e59fc017a9eb7d7e1168a4cdb");

	printf("[4] An unrecognized blob is rejected, not silently accepted\n");
	unsigned char wrongBlob[PAIRFACT_BLOB_LEN];
	memset(wrongBlob, 0x42, sizeof wrongBlob);
	rc = pairfact_fixture_lookup(wrongBlob, PAIRFACT_BLOB_LEN, keys48);
	check_eq("unrecognized blob returns nonzero", (unsigned int)(rc != 0), 1);

	printf("[5] A wrong-length blob is rejected\n");
	rc = pairfact_fixture_lookup(wrongBlob, 79, keys48);
	check_eq("wrong-length blob returns nonzero", (unsigned int)(rc != 0), 1);

	printf("[6] Full 16-byte keys (not just the 31-char truncated form) match\n"
	       "    what two real .reauth files independently decrypt to -- see\n"
	       "    cryptoloop_keys.md's 2026-07-16 \".reauth\" section. This is the\n"
	       "    check that caught the previous 16th-byte guess (0xa0/0xd0/0xb0)\n"
	       "    being wrong (real: 0xa7/0xd2/0xbe).\n");
	blob = read_file("/home/share/kronosology/docs/crypto/pairFact3.bin", &blobLen);
	pairfact_fixture_lookup(blob, blobLen, keys48);
	free(blob);
	static const unsigned char realMod[16] = {
		0xa3,0x36,0xa1,0x5c,0xd8,0x41,0xec,0x89,0x26,0xb9,0x9e,0x7c,0x38,0x84,0xea,0xa7
	};
	static const unsigned char realEva[16] = {
		0x34,0x2e,0xe5,0x9d,0x54,0x9c,0x7d,0x32,0x9d,0x83,0x55,0x37,0xbe,0x05,0x40,0xd2
	};
	static const unsigned char realWaveMotion[16] = {
		0x3e,0x72,0xc0,0xe5,0x9f,0xc0,0x17,0xa9,0xeb,0x7d,0x7e,0x11,0x68,0xa4,0xcd,0xbe
	};
	check_eq("Mod full 16 bytes == real .reauth-decrypted key",
		 (unsigned int)(memcmp(keys48, realMod, 16) == 0), 1);
	check_eq("Eva full 16 bytes == real .reauth-decrypted key",
		 (unsigned int)(memcmp(keys48 + 16, realEva, 16) == 0), 1);
	check_eq("WaveMotion full 16 bytes == real .reauth-decrypted key",
		 (unsigned int)(memcmp(keys48 + 32, realWaveMotion, 16) == 0), 1);

	printf("=====================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
