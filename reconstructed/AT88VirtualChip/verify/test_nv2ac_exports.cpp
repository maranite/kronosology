// SPDX-License-Identifier: GPL-2.0
/*
 * test_nv2ac_exports.cpp  -  the top-level integration test: drives the
 * module ONLY through its two real exported symbols
 * (stgNV2AC_sync_cmd / stgNV2AC_sync_read_cmd), the same interface
 * OA.ko/loadmod.ko actually link against, exercising the full real wire
 * sequence confirmed across this project: $B4 zone select, two $B8
 * rounds, then $B6 config reads and $B2 authenticated Zone0 reads --
 * all against the real captured KronosExtract.bin data.
 *
 * Same honesty note as test_b8_handshake.cpp applies to the $B8 rounds
 * here: the challenge/response values are self-generated using the same
 * algorithm the exported symbols verify against (no independent captured
 * Nc/Q exchange exists to test against instead).
 *
 * This test calls the READ side through `nv2ac_read_cmd_impl()` (pointer-
 * typed) rather than `stgNV2AC_sync_read_cmd()` (the real `int cmd4, int
 * dest` ABI signature) directly -- see nv2ac_exports.cpp's comment on
 * why: that signature's `int`-sized pointer packing is lossless on the
 * real 32-bit target but corrupts real pointers on the 64-bit host this
 * test actually runs on. `stgNV2AC_sync_cmd()` (the WRITE side) already
 * takes a real pointer and is exercised directly, unmodified.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "../at88_chip.h"
#include "../bignum.h"

extern "C" int  at88_chip_module_init(const unsigned char *blob, unsigned int blobLen);
extern "C" int  stgNV2AC_sync_cmd(unsigned char *address, unsigned int data);
extern int nv2ac_read_cmd_impl(const unsigned char *cmd, unsigned char *out);

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

int main(void)
{
	printf("AT88VirtualChip nv2ac_exports integration test\n");
	printf("================================================\n");

	printf("[1] Module init from the REAL captured KronosExtract.bin\n");
	unsigned int blobLen = 0;
	unsigned char *blob = read_file(
		"/home/share/KronosExtract/build/KronosExtract.bin", &blobLen);
	if (!blob) {
		printf("  FAIL  could not read KronosExtract.bin\n");
		return 1;
	}
	int rc = at88_chip_module_init(blob, blobLen);
	check_eq("at88_chip_module_init succeeds", (unsigned int)(rc == 0), 1);
	free(blob);

	printf("[2] $B4 zone select (zone 0) via stgNV2AC_sync_cmd\n");
	unsigned char zoneSel[4] = {0xb4, 0x03, 0x00, 0x00};
	stgNV2AC_sync_cmd(zoneSel, 4);
	/* No direct observable effect (see nv2ac_exports.cpp header) -- this
	 * just confirms the call doesn't crash / mishandle the opcode. */
	check_eq("zone select call completes", 1, 1);

	printf("[3] Read IdN via $B6 (stgNV2AC_sync_read_cmd) -- must match the\n"
	       "    real captured cfg[0x19..0x1f]\n");
	unsigned char idnCmd[4] = {0xb6, 0x00, 0x19, 7};
	unsigned char idn[7];
	rc = nv2ac_read_cmd_impl(idnCmd, idn);
	check_eq("$B6 IdN read rc==0", (unsigned int)(rc == 0), 1);
	static const unsigned char wantIdn[7] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	check_eq("IdN == [REDACTED-PUBLIC-ID]", (unsigned int)(memcmp(idn, wantIdn, 7) == 0), 1);

	printf("[4] Round 1: a self-consistent $B8 challenge (zone 0x00) is accepted\n"
	       "    (observed only indirectly, via the AAC byte -- $B8 is a\n"
	       "    stgNV2AC_sync_cmd \"write\", it has no direct return value)\n");
	unsigned char aacCmd[4] = {0xb6, 0x00, 0x50, 1};
	unsigned char aacBefore;
	nv2ac_read_cmd_impl(aacCmd, &aacBefore);

	unsigned char hostP2[8], hostP3[8] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	synth_sdflkjsvnd2g(wantIdn, hostP2);
	static const unsigned char nc1[8] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
	unsigned char q1[8], p5_1[8];
	DeaxState hostSession;
	deax_init(&hostSession);
	deax_compute_challenges(&hostSession, nc1, hostP2, hostP3, q1, p5_1);

	unsigned char b8cmd1[20];
	b8cmd1[0] = 0xb8; b8cmd1[1] = 0x00; b8cmd1[2] = 0x00; b8cmd1[3] = 0x10;
	memcpy(b8cmd1 + 4, nc1, 8);
	memcpy(b8cmd1 + 12, q1, 8);
	stgNV2AC_sync_cmd(b8cmd1, 20);

	unsigned char aacAfterRound1;
	nv2ac_read_cmd_impl(aacCmd, &aacAfterRound1);
	check_eq("AAC did not decrease after round 1 (accepted)",
		 (unsigned int)(aacAfterRound1 >= aacBefore), 1);

	printf("[5] Round 2: chained $B8 challenge (zone 0x10) is accepted\n");
	static const unsigned char nc2[8] = {0xaa,0xbb,0xcc,0xdd,0xee,0xff,0x01,0x02};
	unsigned char q2[8], p5_2[8];
	deax_compute_challenges(&hostSession, nc2, p5_1, hostP3, q2, p5_2);

	unsigned char b8cmd2[20];
	b8cmd2[0] = 0xb8; b8cmd2[1] = 0x10; b8cmd2[2] = 0x00; b8cmd2[3] = 0x10;
	memcpy(b8cmd2 + 4, nc2, 8);
	memcpy(b8cmd2 + 12, q2, 8);
	stgNV2AC_sync_cmd(b8cmd2, 20);

	unsigned char aacAfterRound2;
	nv2ac_read_cmd_impl(aacCmd, &aacAfterRound2);
	check_eq("AAC did not decrease after round 2 (accepted)",
		 (unsigned int)(aacAfterRound2 >= aacAfterRound1), 1);

	printf("[6] Post-handshake $B2 Zone0 read matches the REAL captured secret\n");
	unsigned char z0Cmd[4] = {0xb2, 0x00, 0x00, 8};
	unsigned char z0cipher[8];
	rc = nv2ac_read_cmd_impl(z0Cmd, z0cipher);
	check_eq("$B2 zone0 read rc==0", (unsigned int)(rc == 0), 1);

	/* Host-side mirrored decrypt: same post-b8 continuation (once, after
	 * round 2) + the same 12 pre-steps + per-byte decrypt as
	 * test_b8_handshake.cpp's test [5]. */
	DeaxState hostZone0State = hostSession;
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0x50);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 1);
	deax_step(&hostZone0State, aacAfterRound2);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);	/* addr=0 */
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 0);
	deax_step(&hostZone0State, 8);	/* len=8 */
	unsigned char recovered[8];
	for (int i = 0; i < 8; i++) {
		recovered[i] = (unsigned char)(z0cipher[i] ^ hostZone0State.gpa);
		deax_step(&hostZone0State, recovered[i]);
		deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
		deax_step(&hostZone0State, 0); deax_step(&hostZone0State, 0);
		deax_step(&hostZone0State, 0);
	}
	static const unsigned char wantZone0First8[8] = {
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	};
	check_eq("recovered Zone0[0..7] matches the real captured secret",
		 (unsigned int)(memcmp(recovered, wantZone0First8, 8) == 0), 1);

	printf("[7] Unrecognized opcodes are rejected / no-op rather than crashing\n");
	unsigned char badCmd[4] = {0xff, 0, 0, 0};
	unsigned char scratch[8];
	rc = nv2ac_read_cmd_impl(badCmd, scratch);
	check_eq("unknown read opcode returns nonzero", (unsigned int)(rc != 0), 1);
	stgNV2AC_sync_cmd(badCmd, 4);	/* must not crash */
	check_eq("unknown write opcode is a safe no-op", 1, 1);

	printf("================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
