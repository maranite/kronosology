/* SPDX-License-Identifier: GPL-2.0 */
/*
 * soc_irq_gate.c - KRONOS2S_V01R10.VSB (Kronos 2) port of the K1
 * reconstruction at K1_V06R06/soc_irq_gate.c: the address block immediately
 * after the ARM exception vector table, made of small per-peripheral AINTC
 * channel enable/"quiesce" (disable+clear) leaf functions plus a handful of
 * larger one-shot crt0/link-status bring-up stubs that call them.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json), read via query_dump_k2.py - no
 * live Ghidra MCP calls this pass (bridge flagged concurrency-unsafe, see
 * this project's own established policy). This is a migration pass from the
 * already-done K1 reconstruction - EVERY function below was independently
 * located in K2's own decompile by CODE SHAPE (not address-offset guessing:
 * K1's own address-to-address deltas for this file's neighboring, already-
 * confirmed primitives range from 0x244 to 0x280 and are NOT constant, so a
 * linear offset from K1's own addresses would have been unreliable here) and
 * cross-checked by resolving its own DAT_ constants, not assumed to carry
 * over unchanged.
 *
 * LOCATION METHOD: K1's own file has no `__FILE__` anchor either (see K1's
 * own header) - attribution here rests on the same evidence class K1 used,
 * strengthened by a systematic full-image sweep of this K2 dump: every K2
 * function whose decompiled text calls `aintc_base()` (aintc.c's own
 * FUN_c0001664) was enumerated (14 hits), 11 of which are genuine AINTC
 * channel-gate leaves. Of the other 3: aintc_base itself self-references in
 * its own decompiled text; eva_board_crt0 (FUN_c0007268, eva_board_main.c's
 * own already-cited crt0 callee) calls aintc_base directly as one of its
 * eleven subsystem-bring-up calls; and the "group A" dispatcher
 * (FUN_c001995c) also calls aintc_base directly in its own tail - THIS one
 * is defined below (CLUSTER 12), not merely cited. Every one of the 11
 * leaves was then decompiled in full and matched, cluster-for-cluster,
 * against K1's own soc_irq_gate.c - a confirmed, non-guessed 1:1 structural
 * match for 10 of the 11 (the 11th, FUN_c0000040/ch-0x15-enable, is K1's own
 * "lives in soc_periph.c" case, cited not defined here either, see
 * CLUSTER 1). The two "group" dispatcher functions that call these leaves
 * (CLUSTER 12 below) were found the same way K1's own file describes for
 * its own group A/B: by tracing the leaves' own `callers` xref lists back
 * to their real invokers.
 *
 * ============================================================================
 * THE CENTRAL FINDING, RECONFIRMED: the SAME fixed ~0x4C-byte bookkeeping
 * table at 0xC00E0000
 * ============================================================================
 *
 * Every DAT_ constant this cluster references that isn't a hardware base
 * resolves into 0xC00E0000-0xC00E004C - the IDENTICAL fixed physical address
 * K1's own file already found for its own, slightly larger (0xC00E0000-
 * 0xC00E0068) version of this same table. Independently re-derived here from
 * K2's own literal-pool arithmetic (e.g. DAT_c0000090's decompiled value
 * -0x3ff1ffb4 == 0x100000000-0x3ff1ffb4 == 0xC00E004C), not copied from K1's
 * own citations. This is strong, confirmed evidence the table is a fixed
 * physical SRAM/scratch location on the OMAP-L138/DA850 silicon itself, not
 * a firmware-image-relative data structure - it survived a genuine firmware
 * rewrite (K1 -> K2) at the exact same address. K2's own table is smaller
 * (0xC00E0000-0xC00E004C vs K1's 0xC00E0000-0xC00E0068) because K2 genuinely
 * dropped several of K1's channel-gate slots (Timer64P1/ch-0x17,
 * channel-0x36 - see CLUSTERS 1 and 5) rather than merely shrinking offsets.
 * The "dead phantom handle" slot every leaf below shares (see K1's own
 * "phantom forwarded parameter" idiom, confirmed identically here) sits at
 * table+0x4C in K2, not K1's own table+0x68 - same ROLE, different absolute
 * offset, a real numeric difference not smoothed over.
 *
 * THE AINTC INDEXED ENABLE/DISABLE REGISTER MODEL - IDENTICAL TO K1:
 *   +0x24  SICR (indexed status-clear), +0x28  EISR (indexed enable-set),
 *   +0x2C  EICR (indexed enable-clear). Every function below that writes
 *   ONLY +0x28 is an "enable" stub; every function that writes +0x2C and/or
 *   +0x24 (never +0x28) is a "disable+clear" stub - re-confirmed, same
 *   hardware idiom, same base (aintc_base() = 0xFFFEE000, aintc.c).
 *
 * ============================================================================
 * REAL DIFFERENCES FROM K1 (not transcription artifacts - each independently
 * re-derived from K2's own dump, not assumed)
 * ============================================================================
 *  1. Timer64P1 (channel 0x17) and channel 0x36 GATING IS GONE. A full-image
 *     text sweep for AINTC writes of literal 0x17 or 0x36 into +0x28/+0x2C
 *     found ZERO matches anywhere in K2's covered function set - not a
 *     coverage gap, a genuine architectural removal (consistent with
 *     aintc.c's/soc_periph.c's own independently-documented finding that
 *     K2's Timer64P1 boot-time bring-up is gone entirely - see those files'
 *     own headers).
 *  2. Channel 0x2a NOW HAS A CONFIRMED ENABLE SIDE. K1's own file explicitly
 *     left this "STILL OPEN" ("no matching enable stub found anywhere in
 *     this file's assigned range"). K2's group-A dispatcher (CLUSTER 12)
 *     inlines a genuine ch-0x2a EISR-enable directly in its own tail, rather
 *     than as a separate leaf function - resolves K1's own open question for
 *     this board, in the negative (it's real, just not a standalone leaf).
 *  3. Channel 0x32's disable side now RE-ARMS the enable guard. K1's own
 *     ch32 quiesce function never touched the enable-side guard flag; K2's
 *     equivalent (soc_irq_gate_ch32_quiesce_gpio below) zeroes the SAME
 *     cached-pointer slot ch32's own enable function uses as its lazy-init
 *     guard, at the very end - meaning K2's disable path re-arms the next
 *     enable call to re-fetch the GPIO base and re-enable the channel from
 *     scratch, a real behavioral difference from K1's simpler one-shot guard.
 *  4. Channel 0x2a's disable-side GPIO ack is INLINED. K1 called a dedicated
 *     omap_gpio.c helper (gpio_pair0_intstat_ack_bit5); K2's equivalent
 *     writes gpio_base+0x34 = 0x20 directly inline, with no separate callee
 *     found anywhere near this offset in omap_gpio.c. Presented as a real
 *     simplification, not forced into a call to a same-shaped omap_gpio.c
 *     function that doesn't actually exist at this exact offset.
 *  5. soc_irq_gate_mcasp2_bringup's K2 body is a genuine SUBSET of K1's own
 *     11-callee sequence - see that function's own comment for the exact,
 *     confirmed differences (a different second base-getter call,
 *     ONE final GPIO write instead of three).
 *  6. midi_hw_write16/read16's caller counts (130/53) are essentially
 *     identical to K1's own independently-documented counts (129/53,
 *     mcasp.c's own re-verification pass note) - reconfirms this pair is
 *     genuinely shared, firmware-wide low-level MMIO plumbing on both
 *     boards, not MIDI-exclusive despite the inherited name.
 *
 * EXCLUDED from this file (physically inside the swept range, already fully
 * reconstructed/cited elsewhere - not duplicated here per this project's
 * "never edit a file outside your own pass" convention):
 *   - FUN_c0000040 (ch-0x15 enable, Timer64P0 lazy-init singleton) - LIVE
 *     QUERY RESOLVED 2026-07-19 (dedicated live Ghidra MCP pass, see CLAUDE.md
 *     "2-agent cap" authorization): now DEFINED as timer64p0_enable_ch15 in
 *     soc_periph.c, closing this file's own oldest citation. The live
 *     decompile/disassembly of this function ALSO resolved two things this
 *     file previously got wrong or left open - see soc_irq_gate_timer0_
 *     quiesce's own updated comment below: (1) the "table+0x08" identity this
 *     file assigned to soc_irq_gate_slot_0x08_handle was a confirmed
 *     transcription ERROR - the real live-memory literal is 0xC00E0000
 *     (table+0x00), not 0xC00E0008; (2) that table+0x00 slot's real identity
 *     is now known: it is the Timer64P0 peripheral base pointer, cached there
 *     by timer64p0_enable_ch15 itself.
 *   - FUN_c0001780 (i2c0_i2c1_base_select, the OMAP-L138 I2C0/I2C1 base
 *     selector) - fully defined in soc_periph.c already; not this file's
 *     hardware family (see panelbus_dispatch.c's own header for why K1's
 *     own panelbus dispatcher built on this exact selector has no confirmed
 *     K2 higher-level counterpart).
 *   - FUN_c000a4bc (this board's eva_link_status_change/"link status fault"
 *     equivalent - CONFIRMED via direct decompile to call
 *     soc_irq_gate_timer0_quiesce/_ch0b_quiesce/_ch35_quiesce/_ch32_quiesce_
 *     gpio/_ch2a_quiesce DIRECTLY, exactly mirroring K1's own
 *     eva_link_status_change calling "group B" disable stubs directly in
 *     ADDITION to group B's own dispatcher) - already cited `extern` by
 *     wire_dispatch.c as `eva_boot_status_unk_4`, flagged there as "not
 *     individually attributed this pass". Not redefined here (out of this
 *     file's own scope, matching K1's identical treatment of FUN_c0008a5c);
 *     cited by address only below, for the cross-file caller evidence.
 *
 * NOT SWEPT this pass (genuinely open, not guessed): the "gap_slot_bringup"
 * cluster (K1's FUN_c0000a20, twice-called usbdc_gap_config_slot-style
 * bring-up) - no K2 omap_l137_addr_gap_misc.c-equivalent file exists in this
 * tree yet to cross-check against, and no confirmed K2 candidate was found
 * in the time budget for this pass; K1's own soc_irq_gate_slot0x00_get
 * (sole caller: master_dispatch_tick) and soc_irq_gate_ring3_state_reset
 * (unbounded "from_func: null" caller) - not independently re-located in K2
 * this pass. See STILL OPEN at the bottom.
 */

#include <stdint.h>

/* ===========================================================================
 * CLUSTER 1 - Timer64P0 AINTC enable/disable pair (ch 0x15). Timer64P1
 * (K1's ch 0x17) has NO K2 counterpart - see REAL DIFFERENCES #1 above.
 *
 * Timer64P0's own "enable" half (FUN_c0000040, ch 0x15) is EXCLUDED here -
 * see the file header's EXCLUDED section. Its "disable+clear" half is here.
 * ============================================================================ */

extern uint32_t aintc_base(void);			/* FUN_c0001664, aintc.c (K1: FUN_c00018e4) */

/* soc_irq_gate_timer0_quiesce - disable+clear pair of soc_periph.c's own
 * timer64p0_enable_ch15 (FUN_c0000040, now defined - see EXCLUDED above).
 * Sets bit 1 (0x2) in a cached handle's own +0x44 register first, then
 * disables+clears AINTC ch 0x15 via EICR/SICR. CONFIRMED called from BOTH
 * FUN_c000a4bc (this board's eva_link_status_change-equivalent, see EXCLUDED
 * above) and soc_irq_gate_group_b_disable below - exactly mirroring K1's own
 * "group B" call shape.
 *
 * LIVE-QUERY RESOLVED 2026-07-19, TWO CORRECTIONS: (1) the cached handle is
 * table+0x00 (0xC00E0000), NOT table+0x08 as this file previously claimed -
 * confirmed by direct read_memory of this function's own literal pool
 * (DAT_c0000124 reads 0xC00E0000 byte-for-byte). (2) its real identity is now
 * known: it is the Timer64P0 peripheral base pointer, cached at table+0x00
 * by soc_periph.c's own timer64p0_enable_ch15 (FUN_c0000040) - see that
 * function's own comment for the full cross-file finding. This also RETRACTS
 * this file's prior "SAME slot as soc_irq_gate_mcasp_param_b_cache" claim
 * below (CLUSTER 2): mcasp_param_b_cache's own literal independently
 * read-verified as 0xC00E0008 (genuinely table+0x08, unchanged) - the two
 * slots are NOT the same; only CLUSTER 1's own annotation was wrong.
 * @0xc00000e0 (K1: @0xc00000f4).
 * ------------------------------------------------------------------------- */
extern uint32_t *soc_irq_gate_slot_0x00_handle;	/* DAT_c0000124/DAT_c0000128, resolved 0xC00E0000 = table+0x00 - CONFIRMED via read_memory 2026-07-19, this file's own prior "table+0x08" annotation was a transcription error. Real identity: Timer64P0 base cache, written by soc_periph.c's timer64p0_enable_ch15 */

void soc_irq_gate_timer0_quiesce(void)		/* FUN_c00000e0 */
{
	uint32_t aintc;

	*(uint32_t *)((uint8_t *)*soc_irq_gate_slot_0x00_handle + 0x44) |= 2;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x15;	/* EICR: disable ch 21 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x15;	/* SICR: clear ch 21 status */
}

/* ===========================================================================
 * CLUSTER 2 - channel 0x0b (11) pair: McASP clock-param latch (enable side)
 * / a 3-word register-block sequence (disable side). CONFIRMED structurally
 * identical to K1's own cluster 2, same double-check quirk on the disable
 * side transcribed exactly.
 * =========================================================================== */

extern uint32_t mcasp_clock_param_select_a(void *base, int sel);	/* FUN_c000171c, mcasp.c */
extern uint32_t mcasp_clock_param_select_b(void *base, int sel);	/* FUN_c0001730, mcasp.c */

/* soc_irq_gate_ch0b_enable_mcasp_latch - lazy-init guard is a plain BYTE
 * flag (table+0x28 = 0xC00E0028), enabling ch 0x0b (11) via EISR on first
 * call. THEN, unconditionally on every call, latches
 * mcasp_clock_param_select_a(_, 0) into table+0x20 (0xC00E0020) and
 * mcasp_clock_param_select_b(_, 0) into table+0x08 (0xC00E0008), and caches
 * aintc_base() into table+0x24 (0xC00E0024). @0xc000012c (K1: @0xc000027c).
 *
 * CORRECTION 2026-07-19 (live Ghidra MCP pass, read_memory-confirmed): this
 * comment previously claimed table+0x08 here was "the SAME slot as CLUSTER
 * 1's own dead-handle cache" - that was wrong on CLUSTER 1's side (CLUSTER
 * 1's own cache is table+0x00, not +0x08, see soc_irq_gate_timer0_quiesce's
 * own updated comment above); this cluster's own table+0x08 = 0xC00E0008
 * value is itself independently read-confirmed correct and unchanged.
 * ------------------------------------------------------------------------- */
extern uint8_t  *soc_irq_gate_ch0b_guard;		/* DAT_c000019c, table+0x28 = 0xC00E0028 */
extern uint32_t  soc_irq_gate_slot_0x4c_a;		/* DAT_c00001a0, table+0x4c = 0xC00E004C - dead phantom handle */
extern uint32_t *soc_irq_gate_mcasp_param_a_cache;	/* DAT_c00001a4, table+0x20 = 0xC00E0020 */
extern uint32_t *soc_irq_gate_mcasp_param_b_cache;	/* DAT_c00001a8, resolved 0xC00E0008 = table+0x08 - CONFIRMED via read_memory 2026-07-19 as genuinely DISTINCT from CLUSTER 1's soc_irq_gate_slot_0x00_handle (table+0x00): this file's prior "SAME slot" claim between the two is RETRACTED, see soc_irq_gate_timer0_quiesce's own updated comment above */
extern uint32_t *soc_irq_gate_ch0b_aintc_cache;	/* DAT_c00001ac, table+0x24 = 0xC00E0024 */

void soc_irq_gate_ch0b_enable_mcasp_latch(void)	/* FUN_c000012c */
{
	if (*soc_irq_gate_ch0b_guard == 0) {
		uint32_t aintc;
		*soc_irq_gate_ch0b_guard = 1;
		aintc = aintc_base();
		*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x0b;	/* EISR: enable ch 11 */
	}

	*soc_irq_gate_mcasp_param_a_cache =
		mcasp_clock_param_select_a((void *)(uintptr_t)soc_irq_gate_slot_0x4c_a, 0);
	*soc_irq_gate_mcasp_param_b_cache =
		mcasp_clock_param_select_b((void *)(uintptr_t)soc_irq_gate_slot_0x4c_a, 0);
	*soc_irq_gate_ch0b_aintc_cache = aintc_base();
}

/* soc_irq_gate_ch0b_quiesce - disable+clear of ch 0x0b (11). Pokes a 3-word
 * register block at a cached handle's own +0x2068/+0x2070/+0x2078 - IDENTICAL
 * literal offsets to K1, including the apparently-redundant double check of
 * +0x2068 (write 1, conditionally write +0x2078, unconditionally overwrite
 * +0x2070 to 2, re-check +0x2068 AGAIN) - transcribed exactly as decompiled,
 * a genuine quirk on both boards, not a transcription error. The cached
 * handle itself (table+0x20 = 0xC00E0020) is the SAME slot
 * soc_irq_gate_mcasp_param_a_cache above uses - re-confirms K1's own "SAME
 * slot as mcasp_param_a_cache" finding exactly. @0xc00002a8 (K1: @0xc0000404).
 * ------------------------------------------------------------------------- */
extern uint32_t *soc_irq_gate_ch0b_regblock;	/* DAT_c0000314, table+0x20 = 0xC00E0020 - SAME slot as soc_irq_gate_mcasp_param_a_cache above */
extern uint32_t  soc_irq_gate_slot_0x4c_b;	/* DAT_c0000320, table+0x4c = 0xC00E004C - dead phantom handle */

void soc_irq_gate_ch0b_quiesce(void)	/* FUN_c00002a8 */
{
	uint8_t *regs = (uint8_t *)*soc_irq_gate_ch0b_regblock;
	uint32_t aintc;

	*(uint32_t *)(regs + 0x2070) = 1;
	if (*(uint32_t *)(regs + 0x2068) != 0)
		*(uint32_t *)(regs + 0x2078) = 1;
	*(uint32_t *)(regs + 0x2070) = 2;	/* overwrites the =1 write above - faithful, not simplified */
	if (*(uint32_t *)(regs + 0x2068) != 0)	/* same condition re-checked - faithful, not simplified */
		*(uint32_t *)(regs + 0x2078) = 1;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x0b;	/* EICR: disable ch 11 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x0b;	/* SICR: clear ch 11 status */
}

/* ===========================================================================
 * CLUSTER 3 - channel 0x3a (58) pair: USB0/OTG-adjacent. Structurally
 * identical to K1's own cluster 3.
 * =========================================================================== */

extern uint32_t usb0_otg_base_get(void *chip);	/* FUN_c000183c, soc_periph.c */

/* soc_irq_gate_ch3a_enable - lazy-init guard (byte flag, table+0x2c =
 * 0xC00E002C), enables ch 0x3a (58) via EISR only. @0xc000049c (K1: @0xc00005f8). */
extern uint8_t  *soc_irq_gate_ch3a_guard;	/* DAT_c00004d4, table+0x2c = 0xC00E002C */
extern uint32_t  soc_irq_gate_slot_0x4c_c;	/* DAT_c00004d8, table+0x4c = 0xC00E004C - dead phantom handle */

void soc_irq_gate_ch3a_enable(void)	/* FUN_c000049c */
{
	uint32_t aintc;

	if (*soc_irq_gate_ch3a_guard != 0)
		return;
	*soc_irq_gate_ch3a_guard = 1;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x3a;	/* EISR: enable ch 58 */
}

/* soc_irq_gate_ch3a_quiesce - disable+clear of ch 0x3a (58). Also copies
 * usb0_otg_base_get(chip)'s own +0x20 field into that SAME handle's +0x28
 * field first - IDENTICAL self-referential copy to K1. @0xc00004f8
 * (K1: @0xc0000654). */
extern uint32_t  soc_irq_gate_slot_0x4c_d;	/* DAT_c000053c, table+0x4c = 0xC00E004C - dead phantom handle */

void soc_irq_gate_ch3a_quiesce(void)	/* FUN_c00004f8 */
{
	uint8_t *usb0 = (uint8_t *)usb0_otg_base_get((void *)(uintptr_t)soc_irq_gate_slot_0x4c_d);
	uint32_t aintc;

	*(uint32_t *)(usb0 + 0x28) = *(uint32_t *)(usb0 + 0x20);

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x3a;	/* EICR: disable ch 58 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x3a;	/* SICR: clear ch 58 status */
}

/* ===========================================================================
 * CLUSTER 4 - channel 0x35 (53) pair: UART1.
 *
 * CROSS-FILE RESOLUTION: soc_periph.c's own header flags uart1_base_get
 * (FUN_c000180c) as "single-value, no-parameter... whether K2 dropped the
 * multi-UART selector shape... is NOT resolved this pass" - this cluster's
 * own enable side (below) is the real, confirmed caller that settles it:
 * uart1_base_get really is called with no arguments here, matching its own
 * declared no-parameter signature exactly.
 * =========================================================================== */

extern uint32_t uart1_base_get(void);	/* FUN_c000180c, soc_periph.c - CONFIRMED no-parameter, see note above */

/* soc_irq_gate_ch35_enable - lazy-init guard (byte flag, table+0x34 =
 * 0xC00E0034), fetches uart1_base_get() into table+0x30 (0xC00E0030),
 * enables ch 0x35 (53) via EISR. @0xc0000540 (K1: @0xc000069c). */
extern uint8_t  *soc_irq_gate_ch35_guard;		/* DAT_c0000588, table+0x34 = 0xC00E0034 */
extern uint32_t  soc_irq_gate_slot_0x4c_e;		/* DAT_c000058c, table+0x4c = 0xC00E004C - dead phantom handle */
extern uint32_t *soc_irq_gate_uart1_base_cache;	/* DAT_c0000590, table+0x30 = 0xC00E0030 */

void soc_irq_gate_ch35_enable(void)	/* FUN_c0000540 */
{
	uint32_t aintc;

	if (*soc_irq_gate_ch35_guard != 0)
		return;
	*soc_irq_gate_ch35_guard = 1;

	*soc_irq_gate_uart1_base_cache = uart1_base_get();

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x35;	/* EISR: enable ch 53 */
}

/* soc_irq_gate_ch35_quiesce - disable+clear of ch 0x35 (53), no extra
 * hardware pokes, same as K1. @0xc0000624 (K1: @0xc0000784). */
extern uint32_t  soc_irq_gate_slot_0x4c_f;	/* DAT_c0000654, table+0x4c = 0xC00E004C - dead phantom handle */

void soc_irq_gate_ch35_quiesce(void)	/* FUN_c0000624 */
{
	uint32_t aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x35;	/* EICR: disable ch 53 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x35;	/* SICR: clear ch 53 status */
}

/* ===========================================================================
 * CLUSTER 5 - K1's channel 0x36 (54) pair: NO K2 COUNTERPART.
 *
 * A full-image text sweep of every K2 function's decompile for an AINTC
 * write of literal 0x36 into +0x28/+0x2c found zero matches - genuinely
 * absent, not a coverage gap. See REAL DIFFERENCES #1 above.
 * =========================================================================== */

/* ===========================================================================
 * CLUSTER 6 - channel 0x32 (50) pair: GPIO-bank-adjacent. Real, confirmed
 * behavioral difference from K1 on the disable side - see REAL DIFFERENCES
 * #3 above.
 * =========================================================================== */

extern uint32_t gpio_bank_get_base(void);	/* FUN_c0001710, soc_periph.c */

/* soc_irq_gate_ch32_enable_gpio - the guard IS the cache (pointer-guard
 * idiom, table+0x38 = 0xC00E0038): if nonzero, already armed, return; else
 * caches gpio_bank_get_base() into that same slot (making it simultaneously
 * the "armed" flag and the cached base) and enables ch 0x32 (50) via EISR.
 * @0xc0000658 (K1: @0xc000087c). */
extern uint32_t   soc_irq_gate_slot_0x4c_g;		/* DAT_c0000698, table+0x4c = 0xC00E004C - dead phantom handle */
extern void     **soc_irq_gate_gpio_bank_cache;	/* DAT_c0000694, table+0x38 = 0xC00E0038 - guard AND cache, same slot */

void soc_irq_gate_ch32_enable_gpio(void)	/* FUN_c0000658 */
{
	uint32_t aintc;

	if (*soc_irq_gate_gpio_bank_cache != 0)
		return;

	*soc_irq_gate_gpio_bank_cache = (void *)(uintptr_t)gpio_bank_get_base();

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x32;	/* EISR: enable ch 50 */
}

/* soc_irq_gate_ch32_quiesce_gpio - disable+clear of ch 0x32 (50). Writes 4
 * into the CACHED gpio bank base's own +0xd4 field (same undecoded register
 * offset K1 already left open), THEN, CONFIRMED REAL DIFFERENCE FROM K1
 * (see REAL DIFFERENCES #3 above): clears the SAME table+0x38 slot the
 * enable side above uses as its guard - re-arming the next enable call to
 * re-fetch the GPIO base from scratch. @0xc00006d8 (K1: @0xc000090c). */
extern void   **soc_irq_gate_gpio_bank_cache_2;	/* DAT_c0000720, table+0x38 = 0xC00E0038 - SAME slot as the enable side above */
extern uint32_t  soc_irq_gate_slot_0x4c_h;		/* DAT_c0000724, table+0x4c = 0xC00E004C - dead phantom handle */

void soc_irq_gate_ch32_quiesce_gpio(void)	/* FUN_c00006d8 */
{
	uint32_t aintc;

	*(uint32_t *)((uint8_t *)*soc_irq_gate_gpio_bank_cache_2 + 0xd4) = 4;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x32;	/* EICR: disable ch 50 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x32;	/* SICR: clear ch 50 status */

	*soc_irq_gate_gpio_bank_cache_2 = 0;	/* CONFIRMED REAL DIFFERENCE FROM K1: re-arms the enable-side guard */
}

/* ===========================================================================
 * CLUSTER 7 - channel 0x2a (42), disable-only leaf here (the enable side is
 * now confirmed real, but inlined into CLUSTER 12's group-A dispatcher, not
 * a standalone leaf - see REAL DIFFERENCES #2 above and CLUSTER 12 below).
 * =========================================================================== */

/* soc_irq_gate_ch2a_quiesce - disable+clear of ch 0x2a (42). Re-fetches
 * gpio_bank_get_base() fresh, then writes gpio_base+0x34 = 0x20 INLINE
 * (CONFIRMED REAL DIFFERENCE FROM K1, see REAL DIFFERENCES #4 above - K1
 * called a dedicated omap_gpio.c helper for this same bit; no matching
 * omap_gpio.c leaf at this exact offset was found in K2) before the AINTC
 * writes. @0xc00007b4 (K1: @0xc00009d8). */
extern uint32_t soc_irq_gate_slot_0x4c_i;	/* DAT_c00007f4, table+0x4c = 0xC00E004C - dead phantom handle */

void soc_irq_gate_ch2a_quiesce(void)	/* FUN_c00007b4 */
{
	uint32_t aintc;
	uint8_t *gpio = (uint8_t *)(uintptr_t)gpio_bank_get_base();

	*(uint32_t *)(gpio + 0x34) = 0x20;	/* inline GPIO ack write - see note above */

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x2a;	/* EICR: disable ch 42 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x2a;	/* SICR: clear ch 42 status */
}

/* ===========================================================================
 * CLUSTER 8 - soc_irq_gate_mcasp2_bringup: the second (McASP-2) instance
 * bring-up stub. K1's own "gap_slot_bringup" sibling (K1: FUN_c0000a20) was
 * NOT located this pass - see the file header's own NOT SWEPT note.
 *
 * CONFIRMED called from FUN_c000a4bc (this board's eva_link_status_change
 * equivalent, see EXCLUDED above) - the SAME cross-file caller relationship
 * K1's own file documents for its own soc_irq_gate_mcasp2_bringup.
 * =========================================================================== */

extern uint32_t mcasp0_base_get(void *chip);		/* FUN_c0001760, soc_periph.c - RESOLVES K1's own unattributed FUN_c00019e0 */
extern uint32_t mcasp0_fifo_base_get(void *chip);	/* FUN_c0001768, soc_periph.c - RESOLVES K1's own unattributed FUN_c00019e8 */
extern void     mcasp_reinit_reduced(void *ma, void *sub_config);	/* FUN_c0002d00 - name/signature per mcasp.c's own citation (that file independently identifies this exact function/caller pair and deliberately does not define a callable body, see its own "STILL OPEN" item 2); NOT redefined here either, cited under mcasp.c's own chosen name for cross-file consistency */
extern uint32_t timer64p0_base_get(void *chip);	/* FUN_c0001670, soc_periph.c */
extern void     board_desc_type_clear_enabled(int handle);	/* FUN_c0001ae8, soc_periph.c - real call sites below show it with NO visible argument (phantom-forward, same idiom as elsewhere) */
extern uint32_t syscfg0_base_get(void *chip);		/* FUN_c00016c8, soc_periph.c */

/* soc_irq_gate_mcasp2_bringup - CONFIRMED REAL SUBSET of K1's own 11-call
 * sequence (see REAL DIFFERENCES #5 above): fetches mcasp0_fifo_base_get()
 * and mcasp0_base_get() (both phantom-forwarded the shared table+0x4c dead
 * handle) and feeds them into mcasp_reinit_reduced(ma=base, sub_config=fifo)
 * - NOTE, unlike K1's own documented argument-order swap, this K2 call site
 * passes them in the SAME order mcasp_reinit_reduced's own two parameters
 * are declared (ma first, sub_config second) - K1's own swap is NOT
 * reproduced here, a real, confirmed difference, not an oversight.
 * `mcasp_reinit_reduced` itself (FUN_c0002d00) is CONFIRMED, by mcasp.c's
 * own independent cross-file analysis, structurally identical to K1's own
 * FUN_c0003228 (same 84-byte size, same shape) and is THIS function's own
 * sole real caller (mcasp.c's own citation: "caller is FUN_c0000864") -
 * deliberately not redefined here either, matching mcasp.c's own choice not
 * to assert an unconfirmed file attribution for it. Then: one more
 * mcasp_clock_param_select_a(_, 0) call (matching CLUSTER 2's enable side);
 * timer64p0_base_get() and board_desc_type_clear_enabled() (both phantom-
 * forwarded); syscfg0_base_get() where K1's own version called
 * timer64p_base_select(_, 1) a second time - CONFIRMED REAL DIFFERENCE, K2
 * calls a genuinely different peripheral base getter at this position, not
 * a re-attribution error; ends with gpio_bank_get_base() and ONE final GPIO
 * write, gpio+0x1c |= 0x100 - K1 had THREE separate GPIO writes here
 * (gpio_pair3_bit15_set, gpio_pair4_bit_set(_,0xf000), a bit-clear variant);
 * K2 does only this one. @0xc0000864 (K1: @0xc0000aa4). */
void soc_irq_gate_mcasp2_bringup(void)	/* FUN_c0000864 */
{
	uint32_t handle = 0;	/* real value: DAT_c00008c8, table+0x4c = 0xC00E004C - dead phantom handle */
	uint32_t fifo_base;
	uint32_t ma_base;
	uint32_t gpio;

	fifo_base = mcasp0_fifo_base_get((void *)(uintptr_t)handle);
	ma_base   = mcasp0_base_get((void *)(uintptr_t)handle);
	mcasp_reinit_reduced((void *)(uintptr_t)ma_base, (void *)(uintptr_t)fifo_base);

	(void)mcasp_clock_param_select_a((void *)(uintptr_t)handle, 0);

	(void)timer64p0_base_get(0 /* phantom-forward, no visible arg at real call site */);
	board_desc_type_clear_enabled(0 /* phantom-forward */);

	(void)syscfg0_base_get(0 /* phantom-forward - CONFIRMED DIFFERENT from K1's own second timer64p_base_select(_,1) call here */);
	/* real decompile calls one more no-arg callee (FUN_c0001bf4) immediately
	 * after - not independently identified anywhere else in this project's
	 * K2 tree, left uncalled here rather than guessed */

	gpio = gpio_bank_get_base();
	*(uint32_t *)((uintptr_t)gpio + 0x1c) = 0x100;	/* single GPIO write - see note above on the K1 3-write difference */
}

/* ===========================================================================
 * CLUSTER 9 - USB endpoint register-block pointer setup. CONFIRMED
 * structurally identical to K1's own cluster 9.
 * =========================================================================== */

extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c000a728, omap_l137_usbdc.c (K1: FUN_c0009194) */

/* soc_irq_gate_usbdc_ep_ptrs_init - computes two USB endpoint-adjacent
 * pointers (reloc_base+0x2e00, reloc_base+0x3000 - IDENTICAL offsets to K1)
 * and stores them into two fixed globals. @0xc000094c (K1: @0xc0000be8). */
extern uint32_t  soc_irq_gate_usbdc_reloc_arg;	/* DAT_c0000988 */
extern uint32_t *soc_irq_gate_usbdc_ep_ptr_a;	/* DAT_c000098c */
extern uint32_t *soc_irq_gate_usbdc_ep_ptr_b;	/* DAT_c0000990 */

void soc_irq_gate_usbdc_ep_ptrs_init(void)	/* FUN_c000094c */
{
	*soc_irq_gate_usbdc_ep_ptr_a = omap_usbdc_reloc(soc_irq_gate_usbdc_reloc_arg) + 0x2e00;
	*soc_irq_gate_usbdc_ep_ptr_b = omap_usbdc_reloc(soc_irq_gate_usbdc_reloc_arg) + 0x3000;
}

/* ===========================================================================
 * CLUSTER 10 - MIDI hardware register primitives. CONFIRMED structurally
 * identical to K1's own cluster 10, including the shared, firmware-wide
 * (not MIDI-exclusive) caller counts - see REAL DIFFERENCES #6 above.
 * =========================================================================== */

extern uint32_t aemif_cs3_base_get(void *chip);	/* FUN_c0001854, soc_periph.c (K1's own FUN_c0001a98: "fixed return regardless of argument") */

extern uint32_t soc_irq_gate_midi_base_arg_w;	/* DAT_c00009cc, table+0x4c-shaped dead phantom handle, fed into aemif_cs3_base_get which ignores it anyway */

/* midi_hw_write16 - @0xc000099c (K1: @0xc0000c38). 130 callers (K1: 129). */
void midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val)	/* FUN_c000099c */
{
	(void)base_sel;
	uint32_t base = aemif_cs3_base_get((void *)(uintptr_t)soc_irq_gate_midi_base_arg_w);
	*(uint16_t *)(base + (reg_off & 0xffff)) = val;
}

extern uint32_t soc_irq_gate_midi_base_arg_r;	/* DAT_c00009f8, same dead-handle role as the write side (separate literal-pool slot) */

/* midi_hw_read16 - @0xc00009d0 (K1: @0xc0000c6c). 53 callers (K1: 53, exact match). */
uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off)	/* FUN_c00009d0 */
{
	(void)base_sel;
	uint32_t base = aemif_cs3_base_get((void *)(uintptr_t)soc_irq_gate_midi_base_arg_r);
	return *(uint16_t *)(base + (reg_off & 0xffff));
}

/* midi_hw_fifo_write - CONFIRMED byte-for-byte structurally identical to K1:
 * same fixed-address-per-call, non-incrementing FIFO push register; channel
 * 0 uses base+0x6e, channel N!=0 uses base+N*0x20+0x72; command byte 0x80
 * (normal) / 0xa0 (trailing odd byte) written to channel 0's fixed 0x60 or
 * N!=0's `(N*0x20+0x68) & mask` register; channel 0's dummy status pre-read
 * (return discarded) preserved. @0xc00009fc (K1: @0xc0000c98). */
extern uint32_t soc_irq_gate_midi_base_arg_fw;	/* DAT_c0000b04, same dead-handle role */
extern uint16_t soc_irq_gate_midi_cmd_align_mask;	/* DAT_c0000b08, real literal 0xfff8, IDENTICAL to K1 */

void midi_hw_fifo_write(uint32_t base_sel, int channel, const uint16_t *data, uint32_t len)
{
	uint32_t words = len >> 1;
	uint16_t cmd_reg;
	uint16_t trigger;

	if (channel == 0) {
		volatile uint16_t *push = (volatile uint16_t *)(aemif_cs3_base_get((void *)(uintptr_t)soc_irq_gate_midi_base_arg_fw) + 0x6e);

		(void)midi_hw_read16(base_sel, 0x70);	/* dummy status poll, return discarded - confirmed real */
		if (len == 0)
			return;

		for (; words != 0; words--)
			*push = *data++;

		if (len & 1) {
			*push = (uint16_t)(uint8_t)*data;
			midi_hw_write16(base_sel, 0x60, 0xa0);
			return;
		}
		cmd_reg = 0x60;
	} else {
		cmd_reg = (uint16_t)(((uint32_t)channel * 0x20 + 0x68) & soc_irq_gate_midi_cmd_align_mask);

		if (len != 0) {
			volatile uint16_t *push = (volatile uint16_t *)
				(aemif_cs3_base_get((void *)(uintptr_t)soc_irq_gate_midi_base_arg_fw) + (uint32_t)channel * 0x20 + 0x72);

			for (; words != 0; words--)
				*push = *data++;

			if (len & 1) {
				*push = (uint16_t)(uint8_t)*data;
				midi_hw_write16(base_sel, 0x60, 0xa0);
				return;
			}
		}
	}

	trigger = 0x80;
	midi_hw_write16(base_sel, cmd_reg, trigger);
}

/* midi_hw_fifo_read - CONFIRMED byte-for-byte structurally identical to K1,
 * including the SAME `(len << 16) >> 17` word-count derivation (faithful to
 * the real decompile, equivalent to `len >> 1` only for len < 0x10000).
 * @0xc0000b50 (K1: @0xc0000dec). */
extern uint32_t soc_irq_gate_midi_base_arg_fr;	/* DAT_c0000be4, same dead-handle role */

uint32_t midi_hw_fifo_read(uint32_t base_sel, int channel, uint8_t *out, uint32_t len)
{
	volatile uint16_t *pop;
	uint32_t ret = 0;
	uint32_t words;

	(void)base_sel;

	if (channel == 0)
		pop = (volatile uint16_t *)(aemif_cs3_base_get((void *)(uintptr_t)soc_irq_gate_midi_base_arg_fr) + 0x6e);
	else
		pop = (volatile uint16_t *)(aemif_cs3_base_get((void *)(uintptr_t)soc_irq_gate_midi_base_arg_fr) + (uint32_t)channel * 0x20 + 0x72);

	if (len == 0)
		return ret;

	ret = len & 0xffff;
	words = (uint32_t)(((uint64_t)len << 16) >> 17);	/* faithful transcription, see K1's own note */

	for (; words != 0; words--) {
		uint16_t v = *pop;
		out[1] = (uint8_t)(v >> 8);
		out[0] = (uint8_t)v;
		out += 2;
	}
	if (len & 1)
		out[0] = (uint8_t)*pop;

	return ret;
}

/* ===========================================================================
 * CLUSTER 11 - USB endpoint FIFO fill-level/ring-index helper, plus a small
 * family of trivial single-word accessors in the same address neighborhood.
 * CONFIRMED structurally identical to K1's own cluster 11's own main
 * function; the "slot0x00_get" and "ring3_state_reset" siblings K1
 * documents were NOT independently re-located in K2 this pass (not swept
 * further - out of the time budget for this already-large pass).
 * =========================================================================== */

extern uint32_t usbdc_ep_regblock_ptr_a(uint32_t unused, int index);	/* out of range, not independently re-cited by any K2 file this pass (K1: FUN_c0007108) */
extern uint32_t scaled_ratio(uint32_t numerator, uint32_t denominator);	/* FUN_c001ab9c (K1: FUN_c001e300) - divide-shaped helper, not independently re-attributed here */

/* soc_irq_gate_usbdc_ring_index_update - CONFIRMED structurally identical
 * to K1's own soc_irq_gate_usbdc_ring_index_update: `commit` selects whether
 * to recompute; captures a hardware fill-level word (cached handle's own
 * +0x244 field, table+0x08 = 0xC00E0008 - a DIFFERENT confirmed use of the
 * SAME physical slot CLUSTER 1/2 also use, not a collision error - table
 * slots are genuinely time-multiplexed across this file's own sub-clusters,
 * same as K1's own table) into table+0x1c (0xC00E001C), computes a byte
 * delta against usbdc_ep_regblock_ptr_a(_, 0)'s own address, divides by the
 * SAME 0xc0-byte stride via scaled_ratio to get a slot count, clamps/wraps
 * into a 0-31 ring window exactly as K1 does, storing both an absolute
 * index (0xc00a09a4 - OUTSIDE the 0xC00E0000 table, matching K1's own
 * "distinct data-segment word" finding for this exact field) and a wrapped
 * delta (table+0x18 = 0xC00E0018). @0xc0000328 (K1: @0xc0000484). */
extern uint32_t  soc_irq_gate_ep_ring_reloc_arg;	/* DAT_c000040c, resolved 0xC01CB2EC */
extern int32_t  *soc_irq_gate_ep_ring_fill_level;	/* DAT_c0000408, table+0x1c = 0xC00E001C */
extern int32_t  *soc_irq_gate_ep_ring_index;		/* DAT_c0000400, resolved 0xC00A09A4 - OUTSIDE the 0xC00E0000 table */
extern void     *soc_irq_gate_ep_ring_hwbase;		/* DAT_c0000404, table+0x08 = 0xC00E0008 - cached hw handle, its own +0x244 field read */
extern int32_t  *soc_irq_gate_ep_ring_delta_out;	/* DAT_c0000410, table+0x18 = 0xC00E0018 */

int32_t soc_irq_gate_usbdc_ring_index_update(char commit)	/* FUN_c0000328 */
{
	int32_t idx = *soc_irq_gate_ep_ring_index;
	int32_t clamped = (idx < 0) ? 0 : idx;
	int32_t result = clamped;

	if (commit != 0) {
		if (idx >= 0) {
			int32_t fill_level = *(int32_t *)((uint8_t *)soc_irq_gate_ep_ring_hwbase + 0x244);
			uint32_t base_addr = usbdc_ep_regblock_ptr_a(soc_irq_gate_ep_ring_reloc_arg, 0);

			*soc_irq_gate_ep_ring_fill_level = fill_level;

			if ((uint32_t)*soc_irq_gate_ep_ring_fill_level < base_addr) {
				*soc_irq_gate_ep_ring_fill_level = 0;
			} else {
				int32_t byte_delta;
				int32_t slot;

				base_addr = usbdc_ep_regblock_ptr_a(soc_irq_gate_ep_ring_reloc_arg, 0);
				byte_delta = *soc_irq_gate_ep_ring_fill_level - (int32_t)base_addr;
				*soc_irq_gate_ep_ring_fill_level = byte_delta;

				slot = (int32_t)scaled_ratio((uint32_t)byte_delta, 0xc0);
				if (slot > 0x1f)
					result = 0;
				*soc_irq_gate_ep_ring_index = slot;
				if (slot > 0x1f)
					*soc_irq_gate_ep_ring_index = result;

				if (*soc_irq_gate_ep_ring_index < 0)
					*soc_irq_gate_ep_ring_index += 0x20;

				{
					int32_t wrapped = *soc_irq_gate_ep_ring_index;
					int32_t adj = wrapped;

					if (wrapped < clamped)
						adj = wrapped - clamped + 0x20;

					if (wrapped < clamped)
						*soc_irq_gate_ep_ring_delta_out = adj;
					else
						*soc_irq_gate_ep_ring_delta_out = wrapped - clamped;
				}
			}
		}
		if (result < 0)
			result += 0x20;
	}

	return result;
}

/* soc_irq_gate_slot0x0c_get / soc_irq_gate_slot0x10_get / soc_irq_gate_
 * slot0x14_clear / soc_irq_gate_slot0x14_get - four trivial single-word
 * accessor/mutator stubs in this same address neighborhood. table+0x14
 * (0xC00E0014) is CONFIRMED the SAME address for both the clear and get
 * variants below - re-confirms K1's own identical finding for its own
 * matched pair (different absolute table offset, same shared-slot pattern).
 * @0xc0000414 / @0xc0000424 / @0xc0000434 / @0xc0000448
 * (K1: @0xc0000570 / @0xc0000580 / @0xc0000590 / @0xc00005a4). */
extern uint32_t *soc_irq_gate_slot_0x0c;	/* DAT_c0000420, table+0x0c = 0xC00E000C */
uint32_t soc_irq_gate_slot0x0c_get(void)	/* FUN_c0000414 */
{
	return *soc_irq_gate_slot_0x0c;
}

extern uint32_t *soc_irq_gate_slot_0x10;	/* DAT_c0000430, table+0x10 = 0xC00E0010 */
uint32_t soc_irq_gate_slot0x10_get(void)	/* FUN_c0000424 */
{
	return *soc_irq_gate_slot_0x10;
}

extern uint32_t *soc_irq_gate_slot_0x14;	/* DAT_c0000444, table+0x14 = 0xC00E0014 */
void soc_irq_gate_slot0x14_clear(void)	/* FUN_c0000434 */
{
	*soc_irq_gate_slot_0x14 = 0;
}

extern uint32_t *soc_irq_gate_slot_0x14_r;	/* DAT_c0000454, table+0x14 = 0xC00E0014 - SAME address as soc_irq_gate_slot_0x14 above, confirmed via query_dump_k2.py dat */
uint32_t soc_irq_gate_slot0x14_get(void)	/* FUN_c0000448 */
{
	return *soc_irq_gate_slot_0x14_r;
}

/* ===========================================================================
 * CLUSTER 12 - the two "group" dispatchers that call this file's own
 * enable/disable leaves. K1's own file left its equivalent pair
 * (FUN_c001ca34/"group A", FUN_c001ca84/"group B") as bare externs, citing
 * task_sched.c as the place that names their call relationship without
 * itself defining full bodies. K2's own equivalents are DEFINED here in
 * full, since (a) neither is claimed by any other K2 file in this tree
 * (task_sched.c does not mention either address) and (b) their own bodies
 * are short, simple call-fanout leaves, not dense scheduler-primitive code.
 * =========================================================================== */

extern void soc_irq_gate_ch32_enable_gpio(void);
extern void soc_irq_gate_ch3a_enable(void);
extern void soc_irq_gate_ch35_enable(void);
extern void soc_irq_gate_ch0b_enable_mcasp_latch(void);
extern void timer64p0_enable_ch15(void);	/* FUN_c0000040, ch-0x15 enable - NOW DEFINED in soc_periph.c, see file header EXCLUDED section (2026-07-19 live-query resolution) */

/* soc_irq_gate_group_a_enable - CONFIRMED called from eva_board_crt0
 * (FUN_c0007268, eva_board_main.c) - the SAME "runs at boot" call
 * relationship K1's own file documents for its own group A. Calls, in
 * order: FUN_c0000040 (ch 0x15 enable, EXCLUDED here), ch32, ch35, ch3a,
 * ch0b-mcasp-latch - then its OWN tail is a genuine ch-0x2a EISR-enable,
 * lazy-init guarded (table+0x3c = 0xC00E003C) - CONFIRMED REAL DIFFERENCE
 * FROM K1, see REAL DIFFERENCES #2 above: K1 never found an enable side for
 * ch 0x2a at all; K2 has one, just inlined into this dispatcher rather than
 * a standalone leaf. @0xc001995c. */
extern uint8_t  *soc_irq_gate_ch2a_guard;	/* DAT_c0000760, table+0x3c = 0xC00E003C */
extern uint32_t  soc_irq_gate_slot_0x4c_j;	/* DAT_c0000764, table+0x4c = 0xC00E004C - dead phantom handle */

void soc_irq_gate_group_a_enable(void)	/* FUN_c001995c */
{
	uint32_t aintc;

	timer64p0_enable_ch15();
	soc_irq_gate_ch32_enable_gpio();
	soc_irq_gate_ch35_enable();
	soc_irq_gate_ch3a_enable();
	soc_irq_gate_ch0b_enable_mcasp_latch();

	if (*soc_irq_gate_ch2a_guard != 0)
		return;
	*soc_irq_gate_ch2a_guard = 1;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x2a;	/* EISR: enable ch 42 - see note above */
}

/* soc_irq_gate_group_b_disable - CONFIRMED reached only from a table/init-
 * list reference (xref `from_func: null` at its own call site, 0xc0019a68)
 * - the SAME "referenced from a table, not a real traced CALL instruction"
 * pattern K1's own file flags for its own soc_irq_gate_ring3_state_reset.
 * Calls, in order: ch2a, ch0b-mcasp, ch3a, ch35, ch32, ch0x15 quiesce - SIX
 * disable calls, matching K1's own group B minus the two channels K2
 * genuinely dropped (0x17, 0x36 - see REAL DIFFERENCES #1). @0xc001999c. */
void soc_irq_gate_group_b_disable(void)	/* FUN_c001999c */
{
	soc_irq_gate_ch2a_quiesce();
	soc_irq_gate_ch0b_quiesce();
	soc_irq_gate_ch3a_quiesce();
	soc_irq_gate_ch35_quiesce();
	soc_irq_gate_ch32_quiesce_gpio();
	soc_irq_gate_timer0_quiesce();
}

/* -------------------------------------------------------------------------
 * STILL OPEN
 * -------------------------------------------------------------------------
 * 2026-07-19 LIVE-QUERY PASS (dedicated single-session live Ghidra MCP
 * bridge, get_disassembly/decompile_function/get_function_info/get_xrefs_to/
 * get_xrefs_from/read_memory/search_bytes, "2-agent cap, no further fan-out"
 * authorization - see CLAUDE.md). Zero Agent-tool subagent calls made.
 * Resolved 2 of this section's own prior items, corrected 2 real errors this
 * file previously carried, and investigated (without fully resolving) the
 * remaining 2 - detail below, replacing the pre-live-query list above it.
 *
 *  - FUN_c0000040 (ch-0x15 enable) - RESOLVED. Now defined as timer64p0_
 *    enable_ch15 in soc_periph.c (get_function_info + decompile_function +
 *    get_disassembly + read_memory on 0xc0000040). See EXCLUDED above and
 *    that function's own comment in soc_periph.c for the two real
 *    corrections this also produced (this file's own prior "table+0x08"
 *    mislabel for CLUSTER 1, and board_desc_init_type5's own "no visible
 *    argument" citation).
 *  - soc_irq_gate_timer0_quiesce's own cached-handle identity - RESOLVED (in
 *    the sense of "whose base pointer is this", not the exact TRM register
 *    field at +0x44): it is the Timer64P0 base pointer, table+0x00, cached by
 *    timer64p0_enable_ch15. The exact +0x44 register FIELD's own TRM meaning,
 *    and soc_irq_gate_ch0b_quiesce's own +0x2068/+0x2070/+0x2078 register
 *    block identity, remain genuinely unresolved - not chased further this
 *    pass (would need the real OMAP-L138 Timer64P TRM, not available here).
 *  - A GENUINELY NEW, previously-undocumented code region was found while
 *    investigating the above: 0xc0000098-0xc00000df (72 bytes), sitting in
 *    the gap between timer64p0_enable_ch15 (ends 0xc0000088) and
 *    soc_irq_gate_timer0_quiesce (starts 0xc00000e0) - Ghidra has NO Function
 *    object bounding it (same "never function-ified" artifact this project
 *    has hit before), so it was manually characterized from raw disassembly/
 *    read_memory rather than decompiled. It opens with the EXACT SAME
 *    "read table+0x00, OR bit 0x2 into +0x44" idiom as soc_irq_gate_timer0_
 *    quiesce's own opening (confirmed via read_memory: its own literal at
 *    0xc00000d4 is also 0xC00E0000), then writes 0x15 DIRECTLY to a
 *    HARDCODED literal address 0xFFFEE024 (= aintc_base()+0x24, SICR - i.e.
 *    the SAME "ack ch 0x15" effect as timer0_quiesce's own SICR write, but
 *    via a baked-in constant instead of a live aintc_base() call - a real,
 *    different code-generation choice, not re-derived further), calls
 *    FUN_c0007cb8(0xC01CB2EC) - itself a tiny "if flag byte set, bump a
 *    counter and eva_wire_report_code(1,1)" gate, confirmed via decompile -
 *    then TAIL-BRANCHES (a genuine ARM `b`, not `bl`+return - confirmed via
 *    disassembly) into 0xc0019d84, a large, separately-prologued function
 *    with its own CPSR IRQ-disable critical-section entry (mrs/tst/msr),
 *    clearly outside this file's own AINTC-gate-leaf territory. get_xrefs_to
 *    on 0xc0000098 itself shows exactly one reference, a PARAM-type (not
 *    CALL-type) read from FUN_c00199dc at 0xc001a1ec - task_sched.c's own
 *    file ALREADY independently names FUN_c00199dc as "one of eva_board_
 *    crt0's own subsystem-bring-up calls" and the real caller of both
 *    kobj_table_init and sched_tcb_table_init_and_autostart's own ROM-table
 *    ROM walk. This strongly suggests 0xc0000098 is itself an entry in that
 *    SAME ROM-table mechanism (very plausibly an auto-started boot task,
 *    matching this project's own established "referenced from a table/
 *    init-list, not a real CALL instruction" pattern already seen for K1's
 *    ring3_state_reset and this file's own group_b_disable) - NOT defined
 *    here (belongs to task_sched.c's own ROM-task-table territory, out of
 *    this file's own scope, and its own tail-call target was not chased).
 *    NEEDS LIVE QUERY if a future task_sched.c pass wants to close it fully.
 *  - K1's own "gap_slot_bringup" cluster (twice-called usbdc_gap_config_
 *    slot-shaped bring-up) - INVESTIGATED, STILL NOT LOCATED. The obvious
 *    live lead (get_xrefs_to on omap_l137_addr_gap_misc.c's own claimed K2
 *    address for usbdc_gap_config_slot, 0xc0002d80) returned exactly the 2
 *    callers that file's own header predicts (FUN_c0000800 plus one
 *    unattributed continuation) - but decompiling FUN_c0000800 directly
 *    shows it is NOT a gap-slot bring-up at all: it is panelbus_hw_bringup,
 *    ALREADY fully defined in panelbus_dispatch.c, and its own call to
 *    0xc0002d80 passes an I2C1 base + a speed-mode int, matching panelbus_
 *    dispatch.c's own independently-decompiled panelbus_i2c_mode_config
 *    (an I2C ICMDR-shaped clock-divider config write) exactly - confirmed
 *    directly via decompile_function on 0xc0002d80 itself (writes +0x24/
 *    +0x30/+0xc/+0x10 register-config fields, nothing usbdc/gap-shaped at
 *    all). This means omap_l137_addr_gap_misc.c's own citation of 0xc0002d80
 *    as usbdc_gap_config_slot is a REAL CROSS-FILE ADDRESS COLLISION/
 *    MISATTRIBUTION BUG (most likely confused with the neighboring panelbus_
 *    i2c_mode_config during that file's own pass) - flagged here, NOT fixed
 *    (out of this pass's own edit scope: omap_l137_addr_gap_misc.c and
 *    panelbus_dispatch.c are neither one of this pass's assigned files).
 *    usbdc_gap_config_slot's REAL K2 address, if it exists at all, remains
 *    unlocated. NEEDS LIVE QUERY: whoever next touches omap_l137_addr_gap_
 *    misc.c should re-derive usbdc_gap_config_slot's real address from
 *    scratch rather than trusting that file's current 0xc0002d80 citation.
 *  - soc_irq_gate_slot0x00_get and soc_irq_gate_ring3_state_reset (K1's own
 *    cluster-11 tail items) - INVESTIGATED, STILL NOT LOCATED, but with more
 *    to go on now: table+0x00 (0xC00E0000) itself IS genuinely live in K2 -
 *    3 confirmed real consumers (timer64p0_enable_ch15's write; soc_irq_
 *    gate_timer0_quiesce's read; the new 0xc0000098 gap-code's read, all
 *    above) - but NONE of the 3 is a bare "return *(table+0x00);" wrapper
 *    shaped like K1's own soc_irq_gate_slot0x00_get; every K2 consumer
 *    inlines the dereference directly. A full-image search_bytes sweep for
 *    the literal 0xC00E0000 (bytes `00 00 0e c0`) found exactly these same 4
 *    occurrences project-wide (the 4th being a data-only literal inside
 *    eva_board_crt0/FUN_c0007268 at 0xc00072b0, itself just another inline
 *    read, not a wrapper) - reasonably strong (not absolute, given ARM MVN-
 *    immediate encodings could in principle hide an equivalent constant from
 *    a raw byte search) evidence no dedicated K2 getter exists at all for
 *    this slot; it was apparently simplified away the same way CLUSTER 11's
 *    own slot-getter family survived only partially. soc_irq_gate_ring3_
 *    state_reset (K1's own -1/0/0 ring-sentinel reset, at data addresses
 *    OUTSIDE the 0xC00E0000 table entirely) has no located K2 candidate at
 *    all - its own K1 sentinel value (-1) is typically built via an ARM MVN
 *    immediate rather than a literal-pool load, so the byte-search technique
 *    that worked elsewhere this pass isn't applicable; genuinely not found,
 *    not confirmed absent.
 *  - mcasp_reinit_reduced (FUN_c0002d00, see mcasp.c's own citation) and
 *    FUN_c0001bf4 (soc_irq_gate_mcasp2_bringup's own uncalled final callee,
 *    not independently identified anywhere in this project's K2 tree) -
 *    neither is defined by any K2 file, matching K1's own treatment of its
 *    equivalents as "too dense, out of scope". Not re-investigated this pass.
 *  - Whether task_sched.c's own eva_board_crt0_tcb_and_kobj_init-equivalent
 *    K2 sequence independently registers soc_irq_gate_group_a_enable/
 *    _group_b_disable the same way K1's own file speculates for FUN_c001ca34/
 *    FUN_c001ca84 - not cross-checked against task_sched.c's own K2 content
 *    this pass (task_sched.c does not currently mention either address).
 * ------------------------------------------------------------------------- */
