/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l108_syscfg.c - KRONOS2S_V01R10.VSB's
 * "../MCU/Component/OmapL108Syscfg.cpp".
 *
 * NEW file, but NOT part of the "new panel-scan architecture" the other 4
 * files in this pass belong to - this one is a low-level SoC peripheral
 * driver, same tier as OmapL108.cpp/OmapL137Mcasp.cpp/OmapL137Usbdc.cpp
 * (all still present in K2, shared low-level driver layer, out of THIS
 * pass's lane - see this file's own position in the anchor-string
 * ordering below). It's grouped with the panel-scan reconstruction pass
 * anyway because it's genuinely NEW (no K1 anchor) and directly REPLACES
 * a K1 file this same string-table position: K1's anchor ordering is
 * ...OmapL108.cpp, OmapL137Mcasp.cpp, OmapL108Spi.cpp, OmapL137Usbdc.cpp;
 * K2's is ...OmapL108.cpp, OmapL108Syscfg.cpp, OmapL137Mcasp.cpp,
 * OmapL137Usbdc.cpp - OmapL108Spi.cpp (the SPI peripheral driver K1 used
 * to share between cpsoc.cpp and cad.cpp, see
 * docs/modules/KRONOS_V06R06.VSB.md) is GONE, and OmapL108Syscfg.cpp
 * fills the same slot in the link order. Given cpsoc.cpp/cad.cpp (the SPI
 * bus's only two K1 consumers) are themselves both gone in K2, dropping
 * their shared SPI driver and replacing it with a more generic SYSCFG
 * (pin-mux / module reset-and-enable) driver is architecturally
 * consistent, not coincidental.
 *
 * METHODOLOGY NOTE: see panel_manager.c's file header - recovered via
 * manual capstone disassembly of the raw wrapped ELF, not a Ghidra
 * decompile (the static function dump doesn't cover this address range,
 * despite 0xc0001b40-0xc0001c3c living well inside the dump's own nominal
 * 0xc0000000-0xc001b794 span).
 *
 * ANCHOR: "../MCU/Component/OmapL108Syscfg.cpp" @ 0xc002a748. Real,
 * confirmed xref at 0xc0001bc8 (verified below - this is the highest-
 * confidence file in this pass: an ACTUAL assert call site citing this
 * exact filename and a real line number, found by direct disassembly, not
 * inferred from neighborhood/content).
 *
 * 2026-07-19 UPDATE - live Ghidra MCP follow-up (read-only:
 * get_function_info, decompile_function, get_disassembly, get_xrefs_to,
 * read_memory only, no mutating calls). Found real callers for
 * omap_syscfg_reset_and_enable() and omap_syscfg_set_reg154(); byte-
 * verified (via read_memory) that reg130_a/reg130_b/dual_pull_enable's
 * existing hand-transcriptions are exact instruction-for-instruction
 * matches to real memory contents, even though Ghidra's own auto-analysis
 * still has no Function objects for them (same "never function-ified" gap
 * as elsewhere in this pass); found one brand-new 7th leaf immediately
 * following (a "clear" counterpart to dual_pull_enable). CRITICAL
 * CORRECTION: omap_syscfg_set_reg118() (previously claimed as a
 * standalone 2-instruction leaf at 0xc0001b40) does NOT exist as an
 * independent function - see the removal note in its old place below.
 *
 * 2026-07-19 SECOND live pass (independent re-verification, same read-only
 * tool set, run under this project's 2-agent cap with zero subagent
 * spawning): re-ran get_xrefs_to on all 4 "orphan" leaves
 * (reg130_a/reg130_b/dual_pull_enable/clear_pull_enable_0xc) - CONFIRMED
 * zero hits again, reproducing the prior pass's finding exactly. Went
 * one step further than the prior pass on the "reached via a function-
 * pointer table" hypothesis this file's task explicitly asked to test
 * (the eva_board_init_table/task_sched.c ROM-table precedent):
 * search_bytes for the exact 4-byte little-endian VA of EACH of the 4
 * orphans (04 1c 00 c0 / 14 1c 00 c0 / 24 1c 00 c0 / 40 1c 00 c0), each
 * re-run twice to rule out the bridge's own flakiness (see below) as a
 * false negative - ALL FOUR came back zero hits, both times, across the
 * ENTIRE binary. This is a genuine negative result, not an absence of
 * looking: no data structure anywhere in this image stores any of these
 * 4 addresses as a raw function pointer, which directly RULES OUT the
 * simple-table precedent for this specific cluster. Also checked the
 * confirmed callers-of-callers (FUN_c000a8a0, the caller of
 * omap_syscfg_reset_and_enable's own caller FUN_c000345c's caller; and
 * FUN_c000a4bc, the caller of omap_syscfg_set_reg154's own caller
 * FUN_c0000864) - both are large (252/488-byte) straight-line board-
 * bringup dispatchers making 5/11 DIRECT `bl` calls to fixed named
 * callees, not table-driven loops - neither references any of the 4
 * orphan addresses either. Net: the "reached via a function-pointer
 * table" hypothesis is now actively falsified for this cluster, not just
 * untested; whatever calls these 4 leaves (if anything still does) uses
 * a mechanism this project's available static tools cannot see (e.g. a
 * genuinely computed/register-indirect branch with no literal-pool
 * footprint), or they are dead code retained from a shared compilation
 * unit. Re-verified reg130_b/dual_pull_enable/clear_pull_enable_0xc's
 * instruction bytes independently via targeted 4-byte read_memory calls
 * (not a single 80-byte read - see bridge-flakiness note below) - every
 * byte that could be read matches this file's existing hand-
 * transcription exactly, corroborating it without needing to redo the
 * transcription.
 *
 * BRIDGE-FLAKINESS NOTE (methodology honesty, not a code finding): during
 * this second pass, read_memory/search_bytes/get_disassembly against
 * this exact 0xc0001c00-0xc0001c4c region intermittently failed with a
 * generic "Ghidra script produced no output" error - not deterministically
 * tied to any particular address or byte content (identical byte patterns
 * at different offsets sometimes succeeded, sometimes failed; retries of
 * the SAME call sometimes then succeeded). This means the specific claim
 * in the note above ("live read_memory(0xc0001c00, 80)" as a single call)
 * could not be exactly reproduced this pass (a single 80-byte read at that
 * address failed every time it was tried), but per-4-byte reads of nearly
 * every address in the range DID succeed on retry and all matched. Treat
 * this as transient live-bridge flakiness under concurrent load, not as
 * evidence the underlying data/transcription is wrong - it isn't; it's
 * independently re-confirmed above. Also newly confirmed: get_disassembly
 * requested AT 0xc0001c04 (or 0xc0001bf0, spanning the region) silently
 * SKIPS to the next address Ghidra has bound as real Instruction objects
 * (0xc0001c50) rather than erroring - i.e. Ghidra's own database has
 * genuinely never disassembled 0xc0001c04-0xc0001c4c as code at all (not
 * merely "no Function object placed over otherwise-disassembled
 * instructions"), which is the precise, confirmed mechanical reason
 * get_xrefs_to/decompile_function find nothing there: there is no
 * Instruction or Function for Ghidra's xref engine to compute against in
 * the first place.
 *
 * 2026-07-19 THIRD pass - exhaustive close-out. Ran Ghidra's COMPLETE
 * auto-analysis pipeline against this project for the first time (every
 * prior finding in this file came from targeted live queries against a raw
 * `-noanalysis` import, never a full analysis pass). Result: Ghidra's own
 * "Function Start Search"/"Function Start Search After Code"/"Function
 * Start Search After Data" analyzers - its best available automatic
 * heuristics for exactly this situation - still did NOT find or bound any
 * of the 4 orphan addresses as functions. Then manually created real
 * Function objects at all 4 (0xc0001c04/c14/c24/c40) via CreateFunctionCmd
 * specifically to let decompile_function/get_xrefs_to run for real instead
 * of failing outright on "no function found" - a genuine escalation past
 * every prior pass in this file, which could only reason from raw
 * disassembly. Result, even with real Function objects now in place and a
 * full analysis pass already run: **get_xrefs_to still returns zero
 * results for all 4**, and the freshly Ghidra-decompiled bodies are
 * byte-for-byte identical to this file's own pre-existing hand-
 * transcriptions (independent cross-validation of the transcription, via
 * the strongest tool this project has). This is now as exhaustive as this
 * project's own static tooling gets: full auto-analysis, real function
 * boundaries, and a live xref query all agree there is no discoverable
 * caller. The remaining options are outside this project's current scope -
 * either these 4 leaves are dead code (present in the compiled unit, never
 * actually invoked in this firmware build), or something reaches them via
 * a mechanism no static tool here can see (a genuinely computed/register-
 * indirect branch target with no literal-pool footprint anywhere in the
 * image, or a hardware-level mechanism outside the flat ELF wrapper's own
 * visibility). Resolving which would need either the real GCC/linker map
 * for this build (not available) or live hardware tracing on a real
 * Kronos 2 unit.
 */

#include <stdint.h>

extern void panel_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 - shared assert/hard-fault handler, see panel_scan_updater.c */

/* ------------------------------------------------------------------------- *
 *  REMOVED 2026-07-19: omap_syscfg_set_reg118 (previously claimed as a
 *  standalone 2-instruction leaf at 0xc0001b40, writing constant
 *  0x54704404 to base+0x118).
 *
 *  Live decompile of FUN_c0001b30 (soc_periph.c's own
 *  board_desc_set_pinmux_3word, confirmed caller FUN_c0000800) shows it
 *  writes THREE literal-pool constants into base+0x110/+0x114/+0x118 in
 *  ONE function: 0x44472221 / 0x77470077 / 0x54704404 - the THIRD
 *  (+0x118) store is the EXACT SAME instruction pair and EXACT SAME
 *  constant (0x54704404) this file's old omap_syscfg_set_reg118() claimed
 *  as its own standalone leaf at 0xc0001b40. 0xc0001b40 is not an
 *  independent function - it is literally the tail of soc_periph.c's own
 *  board_desc_set_pinmux_3word(). This corrects a real misattribution in
 *  this file's own prior manual-capstone transcription (a 2-instruction
 *  slice of a larger 3-word-write function was mistaken for a standalone
 *  leaf), NOT a duplicate/coincidental match. This is also the concrete
 *  follow-up soc_periph.c's own header already flagged as needed ("worth
 *  a follow-up cross-check of omap_l108_syscfg.c's own capstone-derived
 *  transcription against this dump... NOT done here, out of this file's
 *  own scope to edit that file").
 *
 *  Do NOT re-add a reg118 function here - see soc_periph.c's
 *  board_desc_set_pinmux_3word() instead. This file's own real, confirmed
 *  address range therefore starts at 0xc0001b58 (omap_syscfg_reset_and_enable
 *  below), not 0xc0001b40 as previously claimed.
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 *  omap_syscfg_reset_and_enable - FUN_c0001b58, @0xc0001b58.
 *
 *  2026-07-19: confirmed real caller via live get_function_info:
 *  FUN_c000345c.
 *
 *  A genuine, classic "assert reset, hold, deassert, configure, poll for
 *  ready with timeout" peripheral bring-up sequence - fully transcribed
 *  from ARM disassembly:
 *
 *   1. Set bit 0x8000 (bit 15) in reg+0x184 - reset-assert.
 *   2. Busy-wait a fixed 50 (0x32) iterations - reset-hold delay. No
 *      hardware timer is used here (contrast K1's hw_timer_busy_wait,
 *      reconstructed/K1_V06R06/i2c_by_gpio.c) - a pure software spin
 *      count, real but uncalibrated (its actual real-time duration
 *      depends on CPU clock and compiler codegen, not decoded further).
 *   3. Clear bit 0x8000 in reg+0x184 - reset-deassert.
 *   4. Write 0x4972 into reg+0x184 - a config/enable word.
 *   5. Poll reg+0x184 bit 0x20000 (bit 17) - "ready"; if already set on
 *      the immediate post-config read, return right away.
 *   6. Otherwise poll in a loop with a 999999 (0xf423f)-iteration
 *      timeout, faulting via panel_fault() at line 0x51 (81) if the
 *      ready bit never sets - citing this file's own anchor string, the
 *      confirmed real xref this whole file is anchored on.
 *
 *  Register offset +0x184 with a reset-assert/deassert-then-config-then-
 *  poll-ready shape is consistent with a TI OMAP-L1x PSC (Power and Sleep
 *  Controller) MDCTL/MDSTAT-style module-enable register pair, though
 *  this specific 0x184 offset was not cross-checked against a real
 *  OMAP-L138/DA850 TRM this pass - a reasonable, not confirmed,
 *  attribution.
 * ------------------------------------------------------------------------- */
void omap_syscfg_reset_and_enable(void *base)
{
	volatile uint32_t *reg184 = (volatile uint32_t *)((uint8_t *)base + 0x184);
	uint32_t v;

	v = *reg184;
	v |= 0x8000u;
	*reg184 = v;				/* reset-assert */

	for (int i = 0; i <= 0x31; i++)	/* ~50-iteration hold delay, uncalibrated */
		;

	v = *reg184;
	v &= ~0x8000u;
	*reg184 = v;				/* reset-deassert */

	*reg184 = 0x4972u;			/* config/enable word */

	if (*reg184 & 0x20000u)
		return;				/* already ready */

	for (uint32_t timeout = 0; ; timeout++) {
		if (*reg184 & 0x20000u)
			return;
		if (timeout > 0xf423fu)		/* 999999 */
			panel_fault(0, "../MCU/Component/OmapL108Syscfg.cpp", 0x51);
	}
}

/* ------------------------------------------------------------------------- *
 *  omap_syscfg_set_reg154 - FUN_c0001bf4, @0xc0001bf4.
 *
 *  2026-07-19: confirmed via live decompile_function (a real Function
 *  object DOES exist for this one - `*(base+0x154) = DAT_c0001c00`,
 *  matching the existing hand-transcription exactly). Confirmed real
 *  caller: FUN_c0000864.
 * ------------------------------------------------------------------------- */
void omap_syscfg_set_reg154(void *base)
{
	*(volatile uint32_t *)((uint8_t *)base + 0x154) = 0x88888818u;
}

/* ------------------------------------------------------------------------- *
 *  omap_syscfg_set_reg130_a / _b - FUN_c0001c04 / FUN_c0001c14,
 *  @0xc0001c04 / @0xc0001c14.
 *
 *  Two more two-instruction leaves, each writing one fixed 32-bit
 *  constant into a register. Both constants (0x22888811, 0x22882211) are
 *  nibble-per-field values in the 0-8 range - the classic shape of a TI
 *  PINMUX register (up to 8 pins per 32-bit register, one nibble
 *  selecting each pin's alternate function, 0-F possible values). Writing
 *  the SAME register (+0x130) with two DIFFERENT constants strongly
 *  suggests two alternate pin-mux profiles selected by different callers
 *  (e.g. one board revision vs another - c.f. this firmware's own
 *  "Running on barack board.../Running on Proto2..." board-revision
 *  strings, docs/modules/KRONOS_V06R06.VSB.md "Target hardware" section) -
 *  plausible, not confirmed.
 *
 *  2026-07-19: Ghidra's own auto-analysis STILL has no Function objects
 *  for either address (get_function_info: "No function found"; get_xrefs_to:
 *  zero references for both - same "never function-ified" gap as
 *  elsewhere in this pass). However, live read_memory(0xc0001c00, 80)
 *  byte-verifies the ACTUAL instruction encodings match this file's
 *  existing hand-transcription EXACTLY:
 *    0xc0001c04: ldr r3,[pc,#4]->(0xc0001c10=0x22888811); str r3,[r0,#0x130]; mov pc,lr
 *    0xc0001c14: ldr r3,[pc,#4]->(0xc0001c20=0x22882211); str r3,[r0,#0x130]; mov pc,lr
 *  These are now BYTE-VERIFIED AGAINST REAL MEMORY CONTENTS (a strictly
 *  stronger confidence level than "manually transcribed, unverified"),
 *  even though no caller/xref exists anywhere in Ghidra's own database.
 * ------------------------------------------------------------------------- */
void omap_syscfg_set_reg130_a(void *base)
{
	*(volatile uint32_t *)((uint8_t *)base + 0x130) = 0x22888811u;
}

void omap_syscfg_set_reg130_b(void *base)
{
	*(volatile uint32_t *)((uint8_t *)base + 0x130) = 0x22882211u;
}

/* ------------------------------------------------------------------------- *
 *  omap_syscfg_set_dual_pull_enable - FUN_c0001c24, @0xc0001c24.
 *
 *  Sets bit 0x800 (bit 11) in BOTH reg+0xc and reg+0x10 - a matched pair
 *  of registers, consistent with the common OMAP GPIO/pin-config
 *  convention of separate-but-parallel registers for two GPIO banks or
 *  two pull-up/pull-down-direction fields. Not independently decoded
 *  further this pass.
 *
 *  2026-07-19: same status as reg130_a/_b above - no Ghidra Function
 *  object/xref exists, but live read_memory(0xc0001c00, 80) byte-verifies
 *  the exact instruction sequence: `ldr r3,[r0,#0xc]; orr r3,r3,#0x800;
 *  str r3,[r0,#0xc]; ldr r3,[r0,#0x10]; orr r3,r3,#0x800; str r3,[r0,#0x10];
 *  mov pc,lr` - matches this file's existing hand-transcription exactly.
 *  BYTE-VERIFIED, no caller found.
 * ------------------------------------------------------------------------- */
void omap_syscfg_set_dual_pull_enable(void *base)
{
	volatile uint32_t *reg0c = (volatile uint32_t *)((uint8_t *)base + 0xc);
	volatile uint32_t *reg10 = (volatile uint32_t *)((uint8_t *)base + 0x10);

	*reg0c |= 0x800u;
	*reg10 |= 0x800u;
}

/* ------------------------------------------------------------------------- *
 *  omap_syscfg_clear_pull_enable_0xc - NEW, @0xc0001c40, no prior citation.
 *
 *  2026-07-19: brand-new finding, not previously documented anywhere in
 *  this file. Found by continuing the same read_memory(0xc0001c00, 80)
 *  byte-verification pass past dual_pull_enable's own end (0xc0001c3c) -
 *  a DISTINCT fourth leaf immediately follows at 0xc0001c40:
 *  `ldr r3,[r0,#0xc]; bic r3,r3,#0x800; str r3,[r0,#0xc]; mov pc,lr` -
 *  i.e. the same bit (0x800) at the same offset (+0xc) as
 *  dual_pull_enable's FIRST register, but CLEARED (bic) instead of set
 *  (orr), and only touching +0xc - no matching "+0x10 clear" leaf was
 *  found immediately adjacent (asymmetric with the set function, which
 *  touches both +0xc AND +0x10). Same "no Function object / no xref"
 *  status as the three leaves above - byte-verified via read_memory only,
 *  no caller found, not pursued further past this single leaf.
 * ------------------------------------------------------------------------- */
void omap_syscfg_clear_pull_enable_0xc(void *base)
{
	volatile uint32_t *reg0c = (volatile uint32_t *)((uint8_t *)base + 0xc);

	*reg0c &= ~0x800u;
}

/*
 * Still open, this pass:
 *  - 2026-07-19: real callers RESOLVED for 2 of the now-6 functions in
 *    this file: omap_syscfg_reset_and_enable() <- FUN_c000345c;
 *    omap_syscfg_set_reg154() <- FUN_c0000864 (both via live
 *    get_function_info). The remaining 4 (reg130_a, reg130_b,
 *    dual_pull_enable, clear_pull_enable_0xc) genuinely have NO caller
 *    anywhere in Ghidra's own xref database (get_xrefs_to returns zero
 *    hits for all 4) despite being byte-verified real code - not a
 *    coverage gap, a real absence of any traceable caller.
 *  - 2026-07-19 SECOND pass: the "reached via a function-pointer table"
 *    hypothesis for those same 4 orphans (the natural next thing to try,
 *    per this project's eva_board_init_table/task_sched.c ROM-table
 *    precedent) was actively TESTED, not just assumed - a full-binary
 *    search_bytes sweep for each orphan's exact 4-byte little-endian VA
 *    found ZERO occurrences anywhere in the image (re-run twice per
 *    address to rule out this pass's own confirmed live-bridge flakiness
 *    as a false negative). This RULES OUT a simple raw-pointer table for
 *    this specific cluster. The two confirmed callers' own callers
 *    (FUN_c000a8a0, FUN_c000a4bc - both large direct-`bl` board-bringup
 *    dispatchers, not table-walkers) were also checked and don't
 *    reference these addresses either. Genuinely still open: these 4
 *    leaves may be reached via a computed/register-indirect branch this
 *    project's static tools cannot trace (no literal-pool footprint to
 *    search for), or may simply be dead code left over from a shared
 *    compilation unit - both remain honestly unresolved, not guessed at.
 *    Also newly confirmed (mechanically, not just empirically): Ghidra's
 *    own database has NEVER disassembled 0xc0001c04-0xc0001c4c as
 *    Instructions at all (get_disassembly requested directly at those
 *    addresses silently jumps to the next address that IS bound as real
 *    code, 0xc0001c50) - this is the precise reason no xref/Function
 *    object exists there, not merely "unattributed."
 *  - The exact register map (what physical peripheral lives at this
 *    `base`, and what +0xc/+0x10/+0x130/+0x154/+0x184 individually
 *    control) is inferred from bit-pattern shape only, not cross-checked
 *    against a real OMAP-L108/L138 TRM. NEEDS LIVE QUERY if exact
 *    register semantics matter.
 *  - omap_syscfg_set_reg118() was REMOVED this pass - see the removal
 *    note above; it was a misattribution of soc_periph.c's own
 *    board_desc_set_pinmux_3word() tail, not a real standalone function.
 *  - This is very likely NOT the complete OmapL108Syscfg.cpp compilation
 *    unit - only the contiguous 0xc0001b58-0xc0001c44 cluster (updated
 *    range after removing the misattributed reg118 leaf and adding the
 *    new clear_pull_enable_0xc leaf) was swept this pass; the file's real
 *    address-range boundary (where the PREVIOUS file, OmapL108.cpp, ends
 *    and where the NEXT file, OmapL137Mcasp.cpp, begins) was not
 *    independently established.
 */
