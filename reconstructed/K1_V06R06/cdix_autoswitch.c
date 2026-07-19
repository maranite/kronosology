/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cdix_autoswitch.c - assigned address range 0xc000fa64-0xc000fe20 sweep,
 * CDIX side: three functions (FUN_c000fa90, FUN_c000fab8, FUN_c000fb0c)
 * that sit interleaved with cdix4192.c's own already-reconstructed
 * functions but are NOT reconstructed there.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json), 2026-07-18 pass. No live Ghidra MCP calls this pass (the
 * bridge is flagged concurrency-unsafe under this project's own parallel
 * work - see README).
 *
 * COLLISION NOTE - read this first: the assigned sweep range
 * 0xc000fa64-0xc000fe20 OVERLAPS cdix4192.c's own already-committed
 * functions at THE SAME THREE ADDRESSES this file does NOT touch:
 *   - 0xc000fa64 = cdix_reg_write   (cdix4192.c, already done)
 *   - 0xc000fb90 = cdix_reg_read    (cdix4192.c, already done)
 *   - 0xc000fbbc = cdix_configure_and_verify (cdix4192.c, already done)
 * Per this pass's own collision-avoidance rule, cdix4192.c was NOT edited.
 * This file covers exactly the three addresses in the assigned range that
 * cdix4192.c's own text does not mention anywhere (confirmed by grep over
 * that file for "0xc000fa90"/"0xc000fab8"/"0xc000fb0c" - zero hits): all
 * three are genuine CDIX-chip callers of cdix_reg_write, forming a small
 * sample-rate/format auto-detect-and-reconfigure cluster one level above
 * cdix4192.c's own low-level register access. Also covers, and does NOT
 * reconstruct (cited only, out of this file's own assigned range), the
 * outer caller chain that ties this cluster together - see "THE OUTER
 * STATE MACHINE" below.
 *
 * ANCHOR: NONE new. All three functions call directly into cdix_reg_write
 * (0xc000fa64, "../CDix4192.cpp"'s own confirmed anchor neighborhood per
 * cdix4192.c), which is real, disassembly-confirmed evidence these three
 * belong to the same CDIX chip driver, just without their own local
 * string xref (same "anchor by association, not by direct string" pattern
 * cpsoc.c documents for its own third-SPI-device cluster).
 */

#include <stdint.h>
#include <stdbool.h>

/* cdix4192.c's own primitives - re-declared locally per this project's
 * convention (each file keeps its own extern copies rather than a shared
 * header, matching the real firmware's separate-translation-unit
 * structure). Both wrap the same dead-"chip"-argument I2C register access
 * documented in cdix4192.c/i2c_by_gpio.c - re-affirmed, not re-litigated,
 * here. */
extern void cdix_reg_write(void *chip, uint8_t reg, uint8_t value);	/* cdix4192.c, FUN_c000fa64 */

/* Same 4-byte-stride, 0xff-terminated {reg,value} table shape cdix4192.c's
 * own cdix_config_table already uses for cdix_configure_and_verify - a
 * local copy of that type, not an extern (plain struct types carry no
 * linkage, no collision risk reusing the name). */
struct cdix_reg_entry {
	int8_t  reg;		/* -1 (0xff) terminates the table */
	uint8_t value;
	uint8_t pad[2];		/* real stride is 4 bytes; content not inspected */
};

/* ---------------------------------------------------------------------- *
 *  cdix_set_format_reg - FUN_c000fa90, @0xc000fa90 (20 bytes)
 *
 *  Writes CDIX register 3 to one of two hardcoded values depending on
 *  `alt`: 0x29 (the SAME value cdix4192.c's own cdix_config_table already
 *  uses as reg 3's default/power-on value) when `alt == 0`, or 0x69
 *  otherwise. Real signature confirmed from the decompile:
 *  `FUN_c000fa90(undefined4 param_1, int param_2)` -> straight tail call
 *  `cdix_reg_write(param_1, 3, param_2 == 0 ? 0x29 : 0x69)`.
 *
 *  Given reg 3's power-on value (0x29) is reused verbatim as this
 *  function's own "alt == 0" case, this reads as a two-way toggle between
 *  the chip's default format setting and one alternate format (0x69) -
 *  plausibly a sample-rate or S/PDIF-vs-something format bit, not decoded
 *  against a datasheet (same honesty caveat cdix4192.c's own header
 *  already carries for this whole chip).
 *
 *  Sole caller (per xrefs_to): FUN_c000f01c at call site 0xc000f064,
 *  always with a literal `alt = 0` - i.e. every static call site this pass
 *  can see only ever selects the DEFAULT format. FUN_c000f01c itself sits
 *  outside this file's assigned range - cited only, see "THE OUTER STATE
 *  MACHINE" below.
 * ---------------------------------------------------------------------- */
void cdix_set_format_reg(void *chip, int alt)	/* FUN_c000fa90 */
{
	cdix_reg_write(chip, 3, alt == 0 ? 0x29 : 0x69);
}

/* ---------------------------------------------------------------------- *
 *  cdix_apply_mode_table - FUN_c000fab8, @0xc000fab8 (76 bytes)
 *
 *  A write-only sibling of cdix4192.c's own cdix_configure_and_verify: no
 *  readback/verify pass, just walks ONE of two {reg,value} tables
 *  (selected by `mode`) and writes every entry via cdix_reg_write, until
 *  the 0xff/-1 terminator byte.
 *
 *   mode == 0  -> table at 0xc001fa98 (DAT_c000fb04's own real value -
 *                 the code's "else"/default branch, despite the source-
 *                 level `if (param_2 == 0)` reading like mode 0 is the
 *                 override; the decompile's own variable order is
 *                 preserved here, not re-flattened)
 *   mode != 0  -> table at 0xc001facc (DAT_c000fb08)
 *
 *  Real body, transcribed exactly (note the decompile assigns DAT_c000fb04
 *  as the initial/default pointer and OVERWRITES it with DAT_c000fb08 only
 *  when mode == 0 - i.e. mode == 0 selects 0xc001facc, not 0xc001fa98;
 *  spelled out here because it inverts the naive reading of the `if`):
 *
 *      char *table = (char *)0xc001fa98;         // DAT_c000fb04
 *      if (mode == 0)
 *              table = (char *)0xc001facc;        // DAT_c000fb08
 *      ...walk `table` in 4-byte strides until reg == -1...
 *
 *  Both table addresses sit in the same literal-pool neighborhood as
 *  cdix4192.c's own cdix_config_table (0xc001fb6c) and this file's own
 *  cdix_reset_and_configure tables below (0xc001fb00/0xc001fb38) - five
 *  CDIX register tables clustered together in ROM, consistent with one
 *  translation unit's worth of const data.
 *
 *  NEEDS LIVE QUERY: 0xc001fa98 and 0xc001facc (table contents) - the
 *  static all_data.json dump has no raw-byte entry for either address
 *  (same limitation cdix4192.c's own README note already flags for its
 *  table's 2 padding bytes per entry).
 *
 *  Sole caller (per xrefs_to): FUN_c000f0c8 at call site 0xc000f130,
 *  passing through whatever format-detect flag that function just read
 *  (see "THE OUTER STATE MACHINE" below) - i.e. this IS the "apply the
 *  detected format's register table" step in that auto-switch sequence.
 * ---------------------------------------------------------------------- */
void cdix_apply_mode_table(void *chip, int mode)	/* FUN_c000fab8 */
{
	extern const struct cdix_reg_entry cdix_mode0_table;	/* DAT_c000fb04 = 0xc001fa98 */
	extern const struct cdix_reg_entry cdix_mode1_table;	/* DAT_c000fb08 = 0xc001facc */
	const struct cdix_reg_entry *e = &cdix_mode0_table;

	if (mode == 0)
		e = &cdix_mode1_table;

	for (; e->reg != -1; e++)
		cdix_reg_write(chip, (uint8_t)e->reg, e->value);
}

/* ---------------------------------------------------------------------- *
 *  cdix_reset_and_configure - FUN_c000fb0c, @0xc000fb0c (120 bytes)
 *
 *  A heavier reset+reconfigure sequence than cdix_apply_mode_table above:
 *  a delay-gated reset step, THEN both of two more {reg,value} tables
 *  (unconditionally, not mode-selected) written back-to-back via
 *  cdix_reg_write - real signature `FUN_c000fb0c(undefined4 param_1)`.
 *
 *  Real body, in order:
 *   1. `gpio_bank_get_base(0xc00e0068)` (FUN_c0001990, canonical name/body
 *      in i2c_by_gpio.c/soc_periph.c) - called with the project's own
 *      documented "shared context handle" constant (0xC00E0068) as its
 *      dead argument, and its return value is DISCARDED here (not stored
 *      into anything). A real call, confirmed by the decompile, but with
 *      no visible effect given gpio_bank_get_base is a pure accessor with
 *      no side effects of its own - flagged as genuinely vestigial/dead
 *      from this function's own perspective, not resolved further.
 *   2. `FUN_c0002308()` - decompiled with ZERO visible arguments, despite
 *      FUN_c0002308's own one-parameter signature elsewhere
 *      (`void FUN_c0002308(undefined4 param_1) { FUN_c00022d0(param_1, 0,
 *      0x100); }`, itself a thin wrapper over the 13-caller generic
 *      slot-field setter `FUN_c00022d0(base, idx, val)` ->
 *      `*(base + idx*0x28 + 0x18) = val`). Net real effect if the implicit
 *      argument is whatever step 1 left in r0 (gpio_bank_get_base's fixed
 *      return, the GPIO-bank base 0x01E26000 - nothing else writes a
 *      register in between): `*(uint32_t *)(0x01E26000 + 0x18) = 0x100`,
 *      i.e. slot 0 of *some* generic array-of-structs at the GPIO-bank
 *      base has its own +0x18 field set to 256. NOT resolved which array
 *      this really is or whether this interpretation (implicit AAPCS r0
 *      carry-through) is correct - flagged honestly rather than guessed;
 *      genuinely possible this is instead `cdix_reset_and_configure`'s own
 *      `param_1` surviving untouched (if step 1's call is itself dead code
 *      the real compiled output elides - the decompile still shows it as a
 *      live call, so this is treated as the less likely reading here).
 *   3. `hw_timer_busy_wait(0xc00e0068, 500)` (FUN_c0001aa0, canonical name
 *      in i2c_by_gpio.c) - a genuine ~500-unit delay, the reset hold time.
 *   4. Walks table at 0xc001fb38 (DAT_c000fb88), writing every entry via
 *      cdix_reg_write(param_1, reg, value).
 *   5. Walks table at 0xc001fb00 (DAT_c000fb8c), same write pattern.
 *
 *  Both tables sit directly below cdix4192.c's own cdix_config_table
 *  (0xc001fb6c) in the same ROM cluster noted above.
 *
 *  NEEDS LIVE QUERY: 0xc001fb38 and 0xc001fb00 (table contents).
 *
 *  Sole caller (per xrefs_to): FUN_c000ecc4 at call site 0xc000ede8 -
 *  FUN_c000ecc4 is a large (364-byte) initializer calling
 *  omap_usbdc_reloc/FUN_c0009194 four times at offsets +0x400/+0x1c00/
 *  +0x3200/+0x4a00 (the exact same reloc-indirection idiom mcasp.c and
 *  midi_engine.c both already document for USB/MIDI descriptor setup) -
 *  i.e. this CDIX reset-and-configure step is called from deep inside a
 *  USB/MIDI endpoint bring-up routine, not from any audio-specific
 *  init path this pass could find. FUN_c000ecc4 itself is called from
 *  FUN_c00074bc (0xc00074bc, 320 bytes - a larger board-bring-up-shaped
 *  function also directly calling this file's sibling
 *  uart1_channel_init/FUN_c000fc48, see uart1_midi_queue.c), whose own
 *  sole caller (0xc0005648) has no containing function object in the
 *  static dump. Neither FUN_c000ecc4 nor FUN_c00074bc are reconstructed
 *  here (well outside this file's own assigned address range) - cited
 *  only to complete the trace this pass was asked for.
 * ---------------------------------------------------------------------- */
void cdix_reset_and_configure(void *chip)	/* FUN_c000fb0c */
{
	extern void *gpio_bank_get_base(void);				/* FUN_c0001990, i2c_by_gpio.c */
	extern void  hw_timer_busy_wait(void *timer_base, int units);	/* FUN_c0001aa0, i2c_by_gpio.c */
	extern void  cdix_slot0_size_setter(void);			/* FUN_c0002308, real body sets *(0x01E26000+0x18)=0x100 - see note above; kept opaque/unmodelled here, cited for completeness only */
	extern const struct cdix_reg_entry cdix_reset_table_a;	/* DAT_c000fb88 = 0xc001fb38 */
	extern const struct cdix_reg_entry cdix_reset_table_b;	/* DAT_c000fb8c = 0xc001fb00 */
	const struct cdix_reg_entry *e;

	/* DAT_c000fb84 = 0xC00E0068, shared context handle, passed as the real
	 * call's dead argument - gpio_bank_get_base's canonical prototype
	 * (i2c_by_gpio.c/soc_periph.c) is `(void)`, ignoring it entirely;
	 * the arg is dropped here to match that prototype rather than
	 * re-declaring a divergent one just for this call site. */
	(void)gpio_bank_get_base();
	cdix_slot0_size_setter();	/* FUN_c0002308() - see note above re: its real (possibly implicit) argument */
	hw_timer_busy_wait(0 /* DAT_c000fb84 = 0xC00E0068, dead arg per i2c_by_gpio.c's own documented convention */, 500);

	for (e = &cdix_reset_table_a; e->reg != -1; e++)
		cdix_reg_write(chip, (uint8_t)e->reg, e->value);
	for (e = &cdix_reset_table_b; e->reg != -1; e++)
		cdix_reg_write(chip, (uint8_t)e->reg, e->value);
}

/* ---------------------------------------------------------------------- *
 *  THE OUTER STATE MACHINE - context only, NOT reconstructed here (all
 *  addresses below are outside this file's own 0xc000fa90-0xc000fb90
 *  assigned coverage). Traced far enough to confirm this file's three
 *  functions are real pieces of one coherent CDIX auto-format-switch
 *  sequence, not three unrelated one-off callers:
 *
 *   - FUN_c000f0c8 (@0xc0008d2c, called from wire_dispatch.c's own
 *     master_dispatch_tick/FUN_c0008b64 - i.e. this whole cluster is
 *     polled every tick, not event-driven): reads a hardware/GPIO
 *     format-detect bit via FUN_c00057dc, stores it, calls
 *     cdix_apply_mode_table (this file) with that flag as `mode`, then -
 *     if a second detect call (FUN_c0002588) reads zero OR the flag isn't
 *     exactly 1 - falls through to FUN_c000f01c (full reset path).
 *   - FUN_c000f01c (@0xc000f064/0xc000f0c4): the full reset path - a
 *     ~2800-iteration busy-wait, calls cdix_set_format_reg(.., 0) (this
 *     file, always the DEFAULT format), more delay loops, then two
 *     out-of-range helpers (FUN_c000efa8/FUN_c000eff0) and a final
 *     FUN_c0003194 call.
 *   - FUN_c000f0b8 (@0xc0008dc4 x4, also from master_dispatch_tick):
 *     a 2-way dispatcher between FUN_c000ef20 and FUN_c000f01c based on a
 *     byte flag - one more tick-driven entry into the same reset path.
 *
 *  Net picture: this is a genuine, tick-polled "detect current S/PDIF-
 *  style format, reconfigure the CDIX chip's registers to match, and
 *  fully reset-and-reconfigure if detection looks unstable" state
 *  machine. None of FUN_c000f01c/FUN_c000f0c8/FUN_c000f0b8/FUN_c00057dc/
 *  FUN_c0002588/FUN_c000efa8/FUN_c000eff0/FUN_c0003194 are reconstructed
 *  in this file (all outside the assigned sweep range) - a real
 *  candidate for a future "cdix format-detect" file of their own.
 * ---------------------------------------------------------------------- */

/* Still open:
 *  - The 5 CDIX register tables in the 0xc001fa98-0xc001fb7c ROM cluster
 *    (this file's 4 + cdix4192.c's own 1) have no readable byte content in
 *    the static dump - a live `read_memory` pass over that whole 0xe4-byte
 *    span would resolve all of them in one call.
 *  - FUN_c0002308's real argument at its FUN_c000fb0c call site (implicit
 *    AAPCS r0 carry-through vs. a genuine decompiler artifact) - see the
 *    detailed note above cdix_reset_and_configure.
 *  - Whether cdix_apply_mode_table's `mode` and cdix_set_format_reg's
 *    `alt` are driven by the SAME underlying detect bit (both ultimately
 *    fed by FUN_c000f0c8's own single flag read) - plausible given both
 *    are called from the same outer sequence, not independently confirmed
 *    since FUN_c000f0c8 itself is out of this file's scope.
 */
