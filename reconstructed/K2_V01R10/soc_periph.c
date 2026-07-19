/* SPDX-License-Identifier: GPL-2.0 */
/*
 * soc_periph.c - KRONOS2S_V01R10.VSB (Kronos 2) port of the K1 reconstruction
 * at K1_V06R06/soc_periph.c. Same subsystem: the panel board's SoC
 * peripheral base-address table plus the small hardware bring-up primitives
 * (GPIO-bank base accessor, free-running-timer busy-wait engine, Timer64P
 * descriptor constructor, a PSC "enable one LPSC module" sequence, and a
 * 3-word pinmux/config write) that sit in the address gap between aintc.c's
 * and mcasp.c's own K2 coverage.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB,
 * 2026-07-18 (query_dump_k2.py, not the live Ghidra MCP bridge - see
 * cobjectmgr.c's own header for why). This is a migration pass from the
 * already-done K1 reconstruction - EVERY function below was independently
 * re-matched against K2's own decompile by code shape and, where present, by
 * numerically re-resolving its own DAT_ constants (not assumed to carry over
 * from K1's citations).
 *
 * ANCHOR: NONE, same as K1 - no "../<Name>.cpp" string sits near this range
 * in K2 either. Attribution rests on the same address-constant evidence K1
 * used (each base value matched against the public TI OMAP-L1x/DA850 TRM),
 * strengthened here by direct cross-file confirmation: two of this file's
 * own functions (gpio_bank_get_base, hw_timer_busy_wait) are ALREADY cited
 * as `extern` by K2's own i2c_by_gpio.c (with real K2 addresses and caller
 * counts already independently derived there) - this file is where they are
 * actually DEFINED, closing that file's own forward reference.
 *
 * ADDRESS MAP (K1 -> K2, this file's own functions only):
 *   timer64p0_base_get           FUN_c00018f0 -> FUN_c0001670
 *   syscfg0_base_get             FUN_c0001948 -> FUN_c00016c8
 *   psc1_base_get                FUN_c000196c -> FUN_c00016ec
 *   gpio_bank_get_base           FUN_c0001990 -> FUN_c0001710 (i2c_by_gpio.c's own extern)
 *   edma_cc_base_select          FUN_c000199c -> FUN_c000171c
 *   edma_tc_base_select          FUN_c00019b0 -> FUN_c0001730
 *   mcasp0_base_get              FUN_c00019e0 -> FUN_c0001760
 *   mcasp0_fifo_base_get         FUN_c00019e8 -> FUN_c0001768
 *   (i2c0_i2c1_base_select        NONE IN K1  -> FUN_c0001780, NEW, see below)
 *   spi_base_select (SPI0 only)  FUN_c0001a1c -> FUN_c00017d0, see "SPI SURVIVES" below
 *   uart_base (UART1 only)       FUN_c0001a38 -> FUN_c000180c, see "UART SHRINKS" below
 *   lcdc_base_get                FUN_c0001a68 -> FUN_c0001818
 *   ecap1_base_get                FUN_c0001a74 -> FUN_c0001824
 *   (ecap2_base_get               NONE IN K1  -> FUN_c0001830, NEW, see below)
 *   usb0_otg_base_get            FUN_c0001a80 -> FUN_c000183c
 *   aemif_cs3_base_get           FUN_c0001a98 -> FUN_c0001854
 *   hw_timer_busy_wait           FUN_c0001aa0 -> FUN_c000185c (i2c_by_gpio.c's own extern)
 *   board_desc_type_clear_enabled FUN_c0001d28 -> FUN_c0001ae8
 *   board_desc_init_type5        FUN_c0001d38 -> FUN_c0001af8
 *   timer64p0_enable_ch15    (K1: cited only, never defined) -> FUN_c0000040, NOW DEFINED, see below
 *   board_desc_set_pinmux_*(x3)  FUN_c0001e38/48/58 -> FUN_c0001b30, MERGED, see below
 *   psc_module_enable            FUN_c0001e98 -> FUN_c0001c50 (zero callers, confirmed identical)
 *
 * REAL DIFFERENCES FROM K1 (not transcription artifacts - each independently
 * re-derived from K2's own dump):
 *
 *  1. TWO NEW TABLE ENTRIES not present anywhere in K1's own soc_periph.c:
 *     - i2c0_i2c1_base_select (FUN_c0001780, @0xc0001780): 2-way selector,
 *       param!=0 -> I2C1 (0x01E28000), else -> I2C0 (0x01C22000). K1's own
 *       file only ever cited these two constants as "confirmed by
 *       panelbus_dispatch.c's own register-offset evidence" without having
 *       a table ACCESSOR for them - K2 apparently DOES route them through
 *       this table (a real caller, FUN_c0000800, is traced below).
 *     - ecap2_base_get (FUN_c0001830, @0xc0001830): 0x01F08000, immediately
 *       adjacent to ecap1_base_get's own 0x01F07000 - a genuine DA850 eCAP2
 *       base, absent from K1's table entirely.
 *
 *  2. "SPI SURVIVES" - a real, notable finding given K2_V01R10/README.md's
 *     own architecture note that cpsoc.cpp/cad.cpp (K1's only two SPI-bus
 *     consumers) are GONE in K2: this file's SPI0 base accessor
 *     (FUN_c00017d0, returns 0x01C41000) is NOT dead code. Its two real K2
 *     callers are FUN_c0004f70 and, concretely, `panel_scan_updater_run()`
 *     (FUN_c0006ee8, @0xc0006f0c - see panel_scan_updater.c) - the PSoC-
 *     successor panel-scan-system firmware-update sequencer. SPI wasn't
 *     dropped in K2; its *consumers* changed from cpsoc/cad to the new
 *     panel-scan-update mechanism. K1's SPI1 selector arm (0x01F0E000) has
 *     NO K2 counterpart found this pass - only a single-value SPI0 getter
 *     exists here, not a 2-way selector like K1's spi_base_select.
 *
 *  3. "UART SHRINKS" - K1's uart_base_select was a 3-way (UART0/1/2)
 *     selector taking an index. K2's equivalent at this position
 *     (FUN_c000180c, @0xc000180c) is a single-value, NO-PARAMETER accessor
 *     that always returns UART1 (0x01D0C000) - 9 real callers. No UART0/
 *     UART2 K2 accessor was found anywhere near this cluster. Whether K2
 *     dropped the multi-UART selector shape entirely or moved it elsewhere
 *     is NOT resolved this pass - transcribed faithfully as the single-value
 *     leaf K2's own decompile actually shows, not forced into K1's 3-way
 *     shape.
 *
 *  4. PINMUX WRITES MERGED INTO ONE FUNCTION. K1 had three separate
 *     one-line leaves (board_desc_set_pinmux_154/130_a/130_b, each writing
 *     ONE fixed word to ONE offset). K2's equivalent (FUN_c0001b30,
 *     @0xc0001b30) is a SINGLE function writing THREE fixed words to THREE
 *     consecutive offsets (+0x110/+0x114/+0x118) in one call - a real,
 *     confirmed structural simplification, not three leaves collapsed by
 *     this pass's own choice. Offsets differ from K1's own +0x130/+0x154
 *     too (K1's own +0x130/+0x154 fields are NOT written anywhere in this
 *     K2 cluster - genuinely absent, not just merged).
 *
 *  5. board_desc_init_type4 (K1's caller-supplied-length sibling to
 *     board_desc_init_type5, K1 @0xc0001cec) has NO confirmed K2
 *     counterpart in this address range - only the type5 form
 *     (board_desc_init_type5 below) was found, wired to the SAME single
 *     Timer64P0 lazy-init singleton (FUN_c0000040) K1 also used for its own
 *     type5 call. K1's OTHER singleton (FUN_c000019c, Timer64P1 + type4) has
 *     no confirmed K2 sibling either - consistent with, but not proof of,
 *     K2 dropping the second timer instance's own boot-time bring-up
 *     (aintc.c's own K2 header already documents a real, similar caller-
 *     count reduction for aintc_base, attributed to the same architectural
 *     shrinkage). board_desc_type_set_enabled (K1's OR-bit-0 sibling to
 *     board_desc_type_clear_enabled) was likewise not found near this
 *     cluster - only the clear-bit variant (FUN_c0001ae8) is confirmed.
 *
 *  6. TWO psc_module_enable-SHAPED FUNCTIONS, not one. K1 had exactly one
 *     (zero callers, @0xc0001e98) PLUS a separate, address-DISTANT sibling
 *     it left `extern` as `gpio_psc_enable` (K1 @0xc0001fd0, "outside this
 *     file's range", defined nowhere in K1's own tree). K2 has BOTH, at two
 *     genuinely different addresses with IDENTICAL bodies:
 *       - FUN_c0001c50 (@0xc0001c50) - zero callers, this file's own
 *         psc_module_enable, matching K1's zero-caller shape exactly.
 *       - FUN_c0001d88 (@0xc0001d88) - 2 callers (both from
 *         omap_psc_enable_module_0x10, see omap_gpio.c), matching K1's
 *         `gpio_psc_enable` role. THIS one is now, for the first time in
 *         this project, actually DEFINED (see omap_gpio.c) rather than left
 *         as an unresolved extern, since K2's own dump happens to cover it.
 *
 *  Every OTHER function in this list is CONFIRMED structurally identical to
 *  K1 - same branch shape, same constant roles, just at K2's own addresses
 *  and (where independently checked) K2's own, sometimes numerically
 *  different, DAT_ literal values.
 *
 *  7. 2026-07-19 LIVE QUERY RESOLVED (dedicated single-session live Ghidra
 *     MCP pass on soc_irq_gate.c/wire_dispatch.c/omap_gpio.c, "2-agent cap,
 *     no further fan-out" authorization, see CLAUDE.md): FUN_c0000040 (the
 *     Timer64P0 ch-0x15 AINTC-enable lazy-init singleton) is now DEFINED
 *     below as timer64p0_enable_ch15 - soc_irq_gate.c's own file has cited
 *     this exact function, unfilled, as "belongs to soc_periph.c's own
 *     territory" since its very first 2026-07-19 migration pass; this closes
 *     that citation for the first time. See the function's own comment below
 *     for the real cross-file finding this resolved (soc_irq_gate.c's own
 *     table+0x00 slot identity) and the real-argument correction to board_
 *     desc_init_type5's own citation.
 * ---------------------------------------------------------------------------
 */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 */

/* ------------------------------------------------------------------------- *
 *  timer64p0_base_get - K2 @0xc0001670 (K1 @0xc00018f0). Single-value
 *  accessor, param dead. Return value 0x01C20000 - identical to K1.
 * ------------------------------------------------------------------------- */
uint32_t timer64p0_base_get(void *chip)	/* FUN_c0001670 */
{
	(void)chip;
	extern uint32_t timer64p0_base_const;	/* DAT_c0001678, 0x01C20000 */
	return timer64p0_base_const;
}

/* syscfg0_base_get - K2 @0xc00016c8 (K1 @0xc0001948). 0x01C14000, identical
 * to K1. */
uint32_t syscfg0_base_get(void *chip)	/* FUN_c00016c8 */
{
	(void)chip;
	extern uint32_t syscfg0_base_const;	/* DAT_c00016d0, 0x01C14000 */
	return syscfg0_base_const;
}

/* psc1_base_get - K2 @0xc00016ec (K1 @0xc000196c). 0x01E27000, identical to
 * K1. 3 callers this build (not individually traced, same as K1). */
uint32_t psc1_base_get(void *chip)	/* FUN_c00016ec */
{
	(void)chip;
	extern uint32_t psc1_base_const;	/* DAT_c00016f4, 0x01E27000 */
	return psc1_base_const;
}

/* ------------------------------------------------------------------------- *
 *  gpio_bank_get_base - RESOLVES i2c_by_gpio.c's own K2 `extern` declaration
 *  (FUN_c0001710, same address, 30 callers firmware-wide per that file's own
 *  citation). Return value 0x01E26000 - identical to K1, confirming the same
 *  physical GPIO bank hardware. @0xc0001710 (K1: @0xc0001990).
 * ------------------------------------------------------------------------- */
uint32_t gpio_bank_get_base(void)	/* FUN_c0001710, 30 callers firmware-wide */
{
	extern uint32_t gpio_bank_base_const;	/* DAT_c0001718, 0x01E26000 */
	return gpio_bank_base_const;
}

/* ------------------------------------------------------------------------- *
 *  edma_cc_base_select - EDMA3 Channel Controller selector. idx==0 -> CC0
 *  (0x01C00000, hardcoded immediate, no DAT_ symbol, same as K1); else -> CC1
 *  (0x01E30000). Identical shape and values to K1. @0xc000171c
 *  (K1: @0xc000199c).
 * ------------------------------------------------------------------------- */
uint32_t edma_cc_base_select(void *chip, int idx)	/* FUN_c000171c */
{
	(void)chip;
	extern uint32_t edma_cc1_base_const;	/* DAT_c000172c, 0x01E30000 */
	uint32_t v = edma_cc1_base_const;

	if (idx == 0)
		v = 0x01C00000u;
	return v;
}

/* ------------------------------------------------------------------------- *
 *  edma_tc_base_select - EDMA3 Transfer Controller selector, 3-way:
 *  idx==0 -> TC0 (0x01C08000), idx==1 -> TC1 (0x01C08400), else -> TC2
 *  (0x01E38000, default). Identical shape and values to K1. @0xc0001730
 *  (K1: @0xc00019b0).
 * ------------------------------------------------------------------------- */
uint32_t edma_tc_base_select(void *chip, int idx)	/* FUN_c0001730 */
{
	(void)chip;
	extern uint32_t edma_tc0_base_const;	/* DAT_c0001754, 0x01C08000 */
	extern uint32_t edma_tc2_base_const;	/* DAT_c0001758, 0x01E38000 (default) */
	extern uint32_t edma_tc1_base_const;	/* DAT_c000175c, 0x01C08400 */

	if (idx == 0)
		return edma_tc0_base_const;
	if (idx == 1)
		return edma_tc1_base_const;
	return edma_tc2_base_const;
}

/* mcasp0_base_get - K2 @0xc0001760 (K1 @0xc00019e0). Hardcoded immediate
 * 0x01D00000, no DAT_ symbol - identical to K1. Confirmed caller: FUN_c0000864
 * (see this file's own board-bring-up cross-references above). */
uint32_t mcasp0_base_get(void *chip)	/* FUN_c0001760 */
{
	(void)chip;
	return 0x01D00000u;
}

/* mcasp0_fifo_base_get - K2 @0xc0001768 (K1 @0xc00019e8). 0x01D01000,
 * identical to K1. */
uint32_t mcasp0_fifo_base_get(void *chip)	/* FUN_c0001768 */
{
	(void)chip;
	extern uint32_t mcasp0_fifo_base_const;	/* DAT_c0001770, 0x01D01000 */
	return mcasp0_fifo_base_const;
}

/* ------------------------------------------------------------------------- *
 *  i2c0_i2c1_base_select - NEW, no K1 counterpart (see file header point 1).
 *  2-way: idx!=0 -> I2C1 (0x01E28000), idx==0 (default) -> I2C0
 *  (0x01C22000). Both values already independently confirmed by
 *  panelbus_dispatch.c's own register-offset evidence in K1's tree; this K2
 *  table entry is a genuinely new accessor for the same two constants.
 *  Confirmed real caller: FUN_c0000800 (board bring-up, calls with idx=1).
 *  @0xc0001780.
 * ------------------------------------------------------------------------- */
uint32_t i2c0_i2c1_base_select(void *chip, int idx)	/* FUN_c0001780 */
{
	(void)chip;
	extern uint32_t i2c0_base_const;	/* DAT_c0001798, 0x01C22000 (default) */
	extern uint32_t i2c1_base_const;	/* DAT_c0001794, 0x01E28000 */
	uint32_t v = i2c0_base_const;

	if (idx != 0)
		v = i2c1_base_const;
	return v;
}

/* ------------------------------------------------------------------------- *
 *  spi0_base_get - "SPI SURVIVES", see file header point 2. Single-value
 *  accessor (NOT a 2-way selector like K1's spi_base_select - no SPI1 arm
 *  found in K2). Return value 0x01C41000. CONFIRMED real callers:
 *  FUN_c0004f70 and panel_scan_updater_run (FUN_c0006ee8/panel_scan_updater.c)
 *  - the panel-scan-system firmware-update sequencer, NOT cpsoc/cad (both
 *  gone in K2). @0xc00017d0 (K1's spi_base_select: @0xc0001a1c).
 * ------------------------------------------------------------------------- */
uint32_t spi0_base_get(void *chip)	/* FUN_c00017d0 */
{
	(void)chip;
	extern uint32_t spi0_base_const;	/* DAT_c00017d8, 0x01C41000 */
	return spi0_base_const;
}

/* ------------------------------------------------------------------------- *
 *  uart1_base_get - "UART SHRINKS", see file header point 3. Single-value,
 *  no-parameter accessor, always UART1 (0x01D0C000) - K1's own 3-way
 *  uart_base_select has no confirmed K2 counterpart; whether UART0/UART2
 *  accessors exist elsewhere in K2 is NOT resolved this pass. 9 real
 *  callers. @0xc000180c (K1's uart_base_select: @0xc0001a38).
 * ------------------------------------------------------------------------- */
uint32_t uart1_base_get(void)	/* FUN_c000180c */
{
	extern uint32_t uart1_base_const;	/* DAT_c0001814, 0x01D0C000 */
	return uart1_base_const;
}

/* lcdc_base_get - K2 @0xc0001818 (K1 @0xc0001a68). 0x01E13000, identical to
 * K1 - still a strong cross-file lead for clcdc.c, not acted on here. */
uint32_t lcdc_base_get(void *chip)	/* FUN_c0001818 */
{
	(void)chip;
	extern uint32_t lcdc_base_const;	/* DAT_c0001820, 0x01E13000 */
	return lcdc_base_const;
}

/* ecap1_base_get - K2 @0xc0001824 (K1 @0xc0001a74). 0x01F07000, identical to
 * K1 - still single-value/neighborhood-only evidence, not a stronger match. */
uint32_t ecap1_base_get(void *chip)	/* FUN_c0001824 */
{
	(void)chip;
	extern uint32_t ecap1_base_const;	/* DAT_c000182c, 0x01F07000 */
	return ecap1_base_const;
}

/* ------------------------------------------------------------------------- *
 *  ecap2_base_get - NEW, no K1 counterpart (see file header point 1).
 *  0x01F08000, immediately adjacent to ecap1_base_get's own 0x01F07000 -
 *  same single-value/TRM-neighborhood-only evidence strength. @0xc0001830.
 * ------------------------------------------------------------------------- */
uint32_t ecap2_base_get(void *chip)	/* FUN_c0001830 */
{
	(void)chip;
	extern uint32_t ecap2_base_const;	/* DAT_c0001838, 0x01F08000 */
	return ecap2_base_const;
}

/* usb0_otg_base_get - K2 @0xc000183c (K1 @0xc0001a80). Hardcoded immediate
 * 0x01E00000, identical to K1. 8 confirmed callers, matches
 * omap_l137_usbdc.c's own subsystem (not cross-checked against that file's
 * own citations this pass). */
uint32_t usb0_otg_base_get(void *chip)	/* FUN_c000183c */
{
	(void)chip;
	return 0x01E00000u;
}

/* aemif_cs3_base_get - K2 @0xc0001854 (K1 @0xc0001a98). Hardcoded immediate
 * 0x62000000, identical to K1. 7 confirmed callers. */
uint32_t aemif_cs3_base_get(void *chip)	/* FUN_c0001854 */
{
	(void)chip;
	return 0x62000000u;
}

/* ------------------------------------------------------------------------- *
 *  hw_timer_busy_wait - RESOLVES i2c_by_gpio.c's own K2 `extern` declaration
 *  (FUN_c000185c, same address, 17 callers firmware-wide, wraparound period
 *  DAT_c00018e4 = 0x249f1 = 150001 - IDENTICAL to K1's own derived value).
 *  omap_l108.c's own K2 header separately confirmed this same address is
 *  NOT cad_delay_ticks (that function has no confirmed K2 mapping at all) -
 *  consistent with this file's own, narrower "generic scaled-tick busy-wait"
 *  characterization, not a calibration-specific helper.
 *
 *  Structurally identical to K1: omap_tick_config_ptr/omap_tick_read_raw
 *  calls (omap_l108.c), same wraparound correction, same 150-tick (0x96)
 *  folding via omap_tick_scale (also omap_l108.c, K2 @0xc001ac94).
 *  @0xc000185c (K1: @0xc0001aa0).
 * ------------------------------------------------------------------------- */
extern uint32_t omap_tick_config_ptr(void);				/* FUN_c00016bc, see omap_l108.c */
extern uint32_t omap_tick_read_raw(void *handle);			/* FUN_c0001b28, see omap_l108.c */
extern int32_t  omap_tick_scale(int32_t ticks, int divisor);		/* FUN_c001ac94, see omap_l108.c */
extern int32_t  hw_timer_fold_remainder(int32_t raw_delta_acc, int divisor);	/* NOT independently reconstructed - same as K1's own FUN_c001e520 treatment */

void hw_timer_busy_wait(void *chip, int units)	/* FUN_c000185c */
{
	extern int32_t hw_timer_wrap_const;	/* DAT_c00018e4, 0x249f1 = 150001, identical to K1 */
	uint32_t baseline, now;
	int32_t  raw_delta_acc = 0;
	int32_t  elapsed_units = 0;

	omap_tick_config_ptr();
	baseline = omap_tick_read_raw(chip);

	do {
		omap_tick_config_ptr();
		now = omap_tick_read_raw(chip);

		if (now < baseline)
			raw_delta_acc += (int32_t)(now - baseline) + hw_timer_wrap_const;
		else
			raw_delta_acc += (int32_t)(now - baseline);

		if (raw_delta_acc > 0x95) {
			elapsed_units += omap_tick_scale(raw_delta_acc, 0x96);
			raw_delta_acc = hw_timer_fold_remainder(raw_delta_acc, 0x96);
		}

		baseline = now;
	} while (elapsed_units < units);
}

/* ------------------------------------------------------------------------- *
 *  board_desc_type_clear_enabled - single-bit (bit0) clear at handle+0x24.
 *  Identical to K1's own function of the same name. Only ONE caller found
 *  (FUN_c0000864) - K1's own 3-caller total and its paired set_enabled
 *  sibling were NOT found in this K2 cluster (see file header point 5).
 *  @0xc0001ae8 (K1: @0xc0001d28).
 * ------------------------------------------------------------------------- */
void board_desc_type_clear_enabled(int handle)	/* FUN_c0001ae8 */
{
	*(uint32_t *)(handle + 0x24) &= ~1u;
}

/* ------------------------------------------------------------------------- *
 *  board_desc_init_type5 - descriptor constructor, "type" field (+0x24) = 5,
 *  fixed +0x18 length constant (24001, IDENTICAL numeric value to K1's own
 *  0x5dbf). Sole caller FUN_c0000040, the Timer64P0 lazy-init singleton
 *  (same role as K1's own FUN_c0000040) - see file header point 5 for what's
 *  missing (the type4/caller-length sibling, and the second Timer64P1
 *  singleton). @0xc0001af8 (K1: @0xc0001d38).
 * ------------------------------------------------------------------------- */
void board_desc_init_type5(int handle)	/* FUN_c0001af8 */
{
	extern int32_t board_desc_type5_len_const;	/* DAT_c0001b24, 0x5dbf = 24001, identical to K1 */

	*(uint32_t *)(handle + 0x04) = 1;
	*(uint32_t *)(handle + 0x24) = 5;
	*(uint32_t *)(handle + 0x18) = board_desc_type5_len_const;
	*(uint32_t *)(handle + 0x20) = 0x80;
	*(uint32_t *)(handle + 0x44) = 3;
}

/* ------------------------------------------------------------------------- *
 *  timer64p0_enable_ch15 - LIVE-QUERY RESOLVED 2026-07-19 (get_function_info
 *  + decompile_function + get_disassembly + read_memory on 0xc0000040,
 *  kronos2s_v01r10_panel.elf). This is soc_irq_gate.c's own long-excluded
 *  "FUN_c0000040 (ch-0x15 enable, Timer64P0 lazy-init singleton)" - that
 *  file's own header has cited it every pass since its first 2026-07-19
 *  migration as "belongs to soc_periph.c's own territory... not defined
 *  there yet either". Defined HERE for the first time, closing that file's
 *  own oldest open item, since it genuinely IS this file's own subsystem
 *  (a Timer64P0 lazy bring-up primitive sitting alongside board_desc_init_
 *  type5/board_desc_type_clear_enabled, its own confirmed callees below),
 *  not an AINTC-gate leaf itself.
 *
 *  Lazy-init guard (DAT_c000008c, plain byte). Real decompile increments the
 *  guard rather than assigning 1 outright (`guard = guard + 1`), but this
 *  path is only reachable when the guard reads 0, so the observable effect
 *  is identical to a flat "=1" - transcribed faithfully as the real
 *  instruction rather than simplified.
 *
 *  CONFIRMED REAL CROSS-FILE FINDING: caches timer64p0_base_get()'s own
 *  return value into 0xC00E0000 - soc_irq_gate.c's own fixed AINTC-adjacent
 *  bookkeeping table, offset +0x00 (the table's very FIRST word) -
 *  confirmed directly via read_memory of this function's own literal pool
 *  (the literal at DAT_c0000094 reads 0xC00E0000 byte-for-byte, not assumed).
 *  This resolves soc_irq_gate.c's own long-standing "identity of soc_irq_
 *  gate_timer0_quiesce's own +0x44 handle" question: that handle is THIS
 *  SAME cached Timer64P0 base pointer, read back out of table+0x00 - see
 *  soc_irq_gate.c's own updated citation for soc_irq_gate_timer0_quiesce.
 *  It ALSO independently corrects a real, confirmed error in soc_irq_gate.c's
 *  own prior citation of that same slot as "table+0x08 = 0xC00E0008" - the
 *  live-read literal is 0xC00E0000, table offset +0x00, not +0x08 (see that
 *  file's own header for the full correction and its knock-on effect on a
 *  since-retracted "same slot as the CLUSTER 2 mcasp param cache" claim).
 *
 *  CONFIRMED REAL ARGUMENT to board_desc_init_type5, NOT phantom-forwarded:
 *  the freshly-cached timer64p0_base_get() return value is still live in r0
 *  at the `bl board_desc_init_type5` instruction (no reload in between) -
 *  i.e. this function constructs the type-5 board descriptor IN PLACE at the
 *  Timer64P0 base address itself. This corrects board_desc_init_type5's own
 *  prior comment above, which (working only from the static dump's own
 *  decompile, which dropped this implicit register-carried argument) called
 *  this call site's argument invisible - it is real and now transcribed.
 *
 *  Finally enables AINTC channel 0x15 (21) via EISR (+0x28), fetching
 *  aintc_base() fresh (its own phantom-forwarded table+0x4c dead handle,
 *  the same idiom soc_irq_gate.c documents throughout its own file) -
 *  matching that file's own "every function that writes ONLY +0x28 is an
 *  enable stub" idiom exactly. CONFIRMED sole caller: soc_irq_gate_group_a_
 *  enable (soc_irq_gate.c, FUN_c001995c) - the very first call in that
 *  dispatcher's own body (soc_irq_gate.c already cited this call site by
 *  address; this is the same function, now named and defined). @0xc0000040
 *  (K1: cited only by name/role in both K1's soc_periph.c and soc_irq_
 *  gate.c, never actually given a body either).
 * ------------------------------------------------------------------------- */
extern uint32_t aintc_base(void);	/* FUN_c0001664, aintc.c - see soc_irq_gate.c's own citation for the full AINTC EISR/EICR/SICR register model */

void timer64p0_enable_ch15(void)	/* FUN_c0000040 */
{
	extern uint8_t  timer64p0_enable_guard;	/* DAT_c000008c */
	extern uint32_t timer64p0_base_cache;		/* DAT_c0000094, resolved 0xC00E0000 - soc_irq_gate.c's own table+0x00, CONFIRMED via read_memory */
	extern uint32_t soc_periph_slot_0x4c_handle;	/* DAT_c0000090, resolved 0xC00E004C - soc_irq_gate.c's own table+0x4c dead phantom handle, SAME constant that file's own header independently derives from this exact address */
	uint32_t base;

	if (timer64p0_enable_guard != 0)
		return;
	timer64p0_enable_guard = timer64p0_enable_guard + 1;	/* real decompile: increment, not a flat assign - see note above */

	base = timer64p0_base_get((void *)(uintptr_t)soc_periph_slot_0x4c_handle);
	timer64p0_base_cache = base;

	board_desc_init_type5((int)base);	/* CONFIRMED real argument, not phantom-forwarded - see note above */

	*(uint32_t *)((uint8_t *)aintc_base() + 0x28) = 0x15;	/* EISR: enable ch 21 (0x15) */
}

/* ------------------------------------------------------------------------- *
 *  board_desc_set_pinmux_3word - see file header point 4 ("PINMUX WRITES
 *  MERGED INTO ONE FUNCTION"). Writes three fixed 32-bit constants into
 *  three CONSECUTIVE offsets (+0x110/+0x114/+0x118) of a caller-supplied
 *  descriptor in a single call - a real structural difference from K1's
 *  three separate one-word leaves at (+0x130 x2, +0x154). Values retain the
 *  same nibble-per-field PINMUX shape K1 already identified, just not
 *  independently decoded to specific pins this pass either. Sole caller:
 *  FUN_c0000800 (board bring-up). @0xc0001b30.
 * ------------------------------------------------------------------------- */
void board_desc_set_pinmux_3word(int handle)	/* FUN_c0001b30 */
{
	extern uint32_t board_pinmux_word_110;	/* DAT_c0001b4c, 0x44472221 */
	extern uint32_t board_pinmux_word_114;	/* DAT_c0001b50, 0x77470077 */
	extern uint32_t board_pinmux_word_118;	/* DAT_c0001b54, 0x54704404 */

	*(uint32_t *)(handle + 0x110) = board_pinmux_word_110;
	*(uint32_t *)(handle + 0x114) = board_pinmux_word_114;
	*(uint32_t *)(handle + 0x118) = board_pinmux_word_118;
}

/* NOTE: 0xc0001b58-0xc0001c3c (omap_syscfg_reset_and_enable, reg154, etc.)
 * is a DIFFERENT file's own confirmed anchor range - "../MCU/Component/
 * OmapL108Syscfg.cpp" - already fully reconstructed in omap_l108_syscfg.c by
 * a concurrent pass. Deliberately excluded here, same as K1's soc_periph.c
 * excludes omap_l108.c/aintc.c/panelbus_dispatch.c's own ranges. Note for
 * that file's own future maintainers: FUN_c0001b58/FUN_c0001bf4 in this
 * exact sub-range ARE present as real Ghidra Function objects in the K2
 * static dump (confirmed directly, contradicting that file's own "the
 * static function dump doesn't cover this address range" claim for AT LEAST
 * those two addresses) - worth a follow-up cross-check of that file's own
 * capstone-derived transcription against this dump for those two functions
 * specifically; NOT done here (out of this file's own scope to edit that
 * file). */

/* ------------------------------------------------------------------------- *
 *  psc_module_enable - see file header point 6. Byte-for-byte identical PSC
 *  "enable one LPSC module" sequence to K1's own psc_module_enable: MDCTL
 *  NEXT-field request, PTCMD go-bit, PTSTAT spin, MDSTAT spin. ZERO static
 *  callers - CONFIRMED identical to K1's own zero-caller finding (the
 *  sibling that DOES have callers, matching K1's own `gpio_psc_enable`
 *  extern-only role, is FUN_c0001d88 - now actually DEFINED, see
 *  omap_gpio.c). @0xc0001c50 (K1: @0xc0001e98).
 * ------------------------------------------------------------------------- */
void psc_module_enable(int psc_base, int lpsc_module, unsigned int power_domain)	/* FUN_c0001c50 */
{
	int mdctl = lpsc_module * 4 + psc_base + 0xa00;

	*(uint32_t *)mdctl = (*(uint32_t *)mdctl & ~0x7u) | 3u;	/* MDCTL[n].NEXT = Enable (3) */

	*(uint32_t *)(psc_base + 0x120) = 1u << (power_domain & 0xff);	/* PTCMD = go */

	while ((*(uint32_t *)(psc_base + 0x128) & (1u << (power_domain & 0xff))) != 0)
		;	/* spin on PTSTAT until this domain's transition completes */

	while ((*(uint32_t *)(lpsc_module * 4 + psc_base + 0x800) & 0x3fu) != 3u)
		;	/* spin on MDSTAT[n] until the module reports Enable (3) */
}

/* -------------------------------------------------------------------------
 * Still genuinely open (K2-specific, beyond what's flagged inline above):
 *  - i2c0_i2c1_base_select's/spi0_base_get's/ecap2_base_get's real
 *    consumption beyond the one traced caller each - not fully swept.
 *  - board_desc_init_type4 and the Timer64P1 lazy-init singleton: no K2
 *    counterpart found in this cluster - open whether K2 dropped them
 *    entirely or moved them elsewhere.
 *  - uart1_base_get's own UART0/UART2 siblings: not found near this
 *    cluster - NEEDS LIVE QUERY if K2's multi-UART base story matters
 *    (would benefit from a live xref/string sweep for "UART0"/"UART2"-
 *    shaped literal pools elsewhere in the image).
 *  - board_desc_set_pinmux_3word's own +0x110/+0x114/+0x118 field meanings,
 *    and which physical pin groups the three PINMUX words configure - not
 *    decoded, same as K1's own open item for its three separate words.
 *  - psc_module_enable's own zero-caller status - open whether dead code or
 *    reached indirectly, same shape as every other "zero callers" finding
 *    in this project (crypto_at88_self_test, cobjectmgr_object_destroy).
 * ------------------------------------------------------------------------- */
