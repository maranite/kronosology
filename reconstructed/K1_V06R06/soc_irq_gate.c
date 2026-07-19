/* SPDX-License-Identifier: GPL-2.0 */
/*
 * soc_irq_gate.c - the address block immediately after the ARM exception
 * vector table (0xC0000000) and immediately before the AT88 I2C primitives
 * (crypto_at88.c/i2c_by_gpio.c, starting ~0xc0000ef4): 0xc00000f4-0xc0000f30,
 * 28 real Ghidra function objects (30 minus 2 exclusions, see EXCLUDED
 * below). No prior pass reconstructed real bodies here - an earlier attempt
 * was interrupted before writing anything.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled.json
 * / all_data.json, 2026-07-18 pass), read via query_dump.py. No live Ghidra
 * MCP calls this pass (bridge flagged concurrency-unsafe).
 *
 * ANCHOR: NONE. All 14 real "../<Name>.cpp" strings in the image (per a
 * fresh `query_dump.py strings .cpp`) are already claimed by other files'
 * subsystems (see README's own Subsystems table). File named descriptively
 * per this project's established no-anchor convention (aintc.c,
 * heap_alloc.c, panelbus_dispatch.c, wire_dispatch.c, midi_engine.c,
 * chan_link_hw.c, chan_param_ctrl.c, chan_slot_dispatch.c,
 * usbdc_midi_status_glue.c, omap_l137_addr_gap_misc.c all do the same).
 *
 * EXCLUDED from this file (both physically inside the assigned range, both
 * ALREADY fully reconstructed elsewhere - not duplicated here per this
 * project's "never edit a file outside your own pass" convention):
 *   - 0xc0000ba0 = panel_gpio_reset_pulse, cpsoc.c (GPIO bank-3/bit-8 reset
 *     pulse, confirmed reachable from both the diagnostic menu and
 *     wire_dispatch_command's opcode 9).
 *   - 0xc0000ec8 = crypto_at88_queue_init, crypto_at88.c (zeroes the AT88
 *     2-deep ring buffer's index/count fields, offsets +0x40/+0x41/+0x42,
 *     then calls at88_i2c_bus_reset/FUN_c00014fc) - byte-for-byte identical
 *     decompile confirmed against crypto_at88.c's own text before excluding.
 *
 * =============================================================================
 * THE CENTRAL FINDING: a fixed ~0x6C-byte bookkeeping table at 0xC00E0000
 * =============================================================================
 *
 * Nearly every DAT_ constant referenced by this range's functions resolves
 * (via query_dump.py dat, cross-checked by hand: e.g. DAT_c000013c's
 * decompiled value -0x3ff1fff8 == 0x100000000-0x3ff1fff8 == 0xC00E0008)
 * into a small, fixed range: 0xC00E0000-0xC00E0068. This is DIFFERENT from
 * aintc.c's own aintc_sw_channel_table (0xC00E0204, 101 4-byte entries,
 * confirmed 0-terminated software channel-priority array) - a separate,
 * smaller table sitting just below it in the same data segment, used by
 * THIS range's functions as: (a) per-peripheral 1-byte "already
 * initialized" guard flags, (b) cached hardware-base/AINTC-handle pointers
 * written once by a lazy-init "enable" stub and read back by its "disable"
 * sibling, and (c) ONE specific slot, +0x68 (0xC00E0068), reused by more
 * than a dozen call sites throughout this range purely as a DEAD argument -
 * every callee that receives it (aintc_base/FUN_c00018e4, gpio_bank_get_base
 * /FUN_c0001990, omap_usbdc_reloc/FUN_c0009194) is independently documented
 * elsewhere in this project (aintc.c, i2c_by_gpio.c, omap_l137_usbdc.c) as
 * ignoring whatever it's handed - this project's now-repeated "phantom
 * forwarded parameter" idiom, confirmed here at well over a dozen
 * additional call sites. Individual functions below cite their own real
 * resolved table offset; this section documents the pattern once rather
 * than repeating the derivation 20 times.
 *
 * =============================================================================
 * THE AINTC INDEXED ENABLE/DISABLE REGISTER MODEL (extends aintc.c)
 * =============================================================================
 *
 * aintc.c's own header already confirms aintc_base() (FUN_c00018e4) returns
 * the fixed AINTC MMR base 0xFFFEE000, and that GER (+0x10) / HIER (+0x1500)
 * are real bring-up writes. Every "gate" function in this file writes a
 * small integer (a channel number, 0-100 per aintc.c's own 101-channel
 * table finding) into one or more of THREE further fixed offsets off that
 * same base:
 *
 *   +0x24  SICR - System Interrupt Status Clear Register (indexed: writing
 *          channel N clears any latched/pending status for channel N)
 *   +0x28  EISR - System Interrupt Enable SET Register (indexed: writing
 *          channel N enables channel N)
 *   +0x2C  EICR - System Interrupt Enable CLEAR Register (indexed: writing
 *          channel N disables channel N)
 *
 * These are documented AINTC "index" registers on the TI OMAP-L1x/DA8xx
 * family (you write the linear channel number, not a bitmask - the same
 * hardware idiom this project already established for GER/HIER). Every
 * function below that writes ONLY +0x28 is an "enable" stub; every function
 * that writes +0x2C and/or +0x24 (never +0x28) is a "disable+clear" stub.
 * This is a real, address-confirmed register model, not a guess - it
 * explains cleanly why every such stub writes the exact SAME channel number
 * to one or two of these three offsets and nothing else.
 *
 * =============================================================================
 * CROSS-FILE CONFIRMATION: task_sched.c's "group A" / "group B"
 * =============================================================================
 *
 * task_sched.c's own eva_board_crt0_tcb_and_kobj_init section (its own
 * address-labeled call list, c001cb10/c001cb60) ALREADY identifies the two
 * real callers of most of this file's "enable" and "disable" stubs:
 *
 *   FUN_c001ca34 ("group A") - calls FUN_c0000040 (soc_periph.c, Timer64P0,
 *     ch 0x15, "enable"), plus (this file) FUN_c000019c, FUN_c000027c,
 *     FUN_c00005f8, FUN_c000069c, FUN_c00007b8, FUN_c000087c - all "enable"
 *     stubs, all left as bare externs by task_sched.c ("all outside this
 *     sweep's range, left as bare externs").
 *   FUN_c001ca84 ("group B") - calls exactly 8 functions, all in this file:
 *     FUN_c00009d8, FUN_c0000404, FUN_c0000654, FUN_c0000848, FUN_c0000784,
 *     FUN_c000090c, FUN_c0000238, FUN_c00000f4 - all "disable+clear" stubs.
 *     task_sched.c explicitly notes group B is "NOT in eva_board_main.c's
 *     original 11-call list" - group B is a genuinely separate discovery.
 *
 * Additionally, wire_dispatch.c independently names FUN_c0008a5c (which
 * ALSO calls every one of group B's 8 disable stubs, per this file's own
 * xref data) as eva_link_status_change, invoked from master_dispatch_tick
 * on status bit 0x100 - i.e. group B's disable stubs run BOTH somewhere in
 * task_sched.c's own kernel-object bring-up chain AND, independently, every
 * time a live link-status-change event fires at runtime. Given
 * eva_board_crt0_tcb_and_kobj_init's own name (task registration, not
 * necessarily straight-line sequential execution) and the awkwardness of
 * enabling 7 IRQ channels then immediately disabling most of them one
 * instruction later, the most likely reading is that FUN_c001ca84's own
 * slot in that chain REGISTERS eva_link_status_change into a task/kernel
 * object for later event-driven dispatch, rather than calling it inline at
 * boot - plausible, NOT independently confirmed (no disassembly access to
 * task_sched.c's own registration primitives this pass). Left as a
 * genuinely open question rather than asserted as fact - see STILL OPEN.
 *
 * Everything below is additive - no existing file in this project touches
 * any address in 0xc00000f4-0xc0000f30 with a real function DEFINITION
 * (two addresses are cited in OTHER files' comments as evidence only, never
 * defined there - see each function's own note: FUN_c0000484 in
 * usbdc_midi_status_glue.c's PART B, FUN_c0000be8/FUN_c0000ba0 in cpsoc.c's
 * "confirmed NOT cpsoc's own code" sweep).
 */

#include <stdint.h>

/* ===========================================================================
 * CLUSTER 1 - Timer64P0/Timer64P1 AINTC enable/disable pairs (ch 0x15/0x17)
 *
 * Timer64P0's own "enable" half (FUN_c0000040, ch 0x15) lives in
 * soc_periph.c, out of this file's range - cited only. Its "disable" half
 * is here. Timer64P1's BOTH halves are here.
 * =========================================================================== */

extern uint32_t aintc_base(void);				/* FUN_c00018e4, aintc.c */
extern uint32_t timer64p_base_select(void *chip, int idx);	/* FUN_c00018fc, soc_periph.c */
extern void     board_desc_init_type4(int handle, int len_plus_one);	/* FUN_c0001cec, soc_periph.c */

/* soc_irq_gate_timer0_quiesce - disable+clear pair of soc_periph.c's own
 * FUN_c0000040 (Timer64P0 enable, ch 0x15/21). Sets bit 1 (0x2) in some
 * OTHER handle's own +0x44 register first (table slot 0xC00E0008, a cached
 * pointer set up elsewhere - not resolved which peripheral this is; same
 * "false neighbor, real evidence, unresolved identity" honesty this
 * project already applies elsewhere), then disables+clears AINTC ch 0x15
 * via EICR/SICR. Called from BOTH FUN_c0008a5c (eva_link_status_change,
 * wire_dispatch.c) and FUN_c001ca84 (task_sched.c's "group B") - see file
 * header. @0xc00000f4. */
extern uint32_t *soc_irq_gate_slot_0x08_handle;	/* DAT_c0000138, table+0x08 = 0xC00E0008, cached handle - identity NOT resolved */

void soc_irq_gate_timer0_quiesce(void)		/* FUN_c00000f4 */
{
	uint32_t aintc;

	*(uint32_t *)((uint8_t *)*soc_irq_gate_slot_0x08_handle + 0x44) |= 2;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x15;	/* EICR: disable ch 21 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x15;	/* SICR: clear ch 21 status */
}

/* soc_irq_gate_timer1_enable - lazy-init singleton (guarded by table+0x14
 * being non-zero, the SAME "guard doubles as cached handle" idiom
 * soc_periph.c's own header already documents for FUN_c0000040), Timer64P1
 * version. Fetches timer64p_base_select(_, 1), constructs a type-4
 * board descriptor with length 0x1770 (6000, table+0x00 literal pool - real
 * unit not decoded), caches the AINTC handle into table+0x14, and enables
 * ch 0x17 (23) via EISR. Called from FUN_c001ca34 (task_sched.c's "group
 * A"). Structurally identical to soc_periph.c's own FUN_c0000040, per that
 * file's own header note. @0xc000019c. */
extern uint32_t *soc_irq_gate_timer1_handle;	/* DAT_c00001f4, table+0x14 = 0xC00E0014 - cached AINTC handle AND init-done guard */
extern uint32_t *soc_irq_gate_timer1_base;	/* DAT_c00001fc, table+0x10 = 0xC00E0010 - cached Timer64P1 base */
extern uint32_t  soc_irq_gate_slot_0x68_a;	/* DAT_c00001f8, table+0x68 = 0xC00E0068 - dead phantom-handle literal, see file header */
extern uint32_t  soc_irq_gate_timer1_desc_len;	/* DAT_c0000200, literal 0x1770 (6000) */

void soc_irq_gate_timer1_enable(void)		/* FUN_c000019c */
{
	uint32_t base;
	uint32_t aintc;

	if (*soc_irq_gate_timer1_handle != 0)
		return;

	base = timer64p_base_select((void *)(uintptr_t)soc_irq_gate_slot_0x68_a, 1);
	*soc_irq_gate_timer1_base = base;

	aintc = aintc_base();
	*soc_irq_gate_timer1_handle = aintc;

	board_desc_init_type4((int)*soc_irq_gate_timer1_base, (int)soc_irq_gate_timer1_desc_len);
	*(uint32_t *)((uint8_t *)(*soc_irq_gate_timer1_handle) + 0x28) = 0x17;	/* EISR: enable ch 23 */
}

/* soc_irq_gate_timer1_quiesce - disable+clear pair of the above, reusing
 * the SAME table+0x10/+0x14 cached-pointer slots (confirmed: same DAT_
 * resolved addresses). Also sets bit 1 (0x2) in the cached Timer64P1 base's
 * own +0x44 register, same shape as soc_irq_gate_timer0_quiesce above.
 * Called from FUN_c0008a5c/FUN_c001ca84 ("group B", see file header).
 * @0xc0000238. */
void soc_irq_gate_timer1_quiesce(void)		/* FUN_c0000238 */
{
	uint32_t *aintc_slot = soc_irq_gate_timer1_handle;
	uint32_t *base_slot  = soc_irq_gate_timer1_base;
	uint32_t  aintc       = *aintc_slot;

	*(uint32_t *)((uint8_t *)*base_slot + 0x44) |= 2;
	*aintc_slot = 0;
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x17;	/* EICR: disable ch 23 */
	*base_slot = 0;
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x17;	/* SICR: clear ch 23 status */
}

/* ===========================================================================
 * CLUSTER 2 - channel 0x0b (11) pair: McASP clock-param latch (enable side)
 * / an unidentified 3-word register-block sequence (disable side)
 *
 * DECISIVE cross-file link for the enable side: mcasp.c's own header
 * already confirms FUN_c000199c/FUN_c00019b0 ("mis-modeled" as void
 * synchronization primitives in an earlier pass, corrected there) are
 * REAL getters, confirmed via THIS function (FUN_c000027c) being the
 * "second, independent call site" where their return values are captured
 * into globals - mcasp.c names them mcasp_clock_param_select_a/b but does
 * NOT itself define FUN_c000027c (only cites it as evidence). Reconstructed
 * here for the first time.
 * =========================================================================== */

extern uint32_t mcasp_clock_param_select_a(void *base, int sel);	/* FUN_c000199c, mcasp.c */
extern uint32_t mcasp_clock_param_select_b(void *base, int sel);	/* FUN_c00019b0, mcasp.c */

/* soc_irq_gate_ch0b_enable_mcasp_latch - lazy-init guard is a plain BYTE
 * flag (table+0x38, UNLIKE the pointer-guard idiom used above), enabling ch
 * 0x0b (11) via EISR on first call. THEN, unconditionally on every call
 * (not just first), latches mcasp_clock_param_select_a(_, 0) into
 * table+0x30 and mcasp_clock_param_select_b(_, 0) into table+0x18, and
 * caches aintc_base() into table+0x34. The mixed "guarded IRQ enable, then
 * unconditional param re-latch every call" shape is transcribed exactly as
 * decompiled, not simplified. @0xc000027c. */
extern uint8_t  *soc_irq_gate_ch0b_guard;	/* DAT_c00002ec, table+0x38 = 0xC00E0038 */
extern uint32_t  soc_irq_gate_slot_0x68_b;	/* DAT_c00002f0, table+0x68 = 0xC00E0068 - dead phantom handle */
extern uint32_t *soc_irq_gate_mcasp_param_a_cache;	/* DAT_c00002f4, table+0x30 = 0xC00E0030 */
extern uint32_t *soc_irq_gate_mcasp_param_b_cache;	/* DAT_c00002f8, table+0x18 = 0xC00E0018 */
extern uint32_t *soc_irq_gate_ch0b_aintc_cache;	/* DAT_c00002fc, table+0x34 = 0xC00E0034 */

void soc_irq_gate_ch0b_enable_mcasp_latch(void)	/* FUN_c000027c */
{
	if (*soc_irq_gate_ch0b_guard == 0) {
		uint32_t aintc;
		*soc_irq_gate_ch0b_guard = 1;
		aintc = aintc_base();
		*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x0b;	/* EISR: enable ch 11 */
	}

	*soc_irq_gate_mcasp_param_a_cache =
		mcasp_clock_param_select_a((void *)(uintptr_t)soc_irq_gate_slot_0x68_b, 0);
	*soc_irq_gate_mcasp_param_b_cache =
		mcasp_clock_param_select_b((void *)(uintptr_t)soc_irq_gate_slot_0x68_b, 0);
	*soc_irq_gate_ch0b_aintc_cache = aintc_base();
}

/* soc_irq_gate_ch0b_quiesce - disable+clear of ch 0x0b (11), NOT confirmed
 * to be the direct pair of the McASP-latch function above despite sharing
 * the same channel number (called from group B, not group A's own
 * companion slot - see file header's "group A"/"group B" list, which pairs
 * this function with FUN_c000027c only by shared channel number, not by
 * any other structural link). Pokes a 3-word register block at a cached
 * handle's own +0x2068/+0x2070/+0x2078 - spacing (8 bytes) and the
 * conditional-then-unconditional write to +0x2070 (1, then checked, then
 * unconditionally overwritten to 2) are transcribed exactly as decompiled,
 * including the apparently-redundant double check of +0x2068 - a genuine
 * quirk, not a transcription error. Peripheral identity of this register
 * block NOT resolved - see STILL OPEN. @0xc0000404. */
extern uint32_t *soc_irq_gate_ch0b_regblock;	/* DAT_c0000470, table+0x30 = 0xC00E0030 - SAME slot as mcasp_param_a_cache above (shared with the enable side) */
extern uint32_t  soc_irq_gate_slot_0x68_c;	/* DAT_c000047c, table+0x68 = 0xC00E0068 - dead phantom handle */

void soc_irq_gate_ch0b_quiesce(void)	/* FUN_c0000404 */
{
	uint8_t *regs = (uint8_t *)*soc_irq_gate_ch0b_regblock;
	uint32_t aintc;

	*(uint32_t *)(regs + 0x2070) = 1;
	if (*(uint32_t *)(regs + 0x2068) != 0)
		*(uint32_t *)(regs + 0x2078) = 1;
	*(uint32_t *)(regs + 0x2070) = 2;	/* overwrites the =1 write above - faithful, not simplified */
	if (*(uint32_t *)(regs + 0x2068) != 0)	/* same condition re-checked - faithful, not simplified */
		*(uint32_t *)(regs + 0x2078) = 1;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x0b;	/* EICR: disable ch 11 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x0b;	/* SICR: clear ch 11 status */
}

/* ===========================================================================
 * CLUSTER 3 - channel 0x3a (58) pair: USB0/OTG-adjacent
 *
 * Disable side confirmed USB0/OTG-adjacent via usb0_otg_base_get
 * (soc_periph.c, FUN_c0001a80) - a real, named, address-cited cross-file
 * link.
 * =========================================================================== */

extern uint32_t usb0_otg_base_get(void *chip);	/* FUN_c0001a80, soc_periph.c */

/* soc_irq_gate_ch3a_enable - lazy-init guard (byte flag, table+0x3c),
 * enables ch 0x3a (58) via EISR only. @0xc00005f8. */
extern uint8_t  *soc_irq_gate_ch3a_guard;	/* DAT_c0000630, table+0x3c = 0xC00E003C */
extern uint32_t  soc_irq_gate_slot_0x68_d;	/* DAT_c0000634, table+0x68 = 0xC00E0068 - dead phantom handle */

void soc_irq_gate_ch3a_enable(void)	/* FUN_c00005f8 */
{
	uint32_t aintc;

	if (*soc_irq_gate_ch3a_guard != 0)
		return;
	*soc_irq_gate_ch3a_guard = 1;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x3a;	/* EISR: enable ch 58 */
}

/* soc_irq_gate_ch3a_quiesce - disable+clear of ch 0x3a (58). Also copies
 * usb0_otg_base_get(chip)'s own +0x20 field into that SAME handle's +0x28
 * field first (a real, decompile-confirmed self-referential copy - not a
 * transcription artifact). @0xc0000654. */
extern uint32_t  soc_irq_gate_slot_0x68_e;	/* DAT_c0000698, table+0x68 = 0xC00E0068 - dead phantom handle */

void soc_irq_gate_ch3a_quiesce(void)	/* FUN_c0000654 */
{
	uint8_t *usb0 = (uint8_t *)usb0_otg_base_get((void *)(uintptr_t)soc_irq_gate_slot_0x68_e);
	uint32_t aintc;

	*(uint32_t *)(usb0 + 0x28) = *(uint32_t *)(usb0 + 0x20);

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x3a;	/* EICR: disable ch 58 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x3a;	/* SICR: clear ch 58 status */
}

/* ===========================================================================
 * CLUSTER 4 - channel 0x35 (53) pair: UART1
 *
 * Enable side confirmed UART1 via uart_base_select(chip, 1) - soc_periph.c
 * OWNS and DEFINES FUN_c0001a38 as uart_base_select(void *chip, int idx)
 * (real body: 3-way TRM-matched base selector). NOTE: usbdc_midi_status_
 * glue.c independently declares the SAME address as an opaque extern
 * `chan_selector_object(void *handle, int selector)` with a DIFFERENT
 * signature/name - an existing, already-flagged cross-file naming split
 * (not resolved here, not this file's to fix); this file follows
 * soc_periph.c's name since that is the file that actually owns/defines
 * the real function body.
 * =========================================================================== */

extern uint32_t uart_base_select(void *chip, int idx);	/* FUN_c0001a38, soc_periph.c - see note above re: usbdc_midi_status_glue.c's independent "chan_selector_object" naming */

/* soc_irq_gate_ch35_enable - lazy-init guard (byte flag, table+0x44),
 * fetches uart_base_select(_, 1) (UART1) into table+0x48, enables ch 0x35
 * (53) via EISR. @0xc000069c. */
extern uint8_t  *soc_irq_gate_ch35_guard;	/* DAT_c00006e8, table+0x44 = 0xC00E0044 */
extern uint32_t  soc_irq_gate_slot_0x68_f;	/* DAT_c00006ec, table+0x68 = 0xC00E0068 - dead phantom handle */
extern uint32_t *soc_irq_gate_uart1_base_cache;	/* DAT_c00006f0, table+0x40 = 0xC00E0040 */

void soc_irq_gate_ch35_enable(void)	/* FUN_c000069c */
{
	uint32_t aintc;

	if (*soc_irq_gate_ch35_guard != 0)
		return;
	*soc_irq_gate_ch35_guard = 1;

	*soc_irq_gate_uart1_base_cache =
		uart_base_select((void *)(uintptr_t)soc_irq_gate_slot_0x68_f, 1);

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x35;	/* EISR: enable ch 53 */
}

/* soc_irq_gate_ch35_quiesce - disable+clear of ch 0x35 (53), no extra
 * hardware pokes (unlike its cluster-2/3 siblings). @0xc0000784. */
extern uint32_t  soc_irq_gate_slot_0x68_g;	/* DAT_c00007b4, table+0x68 = 0xC00E0068 - dead phantom handle */

void soc_irq_gate_ch35_quiesce(void)	/* FUN_c0000784 */
{
	uint32_t aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x35;	/* EICR: disable ch 53 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x35;	/* SICR: clear ch 53 status */
}

/* ===========================================================================
 * CLUSTER 5 - channel 0x36 (54) pair: peripheral identity NOT resolved
 *
 * Structurally identical to cluster 4 (guard flag, EISR-only enable,
 * EICR+SICR-only disable) but with no extra callee giving away which
 * peripheral this is - transcribed literally.
 * =========================================================================== */

extern uint8_t  *soc_irq_gate_ch36_guard;	/* DAT_c00007f0, table+0x48 = 0xC00E0048 */
extern uint32_t  soc_irq_gate_slot_0x68_h;	/* DAT_c00007f4, table+0x68 = 0xC00E0068 - dead phantom handle */

void soc_irq_gate_ch36_enable(void)	/* FUN_c00007b8 */
{
	uint32_t aintc;

	if (*soc_irq_gate_ch36_guard != 0)
		return;
	*soc_irq_gate_ch36_guard = 1;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x36;	/* EISR: enable ch 54 */
}

extern uint32_t  soc_irq_gate_slot_0x68_i;	/* DAT_c0000878, table+0x68 = 0xC00E0068 - dead phantom handle */

void soc_irq_gate_ch36_quiesce(void)	/* FUN_c0000848 */
{
	uint32_t aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x36;	/* EICR: disable ch 54 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x36;	/* SICR: clear ch 54 status */
}

/* ===========================================================================
 * CLUSTER 6 - channel 0x32 (50) pair: GPIO-bank-adjacent
 *
 * Enable side caches gpio_bank_get_base()'s own return value (real GPIO
 * bank base, i2c_by_gpio.c's own documented shared primitive - NOT the
 * dead-argument idiom this time, the return value IS used); disable side
 * reads that SAME cached pointer back and writes to its own +0xd4 field -
 * a real, confirmed shared-state enable/disable pair, not just a shared
 * channel number.
 * =========================================================================== */

extern void *gpio_bank_get_base(void);	/* FUN_c0001990, i2c_by_gpio.c */

/* soc_irq_gate_ch32_enable_gpio - lazy-init guard (byte flag, table+0x50),
 * caches gpio_bank_get_base() into table+0x4c, enables ch 0x32 (50) via
 * EISR. @0xc000087c. */
extern uint8_t   *soc_irq_gate_ch32_guard;	/* DAT_c00008c4, table+0x50 = 0xC00E0050 */
extern uint32_t   soc_irq_gate_slot_0x68_j;	/* DAT_c00008c8, table+0x68 = 0xC00E0068 - dead phantom handle */
extern void     **soc_irq_gate_gpio_bank_cache;	/* DAT_c00008cc, table+0x4c = 0xC00E004C */

void soc_irq_gate_ch32_enable_gpio(void)	/* FUN_c000087c */
{
	uint32_t aintc;

	if (*soc_irq_gate_ch32_guard != 0)
		return;
	*soc_irq_gate_ch32_guard = 1;

	*soc_irq_gate_gpio_bank_cache = gpio_bank_get_base();

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x28) = 0x32;	/* EISR: enable ch 50 */
}

/* soc_irq_gate_ch32_quiesce_gpio - disable+clear of ch 0x32 (50). Writes 4
 * into the CACHED gpio bank base's own +0xd4 field (real GPIO register
 * offset, meaning not decoded - candidate: an interrupt-status-clear or
 * debounce-control register on this bank; not confirmed against the DA850
 * GPIO TRM this pass). @0xc000090c. */
extern void   **soc_irq_gate_gpio_bank_cache_2;	/* DAT_c000094c, table+0x4c = 0xC00E004C - SAME slot as the enable side above */
extern uint32_t  soc_irq_gate_slot_0x68_k;	/* DAT_c0000950, table+0x68 = 0xC00E0068 - dead phantom handle */

void soc_irq_gate_ch32_quiesce_gpio(void)	/* FUN_c000090c */
{
	uint32_t aintc;

	*(uint32_t *)((uint8_t *)*soc_irq_gate_gpio_bank_cache_2 + 0xd4) = 4;

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x32;	/* EICR: disable ch 50 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x32;	/* SICR: clear ch 50 status */
}

/* ===========================================================================
 * CLUSTER 7 - channel 0x2a (42), disable-only (group B has no group-A
 * counterpart for this channel anywhere in this file's own range)
 * =========================================================================== */

extern void gpio_pair0_intstat_ack_bit5(void *bank_base);	/* FUN_c0002574, omap_gpio.c */

/* soc_irq_gate_ch2a_quiesce - disable+clear of ch 0x2a (42). Re-fetches
 * gpio_bank_get_base() fresh (does NOT read back a cached pointer, unlike
 * cluster 6's disable side - a real, confirmed asymmetry) and calls
 * gpio_pair0_intstat_ack_bit5 on it (return discarded) before the AINTC
 * writes. No matching "enable" stub found anywhere in this file's assigned
 * range - see STILL OPEN. @0xc00009d8. */
extern uint32_t soc_irq_gate_slot_0x68_l;	/* DAT_c0000a14, table+0x68 = 0xC00E0068 - dead phantom handle */

void soc_irq_gate_ch2a_quiesce(void)	/* FUN_c00009d8 */
{
	uint32_t aintc;

	(void)gpio_bank_get_base();
	gpio_pair0_intstat_ack_bit5(0 /* real call site shows no visible argument - phantom-forward per this project's established idiom */);

	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x2c) = 0x2a;	/* EICR: disable ch 42 */
	aintc = aintc_base();
	*(uint32_t *)((uint8_t *)aintc + 0x24) = 0x2a;	/* SICR: clear ch 42 status */
}

/* ===========================================================================
 * CLUSTER 8 - two larger one-shot crt0 bring-up stubs (NOT part of the
 * group-A/group-B channel-gate family - each has exactly one caller)
 * =========================================================================== */

extern void     gpio_bank_hw_init(void *bank_base);		/* FUN_c0002248, omap_gpio.c */
extern uint32_t ecap1_base_get(void *chip);			/* FUN_c0001a74, soc_periph.c */
extern void     FUN_c00054ac(void *handle);			/* eva_crt0_tick_glue.c - "name is a location label, not a confirmed subsystem attribution", left as-is here too */
extern void     usbdc_gap_config_slot(void *obj, int mode);	/* FUN_c00032a8, omap_l137_addr_gap_misc.c */
extern void     board_desc_flag_clear(uint8_t *flag);		/* FUN_c0001b2c, soc_periph.c */

/* soc_irq_gate_gap_slot_bringup - the VERY FIRST real subsystem bring-up
 * step in eva_board_crt0 (FUN_c00055b8): its own real call site is
 * 0xc00055e8, immediately before the aintc_base()/aintc_init() sequence
 * eva_crt0_tick_glue.c's own transcription of eva_board_crt0 begins with
 * (0xc00055f0/_f4) - i.e. this runs before AINTC itself is configured.
 * omap_l137_addr_gap_misc.c's own "Cluster 2" section ALREADY documents
 * this function's caller relationship to usbdc_gap_config_slot
 * (FUN_c00032a8, called here twice with mode=0 and mode=1) and explicitly
 * flags FUN_c0000a20 itself as "neither reconstructed by any file so far"
 * - resolved here for the first time.
 *
 * Sequence: gpio_bank_get_base()'s return discarded (called with a dead
 * argument then ignored even as a return value - a genuine no-op priming
 * call, transcribed faithfully); gpio_bank_hw_init() and FUN_c00054ac()
 * called with NO visible argument each (phantom-forward idiom, same as
 * elsewhere in this project); ecap1_base_get()'s return discarded;
 * FUN_c0001a00 (naming split - see below) used twice as a 2-way object
 * selector (index 0 then 1), each feeding usbdc_gap_config_slot; a flag
 * byte cleared via board_desc_flag_clear (called on the raw dead-handle
 * literal reinterpreted as a byte pointer - a real, decompile-confirmed
 * type-punning quirk, not a bug introduced here); finally
 * gpio_bank_get_base() called AGAIN, this time its real return value used,
 * feeding a GPIO IN_DATA-style register read (FUN_c0002238(base,0) - the
 * exact call shape i2c_by_gpio.c's own gpio_bank_read_sda_bit already
 * documents for reading that register, though bit 18/SDA is not what's
 * extracted here) whose return (an "extraout_var" Ghidra artifact - the
 * callee's return register, left dangling by decompilation) is inverted
 * and right-shifted 7, storing a single detect-style bit into table+0x58.
 * Physical meaning of that final detect bit NOT resolved - see STILL OPEN.
 * @0xc0000a20. */
extern void    *FUN_c0001a00(void *handle, int which);	/* eva_board_main.c names this eva_board_probe_bus_handle; cpsoc.c independently names it cpsoc_get_scan_handle - existing, unresolved cross-file naming split, not settled here */
extern int      FUN_c0002238(void *bank, int unused);		/* out of range - i2c_by_gpio.c cites this exact 2-arg shape (bank,0) as reading a GPIO IN_DATA-style register */
extern uint32_t soc_irq_gate_slot_0x68_m;	/* DAT_c0000a9c, table+0x68 = 0xC00E0068 - dead phantom handle, ALSO reused here as a real handle argument to ecap1_base_get/FUN_c0001a00/board_desc_flag_clear (transcribed as-is; whether those uses are also "dead" is not independently confirmed the way aintc_base's are) */
extern uint8_t  *soc_irq_gate_gap_detect_flag;	/* DAT_c0000aa0, table+0x58 = 0xC00E0058 */

void soc_irq_gate_gap_slot_bringup(void)	/* FUN_c0000a20 */
{
	uint32_t handle = soc_irq_gate_slot_0x68_m;
	void *obj;
	void *gpio_base;
	int   detect_raw;

	(void)gpio_bank_get_base();				/* return discarded - real no-op priming call */
	gpio_bank_hw_init(0 /* phantom-forward, no visible arg at real call site */);
	(void)ecap1_base_get((void *)(uintptr_t)handle);	/* return discarded */
	FUN_c00054ac(0 /* phantom-forward, no visible arg at real call site */);

	obj = FUN_c0001a00((void *)(uintptr_t)handle, 0);
	usbdc_gap_config_slot(obj, 0);
	obj = FUN_c0001a00((void *)(uintptr_t)handle, 1);
	usbdc_gap_config_slot(obj, 1);

	board_desc_flag_clear((uint8_t *)(uintptr_t)handle);	/* dead-handle literal reinterpreted as a byte flag pointer - real, decompile-confirmed */

	gpio_base = gpio_bank_get_base();	/* real return value used this time, unlike the priming call above */
	detect_raw = FUN_c0002238(gpio_base, 0);
	*soc_irq_gate_gap_detect_flag = (uint8_t)(~detect_raw) >> 7;
}

/* soc_irq_gate_mcasp2_bringup - McASP-2 (second instance) bring-up stub,
 * confirmed via omap_l137_addr_gap_misc.c's own "Cluster 1" section: this
 * function is the SOLE caller of mcasp2_reduced_init (FUN_c0003228,
 * defined there), which that file's own header explicitly cites as
 * evidence but does not itself reconstruct ("a small bring-up sequence...
 * itself not reconstructed, out of every file's confirmed scope so far") -
 * resolved here. Called once, from FUN_c0008a5c (eva_link_status_change,
 * wire_dispatch.c's own naming) - i.e. this runs on the SAME runtime
 * link-status-change event as cluster 1-7's "group B" disable sweep, not
 * (only) at boot. Fetches two selector values (FUN_c00019e0/FUN_c00019e8 -
 * NOT independently named/confirmed anywhere else in this project, McASP-
 * instance-selector-shaped by analogy to mcasp_clock_param_select_a/b but
 * not confirmed) and feeds them into mcasp2_reduced_init(ma, sub_config) as
 * (sub_config, ma) - argument ORDER swap vs. that function's own
 * declaration is real and decompile-confirmed, not a transcription error.
 * The remaining 10 calls are a flat, unconditional bring-up sequence with
 * no branching: two more McASP-domain selector/param calls
 * (mcasp_clock_param_select_a/b(_, 0), matching cluster 2's own enable
 * side), then a chain of GPIO pair-bank primitives, ALL already named and
 * DEFINED in omap_gpio.c (gpio_pair3_bit15_set, gpio_pair4_bit_set) -
 * cited as extern, not redefined. @0xc0000aa4. */
extern void     mcasp2_reduced_init(void *ma, void *sub_config);	/* FUN_c0003228, omap_l137_addr_gap_misc.c */
extern void     board_desc_type_clear_enabled(int handle);		/* FUN_c0001d28, soc_periph.c - real sig takes a handle; both real call sites below show it with NO visible argument (phantom-forward, same idiom as elsewhere) */
extern uint32_t timer64p0_base_get(void *chip);			/* FUN_c00018f0, soc_periph.c */
extern void     gpio_pair3_bit15_set(void *bank_base);			/* FUN_c0002320, omap_gpio.c */
extern void     gpio_pair0_intstat_ack_bit5_dup(void *bank_base);	/* alias note: FUN_c0002320/_2574 are distinct, see own decls above/below - kept separate per real address */
extern void     gpio_pair4_bit_set(void *bank_base, uint32_t mask);	/* FUN_c0002344, omap_gpio.c */
extern int       gpio_reg_clear_bit_variant(void *bank, int group, uint32_t mask);	/* FUN_c00022e0, cpsoc.c names this gpio_reg_clear_bit - kept as a distinct local extern to avoid asserting the exact same signature this file's own call site shows (mask literal 0x100) */
extern void     *FUN_c00019e0(void *chip);	/* NOT independently confirmed elsewhere - McASP-instance-selector-shaped by call-site analogy to FUN_c000199c/_19b0 only */
extern void     *FUN_c00019e8(void *chip);	/* same caveat as FUN_c00019e0 above */

void soc_irq_gate_mcasp2_bringup(void)	/* FUN_c0000aa4 */
{
	uint32_t handle = soc_irq_gate_slot_0x68_m;	/* DAT_c0000b34, same resolved 0xC00E0068 slot */
	void *sub_config;
	void *ma;
	void *gpio;

	sub_config = FUN_c00019e8((void *)(uintptr_t)handle);
	ma         = FUN_c00019e0((void *)(uintptr_t)handle);
	mcasp2_reduced_init(ma, sub_config);

	(void)mcasp_clock_param_select_a((void *)(uintptr_t)handle, 0);

	timer64p0_base_get(0 /* phantom-forward */);
	board_desc_type_clear_enabled(0 /* phantom-forward */);

	(void)timer64p_base_select((void *)(uintptr_t)handle, 1);
	board_desc_type_clear_enabled(0 /* phantom-forward */);

	(void)ecap1_base_get((void *)(uintptr_t)handle);
	/* FUN_c0001e38 - out of range, no other file names it; left uncalled
	 * here would be wrong (real decompile DOES call it with no visible
	 * arg) - declared immediately below and invoked faithfully. */
	extern void FUN_c0001e38(void);
	FUN_c0001e38();

	(void)gpio_bank_get_base();
	/* FUN_c0002320 - real decompile calls it with NO visible argument. */
	gpio_pair3_bit15_set(0 /* phantom-forward */);

	gpio = gpio_bank_get_base();
	gpio_pair4_bit_set(gpio, 0xf000);

	gpio = gpio_bank_get_base();
	(void)gpio_reg_clear_bit_variant(gpio, 0, 0x100);
}

/* ===========================================================================
 * CLUSTER 9 - USB endpoint register-block pointer setup (0xc0000be8)
 *
 * cpsoc.c's own "confirmed out of scope" sweep already examined this
 * address and explicitly ruled it OUT of cpsoc.cpp ("confirmed NOT cpsoc's
 * own code... same USB-subsystem false-neighbor pattern"), citing its call
 * into omap_usbdc_reloc (FUN_c0009194) and its real caller (FUN_c000bc1c,
 * itself cited by chan_link_hw.c as "out of range, unclaimed 'slot
 * dispatch' cluster"). cpsoc.c does NOT define a body for it. Resolved
 * here for the first time.
 * =========================================================================== */

extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c0009194, omap_l137_usbdc.c */

/* soc_irq_gate_usbdc_ep_ptrs_init - computes two USB endpoint-adjacent
 * pointers (reloc_base+0x2e00, reloc_base+0x3000 - 0x200 apart, plausibly
 * two adjacent endpoint register blocks or ring-table halves, not
 * independently confirmed) and stores them into two fixed globals.
 * @0xc0000be8. */
extern uint32_t  soc_irq_gate_usbdc_reloc_arg;	/* DAT_c0000c24, resolved 0xC01CCB10 - the SAME usbdc_reloc_base literal usbdc_midi_status_glue.c/midi_engine.c/omap_l137_usbdc.c already document */
extern uint32_t *soc_irq_gate_usbdc_ep_ptr_a;	/* DAT_c0000c28 */
extern uint32_t *soc_irq_gate_usbdc_ep_ptr_b;	/* DAT_c0000c2c */

void soc_irq_gate_usbdc_ep_ptrs_init(void)	/* FUN_c0000be8 */
{
	*soc_irq_gate_usbdc_ep_ptr_a = omap_usbdc_reloc(soc_irq_gate_usbdc_reloc_arg) + 0x2e00;
	*soc_irq_gate_usbdc_ep_ptr_b = omap_usbdc_reloc(soc_irq_gate_usbdc_reloc_arg) + 0x3000;
}

/* ===========================================================================
 * CLUSTER 10 - MIDI hardware register primitives (0xc0000c38-0xc0000dec)
 *
 * midi_engine.c's own header ALREADY forward-declares FUN_c0000c38/_c6c as
 * `midi_hw_write16`/`midi_hw_read16` ("Hardware register access primitives
 * - out of this file's own range... cited from every low-level midi_hw_*
 * function below") - this file provides the real bodies for the first
 * time, using midi_engine.c's own names/signatures for continuity.
 * chan_link_hw.c independently calls midi_hw_read16 too (its own
 * chan_link_hw_service, confirmed via that file's own text) - concrete,
 * address-verified evidence this pair is a genuinely SHARED low-level
 * accessor (129 and 53 real callers respectively, spanning far more of the
 * image than midi_engine.c's own 26-function range), not MIDI-exclusive
 * despite the name inherited from its first-documented caller.
 *
 * Both resolve their own MMIO base through FUN_c0001a98 - out of range,
 * NOT reconstructed here either (midi_engine.c's own header already
 * documents it: "unconditionally returns the fixed literal 0x62000000
 * regardless of any argument", the same repeated phantom-parameter idiom).
 * =========================================================================== */

extern uint32_t FUN_c0001a98(uint32_t unused);	/* out of range; midi_engine.c's header: fixed return 0x62000000, argument dead */

extern uint32_t soc_irq_gate_midi_base_arg_w;	/* DAT_c0000c68, table+0x68 = 0xC00E0068 - dead phantom handle, fed into FUN_c0001a98 which ignores it anyway */

void midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val)	/* FUN_c0000c38 */
{
	(void)base_sel;
	uint32_t base = FUN_c0001a98(soc_irq_gate_midi_base_arg_w);
	*(uint16_t *)(base + (reg_off & 0xffff)) = val;
}

extern uint32_t soc_irq_gate_midi_base_arg_r;	/* DAT_c0000c94, table+0x68 = 0xC00E0068 - dead phantom handle, same as write side (separate literal-pool slot) */

uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off)	/* FUN_c0000c6c */
{
	(void)base_sel;
	uint32_t base = FUN_c0001a98(soc_irq_gate_midi_base_arg_r);
	return *(uint16_t *)(base + (reg_off & 0xffff));
}

/* midi_hw_fifo_write - bulk push of `len` bytes (as `len/2` 16-bit words
 * plus an optional trailing odd byte) into channel `channel`'s FIFO PUSH
 * register. Channel 0 uses a fixed register (base+0x6e); channel N != 0
 * uses base+N*0x20+0x72 - the SAME 0x20-byte-per-channel stride
 * chan_link_hw.c's own midi_hw_read16 callers already use. CRITICAL: the
 * destination register address is computed ONCE and reused for every word
 * written (real hardware FIFO push-register semantics, NOT a memory copy -
 * `puVar3`/dst is never incremented in either loop in the real decompile,
 * transcribed exactly). After the data phase, triggers the transfer by
 * writing a command byte (0x80 normal completion, 0xa0 if a trailing odd
 * byte was written, matching the two `goto LAB_c0000d88` sites in the raw
 * decompile - both collapsed into the shared `trigger` value here without
 * changing behavior) to a channel-indexed command register: channel 0
 * always uses 0x60; channel N != 0 uses `(N*0x20+0x68) & 0xfff8` (DAT_
 * c0000da4, real literal 0xfff8 - low-3-bit alignment mask). Channel 0's
 * status pre-read (`midi_hw_read16(_, 0x70)`, return discarded) is a real,
 * confirmed dummy poll before the data phase - not removed. @0xc0000c98.
 */
extern uint32_t soc_irq_gate_midi_base_arg_fw;	/* DAT_c0000da0, table+0x68 = 0xC00E0068 - dead phantom handle */
extern uint16_t soc_irq_gate_midi_cmd_align_mask;	/* DAT_c0000da4, real literal 0xfff8 */

void midi_hw_fifo_write(uint32_t base_sel, int channel, const uint16_t *data, uint32_t len)
{
	uint32_t words = len >> 1;
	uint16_t cmd_reg;
	uint16_t trigger;

	if (channel == 0) {
		volatile uint16_t *push = (volatile uint16_t *)(FUN_c0001a98(soc_irq_gate_midi_base_arg_fw) + 0x6e);

		(void)midi_hw_read16(base_sel, 0x70);	/* dummy status poll, return discarded - confirmed real */
		if (len == 0)
			return;

		for (; words != 0; words--)
			*push = *data++;

		if (len & 1) {
			*push = (uint16_t)(uint8_t)*data;
			midi_hw_write16(base_sel, 0x60, 0xa0);
			return;
		}
		cmd_reg = 0x60;
	} else {
		cmd_reg = (uint16_t)(((uint32_t)channel * 0x20 + 0x68) & soc_irq_gate_midi_cmd_align_mask);

		if (len != 0) {
			volatile uint16_t *push = (volatile uint16_t *)
				(FUN_c0001a98(soc_irq_gate_midi_base_arg_fw) + (uint32_t)channel * 0x20 + 0x72);

			for (; words != 0; words--)
				*push = *data++;

			if (len & 1) {
				*push = (uint16_t)(uint8_t)*data;
				midi_hw_write16(base_sel, 0x60, 0xa0);
				return;
			}
		}
	}

	trigger = 0x80;
	midi_hw_write16(base_sel, cmd_reg, trigger);
}

/* midi_hw_fifo_read - inverse of midi_hw_fifo_write: reads `len` bytes
 * (as `len/2` 16-bit words unpacked little-endian-byte-order plus an
 * optional trailing byte) OUT of channel `channel`'s FIFO POP register
 * (same fixed-address-per-call, non-incrementing source register as the
 * write side) into a real, incrementing destination byte buffer. The word
 * count is computed as `(len << 16) >> 17` in the raw decompile - faithful
 * transcription below; equivalent to `len >> 1` for len < 0x10000 (the
 * realistic range for this transfer-size field), NOT a general-purpose
 * unsigned divide (loses len's own top 15 bits at the left shift for
 * larger values - transcribed as-is, not "corrected" to a plain shift).
 * Returns `len & 0xffff` (0 if len itself was 0 - the real decompile's own
 * `uVar4 = 0` initializer, never touched on the len==0 path). @0xc0000dec.
 */
extern uint32_t soc_irq_gate_midi_base_arg_fr;	/* DAT_c0000e80, table+0x68 = 0xC00E0068 - dead phantom handle */

uint32_t midi_hw_fifo_read(uint32_t base_sel, int channel, uint8_t *out, uint32_t len)
{
	volatile uint16_t *pop;
	uint32_t ret = 0;
	uint32_t words;

	(void)base_sel;

	if (channel == 0)
		pop = (volatile uint16_t *)(FUN_c0001a98(soc_irq_gate_midi_base_arg_fr) + 0x6e);
	else
		pop = (volatile uint16_t *)(FUN_c0001a98(soc_irq_gate_midi_base_arg_fr) + (uint32_t)channel * 0x20 + 0x72);

	if (len == 0)
		return ret;

	ret = len & 0xffff;
	words = (uint32_t)(((uint64_t)len << 16) >> 17);	/* faithful transcription, see note above */

	for (; words != 0; words--) {
		uint16_t v = *pop;
		out[1] = (uint8_t)(v >> 8);
		out[0] = (uint8_t)v;
		out += 2;
	}
	if (len & 1)
		out[0] = (uint8_t)*pop;

	return ret;
}

/* ===========================================================================
 * CLUSTER 11 - USB endpoint FIFO fill-level/ring-index helper (0xc0000484),
 * plus a small family of trivial single-word accessors in the SAME address
 * neighborhood and caller cluster (0xc000e000-0xc000f800), not independently
 * confirmed to belong to the SAME struct but presented together per shared
 * address/caller-cluster evidence.
 *
 * usbdc_midi_status_glue.c's own PART B section ALREADY decompiled this
 * function's body as supporting evidence for usbdc_ep_regblock_ptr_a's own
 * real-world usage ("Confirmed via a real caller (FUN_c0000484, decompiled
 * directly this pass)") but explicitly does NOT define it there - the
 * quoted decompile in that file's comment is reproduced faithfully here as
 * this function's real, owned reconstruction. `scaled_ratio` (FUN_c001e300)
 * is already named/cited (not defined) in eva_crt0_tick_glue.c as "a
 * general rational-scale helper with power-of-two [fast-path]... not
 * attributed" - used here via extern, not redefined.
 * =========================================================================== */

extern uint32_t usbdc_ep_regblock_ptr_a(uint32_t unused, int index);	/* FUN_c0007108, usbdc_midi_status_glue.c */
extern uint32_t scaled_ratio(uint32_t numerator, uint32_t denominator);	/* FUN_c001e300, general helper, not attributed (eva_crt0_tick_glue.c) */

/* soc_irq_gate_usbdc_ring_index_update - `commit` (param_1) selects
 * whether to actually recompute (false: just returns the clamped-to-zero
 * current index). On commit, captures a hardware fill-level word (cached
 * handle's own +0x244 field) into table+0x2c, computes a byte delta
 * against usbdc_ep_regblock_ptr_a(_, 0)'s own address, divides that delta
 * by the SAME 0xc0-byte stride usbdc_ep_regblock_ptr_a itself uses (via
 * scaled_ratio) to get a slot count, clamps out-of-range results (>0x1f,
 * i.e. more than 32 slots) to 0, wraps negative/high results into a 0-31
 * ring window (`+0x20` / `-0x20` adjustments - a 32-slot modulo ring,
 * matching the `& 0x1f`-shaped bound elsewhere), and stores both an
 * absolute index (table+0x30) and a wrapped delta-from-clamped-start value
 * (table+0x28). Transcribed exactly per the raw decompile
 * usbdc_midi_status_glue.c already quoted - not simplified further; the
 * exact real-hardware meaning of the +0x244 fill-level field and which USB
 * endpoint this ring belongs to are NOT independently confirmed here.
 * @0xc0000484. */
extern uint32_t  soc_irq_gate_ep_ring_reloc_arg;	/* DAT_c0000568, resolved 0xC01CAC00 */
extern int32_t  *soc_irq_gate_ep_ring_fill_level;	/* DAT_c0000564, table+0x2c = 0xC00E002C */
extern int32_t  *soc_irq_gate_ep_ring_index;	/* DAT_c000055c, resolved 0xC0098F5C - OUTSIDE the 0xC00E0000 table, a distinct data-segment word */
extern void     *soc_irq_gate_ep_ring_hwbase;	/* DAT_c0000560, table+0x18 = 0xC00E0018 - cached hw handle, its own +0x244 field read */
extern int32_t  *soc_irq_gate_ep_ring_delta_out;	/* DAT_c000056c, table+0x28 = 0xC00E0028 */

int32_t soc_irq_gate_usbdc_ring_index_update(char commit)	/* FUN_c0000484 */
{
	int32_t idx = *soc_irq_gate_ep_ring_index;
	int32_t clamped = (idx < 0) ? 0 : idx;
	int32_t result = clamped;

	if (commit != 0) {
		if (idx >= 0) {
			int32_t fill_level = *(int32_t *)((uint8_t *)soc_irq_gate_ep_ring_hwbase + 0x244);
			uint32_t base_addr = usbdc_ep_regblock_ptr_a(soc_irq_gate_ep_ring_reloc_arg, 0);

			*soc_irq_gate_ep_ring_fill_level = fill_level;

			if ((uint32_t)*soc_irq_gate_ep_ring_fill_level < base_addr) {
				*soc_irq_gate_ep_ring_fill_level = 0;
			} else {
				int32_t byte_delta;
				int32_t slot;

				base_addr = usbdc_ep_regblock_ptr_a(soc_irq_gate_ep_ring_reloc_arg, 0);
				byte_delta = *soc_irq_gate_ep_ring_fill_level - (int32_t)base_addr;
				*soc_irq_gate_ep_ring_fill_level = byte_delta;

				slot = (int32_t)scaled_ratio((uint32_t)byte_delta, 0xc0);
				if (slot > 0x1f)
					result = 0;
				*soc_irq_gate_ep_ring_index = slot;
				if (slot > 0x1f)
					*soc_irq_gate_ep_ring_index = result;

				if (*soc_irq_gate_ep_ring_index < 0)
					*soc_irq_gate_ep_ring_index += 0x20;

				{
					int32_t wrapped = *soc_irq_gate_ep_ring_index;
					int32_t adj = wrapped;

					if (wrapped < clamped)
						adj = wrapped - clamped + 0x20;

					if (wrapped < clamped)
						*soc_irq_gate_ep_ring_delta_out = adj;
					else
						*soc_irq_gate_ep_ring_delta_out = wrapped - clamped;
				}
			}
		}
		if (result < 0)
			result += 0x20;
	}

	return result;
}

/* soc_irq_gate_slot0x1c_get / soc_irq_gate_slot0x20_get / soc_irq_gate_
 * slot0x24_clear / soc_irq_gate_slot0x24_get - four trivial single-word
 * accessor/mutator stubs in this same address neighborhood, called from
 * the SAME out-of-range caller cluster (0xc000e000-0xc000f800,
 * FUN_c000f230/_efa8/_f174/_f69c/_eff0/_f59c/_6578 - none reconstructed by
 * any file so far) as the ring-index function above. table+0x24 (0xC00E
 * 0024) is confirmed the SAME address for both the clear and get variants
 * below (both DAT_c00005a0 and DAT_c00005b0 resolve identically) - a real
 * write/read accessor pair for one shared flag word, both called from the
 * same caller (FUN_c0006578). NOT confirmed to be fields of the SAME
 * struct as the ring-index function's own table+0x28/0x2c slots above -
 * presented together only for address-cluster/caller-cluster proximity,
 * per this project's own "additive, not force-fit" convention. @0xc0000570
 * / @0xc0000580 / @0xc0000590 / @0xc00005a4. */
extern uint32_t *soc_irq_gate_slot_0x1c;	/* DAT_c000057c, table+0x1c = 0xC00E001C */
uint32_t soc_irq_gate_slot0x1c_get(void)	/* FUN_c0000570 */
{
	return *soc_irq_gate_slot_0x1c;
}

extern uint32_t *soc_irq_gate_slot_0x20;	/* DAT_c000058c, table+0x20 = 0xC00E0020 */
uint32_t soc_irq_gate_slot0x20_get(void)	/* FUN_c0000580 */
{
	return *soc_irq_gate_slot_0x20;
}

extern uint32_t *soc_irq_gate_slot_0x24;	/* DAT_c00005a0, table+0x24 = 0xC00E0024 */
void soc_irq_gate_slot0x24_clear(void)	/* FUN_c0000590 */
{
	*soc_irq_gate_slot_0x24 = 0;
}

extern uint32_t *soc_irq_gate_slot_0x24_r;	/* DAT_c00005b0, table+0x24 = 0xC00E0024 - SAME address as soc_irq_gate_slot_0x24 above, confirmed via query_dump.py dat */
uint32_t soc_irq_gate_slot0x24_get(void)	/* FUN_c00005a4 */
{
	return *soc_irq_gate_slot_0x24_r;
}

/* soc_irq_gate_ring3_state_reset - resets a THIRD, separate 3-word state
 * block (table+0x40... no: resolved addresses are 0xC0098F5C-family, i.e.
 * OUTSIDE the 0xC00E0000 table entirely - same distinct data word family
 * as soc_irq_gate_ep_ring_index above) to sentinel values -1/0/0 (empty
 * ring: read=-1, write=0, count=0 - a plausible ring-buffer init, not
 * independently confirmed). Sole call site (0xc000eec0) has NO containing
 * function in the static dump (`from_func: null`) - the SAME "referenced
 * from a table/init-list, not a real CALL instruction" pattern this
 * project has already flagged elsewhere (e.g. usbdc_midi_status_glue.c's
 * own 3-entry gap note) - NOT confirmed to be genuinely unreachable, just
 * not reachable via a traced CALL. @0xc00005b4. */
extern int32_t *soc_irq_gate_ring3_read_idx;	/* DAT_c00005d8, resolved 0xC0098F5C */
extern int32_t *soc_irq_gate_ring3_write_idx;	/* DAT_c00005dc, table+0x1c = 0xC00E001C - SAME slot as soc_irq_gate_slot_0x1c above (shared) */
extern int32_t *soc_irq_gate_ring3_count;	/* DAT_c00005e0, table+0x20 = 0xC00E0020 - SAME slot as soc_irq_gate_slot_0x20 above (shared) */

void soc_irq_gate_ring3_state_reset(void)	/* FUN_c00005b4 */
{
	*soc_irq_gate_ring3_read_idx = 0xffffffff;
	*soc_irq_gate_ring3_write_idx = 0;
	*soc_irq_gate_ring3_count = 0;
}

/* soc_irq_gate_slot0x00_get - trivial accessor over table+0x00 (0xC00E
 * 0000, the very first word of the table), sole caller FUN_c0008b64
 * (master_dispatch_tick, wire_dispatch.c). @0xc0000140. */
extern uint32_t *soc_irq_gate_slot_0x00;	/* DAT_c000014c, table+0x00 = 0xC00E0000 */
uint32_t soc_irq_gate_slot0x00_get(void)	/* FUN_c0000140 */
{
	return *soc_irq_gate_slot_0x00;
}

/* -------------------------------------------------------------------------
 * STILL OPEN
 * -------------------------------------------------------------------------
 *  - The exact peripheral identity behind: soc_irq_gate_timer0_quiesce's
 *    own +0x44 handle (table+0x08); soc_irq_gate_ch0b_quiesce's own
 *    +0x2068/+0x2070/+0x2078 register block (channel 11, disable side -
 *    NOT confirmed to be the same peripheral as the McASP-clock-param
 *    enable side despite the shared channel number); channels 0x36 (54)
 *    and 0x2a (42) generally (no cross-file callee gives either away).
 *  - Whether task_sched.c's FUN_c001ca34/"group A" and FUN_c001ca84/
 *    "group B" genuinely run back-to-back at boot (enabling then
 *    immediately disabling most of the same channels) or whether
 *    eva_board_crt0_tcb_and_kobj_init's own table slots REGISTER these
 *    functions into kernel objects for later event-driven dispatch rather
 *    than calling them inline - the latter is more plausible given
 *    wire_dispatch.c's independent confirmation that group B's own caller
 *    (FUN_c0008a5c/eva_link_status_change) is ALSO invoked live from
 *    master_dispatch_tick on a runtime event, but this is not independently
 *    confirmed against task_sched.c's own registration primitives (no
 *    disassembly access this pass).
 *  - soc_irq_gate_gap_slot_bringup's own final "detect bit" (table+0x58) -
 *    real physical meaning (cable/board presence? something else?) not
 *    decoded; FUN_c0001a00's cross-file naming split (eva_board_main.c's
 *    eva_board_probe_bus_handle vs. cpsoc.c's cpsoc_get_scan_handle) is
 *    pre-existing and not resolved here.
 *  - soc_irq_gate_mcasp2_bringup's own FUN_c00019e0/FUN_c00019e8 selectors
 *    - McASP-instance-shaped by analogy only, not independently confirmed;
 *    FUN_c0001e38 (called once, no other file names it) left as a bare
 *    extern with zero attribution.
 *  - soc_irq_gate_usbdc_ring_index_update's own +0x244 fill-level field and
 *    which real USB endpoint this ring tracks - not identified.
 *  - The four trivial slot accessors (cluster 11's tail) and
 *    soc_irq_gate_ring3_state_reset's own real callers (FUN_c000f230/
 *    _efa8/_f174/_f69c/_eff0/_f59c/_6578, and the "from_func: null" site
 *    at 0xc000eec0) are ALL out of every file's confirmed range so far -
 *    a genuine, unclaimed 0xc000e000-0xc000f800 neighborhood, same
 *    observation usbdc_midi_status_glue.c/midi_engine.c already made about
 *    their own adjacent gaps. NOT swept by this pass.
 * ------------------------------------------------------------------------- */
