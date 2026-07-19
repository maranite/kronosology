/* SPDX-License-Identifier: GPL-2.0 */
/*
 * aintc.c - the ARM Interrupt Controller (AINTC) bring-up code, ported from
 * K1_V06R06/aintc.c (KRONOS_V06R06.VSB). CONFIRMED IDENTICAL to K1 - not a
 * guessed port: every function below is byte-for-byte the same decompiled
 * logic as K1's own aintc_base/aintc_channel_table_init/aintc_init, just at
 * different addresses (K2's data segment is laid out differently) - see the
 * per-function notes for the exact old->new address mapping and the constant
 * values that were independently re-derived from K2's own dump, not assumed
 * to carry over.
 *
 * Ground truth: pre-fetched static Ghidra dump for
 * kronos2s_v01r10_panel.elf (all_decompiled_k2.json/all_data_k2.json),
 * 2026-07-18. No live Ghidra MCP calls this pass.
 *
 * ANCHOR: NONE, same as K1 - no "../<Name>.cpp" string sits near this
 * range in K2 either. Attribution rests on the same three-point structural
 * match K1's own file already established (base-address constant, GER/HIER
 * register offsets, CMR block position + 101-entry channel count), each
 * independently re-verified against K2's own dump below, PLUS a fourth,
 * even stronger point that K1 didn't have available: eva_board_main.c's
 * own K2 crt0 reconstruction (eva_board_crt0, FUN_c0007268) already lists
 * FUN_c0001664 and FUN_c0001a44 by address among its eleven not-yet-traced
 * subsystem-init calls - this file resolves two of those eleven for real,
 * the same way K1's own aintc.c resolved eva_board_main.c's FUN_c0001c84.
 *
 * RE-VERIFIED CONSTANTS (all independently re-derived from K2's dump, not
 * copied from K1):
 *  - AINTC MMR base: K2's DAT_c000166c decompiles as data_value "-0x12000",
 *    the exact same 32-bit representation as K1's DAT_c00018ec - i.e.
 *    0xFFFEE000, confirmed by a raw byte-pattern search for the little-
 *    endian encoding (00 e0 fe ff) in kronos2s_v01r10_panel.elf, which
 *    lands at file offset 0x16c0 = vaddr 0xc000166c, matching exactly.
 *  - CMR14 constant: K2's DAT_c0001a40 = 0x7000800, IDENTICAL numeric value
 *    to K1's DAT_c0001c80 - not just structurally similar, the literal
 *    routing word is unchanged between the two images.
 *  - Software channel table base: K2's DAT_c0001a3c = 0xC00E01E8 (decompiled
 *    as "-0x3ff1fe18"), DIFFERENT from K1's 0xC00E0204 - expected, this is
 *    an address inside each image's own data segment, not a hardware
 *    constant; the differing value is itself confirmation this is a
 *    firmware-internal table, not a HW register (matches K1's own point).
 *  - Same seven software-table byte offsets (0x5c/0x2c/0xe8/0xc8(200)/0xa8/
 *    0xd4/0x54) written with the same seven values (7/2/3/4/5/6/8) - the
 *    channel bookkeeping assignments are unchanged between K1 and K2.
 *  - Same five non-zero CMR words at the same five offsets
 *    (+0x408=0x2000000, +0x414=CMR14 const, +0x428=0x50000, +0x430=0x40000,
 *    +0x434=0x600, +0x438=0x30000) - the real AINTC channel routing this
 *    firmware installs is UNCHANGED between K1 and K2, strong evidence the
 *    underlying SoC's interrupt map (or at least this subset of it) is
 *    identical hardware, not just a similar SoC family member.
 *
 * ADDRESS MAP (K1 -> K2):
 *  aintc_base                  FUN_c00018e4 -> FUN_c0001664
 *  aintc_base_const (DAT)      DAT_c00018ec -> DAT_c000166c   (0xFFFEE000, same)
 *  aintc_channel_table_init    FUN_c0001bd0 -> FUN_c0001990
 *  aintc_sw_channel_table(DAT) DAT_c0001c7c -> DAT_c0001a3c   (0xC00E01E8, differs - see above)
 *  aintc_cmr14_const (DAT)     DAT_c0001c80 -> DAT_c0001a40   (0x7000800, same)
 *  aintc_init                  FUN_c0001c84 -> FUN_c0001a44
 *  crt0 caller                 FUN_c00055b8 -> FUN_c0007268 (eva_board_crt0,
 *                               see eva_board_main.c's own reconstruction)
 *
 * REAL DIFFERENCE FROM K1 (not a transcription artifact): aintc_base has
 * 20 confirmed callers in K2's dump (19 early bring-up stubs in the
 * 0xc0000040-0xc0007268 range, plus the one real eva_board_crt0 call at
 * 0xc00072a0) versus K1's 24 (23 early stubs + crt0). Consistent with this
 * project's already-documented architectural finding (K2_V01R10/README.md):
 * K2 dropped the SPI-based cpsoc/cad peripheral cluster entirely, so fewer
 * of these tiny per-peripheral early-bringup stub functions exist to call
 * aintc_base at all. The early-stub cluster itself was NOT individually
 * traced here, matching K1's own treatment (out of this file's scope).
 *
 * WHAT REMAINS OPEN (same as K1, not resolved by this port): the exact
 * channel-to-priority/host-interrupt mapping (which of the 101 channels
 * are which physical peripheral) is still not decoded - transcribed
 * faithfully, not guessed.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  aintc_base - trivial accessor, always returns the fixed AINTC MMR base
 *  0xFFFEE000 regardless of any argument passed (same "phantom forwarded
 *  parameter" idiom as K1 - the one visible caller with a real argument,
 *  eva_board_crt0's `FUN_c0001664(DAT_c00072bc)`, passes a discarded data
 *  address; DAT_c00072bc = 0xC00E004C, itself just another firmware data
 *  address, not the AINTC base - confirming the argument really is dead).
 *  20 callers total in K2 (vs K1's 24) - see file header for the real,
 *  architecture-driven count difference. @0xc0001664 (K1: @0xc00018e4).
 * ------------------------------------------------------------------------- */
uint32_t aintc_base(void)	/* FUN_c0001664 (K1: FUN_c00018e4) */
{
	extern uint32_t aintc_base_const;	/* DAT_c000166c, value 0xFFFEE000 - IDENTICAL to K1's DAT_c00018ec */
	return aintc_base_const;
}

/* ------------------------------------------------------------------------- *
 *  aintc_channel_table_init - aintc_init's sole callee, CONFIRMED byte-for-
 *  byte identical logic to K1's own FUN_c0001bd0, just re-addressed (see
 *  file header for the full old->new DAT mapping and re-verified constant
 *  values). @0xc0001990 (K1: @0xc0001bd0).
 * ------------------------------------------------------------------------- */
void aintc_channel_table_init(void *aintc_handle)	/* FUN_c0001990 (K1: FUN_c0001bd0) */
{
	extern uint32_t *aintc_sw_channel_table;	/* DAT_c0001a3c, fixed firmware-data address 0xC00E01E8 (K1: 0xC00E0204) - differs, expected, see header */
	extern uint32_t aintc_cmr14_const;		/* DAT_c0001a40, value 0x7000800 - IDENTICAL to K1's DAT_c0001c80 */
	uint8_t *h = (uint8_t *)aintc_handle;
	uint32_t *sw = aintc_sw_channel_table;
	int i;

	for (i = 0; i < 0x65; i++)
		sw[i] = 0;

	/* seven software-table slots, byte-offset-indexed (offset/4 = index):
	 * IDENTICAL values to K1 - real channel identities not decoded. */
	*(uint32_t *)((uint8_t *)sw + 0x5c) = 7;
	*(uint32_t *)((uint8_t *)sw + 0x2c) = 2;
	*(uint32_t *)((uint8_t *)sw + 0xe8) = 3;
	*(uint32_t *)((uint8_t *)sw + 0xc8) = 4;
	*(uint32_t *)((uint8_t *)sw + 0xa8) = 5;
	*(uint32_t *)((uint8_t *)sw + 0xd4) = 6;
	*(uint32_t *)((uint8_t *)sw + 0x54) = 8;

	/* real AINTC CMR0..CMR15, +0x400..+0x43c off the handle (== AINTC MMR
	 * base when called from aintc_init below) - IDENTICAL routing to K1. */
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
 *  aintc_init - one of eva_board_crt0's eleven init calls
 *  (eva_board_main.c's own K2 reconstruction lists FUN_c0001a44 by address
 *  among its "not individually traced" eleven - resolved here, the same way
 *  K1's aintc.c resolved K1's eva_board_main.c note for FUN_c0001c84).
 *
 *  Real call site (FUN_c0007268 @ 0xc00072a4): `FUN_c0001a44();` - no
 *  visible argument, same phantom-forward pattern as K1: the PRECEDING
 *  crt0 call is `FUN_c0001664(DAT_c00072bc)` (aintc_base itself), whose
 *  return value (0xFFFEE000) is left in r0 immediately before this call.
 *  Modeled here identically to K1's own aintc_init for the same reasons
 *  (offsets written match GER/HIER, and the immediately-preceding call
 *  to aintc_base has no other visible purpose for its return value).
 *
 *  @0xc0001a44 (K1: @0xc0001c84).
 * ------------------------------------------------------------------------- */
void aintc_init(void *aintc_handle)	/* FUN_c0001a44 (K1: FUN_c0001c84) */
{
	uint8_t *h = (uint8_t *)aintc_handle;

	*(uint32_t *)(h + 4) = 0;
	aintc_channel_table_init(aintc_handle);	/* real call site: FUN_c0001990(); - same phantom-forward of aintc_handle */
	*(uint32_t *)(h + 0x10) = 1;		/* GER = 1: global interrupt enable */
	*(uint32_t *)(h + 0x1500) = 2;		/* HIER = 2: enable host IRQ line (bit 0 = FIQ, bit 1 = IRQ) */
}

/* -------------------------------------------------------------------------
 * Still genuinely open (identical to K1's own open items - this port does
 * not resolve them, just confirms they carry over unchanged):
 *  - The real channel identities behind aintc_channel_table_init's seven
 *    software-table entries and five CMR words - not decoded in either
 *    image.
 *  - Whether aintc_init's incoming handle really is the AINTC base as
 *    argued above, vs. some other value still in r0 at that point - same
 *    caveat as K1, no live disassembly access this pass to confirm register
 *    continuity directly against the crt0 call site's raw bytes.
 *  - The early-bringup-stub cluster that accounts for aintc_base's other
 *    19 callers was not individually traced (same as K1's 23) - out of
 *    this file's scope.
 * ------------------------------------------------------------------------- */
