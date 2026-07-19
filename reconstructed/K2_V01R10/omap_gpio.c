/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_gpio.c - KRONOS2S_V01R10.VSB (Kronos 2) counterpart of K1's
 * K1_V06R06/omap_gpio.c. Same physical role (the raw GPIO-bank register
 * layer + a PSC module-enable wrapper + assorted thin bit-twiddle leaves
 * sitting between soc_periph.c's own table and mcasp.c's mcasp_init), but a
 * GENUINELY DIFFERENT ARCHITECTURE, not a straight port - see the headline
 * finding below.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB,
 * 2026-07-18 (query_dump_k2.py, not the live Ghidra MCP bridge).
 *
 * ANCHOR: NONE, same as K1.
 *
 * HEADLINE FINDING - K1's GENERIC PAIR-INDEXED REGISTER LAYER IS GONE. K1's
 * own omap_gpio.c was built on exactly three generic primitives
 * (gpio_reg_read_in/set_bit/clear_bit) taking an explicit `pair` index and
 * computing `bank_base + pair*0x28 + {0x20,0x18,0x1c}` - a real, reusable
 * abstraction over the SoC's bank-pair register layout. A systematic search
 * of every function in the K2 dump's decompiled text for that `* 0x28`
 * stride pattern found EXACTLY ONE unrelated false-positive match and ZERO
 * real hits. Every GPIO-touching function actually found in K2's equivalent
 * address cluster (0xc0001fc0-0xc0002117, immediately before mcasp_init's
 * own confirmed 0xc0002178 - see mcasp.c) instead hardcodes ONE specific
 * bit at ONE specific fixed byte offset directly, with NO pair-index
 * indirection layer at all. This is a real, confirmed architectural
 * simplification/inlining in K2, not a gap in this pass's own coverage -
 * every caller traced below passes a raw GPIO-bank base pointer straight
 * into these leaves, never through anything resembling K1's own
 * gpio_reg_set_bit(base, pair, mask) call shape.
 *
 * Two of this file's functions are still confirmed identical in ROLE (not
 * mechanism) to K1: gpio_bank_set_dir_bit/gpio_bank_read_sda_bit are already
 * declared `extern` by K2's own i2c_by_gpio.c (same names, same K2
 * addresses, same SDA-bit-18 role) - defined here, closing that file's
 * forward reference, exactly as K1's own omap_gpio.c did for i2c_by_gpio.c.
 *
 * ADDRESS MAP (K1 -> K2, role-based, NOT mechanism-identical except where noted):
 *   gpio_bank_set_dir_bit   FUN_c00025ac -> FUN_c000208c (i2c_by_gpio.c's own extern)
 *   gpio_bank_read_sda_bit  FUN_c00025e4 -> FUN_c00020c0 (i2c_by_gpio.c's own extern)
 *   omap_psc_enable_module  FUN_c0002208 -> FUN_c0001fc0, mechanism identical
 *   gpio_psc_enable         FUN_c0001fd0 (extern-only in K1!) -> FUN_c0001d88, NOW DEFINED
 *   gpio_bank_hw_init       FUN_c0002248 -> FUN_c0001ffc, PARTIAL SUBSET only, see below
 *  (everything else in this file is K2-only / not confirmed present in K1's
 *  own reconstructed range - see per-function notes)
 */

#include <stdint.h>

extern void *gpio_bank_get_base(void);	/* FUN_c0001710, defined in soc_periph.c */

/* ------------------------------------------------------------------------- *
 *  psc_module_enable (sibling copy) - see soc_periph.c's own header point 6.
 *  BYTE-FOR-BYTE IDENTICAL body to soc_periph.c's psc_module_enable
 *  (FUN_c0001c50) - two separate compiled copies of the same source-level
 *  function at two different addresses, one with zero callers (soc_periph.c
 *  own copy) and this one with exactly 2, both from
 *  omap_psc_enable_module_0x10 below. Matches K1's own architecture exactly:
 *  K1 ALSO had two such functions, but only ever reconstructed the
 *  zero-caller one, leaving this one (`gpio_psc_enable`, K1 @0xc0001fd0) as
 *  an unresolved `extern`, "outside this file's range". K2's own dump
 *  happens to cover this address as a real function object - defined here
 *  for the first time in this project. @0xc0001d88 (K1's own extern-only
 *  citation: @0xc0001fd0).
 * ------------------------------------------------------------------------- */
void gpio_psc_enable(void *psc_base, int lpsc_module, unsigned int power_domain)	/* FUN_c0001d88 */
{
	int mdctl = lpsc_module * 4 + (int)(intptr_t)psc_base + 0xa00;

	*(uint32_t *)mdctl = (*(uint32_t *)mdctl & ~0x7u) | 3u;

	*(uint32_t *)((uint8_t *)psc_base + 0x120) = 1u << (power_domain & 0xff);

	while ((*(uint32_t *)((uint8_t *)psc_base + 0x128) & (1u << (power_domain & 0xff))) != 0)
		;

	while ((*(uint32_t *)((uint8_t *)psc_base + lpsc_module * 4 + 0x800) & 0x3fu) != 3u)
		;
}

/* ------------------------------------------------------------------------- *
 *  omap_psc_enable_module_0x10 - two back-to-back gpio_psc_enable calls for
 *  PSC module 0x10 (16), force=1 then force=0 - IDENTICAL shape and module
 *  number to K1's own omap_psc_enable_module_0x10. Sole caller: FUN_c0003314
 *  (outside this file's range, not traced - K1's own sole caller,
 *  FUN_c000383c, was likewise not traced). @0xc0001fc0 (K1: @0xc0002208).
 * ------------------------------------------------------------------------- */
void omap_psc_enable_module_0x10(void *psc_base)	/* FUN_c0001fc0 */
{
	gpio_psc_enable(psc_base, 0x10, 1);
	gpio_psc_enable(psc_base, 0x10, 0);
}

/* ------------------------------------------------------------------------- *
 *  gpio_bank_hw_init - a REAL, but genuinely PARTIAL, subset of K1's own
 *  gpio_bank_hw_init. K1's version wrote BINTEN plus full DIR/OUT_DATA/
 *  edge-trigger defaults for FOUR bank-pairs (0, 1, 3, 4). K2's version
 *  writes only:
 *    +0x08  BINTEN = 0x101              - IDENTICAL value to K1
 *    +0x2c  SET_FAL_TRIG (pair 0) = 0x20 - IDENTICAL bit (bit 5) to K1
 *    +0x28  CLR_RIS_TRIG (pair 0) = 0x20 - IDENTICAL bit to K1
 *    +0xcc, +0xc8  = 4, 4                - IDENTICAL "NOT IDENTIFIED" values
 *                                          to K1's own unresolved pair
 *  K1's own DIR/OUT_DATA writes for pairs 0, 1, 3, 4 (7 more register
 *  writes) are ALL ABSENT here - not present anywhere in this function's
 *  real K2 body. LIVE-QUERY RESOLVED 2026-07-19 (see this file's own STILL
 *  OPEN section for the full evidence): explanation (a) is now the
 *  reasonably-confirmed one - K2 genuinely dropped the cpsoc/cad SPI-driven
 *  peripherals that used those extra pins (consistent with K2_V01R10/
 *  README.md's own architecture note), so fewer pins need default DIR/OUT
 *  configuration at boot. Explanation (b) (moved to an unlocated function)
 *  is now reasonably ruled out - an exhaustive sweep of all 30 confirmed
 *  callers of gpio_bank_get_base found none touching the pair-1/3/4 DIR/
 *  OUT_DATA offsets. Transcribed faithfully as the real, smaller K2 body -
 *  not padded out to match K1's shape. Sole
 *  caller: FUN_c0000800 (board bring-up, same caller role as K1's own
 *  FUN_c0000a20/FUN_c0000b50 pair, though K2 has only one call site found).
 *  @0xc0001ffc (K1: @0xc0002248).
 * ------------------------------------------------------------------------- */
void gpio_bank_hw_init(void *bank_base)	/* FUN_c0001ffc */
{
	uint8_t *b = (uint8_t *)bank_base;

	*(uint32_t *)(b + 0x08) = 0x101;	/* BINTEN, identical to K1 */
	*(uint32_t *)(b + 0x2c) = 0x20;		/* pair 0 SET_FAL_TRIG, bit5 */
	*(uint32_t *)(b + 0x28) = 0x20;		/* pair 0 CLR_RIS_TRIG, bit5 */
	*(uint32_t *)(b + 0xcc) = 4;		/* NOT IDENTIFIED, matches K1's own unresolved value */
	*(uint32_t *)(b + 0xc8) = 4;		/* NOT IDENTIFIED, matches K1's own unresolved value */
	*(uint32_t *)(b + 0x90) = 2;		/* NEW vs K1 - no counterpart in K1's own gpio_bank_hw_init */
}

/* ------------------------------------------------------------------------- *
 *  gpio_field_0x18_set - single-word direct field write at +0x18, NOT a
 *  pair-indexed GPIO register (no bank-base evidence this is IN_DATA/
 *  SET_DATA/CLR_DATA at all - see this file's own headline finding). Sole
 *  caller: FUN_c0010ed4, outside this file's range, not traced. No K1
 *  counterpart found. @0xc0002024.
 * ------------------------------------------------------------------------- */
void gpio_field_0x18_set(int handle)	/* FUN_c0002024 */
{
	*(uint32_t *)(handle + 0x18) = 0x100;
}

/* ------------------------------------------------------------------------- *
 *  gpio_field_0x90_set4 - single-word direct field write at +0x90. Sole
 *  caller: FUN_c0011eb4, outside this file's range, not traced. No K1
 *  counterpart. @0xc0002048.
 * ------------------------------------------------------------------------- */
void gpio_field_0x90_set4(int handle)	/* FUN_c0002048 */
{
	*(uint32_t *)(handle + 0x90) = 4;
}

/* ------------------------------------------------------------------------- *
 *  gpio_field_0x90_0x94_select - writes 8 into EITHER +0x90 (sel==1) or
 *  +0x94 (else) - a matched-pair field selector shape, structurally
 *  reminiscent of K1's own TX/RX-serializer paired-field pattern
 *  (mcasp.c's own "2 of 7 paired fields differ" note) though this specific
 *  function sits outside mcasp.c's own confirmed range. 2 callers, both in
 *  FUN_c0000904 (outside this file, not traced - calls this with sel=1 then
 *  sel=0 around an hw_timer_busy_wait call, suggesting a timed
 *  assert/deassert pulse on whichever field is selected). No K1
 *  counterpart. @0xc0002060.
 * ------------------------------------------------------------------------- */
void gpio_field_0x90_0x94_select(int handle, int sel)	/* FUN_c0002060 */
{
	if (sel == 1)
		*(uint32_t *)(handle + 0x90) = 8;
	else
		*(uint32_t *)(handle + 0x94) = 8;
}

/* ------------------------------------------------------------------------- *
 *  gpio_field_0x20_bit22_status - reads bit 22 (0x400000) of a field at
 *  +0x20 (the same byte offset IN_DATA lives at in K1's own pair-indexed
 *  scheme, though this function shows no pair-multiply, consistent with
 *  this file's own headline finding), returns it INVERTED (XOR 1). Sole
 *  caller: FUN_c0009838, outside this file's range, not traced. No K1
 *  counterpart. @0xc0002078.
 * ------------------------------------------------------------------------- */
uint32_t gpio_field_0x20_bit22_status(int handle)	/* FUN_c0002078 */
{
	return ((*(uint32_t *)(handle + 0x20) >> 0x16) ^ 1) & 1;
}

/* ------------------------------------------------------------------------- *
 *  gpio_field_0xb8_0xbc_select_bit0x1000 / _bit0x2000 - a matched pair of
 *  matched-pair selectors: each writes ONE fixed bit into EITHER +0xb8
 *  (sel==1) or +0xbc (else). Same "sel picks between two parallel fields"
 *  shape as gpio_field_0x90_0x94_select above, different offsets/bits.
 *  6 and 4 callers respectively (FUN_c000303c, FUN_c0005088, FUN_c00050f0 -
 *  none traced, out of this file's own range). No K1 counterpart found -
 *  the +0xb8/+0xbc offset pair does not match anything in K1's own
 *  gpio_bank_hw_init register map. @0xc00020e8 / @0xc0002100.
 * ------------------------------------------------------------------------- */
void gpio_field_0xb8_0xbc_select_bit0x1000(int handle, int sel)	/* FUN_c00020e8 */
{
	if (sel == 1)
		*(uint32_t *)(handle + 0xb8) = 0x1000;
	else
		*(uint32_t *)(handle + 0xbc) = 0x1000;
}

void gpio_field_0xb8_0xbc_select_bit0x2000(int handle, int sel)	/* FUN_c0002100 */
{
	if (sel == 1)
		*(uint32_t *)(handle + 0xb8) = 0x2000;
	else
		*(uint32_t *)(handle + 0xbc) = 0x2000;
}

/* ========================================================================= *
 *  Bank-0 bit 18 (SDA) - CONFIRMED cross-file, IDENTICAL ROLE to K1 (both
 *  functions declared `extern` by i2c_by_gpio.c under these exact names,
 *  same single-caller-each pattern). K2's own bodies operate directly on
 *  base+0x10/base+0x20 with NO pair-multiply - i.e. "pair 0" is simply
 *  hardcoded/absent here, consistent with this file's own headline finding
 *  that K2 never generalized the pair-index abstraction at all (K1's own
 *  version, by contrast, DID go through a pair-indexed primitive for the
 *  IN_DATA read, per that file's own point 4 in i2c_by_gpio.c's K2 header).
 * ========================================================================= */

/* gpio_bank_set_dir_bit - direct DIR01 bit 18 toggle. `input` nonzero ->
 * configure as input, zero -> output (same TI convention as K1). Sole
 * caller: i2c_by_gpio.c's i2c_gpio_set_sda_dir. @0xc000208c
 * (K1: @0xc00025ac). */
void gpio_bank_set_dir_bit(void *bank_base, int input)	/* FUN_c000208c */
{
	uint32_t *dir = (uint32_t *)((uint8_t *)bank_base + 0x10);

	if (input == 0)
		*dir &= 0xfffbffffu;
	else
		*dir |= 0x40000u;
}

/* gpio_bank_read_sda_bit - reads IN_DATA01 bit 18 (live SDA line level) as a
 * 0/1 boolean, DIRECTLY off base+0x20 (K1 went through one more helper call,
 * FUN_c0002238, for the same read - i2c_by_gpio.c's own K2 header already
 * flagged this as "a shallower inline in this build", independently
 * confirming this file's own headline finding). Sole caller: i2c_by_gpio.c's
 * i2c_gpio_sda_read. @0xc00020c0 (K1: @0xc00025e4). */
uint32_t gpio_bank_read_sda_bit(void *bank_base)	/* FUN_c00020c0 */
{
	return (*(uint32_t *)((uint8_t *)bank_base + 0x20) >> 0x12) & 1;
}

/* -------------------------------------------------------------------------
 * Still genuinely open:
 *  - Every "gpio_field_*" leaf's real physical meaning (which peripheral
 *    `handle` actually points at for each caller) - none of these were
 *    cross-checked against a base-address constant the way soc_periph.c's
 *    own table entries were; all are transcribed purely from their own
 *    isolated bit/offset shape.
 *  - No caller was traced past one level for ANY function in this file
 *    (FUN_c000303c/FUN_c0005088/FUN_c00050f0/FUN_c0000904/FUN_c0009838/
 *    FUN_c0010ed4/FUN_c0011eb4/FUN_c0003314 all left unexamined) - out of
 *    this pass's own time budget, prioritizing breadth across all 4
 *    assigned files over exhaustively tracing this one cluster's callers.
 *
 * 2026-07-19 LIVE-QUERY PASS (dedicated single-session live Ghidra MCP
 * bridge, get_xrefs_to/decompile_function/search_bytes, "2-agent cap, no
 * further fan-out" authorization - see CLAUDE.md; zero Agent-tool subagent
 * calls made) - RESOLVED, with reasonable but not absolute confidence:
 *
 *  - Whether K1's own missing DIR/OUT_DATA pairs-1/3/4 defaults (absent from
 *    this file's gpio_bank_hw_init) are genuinely gone in K2 or live in a
 *    function this pass didn't locate. get_xrefs_to on gpio_bank_get_base
 *    confirmed exactly the 30 callers i2c_by_gpio.c's own header already
 *    counted; EVERY ONE of the 30 was individually examined this pass
 *    (get_function_info + decompile_function, or matched against an already-
 *    documented file - i2c_by_gpio.c: i2c_gpio_set_scl/_sda/FUN_c0000f4c/
 *    i2c_gpio_sda_read; wire_dispatch.c: eva_boot_status_unk_6/FUN_c000a1d0;
 *    this file: gpio_field_0x18_set/_0x90_set4/_0x90_0x94_select/
 *    _0x20_bit22_status/_0xb8_0xbc_select pair; soc_irq_gate.c: ch32_enable_
 *    gpio/ch2a_quiesce/mcasp2_bringup; panelbus_dispatch.c: panelbus_hw_
 *    bringup; plus FUN_c000303c/FUN_c0005088/FUN_c00050f0/FUN_c001041c/
 *    FUN_c00080e4 freshly decompiled here). NONE of the 30 writes to the
 *    pair-1/3/4 absolute DIR/OUT_DATA offsets (+0x38, +0x88/+0x8c, +0xb0/
 *    +0xb4) - every write among all 30 lands on offsets already documented
 *    elsewhere in this project (+0x08/+0x10/+0x18/+0x1c/+0x20/+0x28/+0x2c/
 *    +0x90/+0x94/+0xb8/+0xbc/+0xd4/+0x244, none of them pair-1/3/4 DIR/OUT).
 *    A supplementary full-image search_bytes sweep for K1's own three DIR
 *    default constants as raw little-endian ldr-literal bytes (0xffffd7ff,
 *    0xffff7ff1, 0xffff0ffd - pair 1/3/4 respectively) found ZERO hits for
 *    all three. CAVEAT, stated honestly rather than oversold: ARM can
 *    generate a "mostly-1s" constant like these via an MVN-immediate
 *    instruction (8-bit value + rotation) with no literal-pool entry at all,
 *    which a raw byte search cannot catch - so this second check is
 *    supporting evidence, not independent proof. Taken together (exhaustive
 *    caller sweep + literal search + gpio_bank_get_base being the ONLY GPIO-
 *    bank-base source anywhere in this cluster), this closes the question
 *    with reasonable confidence in the negative: K1's pair-1/3/4 DIR/OUT_DATA
 *    boot-time defaults are genuinely DROPPED in K2, not merely unlocated -
 *    consistent with cpsoc.cpp/cad.cpp's own confirmed removal freeing up
 *    whatever pins those pairs used to configure.
 * ------------------------------------------------------------------------- */
