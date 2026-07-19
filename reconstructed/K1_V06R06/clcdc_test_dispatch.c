/* SPDX-License-Identifier: GPL-2.0 */
/*
 * clcdc_test_dispatch.c - the factory test-menu keypress dispatcher
 * (`clcdc_test_pattern_dispatch`, FUN_c001123c) and its two private
 * one-line helper setters (FUN_c001120c / FUN_c001121c).
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, range sweep
 * 0xc0010f60-0xc001150c, 2026-07-18.
 *
 * ANCHOR: real `__FILE__` anchor is `"../clcdc.cpp"` (string @0xc0023bac) -
 * the SAME anchor as clcdc.c. This is genuinely clcdc.cpp's own code:
 * clcdc.c's own header comment for `clcdc_test_pattern` (@0xc00154e8)
 * already documents FUN_c001123c as "called from FUN_c001123c" and as the
 * dispatcher's one real evidence tying it to clcdc.cpp (mode 5's
 * clcdc_assert(0, "../clcdc.cpp", <line>) call). cpsoc.c independently
 * cites the same three functions and even claims (at its own line ~776)
 * that FUN_c001123c is "already reconstructed in clcdc.c" - that claim is
 * WRONG as of this pass: clcdc.c discusses FUN_c001123c/c001120c/c001121c
 * only in prose (see its own comment block above clcdc_test_pattern,
 * "Item 6 confirmation"), it never actually defines compilable bodies for
 * any of the three. This file supplies those bodies for the first time.
 *
 * WHY A SEPARATE FILE INSTEAD OF EDITING clcdc.c: this sweep's own
 * collision-avoidance rule forbids touching any pre-existing file, and
 * clcdc.c already exists with substantial unrelated content. Physically
 * these three functions belong in clcdc.c (same anchor, same compilation
 * unit) - flagged in the final report as a follow-up merge, not done here.
 *
 * The functions physically sit inside cpsoc.c's own documented address
 * range (0xc0010f00-0xc00117ff) but are explicitly excluded there as
 * "false neighbors" - see cpsoc.c's own "CONFIRMED OUT OF SCOPE" note for
 * FUN_c001120c/FUN_c001121c, and its extern declaration/citation of
 * FUN_c001123c as `clcdc_test_pattern_dispatch`, "see clcdc.c". Function
 * and variable names here are kept consistent with cpsoc.c's own citations
 * wherever cpsoc.c already named something (the LED-set/-clear pair, the
 * dispatcher's own name) - see each function's own comment below.
 */

#include <stdint.h>

/* --- cross-file externs, cpsoc.c (@0xc0010fb8 / @0xc0010fe8) --- */
extern void cpsoc_led_set(void *cpsoc, int led_index);	/* FUN_c0010fb8 */
extern void cpsoc_led_clear(void *cpsoc, int led_index);	/* FUN_c0010fe8 */

/* --- cross-file extern, clcdc.c (@0xc00154e8) --- */
extern void clcdc_test_pattern(int mode);	/* FUN_c00154e8 */

/* ------------------------------------------------------------------------- *
 *  clcdc_test_pattern_led_cache - `*(byte *)0xc0098f9f` (DAT_c0011370 here).
 *  Holds the currently-lit test-pattern-menu LED tag byte (range 0xd-0x13,
 *  one value per test-pattern mode 0-6 - see clcdc_test_pattern_dispatch
 *  below). RESOLVED this pass: this address is the 4th byte of the SAME
 *  4-byte contiguous LED-cache block cpsoc.c already documents 3 bytes of -
 *  `cpsoc_analog_led_cache` (0xc0098f9c), `cpsoc_led_ramp_position`
 *  (0xc0098f9d), `cpsoc_led_quantize_cache` (0xc0098f9e) - making this a
 *  4-channel cache block, not 3, though this 4th slot belongs to
 *  clcdc.cpp's own test-menu handler rather than one of cpsoc.c's own
 *  LED-bargraph channels. Not renamed/moved into cpsoc.c's own block
 *  (different compilation unit, same discipline as the rest of this file).
 */
extern uint8_t clcdc_test_pattern_led_cache;	/* *0xc0098f9f (DAT_c0011370) */

/* ------------------------------------------------------------------------- *
 *  cpsoc_analog_reset_arg / cpsoc_analog_table_index_raw / cpsoc_analog_
 *  init_arg - the three globals FUN_c001120c/FUN_c001121c/
 *  clcdc_test_pattern_dispatch itself write into: 0xc01cd4f8, 0xc01cd4fc,
 *  0xc01cd500 respectively (three consecutive 4-byte slots).
 *
 *  CROSS-FILE FINDING (new this pass): these are the EXACT SAME THREE
 *  ADDRESSES cpsoc.c's own analog-polling code (cpsoc_analog_poll_channel
 *  neighborhood, @0xc0011534-ish) already names and uses:
 *    - cpsoc.c: `extern int cpsoc_analog_reset_arg;` "*DAT_c0011608
 *      (pointer itself at 0xc01cd4f8)"
 *    - cpsoc.c: `extern int *cpsoc_analog_table_index_ptr;` "0xc01cd4fc
 *      (DAT_c0011614) - a POINTER TO the index variable... dereferenced
 *      TWICE (`lookup_table[*index_ptr]`)"
 *    - cpsoc.c: `extern int cpsoc_analog_init_arg;` "*DAT_c0011610
 *      (pointer itself at 0xc01cd500)"
 *  i.e. the factory test-menu key codes handled by THIS file
 *  (0x18-0x1e -> FUN_c001120c, 0x3a-0x3d -> FUN_c001121c, 0x42-0x49 ->
 *  direct store) are, concretely, the menu's way of configuring/poking
 *  cpsoc.c's own analog-channel-polling subsystem - not generic "UI state"
 *  as an earlier pass's placeholder comment guessed. Kept as separate
 *  extern declarations here (own compilation unit) rather than sharing
 *  cpsoc.c's exact declarations, per this project's established
 *  cross-file-duplication convention.
 *
 *  APPARENT UNRESOLVED CONFLICT flagged, not fixed: cpsoc.c's own comment
 *  insists 0xc01cd4fc holds a POINTER that gets dereferenced twice by its
 *  analog-polling code. clcdc_test_pattern_dispatch below writes a bare
 *  small integer (`uVar5 - 0x42`, range 0-7) directly into that same
 *  4-byte slot with a single, non-pointer store - never sets it up as a
 *  valid pointer first. Either cpsoc.c's "pointer, dereferenced twice"
 *  reading of its own decompile is a misattribution (plausible: a raw
 *  0-7 index is exactly the shape of a `lookup_table[index]` selector, no
 *  indirection needed), or this test-menu code path genuinely stomps a
 *  live pointer with a small integer and would crash cpsoc.c's analog
 *  poller if it ever ran right after - a real firmware-quirk-or-bug
 *  candidate worth a live re-check, not resolved here without editing
 *  cpsoc.c to compare notes directly.
 */
extern int cpsoc_analog_reset_arg;		/* 0xc01cd4f8 (DAT_c0011218 here / DAT_c0011608 in cpsoc.c) */
extern int cpsoc_analog_table_index_raw;	/* 0xc01cd4fc (DAT_c0011238 here / DAT_c0011614 in cpsoc.c, see conflict note above) */
extern int cpsoc_analog_init_arg;		/* 0xc01cd500 (DAT_c0011228 here / DAT_c0011610 in cpsoc.c) */

/* ------------------------------------------------------------------------- *
 *  FUN_c001120c / FUN_c001121c - one-line global setters, each written by
 *  exactly one call site inside clcdc_test_pattern_dispatch below (per
 *  cpsoc.c's own xrefs_to-confirmed "1 caller each" note). Real decompiled
 *  shape:
 *
 *      void FUN_c001120c(undefined4 param_1, undefined4 param_2)
 *      { *DAT_c0011218 = param_2; return; }   // writes cpsoc_analog_reset_arg
 *
 *      void FUN_c001121c(undefined4 param_1, undefined4 param_2)
 *      { *DAT_c0011228 = param_2; return; }   // writes cpsoc_analog_init_arg
 *
 *  param_1 (the cpsoc/opcode-context handle forwarded by the caller) is
 *  read nowhere in either body - dead, same "phantom forwarded parameter"
 *  pattern already documented independently in cdix4192.c
 *  (cdix_reg_write/cdix_reg_read) and eva_board_main.c
 *  (eva_board_watchdog_fault_wrapper).
 *
 *  param_2 IS genuinely open, and more open than the usual phantom-param
 *  case: clcdc_test_pattern_dispatch's own two call sites below
 *  (`FUN_c001120c(param_1)` / `FUN_c001121c(param_1)`) show only ONE
 *  visible argument each in the decompile, yet both callees read a second
 *  parameter. No disassembly access in this pass (decompiled_c only, per
 *  this sweep's data-access constraint) to confirm what, if anything, is
 *  actually live in the second argument register at each call site - left
 *  as an explicit "value written is unknown, NOT fabricated" gap rather
 *  than guessing a plausible-looking constant. @0xc001120c, @0xc001121c.
 */
void clcdc_analog_reset_arg_set(void *cpsoc_unused, int value)	/* FUN_c001120c */
{
	(void)cpsoc_unused;
	/* CAUTION: real call site below passes only 1 visible argument;
	 * `value` here reflects the callee's own declared 2nd parameter, not
	 * a confirmed caller-supplied value - see header comment. */
	cpsoc_analog_reset_arg = value;
}

void clcdc_analog_init_arg_set(void *cpsoc_unused, int value)		/* FUN_c001121c */
{
	(void)cpsoc_unused;
	/* Same caveat as clcdc_analog_reset_arg_set above. */
	cpsoc_analog_init_arg = value;
}

/* ------------------------------------------------------------------------- *
 *  clcdc_test_pattern_dispatch - the factory test-menu keypress handler,
 *  named to match cpsoc.c's own extern citation of it
 *  (`extern void clcdc_test_pattern_dispatch(void *cpsoc);`). Reads a
 *  key-code byte from the shared cpsoc scratch struct at offset 0x821 (the
 *  SAME offset cpsoc.c's own cpsoc_diag_menu_input/opcode-dispatch code
 *  already established as the shared command/tag payload byte), then:
 *
 *    - unconditionally lights an LED at `((key-1) % 0x10) + 0x21` - a
 *      generic "flash the LED for whatever menu button was just pressed"
 *      feedback step, using the exact same [0x21,0x30] indexing formula
 *      cpsoc.c's own cpsoc_led_cycle documents for its own, unrelated LED.
 *    - key 0x1f-0x25 (7 values): selects test-pattern mode 0-6
 *      (clcdc_test_pattern), clears the PREVIOUS test-mode LED
 *      (cpsoc_led_clear on the old clcdc_test_pattern_led_cache value)
 *      before running the new mode, then sets the NEW mode's LED
 *      (cpsoc_led_set) and caches its tag byte (0xd-0x13) for next time.
 *    - key 0x18-0x1e (7 values): forwards to clcdc_analog_reset_arg_set
 *      (see its own header comment for the real cpsoc.c cross-link).
 *    - key 0x3a-0x3d (4 values): forwards to clcdc_analog_init_arg_set
 *      (same cross-link).
 *    - key 0x42-0x49 (8 values): writes `key - 0x42` (0-7) directly into
 *      cpsoc_analog_table_index_raw and returns early WITHOUT running any
 *      of the LED-set/-clear logic above (real early `return param_1;` in
 *      the decompile - the only branch that skips the final LED-set call).
 *
 *  Return value: the real decompile threads a captured return value
 *  (`iVar2`) from cpsoc_led_set/cpsoc_led_clear through to this function's
 *  own return - but cpsoc.c declares both of those `void`. Same
 *  void-vs-captured-return inconsistency the project's own README already
 *  documents for clcdc_draw_text; not resolved here for the same reason
 *  (different compilation units, no live cross-check in this pass).
 *  Modeled here as `void` to match cpsoc.c's own callee signatures
 *  (cpsoc.c's two call sites at its own lines ~803/~828 already call this
 *  function and discard any result, consistent with `void`).
 *
 *  @0xc001123c (316 bytes), key-code byte offset 0x821. Callers (both
 *  inside cpsoc.c, per xrefs_to): FUN_c0011374 (cpsoc_tag_router_c) and
 *  FUN_c00113d0 (cpsoc_tag_router_a), both unconditional calls.
 */
void clcdc_test_pattern_dispatch(void *cpsoc)		/* FUN_c001123c */
{
	/* Real decompile widens the key byte to `uint` (uVar5) BEFORE any
	 * arithmetic, so `key - 1` at key==0 wraps to 0xffffffff and
	 * `% 0x10` yields 15 (-> LED 0x30), not the -1/-1 a plain
	 * `uint8_t` subtraction would give under C's signed-int promotion.
	 * `key` kept as `unsigned int` throughout this function to preserve
	 * that exact wraparound, not just for style. */
	unsigned int key = *(uint8_t *)((uint8_t *)cpsoc + 0x821);

	/* Unconditional "button pressed" LED feedback flash, every key code. */
	cpsoc_led_set(cpsoc, (int)((key - 1) % 0x10) + 0x21);

	if (key - 0x1f <= 6) {
		uint8_t new_tag;
		int     mode;

		cpsoc_led_clear(cpsoc, clcdc_test_pattern_led_cache);

		switch (key) {
		case 0x1f: mode = 0; new_tag = 0x0d; break;
		case 0x20: mode = 1; new_tag = 0x0e; break;
		case 0x21: mode = 2; new_tag = 0x0f; break;
		case 0x22: mode = 3; new_tag = 0x0f; break;	/* real decompile: both 0x21 and
								 * 0x22 fall through to the same
								 * `goto LAB_c00112d4` new_tag=0xf
								 * store - only `mode` differs
								 * (2 vs 3); transcribed verbatim,
								 * not simplified away. */
		case 0x23: mode = 4; new_tag = 0x11; break;
		case 0x24: mode = 5; new_tag = 0x12; break;
		default:   mode = 6; new_tag = 0x13; break;	/* key == 0x25 */
		}

		clcdc_test_pattern(mode);
		clcdc_test_pattern_led_cache = new_tag;
		cpsoc_led_set(cpsoc, clcdc_test_pattern_led_cache);
	}

	if (key - 0x18 < 7)
		clcdc_analog_reset_arg_set(cpsoc, 0 /* see clcdc_analog_reset_arg_set's own header - value not confirmed */);

	if (key - 0x3a < 4)
		clcdc_analog_init_arg_set(cpsoc, 0 /* see clcdc_analog_init_arg_set's own header - value not confirmed */);

	if (key - 0x42 < 8) {
		cpsoc_analog_table_index_raw = key - 0x42;
		return;		/* real early return - skips nothing further below since
				 * this is already the last statement, but matches the
				 * decompile's own explicit early `return param_1;` here
				 * rather than falling through implicitly. */
	}
}
