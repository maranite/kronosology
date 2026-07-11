// SPDX-License-Identifier: GPL-2.0
/*
 * atmel_zone_io.cpp  -  fFfFfFfFfFfF13()/cm_ReadUserZone() (batch 46):
 * OA.ko's own internal AT88 "issue a raw chip command, retry until the
 * chip is ready, then optionally DEAX-decode the response in place"
 * wrappers -- the two siblings sec 10.194 (batch 43, src/auth/
 * atmel_deax.cpp) found reading the same persistent `gpa` keystream byte
 * `cm_AuthenEncryptMAC` writes, and left deliberately deferred at the
 * time.
 *
 * Both are genuinely DEFINED inside OA.ko (`T` symbols in ground truth,
 * not the hardware primitives themselves) -- they each call OUT to the
 * real AT88/NV2AC driver's exported functions (`stgNV2AC_sync_read_cmd`/
 * `stgNV2AC_sync_cmd`, both confirmed genuinely `U` in ground truth's own
 * `nm -u` too) and to `msleep` (standard kernel API). Per the sec 10.185
 * RTAI-substitution policy, these three remain legitimate externals here
 * -- this file reconstructs OA.ko's own dispatch/retry/decode LOGIC
 * around them, not the driver itself.
 *
 * fFfFfFfFfFfF13(zone, len, buf) (.text+0x4f4840, 448 bytes) -- called
 * directly (no friendly alias, matching this project's own existing
 * convention, oa_crypto.h) by ParseAuths/VerifyAuthorizationString to
 * read the three 8-byte auth-key-material zones (0x10/0x18/0x20).
 *
 * fFfFfFfFfFfF1C(p1, len, buf) (.text+0x4f4a80, ~450 bytes, plus an
 * unrelated `.clone.0`/`bzzzzzzzzzzzt15` wrapper NOT needed here -- no
 * call from either of these two functions reaches it) -- aliased
 * `cm_ReadUserZone` in this project's own ecosystem (oa_atmel.h):
 * `SetupAtmelForAuthorizations`'s own two real call sites (zone 0x19/len
 * 7 for the chip config zone, zone 0x50/len 8 for the IV) match
 * fFfFfFfFfFfF1C's own confirmed relocations from ground truth's
 * `SetupAtmelForAuthorizations` (.text+0x207a50) exactly (same EAX=zone/
 * EDX=len register values at both call sites, confirmed via `objdump
 * -dr`).
 *
 * Both functions share essentially the same 4-part shape (confirmed via
 * full, independent disassembly of both):
 *
 *   1. Build a 4-byte raw AT88 command `cmd = {opcode, 0, p1, len}` --
 *      opcode is 0xb2 for fFfFfFfFfFfF13, 0xb6 for fFfFfFfFfFfF1C (both
 *      confirmed real immediates). fFfFfFfFfFfF13 additionally bounds-
 *      checks `zone+len <= 0x40` first (both operands zero-extended
 *      bytes, NOT truncated to a byte before the compare) -- return -1
 *      if not. fFfFfFfFfFfF1C has no such check in ground truth (a real,
 *      faithfully-preserved gap -- every actual call site passes small,
 *      safe constants, so this is never exercised in practice).
 *
 *   2. A 12-step DEAX keystream advance using (p1, len) as key material
 *      -- 5 zero-steps, one step with p1, 5 more zero-steps, one step
 *      with len (`bzzzzzzzzzzzt12`, atmel_deax.cpp, operating on the
 *      SAME persistent cipher state `cm_AuthenEncryptMAC` uses).
 *      fFfFfFfFfFfF1C always does this unconditionally; fFfFfFfFfFfF13
 *      only does it when the persistent `mode` global (confirmed real,
 *      non-obfuscated ground-truth name, .bss 0x5c90c0, immediately
 *      adjacent to `gpa_byte`@0x5c90c1) is nonzero -- when `mode==0` it
 *      instead issues the hardware read immediately (see next step).
 *
 *   3. A retry loop: inspect `cmd[0] & 0xf` (the driver call is
 *      confirmed to overwrite `cmd[0]` with a real status/response
 *      code) -- if it's 2 or 6, (re)issue `stgNV2AC_sync_read_cmd(cmd,
 *      scratch, 0)`; otherwise issue `stgNV2AC_sync_cmd(cmd, cmd[3]+4)`.
 *      Either way, `msleep(20)`, then return -1 on a nonzero driver
 *      return, or proceed to step 4 on success. (For fFfFfFfFfFfF13's
 *      `mode==0` path, this loop's first read already happened in step
 *      2's place -- see the code below for the exact shared-tail shape,
 *      transcribed as a genuine loop rather than assumed-unreachable,
 *      even though `cmd[0]` is provably still its just-initialized
 *      opcode constant -- nibble 2 for 0xb2, nibble 6 for 0xb6 -- the
 *      FIRST time this loop is reached in every real call, making the
 *      "cmd[3]+4 / sync_cmd" branch dead in practice for both functions
 *      as currently called; kept as a real loop anyway for fidelity and
 *      to not have to prove that dead-code claim watertight.)
 *
 *   4. Copy `len` bytes from the shared 32-byte scratch response buffer
 *      (`bzzzzzzzzzt18`, ground truth `.bss+0x5c90e0`, confirmed real
 *      non-obfuscated name and confirmed real size via `nm`) into `buf`
 *      -- `bzzzzzzzzzt17` in ground truth (.text+0x4f4140, a dword/word/
 *      byte-decomposed `rep movs`, functionally a plain byte copy;
 *      fFfFfFfFfFfF13 calls it as a real subroutine, fFfFfFfFfFfF1C
 *      inlines the equivalent copy -- transcribed here as one shared
 *      helper either way, matching this project's own "factor a
 *      duplicated-in-ground-truth sequence into one helper" precedent,
 *      e.g. channel_values_reset.cpp's `ApplyDefaultControllerValue`).
 *      Then, per byte, depending on `mode`:
 *        - mode==0: pass through unchanged, no cipher stepping at all.
 *        - mode==2: XOR-decode the byte using the CURRENT `gpa`, THEN
 *          feed the now-decoded byte into the cipher (1 real step + 5
 *          zero-steps).
 *        - anything else: no decode, but still feed the RAW byte into
 *          the cipher (same 1+5 step shape) -- tracks cipher position
 *          without touching the data.
 *
 * fFfFfFfFfFfF1C's own per-byte loop (step 4) has one further real,
 * confirmed-via-disassembly wrinkle: its first parameter `p1` gates
 * whether decode is even POSSIBLE at all for this call -- `p1 <= 0xaf`
 * takes a separate code path (ground truth's own "loop A",
 * .text+0x4f4ba0) that NEVER decodes regardless of `mode` (pure cipher-
 * position tracking only); `p1 > 0xaf` takes ground truth's "loop B"
 * (.text+0x4f4be0), which supports the mode==2 decode described above.
 * Both of `cm_ReadUserZone`'s own two real call sites pass p1 in {0x19,
 * 0x50} (both <= 0xaf), so in practice `cm_ReadUserZone` never decodes --
 * consistent with reading the config/IV zones BEFORE the cipher/encrypt
 * handshake has set `mode`, whereas fFfFfFfFfFfF13 (auth-key-material
 * zones, read AFTER the handshake) is the one that actually exercises
 * mode==2 decode. Transcribed faithfully (the `p1` gate is real, not an
 * invented simplification) even though the currently-known call sites
 * never reach the decode-capable branch.
 */

#include "oa_atmel.h"
#include "oa_crypto.h"

extern "C" int  stgNV2AC_sync_read_cmd(unsigned char *cmd, unsigned char *out, int unused)
	__attribute__((regparm(3)));
extern "C" int  stgNV2AC_sync_cmd(unsigned char *cmd, int param)
	__attribute__((regparm(3)));
extern "C" void msleep(unsigned int msecs);

/*
 * `mode` (ground truth `.bss+0x5c90c0`, confirmed real non-obfuscated
 * name, `B` = global/exported). No setter is reconstructed yet (the
 * real setter is almost certainly `nv2ac_enable_cipher`/
 * `nv2ac_enable_encrypt`, both still stubs in bar2_stubs_c.cpp) --
 * defaults to 0 (uninitialized-.bss-equivalent), matching ground
 * truth's own default and this file's own "pass-through, no decode"
 * behavior for mode==0 until a future batch reconstructs the setter.
 * Declared `extern "C"` in oa_atmel.h (included above); defined here,
 * alongside the two functions that read it, matching this project's own
 * "home a shared global alongside its first real reader/writer"
 * convention.
 */
unsigned char mode;

/* bzzzzzzzzzt18 (ground truth `.bss+0x5c90e0`, confirmed real
 * non-obfuscated name, `b` = file-local -- kept `static` here to match). */
static unsigned char g_atmelZoneScratch[0x20];

/*
 * bzzzzzzzzzt17(buf, len) (.text+0x4f4140, 64 bytes): copies `len` bytes
 * from the shared scratch buffer into `buf`. Ground truth's own body is
 * a dword/word/byte-decomposed `rep movs` sequence (a compiler-generated
 * memcpy expansion) -- functionally identical to a plain byte loop, used
 * here instead to keep this freestanding build's established "no libc"
 * convention (see setup_global_resources.cpp's own `ZeroBytes()`
 * precedent). Always returns 0 in ground truth (there is no failure
 * path in the real disassembly at all).
 */
static int AtmelZoneCopyOut(unsigned char *buf, unsigned int len)
{
	for (unsigned int i = 0; i < len; i++)
		buf[i] = g_atmelZoneScratch[i];
	return 0;
}

extern "C" int fFfFfFfFfFfF13(unsigned int zone, unsigned int len, unsigned char *buf)
{
	unsigned char zoneByte = (unsigned char)zone;
	unsigned char lenByte  = (unsigned char)len;

	if ((unsigned int)zoneByte + (unsigned int)lenByte > 0x40)
		return -1;

	unsigned char cmd[4];
	cmd[0] = 0xb2;
	cmd[1] = 0;
	cmd[2] = zoneByte;
	cmd[3] = lenByte;

	int rc;

	if (mode == 0) {
		rc = stgNV2AC_sync_read_cmd(cmd, g_atmelZoneScratch, 0);
		msleep(20);
		if (rc != 0)
			return -1;
	} else {
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(zoneByte);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(lenByte);

		for (;;) {
			unsigned char status = (unsigned char)(cmd[0] & 0xf);
			if (status == 2 || status == 6) {
				rc = stgNV2AC_sync_read_cmd(cmd, g_atmelZoneScratch, 0);
				msleep(20);
				if (rc != 0)
					return -1;
				break;
			} else {
				int p = (int)cmd[3] + 4;
				rc = stgNV2AC_sync_cmd(cmd, p);
				msleep(20);
				if (rc != 0)
					return -1;
				break;
			}
		}
	}

	if (AtmelZoneCopyOut(buf, lenByte) != 0)
		return -2;
	if (lenByte == 0)
		return 0;

	for (unsigned int i = 0; i < lenByte; i++) {
		unsigned char m = mode;
		if (m == 0)
			continue;
		if (m == 2)
			buf[i] = (unsigned char)(buf[i] ^ DeaxCurrentGpa());
		bzzzzzzzzzzzt12(buf[i]);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
	}
	return 0;
}

int cm_ReadUserZone(int zoneArg, int lenArg, unsigned char *buf)
{
	unsigned char p1Byte  = (unsigned char)zoneArg;
	unsigned char lenByte = (unsigned char)lenArg;

	unsigned char cmd[4];
	cmd[0] = 0xb6;
	cmd[1] = 0;
	cmd[2] = p1Byte;
	cmd[3] = lenByte;

	bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
	bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
	bzzzzzzzzzzzt12(p1Byte);
	bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
	bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
	bzzzzzzzzzzzt12(lenByte);

	int rc;
	for (;;) {
		unsigned char status = (unsigned char)(cmd[0] & 0xf);
		if (status == 2 || status == 6) {
			rc = stgNV2AC_sync_read_cmd(cmd, g_atmelZoneScratch, 0);
			msleep(20);
			if (rc != 0)
				return -1;
			break;
		} else {
			int p = (int)cmd[3] + 4;
			rc = stgNV2AC_sync_cmd(cmd, p);
			msleep(20);
			if (rc != 0)
				return -1;
			break;
		}
	}

	AtmelZoneCopyOut(buf, lenByte);

	if (lenByte == 0)
		return 0;

	if (p1Byte <= 0xaf) {
		/* "loop A" -- pure cipher-position tracking, never decodes. */
		for (unsigned int i = 0; i < lenByte; i++) {
			bzzzzzzzzzzzt12(buf[i]);
			bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
			bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		}
		return 0;
	}

	/* "loop B" -- supports mode==2 decode. buf[i] is decoded (if
	 * mode==2) the moment index i is reached, using whatever gpa value
	 * is current at that point, THEN consumed (read back) and fed into
	 * the cipher on the following iteration -- see this file's own
	 * header comment for the full derivation. */
	unsigned int i = 0;
	if (mode == 2)
		buf[0] = (unsigned char)(buf[0] ^ DeaxCurrentGpa());
	for (;;) {
		unsigned char b = buf[i];
		i++;
		bzzzzzzzzzzzt12(b);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		bzzzzzzzzzzzt12(0); bzzzzzzzzzzzt12(0);
		if (i >= lenByte)
			break;
		if (mode == 2)
			buf[i] = (unsigned char)(buf[i] ^ DeaxCurrentGpa());
	}
	return 0;
}

/*
 * nv2ac_dispatch_cmd (fFfFfFfFfFfF1F, .text+0x4f4c70, 105 bytes, batch
 * 55): a persistent-global AT88 command dispatcher. Ground truth's own
 * pending-command buffer lives at a fixed .data location, statically
 * initialized to {0xb8, 0x00, 0x00, 0x00} (confirmed via a direct hex
 * dump of ground truth's own .data section at that offset) -- 0xb8 is
 * the real "$B8 Verify Crypto" opcode (CLAUDE.md's own documented AT88SC
 * protocol), i.e. this global starts pre-armed with a pending verify-
 * crypto command every time OA.ko loads, not runtime garbage. Nothing
 * else in this project's own reconstruction currently writes this
 * buffer (no other reconstructed function stores through it) --
 * reproduced here as a plain static initializer, matching its real
 * .data role exactly.
 *
 * Real control flow, confirmed via full disassembly: inspects the
 * buffer's own status nibble (byte[0]&0xf); dispatches either
 * stgNV2AC_sync_read_cmd (status 2 or 6) or stgNV2AC_sync_cmd (byte[3]+4
 * as the length) into the SAME shared 32-byte scratch buffer
 * (g_atmelZoneScratch) cm_ReadUserZone/fFfFfFfFfFfF13 use -- but its OWN
 * return code is NEVER CHECKED (a real, confirmed ground-truth quirk,
 * not a reconstruction gap: `eax` is clobbered by the immediately-
 * following `mov $0x14,%eax`/msleep call before any `test eax,eax`).
 * Then unconditionally reads 1 byte from AT88 zone 0x50 (ground truth's
 * own `fFfFfFfFfFfF1C.clone.0` helper, confirmed byte-for-byte identical
 * to cm_ReadUserZone(0x50,1,&scratch) for this zone -- see this file's
 * own header comment for the "loop A" derivation), discarding the
 * result -- purely to advance the DEAX cipher position. ALWAYS returns 0
 * (ground truth never produces any other value here).
 */
static unsigned char gNv2acPendingCmd[4] = { 0xb8, 0x00, 0x00, 0x00 };

int nv2ac_dispatch_cmd(void)
{
	unsigned char status = (unsigned char)(gNv2acPendingCmd[0] & 0xf);

	if (status == 2 || status == 6) {
		stgNV2AC_sync_read_cmd(gNv2acPendingCmd, g_atmelZoneScratch, 0);
	} else {
		int param = (int)gNv2acPendingCmd[3] + 4;
		stgNV2AC_sync_cmd(gNv2acPendingCmd, param);
	}
	msleep(20);

	unsigned char discard;
	cm_ReadUserZone(0x50, 1, &discard);

	return 0;
}
