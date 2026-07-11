// SPDX-License-Identifier: GPL-2.0
/*
 * nv2ac_handshake.cpp  -  cm_SetUserZone()/nv2ac_enable_cipher()/
 * nv2ac_enable_encrypt() (batch 55), closing (together with
 * nv2ac_dispatch_cmd, added this same batch to atmel_zone_io.cpp, and
 * cm_ComputeChallenge, atmel_challenge.cpp) the sec 10.206-flagged AT88
 * GPA-handshake wrapper cluster that was hard-blocking
 * SetupAtmelForAuthorizations() -- and therefore init_module() step 9 --
 * at a bare `return -1;` in bar2_stubs_c.cpp.
 *
 * cm_SetUserZone (fFfFfFfFfFfF1A, .text+0x4f4a00, 114 bytes): rejects
 * zone>3 (unsigned byte compare) with -1, else sends AT88 command
 * {0xb4, 0x03, zone, 0x00} via stgNV2AC_sync_cmd(cmd,4) -- matches
 * CLAUDE.md's own documented AT88SC protocol ("Zone selection required
 * before user zone reads: send {0xb4, 0x03, zone, 0x00}") -- then on
 * driver success feeds the zone byte into the DEAX cipher stream (a
 * single bzzzzzzzzzzzt12 step, NOT the 12-step zero-padded preamble
 * atmel_zone_io.cpp's zone-READ wrappers use). Returns -2 on driver
 * failure, 0 on success.
 *
 * nv2ac_enable_cipher/nv2ac_enable_encrypt (fFfFfFfFfFfF1G/1H,
 * .text+0x4f4ce0/0x4f4d10, 46/54 bytes) both tail-call a shared internal
 * helper (ground truth `bzzzzzzzzzzzt16`, .text+0x4f3fd0, 286 bytes,
 * reconstructed here as Nv2acVerifyRound) with a different hardcoded
 * "authZoneSel" nibble baked in by each caller (0x00 for cipher, 0x10
 * for encrypt -- matches oa_atmel.h's own pre-existing comment
 * "enable_cipher uses chip auth zone 0x00 ... enable_encrypt uses zone
 * 0x10"). Nv2acVerifyRound, confirmed via full disassembly:
 *   1. rejects sel>3 (unsigned byte compare) -> -1.
 *   2. builds a 20-byte AT88 "$B8 Verify Crypto" command:
 *      {0xb8, authZoneSel|sel, 0x00, 0x10, challenge[0..7], c2[0..7]}
 *      and issues it via stgNV2AC_sync_cmd(cmd,20); returns -1 on driver
 *      failure.
 *   3. reads back ONE status byte from AT88 zone
 *      {0x50,0x60,0x70,0x80}[sel] (a real .rodata byte lookup table,
 *      ground truth .rodata+0xb08a0, confirmed via a direct hex dump)
 *      via the same "read exactly 1 byte, zone<=0xaf never decodes"
 *      shape cm_ReadUserZone already implements as its own "loop A"
 *      (ground truth's `fFfFfFfFfFfF1C.clone.0` -- confirmed byte-for-
 *      byte identical logic to cm_ReadUserZone(zone,1,&byte) for every
 *      one of these four real zone-table entries, all <=0xaf, so reused
 *      directly rather than re-implementing the clone -- see
 *      atmel_zone_io.cpp's own header comment for the "loop A" vs
 *      "loop B" derivation). Returns -1 on driver failure.
 *   4. returns 0 if the status byte == 0xff (the real "verified OK"
 *      chip sentinel), 1 otherwise (verification not yet complete /
 *      failed) -- confirmed via ground truth's own `cmp $0xff`/`setne`
 *      sequence, NOT the more intuitive "1 == success" polarity.
 *
 * nv2ac_enable_cipher/encrypt each latch the persistent `mode` global
 * (oa_atmel.h, defined in atmel_zone_io.cpp) from their own return
 * value:
 *   nv2ac_enable_cipher:  mode = (rc == 0) ? 1 : 0   (`sete` sequence)
 *   nv2ac_enable_encrypt: mode = (rc == 0) ? 2 : 0   (`cmp $1`+`sbb`+`and $2`)
 * This is the real setter atmel_zone_io.cpp's own header comment
 * predicted but left unreconstructed ("almost certainly nv2ac_enable_
 * cipher/nv2ac_enable_encrypt") -- confirmed here. `mode` only reaches 2
 * (the decode-enabling value fFfFfFfFfFfF13/cm_ReadUserZone's own "loop
 * B" checks for) once BOTH rounds of SetupAtmelForAuthorizations()'s
 * handshake have succeeded in sequence -- matches CLAUDE.md's own
 * documented requirement that AT88 Zone0[0x10..0x27] decode needs the
 * full two-round handshake completed first.
 */

#include "oa_atmel.h"

extern "C" int  stgNV2AC_sync_cmd(unsigned char *cmd, int param)
	__attribute__((regparm(3)));
extern "C" void msleep(unsigned int msecs);

int cm_SetUserZone(int zone)
{
	if ((unsigned int)(unsigned char)zone > 3)
		return -1;

	unsigned char cmd[4];
	cmd[0] = 0xb4;
	cmd[1] = 0x03;
	cmd[2] = (unsigned char)zone;
	cmd[3] = 0x00;

	int rc = stgNV2AC_sync_cmd(cmd, 4);
	msleep(20);
	if (rc != 0)
		return -2;

	bzzzzzzzzzzzt12((unsigned char)zone);
	return 0;
}

/* Ground truth .rodata+0xb08a0, confirmed via a direct hex dump: the
 * four real AT88 zones read back to confirm each of the (at most four,
 * sel 0..3) $B8 verify-crypto rounds. */
static const unsigned char kNv2acStatusZone[4] = { 0x50, 0x60, 0x70, 0x80 };

static int Nv2acVerifyRound(unsigned int authZoneSel, unsigned char sel,
			     const unsigned char *challenge, const unsigned char *c2)
{
	if (sel > 3)
		return -1;

	unsigned char cmd[20];
	cmd[0] = 0xb8;
	cmd[1] = (unsigned char)(authZoneSel | sel);
	cmd[2] = 0x00;
	cmd[3] = 0x10;
	for (int i = 0; i < 8; i++)
		cmd[4 + i] = challenge[i];
	for (int i = 0; i < 8; i++)
		cmd[12 + i] = c2[i];

	int rc = stgNV2AC_sync_cmd(cmd, 20);
	msleep(20);
	if (rc != 0)
		return -1;

	unsigned char status;
	if (cm_ReadUserZone(kNv2acStatusZone[sel], 1, &status) != 0)
		return -1;

	return (status != 0xff) ? 1 : 0;
}

int nv2ac_enable_cipher(unsigned char sel, const unsigned char *challenge, const unsigned char *c2)
{
	int rc = Nv2acVerifyRound(0x00, sel, challenge, c2);
	mode = (rc == 0) ? 1 : 0;
	return rc;
}

int nv2ac_enable_encrypt(unsigned char sel, const unsigned char *challenge, const unsigned char *c2)
{
	int rc = Nv2acVerifyRound(0x10, sel, challenge, c2);
	mode = (rc == 0) ? 2 : 0;
	return rc;
}
