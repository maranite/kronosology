/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_addr_gap_misc.c - K2 (KRONOS2S_V01R10.VSB / "KRONOS II") port of
 * K1_V06R06/omap_l137_addr_gap_misc.c: a grab-bag of small functions that sit
 * near the USB-device-controller/McASP address territory but belong to
 * different, unrelated subsystems - K1's own file makes no claim these form
 * one coherent compilation unit, and neither does this one.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB
 * (query_dump_k2.py), 2026-07-19, cross-checked with a raw capstone/byte
 * search of the wrapped ELF image (kronos2s_v01r10_panel.elf) for cluster 3
 * (see that cluster's own note below) - no live Ghidra MCP calls this pass.
 *
 * COVERAGE SUMMARY, this pass - of K1's 6 clusters:
 *   Cluster 1 (McASP2 reduced-reinit)      -> FOUND, ported, EXACT size match x3
 *   Cluster 2 (usbdc_gap_config_slot)      -> FOUND, ported, structurally identical
 *   Cluster 3 (UART-shaped register pair)  -> CONFIRMED ABSENT (see below - a real
 *                                              finding, not a coverage gap)
 *   Cluster 4 (two tiny bit-extraction helpers) -> FOUND, ported, plus one new
 *                                              K2-only setter sibling
 *   Cluster 5 (clcdc default palette loader)     -> FOUND, ported, one cross-build
 *                                              confirmation of a K1 open question
 *   Cluster 6 (large struct-zero-init, mcasp)    -> FOUND, ported, EXACT size match
 *
 * Every cluster below was independently re-matched against K2's own dump -
 * addresses, sizes, and constants were NOT assumed from K1's own citations.
 */

#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 * Cluster 1 (0xc0002c6c-0xc0002d54) - K2 counterpart of K1's second McASP
 * instance reduced init/reconfigure path (K1 cluster 1, 0xc0003194-0xc0003228).
 *
 * mcasp.c's own K2 port (this tree) already cites and confirms this exact
 * cluster in its own "STILL OPEN" section ("mcasp_reinit_reduced (K2
 * FUN_c0002d00) ... Deliberately NOT reconstructed as a callable function
 * body here, matching K1's own choice, to avoid asserting an unconfirmed
 * file attribution") - this file supplies the bodies mcasp.c deliberately
 * left out, exactly the same collision-avoidance split K1's own two files
 * used for the identical reason. mcasp.c is NOT edited here.
 *
 * All three functions confirmed EXACT byte-size matches against K1 (28/28,
 * 28/28, 84/84 bytes) with numerically IDENTICAL constants throughout
 * (0x2000000, 0xfdffffff, 0x8000, 0xffff7fff) - a clean, unmodified port.
 *
 * mcasp2_reduced_init's sole caller: FUN_c0000864 (K1: FUN_c0000aa4), call
 * site 0xc000088c (K1: 0xc0000acc) - not reconstructed here, same as K1.
 * =========================================================================== */

/* mcasp2_set_bit25 / mcasp2_set_bit15 - K2 FUN_c0002c6c/FUN_c0002c88 (K1:
 * FUN_c0003194/FUN_c00031b0). Same OR/AND-a-single-bit-into-ma+0x18 shape.
 *
 * REAL, CONFIRMED K2-SPECIFIC FINDING (new versus K1's own file, which found
 * these two helpers called only from mcasp2_reduced_init and two out-of-range
 * functions it never traced): K2's own static dump directly covers TWO
 * further caller pairs K1 could not see:
 *   - FUN_c0010258 (@0xc0010258): calls mcasp2_set_bit25(*p,1) then
 *     mcasp2_set_bit15(*p,1) - both "enable" - then sets a flag word to 1.
 *   - FUN_c0010380 (@0xc0010380): calls mcasp2_set_bit15(*p,0) near its own
 *     start and mcasp2_set_bit25(*p,0) at its own end - both "disable" -
 *     bracketing a CDIX format-reset call (cdix_set_format_reg, see this
 *     tree's own cdix_autoswitch.c) and two delay loops in between.
 * Both FUN_c0010258 and FUN_c0010380 are themselves the K2 counterparts of
 * K1's own CDIX "outer state machine" functions (K1 FUN_c000f0c8/
 * FUN_c000f01c, per K1's own cdix_autoswitch.c) - see that file's own
 * updated "THE OUTER STATE MACHINE" section in this K2 tree for the full
 * writeup of this real, confirmed CDIX/McASP2 coupling. Neither caller is
 * reconstructed here (out of this file's own scope) - cited only, per this
 * project's established practice of documenting real cross-file connections
 * without expanding a given pass's own file boundaries.
 * @0xc0002c6c / @0xc0002c88. */
void mcasp2_set_bit25(void *ma, int enable)	/* FUN_c0002c6c */
{
	uint32_t *flags = (uint32_t *)((uint8_t *)ma + 0x18);
	if (enable == 0)
		*flags &= 0xfdffffff;
	else
		*flags |= 0x2000000;
}

void mcasp2_set_bit15(void *ma, int enable)	/* FUN_c0002c88 */
{
	uint32_t *flags = (uint32_t *)((uint8_t *)ma + 0x18);
	if (enable == 0)
		*flags |= 0x8000;
	else
		*flags &= 0xffff7fff;
}

/* mcasp2_reduced_init - zeroes ma+0x44/0x60/0xa0/0x14 (same 4 fields
 * mcasp.c's own mcasp_init sets up on the first McASP instance) and, if
 * `sub_config` is non-NULL, calls mcasp_configure_clock/mcasp_configure_pins
 * on it. Confirmed structurally identical to K1, including the same
 * "configure_clock called with the visible argument dropped in the K2
 * decompile" artifact mcasp.c's own header already flagged for this exact
 * call (same r0-reuse pattern as mcasp_init's own clock-step calls).
 * @0xc0002d00 (84 bytes, EXACT match with K1's FUN_c0003228). */
extern void mcasp_configure_clock(void *cfg);	/* FUN_c0002130, mcasp.c (K1: FUN_c0002658) */
extern void mcasp_configure_pins(void *cfg);	/* FUN_c0002140, mcasp.c (K1: FUN_c0002668) */

void mcasp2_reduced_init(void *ma, void *sub_config)	/* FUN_c0002d00 */
{
	uint8_t *m = (uint8_t *)ma;

	mcasp2_set_bit25(ma, 1);
	mcasp2_set_bit15(ma, 1);
	*(uint32_t *)(m + 0x44) = 0;
	*(uint32_t *)(m + 0x60) = 0;
	*(uint32_t *)(m + 0xa0) = 0;
	*(uint32_t *)(m + 0x14) = 0;
	if (sub_config == 0)
		return;
	mcasp_configure_clock(sub_config);
	mcasp_configure_pins(sub_config);
}

/* ===========================================================================
 * Cluster 2 (0xc0002d80) - K2 counterpart of K1's unattributed small
 * config-slot selector (K1 cluster 2, 0xc00032a8).
 *
 * Still genuinely NOT confirmed to belong to any subsystem this K2 tree has
 * anchored so far, same as K1 - retained as generically named. Confirmed
 * body IDENTICAL to K1's, statement-for-statement (same +0x24/+0x30/+0xc/
 * +0x10 field layout, same mode==0 -> {1,0x18,0x18} vs. else -> {5,0x14,0x14}
 * branch values, same trailing |=0x4000 / |=0x20 pair).
 *
 * Callers (per xrefs_to, 2 total): FUN_c0000800 (call site 0xc0000850,
 * mode=1) and a second call site (0xc00008fc, mode=1) whose containing
 * function object did not exist in the K2 static dump (from_func == None).
 *
 * RESOLVED 2026-07-19 (full auto-analysis + manual CreateFunctionCmd pass):
 * 0xc00008fc is not a bare call site at all - it is `b 0xc0002d80` (an
 * unconditional BRANCH, not `bl`), i.e. a tail call at the very end of a
 * real, well-formed function whose entry Ghidra's analysis (both the
 * -noanalysis sweep used throughout this project and a subsequent full
 * auto-analysis pass) never bounded. Manually created the boundary at
 * 0xc00008cc (stmdb sp!,{r4,r11,r12,lr,pc} prologue, matching frame shape
 * exactly) - decompiles cleanly as a two-call setup (gpio_bank_get_base
 * variants FUN_c0001710/FUN_c0001ffc, then FUN_c0001780) before the
 * mode=1 tail call into this function, structurally a direct sibling of
 * FUN_c0000800/FUN_c0000864/FUN_c000094c (same cpy r12,sp;stmdb
 * sp!,{...,lr,pc} shape, same fixed-handle-argument pattern).
 *
 * The sibling comparison is what makes this genuinely interesting: each of
 * FUN_c0000800, FUN_c0000864, and FUN_c000094c has exactly ONE external
 * caller, each from a DIFFERENT, otherwise-unrelated dispatcher
 * (FUN_c0007268, FUN_c000a4bc, FUN_c000cf20 respectively - all UNCONDITIONAL_CALL,
 * real `bl` instructions, not a shared jump table). FUN_c00008cc itself,
 * despite matching their shape byte-for-byte in structure, has NO such
 * caller anywhere: zero xrefs_to hits, and a raw byte-pattern search of the
 * entire 917504-byte wrapped image for its address as a little-endian
 * literal (cc 08 00 c0) also returned zero hits - ruling out an
 * untyped/unrecognized function-pointer table entry, the same technique
 * that DID resolve the Cluster 4 case below. This is now a CONFIRMED,
 * exhaustive dead end in the same category as K1's omap_l108_syscfg.c
 * orphans: the code exists, is well-formed, and is structurally identical
 * to three functions that ARE reachable, but nothing in this K2 binary
 * actually calls it. Full reconstruction with a real name and body lives in
 * this tree's own panelbus_dispatch.c (panelbus_hw_bringup_unreached) since
 * its other two calls (i2c0_i2c1_base_select, panelbus_i2c_mode_config -
 * this file's own usbdc_gap_config_slot, SAME address 0xc0002d80, two
 * names for the same function per each file's own independent attribution)
 * are that file's own primitives - not duplicated here to avoid two
 * conflicting bodies for one address.
 *
 * FUN_c0000800's own body calls gpio_bank_get_base (FUN_c0001710,
 * i2c_by_gpio.c) once, alongside several other small handle-scoped helper
 * calls (FUN_c00016c8/FUN_c0001824/FUN_c0001830/FUN_c0001780, all taking
 * the same fixed handle DAT_c0000860) before this one - weaker evidence
 * than K1's own "heavily" characterization of its analogous caller
 * (FUN_c0000a20), but the same circumstantial "GPIO/hardware-bring-up-
 * adjacent" flavor K1's own file already flagged as real but not strong
 * enough for a confident subsystem attribution. @0xc0002d80. */
void usbdc_gap_config_slot(void *obj, int mode)	/* FUN_c0002d80 - name is a location label,
							   NOT a confirmed subsystem attribution, same as K1 */
{
	uint8_t *o = (uint8_t *)obj;

	*(uint32_t *)(o + 0x24) = 0;
	if (mode == 0) {
		*(uint32_t *)(o + 0x30) = 1;
		*(uint32_t *)(o + 0xc) = 0x18;
		*(uint32_t *)(o + 0x10) = 0x18;
	} else {
		*(uint32_t *)(o + 0x30) = 5;
		*(uint32_t *)(o + 0xc) = 0x14;
		*(uint32_t *)(o + 0x10) = 0x14;
	}
	*(uint32_t *)(o + 0x24) |= 0x4000;
	*(uint32_t *)(o + 0x24) |= 0x20;
}

/* ===========================================================================
 * Cluster 3 - K1's UART-shaped register block writer/status-poll pair
 * (uart_gap_configure @0xc00034fc, uart_gap_read_status @0xc000366c) -
 * CONFIRMED ABSENT from K2. A real finding, not a coverage gap.
 *
 * K1's uart_gap_configure is keyed on several distinctive literal
 * immediates that would have to appear as literal-pool words anywhere the
 * function's compiled code lives, regardless of whether Ghidra's
 * auto-analysis turned that code into a "Function" object: the 3-way mode
 * selector (0xe00/0xe01/0xf01) and the baud/divisor-shaped pair
 * (0x4a10/0x14a10). A raw byte-pattern search of the ENTIRE wrapped ELF
 * image (kronos2s_v01r10_panel.elf, the full 0xC0000000-0xC00E0000 span,
 * i.e. well beyond the static dump's own 0xc0000000-0xc001b794 function
 * coverage) for all four of these 32-bit little-endian literal words found
 * ZERO occurrences of any of them, anywhere in the 917504-byte image. This
 * directly rules out both candidate explanations K1's own file left open
 * (a live-bridge coverage gap, or the function simply not being
 * Ghidra-function-ified the way PanelManager.cpp's own cluster is) - the
 * exact register values this K1 driver used are not present in K2's binary
 * at all, in code OR data.
 *
 * uart_gap_read_status's own signature constant (a 30-retry poll against a
 * status halfword, checking bit 0x40000000) was not independently searched
 * for (a 3-retry-count/status-bit combination is too generic to search for
 * confidently as raw bytes without a specific literal to anchor on) - but
 * given its sole reason for existing in K1 is as uart_gap_configure's own
 * status-poll counterpart (both cited from the same three out-of-range
 * K1 callers, FUN_c00136c0/FUN_c0013d2c/FUN_c0011534), and the configure
 * side is conclusively gone, this pass treats the pair as gone together
 * rather than searching further for a orphaned status-poll leaf with no
 * remaining configure-side caller to make sense of.
 *
 * WHAT THIS MEANS: whatever full-duplex serial peripheral K1's firmware
 * build talked to via this register shape (distinct from I2C-by-GPIO, the
 * hardware I2C0/I2C1 panelbus_dispatch.c documents, SPI, and USB - see K1's
 * own file header) is not configured by K2's firmware at all, at least not
 * through this register layout. Whether K2 dropped this peripheral's use
 * entirely, or reconfigures equivalent hardware through a completely
 * different register shape/base address this pass's targeted literal
 * search would not find, is NOT resolved - genuinely open, not smoothed
 * into either conclusion.
 * =========================================================================== */

/* ===========================================================================
 * Cluster 4 (0xc000320c, 0xc0003230, 0xc000325c) - K2 counterpart of K1's
 * two tiny, unrelated bit-extraction helpers (K1 cluster 4, 0xc0003734/
 * 0xc0003784), PLUS one K2-only addition.
 *
 * Both K1 helpers are confirmed present, structurally identical
 * (`(*(obj+8)>>1)&7` and `*obj&0xff`), still with single, context-free
 * callers this pass could not attribute to any subsystem - same as K1.
 * =========================================================================== */

/* @0xc000320c (16 bytes), sole caller has NO containing function object in
 * the K2 static dump (from_func == None, call site 0xc00005b8) - same
 * unresolved-boundary artifact as cluster 2's second caller above.
 *
 * RESOLVED 2026-07-19: unlike cluster 2's case, the bytes at the real
 * function entry (0xc0000594) had never been disassembled by Ghidra AT ALL
 * (not even as raw Instructions) - the linear sweep stopped at the
 * preceding function's `ldmia sp,{r11,sp,pc}` return and never resumed
 * until the loop head at 0xc00005b0, which is mid-function, not an entry
 * point. Ran DisassembleCommand at 0xc0000594 followed by CreateFunctionCmd
 * - decompiles as a clean state-handler: stores state id 0x35 into its
 * object, then loops calling gap_extract_bits_1_3 on a fixed handle and
 * dispatching FUN_c0008be8/FUN_c0008b98/FUN_c0008b8c by the result (cases
 * 1/2,6/3), falling through on any other value.
 *
 * Unlike FUN_c00008cc above, this one has a CONFIRMED real caller: a raw
 * byte search for its address as a little-endian literal (94 05 00 c0)
 * found exactly one hit, at 0xc002a648, sitting inside a small
 * function-pointer/state-id table (12-byte rows, {ptr, state_id, 0}) never
 * typed as such by Ghidra (hence no auto-xref):
 *   {0xc0000768, 50}, {0xc000069c, 53}, {0xc0000594, 58}, {0xc00004dc, 6}
 * i.e. FUN_c0000594 is state 58 (0x3a) in the same state-id scheme its OWN
 * body writes (0x35/0x3a constants appear throughout this sibling group -
 * see FUN_c0000500/FUN_c0000540 immediately before it in the image, which
 * write the same 0x3a/0x35 pair into analogous object fields). The table
 * itself (0xc002a648 and its neighbors) and whatever outer dispatcher walks
 * it by state id are not traced here - real, concrete context, but out of
 * this file's own scope. */
uint32_t gap_extract_bits_1_3(void *obj)	/* FUN_c000320c */
{
	return (*(uint32_t *)((uint8_t *)obj + 8) >> 1) & 7;
}

/* @0xc000325c (12 bytes), sole caller FUN_c00117c8 (call site 0xc00117e4) -
 * a 292-byte function sitting just before clcdc.c's own confirmed K2 range
 * (clcdc_cursor_set_stride starts at 0xc0011f34) - not itself reconstructed
 * or attributed to clcdc.cpp, cited only. */
uint32_t gap_extract_low_byte(uint32_t *obj)	/* FUN_c000325c */
{
	return *obj & 0xff;
}

/* gap_store_low_byte - K2-ONLY, no K1 counterpart was documented in
 * K1_V06R06/omap_l137_addr_gap_misc.c (either genuinely absent from K1's
 * build, or present but not found by that pass's own sweep). The obvious
 * write-side counterpart to gap_extract_low_byte above: `*obj = value &
 * 0xff`. Sole caller: FUN_c00110e8 (call site 0xc0011104), itself called
 * from 4 sites inside FUN_c0011210 - all four addresses sit inside the
 * 0xc0010e00-0xc0011400 neighborhood this K2 tree's own cdix4192.c/
 * cdix_autoswitch.c cluster occupies, but FUN_c0011210/FUN_c00110e8 are
 * NOT part of either of those files' own confirmed anchor ranges - cited
 * only, not claimed for either file. @0xc0003230 (12 bytes). */
void gap_store_low_byte(uint32_t *obj, uint32_t value)	/* FUN_c0003230 */
{
	*obj = value & 0xff;
}

/* ===========================================================================
 * Cluster 5 (0xc0003288-0xc0003400) - K2 counterpart of K1's default
 * 256-entry RGB565 palette loader (K1 cluster 5, 0xc00037b0-0xc000383c).
 *
 * clcdc.c's own K2 port (this tree) already cites and uses FUN_c0003288
 * directly - "clcdc_dispatch_set_palette_hook ... a one-line wrapper ...
 * over a shared RGB->RGB565 palette-entry-set primitive (K2's counterpart
 * of K1's FUN_c00037b0, NOT part of clcdc.cpp, owning file not traced here
 * either)" - this file supplies that reconstruction for the first time in
 * this K2 tree, exactly mirroring K1's own cross-file split.
 * =========================================================================== */

/* clcdc_palette_lut - the 256-entry RGB565 lookup table, K2 address
 * 0xc01ca9a4 (DAT_c00032b4, K1: DAT_c00037dc/0xc0023... this K2 tree's own
 * clcdc.c cites a DIFFERENT, later-stage runtime palette pointer,
 * clcdc_palette - *DAT_c0012340 - dereferenced through a pointer; this is
 * the raw FIXED backing array the defaults loader below writes into
 * directly, not independently confirmed to be the SAME memory clcdc.c's
 * own runtime-dereferenced clcdc_palette ultimately points at, same
 * "strongly suggested, not confirmed by a live pointer read" caveat K1's
 * own file carried for the analogous K1 pair. */
extern uint16_t clcdc_palette_lut[256];	/* DAT_c00032b4 = 0xc01ca9a4 */

/* clcdc_palette_set_entry - lut[index] = RGB565(r,g,b), bounds-checked
 * (index < 0x100). Confirmed structurally identical to K1's FUN_c00037b0,
 * EXACT 44/44-byte size match (independently re-verified against K1's own
 * dump this pass, not assumed) - same RGB565 packing formula,
 * `(g>>2)<<5 | (b>>3)<<11 | (r>>3)`, numerically identical to K1.
 * Two confirmed callers: clcdc.c's own clcdc_dispatch_set_palette_hook
 * (K2 FUN_c0011f3c, opcode 0xc5's handler) and clcdc_palette_load_defaults
 * below. @0xc0003288. */
void clcdc_palette_set_entry(void *lut_unused, int index, int r, int g, int b)	/* FUN_c0003288 */
{
	(void)lut_unused;	/* real store always targets clcdc_palette_lut directly,
				 * same as K1 - see that file's own identical note */
	if (index < 0x100)
		clcdc_palette_lut[index] = (uint16_t)(((g >> 2) << 5) | ((b >> 3) << 11) | (r >> 3));
}

/* clcdc_palette_load_defaults - fills all 256 palette entries from a packed
 * 3-bytes-per-entry default RGB table (DAT_c0003310 = 0xc001b81c).
 * Structurally identical to K1's FUN_c00037e0 (both loop-index variables in
 * the real decompile step together by 3 each iteration and read the SAME
 * underlying table - the decompiler's own two-alias artifact, same as K1's
 * `e[0]`/direct-index duality, transcribed here as the single real table
 * pointer it actually is). Sole caller: clcdc_display_object_init below.
 * @0xc00032b8, EXACT 88/88-byte size match with K1's FUN_c00037e0. */
extern const uint8_t clcdc_default_rgb_table[256 * 3];	/* DAT_c0003310 = 0xc001b81c */

void clcdc_palette_load_defaults(void *lut_unused)	/* FUN_c00032b8 */
{
	for (int i = 0; i < 0x100; i++) {
		const uint8_t *e = &clcdc_default_rgb_table[i * 3];
		clcdc_palette_set_entry(lut_unused, i, e[0], e[1], e[2]);
	}
}

/* clcdc_display_object_init - loads the default palette, then initializes a
 * 256-entry glyph/attribute table by remapping each byte of a caller-
 * supplied `attr_map` through a second lookup table pair, zeroes a 16-entry
 * scratch array, and sets up a handful of scattered pointer/flag fields.
 * Sole caller: FUN_c0011eb4 (call site 0xc0011f00) - sits IMMEDIATELY
 * BEFORE clcdc.c's own confirmed K2 range (clcdc_cursor_set_stride starts
 * at 0xc0011f34, only 0x80 bytes later) - a real, concrete lead that
 * clcdc.cpp's own true compilation-unit boundary may extend slightly
 * earlier than that file currently documents, NOT acted on here (out of
 * this file's own scope, clcdc.c not edited) but flagged for a future
 * boundary-audit pass the way K1's own project repeatedly did for other
 * files (e.g. clcdc.c's own K1 "real compilation-unit ends at
 * clcdc_blit_glyph" correction).
 *
 * CROSS-BUILD CONFIRMATION of K1's own flagged anomaly: K1's file left
 * DAT_c0003934 (the remap loop's own upper bound) as a genuinely suspicious
 * raw value, 0x752ff (~480000), too large to plausibly be a real <=256-byte
 * `attr_map` loop bound, and declined to "correct" it without live
 * verification. K2's OWN independently-resolved equivalent constant,
 * DAT_c000340c, resolves to the EXACT SAME VALUE - 0x752ff - in this
 * completely separately-compiled, separately-linked firmware image. Two
 * independent builds producing the identical unusual constant is strong
 * evidence this is a genuine (if still unexplained) compiled value, not a
 * Ghidra data-type/size misinference specific to either dump - reinforces
 * rather than resolves K1's own honest "left as the tool-resolved value
 * rather than silently corrected" stance; still not independently verified
 * against real hardware or a live disassembly of the loop bound's own
 * instruction encoding.
 *
 * @0xc0003314, EXACT 244/244-byte size match with K1's FUN_c000383c
 * (independently re-verified against K1's own dump this pass, not assumed
 * from that file's own header, which did not itself record this byte
 * count). */
extern void *clcdc_display_timer_channel;	/* DAT_c000342c = 0xc00e004c - SAME constant this
						 * tree's own cdix_autoswitch.c documents for
						 * cdix_reset_and_configure's shared context
						 * handle (see that file's own cross-file note);
						 * K1's equivalent (DAT_c0003954) held a
						 * DIFFERENT value (0xc00e0068) */
extern void clcdc_attr_finish_setup(void *timer_channel);	/* FUN_c00016ec, out of range (K1: FUN_c000196c) */
extern void clcdc_attr_finish_setup2(void);			/* FUN_c0001fc0, out of range (K1: FUN_c0002208) */

void clcdc_display_object_init(void *obj, const uint8_t *attr_map)	/* FUN_c0003314 */
{
	uint8_t *o = (uint8_t *)obj;
	extern uint16_t *clcdc_attr_remap_dst;		/* DAT_c0003410 = 0xc001b818, dereferenced */
	extern uint16_t *clcdc_attr_remap_src;		/* DAT_c0003414 = 0xc001b814, dereferenced */
	extern uint16_t clcdc_attr_scratch[16];	/* DAT_c0003418 */

	*(uint32_t *)(o + 0x28) &= ~1u;
	*(uint32_t *)(o + 8) = 0x3ff;	/* DAT_c0003408, IDENTICAL to K1's 0x3ff */
	clcdc_palette_load_defaults(0);

	/* remap loop - bound genuinely suspicious, see cross-build confirmation
	 * note above (DAT_c000340c = 0x752ff, identical to K1's DAT_c0003934) */
	for (uint32_t i = 0; i <= 0x752ff; i++) {
		clcdc_attr_remap_dst[i] = clcdc_attr_remap_src[attr_map[i]];
	}

	for (int i = 0; i < 0x10; i++)
		clcdc_attr_scratch[i] = 0;

	*(uint32_t *)(o + 4) = 0x401;		/* DAT_c000341c, IDENTICAL to K1's 0x401 */
	*(uint32_t *)(o + 0x2c) = 0x2dd19f10;	/* DAT_c0003424, IDENTICAL to K1's DAT_c000394c */
	*(uint32_t *)(o + 0x30) = 0x160b4e57;	/* DAT_c0003428, IDENTICAL to K1's DAT_c0003950 */
	*(uint32_t *)(o + 0x34) = 0x2300b00;	/* DAT_c0003430, IDENTICAL to K1's DAT_c0003958 */
	*(uint16_t **)(o + 0x44) = clcdc_attr_scratch;
	*(int *)(o + 0x48) = (int)(uintptr_t)clcdc_attr_scratch + 0xea61e;	/* DAT_c0003420, IDENTICAL to K1's DAT_c0003948 */
	*(uint32_t *)(o + 0x40) = 0x20;
	*(uint32_t *)(o + 0x28) |= 0x80;
	clcdc_attr_scratch[0] = 0x4000;

	clcdc_attr_finish_setup(clcdc_display_timer_channel);
	clcdc_attr_finish_setup2();
	*(uint32_t *)(o + 0x28) |= 1;
}

/* ===========================================================================
 * Cluster 6 (0xc0004710) - K2 counterpart of K1's large struct-zero-init
 * function (K1 cluster 6, 0xc0004cdc).
 *
 * mcasp.c's own K2 port (this tree) already cites and confirms this exact
 * function under the name "mcasp_clock_step_a" - "ported from K1
 * FUN_c0004cdc ... -> K2 FUN_c0004710 ... step_a (FUN_c0004710), size 148
 * bytes (K1: 148), zeroes/initializes the same field layout at the same
 * offsets ... identical to K1's FUN_c0004cdc field-for-field" - but leaves
 * it `extern void mcasp_clock_step_a(uint32_t base);` WITHOUT a body,
 * exactly the same collision-avoidance split as cluster 1 above. This file
 * supplies that body for the first time in this K2 tree.
 *
 * CONFIRMED, exact 148-byte size match with K1, identical field offsets
 * (0x200-0x380) and identical sentinel constants (0x284=0x10,
 * 0x308=0xffffffff, DAT_c00047a4=0x10003 at +0x31c IDENTICAL to K1's
 * DAT_c0004d70, 0x340=3) - a clean, unmodified port.
 *
 * Sole caller (per xrefs_to): FUN_c0002178 (mcasp_init itself, mcasp.c),
 * call site 0xc00023e4 - this IS mcasp_init's own documented
 * `mcasp_clock_step_a(mcasp_clock_param_select_a(cfg_base, 0))` call
 * (mcasp.c's own line "clk_param = mcasp_clock_param_select_a(cfg_base, 0);
 * mcasp_clock_step_a(clk_param);"). This directly confirms, via K2's own
 * covered dump, the SAME caller relationship K1's own file could only
 * establish by noting FUN_c00026a0 (K1's mcasp_init) as the sole caller
 * without K1's own static dump being able to show the call site's own
 * argument-passing shape this clearly.
 * @0xc0004710. */
void mcasp_gap_zero_pipeline_object(void *obj)	/* FUN_c0004710 */
{
	uint8_t *o = (uint8_t *)obj;

	for (int i = 0; i < 8; i++)
		*(uint32_t *)(o + 0x200 + i * 4) = 0;
	for (int i = 0; i < 4; i++)
		*(uint32_t *)(o + 0x240 + i * 4) = 0;
	*(uint32_t *)(o + 0x260) = 0;
	*(uint32_t *)(o + 0x284) = 0x10;
	*(uint32_t *)(o + 0x308) = 0xffffffff;
	*(uint32_t *)(o + 0x314) = 0;
	*(uint32_t *)(o + 0x31c) = 0x10003;	/* DAT_c00047a4, IDENTICAL to K1's DAT_c0004d70 */
	*(uint32_t *)(o + 800) = 0;		/* 0x320 */
	*(uint32_t *)(o + 0x340) = 3;
	*(uint32_t *)(o + 0x348) = 0;
	*(uint32_t *)(o + 0x350) = 0;
	*(uint32_t *)(o + 0x358) = 0;
	for (int i = 0; i < 4; i++)
		*(uint32_t *)(o + 0x380 + i * 4) = 0;	/* real K2 code, like K1's, advances the base
							 * pointer itself by 4 each iteration and always
							 * stores to the SAME +0x380 offset relative to
							 * the CURRENT base - equivalent to what's
							 * written here (o+0x380, o+0x384, o+0x388, o+0x38c) */
}

/* -------------------------------------------------------------------------
 * Still genuinely open, this pass:
 *  - Cluster 2 (usbdc_gap_config_slot) and its two callers - real subsystem
 *    not identified, same as K1.
 *  - Cluster 3's real fate: CONFIRMED the specific K1 register values are
 *    entirely absent from K2's binary (code and data), but whether the
 *    underlying peripheral itself was dropped or reconfigured through a
 *    completely different register layout this pass's targeted search
 *    would not find is NOT resolved.
 *  - Cluster 4's gap_store_low_byte (K2-only) and its caller FUN_c00110e8/
 *    FUN_c0011210 - real subsystem not identified; whether K1 genuinely
 *    lacked this setter or simply didn't find it is open.
 *  - Cluster 5's DAT_c000340c/0x752ff loop-bound anomaly - now cross-build
 *    confirmed as a real compiled value in TWO independent firmware images,
 *    but still not explained. clcdc_display_object_init's own real caller
 *    (FUN_c0011eb4) sitting immediately adjacent to clcdc.c's own confirmed
 *    range boundary - a real lead for a future boundary-audit pass, not
 *    acted on here.
 *  - Cluster 6's real ownership - CONFIRMED this pass (via mcasp.c's own
 *    direct call-site evidence) to be mcasp.c's own pipeline-object code,
 *    stronger confirmation than K1 had, but still not moved into mcasp.c
 *    itself (out of this pass's own scope, that file not edited here).
 *  - The real, shared meaning of the mcasp2/CDIX bit-toggle coupling found
 *    in cluster 1's caller trace (FUN_c0010380/FUN_c0010258) - confirmed to
 *    exist (see cluster 1's own note and this tree's own cdix_autoswitch.c),
 *    not decoded against any hardware documentation.
 * ------------------------------------------------------------------------- */
