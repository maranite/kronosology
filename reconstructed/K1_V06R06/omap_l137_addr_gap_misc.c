/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_addr_gap_misc.c - everything else found while sweeping this
 * project's assigned address ranges (0xc0003194-0xc00032f8,
 * 0xc00034fc-0xc000395c, 0xc00047d4-0xc0004d74) that is NOT part of the USB
 * device-controller cluster reconstructed in omap_l137_usbdc_ext.c. These
 * functions happen to sit in the same address neighborhood as the USB core
 * (hence being swept by the same assignment), but the evidence below points
 * each of them at a DIFFERENT real subsystem - grouped here rather than
 * force-fit into the USB file, and NOT claimed to be one coherent
 * compilation unit themselves (they are 4 unrelated small clusters that
 * share nothing but address proximity).
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (2026-07-18 pass),
 * no live Ghidra bridge access this pass.
 */

#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 * Cluster 1 (0xc0003194-0xc0003228) - a second McASP instance's reduced
 * init/reconfigure path.
 *
 * mcasp.c's own "still open" notes already flagged FUN_c0003228 as
 * "structurally a 'reduced reinit' sibling of mcasp_init... deliberately
 * not reconstructed here to avoid asserting an unconfirmed attribution."
 * That attribution is now confirmed, not asserted: this function zeroes
 * the EXACT SAME four fields mcasp_init does (ma+0x44/0x60/0xa0/0x14,
 * per mcasp.c's own decompile) and conditionally calls mcasp_configure_clock
 * / mcasp_configure_pins (FUN_c0002658/FUN_c0002668) - the SAME two
 * functions, at the SAME addresses, INSIDE mcasp.c's own confirmed
 * "../MCU/Component/OmapL137Mcasp.cpp" anchor range. mcasp.c itself is not
 * edited here (per this project's own convention against files claiming a
 * confirmed anchor being edited by an unrelated pass); this is purely
 * additive, cross-referencing mcasp.c's own McASP-domain functions.
 *
 * Sole caller of the top-level function (FUN_c0003228): FUN_c0000aa4
 * (0xc0000aa4, a small bring-up sequence calling FUN_c0005438/FUN_c0001d28/
 * FUN_c00018f0 etc. alongside it - itself not reconstructed, out of every
 * file's confirmed scope so far).
 * =========================================================================== */

/* mcasp2_set_bit25 / mcasp2_set_bit15 - OR/AND a single bit into ma+0x18
 * depending on `enable`. Both called from mcasp2_reduced_init below AND,
 * independently, from FUN_c000ef20/FUN_c000f01c (addresses well outside
 * this project's confirmed ranges so far - not reconstructed, cited only
 * as evidence these two bit-toggle helpers are shared more widely than
 * just this reinit path). @0xc0003194 / @0xc00031b0. */
void mcasp2_set_bit25(void *ma, int enable)	/* FUN_c0003194 */
{
	uint32_t *flags = (uint32_t *)((uint8_t *)ma + 0x18);
	if (enable == 0)
		*flags &= 0xfdffffff;
	else
		*flags |= 0x2000000;
}

void mcasp2_set_bit15(void *ma, int enable)	/* FUN_c00031b0 */
{
	uint32_t *flags = (uint32_t *)((uint8_t *)ma + 0x18);
	if (enable == 0)
		*flags |= 0x8000;
	else
		*flags &= 0xffff7fff;
}

/* mcasp2_reduced_init - zeroes ma+0x44/0x60/0xa0/0x14 (same 4 fields
 * mcasp.c's own mcasp_init sets up on the FIRST McASP instance) and, if
 * `sub_config` is non-NULL, calls mcasp_configure_clock/mcasp_configure_pins
 * on it - the exact shape mcasp.c's own README already documented for
 * mcasp_init's own sub-config path, strong evidence this really is a
 * second/reduced instance of the same init logic rather than a coincidence.
 * @0xc0003228. */
extern void mcasp_configure_clock(void *cfg);	/* FUN_c0002658, mcasp.c */
extern void mcasp_configure_pins(void *cfg);	/* FUN_c0002668, mcasp.c */

void mcasp2_reduced_init(void *ma, void *sub_config)	/* FUN_c0003228 */
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
 * Cluster 2 (0xc00032a8) - unattributed small config-slot selector.
 *
 * Genuinely NOT confirmed to belong to any subsystem this project has
 * anchored so far. Called from FUN_c0000a20 (twice, param_2=0 and 1) and
 * FUN_c0011b44 (once, param_2=0) - both of which are themselves called
 * from eva_board_main.c's own crt0 chain (FUN_c00055b8, the SAME function
 * that calls aintc.c's aintc_init) and from a cluster around 0xc0012xxx,
 * neither reconstructed by any file so far. FUN_c0000a20's own body uses
 * gpio_bank_get_base (FUN_c0001990, i2c_by_gpio.c's own documented shared
 * primitive) heavily, which is circumstantial evidence toward a GPIO- or
 * interrupt-channel-adjacent role, but that is NOT strong enough evidence
 * to name this function with confidence - left generically named.
 * @0xc00032a8. */
void usbdc_gap_config_slot(void *obj, int mode)	/* FUN_c00032a8 - name is a location label,
							   NOT a confirmed subsystem attribution */
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
 * Cluster 3 (0xc00034fc-0xc000366c) - a UART-shaped register block writer
 * and status-poll reader. Field layout (a 1-bit-flags word, a 3-value mode
 * select, a baud/divisor-shaped word chosen from a 3-way table, and a
 * poll-with-30-retry status read gated through hw_timer_busy_wait
 * (FUN_c0001aa0, i2c_by_gpio.c's own documented 16-caller shared timer
 * primitive) strongly suggests a UART or similar full-duplex serial
 * peripheral config/status pair, distinct from every other communication
 * channel this project has already anchored (I2C-by-GPIO, the hardware
 * I2C0/I2C1 panelbus_dispatch.c documents, SPI, and USB) - NOT confirmed
 * against a specific `__FILE__` string or TI register map this pass.
 * Callers (FUN_c00136c0, FUN_c0013d2c, FUN_c0011534) are all addresses
 * outside every file's confirmed range so far.
 * =========================================================================== */

/* uart_gap_configure - param_2 selects one of 3 fixed "mode" word values
 * (0xe00 literal / DAT_c0003594=0xe01 / DAT_c0003598=0xf01 for param_2
 * 0/1/2) written into a length/mode-shaped field, plus two more fixed
 * words (DAT_c000359c=0x4a10, DAT_c00035a0=0x14a10 - plausibly a baud-rate
 * divisor pair) fanned out into 3 adjacent slots. @0xc00034fc. */
extern uint32_t hw_timer_busy_wait(uint32_t channel);	/* FUN_c0001aa0, i2c_by_gpio.c */
extern void *uart_gap_timer_channel;			/* DAT_c0003590/DAT_c00036e0, shared with cluster below */

void uart_gap_configure(uint32_t *cfg, int mode)	/* FUN_c00034fc */
{
	uint32_t sel;

	cfg[0] = 0;
	hw_timer_busy_wait((uint32_t)(uintptr_t)uart_gap_timer_channel);	/* real call: FUN_c0001aa0(DAT_c0003590,5) -
			second argument (5) not forwarded by hw_timer_busy_wait's own
			reconstructed 1-arg signature in i2c_by_gpio.c - left out here too,
			flagged rather than silently dropped */
	cfg[0] |= 1;
	cfg[1] = 3;
	if (mode == 0)
		sel = 0xe00;
	else if (mode == 1)
		sel = 0xe01;	/* DAT_c0003594 */
	else if (mode == 2)
		sel = 0xf01;	/* DAT_c0003598 */
	else
		sel = 0xe00;	/* real code: falls through with the mode==0 value for
				 * any mode not 0/1/2 - no explicit default branch */
	cfg[5] = sel;
	cfg[0xf] = 0;
	cfg[0x14] = 0x14a10;	/* DAT_c00035a0 */
	cfg[0x15] = 0x4a10;	/* DAT_c000359c */
	cfg[0x16] = 0x4a10;
	cfg[0x17] = 0x4a10;
	cfg[1] |= 0x1000000;
}

/* uart_gap_read_status - polls a status halfword at cfg+0x100 up to 30
 * times (0x1d, via hw_timer_busy_wait between attempts) waiting for the
 * sign bit to clear; on success writes the low 16 bits out through
 * `*out` and returns true unless bit 0x40000000 is also set (an error/
 * framing-fault bit, returns false in that case even though data was
 * available). @0xc000366c. */
bool uart_gap_read_status(void *cfg, int16_t *out)	/* FUN_c000366c */
{
	int32_t *status = (int32_t *)((uint8_t *)cfg + 0x40);
	int tries = 0;

	while (*status < 0) {
		if (tries > 0x1d)
			break;
		hw_timer_busy_wait((uint32_t)(uintptr_t)uart_gap_timer_channel);
		tries++;
	}
	if ((*(uint32_t *)status & 0x40000000) != 0)
		return false;
	*out = (int16_t)*status;
	return true;
}

/* ===========================================================================
 * Cluster 4 (0xc0003734/0xc0003784) - two tiny, unrelated bit-extraction
 * helpers with single callers outside every confirmed range so far. Too
 * small and too context-free to attribute to any subsystem - reconstructed
 * literally, left generically named.
 * =========================================================================== */

/* @0xc0003734, sole caller FUN_... at 0xc0000718 (unresolved). */
uint32_t gap_extract_bits_1_3(void *obj)	/* FUN_c0003734 */
{
	return (*(uint32_t *)((uint8_t *)obj + 8) >> 1) & 7;
}

/* @0xc0003784, sole caller FUN_c00103e4 (unresolved, outside every
 * confirmed range so far). */
uint32_t gap_extract_low_byte(uint32_t *obj)	/* FUN_c0003784 */
{
	return *obj & 0xff;
}

/* ===========================================================================
 * Cluster 5 (0xc00037b0-0xc000383c) - the default 256-entry RGB565 palette
 * loader. clcdc.c's own "clcdc_dispatch_set_palette_hook" section already
 * cites FUN_c00037b0 as "NOT part of clcdc.cpp... a 5-argument RGB->RGB565
 * palette-entry-set primitive" without reconstructing it (out of its own
 * scope). This is that reconstruction, plus its two real callers.
 * =========================================================================== */

/* clcdc_palette_lut - the 256-entry RGB565 lookup table itself
 * (DAT_c00037dc, used as a bare base address rather than a dereferenced
 * pointer - i.e. a real fixed global array, not a pointer-to-array). */
extern uint16_t clcdc_palette_lut[256];	/* DAT_c00037dc */

/* clcdc_palette_set_entry - lut[index] = RGB565(r,g,b), bounds-checked
 * (index < 0x100). Two confirmed callers: clcdc.c's own
 * clcdc_dispatch_set_palette_hook (opcode 0xc5's handler, per that file's
 * own note - single-argument call there, likely an unpacked-struct
 * artifact per clcdc.c's own caution) and clcdc_palette_load_defaults
 * below. @0xc00037b0. */
void clcdc_palette_set_entry(void *lut_unused, int index, int r, int g, int b)	/* FUN_c00037b0 */
{
	(void)lut_unused;	/* real first parameter - not independently confirmed
				 * to be forwarded to clcdc_palette_lut vs. always using
				 * the fixed global; modeled here as unused since the
				 * real store always targets DAT_c00037dc directly */
	if (index < 0x100)
		clcdc_palette_lut[index] = (uint16_t)(((g >> 2) << 5) | ((b >> 3) << 11) | (r >> 3));
}

/* clcdc_palette_load_defaults - fills all 256 palette entries from a
 * packed 3-bytes-per-entry default RGB table (DAT_c0003838). Sole caller:
 * clcdc_display_object_init below. @0xc00037e0. */
extern const uint8_t clcdc_default_rgb_table[256 * 3];	/* DAT_c0003838 */

void clcdc_palette_load_defaults(void *lut_unused)	/* FUN_c00037e0 */
{
	for (int i = 0; i < 0x100; i++) {
		const uint8_t *e = &clcdc_default_rgb_table[i * 3];
		clcdc_palette_set_entry(lut_unused, i, e[0], e[1], e[2]);
	}
}

/* clcdc_display_object_init - loads the default palette, then initializes
 * a 256-entry glyph/attribute table (obj+8 = DAT_c0003930 = 0x3ff - a mode
 * word) by remapping each byte of a caller-supplied `attr_map` through a
 * second lookup table pair (DAT_c0003938/DAT_c000393c), zeroes a 16-entry
 * scratch array, and sets up a handful of scattered pointer/flag fields.
 * Sole caller: FUN_c0014f84 (outside every confirmed range so far).
 *
 * FLAGGED, not resolved: DAT_c0003934 (the remap loop's own upper bound)
 * resolves in this pass's static dump to the raw value 0x752ff - if taken
 * literally this loops ~480000 times over what is almost certainly a
 * <=256-byte `attr_map`, which cannot be correct firmware behavior. Left
 * as the tool-resolved value rather than silently "corrected" to a guessed
 * 0xff, since there's a real possibility this is a genuine Ghidra data-
 * type/size misinference on a constant this pass had no live-bridge access
 * to re-check (see this project's own "false negative" precedent for
 * similarly surprising resolved constants). @0xc000383c. */
extern void *clcdc_display_timer_channel;	/* DAT_c0003954, == uart_gap_timer_channel's own value
						 * (0xc00e0068) - same shared timer primitive input,
						 * not confirmed to be the same GLOBAL VARIABLE */
extern void clcdc_attr_finish_setup(void *timer_channel);	/* FUN_c000196c, out of range */
extern void clcdc_attr_finish_setup2(void);			/* FUN_c0002208, out of range */

void clcdc_display_object_init(void *obj, const uint8_t *attr_map)	/* FUN_c000383c */
{
	uint8_t *o = (uint8_t *)obj;
	extern uint16_t *clcdc_attr_remap_dst;		/* DAT_c0003938, dereferenced */
	extern uint16_t *clcdc_attr_remap_src;		/* DAT_c000393c, dereferenced */
	extern uint16_t clcdc_attr_scratch[16];	/* DAT_c0003940 */

	*(uint32_t *)(o + 0x28) &= ~1u;
	*(uint32_t *)(o + 8) = 0x3ff;	/* DAT_c0003930 */
	clcdc_palette_load_defaults(0);

	/* remap loop - bound genuinely suspicious, see note above */
	for (uint32_t i = 0; i <= 0x752ff; i++) {
		clcdc_attr_remap_dst[i] = clcdc_attr_remap_src[attr_map[i]];
	}

	for (int i = 0; i < 0x10; i++)
		clcdc_attr_scratch[i] = 0;

	*(uint32_t *)(o + 4) = 0x401;		/* DAT_c0003944 */
	*(uint32_t *)(o + 0x2c) = 0x2dd19f10;	/* DAT_c000394c, resolved raw value */
	*(uint32_t *)(o + 0x30) = 0x160b4e57;	/* DAT_c0003950 */
	*(uint32_t *)(o + 0x34) = 0x2300b00;	/* DAT_c0003958 */
	*(uint16_t **)(o + 0x44) = clcdc_attr_scratch;
	*(int *)(o + 0x48) = (int)(uintptr_t)clcdc_attr_scratch + 0xea61e;	/* DAT_c0003948 */
	*(uint32_t *)(o + 0x40) = 0x20;
	*(uint32_t *)(o + 0x28) |= 0x80;
	clcdc_attr_scratch[0] = 0x4000;

	clcdc_attr_finish_setup(clcdc_display_timer_channel);
	clcdc_attr_finish_setup2();
	*(uint32_t *)(o + 0x28) |= 1;
}

/* ===========================================================================
 * Cluster 6 (0xc0004cdc) - a large struct-zero-init function, uncertain
 * subsystem. Sole caller: FUN_c00026a0 (0xc00026a0), which IS inside
 * mcasp.c's own confirmed address range and IS already documented there
 * (mcasp.c's own note: "wait. @0xc00026a0. Sole caller: FUN_c000ecc4 [an
 * audio-pipeline-object..."). This function is the OTHER direction - a
 * callee FUN_c00026a0 reaches out to, not previously cross-referenced by
 * mcasp.c. Field offsets (0x200-0x380, mostly 4/8/12-entry zero loops and
 * a handful of fixed sentinel values: 0x284=0x10, 0x308=0xffffffff,
 * 0x31c=DAT_c0004d70=0x10003, 0x340=3) do NOT match the USB `dev` struct's
 * own offset conventions this project has established elsewhere (which
 * cluster around 0x400-0x520 and 0x1800-0x5000) - almost certainly NOT
 * USB-related despite sitting inside this project's USB-adjacent address
 * sweep range, most likely a generic audio-pipeline object mcasp.c's own
 * FUN_c00026a0 owns and zeroes as part of its own init. Not moved into
 * mcasp.c itself (out of this pass's own scope, that file not edited
 * here) - flagged for that file's own next pass. @0xc0004cdc. */
void mcasp_gap_zero_pipeline_object(void *obj)	/* FUN_c0004cdc */
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
	*(uint32_t *)(o + 0x31c) = 0x10003;	/* DAT_c0004d70 */
	*(uint32_t *)(o + 800) = 0;		/* 0x320 */
	*(uint32_t *)(o + 0x340) = 3;
	*(uint32_t *)(o + 0x348) = 0;
	*(uint32_t *)(o + 0x350) = 0;
	*(uint32_t *)(o + 0x358) = 0;
	for (int i = 0; i < 4; i++)
		*(uint32_t *)(o + 0x380 + i * 4) = 0;	/* real code advances `param_1` itself by 4 each
							 * iteration and always stores to the SAME +0x380
							 * offset relative to the CURRENT param_1 - i.e. it
							 * actually zeroes o+0x380, o+0x384, o+0x388, o+0x38c,
							 * equivalent to what's written here */
}

/* -------------------------------------------------------------------------
 * Still genuinely open:
 *  - Cluster 2 (usbdc_gap_config_slot / FUN_c00032a8) and its two callers
 *    (FUN_c0000a20, FUN_c0011b44) - real subsystem not identified.
 *  - Cluster 3's real peripheral identity (UART-shaped register layout,
 *    not confirmed against any `__FILE__` anchor or TI register map).
 *  - Cluster 4's two tiny helpers - no subsystem context available from
 *    their single, unresolved call sites.
 *  - Cluster 5's DAT_c0003934 loop-bound anomaly (see note at its use
 *    site) - genuinely unresolved, not silently corrected.
 *  - Cluster 6's real ownership - strong circumstantial case for being
 *    mcasp.c's own pipeline-object code, not independently confirmed by a
 *    `__FILE__` anchor.
 * ------------------------------------------------------------------------- */
