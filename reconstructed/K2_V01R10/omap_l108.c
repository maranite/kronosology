/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l108.c - KRONOS2S_V01R10.VSB (Kronos 2 / "KRONOS II") port of the K1
 * reconstruction at K1_V06R06/omap_l108.c. Same subsystem: the SoC-level
 * free-running tick-counter API.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB,
 * 2026-07-18 (query_dump_k2.py, not the live Ghidra MCP bridge - see
 * cobjectmgr.c's own header for why).
 *
 * Anchor: "../MCU/OmapL108.cpp" string at K2 0xc002a734 (K1: 0xc0022d0c).
 * Located via its literal-pool DAT_ holder (same technique as cobjectmgr.c):
 * signed data_value -0x3ffd58cc matched exactly one DAT_ constant in the K2
 * dump, 0xc0001938, referenced from exactly one function - omap_tick_init's
 * own init-guard fault call, same as K1's single anchor xref.
 *
 * The four low-level tick primitives (config_ptr/read_raw/init/
 * elapsed_scaled) and the shared division/scale helper are all confirmed
 * BYTE-FOR-BYTE structurally identical to K1 - see each function's own note.
 *
 * GENUINELY OPEN, not resolved this pass: cad_delay_ticks and
 * cad_calibration_progress_pump (K1's own FUN_c00085a8/FUN_c0005a1c) could
 * NOT be confidently mapped to any K2 function - see the "NOT FOUND IN K2"
 * section at the end of this file for the full search trail. Flagged as
 * NEEDS LIVE QUERY rather than guessed at.
 */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 (K1: FUN_c000919c) - see cobjectmgr.c's own confirmation note */

/* ------------------------------------------------------------------------- *
 *  omap_tick_config_ptr - K2 @0xc00016bc (K1 @0xc000193c). Confirmed
 *  identical trivial accessor: `return DAT_c00016c4;` (K1: DAT_c0001944).
 *  Same "role unclear, most likely a fixed base/rate constant, not an active
 *  reconfiguration call" caveat as K1 - not re-resolved here.
 * ------------------------------------------------------------------------- */
extern uint32_t omap_tick_config_ptr(void);	/* FUN_c00016bc, returns DAT_c00016c4 verbatim */

/* ------------------------------------------------------------------------- *
 *  omap_tick_read_raw - K2 @0xc0001b28 (K1 @0xc0001d68). Confirmed identical:
 *  `return *(uint32_t *)(handle + 0x10);` - same offset, same plain
 *  register/memory read, no side effects.
 * ------------------------------------------------------------------------- */
uint32_t omap_tick_read_raw(void *handle)	/* FUN_c0001b28 */
{
	return *(uint32_t *)((uint8_t *)handle + 0x10);
}

/* ------------------------------------------------------------------------- *
 *  omap_tick_init - the confirmed anchor. K2 @0xc00018f4 (K1 @0xc0001b38).
 *  Confirmed structurally identical to K1: same init-once hard-fault guard,
 *  same omap_tick_config_ptr call, same baseline snapshot into offset +4.
 *
 *  GENUINELY OPEN, differs from K1: this function has ZERO static callers
 *  anywhere in the K2 decompile set (checked both via xrefs_to and a raw
 *  text search for "FUN_c00018f4" across every function's decompiled_c).
 *  omap_tick_elapsed_scaled (below) is in the same position - also zero
 *  static callers. K1's own equivalents were reached via cad_delay_ticks,
 *  which itself could not be located in K2 (see this file's own "NOT FOUND
 *  IN K2" section at the end) - consistent with, but not proof of, this
 *  whole tick-service API being either genuinely unused in K2's build or
 *  reached only through an indirect/task-table call this static dump's xref
 *  sweep does not see (the same "zero static callers" shape cobjectmgr.c's
 *  own cobjectmgr_object_destroy has, there confirmed to be reached via
 *  vtable dispatch).
 * ------------------------------------------------------------------------- */
void omap_tick_init(void *handle)	/* FUN_c00018f4 */
{
	uint8_t *h = (uint8_t *)handle;

	if (*h != 0)
		crypto_at88_fault(0, 0 /* DAT_c0001938, "../MCU/OmapL108.cpp" */, 0 /* DAT_c000193c */);

	*h = 1;
	omap_tick_config_ptr();
	*(uint32_t *)(h + 4) = omap_tick_read_raw(handle);
}

/* ------------------------------------------------------------------------- *
 *  omap_tick_elapsed_scaled - K2 @0xc000194c (K1 @0xc0001b8c). Confirmed
 *  structurally identical to K1: re-fetches the config-ptr constant,
 *  re-reads the raw counter, computes elapsed-since-baseline with the same
 *  wraparound correction (K2's wrap constant is DAT_c000198c; K1's was
 *  DAT_c0001bcc - different literal-pool address, not verified to hold the
 *  same numeric value), then scales through the same shared divide-by-150
 *  helper (omap_tick_scale, divisor 0x96 - identical divisor to K1).
 *
 *  Same "zero static callers in K2" open question as omap_tick_init, see its
 *  note above.
 * ------------------------------------------------------------------------- */
extern int32_t omap_tick_scale(int32_t ticks, int divisor);	/* FUN_c001ac94 (K1: FUN_c001e3f8) */

int32_t omap_tick_elapsed_scaled(void *handle)		/* FUN_c000194c */
{
	uint8_t *h = (uint8_t *)handle;
	extern int32_t omap_tick_wrap_const;	/* DAT_c000198c (K1: DAT_c0001bcc) */
	int32_t now, elapsed;

	omap_tick_config_ptr();
	now = omap_tick_read_raw(handle);
	elapsed = now - *(int32_t *)(h + 4);
	if (now < *(int32_t *)(h + 4))
		elapsed += omap_tick_wrap_const;

	return omap_tick_scale(elapsed, 0x96);
}

/* ------------------------------------------------------------------------- *
 * omap_tick_scale (K1: FUN_c001e3f8, "clcdc_progress_bar's fixed-point
 * scaling math", divisor 150/0x96 here per K1's own cross-file note) - K2
 * @0xc001ac94. NOT a simple fixed-point multiply-scale as K1's name implies:
 * K2's decompile shows this is a full, general-purpose 32-bit SIGNED
 * DIVISION routine (magnitude/sign split, power-of-two fast path, and a
 * shift-and-subtract long-division fallback for the general case) - i.e.
 * this is the compiler-emitted software integer-division primitive
 * (ARM926EJ-S has no hardware integer divide instruction), not
 * cobjectmgr/omap_l108-specific logic. K1's own file already flagged
 * FUN_c001e3f8 as "a SECOND, unrelated caller of the exact same function"
 * clcdc_progress_bar uses (100 there, 150 here) - K2 reinforces that
 * "shared, firmware-wide, not subsystem-specific" conclusion further: this
 * K2 address has 7 distinct callers (FUN_c000185c, FUN_c000d6a0,
 * FUN_c0008018, FUN_c0006700 x2, FUN_c0009954, plus this file's own
 * omap_tick_elapsed_scaled), spanning subsystems well outside anything this
 * file or clcdc.c own. NOT transcribed here (generic compiler-generated
 * division code, not firmware logic) - modeled as a bodyless extern, same
 * treatment K1 already gave it.
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 * NOT FOUND IN K2 this pass - NEEDS LIVE QUERY:
 *
 * cad_delay_ticks (K1's own name for FUN_c00085a8, @0xc00085a8) and
 * cad_calibration_progress_pump (K1's own name for FUN_c0005a1c,
 * @0xc0005a1c) could NOT be confidently mapped to any K2 function. Search
 * trail:
 *  - omap_tick_init has zero static callers in K2 (see its own note above) -
 *    cad_delay_ticks's defining behaviour in K1 ("bounds the delay, arms a
 *    tick baseline via omap_tick_init, then loops") has no candidate call
 *    site to start from.
 *  - cobjectmgr_tick's own K2 caller (FUN_c000a6dc, see cobjectmgr.c's own
 *    note) is a BARE infinite loop with nothing else in it - not a
 *    delay-bounded loop that also services a calibration pump, so it is NOT
 *    cad_delay_ticks despite also calling cobjectmgr_tick.
 *  - A different function, FUN_c000185c (@0xc000185c, 136 bytes, 17 static
 *    callers), performs superficially similar work - it calls
 *    omap_tick_config_ptr/omap_tick_read_raw directly (inlined, not via a
 *    sub-call to omap_tick_elapsed_scaled) in a loop, feeding omap_tick_scale
 *    - but its shape does NOT match cad_delay_ticks: no omap_tick_init call,
 *    no cobjectmgr_tick call, no calibration-pump call, and 17 callers is far
 *    too broad for what K1 documents as a cad.cpp-specific calibration delay
 *    helper. Left unmapped rather than forced onto cad_delay_ticks's name;
 *    its real identity is a generic "delay N scaled ticks" primitive at
 *    best, not confirmed.
 *  - cad_calibration_progress_pump's own distinguishing callees
 *    (cad_calibration_pop_changed, cad_calibration_slot_is_raw,
 *    cad_pedal_present - all cad.c-owned, out of this file's scope to
 *    resolve independently) were not searched for directly; a text sweep for
 *    the pump's distinctive screen-column literal (0x17c) turned up 6
 *    candidate K2 functions (0xc00047a8, 0xc0004b00, 0xc0007454, 0xc000cba4,
 *    0xc000fb30, 0xc000fd0c) but none was decompiled/verified this pass -
 *    left as an open lead for a future pass rather than guessed at.
 *
 * Whoever needs cad_delay_ticks/cad_calibration_progress_pump's K2 addresses
 * (this project's own cad.c, if/when ported) should treat both as
 * unresolved.
 * ------------------------------------------------------------------------- */
