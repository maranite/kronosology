// SPDX-License-Identifier: GPL-2.0
/*
 * nv2ac_exports.cpp  -  the two real exported symbols this module must
 * provide so OA.ko/loadmod.ko link against it exactly as they do against
 * the real OmapNKS4Module.ko, unmodified.
 *
 * Signatures confirmed from OmapNKS4Module's own reconstruction
 * (reconstructed/OmapNKS4Module/driver.cpp, lines 414/419) -- matched
 * here verbatim, including the "cmd4"/"dest" int-typed-pointer style
 * that reconstruction uses (an -mregparm=3 ABI detail: the real function
 * only has 2 meaningful parameters, EAX/EDX; callers that pass a spurious
 * third "unused" argument are fine since it just lands in ECX and is
 * never read):
 *
 *   void stgNV2AC_sync_cmd(unsigned char *address, unsigned int data);
 *   int  stgNV2AC_sync_read_cmd(int cmd4, int dest);
 *
 * Dispatch: which AT88 opcode (address[0] / ((u8*)cmd4)[0]) selects which
 * chip_state.cpp/b8_handshake.cpp entry point to call, confirmed against
 * every real command byte this project has ground-truthed:
 *   $B4 (zone select)      -- stored, not yet gated on (see at88_chip.h's
 *                             "Not yet implemented" note; only zone 0 is
 *                             ever emulated so there is nothing else to
 *                             select into).
 *   $B8 (verify crypto)    -- at88_chip_handle_b8(). Note this is a
 *                             "write" command (stgNV2AC_sync_cmd, void
 *                             return) -- accept/reject is NOT signaled
 *                             back directly; the real protocol (and
 *                             kronos_extract.c's own diagnostic pattern)
 *                             only reveals it via a SUBSEQUENT $B6 read of
 *                             the AAC byte increasing or decreasing.
 *   $B6 (config zone read) -- at88_chip_read_config().
 *   $B2 (zone0 read)       -- at88_chip_read_zone0(), using the chip's
 *                             persistent session cipher state.
 *
 * This file stays freestanding/host-testable on purpose (no real Linux
 * headers, no EXPORT_SYMBOL) -- the kernel-only glue that makes
 * stgNV2AC_sync_cmd/stgNV2AC_sync_read_cmd actually visible to
 * OA.ko/loadmod.ko at link time, plus module_init/exit and the RTAI-safe
 * deferred chip-data loading, lives in module_main.cpp instead.
 */

#include "at88_chip.h"

static AT88ChipState g_chip;

/*
 * at88_chip_module_init -- load the real captured chip data. Meant to be
 * called once from the eventual real init_module(), after reading a
 * KronosExtract.bin-format blob from wherever the real module stores it
 * (a module parameter path, or an embedded data section -- that's a
 * kernel-integration detail out of scope for this file). Returns whatever
 * at88_chip_load_from_extract() returns.
 */
extern "C" int at88_chip_module_init(const unsigned char *blob, unsigned int blobLen)
{
	return at88_chip_load_from_extract(&g_chip, blob, blobLen);
}

extern "C" void stgNV2AC_sync_cmd(unsigned char *address, unsigned int data)
{
	if (!address || data < 1)
		return;

	switch (address[0]) {
	case 0xb4:	/* zone select -- stored for protocol fidelity, unused
			 * (see file header: only zone 0 is ever emulated) */
		if (data >= 3)
			g_chip.selectedZone = address[2];
		break;
	case 0xb8:	/* verify crypto: {0xb8, zone, 0x00, 0x10, Nc[8], Q[8]} */
		if (data >= 20)
			at88_chip_handle_b8(&g_chip, address[1], address + 4, address + 12);
		break;
	default:
		break;	/* unrecognized opcode -- no-op, matching a real chip
			 * silently ignoring a command it doesn't implement */
	}
}

/*
 * nv2ac_read_cmd_impl -- the real logic, pointer-typed. Split out from the
 * int-signature ABI wrapper below on purpose: on the real 32-bit target,
 * `int` and a pointer are the same width, so packing/unpacking through it
 * is lossless -- but on a 64-bit host (where these known-answer tests
 * actually run), truncating a real 64-bit pointer into a 32-bit int and
 * casting it back corrupts it (confirmed the hard way: an early version of
 * this file's test called through the int-signature wrapper directly on a
 * 64-bit host and segfaulted). Same fix this project has used before for
 * 32-bit-target/64-bit-host ABI mismatches: keep the lossy cast confined to
 * a thin wrapper matching the real ABI, and give tests a pointer-typed core
 * to call directly instead.
 */
int nv2ac_read_cmd_impl(const unsigned char *cmd, unsigned char *out)
{
	if (!cmd || !out)
		return -1;

	switch (cmd[0]) {
	case 0xb6:	/* config zone read: {0xb6, 0x00, addr, len} */
		return at88_chip_read_config(&g_chip, cmd[2], cmd[3], out);
	case 0xb2:	/* zone0 (authenticated) read: {0xb2, 0x00, addr, len} */
		return at88_chip_read_zone0(&g_chip, &g_chip.session, cmd[2], cmd[3], out);
	default:
		return -1;	/* unrecognized opcode */
	}
}

extern "C" int stgNV2AC_sync_read_cmd(int cmd4, int dest)
{
	if (!dest)
		return -1;
	return nv2ac_read_cmd_impl((const unsigned char *)(unsigned long)(unsigned int)cmd4,
				    (unsigned char *)(unsigned long)(unsigned int)dest);
}
