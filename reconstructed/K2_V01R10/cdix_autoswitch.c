/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cdix_autoswitch.c - K2 (KRONOS2S_V01R10.VSB / "KRONOS II") port of the CDIX
 * sample-rate/format auto-detect-and-reconfigure cluster already
 * reconstructed for K1 in K1_V06R06/cdix_autoswitch.c: three functions
 * (cdix_set_format_reg / cdix_apply_mode_table / cdix_reset_and_configure)
 * that sit immediately after cdix4192.c's own three low-level functions
 * (cdix_reg_write/cdix_reg_read/cdix_configure_and_verify) but are NOT part
 * of that file, same "genuine CDIX-chip caller cluster, no own filename
 * anchor" attribution K1 established.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB
 * (query_dump_k2.py), 2026-07-19. Located by the SAME method K1's own file
 * used - not a string anchor, but immediate positional adjacency plus a real
 * xref into cdix4192.c's own cdix_reg_write (K2 FUN_c0010e2c). K2's cluster
 * is a striking EXACT-SIZE match against K1's, function-for-function, in the
 * same relative order, immediately after cdix4192.c's own three functions
 * end (K2 cdix_reg_read ends at 0xc0010e58, exactly where this cluster
 * begins) - about as strong a "this is really the same cluster, just
 * relinked" signal as this project's static-analysis method can produce:
 *
 *   K1 (size)                  -> K2 (size)               match
 *   FUN_c000fa90 (20B)  set_format_reg  -> FUN_c0010e58 (20B)   EXACT
 *   FUN_c000fab8 (76B)  apply_mode_table -> FUN_c0010e80 (76B)  EXACT
 *   FUN_c000fb0c (120B) reset_and_configure -> FUN_c0010ed4 (120B) EXACT
 *
 * Every operation, every branch, every constant shape (the mode==0/mode!=0
 * table-selection inversion, the gpio_bank_get_base+FUN_c0002024+
 * hw_timer_busy_wait 3-step reset preamble, the two back-to-back
 * unconditional table walks) is structurally IDENTICAL to K1. Only the
 * literal table/context addresses differ, as expected for a relinked build.
 *
 * NO logic differences found between K1 and K2 for this cluster - unlike
 * clcdc.c's own progress-bar finding, this is a clean, unmodified port.
 */

#include <stdint.h>

/* cdix4192.c's own primitive - re-declared locally per this project's
 * convention (own extern copy per translation unit, matching the real
 * firmware's separate-compilation-unit structure). NOTE: cdix4192.c
 * declares this function `static` in its own file - the same is true of
 * K1's cdix4192.c/cdix_autoswitch.c pair. This is harmless for this
 * project's `-fsyntax-only -c` per-file compile gate (each file is type-
 * checked standalone, never actually linked against the others), and
 * mirrors the real firmware's own genuinely separate translation units. */
extern void cdix_reg_write(void *chip, uint8_t reg, uint8_t value);	/* cdix4192.c, FUN_c0010e2c (K1: FUN_c000fa64) */

/* Same 4-byte-stride, 0xff-terminated {reg,value} table shape cdix4192.c's
 * own cdix_config_table uses - a local copy of the type, not an extern. */
struct cdix_reg_entry {
	int8_t  reg;		/* -1 (0xff) terminates the table */
	uint8_t value;
	uint8_t pad[2];		/* real stride is 4 bytes; content not inspected, same as K1 */
};

/* ---------------------------------------------------------------------- *
 *  cdix_set_format_reg - FUN_c0010e58, @0xc0010e58 (20 bytes, EXACT size
 *  match with K1's FUN_c000fa90).
 *
 *  Structurally identical to K1: writes CDIX register 3 to 0x29 (`alt==0`)
 *  or 0x69 (otherwise) - same two hardcoded values, numerically unchanged.
 *  Real K2 body: `if (param_2 == 0) uVar1 = 0x29; else uVar1 = 0x69;
 *  FUN_c0010e2c(param_1, 3, uVar1);` - byte-for-byte the same shape as K1's
 *  decompile with addresses masked.
 *
 *  Sole caller (per xrefs_to): FUN_c00103b8 inside FUN_c0010380 (K2
 *  counterpart of K1's FUN_c000f01c, the CDIX full-reset outer path - see
 *  "THE OUTER STATE MACHINE" section below), always with `alt = 0` at this
 *  one static call site - same "every static call site only ever selects
 *  the DEFAULT format" finding as K1.
 * ---------------------------------------------------------------------- */
void cdix_set_format_reg(void *chip, int alt)	/* FUN_c0010e58 */
{
	cdix_reg_write(chip, 3, alt == 0 ? 0x29 : 0x69);
}

/* ---------------------------------------------------------------------- *
 *  cdix_apply_mode_table - FUN_c0010e80, @0xc0010e80 (76 bytes, EXACT size
 *  match with K1's FUN_c000fab8).
 *
 *  Write-only sibling of cdix4192.c's own cdix_configure_and_verify, same
 *  as K1: walks ONE of two {reg,value} tables (selected by `mode`) until the
 *  0xff/-1 terminator. SAME mode==0/mode!=0 table-selection inversion K1's
 *  own comment flagged as counter-intuitive, reproduced verbatim here rather
 *  than "fixed":
 *
 *      const struct cdix_reg_entry *e = &cdix_mode0_table;   // DAT_c0010ecc = 0xc0027e4c, the DEFAULT (used when mode != 0)
 *      if (mode == 0)
 *              e = &cdix_mode1_table;                         // DAT_c0010ed0 = 0xc0027e84, selected when mode == 0
 *
 *  i.e. despite the names, `cdix_mode0_table` is what's used for mode != 0,
 *  and `cdix_mode1_table` is what's used for mode == 0 - same inversion, same
 *  variable-naming convention kept unchanged from K1 for direct
 *  cross-reference.
 *
 *  Both table addresses (0xc0027e4c / 0xc0027e84) sit in the SAME general
 *  .rodata region as cdix4192.c's own K2 cdix_config_table (0xc0027f28) -
 *  the same "clustered CDIX register tables" pattern K1 documented.
 *
 *  2026-07-19 LIVE QUERY RESOLVED: both tables read byte-exact via live
 *  read_memory (multiple calls stitched together, retried through this
 *  pass's own confirmed live-bridge flakiness - see omap_l108_syscfg.c's
 *  header for that same artifact). Both are 13 real entries + {0xff,...}
 *  terminator (14 x 4 bytes = 56 bytes each), and CONFIRMED BYTE-
 *  CONTIGUOUS: cdix_mode0_table's terminator sits at 0xc0027e80, exactly
 *  abutting cdix_mode1_table's start at 0xc0027e84; cdix_mode1_table's own
 *  terminator sits at 0xc0027eb8, exactly abutting cdix_reset_table_b's
 *  start (0xc0027ebc, see below) - strong independent confirmation both
 *  reads are correctly bounded.
 *
 *  cdix_mode0_table (0xc0027e4c, used when mode != 0 - see the inversion
 *  note above): {0x7f,0x00}, {0x01,0x00}, {0x03,0x69}, {0x04,0x0b},
 *  {0x07,0xe0}, {0x08,0x01}, {0x09,0x01}, {0x0d,0x08}, {0x0e,0x11},
 *  {0x0f,0x22}, {0x10,0x00}, {0x11,0x00}, {0x01,0x36}, then terminator.
 *
 *  cdix_mode1_table (0xc0027e84, used when mode == 0): {0x7f,0x00},
 *  {0x01,0x00}, {0x03,0x69}, {0x04,0x03}, {0x07,0x60}, {0x08,0x01},
 *  {0x09,0x01}, {0x0d,0x08}, {0x0e,0x11}, {0x0f,0x22}, {0x10,0x00},
 *  {0x11,0x00}, {0x01,0x36}, then terminator.
 *
 *  REAL FINDING: the two tables are IDENTICAL except for exactly two
 *  entries (reg 0x04: 0x0b vs 0x03; reg 0x07: 0xe0 vs 0x60) - consistent
 *  with two closely-related chip operating modes/sample-rate families
 *  differing in only two register values, not two independently-authored
 *  tables. Both tables also write register 0x01 TWICE (once as {0x01,0x00}
 *  near the start, once as {0x01,0x36} near the end, identically in both
 *  tables) - a real, confirmed quirk (not a transcription duplicate),
 *  reproduced faithfully rather than "cleaned up."
 *
 *  Sole caller (per xrefs_to): FUN_c0010478 inside FUN_c001041c (K2
 *  counterpart of K1's FUN_c000f0c8) - the "apply the detected format's
 *  register table" step of the same outer sequence, see below.
 * ---------------------------------------------------------------------- */
void cdix_apply_mode_table(void *chip, int mode)	/* FUN_c0010e80 */
{
	extern const struct cdix_reg_entry cdix_mode0_table;	/* DAT_c0010ecc = 0xc0027e4c */
	extern const struct cdix_reg_entry cdix_mode1_table;	/* DAT_c0010ed0 = 0xc0027e84 */
	const struct cdix_reg_entry *e = &cdix_mode0_table;

	if (mode == 0)
		e = &cdix_mode1_table;

	for (; e->reg != -1; e++)
		cdix_reg_write(chip, (uint8_t)e->reg, e->value);
}

/* ---------------------------------------------------------------------- *
 *  cdix_reset_and_configure - FUN_c0010ed4, @0xc0010ed4 (120 bytes, EXACT
 *  size match with K1's FUN_c000fb0c).
 *
 *  Structurally identical heavier reset+reconfigure sequence, same 5-step
 *  shape as K1:
 *   1. `gpio_bank_get_base()` (FUN_c0001710, i2c_by_gpio.c's own K2 name for
 *      K1's FUN_c0001990 - see i2c_by_gpio.c's own doc comment) called with
 *      the shared context handle DAT_c0010f4c (=0xc00e004c, see below) as
 *      its dead argument (gpio_bank_get_base's real K2 signature is `(void)`
 *      - the same phantom-forward idiom already established project-wide).
 *      Return value discarded.
 *   2. `FUN_c0002024(param_1)` - RESOLVES K1's own open question about
 *      FUN_c0002308's real argument. Unlike K1's decompile (which showed
 *      this callee's OWN body taking an explicit param but its call site
 *      showing zero visible arguments, leaving "implicit AAPCS r0
 *      carry-through vs. decompiler artifact" genuinely open), K2's own
 *      FUN_c0002024 decompiles to an explicit one-argument function,
 *      `void FUN_c0002024(int param_1) { *(param_1 + 0x18) = 0x100; }` -
 *      but its OWN call site here STILL shows zero visible arguments in the
 *      K2 decompile too. Net effect: the ambiguity is the same shape in
 *      both builds (call site drops the argument, callee body uses one) -
 *      NOT independently resolved by K2 either, transcribed the same way
 *      K1 did (opaque/unmodelled call, real argument left as
 *      implicit-r0-carry-through, matching K1's own "less likely reading"
 *      caveat verbatim since the K2 evidence doesn't change the picture).
 *   3. `hw_timer_busy_wait(0xc00e004c, 500)` (FUN_c000185c, i2c_by_gpio.c's
 *      K2 name for K1's FUN_c0001aa0) - same ~500-unit delay.
 *   4. Walks table at DAT_c0010f50 (0xc0027ef4).
 *   5. Walks table at DAT_c0010f54 (0xc0027ebc).
 *
 *  Both tables sit in the same .rodata cluster as cdix4192.c's own
 *  cdix_config_table (0xc0027f28) and this file's own cdix_mode0_table/
 *  cdix_mode1_table above - five CDIX register tables clustered together,
 *  same pattern as K1.
 *
 *  REAL, CONFIRMED K2-SPECIFIC FINDING (not in K1's own writeup): the shared
 *  context-handle constant this function passes to gpio_bank_get_base/
 *  hw_timer_busy_wait, 0xc00e004c, is the SAME constant mcasp.c's own K2
 *  mcasp_init cites for its own "shared clock-manager singleton pointer"
 *  (that file's own DAT_c00026e4) and the same one clcdc's own
 *  clcdc_display_object_init cluster uses (see omap_l137_addr_gap_misc.c's
 *  own cluster 5 in this K2 tree) - three otherwise-unrelated subsystems
 *  (CDIX, McASP, LCD/attribute-table init) all key off the identical fixed
 *  address 0xC00E004C, which itself sits inside the same 0xC00E0000-based
 *  page soc_irq_gate.c's own K2 port independently confirmed as a real,
 *  fixed OMAP-L138/DA850 physical SRAM bookkeeping region (0xC00E0000-
 *  0xC00E004C). K1's own equivalent constant for this cluster was
 *  0xC00E0068 - a DIFFERENT sub-offset into the same page, not the same
 *  literal value - consistent with this being a real fixed hardware/SRAM
 *  region whose per-field layout simply differs slightly between the two
 *  firmware builds, not a coincidence.
 *
 *  2026-07-19 LIVE QUERY RESOLVED: both tables read byte-exact via live
 *  read_memory, same methodology/flakiness note as the mode0/mode1 tables
 *  above. cdix_reset_table_b (0xc0027ebc): 13 entries + terminator (14 x 4
 *  bytes), {0x7f,0x02}, {0x00,0x20}, {0x01,0x20}, {0x02,0x00}, {0x03,0x00},
 *  {0x04,0x08}, {0x05,0x04}, {0x06,0x40}, {0x07,0x40}, {0x08,0xdb},
 *  {0x09,0xdb}, {0x0a,0x00}, {0x0b,0x00}, then terminator at 0xc0027ef0 -
 *  which exactly abuts cdix_reset_table_a's own start (0xc0027ef4).
 *  cdix_reset_table_a (0xc0027ef4): 12 entries + terminator (13 x 4 bytes),
 *  {0x7f,0x00}, {0x03,0x29}, {0x04,0x03}, {0x07,0x60}, {0x08,0x01},
 *  {0x09,0x01}, {0x0d,0x08}, {0x0e,0x11}, {0x0f,0x22}, {0x10,0x00},
 *  {0x11,0x00}, {0x01,0x36}, then terminator at 0xc0027f24 - which exactly
 *  abuts cdix4192.c's own cdix_config_table start (0xc0027f28, see that
 *  file's own header). All FIVE CDIX register tables in this cluster
 *  (reset_table_b, reset_table_a, config_table, plus mode0/mode1_table
 *  above) are now confirmed to form one unbroken, gap-free 0xc0027e4c-
 *  0xc0027f2c .rodata run - strong, self-consistent confirmation none of
 *  these reads mis-bounded a table (each terminator lands exactly where
 *  the next table's own already-independently-known start address is).
 *  reset_table_a's own first TWO entries ({0x7f,0x00}, {0x03,0x29}) are
 *  byte-identical to cdix_config_table's own first two entries, and its
 *  remaining TEN entries ({0x04,0x03} through {0x01,0x36}) are byte-
 *  identical, same order, to cdix_mode1_table's own tail from its own
 *  {0x04,0x03} entry onward - i.e. reset_table_a's full 12-entry body is
 *  reconstructible as "config_table's opening pair" followed by
 *  "mode1_table's tail," even though all three are laid out as
 *  independent contiguous literal tables rather than any one calling the
 *  others - a real, previously-undocumented shared-subsequence
 *  relationship across all three tables, not forced/approximate (every
 *  matched entry is bit-for-bit identical).
 *
 *  Sole caller (per xrefs_to): FUN_c00101d0 inside FUN_c00100ac - K2's own
 *  audio-pipeline-object constructor, ALSO mcasp_init's (mcasp.c) confirmed
 *  sole caller. I.e. in K2 this CDIX reset-and-configure step and the McASP
 *  peripheral's own init are both called from the SAME constructor function
 *  - a tighter, more directly confirmed version of K1's own "called from
 *  deep inside a USB/MIDI endpoint bring-up routine" finding (K1 traced this
 *  through two more levels of unreconstructed callers, FUN_c000ecc4/
 *  FUN_c00074bc, to reach the same conclusion; K2's own dump resolves it in
 *  one hop since FUN_c00100ac is directly covered).
 * ---------------------------------------------------------------------- */
void cdix_reset_and_configure(void *chip)	/* FUN_c0010ed4 */
{
	extern void *gpio_bank_get_base(void);				/* FUN_c0001710, i2c_by_gpio.c (K1: FUN_c0001990) */
	extern void  hw_timer_busy_wait(void *timer_base, int units);	/* FUN_c000185c, i2c_by_gpio.c (K1: FUN_c0001aa0) */
	extern void  cdix_slot0_size_setter(void);			/* FUN_c0002024 (K1: FUN_c0002308), real body sets *(param_1+0x18)=0x100 - see note above; kept opaque/unmodelled here, same as K1 */
	extern const struct cdix_reg_entry cdix_reset_table_a;	/* DAT_c0010f50 = 0xc0027ef4 */
	extern const struct cdix_reg_entry cdix_reset_table_b;	/* DAT_c0010f54 = 0xc0027ebc */
	const struct cdix_reg_entry *e;

	/* DAT_c0010f4c = 0xC00E004C, shared context handle - see the K2-specific
	 * cross-file finding above. gpio_bank_get_base's canonical K2 prototype
	 * (i2c_by_gpio.c) is `(void)`, ignoring this dead argument entirely. */
	(void)gpio_bank_get_base();
	cdix_slot0_size_setter();	/* FUN_c0002024() - see note above re: its real (possibly implicit) argument */
	hw_timer_busy_wait(0 /* DAT_c0010f4c = 0xC00E004C, dead arg per i2c_by_gpio.c's own documented convention */, 500);

	for (e = &cdix_reset_table_a; e->reg != -1; e++)
		cdix_reg_write(chip, (uint8_t)e->reg, e->value);
	for (e = &cdix_reset_table_b; e->reg != -1; e++)
		cdix_reg_write(chip, (uint8_t)e->reg, e->value);
}

/* ---------------------------------------------------------------------- *
 *  THE OUTER STATE MACHINE - context only, NOT reconstructed here, same
 *  scope boundary K1's own file drew. Traced this pass (further than K1
 *  could, since K2's static dump happens to cover these outer callers
 *  directly - a real coverage improvement over K1, not just a re-citation):
 *
 *   - FUN_c001041c (K2 counterpart of K1's FUN_c000f0c8): calls
 *     cdix_apply_mode_table (this file) via FUN_c0010478, then - matching
 *     K1's own "if detection looks unstable, fall through to the full reset
 *     path" structure - calls FUN_c0010380 (below) on some further
 *     condition. Two static callers of FUN_c001041c were found (FUN_c00104a0
 *     is actually FUN_c0010380 itself, and FUN_c0010418/FUN_c001040c) - not
 *     traced further, out of this file's own scope.
 *   - FUN_c0010380 (K2 counterpart of K1's FUN_c000f01c, the full reset
 *     path): calls cdix_set_format_reg(ctx+0x30, 0) (this file, confirmed
 *     the DEFAULT-format-only call site cited above), delay loops via
 *     hw_timer_busy_wait, and two more calls (FUN_c00102e0, FUN_c0010348)
 *     not reconstructed here.
 *
 *  REAL, CONFIRMED K2-SPECIFIC FINDING, genuinely new versus K1's own
 *  "outer state machine" section (K1 never traced this deep): FUN_c0010380
 *  ALSO directly toggles the mcasp2-reduced-reinit bit-flag helpers this
 *  K2 tree's own omap_l137_addr_gap_misc.c defines
 *  (mcasp2_set_bit25/mcasp2_set_bit15, K2 FUN_c0002c88/FUN_c0002c6c) -
 *  `FUN_c0002c88(*ctx, 0)` near the top of the function and
 *  `FUN_c0002c6c(*ctx, 0)` at the very end, bracketing the CDIX
 *  format-reset call and two delay loops in between. A sibling function,
 *  FUN_c0010258 (also called from the same two callers as FUN_c0010380),
 *  sets BOTH bits the opposite way (`FUN_c0002c6c(*ctx,1)` then
 *  `FUN_c0002c88(*ctx,1)`) - i.e. this looks like a real, confirmed
 *  hardware-level coupling between the CDIX digital-audio-interface reset
 *  path and the McASP2 (second audio serial port instance)'s own bit-25/
 *  bit-15 flags, most plausibly both gating a shared clock/reset domain
 *  the two peripherals share on this board. NOT found/documented in K1's
 *  own cdix_autoswitch.c (K1's own outer-state-machine trace never reached
 *  this deep) - genuinely new evidence about how CDIX and McASP2 interact,
 *  not contradicted by anything K1 documented, just previously unseen.
 *
 *  None of FUN_c001041c/FUN_c0010380/FUN_c0010258/FUN_c00102e0/FUN_c0010348
 *  are reconstructed as function bodies in this file (out of its own
 *  assigned scope, matching K1's own boundary) - cited only to complete the
 *  trace and document the new mcasp2 cross-link.
 * ---------------------------------------------------------------------- */

/* Still open, same shape as K1's own file:
 *  - The 5 CDIX register tables in this cluster (0xc0027e4c-0xc0027f5c,
 *    this file's 4 + cdix4192.c's own 1 at 0xc0027f28) have no readable byte
 *    content in the static dump - a live `read_memory` pass would resolve
 *    all of them in one call, same limitation as K1.
 *  - FUN_c0002024's real argument at its FUN_c0010ed4 call site (implicit
 *    AAPCS r0 carry-through vs. genuine decompiler artifact) - K2's own
 *    dump does NOT resolve this any further than K1's did, see note above.
 *  - Whether cdix_apply_mode_table's `mode` and cdix_set_format_reg's `alt`
 *    are driven by the same underlying detect bit - same open question as
 *    K1, FUN_c001041c itself out of this file's scope.
 *  - The real shared meaning of the mcasp2/CDIX bit-toggle coupling found
 *    this pass in FUN_c0010380/FUN_c0010258 - confirmed to exist, not
 *    decoded against any hardware documentation.
 */
