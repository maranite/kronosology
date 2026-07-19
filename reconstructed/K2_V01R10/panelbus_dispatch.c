/* SPDX-License-Identifier: GPL-2.0 */
/*
 * panelbus_dispatch.c - KRONOS2S_V01R10.VSB (Kronos 2) port of the K1
 * reconstruction at K1_V06R06/panelbus_dispatch.c: the second, on-chip
 * OMAP-L138 I2C0/I2C1 hardware controller (distinct from i2c_by_gpio.c's
 * bit-banged bus) K1 built an entire per-tick RX/TX opcode dispatcher on top
 * of, feeding cad.c's calibration handlers and a device-ID negotiation
 * handshake.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json), read via query_dump_k2.py - no
 * live Ghidra MCP calls this pass. Migration pass from K1_V06R06's already-
 * done reconstruction.
 *
 * ============================================================================
 * HEADLINE FINDING: K1's higher-level dispatcher (RX poll loop, TX ring
 * drain, the opcode-table command interpreter) HAS NO CONFIRMED K2
 * COUNTERPART. This is a real, evidence-based architecture finding, not a
 * coverage gap - see below.
 * ============================================================================
 *
 * K1's own panelbus_dispatch.c is built entirely on top of two now-gone K1
 * subsystems: cad.c (calibration - `cad_trigger_calibration`/`cad_trim_
 * adjust`, fed via ports 0x78/0x79/0x7a) and cpsoc.c's own "third SPI
 * device" (the same +0x08/+0x14/+0x1c/+0x24 register shape, per K1's own
 * cross-file finding). K2_V01R10/README.md's own top-level architecture
 * note ALREADY establishes that BOTH cpsoc.cpp and cad.cpp are entirely
 * absent from K2's string table, replaced by a genuinely redesigned panel-
 * scan architecture (PanelManager.cpp/PanelScanUpdater.cpp/
 * SwitchOnChatteringDetector.cpp/SystemInfoHolder.cpp) - none of which K1's
 * own I2C0/I2C1-based opcode dispatcher has any confirmed relationship to.
 * This pass independently confirms the CONSEQUENCE of that architecture
 * change for this specific hardware peripheral, by direct evidence, not by
 * inference from the README alone:
 *
 *  - The K2 I2C0/I2C1 base selector itself DOES survive, structurally
 *    unchanged (2-way selector, dead first argument, same two literal base
 *    addresses 0x01E28000/0x01C22000) - but it is ALREADY fully defined in
 *    soc_periph.c as `i2c0_i2c1_base_select` (FUN_c0001780, @0xc0001780,
 *    K1: @0xc0001a00) as part of that file's own migration pass. NOT
 *    redefined here - see soc_periph.c's own header/body for the real code.
 *  - A full xref sweep of i2c0_i2c1_base_select's own `callers` list in the
 *    K2 dump finds exactly TWO call sites, both inside a single crt0 board
 *    bring-up stub (see below) - NOT a per-tick-polled RX/TX dispatcher
 *    cluster the way K1's own file's 10 functions were. soc_periph.c's own
 *    header already flags this exact gap ("i2c0_i2c1_base_select's real
 *    consumption beyond the one traced caller each - not fully swept") -
 *    this file's own investigation closes that specific open item: there is
 *    no further consumption to find. The hardware is initialized once at
 *    boot and, as far as this static dump's own xref data shows, never
 *    touched again.
 *  - No function anywhere in the K2 dump was found calling `cad_trigger_
 *    calibration`- or `cad_trim_adjust`-shaped externs (unsurprising - no
 *    cad.c-equivalent file exists in this K2 tree at all, consistent with
 *    the README's own "cad.cpp entirely absent" finding), and a broad text
 *    sweep for the K1-shaped blocking-I2C-read primitive (busy-wait on a
 *    status register, per-byte 1000-spin poll, a trailing STOP-bit OR) found
 *    no K2 candidate either.
 *
 * CONCLUSION: whatever the on-chip I2C0/I2C1 controller does for K2's own
 * redesigned panel-scan architecture, it is NOT the elaborate per-tick
 * opcode dispatcher K1 built on it - either the new architecture doesn't use
 * this specific hardware block for its own runtime traffic at all (most
 * consistent with the evidence: PanelScanUpdater.cpp's own confirmed bus is
 * SPI, per soc_periph.c's own "SPI SURVIVES" finding, not this I2C0/I2C1
 * block), or its real K2 consumer sits outside the address ranges swept by
 * this static dump (the same "Ghidra never turned this into Function
 * objects" failure mode this project's own 2026-07-18 panel-scan pass
 * already documented for PanelManager.cpp/PanelScanUpdater.cpp's own code).
 * Not force-fit into either explanation without more evidence.
 *
 * What IS reconstructed below is the one real, confirmed consumer this pass
 * actually found: the crt0-time hardware bring-up sequence.
 */

#include <stdint.h>

extern uint32_t i2c0_i2c1_base_select(void *chip, int idx);	/* FUN_c0001780, soc_periph.c - NOT redefined here, see file header */
extern uint32_t gpio_bank_get_base(void);			/* FUN_c0001710, soc_periph.c - real call site passes a dead handle arg the real (no-parameter) callee ignores, phantom-forward idiom */
extern uint32_t ecap1_base_get(void *chip);			/* FUN_c0001824, soc_periph.c */
extern uint32_t ecap2_base_get(void *chip);			/* FUN_c0001830, soc_periph.c */
extern uint32_t syscfg0_base_get(void *chip);			/* FUN_c00016c8, soc_periph.c */
extern void     board_desc_set_pinmux_3word(int handle);	/* FUN_c0001b30, soc_periph.c - real call site here shows NO visible argument, phantom-forward */

/* ------------------------------------------------------------------------- *
 *  panelbus_i2c_mode_config - configures an OMAP-L138 I2C controller's
 *  ICMDR-shaped mode register (+0x24, bits 0x4000/0x20 set unconditionally -
 *  plausibly FREE + IRS/reset-clear, not independently decoded against the
 *  TRM this pass) and a clock/prescaler-shaped field trio (+0xc/+0x10/+0x30)
 *  whose two branches (`speed_mode == 0` vs nonzero) select between two
 *  fixed constant sets (0x18/0x18/1 vs 0x14/0x14/5) - consistent with an I2C
 *  bit-rate or address-width mode select, not independently confirmed. Takes
 *  the CONTROLLER BASE POINTER directly as param_1 (not a `chip` handle
 *  indirection the way most of this project's shared primitives do).
 *  @0xc0002d80. NO K1 COUNTERPART - this function's own address range (and
 *  the crt0 bring-up sequence it's called from, below) sit entirely outside
 *  K1's own panelbus_dispatch.c's swept range; not attributable to any K1
 *  function this pass found.
 * ------------------------------------------------------------------------- */
void panelbus_i2c_mode_config(uintptr_t i2c_base, int speed_mode)	/* FUN_c0002d80 */
{
	uint32_t *base = (uint32_t *)i2c_base;

	base[0x24 / 4] = 0;
	if (speed_mode == 0) {
		base[0x30 / 4] = 1;
		base[0x0c / 4] = 0x18;
		base[0x10 / 4] = 0x18;
	} else {
		base[0x30 / 4] = 5;
		base[0x0c / 4] = 0x14;
		base[0x10 / 4] = 0x14;
	}
	base[0x24 / 4] |= 0x4000;
	base[0x24 / 4] |= 0x20;
}

/* ------------------------------------------------------------------------- *
 *  panelbus_hw_bringup - CONFIRMED one of eva_board_crt0's own eleven
 *  subsystem bring-up calls (eva_board_main.c's own header lists this
 *  address, "not individually traced this pass" there - resolved here for
 *  the first time). Ends by selecting I2C1 (idx=1 - NOTE: this is the
 *  OPPOSITE selector value from every one of K1's own panelbus_dispatch.c
 *  call sites, which exclusively used idx=0/I2C0 - a real, confirmed
 *  difference, though whether this reflects a genuinely different physical
 *  bus assignment on the K2 board or simply a different, unrelated hardware
 *  consumer of this shared selector is NOT resolved) and feeding the result
 *  into panelbus_i2c_mode_config(_, 1) above.
 *
 *  2026-07-19 SUPERSEDES the "unbounded continuation" note this file
 *  previously carried (which speculated 0xc00008d4-0xc0000904 might be part
 *  of THIS function's own body, "whether one function repeats itself or two
 *  distinct functions is left open"). That question is now definitively
 *  resolved, and the earlier framing was wrong: get_function_info(FUN_c0000800)
 *  reports size=108 (0xc0000800-0xc000086c) - a clean, self-contained
 *  function that tail-merges only with the tiny shared `*p=0;return;`
 *  fragment at 0xc00018e8 (confirmed via live disassembly: `mov r3,#0;
 *  strb r3,[r0,#0]; mov pc,lr`, exactly matching this body's own trailing
 *  `*puVar1 = 0;` statement below) - it does NOT reach anywhere near
 *  0xc00008cc.
 *
 *  Running DisassembleCommand/CreateFunctionCmd at 0xc00008cc (see this
 *  project's K2_V01R10/omap_l137_addr_gap_misc.c, Cluster 2's own updated
 *  writeup, for the disassembly-level detail) proves the region previously
 *  called "the continuation" is in fact a wholly SEPARATE, independent
 *  function - see panelbus_hw_bringup_unreached below. It is a true sibling
 *  of this function (same phantom-handle/gpio_bank_get_base/gpio_bank_hw_init/
 *  i2c0_i2c1_base_select(idx=1)/panelbus_i2c_mode_config(mode=1) shape), not
 *  a second half of it.
 *
 *  This also CORRECTS this file's own former "get_function_info
 *  (i2c0_i2c1_base_select) lists exactly ONE caller ... DEFINITIVELY
 *  closes ... no other consumer" claim: i2c0_i2c1_base_select in fact has
 *  TWO real callers, confirmed once the FUN_c00008cc boundary existed for
 *  Ghidra's own xref engine to attribute the second call site to instead of
 *  silently folding it into this function's name. See
 *  panelbus_hw_bringup_unreached's own comment for why this doesn't change
 *  the file's overall architectural conclusion. @0xc0000800.
 * ------------------------------------------------------------------------- */
extern void gpio_bank_hw_init(void *bank_base);	/* FUN_c0001ffc, omap_gpio.c - CONFIRMED: that file's own header independently names THIS EXACT call site ("Sole caller: FUN_c0000800") as its one and only K2 caller. Real call site here shows NO visible argument (phantom-forward, same idiom as elsewhere) */
extern void FUN_c0004ee0(void);	/* not independently attributed this pass, called twice */

void panelbus_hw_bringup(void)	/* FUN_c0000800, @0xc0000800 - CONFIRMED one of eva_board_crt0's 11 calls */
{
	uint8_t *handle = 0;	/* real value: DAT_c0000860, table+0x4c-shaped dead phantom handle - see soc_irq_gate.c's own header for this project-wide idiom; reused here as a REAL argument to several calls below, not just discarded, matching K1's own "not independently confirmed dead at every use site" caveat */
	uint32_t i2c1_base;

	(void)syscfg0_base_get((void *)handle);
	board_desc_set_pinmux_3word(0 /* phantom-forward, no visible arg at real call site */);
	(void)gpio_bank_get_base();	/* real call site passes `handle`, real callee ignores it (no-parameter signature) */
	gpio_bank_hw_init(0 /* phantom-forward, no visible arg at real call site - see extern decl note */);
	(void)ecap1_base_get((void *)handle);
	(void)FUN_c0004ee0();
	(void)ecap2_base_get((void *)handle);
	(void)FUN_c0004ee0();

	i2c1_base = i2c0_i2c1_base_select((void *)handle, 1);	/* idx=1 -> I2C1 (0x01E28000) - see note above on the selector-value difference from K1 */
	panelbus_i2c_mode_config(i2c1_base, 1);

	*handle = 0;	/* tail-merged shared fragment @0xc00018e8: `*p = 0; return;` - see header comment */
}

/* ------------------------------------------------------------------------- *
 *  panelbus_hw_bringup_unreached - FUN_c00008cc, @0xc00008cc (52 bytes:
 *  0xc00008cc-0xc00008ff). Boundary manually created 2026-07-19
 *  (DisassembleCommand had already run here - the bytes were valid
 *  instructions, just never grouped into a Function by either the
 *  -noanalysis sweep this project used throughout or a subsequent full
 *  auto-analysis pass).
 *
 *  A near-exact structural sibling of panelbus_hw_bringup above: same
 *  phantom-handle load, same gpio_bank_get_base()/gpio_bank_hw_init(0) pair,
 *  same i2c0_i2c1_base_select(handle,1) / panelbus_i2c_mode_config(_,1)
 *  tail. This IS the code this file previously (incorrectly) transcribed as
 *  extra statements appended to panelbus_hw_bringup's own body - see that
 *  function's header comment for the correction. It genuinely is a second,
 *  independent function, not a continuation.
 *
 *  UNLIKE panelbus_hw_bringup, this function has NO CONFIRMED CALLER
 *  anywhere in the K2 image: get_xrefs_to found zero references to
 *  0xc00008cc, and a raw byte-pattern search of the entire 917504-byte
 *  wrapped image for its address as a little-endian literal (cc 08 00 c0)
 *  also found zero hits - ruling out an untyped function-pointer table
 *  entry (the same technique that DID find a real table reference for
 *  omap_l137_addr_gap_misc.c's own Cluster 4 case). Genuinely dead/unreached
 *  code in this build, not a coverage gap.
 *
 *  This refines rather than overturns the file's own architectural
 *  conclusion above: i2c0_i2c1_base_select has two call sites in the
 *  static call graph, but only ONE (panelbus_hw_bringup, called from
 *  eva_board_crt0 via FUN_c0007268) is actually reachable at runtime. There
 *  is still no live K2 dispatcher consuming this hardware beyond boot-time
 *  bring-up - this second copy is inert, not a hidden second consumer.
 *  @0xc00008cc. */
void panelbus_hw_bringup_unreached(void)	/* FUN_c00008cc - dead code, no confirmed caller, see comment above */
{
	uint8_t *handle = 0;	/* real value: DAT_c0000900, same phantom-handle idiom as panelbus_hw_bringup */
	uint32_t i2c1_base;

	(void)gpio_bank_get_base();
	gpio_bank_hw_init(0);
	i2c1_base = i2c0_i2c1_base_select((void *)handle, 1);
	panelbus_i2c_mode_config(i2c1_base, 1);	/* real tail call (b, not bl+mov pc,lr) */
}

/* -------------------------------------------------------------------------
 * STILL OPEN
 * -------------------------------------------------------------------------
 *  - The higher-level RX/TX opcode dispatcher itself (K1's
 *    panelbus_rx_dispatch_loop/panelbus_tx_drain_channel/panelbus_cmd_
 *    dispatch, ~7 more functions) - CONFIRMED to have no located K2
 *    counterpart this pass, see the file header's own HEADLINE FINDING.
 *    2026-07-19 RE-RESOLVED after creating the panelbus_hw_bringup_unreached
 *    (FUN_c00008cc) boundary: i2c0_i2c1_base_select actually has TWO callers,
 *    not one - but the second, panelbus_hw_bringup_unreached, is itself
 *    unreached code (no confirmed caller anywhere in the image, see that
 *    function's own comment). Net conclusion UNCHANGED: there is no live
 *    K2 dispatcher consuming this hardware at runtime beyond the one boot-time
 *    bring-up call. Whether the dead second copy was removed outright with
 *    cpsoc.cpp/cad.cpp, or the runtime consumer moved to a subsystem that
 *    doesn't touch this I2C hardware block at all (most likely, per
 *    soc_periph.c's own "SPI SURVIVES" finding for the real panel-scan bus),
 *    remains the open architectural question - just no longer an open "did
 *    we look hard enough, or miscount the callers" question.
 *  - panelbus_hw_bringup's own former "unbounded continuation" claim -
 *    CORRECTED 2026-07-19: it was never part of this function's body at
 *    all. See panelbus_hw_bringup's own header comment and
 *    panelbus_hw_bringup_unreached above for the real, corrected account.
 *  - FUN_c0001ffc/FUN_c0004ee0 (called from panelbus_hw_bringup, no visible
 *    arguments at their real call sites - phantom-forward idiom) - not
 *    independently attributed to any file in this project's K2 tree yet.
 *  - Whether this file's own name is still the right one for what little
 *    survives here (a boot-time I2C0/I2C1 hardware bring-up stub, not a
 *    dispatcher) - kept for continuity with K1's own file/module naming and
 *    because the underlying hardware block is the same one K1's own
 *    panelbus_dispatch.c is named for, not because a dispatcher was found.
 * ------------------------------------------------------------------------- */
