/* SPDX-License-Identifier: GPL-2.0 */
/*
 * aintc.c - the ARM Interrupt Controller (AINTC) bring-up code: one of
 * eva_board_crt0's own eleven back-to-back subsystem-init calls
 * (FUN_c0001c84, address-cited but "none individually traced" in
 * eva_board_main.c's crt0-chain section) plus its base-address accessor
 * (FUN_c00018e4) and its real init-table helper (FUN_c0001bd0).
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, read from the
 * pre-fetched dump (all_decompiled.json/all_data.json), 2026-07-18. No live
 * Ghidra MCP calls this pass (bridge flagged concurrency-unsafe this round).
 *
 * ANCHOR: NONE - no "../<Name>.cpp" string sits near 0xc00018e4-0xc0001fd0
 * (same situation panelbus_dispatch.c and heap_alloc.c already document for
 * their own ranges). Attribution here rests entirely on register-offset and
 * constant-value evidence, laid out below - not a string xref.
 *
 * WHY THIS IS AINTC, not a guess:
 *  1. aintc_base() (FUN_c00018e4) unconditionally returns the fixed 32-bit
 *     constant 0xFFFEE000 (decompiled as `DAT_c00018ec`, stored as the
 *     negative literal -0x12000 - i.e. 0x100000000 - 0x12000 - a classic
 *     Ghidra sign-extension artifact for a high MMIO address, not a real
 *     negative value). 0xFFFEE000 is the documented AINTC register base on
 *     TI's OMAP-L1x/DA8xx SoC family (the "TI OMAP-L1x" part this whole
 *     project's README already names as the target).
 *  2. aintc_init (FUN_c0001c84) writes three fixed values into an incoming
 *     handle: +0x04 = 0, +0x10 = 1, +0x1500 = 2. Offset +0x10 and +0x1500
 *     match, byte-for-byte, two AINTC registers with well-known roles on
 *     this SoC family: GER (Global Enable Register, offset 0x10) and HIER
 *     (Host Interrupt Enable Register, offset 0x1500). Writing GER=1 is
 *     literally "turn the controller on"; writing HIER=2 sets exactly the
 *     bit that routes AINTC output onto the ARM926's IRQ line (as opposed
 *     to FIQ, bit 0) - both are textbook AINTC bring-up writes, not a
 *     coincidental offset match.
 *  3. aintc_channel_table_init (FUN_c0001bd0), aintc_init's own sole
 *     callee, zeroes a table of exactly 0x65 (101) 4-byte entries before
 *     populating a handful of them - 101 is the documented AINTC system
 *     interrupt channel count on this SoC family (channels 0-100 map
 *     through the Channel Map Registers onto a smaller set of host
 *     interrupt numbers). The same function then writes 16 further 4-byte
 *     values at handle offsets +0x400 through +0x438 - exactly the CMR0-
 *     CMR15 (Channel Map Register) block, which by definition starts at
 *     offset 0x400 and spans 16 registers on this controller.
 *  Three independent structural matches (base address, GER/HIER offsets,
 *  and the CMR block position + the 101-entry channel count) is why this
 *  file asserts "AINTC" as a real finding rather than a hedge - it settles
 *  eva_board_main.c's own "none individually traced" note for FUN_c0001c84.
 *
 * WHAT REMAINS OPEN: the exact channel-to-priority/host-interrupt mapping
 * this firmware installs (which of the 101 channels get non-zero values,
 * and what physical peripheral each corresponds to) is NOT decoded here -
 * see aintc_channel_table_init's own comment below for the raw values as
 * decompiled, presented without a guessed peripheral-to-channel table.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  aintc_base - trivial accessor, always returns the fixed AINTC MMR base
 *  0xFFFEE000 regardless of any argument passed (the incoming register is
 *  never read - same "phantom forwarded parameter" idiom already documented
 *  elsewhere in this project, e.g. eva_board_main.c's watchdog-fault-wrapper
 *  correction and cdix4192.c's reg_write/reg_read). Called from 24 sites
 *  across the earliest 0xc0000000-0xc0000a20 address range (a cluster of
 *  small, address-adjacent per-peripheral bring-up stubs this pass did not
 *  individually trace - out of this file's assigned range) plus once more
 *  directly from eva_board_crt0 itself (FUN_c00055b8 @ 0xc00055f0), where
 *  its return value ends up silently forwarded into aintc_init below as
 *  that call's own "phantom" argument (see aintc_init's own note).
 *  @0xc00018e4.
 * ------------------------------------------------------------------------- */
uint32_t aintc_base(void)	/* FUN_c00018e4 */
{
	extern uint32_t aintc_base_const;	/* DAT_c00018ec, real value 0xFFFEE000 */
	return aintc_base_const;
}

/* ------------------------------------------------------------------------- *
 *  aintc_channel_table_init - aintc_init's sole callee, and the function
 *  that actually does the confirming work (see file header). Two distinct
 *  writes:
 *
 *   1. A 101-entry software table at a FIXED image address (DAT_c0001c7c,
 *      resolved value 0xC00E0204 - i.e. inside this firmware's OWN data
 *      segment, NOT a hardware register: this is a software channel/
 *      priority bookkeeping array, separate from the real AINTC hardware
 *      registers touched in step 2). Zeroed in full, then seven specific
 *      byte-offset entries get small non-zero values (2,3,4,5,6,7,8) - a
 *      real, disassembly-confirmed set of channel->something assignments,
 *      but WHICH seven of the 101 channels these are, and what physical
 *      peripheral each corresponds to, is not decoded here (would require
 *      cross-referencing this SoC's real channel numbering, not available
 *      to this pass) - transcribed exactly, not renamed into a guessed
 *      peripheral table.
 *   2. The incoming handle's own CMR0-CMR15 block (offsets +0x400 through
 *      +0x438 - see file header for why this offset range is CMR, not a
 *      guess): mostly zeroed, with five non-zero words (+0x408=0x2000000,
 *      +0x414=DAT_c0001c80 [0x7000800], +0x428=0x50000, +0x430=0x40000,
 *      +0x434=0x600, +0x438=0x30000) - each CMR packs four 8-bit channel-
 *      to-host-interrupt mappings, so these non-zero bytes are individual
 *      channel routings; which channel numbers they land on (CMR register
 *      index * 4 + byte position) is mechanically recoverable from the
 *      offsets but not spelled out as named IRQs here, for the same reason
 *      as the software table above.
 *
 *  @0xc0001bd0.
 * ------------------------------------------------------------------------- */
void aintc_channel_table_init(void *aintc_handle)	/* FUN_c0001bd0 */
{
	extern uint32_t *aintc_sw_channel_table;	/* DAT_c0001c7c, fixed firmware-data address 0xC00E0204, NOT a hw reg */
	extern uint32_t aintc_cmr14_const;		/* DAT_c0001c80, real value 0x7000800, written into CMR14 (+0x414) */
	uint8_t *h = (uint8_t *)aintc_handle;
	uint32_t *sw = aintc_sw_channel_table;
	int i;

	for (i = 0; i < 0x65; i++)
		sw[i] = 0;

	/* seven software-table slots, byte-offset-indexed (offset/4 = index):
	 * see the header comment above - real channel identities not decoded. */
	*(uint32_t *)((uint8_t *)sw + 0x5c) = 7;
	*(uint32_t *)((uint8_t *)sw + 0x2c) = 2;
	*(uint32_t *)((uint8_t *)sw + 0xe8) = 3;
	*(uint32_t *)((uint8_t *)sw + 0xc8) = 4;
	*(uint32_t *)((uint8_t *)sw + 0xa8) = 5;
	*(uint32_t *)((uint8_t *)sw + 0xd4) = 6;
	*(uint32_t *)((uint8_t *)sw + 0x54) = 8;

	/* real AINTC CMR0..CMR15, +0x400..+0x43c off the handle (== AINTC MMR
	 * base when called from aintc_init below). */
	*(uint32_t *)(h + 0x400) = 0;
	*(uint32_t *)(h + 0x404) = 0;
	*(uint32_t *)(h + 0x408) = 0x2000000;
	*(uint32_t *)(h + 0x40c) = 0;
	*(uint32_t *)(h + 0x410) = 0;
	*(uint32_t *)(h + 0x414) = aintc_cmr14_const;
	*(uint32_t *)(h + 0x418) = 0;
	*(uint32_t *)(h + 0x41c) = 0;
	*(uint32_t *)(h + 0x420) = 0;
	*(uint32_t *)(h + 0x424) = 0;
	*(uint32_t *)(h + 0x428) = 0x50000;
	*(uint32_t *)(h + 0x42c) = 0;
	*(uint32_t *)(h + 0x430) = 0x40000;
	*(uint32_t *)(h + 0x434) = 0x600;
	*(uint32_t *)(h + 0x438) = 0x30000;
}

/* ------------------------------------------------------------------------- *
 *  aintc_init - one of eva_board_crt0's confirmed eleven init calls (see
 *  eva_board_main.c's own crt0-chain section, which lists FUN_c0001c84 by
 *  address and leaves it "none individually traced" - resolved here).
 *
 *  Real call site (FUN_c00055b8 @ 0xc00055f4): `FUN_c0001c84();` - NO
 *  visible argument in the crt0 decompile. Per this project's now-repeated
 *  "phantom forwarded parameter" finding (see file header point 1 and
 *  eva_board_main.c's own watchdog-fault-wrapper correction), the real
 *  argument is whatever is still sitting in r0 at that point in crt0 - and
 *  the PRECEDING crt0 call is `FUN_c00018e4(DAT_c000560c);`, i.e.
 *  aintc_base() itself, whose return value (0xFFFEE000, the AINTC MMR
 *  base) is left in r0 immediately before this call. Modeled here as
 *  receiving that base pointer directly - consistent both with the offsets
 *  it writes (GER/HIER, real AINTC registers - see header) and with crt0
 *  calling aintc_base() immediately beforehand for no other visible reason
 *  (its own return value is otherwise discarded).
 *
 *  @0xc0001c84.
 * ------------------------------------------------------------------------- */
void aintc_init(void *aintc_handle)	/* FUN_c0001c84 */
{
	uint8_t *h = (uint8_t *)aintc_handle;

	*(uint32_t *)(h + 4) = 0;
	aintc_channel_table_init(aintc_handle);	/* real call site: FUN_c0001bd0(); - same phantom-forward of aintc_handle */
	*(uint32_t *)(h + 0x10) = 1;		/* GER = 1: global interrupt enable */
	*(uint32_t *)(h + 0x1500) = 2;		/* HIER = 2: enable host IRQ line (bit 0 = FIQ, bit 1 = IRQ) */
}

/* -------------------------------------------------------------------------
 * Still genuinely open:
 *  - The real channel identities behind aintc_channel_table_init's seven
 *    software-table entries and five CMR words (which of the 101 AINTC
 *    channels are which peripheral's interrupt line) - not decoded, would
 *    need this SoC variant's own interrupt-channel numbering table, not
 *    available to this pass.
 *  - DAT_c0001c80 (aintc_cmr14_const, 0x7000800) and the software table's
 *    fixed base DAT_c0001c7c (0xC00E0204) - no data-segment symbols in this
 *    ELF-wrapper import to resolve either further.
 *  - Whether aintc_init's incoming handle really is the AINTC base as
 *    argued above, vs. some other value that happens to still be in r0 -
 *    the offsets written (GER/HIER/CMR) are strong corroborating evidence
 *    but this pass had no live disassembly access to confirm the r0
 *    register-continuity claim directly against the crt0 call site's raw
 *    bytes.
 * ------------------------------------------------------------------------- */
