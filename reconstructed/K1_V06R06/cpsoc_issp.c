/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cpsoc_issp.c - a bit-banged Cypress PSoC1 ISSP (In-System Serial
 * Programming) driver for up to 4 companion PSoC1 chips on the front-panel
 * board, plus the one board-bring-up entry point built on top of it.
 *
 * SCOPE / HOW THIS FILE WAS FOUND: assigned sweep range 0xc001182c-0xc001288c
 * (~31 functions). Two independent prior passes had already looked at this
 * exact address range and explicitly punted on it:
 *   - cpsoc.c's own comment calls it "a generic GPIO pulse-toggle cluster
 *     (~16 functions)... confirmed NOT cpsoc's own code; left unattributed"
 *     (see cpsoc.c's "CONFIRMED OUT OF SCOPE" list, citing 0xc0011820-
 *     ~0xc0012198).
 *   - omap_gpio.c's own comment independently reaches the same
 *     "still genuinely unattributed" conclusion, while noting the cluster
 *     calls into ITS OWN gpio_pair4_bit_set/gpio_pair4_bit_clear and
 *     gpio_pair0_bit20_level/gpio_pair0_bit21_level wrappers.
 * Both were right that it isn't cpsoc.c's own code (no "../cpsoc.cpp" fault
 * string, no cpsoc+0x820/+0x821 scratch convention, no reg 0x78-0x7b I2C-
 * dispatch calls) - but a full decompile of every function in the range,
 * cross-checked against the well-documented Cypress PSoC1 ISSP protocol
 * (Cypress AN2026A "PSoC1 In-System Serial Programming"), makes what it
 * actually drives unambiguous:
 *
 *   - issp_send_vector below builds an exact 22-bit "vector": 5 fixed
 *     preamble bits ('1','0','0','1','0'), 6 variable address bits, 8
 *     variable data bits, 3 fixed stop bits ('1','1','1') - this is
 *     Cypress's own documented PSoC1 ISSP WRITE-BYTE vector bit layout,
 *     not a coincidental shape.
 *   - Programming/verify data is moved in 64-byte pages (issp_program_blocks/
 *     issp_verify_blocks both loop in units of 0x40 bytes) - PSoC1 Flash is
 *     itself organized in 64-byte blocks; this is the textbook page size,
 *     not a guessed one.
 *   - issp_read_silicon_id and issp_read_checksum both read back a 16-bit
 *     value after a device-family-dependent setup vector - exactly
 *     matching ISSP's own "read Silicon ID" / "read Checksum" steps.
 *   - issp_probe_and_init_target branches its whole vector-table selection
 *     on the low byte of the just-read Silicon ID (values '4'/0x14/0x19/
 *     0x0b seen in this range) - PSoC1's own documented behavior of using
 *     different INIT-2/verify-setup macro vectors per silicon revision.
 *   - Devices are selected one at a time via 4 chip-select bits (GPIO pair
 *     4 / bank 8-9, bits 12-15) using the SAME four "register bank" values
 *     (0x78/0x79/0x7a/0x7b, plus 0xff = "select all four at once") that
 *     cpsoc.c's OWN runtime driver happens to reuse for its two I2C-
 *     dispatch register banks - a real, confirmed naming coincidence
 *     between two unrelated protocols to what may be the same physical
 *     chips (this file's ISSP flasher talks to them at boot/field-update
 *     time; cpsoc.c's cpsoc_i2c_dispatch talks to them at runtime), NOT
 *     evidence they're the same code.
 *
 * NOT resolved: no `__FILE__` anchor string was found for this range (the
 * "Psoc version error %02x != %02x : Id %03d" / "Fail to send data to psoc
 * : %d" strings at 0xc0022e6c/0xc00231a0 sit in the SAME literal-pool
 * neighborhood as "../cpsoc.cpp" and read exactly like they belong to this
 * cluster's own ID-mismatch/write-failure paths - the "%02x != %02x : Id
 * %03d" shape is a very good match for issp_read_silicon_id's own 2-byte ID
 * readback - but no function anywhere in the 691-function decompile dump
 * visibly loads either string's address, so the real caller is NOT
 * identified here. Flagged as a concrete lead for whoever revisits cpsoc.c
 * next (that file's own literal pool, not this one's).
 *
 * Named `cpsoc_issp.c` (not `cpsoc.c`) precisely because it is proven NOT
 * to share cpsoc.cpp's own translation unit - "cpsoc" only because it
 * plausibly programs the very same class of chip cpsoc.c talks to at
 * runtime.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB, 2026-07-18 pass
 * (all_decompiled.json / all_data.json, live MCP bridge not used - see
 * this project's README "2026-07-18" section for why).
 */

#include <stdint.h>

/* ===========================================================================
 * Cross-file dependencies - all already reconstructed/cited elsewhere,
 * declared here with the SAME names/signatures those files already chose
 * (project convention: avoid yet another naming split).
 * ======================================================================== */

/* omap_gpio.c - generic bank-pair GPIO primitives. */
extern void    *gpio_bank_get_base(void);				/* FUN_c0001990, i2c_by_gpio.c - always returns the fixed GPIO-controller base regardless of its own (ignored) argument */
extern void     gpio_pair4_bit_clear(void *bank_base, uint32_t mask);	/* FUN_c0002338, omap_gpio.c - CLR_DATA89 (pair 4 = bank 8/9); THIS file's own 4 chip-select bits (0x1000/0x2000/0x4000/0x8000) live here */
extern void     gpio_pair4_bit_set(void *bank_base, uint32_t mask);	/* FUN_c0002344, omap_gpio.c - SET_DATA89 sibling of the above */
extern void     gpio_pair0_bit20_dir(void *bank_base, int input);	/* FUN_c0002434, omap_gpio.c - direction toggle for the SDATA line (bank-1 pin 4) */
extern void     gpio_pair0_bit20_level(void *bank_base, int level);	/* FUN_c0002450, omap_gpio.c - SDATA level drive */
extern uint32_t gpio_pair0_bit20_status(void *bank_base);		/* FUN_c000246c, omap_gpio.c - SDATA live level read */
extern void     gpio_pair0_bit21_level(void *bank_base, int level);	/* FUN_c000248c, omap_gpio.c - SCLK level drive (bank-1 pin 5) */
extern void     gpio_pins_bank2_10_13_variant_a(void *bank_base);	/* FUN_c0002350, omap_gpio.c - also configures DIR01 bits 20-21 (SDATA/SCLK) as OUTPUT as a side effect; this file's own issp_bus_prepare relies on that side effect */
extern void     gpio_pins_bank2_10_13_variant_b(void *bank_base);	/* FUN_c00023ec, omap_gpio.c - the complementary/tristate-adjacent variant this file's own issp_release_bus uses */

/* soc_periph.c - misc SoC base getters / pinmux word installers. Real
 * meaning of the two base getters is itself still open in soc_periph.c's
 * own words ("unidentified 0x01E2C000... no confident attribution") - used
 * here exactly as that file's own callers use them, not reinterpreted. */
extern uint32_t syscfg0_base_get(void *chip);				/* FUN_c0001948, soc_periph.c */
extern uint32_t board_periph_base_unknown_195c(void *chip);		/* FUN_c0001954, soc_periph.c */
extern void     board_desc_set_pinmux_130_a(int handle);		/* FUN_c0001e48, soc_periph.c */
extern void     board_desc_set_pinmux_130_b(int handle);		/* FUN_c0001e58, soc_periph.c */
extern void     board_desc_clear_status_bit11(int handle);		/* FUN_c0001e88, soc_periph.c */
extern void     hw_timer_busy_wait(void *chip, int units);		/* FUN_c0001aa0, soc_periph.c - the real engine underneath this file's own issp_target_delay */

/* panelbus_dispatch.c / eva_board_main.c both already claim FUN_c0001a00
 * under different names/signatures (panelbus_i2c_base(uint32_t,int)->struct
 * omap_i2c_regs* vs eva_board_probe_bus_handle(void*,int)->void*) - a
 * pre-existing cross-file discrepancy this file did not introduce and does
 * not resolve. Declared minimally here matching this file's own call-site
 * usage (return value is passed straight through as an opaque handle). */
extern void *panelbus_i2c_base(void *unused, int select_i2c1);	/* FUN_c0001a00, see panelbus_dispatch.c / eva_board_main.c for the two competing names */

/* omap_l137_addr_gap_misc.c - unattributed config-slot helper; that file's
 * own comment already anticipates being called from "a cluster around
 * 0xc0012xxx, neither reconstructed by any file so far" - this file. */
extern void usbdc_gap_config_slot(void *obj, int mode);		/* FUN_c00032a8, omap_l137_addr_gap_misc.c */

/* ===========================================================================
 * Bit-stream vector-data tables - raw ISSP macro-vector blobs baked into
 * the firmware image. Each is consumed by issp_clock_bits as a flat
 * MSB-first bitstream of the cited length; contents are NOT decoded here
 * (same treatment eva_board_main.c already gives this cluster's "dense
 * bus-transaction arithmetic" - cited by address/length, not guessed at).
 * ======================================================================== */
extern const uint32_t issp_vec_init2_a[];		/* DAT_c0011e40, issp_write_block_and_lock: device-type '4'/0x14/0x19 branch, 0x134 bits */
extern const uint32_t issp_vec_init2_b[];		/* DAT_c0011e44, issp_write_block_and_lock: device-type 0x0b branch, 0x134 bits */
extern const uint32_t issp_vec_erase_setup[];		/* DAT_c0011e74, issp_erase_or_init, 0x134 bits */
extern const uint32_t issp_vec_id_setup_1[];		/* DAT_c0011ee8, issp_init2_sequence step 1, 0x18c (396) bits */
extern const uint32_t issp_vec_id_setup_2a[];		/* DAT_c0011eec, issp_init2_sequence step 2, length DAT_c0011ef0 = 0x11e (286) bits */
extern const uint32_t issp_vec_id_setup_3[];		/* DAT_c0011ef4, issp_init2_sequence step 3, 0x344 (836) bits */
extern const uint32_t issp_vec_verify_select[];	/* DAT_c0011f78, issp_select_verify_block trailer, 0x108 (264) bits */
extern const uint32_t issp_vec_program_a[];		/* DAT_c0012028, issp_program_block_trigger: device-type '4'/0x14/0x19 branch, 0x134 bits */
extern const uint32_t issp_vec_program_b[];		/* DAT_c001202c, issp_program_block_trigger: device-type 0x0b branch, 0x134 bits */
extern const uint32_t issp_vec_checksum_a[];		/* DAT_c001227c, issp_read_checksum: device-type branch, length DAT_c0012280 = 0x11e bits */
extern const uint32_t issp_vec_checksum_b[];		/* DAT_c0012284, issp_read_checksum: device-type 0x0b branch, same length */
extern const uint32_t issp_vec_checksum_read_hi[];	/* DAT_c0012288, issp_read_checksum trailer 1, 0xb (11) bits */
extern const uint32_t issp_vec_checksum_read_lo[];	/* DAT_c001228c, issp_read_checksum trailer 2, 0xc (12) bits */
extern const uint32_t issp_vec_id_read_setup[];	/* DAT_c0012338, issp_read_silicon_id, length DAT_c001233c = 0x14a (330) bits */
extern const uint32_t issp_vec_id_read_hi[];		/* DAT_c0012340, issp_read_silicon_id trailer 1, 0xb bits */
extern const uint32_t issp_vec_id_read_lo[];		/* DAT_c0012344, issp_read_silicon_id trailer 2, 0xb bits */
/* DAT_c0011f74 and DAT_c0012024 resolve to the IDENTICAL data address (both
 * "-0x3ffe0258" per the resolved-constant dump) - one shared 0xb-bit
 * "select block N" prefix vector used by BOTH issp_select_verify_block and
 * issp_program_block_trigger, not two separate tables. */
extern const uint32_t issp_vec_select_block_prefix[];	/* DAT_c0011f74 == DAT_c0012024, 0xb bits */
extern const uint32_t issp_vec_wait_trailer[];		/* DAT_c0011db4, issp_wait_ready's own final trailer vector, 0x28 (40) bits */
extern const uint8_t  issp_flash_image[];		/* DAT_c0012720 - 0x1000, program data for eva_board_probe_summary's 4 targets */
extern const uint8_t  issp_security_row[];		/* DAT_c0012720, same base, used as issp_write_block_and_lock's own security-row byte count/pointer - see eva_board_probe_summary's own call-site note */

/* NOTE: DAT_c0012790 (cpsoc.c's own `cpsoc_i2c_handle`) is NOT this file's
 * handle - it belongs entirely to cpsoc.c's unrelated runtime I2C-dispatch
 * path (cpsoc_read_switch_or_led). This file's own per-call bus handles
 * come from gpio_bank_get_base()/panelbus_i2c_base() directly; no shared
 * global of its own was found. */

/* ===========================================================================
 * Timing primitives
 * ======================================================================== */

/* issp_target_delay - thin per-cluster wrapper over soc_periph.c's shared
 * hw_timer_busy_wait. `handle` is forwarded untouched (call-site-shape
 * fidelity only, per this project's established "hidden r0" idiom - see
 * i2c_by_gpio.c's own note on the same pattern); `units` is genuinely used.
 * NOTE: physically sits at 0xc0011820, 12 bytes BEFORE this sweep's nominal
 * 0xc001182c start. Included here anyway: all 16 of its callers (verified
 * via xrefs_to) are inside this file's own functions - it is this
 * cluster's own private delay primitive, not cpsoc.c's or omap_gpio.c's
 * (neither references it), so leaving it out under either of those files
 * would be a worse fit than the 12-byte range extension. @0xc0011820. */
void issp_target_delay(void *handle, int units)	/* FUN_c0011820 */
{
	hw_timer_busy_wait(handle, units);
}

/* issp_target_delay_ms - millisecond-scaled sibling, tail-calls the above
 * with units*1000. Only 2 callers, both inside this file. @0xc001182c. */
void issp_target_delay_ms(void *handle, int ms)	/* FUN_c001182c */
{
	issp_target_delay(handle, ms * 1000);
}

/* ===========================================================================
 * Bit/vector clocking primitives - the real bottom of this file's ISSP
 * bus, everything else below is built on these three plus
 * issp_target_delay above.
 * ======================================================================== */

/* issp_clock_bit - drive SDATA to one bit, pulse SCLK high then low, each
 * edge separated by a 1-unit issp_target_delay. 28 callers, all internal.
 * @0xc001183c. */
void issp_clock_bit(void *handle, uint32_t bit)	/* FUN_c001183c */
{
	void *base = gpio_bank_get_base();

	gpio_pair0_bit20_level(base, bit & 1);		/* SDATA */
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 1);		/* SCLK high */
	issp_target_delay(handle, 1);
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 0);		/* SCLK low */
	issp_target_delay(handle, 1);
}

/* issp_clock_bits - shifts `nbits` bits out of `words` (an array of 32-bit
 * words, up to 32 bits consumed per word, MSB-first), one issp_clock_bit
 * call per bit. Used both for the fixed 22-bit R/W vectors below and for
 * the much longer device-family macro-vector blobs (up to 0x344 = 836
 * bits). 18 callers, all internal. @0xc001193c. */
void issp_clock_bits(void *handle, const uint32_t *words, uint32_t nbits)	/* FUN_c001193c */
{
	uint32_t nwords = nbits / 32;
	uint32_t w, word, bits_left;

	if (nwords * 32 < nbits)
		nwords++;
	if (nwords == 0)
		return;

	for (w = 0; w < nwords; w++) {
		word = words[w];
		bits_left = (nbits > 31) ? 32 : nbits;
		for (; bits_left != 0; bits_left--) {
			issp_clock_bit(handle, word >> 31);
			word <<= 1;
			nbits--;
		}
	}
}

/* ===========================================================================
 * Bus prepare / idle helpers - re-prime the SDATA/SCLK direction and CS
 * lines. Called at the start of nearly every higher-level function below.
 * ======================================================================== */

/* issp_bus_prepare - (re)configures SDATA/SCLK (GPIO bank-1 pins 4/5) as
 * OUTPUTs via gpio_pins_bank2_10_13_variant_a's own DIR01 bits-20/21 side
 * effect, applies pinmux word "130_a", and additionally sets bit 11 (mask
 * 0x800) at offsets +0xc/+0x10 off the board_periph_base_unknown_195c base
 * - a second, still-unidentified pad/pinmux register pair (soc_periph.c's
 * own base getter has no confirmed identity either; transcribed exactly as
 * decompiled, not reinterpreted). 10 callers, all internal. @0xc00118b4. */
void issp_bus_prepare(void)	/* FUN_c00118b4 */
{
	uint32_t *periph;

	syscfg0_base_get(0);			/* return value unused - side-effecting call only, transcribed as-is */
	board_desc_set_pinmux_130_a(0);
	gpio_bank_get_base();			/* return value unused here */
	gpio_pins_bank2_10_13_variant_a(0);	/* real arg is the same "hidden r0" base as every other wrapper in this file - configures SDATA/SCLK as outputs */
	periph = (uint32_t *)(uintptr_t)board_periph_base_unknown_195c(0);
	*(uint32_t *)((uint8_t *)periph + 0xc)  |= 0x800;
	*(uint32_t *)((uint8_t *)periph + 0x10) |= 0x800;
}

/* issp_bus_idle_all - issp_bus_prepare() plus: deassert all 4 chip-select
 * bits (CLR_DATA89 mask 0xf000) and drive SDATA/SCLK both low. The real
 * "get the bus into a known idle state before touching one target" entry
 * point - called first by both issp_probe_and_init_target and
 * issp_program_and_verify_target. 2 callers, both internal. @0xc00118f0. */
void issp_bus_idle_all(void)	/* FUN_c00118f0 */
{
	void *base;

	issp_bus_prepare();
	base = gpio_bank_get_base();
	gpio_pair4_bit_clear(base, 0xf000);
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 0);
	base = gpio_bank_get_base();
	gpio_pair0_bit20_level(base, 0);
}

/* issp_send_vector - builds and clocks out one Cypress PSoC1 ISSP-style
 * 22-bit "vector": preamble '10010' (5 bits), 6 address bits, 8 data bits,
 * trailer '111' (3 stop bits) = 22 bits total - the documented PSoC1 ISSP
 * WRITE-BYTE vector bit layout (Cypress AN2026A). Only 2 callers, both
 * internal (issp_write_block_and_lock's 64-byte loop and
 * issp_program_blocks' identical 64-byte loop). @0xc00119b8. */
void issp_send_vector(void *handle, uint32_t addr, uint32_t data)	/* FUN_c00119b8 */
{
	uint32_t a = addr << 2;
	uint32_t d = data & 0xff;
	int i;

	issp_bus_prepare();
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 0);
	issp_clock_bit(handle, 0);
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 0);
	for (i = 6; i != 0; i--) {
		issp_clock_bit(handle, (a & 0x80) >> 7);
		a <<= 1;
	}
	for (i = 8; i != 0; i--) {
		issp_clock_bit(handle, d >> 7);
		d = (d & 0x7f) << 1;
	}
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 1);
}

/* ===========================================================================
 * Target select / release - the 4 chip-select lines (GPIO pair 4 / bank
 * 8-9, bits 12-15). CONFIRMED active-low select semantics from the real
 * op order: gpio_pair4_bit_SET (0xf000) always runs first here to
 * deassert (drive high / idle) all 4 lines, THEN gpio_pair4_bit_CLEAR
 * asserts (drives low) only the target(s) actually selected - i.e. these
 * are active-low chip-selects, not active-high enables as a first glance
 * at "which primitive is called last" might suggest.
 * ======================================================================== */

/* issp_select_target - deassert all 4 targets, delay 10ms, then assert
 * (active-low) the target(s) named by `select_code`: 0x78->CS0(bit12),
 * 0x79->CS1(bit13), 0x7b->CS2(bit14), 0x7a->CS3(bit15), 0xff->all four at
 * once (broadcast select - used only for a synchronized reset/XRES-style
 * pulse, never followed by addressed vectors in this file's own callers).
 * Any other code: leaves everything deasserted. Delay 0x28 (40) ticks
 * either way before returning. 4 callers, all internal. @0xc0011a7c. */
void issp_select_target(void *handle, int select_code)	/* FUN_c0011a7c */
{
	void *base;
	uint32_t mask;
	int have_mask = 1;

	issp_bus_prepare();
	base = gpio_bank_get_base();
	gpio_pair4_bit_set(base, 0xf000);	/* deassert (idle high) all 4 */
	issp_target_delay_ms(handle, 10);

	switch (select_code) {
	case 0x78: mask = 0x1000; break;	/* CS0 */
	case 0x79: mask = 0x2000; break;	/* CS1 */
	case 0x7b: mask = 0x4000; break;	/* CS2 */
	case 0x7a: mask = 0x8000; break;	/* CS3 */
	case 0xff: mask = 0xf000; break;	/* all four */
	default:   have_mask = 0;         break;
	}
	if (have_mask) {
		base = gpio_bank_get_base();
		gpio_pair4_bit_clear(base, mask);	/* assert (active-low) */
	}
	issp_target_delay(handle, 0x28);
}

/* issp_release_bus - re-configures SDATA/SCLK via the "variant_b"/tristate-
 * adjacent primitive and applies pinmux word "130_b" (the sibling of
 * issp_bus_prepare's own "130_a"), deasserts all 4 chip-selects, then
 * asserts ALL FOUR simultaneously as one broadcast strobe (135-tick hold),
 * then hands off to omap_l137_addr_gap_misc.c's own unattributed
 * usbdc_gap_config_slot helper - that file's own comment already
 * anticipated this exact call site. Structurally a "reset/release all 4
 * targets together" bus-teardown step, called once at the end of both
 * issp_probe_and_init_target and issp_program_and_verify_target. 2
 * callers, both internal. @0xc0011b44. */
void issp_release_bus(void *handle)	/* FUN_c0011b44 */
{
	void *base;
	void *bus;

	gpio_bank_get_base();			/* return value unused (same "hidden r0" pattern) */
	gpio_pins_bank2_10_13_variant_b(0);
	syscfg0_base_get(0);
	board_desc_set_pinmux_130_b(0);
	base = gpio_bank_get_base();
	gpio_pair4_bit_set(base, 0xf000);	/* deassert all 4 (idle) */
	issp_target_delay(handle, 0x14);
	base = gpio_bank_get_base();
	gpio_pair4_bit_clear(base, 0xf000);	/* assert all 4 together - broadcast strobe */
	issp_target_delay(handle, 0x87);
	bus = panelbus_i2c_base(0, 0);
	usbdc_gap_config_slot(bus, 0);
	issp_target_delay_ms(handle, 10);
}

/* issp_read_bit - pulses SCLK once (high then low, 1-unit delays either
 * side) and samples SDATA's live level afterward into `*out_bit`. The
 * SDATA level-drive call at entry (mirroring issp_clock_bit's own first
 * step) is a real, transcribed part of the decompiled body but is a
 * hardware no-op whenever this is called with SDATA already switched to
 * INPUT direction (both of this function's own callers - issp_wait_ready's
 * second phase and issp_read_byte - run after a gpio_pair0_bit20_dir(...,1)
 * call): on this SoC family writing SET_DATA/CLR_DATA while a pin is
 * configured as input does not drive anything, so the target chip is free
 * to drive the real bit value that gets sampled below. 1 caller (from
 * issp_read_byte's own 8-bit loop) is direct; issp_wait_ready inlines the
 * same shape itself rather than calling this. @0xc0011bd0. */
void issp_read_bit(void *handle, uint32_t *out_bit)	/* FUN_c0011bd0 */
{
	void *base = gpio_bank_get_base();

	gpio_pair0_bit20_level(base, 0);
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 1);	/* SCLK high */
	issp_target_delay(handle, 1);
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 0);	/* SCLK low */
	issp_target_delay(handle, 1);
	base = gpio_bank_get_base();
	*out_bit = (gpio_pair0_bit20_status(base) != 0) ? 1 : 0;
}

/* ===========================================================================
 * issp_wait_ready - poll-for-target-ready plus a trailing settle/ack
 * handshake and final trailer vector. This is the single most fiddly
 * function in this file; transcribed as literally as possible from the
 * decompile (including the one real `goto`) rather than "cleaned up" in a
 * way that risks silently changing its bit-level timing/ordering - the
 * same caution eva_board_main.c's own comment already applies to this
 * cluster's "dense bus-transaction arithmetic".
 *
 * Structure (all 8 callers of this function agree on this shape):
 *   1. Switch SDATA to INPUT (gpio_pair0_bit20_dir(...,1)) and apply the
 *      "clear status bit 11" pinmux tweak - mirrors issp_bus_prepare's own
 *      OUTPUT-direction setup, just for the read side.
 *   2. Poll loop: sample SDATA (busy iff 0, ready iff 1 - `not_ready` is
 *      literally `status ^ 1` in the real code), unconditionally pulse
 *      SCLK once per iteration regardless of the sampled value, and give
 *      up after DAT_c0011db0 = 0xf423f (999999) elapsed delay-units.
 *      Timeout -> return 0 (failure) immediately.
 *   3. A second do/while phase: repeatedly sample SDATA + pulse SCLK; if a
 *      sample doesn't equal exactly 1 (`cVar2 != 1`), jump straight to
 *      step 4; otherwise take a second sample and loop again while that
 *      second sample stays 1. Read literally as: wait for a one-shot ack
 *      pulse to actually go low before proceeding - not independently
 *      confirmed beyond this structural description.
 *   4. Drive SCLK high one last time, clock out the 40-bit trailer vector
 *      (issp_vec_wait_trailer), return 1 (success).
 * @0xc0011c50. */
uint32_t issp_wait_ready(void *handle)	/* FUN_c0011c50 */
{
	void *base;
	uint32_t not_ready, sample, elapsed;

	base = gpio_bank_get_base();
	gpio_pair0_bit20_dir(base, 1);		/* SDATA -> INPUT */
	board_periph_base_unknown_195c(0);	/* return value unused - same "hidden r0, ignored constant" pattern as issp_bus_prepare */
	board_desc_clear_status_bit11(0);

	base = gpio_bank_get_base();
	not_ready = (gpio_pair0_bit20_status(base) == 0);
	elapsed = 0;
	while (not_ready) {
		base = gpio_bank_get_base();
		not_ready = (gpio_pair0_bit20_status(base) == 0);
		base = gpio_bank_get_base();
		gpio_pair0_bit21_level(base, 1);
		issp_target_delay(handle, 1);
		base = gpio_bank_get_base();
		gpio_pair0_bit21_level(base, 0);
		issp_target_delay(handle, 1);
		elapsed += 2;
		if (!not_ready)
			break;
		if (elapsed > 0xf423f)		/* DAT_c0011db0, real value 999999 */
			break;
	}
	if (not_ready)
		return 0;

	for (;;) {
		base = gpio_bank_get_base();
		sample = gpio_pair0_bit20_status(base);
		issp_target_delay(handle, 1);
		if (sample != 1)
			goto settle_done;
		base = gpio_bank_get_base();
		sample = gpio_pair0_bit20_status(base);
		issp_target_delay(handle, 1);
		if (sample != 1)
			break;
	}
	/* Transcribed for fidelity: the real code re-checks `sample == 1` here
	 * too, but the loop can only reach this point via the `break` above,
	 * which already requires `sample != 1` - this is unreachable in
	 * practice, not a live third exit path. */
	if (sample == 1)
		return 0;

settle_done:
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 1);
	issp_clock_bits(handle, issp_vec_wait_trailer, 0x28);
	return 1;
}

/* ===========================================================================
 * Page write, erase/init, and the 2-step device-family ID setup
 * ======================================================================== */

/* issp_write_block_and_lock - writes 64 bytes via 64 individual WRITE-BYTE
 * vectors (addr 0..0x3f), then sends a device-family-dependent
 * "lock"/security vector (0x134 bits) selected by `*(uint8_t *)handle` -
 * the SAME byte issp_probe_and_init_target writes with the low byte of the
 * just-read Silicon ID, i.e. `handle` doubles as a small mutable state
 * buffer, not just an opaque delay context, everywhere in this file. An
 * unrecognized device-type byte aborts with 0 (no lock vector sent, no
 * wait). 1 caller, internal (issp_program_and_verify_target's own
 * "write security row" step). @0xc0011db8. */
uint32_t issp_write_block_and_lock(void *handle, const uint8_t *page)	/* FUN_c0011db8 */
{
	uint8_t *tag = (uint8_t *)handle;
	const uint32_t *lock_vec;
	uint32_t i;

	for (i = 0; i < 0x40; i++)
		issp_send_vector(handle, i, page[i]);

	switch (*tag) {
	case '4': case 0x14: case 0x19:
		lock_vec = issp_vec_init2_a;	/* DAT_c0011e40 */
		break;
	case 0x0b:
		lock_vec = issp_vec_init2_b;	/* DAT_c0011e44 */
		break;
	default:
		return 0;
	}
	issp_clock_bits(handle, lock_vec, 0x134);
	return issp_wait_ready(handle);
}

/* issp_erase_or_init - sends one fixed 0x134-bit vector then polls ready.
 * Real op (erase vs. some other init macro) not independently confirmed
 * beyond "one macro vector + wait", consistent with Cypress ISSP's own
 * ERASE vector shape. 2 callers, both internal. @0xc0011e48. */
uint32_t issp_erase_or_init(void *handle)	/* FUN_c0011e48 */
{
	issp_clock_bits(handle, issp_vec_erase_setup, 0x134);	/* DAT_c0011e74 */
	return issp_wait_ready(handle);
}

/* issp_init2_sequence - the 3-step device presence/ID-setup macro
 * (Cypress ISSP's own "INIT-2" sequence): send a fixed 0x18c-bit vector
 * and poll ready; if ready, send a fixed 0x11e-bit vector and poll ready
 * again; if that also succeeds, send a final fixed 0x344-bit vector
 * (no poll after this last one) and report success. Any poll failure
 * along the way aborts with 0. 2 callers, both internal. @0xc0011e78. */
uint32_t issp_init2_sequence(void *handle)	/* FUN_c0011e78 */
{
	issp_clock_bits(handle, issp_vec_id_setup_1, 0x18c);	/* DAT_c0011ee8 */
	if (!issp_wait_ready(handle))
		return 0;
	issp_clock_bits(handle, issp_vec_id_setup_2a, 0x11e);	/* DAT_c0011eec, length DAT_c0011ef0 */
	if (!issp_wait_ready(handle))
		return 0;
	issp_clock_bits(handle, issp_vec_id_setup_3, 0x344);	/* DAT_c0011ef4 */
	return 1;
}

/* ===========================================================================
 * Per-block program/verify triggers and the byte-readback primitive
 * ======================================================================== */

/* issp_select_verify_block - selects flash block `block` for a subsequent
 * verify (readback) pass: an 0xb-bit "select block" prefix (shared with
 * issp_program_block_trigger below - same underlying table, see the
 * DAT_c0011f74/DAT_c0012024 note above), the block index as 8 bits
 * (top byte of `block << 24`), 3 trailing '1' stop-clocks, then a FIXED
 * (not device-type-dependent) 0x108-bit macro vector, then a ready poll
 * whose result is discarded. 1 caller, internal (issp_verify_blocks'
 * own per-block loop). @0xc0011ef8. */
void issp_select_verify_block(void *handle, uint32_t block)	/* FUN_c0011ef8 */
{
	uint32_t block_byte = block << 0x18;

	issp_clock_bits(handle, issp_vec_select_block_prefix, 0xb);
	issp_clock_bits(handle, &block_byte, 8);
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 1);
	issp_clock_bits(handle, issp_vec_verify_select, 0x108);	/* DAT_c0011f78 */
	issp_wait_ready(handle);	/* return value discarded, matches original */
}

/* issp_program_block_trigger - selects flash block `block` for a program
 * (write) pass: same 0xb-bit "select block" prefix + 8-bit index + 3
 * stop-clocks as issp_select_verify_block above, but then CONDITIONALLY
 * sends a device-type-dependent 0x134-bit trigger vector (only if the
 * device-type byte at `*handle` is recognized - unrecognized types skip
 * straight to the ready poll with no trigger vector sent at all, matching
 * the real code's own behavior). 1 caller, internal (issp_program_blocks'
 * own per-block loop). @0xc0011f7c. */
void issp_program_block_trigger(void *handle, uint32_t block)	/* FUN_c0011f7c */
{
	uint8_t *tag = (uint8_t *)handle;
	uint32_t block_byte = block << 0x18;
	const uint32_t *prog_vec = 0;

	issp_clock_bits(handle, issp_vec_select_block_prefix, 0xb);	/* DAT_c0012024 == DAT_c0011f74 */
	issp_clock_bits(handle, &block_byte, 8);
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 1);

	switch (*tag) {
	case '4': case 0x14: case 0x19:
		prog_vec = issp_vec_program_a;	/* DAT_c0012028 */
		break;
	case 0x0b:
		prog_vec = issp_vec_program_b;	/* DAT_c001202c */
		break;
	default:
		break;
	}
	if (prog_vec != 0)
		issp_clock_bits(handle, prog_vec, 0x134);
	issp_wait_ready(handle);	/* return value discarded, matches original */
}

/* issp_program_blocks - the real multi-block flash WRITE loop: for each
 * 64-byte page of `data` (len/64 pages, any remainder bytes silently
 * dropped - matches the real `param_3 >> 6` truncation), write all 64
 * bytes via issp_send_vector then fire issp_program_block_trigger for
 * that page index. 1 caller, internal
 * (issp_program_and_verify_target). @0xc0012030. */
void issp_program_blocks(void *handle, const uint8_t *data, uint32_t len)	/* FUN_c0012030 */
{
	uint32_t nblocks = len >> 6;
	uint32_t block, i, off = 0;

	for (block = 0; block < nblocks; block++) {
		for (i = 0; i < 0x40; i++, off++)
			issp_send_vector(handle, i, data[off]);
		issp_program_block_trigger(handle, block);
	}
}

/* issp_read_byte - switches SDATA to INPUT, applies the same pinmux tweak
 * issp_wait_ready uses, pulses SCLK once as a framing/dummy edge, then
 * clocks in 8 bits MSB-first via issp_read_bit into `*out_byte`, then
 * pulses SCLK once more as a trailing framing edge. 5 callers, all
 * internal. @0xc0012098. */
void issp_read_byte(void *handle, uint8_t *out_byte)	/* FUN_c0012098 */
{
	void *base;
	uint32_t bit;
	int i;

	base = gpio_bank_get_base();
	gpio_pair0_bit20_dir(base, 1);		/* SDATA -> INPUT */
	board_periph_base_unknown_195c(0);
	board_desc_clear_status_bit11(0);

	base = gpio_bank_get_base();
	gpio_pair0_bit20_level(base, 0);
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 1);
	issp_target_delay(handle, 1);
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 0);
	issp_target_delay(handle, 1);

	*out_byte = 0;
	for (i = 7; i >= 0; i--) {
		issp_read_bit(handle, &bit);
		*out_byte = (uint8_t)((*out_byte << 1) | (bit & 1));
	}

	base = gpio_bank_get_base();
	gpio_pair0_bit20_level(base, 0);
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 1);
	issp_target_delay(handle, 1);
	base = gpio_bank_get_base();
	gpio_pair0_bit21_level(base, 0);
	issp_target_delay(handle, 1);
}

/* ===========================================================================
 * 16-bit readback helpers - Checksum and Silicon ID. Both share the exact
 * same "setup vector, poll ready, read hi byte, bus_prepare + stop-clock,
 * read lo byte, bus_prepare + stop-clock" shape; assembled MSB-first into
 * a native little-endian uint16_t.
 * ======================================================================== */

/* issp_read_checksum - device-type-dependent setup vector (0x11e bits),
 * poll; on success reads 2 bytes (hi then lo) via issp_read_byte with a
 * distinct short setup vector ahead of each (0xb bits, then 0xc bits).
 * Unrecognized device-type byte or a failed poll both return 0 - which is
 * ambiguous with a genuine checksum value of 0, exactly as the real
 * decompiled code is (not resolved further here). 1 caller, internal
 * (issp_program_and_verify_target's own final check). @0xc00121a4. */
uint32_t issp_read_checksum(void *handle)	/* FUN_c00121a4 */
{
	uint8_t *tag = (uint8_t *)handle;
	const uint32_t *cksum_vec;
	uint32_t ready;
	union { uint16_t val; uint8_t b[2]; } result;

	switch (*tag) {
	case '4': case 0x14: case 0x19:
		cksum_vec = issp_vec_checksum_a;	/* DAT_c001227c */
		break;
	case 0x0b:
		cksum_vec = issp_vec_checksum_b;	/* DAT_c0012284 */
		break;
	default:
		return 0;
	}

	issp_clock_bits(handle, cksum_vec, 0x11e);	/* DAT_c0012280 */
	ready = issp_wait_ready(handle) & 0xff;
	if (!ready)
		return 0;

	issp_clock_bits(handle, issp_vec_checksum_read_hi, 0xb);	/* DAT_c0012288 */
	issp_read_byte(handle, &result.b[1]);
	issp_bus_prepare();
	issp_clock_bit(handle, 1);

	issp_clock_bits(handle, issp_vec_checksum_read_lo, 0xc);	/* DAT_c001228c */
	issp_read_byte(handle, &result.b[0]);
	issp_bus_prepare();
	issp_clock_bit(handle, 1);

	return result.val;
}

/* issp_read_silicon_id - same shape as issp_read_checksum but with a
 * single, non-device-type-dependent setup vector (0x14a bits) and two
 * identically-sized (0xb-bit) trailer setup vectors. The low byte of this
 * function's own return value is what issp_probe_and_init_target stores
 * back into `*handle` as the "device-type tag" every other
 * device-type-branching function in this file reads. 2 callers, both
 * internal. @0xc0012290. */
uint32_t issp_read_silicon_id(void *handle)	/* FUN_c0012290 */
{
	uint32_t ready;
	union { uint16_t val; uint8_t b[2]; } result;

	issp_clock_bits(handle, issp_vec_id_read_setup, 0x14a);	/* DAT_c0012338, length DAT_c001233c */
	ready = issp_wait_ready(handle) & 0xff;
	if (!ready)
		return 0;

	issp_clock_bits(handle, issp_vec_id_read_hi, 0xb);	/* DAT_c0012340 */
	issp_read_byte(handle, &result.b[1]);
	issp_bus_prepare();
	issp_clock_bit(handle, 1);

	issp_clock_bits(handle, issp_vec_id_read_lo, 0xb);	/* DAT_c0012344 */
	issp_read_byte(handle, &result.b[0]);
	issp_bus_prepare();
	issp_clock_bit(handle, 1);

	return result.val;
}

/* issp_verify_byte - the READ-BYTE-vector counterpart to issp_send_vector:
 * a DIFFERENT 5-bit preamble ('10110' vs. WRITE's '10010' - the expected
 * Cypress ISSP Read/Write opcode-bit difference), 6 address bits, then
 * receives 8 data bits back via issp_read_byte instead of clocking data
 * bits out, then a single trailing '0' clock (not 3 stop bits like the
 * write vector). 1 caller, internal (issp_verify_blocks' own per-byte
 * loop). @0xc001243c. */
uint8_t issp_verify_byte(void *handle, uint32_t addr)	/* FUN_c001243c */
{
	uint32_t a = addr << 2;
	uint8_t data;
	int i;

	issp_bus_prepare();
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 0);
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 1);
	issp_clock_bit(handle, 0);
	for (i = 6; i != 0; i--) {
		issp_clock_bit(handle, (a & 0x80) >> 7);
		a <<= 1;
	}
	issp_read_byte(handle, &data);
	issp_bus_prepare();
	issp_clock_bit(handle, 0);
	return data;
}

/* issp_verify_blocks - readback-and-compare loop: for each 64-byte page of
 * `expected` (len/64 pages, remainder truncated same as
 * issp_program_blocks), select that page for verify then read all 64
 * bytes back one at a time via issp_verify_byte, bailing out with 0 on the
 * first mismatch. 1 caller, internal
 * (issp_program_and_verify_target). @0xc00124e0. */
uint32_t issp_verify_blocks(void *handle, const uint8_t *expected, uint32_t len)	/* FUN_c00124e0 */
{
	uint32_t nblocks = len >> 6;
	uint32_t block, i, off = 0;

	for (block = 0; block < nblocks; block++) {
		issp_select_verify_block(handle, block);
		for (i = 0; i < 0x40; i++, off++) {
			uint8_t got = issp_verify_byte(handle, i);
			if (expected[off] != got)
				return 0;
		}
	}
	return 1;
}

/* ===========================================================================
 * Top-level orchestration
 * ======================================================================== */

/* issp_probe_and_init_target - select one target (up to 3 retries),
 * running issp_init2_sequence + issp_read_silicon_id to confirm presence
 * and capture the device-type tag into `*handle`; on success, run
 * issp_erase_or_init once. Always deselects with the 0xff broadcast code
 * once per outer attempt (up to 3 outer attempts total, stopping as soon
 * as a target is confirmed present+erased), then releases the bus.
 * Probe-only - does NOT program or verify flash content (see
 * issp_program_and_verify_target below for the full read/write/verify
 * variant sharing this exact same probe shape). 4 callers, all internal
 * (issp_probe_and_init_all's own 4 fixed device-select calls). @0xc0012348. */
void issp_probe_and_init_target(void *handle, int select_code)	/* FUN_c0012348 */
{
	uint32_t found = 0;
	int outer;

	issp_bus_idle_all();

	for (outer = 0; outer < 3; outer++) {
		if (!found) {
			int inner;

			for (inner = 0; inner < 3 && !found; inner++) {
				issp_select_target(handle, select_code);
				found = issp_init2_sequence(handle);
				if (found) {
					uint8_t id_lo = (uint8_t)issp_read_silicon_id(handle);

					found &= 1;
					*(uint8_t *)handle = id_lo;
					if (id_lo == 0)
						found = 0;
				}
			}
		}
		if (found)
			found = issp_erase_or_init(handle);
		issp_select_target(handle, 0xff);
		if (found)
			break;
	}
	issp_release_bus(handle);
}

/* issp_probe_and_init_all - runs issp_probe_and_init_target for all 4
 * physical targets in board order (0x78, 0x79, 0x7b, 0x7a - NOT bit
 * order, matches issp_select_target's own non-sequential CS-bit mapping).
 *
 * OPEN ITEM: this function has ZERO callers anywhere in the full
 * 691-function xrefs_to data - confirmed via the same query tooling this
 * whole project uses for its other "zero static callers" findings (e.g.
 * crypto_at88_self_test in crypto_at88.c). Not reachable from
 * eva_board_final_setup's own init-table walk either (that table has
 * exactly one real entry, already unrelated - see eva_board_main.c's own
 * "SPI/USB cleanup pass" note). Left as a genuinely unresolved reachability
 * gap, not fabricated as dead code or as a real entry point - it may be
 * invoked via an indirect function pointer this static dump doesn't
 * capture (e.g. a menu/debug-command table), or it may be real
 * development-time-only code that never runs in production. @0xc00123fc. */
void issp_probe_and_init_all(void *handle)	/* FUN_c00123fc */
{
	issp_probe_and_init_target(handle, 0x78);
	issp_probe_and_init_target(handle, 0x79);
	issp_probe_and_init_target(handle, 0x7b);
	issp_probe_and_init_target(handle, 0x7a);
}

/* issp_program_and_verify_target - the full erase/program/verify/lock/
 * checksum cycle for ONE target: shares issp_probe_and_init_target's exact
 * probe-and-confirm shape (select, INIT-2, read Silicon ID into the
 * device-type tag, up to 3x3 retries) but on success additionally: erases
 * (issp_erase_or_init), programs all of `image` (issp_program_blocks),
 * reads it back and compares (issp_verify_blocks), writes the 64-byte
 * `lock_row` as a security/lock page (issp_write_block_and_lock), and
 * finally reads back the device checksum (issp_read_checksum) - a
 * checksum of exactly 0 is treated as failure (same ambiguity with a
 * genuine all-zero checksum already flagged in issp_read_checksum's own
 * comment). Combines erase-ok, verify-ok, and lock-write-ok into the
 * final result with a plain AND. Same outer deselect/retry/release
 * structure as issp_probe_and_init_target. 4 callers, all internal
 * (eva_board_probe_summary's own 4 fixed device-select calls). @0xc0012560. */
uint32_t issp_program_and_verify_target(void *handle, int select_code,
					 const uint8_t *lock_row,
					 const uint8_t *image, uint32_t image_len)	/* FUN_c0012560 */
{
	uint32_t found = 0;
	int outer;

	issp_bus_idle_all();

	for (outer = 0; outer < 3; outer++) {
		if (!found) {
			int inner;

			for (inner = 0; inner < 3 && !found; inner++) {
				issp_select_target(handle, select_code);
				found = issp_init2_sequence(handle);
				if (found) {
					uint8_t id_lo = (uint8_t)issp_read_silicon_id(handle);

					found &= 1;
					*(uint8_t *)handle = id_lo;
					if (id_lo == 0)
						found = 0;
				}
			}
		}
		if (found) {
			uint32_t erase_ok = issp_erase_or_init(handle);
			uint32_t verify_ok;

			issp_program_blocks(handle, image, image_len);
			verify_ok = issp_verify_blocks(handle, image, image_len);
			found = issp_write_block_and_lock(handle, lock_row);
			found = ((erase_ok & verify_ok) != 0) & found;
			if (issp_read_checksum(handle) == 0)
				found = 0;
		}
		issp_select_target(handle, 0xff);
		if (found)
			break;
	}
	issp_release_bus(handle);
	return found;
}

/* ===========================================================================
 * eva_board_probe_summary - FUN_c001267c, the ONLY function in this whole
 * file with a caller outside it: eva_board_compat_check (FUN_c00073fc,
 * eva_board_main.c), on its own hardware-compat-check failure path.
 *
 * CORRECTION to eva_board_main.c's own characterization (that file is not
 * edited here - flagged as a cross-file discrepancy per this project's
 * established convention instead): eva_board_main.c's own comment
 * describes this function as "re-probing the same four bank numbers
 * 0x78/0x79/0x7b/0x7a in sequence to decide which [error] message to
 * show" and explicitly declines to transcribe its callee (FUN_c0012560)
 * as merely "dense bus-transaction arithmetic". Having now fully
 * decompiled FUN_c0012560 (issp_program_and_verify_target above) and its
 * entire call tree, that description is NOT what this code does: each of
 * the 4 calls below runs a COMPLETE Cypress PSoC1 ISSP erase + program
 * (4KB) + verify + security-row-write + checksum-read cycle against one
 * physical target, not a lightweight presence probe. Concretely,
 * eva_board_compat_check's own hardware-compat failure path does not just
 * pick between two error strings - it actually attempts to REFLASH all 4
 * front-panel PSoC1 controllers from a baked-in ~4KB firmware image
 * (issp_flash_image) before reporting failure/success.
 *
 * `issp_security_row` and `issp_flash_image` are the SAME base address
 * (DAT_c0012720) used two different ways at each call site: as a plain
 * pointer (the 64-byte lock/security row, passed as `lock_row`) and as
 * `DAT_c0012720 - 0x1000` (the 4096-byte image immediately BEFORE it in
 * the firmware's own data image, passed as `image`) - i.e. the real
 * layout is one contiguous `uint8_t blob[0x1000 + 64]` with the 4KB Flash
 * image first and the 64-byte security row appended immediately after it,
 * not two independent tables. 1 caller (eva_board_compat_check,
 * eva_board_main.c). @0xc001267c. */
int eva_board_probe_summary(void *handle)	/* FUN_c001267c */
{
	if (!issp_program_and_verify_target(handle, 0x78, issp_security_row, issp_flash_image, 0x1000))
		return 0;
	if (!issp_program_and_verify_target(handle, 0x79, issp_security_row, issp_flash_image, 0x1000))
		return 0;
	if (!issp_program_and_verify_target(handle, 0x7b, issp_security_row, issp_flash_image, 0x1000))
		return 0;
	if (!issp_program_and_verify_target(handle, 0x7a, issp_security_row, issp_flash_image, 0x1000))
		return 0;
	return 1;
}
