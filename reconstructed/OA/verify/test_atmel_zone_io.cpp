// SPDX-License-Identifier: GPL-2.0
/*
 * test_atmel_zone_io.cpp  -  host-side known-answer test for
 * fFfFfFfFfFfF13()/cm_ReadUserZone() (src/auth/atmel_zone_io.cpp, batch
 * 46).
 *
 * Links src/auth/atmel_zone_io.cpp + src/auth/atmel_deax.cpp directly.
 * Provides host-side mocks for the three genuine external dependencies
 * (stgNV2AC_sync_read_cmd/stgNV2AC_sync_cmd/msleep -- real AT88/NV2AC
 * driver + kernel primitives, legitimately left undefined by this
 * freestanding reconstruction, per the sec 10.185 RTAI-substitution
 * policy).
 *
 * No real hardware-captured vectors exist for these two functions
 * specifically (unlike cm_AuthenEncryptMAC's own test_atmel_deax.cpp) --
 * this is OA.ko's own internal command-dispatch/decode wrapper, not
 * something previously captured off a real chip. Verification here is
 * therefore: (a) deterministic checks not depending on cipher internals
 * (bounds check, mode==0 passthrough, mode!=0/!=2 pure-tracking leaves
 * data untouched but visibly advances the cipher state), and (b) a
 * genuine round-trip test built from an INDEPENDENT reference "encode"
 * routine (ReferenceDeaxEncode below) that uses only the same low-level
 * primitives (bzzzzzzzzzzzt12/DeaxCurrentGpa) the real functions call --
 * not a copy of either function's own per-byte loop -- so a real
 * translation bug in the loop shape would still be caught by the
 * round-trip failing to recover the original plaintext.
 */

#include <cstdio>
#include <cstring>

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-55s %ld\n", label, got); return; }
	printf("  FAILED: %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static void check_bytes(const char *label, const unsigned char *got,
			 const unsigned char *want, int n)
{
	if (memcmp(got, want, n) == 0) {
		printf("  ok    %-55s (%d bytes match)\n", label, n);
		return;
	}
	printf("  FAILED: %-55s\n    got : ", label);
	for (int i = 0; i < n; i++) printf("%02x ", got[i]);
	printf("\n    want: ");
	for (int i = 0; i < n; i++) printf("%02x ", want[i]);
	printf("\n");
	g_fail++;
}

/* ---- mocks for the genuine external dependencies ---- */
static int g_readCalls, g_syncCalls;
static int g_forceReadFail, g_forceSyncFail;
static unsigned char g_fakeChip[32];
static unsigned char g_lastCmd[4];

extern "C" int stgNV2AC_sync_read_cmd(unsigned char *cmd, unsigned char *out, int unused)
{
	(void)unused;
	g_readCalls++;
	memcpy(g_lastCmd, cmd, 4);
	if (g_forceReadFail)
		return -1;
	memcpy(out, g_fakeChip, 32);
	return 0;
}

extern "C" int stgNV2AC_sync_cmd(unsigned char *cmd, int param)
{
	(void)param;
	g_syncCalls++;
	memcpy(g_lastCmd, cmd, 4);
	if (g_forceSyncFail)
		return -1;
	return 0;
}

extern "C" void msleep(unsigned int) { }

/* Declared in oa_atmel.h; defined in atmel_zone_io.cpp. */
extern "C" unsigned char mode;
extern "C" void bzzzzzzzzzzzt11(void);
extern "C" void bzzzzzzzzzzzt12(unsigned char in);
extern "C" unsigned char DeaxCurrentGpa(void);

extern "C" int fFfFfFfFfFfF13(unsigned int zone, unsigned int len, unsigned char *buf);
int cm_ReadUserZone(int zone, int len, unsigned char *buf);

/*
 * Independent reference "encode" -- the functional inverse of the
 * mode==2 decode loop, built ONLY from the shared low-level primitives
 * (never copies fFfFfFfFfFfF13/cm_ReadUserZone's own per-byte loop):
 * ciphertext[i] = plaintext[i] ^ gpa(before), then feed plaintext[i]
 * (not ciphertext[i]) into the cipher to advance to the next gpa --
 * exactly the encode side of a self-synchronizing/plaintext-feedback
 * stream cipher, matching the decode side's own confirmed "decode using
 * current gpa, then step with the DECODED value" semantics.
 */
static void ReferenceDeaxEncode(unsigned char *data, unsigned int n)
{
	for (unsigned int i = 0; i < n; i++) {
		unsigned char plain = data[i];
		data[i] = (unsigned char)(plain ^ DeaxCurrentGpa());
		bzzzzzzzzzzzt12(plain);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
	}
}

int main()
{
	printf("[1] fFfFfFfFfFfF13 -- bounds check (zone+len > 0x40)\n");
	{
		unsigned char buf[8] = {0};
		g_readCalls = g_syncCalls = 0;
		int rc = fFfFfFfFfFfF13(0x30, 0x20, buf); /* 0x50 > 0x40 */
		check_eq("  return -1", rc, -1);
		check_eq("  no hardware calls made", g_readCalls + g_syncCalls, 0);
	}

	printf("[2] fFfFfFfFfFfF13 -- mode==0 passthrough (no decode)\n");
	{
		mode = 0;
		for (int i = 0; i < 32; i++) g_fakeChip[i] = (unsigned char)(0xa0 + i);
		unsigned char before = DeaxCurrentGpa();
		unsigned char buf[8];
		g_readCalls = g_syncCalls = 0;
		int rc = fFfFfFfFfFfF13(0x10, 8, buf);
		check_eq("  return 0", rc, 0);
		check_eq("  exactly one hardware read", g_readCalls, 1);
		check_eq("  no sync_cmd needed", g_syncCalls, 0);
		check_bytes("  buf == raw chip data (unchanged)", buf, g_fakeChip, 8);
		check_eq("  cipher state untouched (mode==0 never steps)",
			 DeaxCurrentGpa(), before);
		check_eq("  cmd opcode was 0xb2", g_lastCmd[0], 0xb2);
		check_eq("  cmd zone byte", g_lastCmd[2], 0x10);
		check_eq("  cmd len byte", g_lastCmd[3], 8);
	}

	printf("[3] fFfFfFfFfFfF13 -- mode==1 (\"other\"): raw pass-through data,"
	       " but cipher state visibly advances\n");
	{
		mode = 1;
		for (int i = 0; i < 32; i++) g_fakeChip[i] = (unsigned char)(0x55 + i);
		unsigned char before = DeaxCurrentGpa();
		unsigned char buf[6];
		int rc = fFfFfFfFfFfF13(0x18, 6, buf);
		check_eq("  return 0", rc, 0);
		check_bytes("  buf == raw chip data (not decoded, mode!=2)", buf, g_fakeChip, 6);
		/* gpa is 8-bit and step is nonlinear; just confirm SOMETHING moved
		 * (a real, if coarse, signal that cipher stepping happened -- an
		 * unchanged gpa here would mean the step calls were silently
		 * skipped, a real translation bug). */
		int changed = (DeaxCurrentGpa() != before);
		check_eq("  cipher state advanced", changed, 1);
	}

	printf("[4] fFfFfFfFfFfF13 -- mode==0 forced read failure\n");
	{
		mode = 0;
		g_forceReadFail = 1;
		unsigned char buf[8];
		int rc = fFfFfFfFfFfF13(0x20, 8, buf);
		check_eq("  return -1 on driver failure", rc, -1);
		g_forceReadFail = 0;
	}

	printf("[5] fFfFfFfFfFfF13 -- mode==0, len==0 is a no-op success\n");
	{
		mode = 0;
		unsigned char buf[1] = { 0xcc };
		int rc = fFfFfFfFfFfF13(0x10, 0, buf);
		check_eq("  return 0", rc, 0);
		check_eq("  buf untouched", buf[0], 0xcc);
	}

	printf("[6] fFfFfFfFfFfF13 -- mode==2 round trip via independent"
	       " reference encode\n");
	{
		/* Reset to the cipher's own real zero state (bzzzzzzzzzzzt11,
		 * ground truth's own init primitive), then drive it to a fixed,
		 * reproducible position -- not claiming this matches any real
		 * handshake state, just needs to be REPRODUCIBLE so the encode
		 * and decode sides start from the exact same place (replaying a
		 * step sequence from a state that already diverged does NOT
		 * "rewind" it back -- only a true reset does). */
		const unsigned char zoneB = 0x20, lenB = 8;
		auto resetToS0 = [](void) {
			bzzzzzzzzzzzt11();
			for (int i = 0; i < 20; i++)
				bzzzzzzzzzzzt12((unsigned char)i);
		};

		resetToS0();
		unsigned char gpaAtS0 = DeaxCurrentGpa();
		/* fFfFfFfFfFfF13 ITSELF runs a 12-step (zone,len) preamble
		 * whenever mode!=0 (which mode==2 is) -- BEFORE its per-byte
		 * decode loop. Simulate that same preamble here, from S0, so the
		 * reference encode starts from the exact state the real
		 * function's OWN decode loop will see after ITS OWN (not
		 * replayed twice) preamble runs. */
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(zoneB);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(lenB);

		unsigned char plaintext[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef };
		unsigned char cipherText[8];
		memcpy(cipherText, plaintext, 8);
		ReferenceDeaxEncode(cipherText, 8);
		check_eq("  reference encode actually changed the bytes",
			 memcmp(cipherText, plaintext, 8) != 0, 1);

		/* Reset back to S0 -- do NOT replay the preamble again here: the
		 * upcoming real call itself will apply that exact preamble
		 * (mode!=0), so replaying it manually a second time would
		 * desynchronize the two sides (this was a real bug in an
		 * earlier draft of this test, caught by this exact check). */
		resetToS0();
		check_eq("  cipher reset to S0", DeaxCurrentGpa(), gpaAtS0);

		/* The real function overwrites `buf` with the (mocked) hardware
		 * read response BEFORE its decode loop ever runs -- so the
		 * reference ciphertext must be staged as the MOCK's chip data,
		 * not written into `buf` directly (buf's incoming content is
		 * irrelevant, matching ground truth's own "buf is purely an
		 * output parameter" contract). */
		memcpy(g_fakeChip, cipherText, 8);
		mode = 2;
		unsigned char buf[8];
		int rc = fFfFfFfFfFfF13(zoneB, lenB, buf);
		check_eq("  return 0", rc, 0);
		check_bytes("  decoded buf == original plaintext", buf, plaintext, 8);
	}

	printf("[7] cm_ReadUserZone (fFfFfFfFfFfF1C) -- p1<=0xaf (\"loop A\"):"
	       " never decodes even with mode==2\n");
	{
		mode = 2; /* deliberately -- loop A must ignore this */
		for (int i = 0; i < 32; i++) g_fakeChip[i] = (unsigned char)(0x70 + i);
		unsigned char before = DeaxCurrentGpa();
		unsigned char buf[7];
		int rc = cm_ReadUserZone(0x19, 7, buf); /* real call site: config zone */
		check_eq("  return 0", rc, 0);
		check_bytes("  buf == raw chip data (loop A never decodes)", buf, g_fakeChip, 7);
		int changed = (DeaxCurrentGpa() != before);
		check_eq("  cipher state still advances (tracking only)", changed, 1);
		check_eq("  cmd opcode was 0xb6", g_lastCmd[0], 0xb6);
	}

	printf("[8] cm_ReadUserZone -- p1>0xaf (\"loop B\") round trip via"
	       " independent reference encode\n");
	{
		const unsigned char p1 = 0xb0; /* > 0xaf -- takes loop B */
		const unsigned char lenb = 5;
		unsigned char plaintext[5] = { 0x11, 0x22, 0x33, 0x44, 0x55 };

		/* A fixed, reproducible "base" cipher position -- reset to the
		 * cipher's own real zero state first (bzzzzzzzzzzzt11), THEN
		 * replay a fixed step sequence, so this is genuinely
		 * reproducible (replaying steps from an already-diverged state
		 * would NOT reach the same position -- only a true reset does). */
		auto rewindToBase = [](void) {
			bzzzzzzzzzzzt11();
			for (int i = 0; i < 20; i++)
				bzzzzzzzzzzzt12((unsigned char)(i * 3));
		};

		rewindToBase();
		/* cm_ReadUserZone's OWN 12-step preamble (5 zero-steps, step(p1),
		 * 5 zero-steps, step(len)) is deterministic given the starting
		 * state -- replay it manually here so the reference encode below
		 * starts from the EXACT state cm_ReadUserZone's own decode loop
		 * will see after ITS internal preamble runs. */
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(p1);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(lenb);

		unsigned char cipherText[5];
		memcpy(cipherText, plaintext, 5);
		ReferenceDeaxEncode(cipherText, 5);
		check_eq("  reference encode actually changed the bytes",
			 memcmp(cipherText, plaintext, 5) != 0, 1);

		/* Rewind to the SAME base position before the real call --
		 * cm_ReadUserZone runs its own preamble internally, landing on
		 * the identical state ReferenceDeaxEncode just used above. */
		rewindToBase();

		/* Same "buf gets overwritten by the mocked hardware response
		 * before decode runs" contract as test [6] -- stage the
		 * ciphertext as the mock's chip data, not in `buf` itself. */
		memcpy(g_fakeChip, cipherText, 5);
		mode = 2;
		unsigned char buf[5];
		int rc = cm_ReadUserZone(p1, lenb, buf);
		check_eq("  return 0", rc, 0);
		check_bytes("  decoded buf == original plaintext", buf, plaintext, 5);
	}

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
	return g_fail ? 1 : 0;
}
