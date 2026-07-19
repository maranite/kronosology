/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_gpio.c - the NKS4 panel firmware's raw TI OMAP-L1x/DA850 GPIO
 * peripheral register driver: the generic bank-pair SET_DATA/CLR_DATA/
 * IN_DATA/DIR primitives, plus a family of thin per-pin wrappers built on
 * them, that every other subsystem in this firmware (I2C bit-bang, the
 * PSoC panel-reset pulse, McASP bring-up, and a wide, still-unattributed
 * "generic GPIO pulse-toggle utility cluster" cpsoc.c already found and
 * flagged) reaches through to touch discrete GPIO pins.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB (TI OMAP-L1x,
 * ARM926EJ-S, loaded at 0xC0000000), 2026-07-18, read from the pre-fetched
 * dump (all_decompiled.json/all_data.json), not the live Ghidra bridge
 * (flagged concurrency-unsafe for parallel sessions this pass).
 *
 * ANCHOR: NONE. No "../<Name>.cpp" literal string sits anywhere near
 * 0xc0002208-0xc000261f (checked via `strings` search for "Gpio", "GPIO",
 * "Pin", and every other candidate substring - the only GPIO-named string
 * in the whole image is "../I2cByGpio.cpp" at 0xc0022cf8, already claimed
 * by i2c_by_gpio.c for its own, higher-level bit-bang cluster). Same
 * situation aintc.c/panelbus_dispatch.c/heap_alloc.c already document for
 * their own ranges - attribution here rests entirely on register-offset
 * and cross-file evidence, laid out below, not a string xref.
 *
 * ASSIGNED RANGE, AS ACTUALLY OWNED: the task brief that spawned this file
 * described "0xc0002208-0xc0002684, immediately before mcasp.c's own
 * range (which starts at 0xc0002620)". Checking that boundary against the
 * two files that already exist here found it is NOT that clean:
 *
 *   - 0xc0002620 (`FUN_c0002620`) is ALREADY reconstructed, with a real
 *     body, in cpsoc.c as `panel_gpio_level_set` - confirmed cpsoc-PRIVATE
 *     there (exactly 2 callers, both inside cpsoc.c's own
 *     `panel_gpio_reset_pulse`). It happens to sit address-adjacent to
 *     this file's own cluster and to CALL two of this file's own
 *     primitives (`gpio_reg_set_bit`/`gpio_reg_clear_bit`, see below), but
 *     it is not itself part of this file.
 *   - 0xc0002640/0xc0002658/0xc0002668 are ALREADY reconstructed in
 *     mcasp.c (`mcasp_retry_bound_check`/`mcasp_configure_clock`/
 *     `mcasp_configure_pins`).
 *
 * This file's REAL, non-overlapping range is therefore 0xc0002208-
 * 0xc00025ff (26 functions, `FUN_c0002208` through `FUN_c00025e4`) - the
 * dense, gap-free run immediately BEFORE cpsoc.c's `panel_gpio_level_set`
 * at 0xc0002620, not up to the task brief's stated 0xc0002684. Cited to
 * both existing files' own citations above so this doesn't collide.
 *
 * WHY THIS IS THE RAW GPIO REGISTER FILE, not a guess - four independent
 * structural matches:
 *
 *  1. Three generic primitives, `gpio_reg_read_in`/`gpio_reg_set_bit`/
 *     `gpio_reg_clear_bit` (FUN_c0002238/FUN_c00022d0/FUN_c00022e0), each
 *     take (bank_base, pair_index, mask) and touch
 *     `bank_base + pair_index*0x28 + {0x20, 0x18, 0x1c}` respectively.
 *     0x20/0x18/0x1c are IN_DATA/SET_DATA/CLR_DATA on the real TI
 *     OMAP-L138/DA850 GPIO peripheral (TRM "GPIO Bank Registers" table),
 *     which packs TWO physical 16-pin banks per 32-bit register and
 *     repeats its whole DIR/OUT_DATA/SET_DATA/CLR_DATA/IN_DATA/
 *     SET_RIS_TRIG/CLR_RIS_TRIG/SET_FAL_TRIG/CLR_FAL_TRIG/INTSTAT register
 *     block every 0x28 (40) bytes for each bank-pair (01, 23, 45, 67, 89) -
 *     an exact match, not a coincidental offset.
 *  2. `gpio_bank_get_base()` (FUN_c0001990, already canonicalized in
 *     i2c_by_gpio.c) always returns the fixed constant 0x01E26000 - the
 *     documented GPIO controller base on this SoC family - regardless of
 *     its argument. Every call into this file's own primitives that could
 *     be traced back to its ultimate base pointer resolves to that exact
 *     value (see e.g. FUN_c0000aa4's own call chain, outside this file).
 *  3. Direct cross-file confirmation: i2c_by_gpio.c already independently
 *     established "SDA = bank-0 bit 18" for its own `i2c_gpio_set_sda_dir`/
 *     `i2c_gpio_sda_read` (FUN_c00011cc/FUN_c0001198), and cites this
 *     file's own `FUN_c00025ac`/`FUN_c00025e4` (mask 0x40000 = bit 18,
 *     pair index 0) as the physical functions underneath them - an exact,
 *     independently-derived bit-for-bit match confirmed from BOTH sides.
 *  4. cpsoc.c's own `panel_gpio_level_set` (FUN_c0002620, physically the
 *     next function after this file's own range) is a one-line wrapper
 *     over this file's `gpio_reg_set_bit`/`gpio_reg_clear_bit` (pair 3,
 *     mask 8 = bank-6 bit 3) - a second, independent confirmation from a
 *     different existing file that these two primitives are the real,
 *     generic, firmware-wide GPIO set/clear entry points.
 *
 * NAMING: `gpio_reg_set_bit`/`gpio_reg_clear_bit` and `gpio_bank_get_base`
 * reuse the EXACT names/signatures cpsoc.c and i2c_by_gpio.c already
 * declared `extern` for these same functions, to avoid yet another
 * cross-file naming split. `gpio_reg_read_in` is a new name (neither
 * existing file named FUN_c0002238 directly), chosen for symmetry.
 *
 * KNOWN CROSS-FILE OPEN ITEM (not resolved here, flagged honestly): cpsoc.c
 * itself explicitly confirmed "OUT OF SCOPE" a much larger, distant cluster
 * at 0xc0011820 onward (~16+ functions, 0xc0011820-~0xc0012198) that calls
 * INTO several of this file's own wrappers (FUN_c0002450/FUN_c000248c/
 * FUN_c0002338 by cpsoc.c's own citation) - described there as "a generic
 * GPIO pulse-toggle utility cluster... confirmed NOT cpsoc's own code;
 * left unattributed." That cluster is far outside this file's own address
 * range and is NOT reconstructed here either - noted only as the real
 * caller-side identity gap for several of this file's own wrappers below.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Out-of-scope dependency, immediately BELOW this file's own range
 *  (0xc0001fd0 < 0xc0002208) and not claimed by any existing file either
 *  (aintc.c's own header only cites it as the outer bound of its own
 *  string search, does not reconstruct it). Recognizable on register-shape
 *  evidence as a TI OMAP-L138/DA850 PSC (Power and Sleep Controller) LPSC
 *  "enable module" bring-up sequence: MDCTL[module] NEXT field (bits 0-2)
 *  set to Enable (3) at `base + module*4 + 0xA00`; PTCMD GO-bit for power
 *  domain `force` set at `base + 0x120`; busy-wait on PTSTAT GOSTAT at
 *  `base + 0x128`; then busy-wait on MDSTAT[module] STATE field
 *  (`& 0x3f == 3`) at `base + module*4 + 0x800` - all four offsets and the
 *  Enable/state-3 convention are textbook PSC bring-up on this SoC family,
 *  not a guessed shape. Left extern rather than reconstructed here since
 *  it sits outside this file's own assigned range; only `gpio_psc_enable`'s
 *  one caller in this file (`FUN_c0002208` / `omap_psc_enable_module_0x10`,
 *  below) is in scope.
 */
extern void gpio_psc_enable(void *psc_base, int module, int force);	/* FUN_c0001fd0, @0xc0001fd0, NOT reconstructed here - see note above */

/* Canonical singleton GPIO-bank base getter - already reconstructed in
 * i2c_by_gpio.c (FUN_c0001990, 66 firmware-wide callers, always returns
 * the fixed constant 0x01E26000 regardless of its own argument). Reused
 * here by name/signature rather than duplicated. */
extern void *gpio_bank_get_base(void);	/* FUN_c0001990, defined in i2c_by_gpio.c */

/* ========================================================================= *
 *  Generic bank-pair register primitives - the real bottom of this whole
 *  firmware's GPIO access, everything else in this file (and every other
 *  file's own GPIO pokes) is built on these three.
 *
 *  `pair` selects one of the SoC's bank-pair register blocks (0=GPIO0/1,
 *  1=GPIO2/3, 2=GPIO4/5, 3=GPIO6/7, 4=GPIO8/9 - only 0,1,3,4 are ever
 *  actually used by any caller found in this firmware; pair 2 never
 *  appears). Within a 32-bit register, bits 0-15 address the EVEN bank of
 *  the pair, bits 16-31 the ODD bank - e.g. pair 0, bit 18 (0x40000) is
 *  bank-1 bit 2 in raw SoC terms, which is exactly the bit i2c_by_gpio.c
 *  independently calls "bank-0 bit 18" using this file's own "pair index"
 *  as its own informal "bank" terminology - consistent, not contradictory.
 * ========================================================================= */

/* gpio_reg_read_in - reads a bank-pair's IN_DATA register (live pin
 * levels, both directions). @0xc0002238. 7 callers, all within this file
 * (see below) plus FUN_c0000a20 (outside this file's range, not traced
 * here). */
uint32_t gpio_reg_read_in(void *bank_base, int pair)	/* FUN_c0002238 */
{
	return *(uint32_t *)((uint8_t *)bank_base + pair * 0x28 + 0x20);
}

/* gpio_reg_set_bit - writes a bank-pair's SET_DATA register. Real TI
 * SET_DATA registers are self-masking (writing 1 sets that output bit,
 * writing 0 is a no-op on the corresponding bit) - a plain store of `mask`
 * is the correct, complete "set these output bits" operation, no
 * read-modify-write needed; matches the real hardware's own design, not
 * a simplification. NAME/SIGNATURE MATCH cpsoc.c's own extern declaration
 * for this same function - see that file's `panel_gpio_level_set`.
 * @0xc00022d0. 13 firmware-wide callers (per cpsoc.c's own citation),
 * this file accounts for 12 of them internally; the 13th is cpsoc.c's own
 * `panel_gpio_level_set` (FUN_c0002620), outside this file's range. */
void gpio_reg_set_bit(void *bank_base, int pair, uint32_t mask)	/* FUN_c00022d0 */
{
	*(uint32_t *)((uint8_t *)bank_base + pair * 0x28 + 0x18) = mask;
}

/* gpio_reg_clear_bit - sibling CLR_DATA register write, same self-masking
 * design and same cross-file name/signature match as gpio_reg_set_bit
 * above. @0xc00022e0. 16 firmware-wide callers; this file accounts for 15
 * internally, the 16th being cpsoc.c's `panel_gpio_level_set`. */
void gpio_reg_clear_bit(void *bank_base, int pair, uint32_t mask)	/* FUN_c00022e0 */
{
	*(uint32_t *)((uint8_t *)bank_base + pair * 0x28 + 0x1c) = mask;
}

/* ========================================================================= *
 *  gpio_bank_hw_init - the one real "bring-up the whole GPIO block" init
 *  function in this cluster. Writes BINTEN plus DIR/OUT_DATA/edge-trigger
 *  defaults for bank-pairs 0, 1, 3 and 4 (2 is skipped, matching every
 *  other function in this file - the firmware genuinely never touches
 *  GPIO4/5). Takes the GPIO-bank base pointer explicitly (unlike most of
 *  this file's callers, its own caller `FUN_c0000a20` - outside this
 *  file's range - passes it through the same "hidden r0" idiom documented
 *  project-wide: it calls `gpio_bank_get_base()` immediately before this
 *  function with no visible argument on this call, so the register is
 *  still holding that return value).
 *
 *  Field-by-field, matched against the same DIR=+0x10/OUT_DATA=+0x14/
 *  SET_DATA=+0x18/CLR_DATA=+0x1c/IN_DATA=+0x20/SET_RIS_TRIG=+0x24/
 *  CLR_RIS_TRIG=+0x28/SET_FAL_TRIG=+0x2c/CLR_FAL_TRIG=+0x30/INTSTAT=+0x34
 *  per-pair-block layout established above (pair k at base offset
 *  0x28*k):
 *
 *   +0x08              BINTEN (global, not per-pair) = 0x101 - enables the
 *                       AINTC-facing interrupt line for pairs 0 and 8
 *                       (bits 0 and 8 of BINTEN gate pair-N's combined IRQ).
 *   +0x10 (pair 0 DIR)  = 0xFFF3FC7F (all-input default except a handful
 *                       of specific output pins cleared)
 *   +0x14 (pair 0 OUT)  = 0x000C0080 (initial output latch for whichever
 *                       pair-0 pins that DIR value configures as outputs)
 *   +0x28 (pair 0 CLR_RIS_TRIG) = 0x20, +0x2c (pair 0 SET_FAL_TRIG) = 0x20
 *                       - CONFIRMED CROSS-CHECK: both target bit 5 (pair
 *                       0 = bank-0 pin 5), configuring it for a
 *                       falling-edge-only interrupt. This is the EXACT
 *                       same bit `gpio_pair0_intstat_ack_bit5` (FUN_c0002574,
 *                       below) acknowledges (writes INTSTAT01 = 0x20) -
 *                       a real, internally-consistent init/ack pair for
 *                       one specific interrupt source, not a coincidence.
 *   +0x38 (pair 1 DIR)  = 0xFFFFD7FF, +0x3c (pair 1 SET_FAL_TRIG) = 0x2000
 *                       - bit 13 (pair 1 = bank-2 pin 13) is configured as
 *                       OUTPUT by this DIR value (bit clear = output, the
 *                       same TI convention i2c_by_gpio.c already
 *                       documented) yet ALSO gets a falling-edge-trigger
 *                       enable here - genuinely inconsistent for an
 *                       output-configured pin. Transcribed faithfully,
 *                       not smoothed into a cleaner story; possibly a
 *                       harmless blind default-register write (BINTEN
 *                       above never routes pair 1's IRQ line at all, so
 *                       the trigger config is likely inert). This is the
 *                       SAME bit 13 the three-function direction-swap
 *                       cluster below (`gpio_pins_bank2_10_13_*`) also
 *                       manipulates at runtime - see that section's own
 *                       note.
 *   +0x88 (pair 3 DIR)  = 0x8000, +0x8c (pair 3 OUT) = 0xFFFF7FF1
 *   +0xb0 (pair 4 DIR)  = 0xFFFF0FFD, +0xb4 (pair 4 OUT) = 0
 *   +0xc8, +0xcc        = 4, 4 - do NOT land on any DIR/OUT_DATA/trigger
 *                       offset for pair 5 (would need pair-5 DIR at
 *                       +0xd8) or any other pair in this register file.
 *                       NOT IDENTIFIED - left as an honest gap rather
 *                       than forced into the pair-register table; possibly
 *                       a reserved/adjacent register or a software field
 *                       appended past the raw peripheral's own register
 *                       block, not independently confirmed either way.
 *
 *  @0xc0002248. 2 callers, both outside this file's own range
 *  (FUN_c0000a20, FUN_c0000b50 per xrefs_to) - not traced further here.
 * ========================================================================= */
void gpio_bank_hw_init(void *bank_base)	/* FUN_c0002248 */
{
	uint8_t *b = (uint8_t *)bank_base;

	*(uint32_t *)(b + 0x08) = 0x101;		/* BINTEN */

	*(uint32_t *)(b + 0x14) = 0x000c0080;		/* pair 0 OUT_DATA */
	*(uint32_t *)(b + 0x10) = 0xfff3fc7f;		/* pair 0 DIR */
	*(uint32_t *)(b + 0x2c) = 0x20;		/* pair 0 SET_FAL_TRIG, bit5 */
	*(uint32_t *)(b + 0x28) = 0x20;		/* pair 0 CLR_RIS_TRIG, bit5 */

	*(uint32_t *)(b + 0x3c) = 0x2000;		/* pair 1 SET_FAL_TRIG, bit13 */
	*(uint32_t *)(b + 0x38) = 0xffffd7ff;		/* pair 1 DIR */

	*(uint32_t *)(b + 0x8c) = 0x8000;		/* pair 3 OUT_DATA */
	*(uint32_t *)(b + 0x88) = 0xffff7ff1;		/* pair 3 DIR */

	*(uint32_t *)(b + 0xb4) = 0;			/* pair 4 OUT_DATA */
	*(uint32_t *)(b + 0xb0) = 0xffff0ffd;		/* pair 4 DIR */

	*(uint32_t *)(b + 0xcc) = 4;			/* NOT IDENTIFIED, see header note */
	*(uint32_t *)(b + 0xc8) = 4;			/* NOT IDENTIFIED, see header note */
}

/* ------------------------------------------------------------------------- *
 *  omap_psc_enable_module_0x10 - two back-to-back PSC "enable module"
 *  calls (see gpio_psc_enable/FUN_c0001fd0's own citation above) for PSC
 *  module number 0x10 (16), once with `force`=1 then once with `force`=0.
 *  The real per-domain/force meaning of that third argument is defined in
 *  FUN_c0001fd0 itself, outside this file's range - not re-derived here.
 *  Which physical peripheral PSC module 16 actually gates on this SoC
 *  variant is NOT identified this pass (would need the real DA850 PSC1
 *  module table, not available to this static-dump pass).
 *
 *  Sole caller: `FUN_c000383c` (@0xc000383c, well outside this file's own
 *  range - not reconstructed here). That caller's own shape (a 16-entry
 *  lookup-table zero/copy loop feeding a small descriptor struct, then
 *  this reset-like double PSC-enable call) looks display/lookup-table
 *  related, but is NOT claimed by clcdc.c's own address range either
 *  (0xc0015010-0xc0015820) - genuinely unattributed; flagged here as a
 *  cross-file open item rather than guessed.
 *
 *  @0xc0002208.
 * ------------------------------------------------------------------------- */
void omap_psc_enable_module_0x10(void *psc_base)	/* FUN_c0002208 */
{
	gpio_psc_enable(psc_base, 0x10, 1);
	gpio_psc_enable(psc_base, 0x10, 0);
}

/* ========================================================================= *
 *  Thin fixed-bit wrappers over gpio_reg_set_bit/gpio_reg_clear_bit. Each
 *  is a one-line convenience shim baking in one specific (pair, mask) -
 *  the same shape as cpsoc.c's own `panel_gpio_level_set` (pair 3, mask
 *  8), just for different pins. Real caller identities noted where known;
 *  several trace into cpsoc.c's own explicitly-flagged "generic GPIO
 *  pulse-toggle utility cluster... confirmed NOT cpsoc's own code" at
 *  0xc0011820 onward - that cluster is not reconstructed in this file
 *  either (well outside this file's own address range).
 * ========================================================================= */

/* gpio_pair1_bit11_clear/_set - CLR_DATA23/SET_DATA23 bit 11 (pair 1 =
 * bank-2/3, bit 11 = bank-2 pin 11). Single caller each, both inside
 * `FUN_c000f0c8` (outside this file's range, not traced - a matched
 * clear-then-set pair strongly suggests a brief reset/strobe pulse on one
 * discrete line from that caller, not independently confirmed).
 * @0xc00022f0 / @0xc00022fc. */
void gpio_pair1_bit11_clear(void *bank_base)	/* FUN_c00022f0 */
{
	gpio_reg_clear_bit(bank_base, 1, 0x800);
}

void gpio_pair1_bit11_set(void *bank_base)	/* FUN_c00022fc */
{
	gpio_reg_set_bit(bank_base, 1, 0x800);
}

/* gpio_pair0_bit8_set - SET_DATA01 bit 8 (bank-0 pin 8). Single caller
 * `FUN_c000fb0c`, outside this file's range, not traced. @0xc0002308. */
void gpio_pair0_bit8_set(void *bank_base)	/* FUN_c0002308 */
{
	gpio_reg_set_bit(bank_base, 0, 0x100);
}

/* gpio_pair3_bit15_set - SET_DATA67 bit 15 (pair 3 = bank-6/7, bit 15 =
 * bank-6 pin 15). Single caller `FUN_c0000aa4`, outside this file's
 * range - that caller's own body (traced while establishing this file's
 * scope) is a McASP0-adjacent bring-up routine that also calls
 * `gpio_bank_get_base()`/`gpio_pair4_bit_set` below via the same "hidden
 * r0" idiom documented throughout this project; likely a McASP-related
 * reset or clock-enable GPIO pulse, not independently confirmed.
 * @0xc0002320. */
void gpio_pair3_bit15_set(void *bank_base)	/* FUN_c0002320 */
{
	gpio_reg_set_bit(bank_base, 3, 0x8000);
}

/* gpio_pair4_bit_clear/_set - CLR_DATA89/SET_DATA89 (pair 4 = bank-8/9),
 * caller-supplied mask (not a fixed bit like the wrappers above). 4 and 3
 * callers respectively, none inside this file - per cpsoc.c's own
 * explicit citation, these are exactly the two primitives its "generic
 * GPIO pulse-toggle utility cluster" (0xc0011820 onward, confirmed NOT
 * cpsoc's own code, not reconstructed anywhere in this project yet) is
 * built on. @0xc0002338 / @0xc0002344. */
void gpio_pair4_bit_clear(void *bank_base, uint32_t mask)	/* FUN_c0002338 */
{
	gpio_reg_clear_bit(bank_base, 4, mask);
}

void gpio_pair4_bit_set(void *bank_base, uint32_t mask)	/* FUN_c0002344 */
{
	gpio_reg_set_bit(bank_base, 4, mask);
}

/* ========================================================================= *
 *  gpio_pins_bank2_10_13_* - three direction/level configuration variants
 *  for the SAME two pins, bank-2 pin 10 and bank-2 pin 13 (pair 1's low
 *  half), plus (variant "a" only) a one-shot DIR01 bits 20-21 (bank-1 pin
 *  4/5) output-configure. All three directly poke DIR23 (bank_base+0x38)
 *  rather than going through this file's own gpio_reg_* primitives for
 *  the DIR write (only the SET_DATA/CLR_DATA parts route through them) -
 *  transcribed exactly as decompiled, not normalized.
 *
 *  Real, confirmed-by-register-evidence roles:
 *    variant_a (FUN_c0002350): pin13->INPUT, pin10->OUTPUT (driven low);
 *                              ALSO configures bank-1 pins 4/5 as outputs.
 *    variant_tristate (FUN_c00023a4): pin13->INPUT, pin10->INPUT (both
 *                              released, outputs driven low first).
 *    variant_b (FUN_c00023ec): pin13->OUTPUT (driven high), pin10->INPUT
 *                              - the exact role-reversal of variant_a for
 *                              these same two pins.
 *
 *  This shape (two pins whose direction and drive level swap between two
 *  complementary configurations, plus a "both released" middle state) is
 *  the classic pattern for a half-duplex, direction-switched 2-wire
 *  handshake/strobe pair - a real, structurally-confirmed observation,
 *  NOT a confirmed protocol identity. No caller was found in the
 *  691-function xref data for `variant_a`'s siblings: `FUN_c00023a4` has
 *  ZERO xrefs_to entries, and `FUN_c00023ec` has exactly one
 *  (`FUN_c0011b44`, itself part of cpsoc.c's already-flagged
 *  out-of-scope 0xc0011820+ cluster). `variant_a` (FUN_c0002350) has 2
 *  callers (`FUN_c00118b4`, `FUN_c00056a4`), both also outside this
 *  file's range and not traced here. Given zero/near-zero static callers
 *  for two of the three and no corroborating string/struct evidence,
 *  which higher-level bus (if any - possibly panelbus_dispatch.c's or
 *  wire_dispatch.c's own internal command channel, given the swap
 *  pattern, but NOT confirmed by any call-site evidence found this pass)
 *  these two pins actually belong to is left genuinely open.
 *
 *  @0xc0002350 / @0xc00023a4 / @0xc00023ec.
 * ========================================================================= */
void gpio_pins_bank2_10_13_variant_a(void *bank_base)	/* FUN_c0002350 */
{
	uint8_t *b = (uint8_t *)bank_base;

	gpio_reg_clear_bit(bank_base, 1, 0x400);	/* CLR_DATA23 bit10 */
	gpio_reg_clear_bit(bank_base, 1, 0x2000);	/* CLR_DATA23 bit13 */
	*(uint32_t *)(b + 0x38) |= 0x2000;		/* DIR23 bit13 -> input */
	*(uint32_t *)(b + 0x38) &= 0xfffffbff;		/* DIR23 bit10 -> output */
	*(uint32_t *)(b + 0x10) &= 0xffcfffff;		/* DIR01 bits20-21 -> output */
}

void gpio_pins_bank2_10_13_tristate(void *bank_base)	/* FUN_c00023a4 */
{
	uint8_t *b = (uint8_t *)bank_base;

	gpio_reg_clear_bit(bank_base, 1, 0x400);	/* CLR_DATA23 bit10 */
	gpio_reg_clear_bit(bank_base, 1, 0x2000);	/* CLR_DATA23 bit13 */
	*(uint32_t *)(b + 0x38) |= 0x2000;		/* DIR23 bit13 -> input */
	*(uint32_t *)(b + 0x38) |= 0x400;		/* DIR23 bit10 -> input */
}

void gpio_pins_bank2_10_13_variant_b(void *bank_base)	/* FUN_c00023ec */
{
	uint8_t *b = (uint8_t *)bank_base;

	gpio_reg_set_bit(bank_base, 1, 0x2000);	/* SET_DATA23 bit13 (drive high) */
	gpio_reg_clear_bit(bank_base, 1, 0x400);	/* CLR_DATA23 bit10 (drive low) */
	*(uint32_t *)(b + 0x38) |= 0x400;		/* DIR23 bit10 -> input */
	*(uint32_t *)(b + 0x38) &= 0xffffdfff;		/* DIR23 bit13 -> output */
}

/* ========================================================================= *
 *  Pair-0, bit-20/21 direction+level helpers (bank-1 pins 4/5 - the same
 *  two pins `gpio_pins_bank2_10_13_variant_a` above configures as outputs
 *  via its own direct DIR01 write). Bit 20 gets both a direction toggle
 *  (this section) and a level toggle (below); bit 21 gets only a level
 *  toggle here - its own direction is set only by variant_a's combined
 *  bits20-21 write above, not individually.
 * ========================================================================= */

/* gpio_pair0_bit20_dir - direct DIR01 bit 20 toggle (bank-1 pin 4).
 * `input` nonzero -> configure as input; zero -> output. Same TI
 * convention as i2c_by_gpio.c's `gpio_bank_set_dir_bit`. 2 callers
 * (`FUN_c0011c50`, `FUN_c0012098`), both outside this file's range, not
 * traced. @0xc0002434. */
void gpio_pair0_bit20_dir(void *bank_base, int input)	/* FUN_c0002434 */
{
	uint32_t *dir = (uint32_t *)((uint8_t *)bank_base + 0x10);

	if (input == 0)
		*dir &= 0xffefffff;
	else
		*dir |= 0x100000;
}

/* gpio_pair0_bit20_level - SET_DATA01/CLR_DATA01 bit 20 via the generic
 * primitives (unlike the direct-DIR wrappers above, this one routes
 * through gpio_reg_set_bit/gpio_reg_clear_bit like the rest of this
 * file's ordinary level wrappers). `level` nonzero -> set, zero -> clear.
 * 8 callers, all outside this file's range - per cpsoc.c's own citation,
 * part of its flagged-but-unattributed 0xc0011820+ cluster plus two
 * `FUN_c00056a4`-family callers. @0xc0002450. */
void gpio_pair0_bit20_level(void *bank_base, int level)	/* FUN_c0002450 */
{
	if (level != 0)
		gpio_reg_set_bit(bank_base, 0, 0x100000);
	else
		gpio_reg_clear_bit(bank_base, 0, 0x100000);
}

/* gpio_pair0_bit20_status - reads IN_DATA01 bit 20 (live level of
 * bank-1 pin 4) as a 0/1 boolean. 5 callers, all outside this file's
 * range (all in cpsoc.c's flagged 0xc0011820+ cluster per address).
 * @0xc000246c. */
uint32_t gpio_pair0_bit20_status(void *bank_base)	/* FUN_c000246c */
{
	return (gpio_reg_read_in(bank_base, 0) >> 0x14) & 1;
}

/* gpio_pair0_bit21_level - SET_DATA01/CLR_DATA01 bit 21 (bank-1 pin 5),
 * same shape as gpio_pair0_bit20_level above, no matching direct-DIR
 * sibling in this file (bit 21's own direction is set only via
 * `gpio_pins_bank2_10_13_variant_a`'s combined DIR01 bits20-21 clear).
 * 12 callers, all outside this file's range (same 0xc0011820+ cluster
 * family). @0xc000248c. */
void gpio_pair0_bit21_level(void *bank_base, int level)	/* FUN_c000248c */
{
	if (level != 0)
		gpio_reg_set_bit(bank_base, 0, 0x200000);
	else
		gpio_reg_clear_bit(bank_base, 0, 0x200000);
}

/* ========================================================================= *
 *  Remaining pair-0/pair-3/pair-4 single-purpose helpers.
 * ========================================================================= */

/* gpio_pair0_bit2_set - SET_DATA01 bit 2 (bank-0 pin 2). Single caller
 * `FUN_c0014f84` (outside this file's range - a UART/timer-shaped
 * bring-up routine that also calls this file's `gpio_pair0_bit1_set_
 * if_ready` below; not otherwise traced this pass). @0xc00024a8. */
void gpio_pair0_bit2_set(void *bank_base)	/* FUN_c00024a8 */
{
	gpio_reg_set_bit(bank_base, 0, 4);
}

/* gpio_pair0_bit1_set_if_ready / _clear_if_ready - a matched pair: both
 * read pair-0's full IN_DATA word, bail if its sign bit (bit 31 = bank-1
 * pin 15) is set, and otherwise SET or CLR bit 1 of PAIR 3 (bank-6 pin 1)
 * - note the read and write target DIFFERENT bank-pairs, a genuine
 * cross-pin gated-write idiom, transcribed exactly as decompiled. Real
 * meaning (what bank-1 pin 15 "readiness" gates on bank-6 pin 1) not
 * identified this pass.
 *
 * `gpio_pair0_bit1_set_if_ready`: 1 caller (`FUN_c0014f84`, outside this
 * file's range, see gpio_pair0_bit2_set above). @0xc00024b4.
 * `gpio_pair0_bit1_clear_if_ready`: ZERO callers found in the
 * 691-function xref data - genuinely unreachable by static analysis, same
 * "confirmed zero callers" situation crypto_at88.c documents for its own
 * `crypto_at88_self_test`. @0xc00024e8.
 * ========================================================================= */
void gpio_pair0_bit1_set_if_ready(void *bank_base)	/* FUN_c00024b4 */
{
	if ((int32_t)gpio_reg_read_in(bank_base, 0) < 0)
		return;
	gpio_reg_set_bit(bank_base, 3, 2);
}

void gpio_pair0_bit1_clear_if_ready(void *bank_base)	/* FUN_c00024e8 */
{
	if ((int32_t)gpio_reg_read_in(bank_base, 0) < 0)
		return;
	gpio_reg_clear_bit(bank_base, 3, 2);
}

/* gpio_pair4_bit1_toggle - reads pair-4's IN_DATA bit 1 (bank-8 pin 1)
 * and writes the OPPOSITE value back via SET_DATA/CLR_DATA - a genuine
 * read-modify-write toggle idiom, not a fixed set or clear. ZERO callers
 * found in the 691-function xref data - same "confirmed unreachable by
 * static analysis" situation as gpio_pair0_bit1_clear_if_ready above.
 * @0xc000251c. */
void gpio_pair4_bit1_toggle(void *bank_base)	/* FUN_c000251c */
{
	uint32_t in = gpio_reg_read_in(bank_base, 4);

	if ((in & 2) == 0)
		gpio_reg_set_bit(bank_base, 4, 2);
	else
		gpio_reg_clear_bit(bank_base, 4, 2);
}

/* gpio_pair0_intstat_ack_bit5 - writes INTSTAT01 = 0x20 (write-1-to-clear
 * semantics on real TI GPIO INTSTAT registers), acknowledging bank-0 pin
 * 5's pending edge interrupt. CONFIRMED paired with `gpio_bank_hw_init`
 * above, which configures that exact same bit for falling-edge-only
 * triggering - see that function's own header note. Single caller
 * `FUN_c00009d8` (outside this file's range - that caller also writes two
 * fields at offsets +0x24/+0x2c off `aintc_base()`'s own return value,
 * i.e. real AINTC channel-priority/CMR-adjacent registers per aintc.c's
 * own documented offset table - meaning this function's caller is a
 * combined GPIO-ack + AINTC-configure routine, most likely the real ISR
 * install/ack site for bank-0 pin 5's interrupt; not itself resolved or
 * claimed by aintc.c's own address range either, left as a cross-file
 * open item). @0xc0002574. */
void gpio_pair0_intstat_ack_bit5(void *bank_base)	/* FUN_c0002574 */
{
	*(uint32_t *)((uint8_t *)bank_base + 0x34) = 0x20;
}

/* gpio_pair0_bit6_read - reads IN_DATA01 bit 6 (bank-0 pin 6) as a 0/1
 * boolean. 4 callers spanning several unrelated subsystems by address
 * (`FUN_c000ef60`, `FUN_c0008b64`, `FUN_c000f0c8`, `FUN_c0006578`, all
 * outside this file's range) - consistent with a single shared status/
 * ready input pin polled by multiple independent drivers, not identified
 * further this pass. @0xc0002588. */
uint32_t gpio_pair0_bit6_read(void *bank_base)	/* FUN_c0002588 */
{
	return (gpio_reg_read_in(bank_base, 0) & 0x40) != 0;
}

/* ========================================================================= *
 *  Bank-0 bit 18 (SDA) - CONFIRMED cross-file: physically part of this
 *  file's generic register cluster (address-adjacent to everything else
 *  above), but functionally I2C-exclusive - i2c_by_gpio.c's own
 *  `i2c_gpio_set_sda_dir`/`i2c_gpio_sda_read` (FUN_c00011cc/FUN_c0001198)
 *  are these two functions' ONLY real callers, and that file already
 *  declares matching `extern` prototypes for both under the names reused
 *  verbatim here (`gpio_bank_set_dir_bit`/`gpio_bank_read_sda_bit`) - see
 *  i2c_by_gpio.c's own SCOPE NOTE, which independently arrived at "these
 *  physically live in a different, generic GPIO-bank-register file" and
 *  named it correctly without yet having this file to point to.
 * ========================================================================= */

/* gpio_bank_set_dir_bit - direct DIR01 bit 18 toggle. `input` nonzero ->
 * configure as input, zero -> output (same TI convention as
 * gpio_pair0_bit20_dir above). 1 caller: i2c_by_gpio.c's
 * `i2c_gpio_set_sda_dir`. @0xc00025ac. */
void gpio_bank_set_dir_bit(void *bank_base, int input)	/* FUN_c00025ac */
{
	uint32_t *dir = (uint32_t *)((uint8_t *)bank_base + 0x10);

	if (input == 0)
		*dir &= 0xfffbffff;
	else
		*dir |= 0x40000;
}

/* gpio_bank_read_sda_bit - reads IN_DATA01 bit 18 (live SDA line level)
 * as a 0/1 boolean. 1 caller: i2c_by_gpio.c's `i2c_gpio_sda_read`.
 * @0xc00025e4. */
uint32_t gpio_bank_read_sda_bit(void *bank_base)	/* FUN_c00025e4 */
{
	return (gpio_reg_read_in(bank_base, 0) >> 0x12) & 1;
}
