/* SPDX-License-Identifier: GPL-2.0 */
/*
 * eva_crt0_tick_glue.c - reconstruction of the address range
 * 0xc0005438-0xc0005d34 (19 real functions per `range c0005438 c0005d34`;
 * 17 reconstructed here, 2 already fully reconstructed elsewhere - see
 * "ALREADY COVERED" below).
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, read from the
 * pre-fetched dump (all_decompiled.json/all_data.json), 2026-07-18. No live
 * Ghidra MCP calls this pass (bridge flagged concurrency-unsafe this round;
 * get_disassembly was unavailable, so two functions below - irq_save_and_
 * disable/irq_restore - are reconstructed from decompiler-artifact +
 * call-site evidence, not verbatim raw disassembly; flagged explicitly at
 * their own definitions).
 *
 * ANCHOR: NO single "../<Name>.cpp" string covers this whole range. A
 * `strings ".cpp"` sweep of the full dump found only ONE hit anywhere near
 * these addresses: "../cobjectmgr.cpp" @0xc0022dcc, referenced by exactly
 * ONE function in this range (FUN_c0005cc4, confirmed below). Every other
 * function here is genuinely unanchored - this is a real "glue" address
 * range straddling several subsystems' own un-anchored connective tissue,
 * the same situation aintc.c/heap_alloc.c/panelbus_dispatch.c already
 * documented for their own ranges. Grouped here under one file rather than
 * split, per this project's own precedent for small heterogeneous ranges
 * (see omap_l137_addr_gap_misc.c).
 *
 * ALREADY COVERED (not duplicated here, per this project's no-collision
 * rule - see each file's own reconstruction):
 *   - FUN_c0005a1c = cad_calibration_progress_pump, full body in
 *     omap_l108.c.
 *   - FUN_c0005c50 = cobjectmgr_notify_host, full body in cobjectmgr.c.
 *
 * REAL, CONCRETE FINDINGS THIS PASS:
 *   - eva_board_crt0 (FUN_c00055b8) itself was, until now, discussed at
 *     length in eva_board_main.c's own header comment (raw-disassembly
 *     excerpt, callee list, "never returns" analysis) but never actually
 *     DEFINED as a C function anywhere in the project - eva_board_main.c
 *     only carries prose about it. Defined for real below, along with its
 *     first callee FUN_c0005530 (also previously undefined).
 *   - eva_board_init_table_entry_0_zero (FUN_c0005720) was only ever an
 *     `extern` declaration in eva_board_main.c (no body anywhere). Defined
 *     for real below, matching that file's own declared signature exactly.
 *   - eva_wire_reg_low_dispatch (FUN_c0005738), eva_tick_unk_2/_4/_6
 *     (FUN_c000594c/_c000590c/_c0005cc4) were only ever `extern`
 *     declarations in wire_dispatch.c (phantom-declared from that file's
 *     own master-dispatch-tick bit-fanout, never given bodies). All four
 *     defined for real below, matching wire_dispatch.c's own declared
 *     names/signatures.
 *   - FUN_c0005b14 (wire_dispatch.c's `eva_tick_unk_3`) is, per
 *     ctouchpanel.c's own citation, "the firmware's touch-event consumer
 *     loop" - confirmed and defined below under a real name,
 *     `ctouchpanel_event_consumer_tick`. This is a DELIBERATE naming
 *     divergence from wire_dispatch.c's `eva_tick_unk_3` extern - flagged
 *     here, not silently reconciled, per this project's own established
 *     convention for such cases (see README's own "known cross-file
 *     discrepancies" section for the FUN_c00140d4 precedent).
 *   - irq_save_and_disable/irq_restore (FUN_c0005500/FUN_c0005510) are
 *     used as a critical-section pair at 21+19 call sites project-wide
 *     (already named and `extern`-declared identically in crypto_at88.c,
 *     cpsoc.c, panelbus_dispatch.c, midi_engine.c, chan_param_ctrl.c) but
 *     NEVER DEFINED anywhere - this is their one real definition,
 *     reconstructed from the standard ARM926 bare-metal CPSR IRQ-disable/
 *     restore idiom (see the functions' own comments for the confidence
 *     basis).
 * ============================================================================
 */

#include <stdint.h>

/* ===========================================================================
 * SECTION 1 - shared IRQ save/restore primitives.
 *
 * FUN_c0005500 (16 bytes) and FUN_c0005510 (24 bytes) both decompile to the
 * SAME nonsensical body:
 *
 *     return (uint)(byte)(in_NG<<4|in_ZR<<3|in_CY<<2|in_OV<<1|in_Q) << 0x1b;
 *
 * This is a well-known Ghidra decompiler artifact for a raw `MRS Rd,CPSR` /
 * `MSR CPSR_c,Rd` sequence it can't model as a named intrinsic: it falls
 * back to describing the NZCVQ condition-flag bits it CAN see as pseudo-
 * variables (in_NG/in_ZR/in_CY/in_OV/in_Q) and reassembles them into the
 * position they occupy in a real CPSR word (bits 27-31) - i.e. it is
 * literally rendering "whatever ends up in the flags-relevant top byte of
 * a CPSR read" without understanding the surrounding MRS/ORR/MSR sequence
 * that produced it. Both functions render IDENTICALLY because Ghidra's
 * fallback has no way to distinguish "read CPSR and mask in the I-bit" from
 * "read CPSR and mask it back out" - both look like "read some flag bits"
 * to it.
 *
 * Confidence basis for the reconstruction below (get_disassembly was not
 * available this pass - this is NOT a verbatim citation of raw bytes):
 *   1. Size match: 16 bytes = 4 ARM instructions (MRS, ORR, MSR, BX/MOV PC)
 *      for the save+disable side; 24 bytes = 6 instructions (MRS, BIC, AND,
 *      ORR, MSR, BX/MOV PC) for the restore side - exactly the standard
 *      ARM926 bare-metal "disable IRQ / restore prior I-bit" idiom shape.
 *   2. Call-site evidence: chan_param_ctrl.c's own decompile (@0xc0010bf0
 *      neighborhood) already noted "FUN_c0005500()'s return value (still
 *      live in r0) is implicitly forwarded to FUN_c0005510() with no
 *      intervening [modification]" - i.e. the real ABI is exactly
 *      "save() returns an opaque token; restore(token) undoes it",
 *      matching a saved-CPSR-word critical-section pair, not two
 *      independent flag reads.
 *   3. Every one of the 40 real call sites (21 + 19, spread across
 *      crypto_at88.c/cpsoc.c/panelbus_dispatch.c/midi_engine.c/
 *      chan_param_ctrl.c/this file) already uses these exact two names
 *      with this exact save/restore shape - this file supplies the one
 *      missing piece (their real bodies), it does not invent the API.
 *
 * The precise IRQ-bit mask (0x80, PSR bit 7) is the standard ARM I-bit
 * position, not independently re-derived from raw bytes this pass -
 * flagged, not asserted as a byte-exact disassembly match.
 * =========================================================================== */

int irq_save_and_disable(void)		/* FUN_c0005500, @0xc0005500, size 16 */
{
	int cpsr;

	__asm__ __volatile__(
		"mrs %0, cpsr\n\t"
		"orr r3, %0, #0x80\n\t"
		"msr cpsr_c, r3\n\t"
		: "=r" (cpsr)
		:
		: "r3", "memory", "cc");

	return cpsr;
}

void irq_restore(int flags)		/* FUN_c0005510, @0xc0005510, size 24 */
{
	__asm__ __volatile__(
		"mrs r3, cpsr\n\t"
		"bic r3, r3, #0x80\n\t"
		"and %0, %0, #0x80\n\t"
		"orr r3, r3, %0\n\t"
		"msr cpsr_c, r3\n\t"
		:
		: "r" (flags)
		: "r3", "memory", "cc");
}

/* ===========================================================================
 * SECTION 2 - eva_board_crt0 and its first callee.
 *
 * eva_board_crt0 (FUN_c00055b8) - discussed at length in eva_board_main.c's
 * own header (raw-disassembly excerpt @0xc0009534-0xc000954c, the eleven-
 * callee list, the "never returns, falls into the scheduler idle loop"
 * conclusion) but never itself given a body there. Defined for real here,
 * matching that file's own citation exactly: zero a BSS-style region
 * (DAT_c0005600..DAT_c0005604), call FUN_c0005530 (below), then ten more
 * subsystem bring-up calls (none individually traced by this project yet -
 * eva_board_main.c's own note stands: "address-adjacent to
 * eva_board_start_task's own scheduler primitives... not to any already-
 * reconstructed peripheral driver", except FUN_c0001c84 which aintc.c has
 * SINCE identified as aintc_init), then the scheduler idle/dispatch tail
 * (poll-next-ready-task / WFI / indirect-call-through-TCB) that
 * eva_board_main.c's own eva_board_start_task section separately confirms
 * is the SAME tail FUN_c001d850 uses. @0xc00055b8.
 * =========================================================================== */
extern void eva_bss_zero_region_start;			/* DAT_c0005600, .bss-style start pointer */
extern void eva_bss_zero_region_end;			/* DAT_c0005604 */
extern void eva_board_crt0_task_tables_init(void);	/* FUN_c0005530, defined below */
extern void aintc_init(void *handle);			/* FUN_c0001c84, full body in aintc.c */
extern void *eva_board_crt0_scheduler_ready_flag;	/* *DAT_c001cb28 */
extern void *eva_board_crt0_sched_next_task_word;	/* DAT_c001d9cc - "next ready task" global, cleared then polled */
extern void *eva_board_crt0_sched_ready_src;		/* DAT_c001d9d0 - source read into the above */
extern void *eva_board_crt0_sched_active_word;		/* DAT_c001d9c8 - mirrors the ready-task word while running */
extern void coproc_moveto_Wait_for_interrupt(int mode);	/* ARM WFI intrinsic wrapper, already used firmware-wide */

/* The other nine crt0 callees - none individually traced by any file yet
 * (eva_board_main.c's own note; not re-asserted with more confidence here).
 * Left as opaque calls, matching that file's own citation list verbatim. */
extern void FUN_c0009188(void *arg);
extern void FUN_c0000a20(void);
extern void FUN_c00018e4(void *arg);	/* aintc_base, aintc.c names it but doesn't extern it under this exact signature */
extern void FUN_c001cc2c(void);
extern void FUN_c001cba8(void);
extern void FUN_c001cba4(void);
extern void FUN_c001ce40(void);
extern void FUN_c001cad4(void);
extern void FUN_c001ca34(void);

void eva_board_crt0(void)		/* FUN_c00055b8, @0xc00055b8 */
{
	uint32_t *p;

	for (p = (uint32_t *)&eva_bss_zero_region_start;
	     p < (uint32_t *)&eva_bss_zero_region_end; p++)
		*p = 0;

	eva_board_crt0_task_tables_init();
	FUN_c0009188(0 /* DAT_c0005608, real arg not resolved to a concrete value */);
	FUN_c0000a20();
	FUN_c00018e4(0 /* DAT_c000560c */);
	aintc_init(0 /* argument elided by Ghidra - aintc.c's own aintc_init takes a handle; call-site value not resolved */);
	FUN_c001cc2c();
	FUN_c001cba8();
	FUN_c001cba4();
	FUN_c001ce40();
	FUN_c001cad4();
	FUN_c001ca34();

	*(int *)&eva_board_crt0_scheduler_ready_flag = 1;
	*(int *)&eva_board_crt0_sched_next_task_word = 0;

	for (;;) {
		int next = *(int *)&eva_board_crt0_sched_ready_src;
		*(int *)&eva_board_crt0_sched_active_word = next;
		if (next != 0)
			break;
		*(int *)&eva_board_crt0_sched_next_task_word = 1;
		coproc_moveto_Wait_for_interrupt(1);
		*(int *)&eva_board_crt0_sched_next_task_word = 0;
	}

	/* indirect call through the ready task's own +0x1c function-pointer
	 * field - Ghidra could not recover a jump table here (single dynamic
	 * target, not a switch); matches eva_board_main.c's own description
	 * of this exact tail. */
	{
		int next = *(int *)&eva_board_crt0_sched_active_word;
		void (*entry)(void) = *(void (**)(void))((uint8_t *)next + 0x1c);
		entry();
	}
}

/* eva_board_crt0_task_tables_init (FUN_c0005530) - eva_board_crt0's first
 * callee: fills [DAT_c00055a4, DAT_c00055a8) with the pattern 0xAAAAAAAA
 * (classic "stack/region painting" fill, used elsewhere in embedded
 * firmware to later measure high-water-mark usage - NOT confirmed here to
 * be stack-specific, just structurally identical to that idiom), then
 * fills two further record arrays - one whose count/base live at
 * DAT_c00055b0+0x10/+0x14, one at the SAME base+0x30/+0x34 - with the
 * pattern 0x11111111. Two visually distinct fill patterns for the "paint"
 * region vs the two record arrays is real, structurally confirmed evidence
 * these are different memory objects, not one contiguous region reusing a
 * loop. Sole caller: eva_board_crt0. @0xc0005530. */
extern uint32_t *eva_paint_region_start;	/* DAT_c00055a4 */
extern uint32_t *eva_paint_region_end;		/* DAT_c00055a8 */
extern void *eva_task_table_base;		/* DAT_c00055b0 - base object with count/ptr pairs at +0x10/+0x14 and +0x30/+0x34 */

void eva_board_crt0_task_tables_init(void)	/* FUN_c0005530, @0xc0005530 */
{
	uint32_t *p;
	uint8_t *base = (uint8_t *)eva_task_table_base;
	uint32_t count1, count2;
	uint32_t *arr1, *arr2;
	uint32_t i;

	for (p = eva_paint_region_start; p < eva_paint_region_end; p++)
		*p = 0xAAAAAAAAu;

	count1 = *(uint32_t *)(base + 0x10);
	arr1   = *(uint32_t **)(base + 0x14);
	for (i = 0; i < (count1 >> 2); i++)
		arr1[i] = 0x11111111u;

	count2 = *(uint32_t *)(base + 0x30);
	arr2   = *(uint32_t **)(base + 0x34);
	for (i = 0; i < (count2 >> 2); i++)
		arr2[i] = 0x11111111u;
}

/* ===========================================================================
 * SECTION 3 - eva_board_init_table_entry_0_zero.
 *
 * eva_board_main.c's own `eva_board_init_table_entry_0` (FUN_c0009168)
 * already declares this exact `extern void eva_board_init_table_entry_0_
 * zero(uint8_t *flag);` and describes it as "just zeroes one byte at a
 * fixed pointer" - the real decompile confirms that description exactly.
 * Defined here for real, matching that file's declared signature. @0xc0005720.
 * =========================================================================== */
void eva_board_init_table_entry_0_zero(uint8_t *flag)	/* FUN_c0005720, @0xc0005720 */
{
	*flag = 0;
}

/* ===========================================================================
 * SECTION 4 - eva_wire_reg_low_dispatch (FUN_c0005738).
 *
 * wire_dispatch.c already `extern`-declares this exact name/signature
 * (`void eva_wire_reg_low_dispatch(void *handle, uint8_t value)`) as the
 * reg<0x10, op-byte-0 case of its own master command dispatcher
 * (FUN_c0007d1c), but never defines it. Defined here for real.
 *
 * REAL STRUCTURE (cross-file confirmed this pass):
 *   - bit 4 (0x10) of `value` toggles a flag word via FUN_c0012d70 (OR) /
 *     FUN_c0012d58 (AND-NOT) on a CAD-engine-adjacent handle
 *     (DAT_c00057d0/DAT_c00057d4, 8 bytes apart from FUN_c000594c's own
 *     CAD-pedal handle DAT_c0005a10 below - same object family, not proven
 *     identical).
 *   - bit 3 (0x08) toggles between cad.c's OWN already-named
 *     cad_pedal_object_probe (FUN_c0013368) and cad_pedal_object_reset
 *     (FUN_c001335c) - confirmed via cad.c's own reconstruction, real
 *     cross-file evidence this function drives cad.c's pedal object.
 *   - bit 6 (0x40) toggles between ctouchpanel.c's OWN already-named
 *     ctouchpanel_cad_channels_enable (FUN_c0014288) / _disable
 *     (FUN_c00142e0) - per ctouchpanel.c's own citation, THIS function
 *     ("a broader multi-subsystem mode-toggle function, itself called from
 *     [the] central dispatcher") is the confirmed caller.
 *   - bit 1 (0x02) sweeps CAD engine slots 0-37 (0x25), and for every slot
 *     NOT in the fixed set {5, 0xd, 0x15, 0x1d} (a stride-8 arithmetic
 *     sequence - the "reference/ground" slot of each 8-channel bank, per
 *     cad.c's own FUN_c0013910 membership test), calls cad.c's OWN
 *     cad_calibration_unmask_slot (FUN_c00138b8) or cad_calibration_
 *     mask_slot (FUN_c00138e4) depending on the SAME bit-4 sense used
 *     above - i.e. bit 4 selects "unmask sweep" vs "mask sweep" for the
 *     whole 38-slot CAD engine in one pass, while bit 1 gates whether the
 *     sweep runs at all.
 *
 * Net effect: a single wire-protocol byte that reconfigures the CAD
 * calibration engine's pedal object and channel masking in one shot -
 * genuinely CAD-engine-owned in function, even though it is not
 * physically inside cad.c's own confirmed "../cad.cpp" anchor range.
 * cad.c is NOT edited here, per this project's own convention against
 * editing a file with a confirmed anchor from an unrelated pass - this
 * is purely additive, cross-referencing cad.c's/ctouchpanel.c's own
 * already-named functions. @0xc0005738.
 * =========================================================================== */
extern void cad_engine_flag_set(void *cad, uint16_t bit);	/* FUN_c0012d70, name is descriptive, not confirmed */
extern void cad_engine_flag_clear(void *cad, uint16_t bit);	/* FUN_c0012d58 */
extern void cad_pedal_object_probe(void *obj);			/* FUN_c0013368, cad.c */
extern void cad_pedal_object_reset(void *obj);			/* FUN_c001335c, cad.c */
extern void ctouchpanel_cad_channels_enable(void *cad);	/* FUN_c0014288, ctouchpanel.c */
extern void ctouchpanel_cad_channels_disable(void *cad);	/* FUN_c00142e0, ctouchpanel.c */
extern int  cad_slot_is_bank_reference(void *cad, int slot);	/* FUN_c0013910, cad-adjacent membership test - {5,0xd,0x15,0x1d} */
extern void cad_calibration_unmask_slot(void *cad, int slot);	/* FUN_c00138b8, cad.c (cited via ctouchpanel.c) */
extern void cad_calibration_mask_slot(void *cad, int slot);	/* FUN_c00138e4, cad.c (cited via ctouchpanel.c) */
extern void *eva_wire_cad_flag_handle_a;	/* DAT_c00057d0 */
extern void *eva_wire_cad_flag_handle_b;	/* DAT_c00057d4 */
extern void *eva_wire_cad_pedal_handle;	/* DAT_c00057d8 */

void eva_wire_reg_low_dispatch(void *handle, uint8_t value)	/* FUN_c0005738, @0xc0005738 */
{
	(void)handle;	/* not read in the real body - matches this project's own
			 * established "phantom forwarded parameter" pattern */

	if ((value & 0x10) == 0)
		cad_engine_flag_clear(eva_wire_cad_flag_handle_a, 1);
	else
		cad_engine_flag_set(eva_wire_cad_flag_handle_a, 1);

	if ((value >> 3 & 1) == 0)
		cad_pedal_object_reset(eva_wire_cad_flag_handle_b);
	else
		cad_pedal_object_probe(eva_wire_cad_flag_handle_b);

	if ((value >> 6 & 1) == 0)
		ctouchpanel_cad_channels_disable(eva_wire_cad_pedal_handle);
	else
		ctouchpanel_cad_channels_enable(eva_wire_cad_pedal_handle);

	if ((value >> 1 & 1) != 0) {
		for (int slot = 0; slot < 0x26; slot++) {
			if (!cad_slot_is_bank_reference(eva_wire_cad_pedal_handle, slot))
				cad_calibration_mask_slot(eva_wire_cad_pedal_handle, slot);
		}
		return;
	}
	for (int slot = 0; slot < 0x26; slot++) {
		if (!cad_slot_is_bank_reference(eva_wire_cad_pedal_handle, slot))
			cad_calibration_unmask_slot(eva_wire_cad_pedal_handle, slot);
	}
}

/* ===========================================================================
 * SECTION 5 - the host-notify wire-event family (extends cobjectmgr.c's own
 * "small family of host-notify event senders" beyond the three it named).
 *
 * cobjectmgr.c documents cobjectmgr_notify_host (FUN_c0005c50) as one of a
 * family "siblings FUN_c0005cc4/FUN_c0005d34, all three called from the
 * master dispatcher FUN_c0008b64 and all sharing the same output channel
 * constant, -0x3fe35340" but leaves FUN_c0005cc4 unreconstructed. Confirmed
 * and defined here (as `eva_tick_unk_6`, matching wire_dispatch.c's own
 * extern name for it), AND a genuinely NEW finding this pass: FUN_c000590c
 * (wire_dispatch.c's `eva_tick_unk_4`) shares the EXACT SAME output channel
 * constant (DAT_c0005948 = -0x3fe35340, byte-identical to cobjectmgr.c's
 * DAT_c0005cc0) - i.e. this is (at least) a FOUR-member family, not three.
 * cobjectmgr.c is not edited to reflect this (out of scope for this file's
 * own no-collision rule) - recorded here instead.
 * =========================================================================== */
extern void at88_usb_tx_submit(void *dest_channel, const void *buf, int len);	/* FUN_c000acec, full body in crypto_at88.c */
extern void *eva_notify_channel;	/* DAT_c0005948/DAT_c0005a18/DAT_c0005d30 - all resolve to the SAME
					 * address as cobjectmgr.c's own DAT_c0005cc0 (cobjectmgr_notify_channel) */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* full body in crypto_at88.c */

/* eva_tick_unk_4 (FUN_c000590c, wire_dispatch.c bit 0x10) - the simplest of
 * the family: sends a FIXED 4-byte wire event {0,0,0x66,0} with no incoming
 * state at all. @0xc000590c. */
void eva_tick_unk_4(void *handle)	/* FUN_c000590c, @0xc000590c */
{
	uint8_t wire[4] = { 0, 0, 0x66, 0 };

	(void)handle;	/* dead parameter - real body takes no arguments */

	at88_usb_tx_submit(eva_notify_channel, wire, 4);
}

/* eva_tick_unk_6 (FUN_c0005cc4, wire_dispatch.c bit 0x40) - takes a real
 * handle (unlike its siblings above): reads a mode word at handle+0x3c; if
 * it equals 2, hard-faults (crypto_at88_fault, line 0x368/872 of
 * "../cobjectmgr.cpp" - CONFIRMED via DAT_c0005d2c resolving to the exact
 * same 0xc0022dcc string address cobjectmgr.c's own anchor uses - this is
 * the ONE function in this whole range with a real ".cpp" anchor hit).
 * Otherwise sends a 4-byte wire event {0, (mode!=0), 0x71, 1}. @0xc0005cc4. */
void eva_tick_unk_6(void *handle)	/* FUN_c0005cc4, @0xc0005cc4 */
{
	uint8_t *h = (uint8_t *)handle;
	int mode = *(int *)(h + 0x3c);
	uint8_t wire[4];

	if (mode == 2)
		crypto_at88_fault(0, "../cobjectmgr.cpp", 0x368);

	wire[0] = 0;
	wire[1] = (mode != 0) ? 1 : 0;
	wire[2] = 0x71;
	wire[3] = 1;
	at88_usb_tx_submit(eva_notify_channel, wire, 4);
}

/* ===========================================================================
 * SECTION 6 - eva_tick_unk_2 (FUN_c000594c, wire_dispatch.c bit 0x2).
 *
 * REAL CROSS-FILE FINDING: the inner encode call, FUN_c00133f8, is ALREADY
 * declared (prototype only, no body) in cad.c as `cad_pedal_encode_step
 * (int16_t *obj, uint8_t out_pair[2])` - "converts an accumulated 16-bit
 * pedal position into a stream of (index, magnitude) byte pairs". This
 * confirms eva_tick_unk_2 is cad.c's pedal-position drain loop: for each
 * (index, magnitude) pair produced, sends a 4-byte wire event
 * {0, magnitude, index|0x50, 1} through the SAME host-notify channel as
 * Section 5, then (if a feature-gate bit test passes) applies `magnitude`
 * as a SIGNED delta to an accumulated level word, clamped at a floor of 0,
 * and drives it through FUN_c0015650 twice - once at the pre-update value,
 * once at the post-update value (an old->new ramp/transition call shape,
 * matching eva_tick_unk_3's own old->new interpolation below). FUN_c0015650
 * itself is not attributed to any subsystem this pass (used identically by
 * eva_tick_unk_3 in Section 7) - cited by address only. @0xc000594c.
 * =========================================================================== */
extern int cad_pedal_encode_step(int16_t *obj, uint8_t out_pair[2]);	/* FUN_c00133f8, cad.c - prototype only, no body there */
extern int board_feature_bit_test(void *handle);			/* FUN_c00094d8, not attributed to a subsystem */
extern void level_ramp_drive(int16_t value, int timeout_ms, void *channel, int flags);	/* FUN_c0015650, not attributed */
extern int16_t *eva_tick2_pedal_obj;		/* DAT_c0005a10 */
extern int32_t *eva_tick2_level_word;		/* DAT_c0005a14 */
extern void *eva_tick2_feature_handle;		/* DAT_c0005a04 */
extern void *eva_tick2_ramp_channel;		/* DAT_c0005a08 */
extern void *eva_tick2_ramp_channel_b;		/* DAT_c0005a0c */

void eva_tick_unk_2(void *handle)	/* FUN_c000594c, @0xc000594c */
{
	uint8_t pair[2];
	uint8_t wire[4];

	(void)handle;	/* dead parameter, matches wire_dispatch.c's own phantom-param note */

	while (cad_pedal_encode_step(eva_tick2_pedal_obj, pair)) {
		wire[0] = 0;
		wire[1] = pair[1];		/* magnitude, signed */
		wire[2] = pair[0] | 0x50;	/* index, tagged */
		wire[3] = 1;
		at88_usb_tx_submit(eva_notify_channel, wire, 4);

		if (board_feature_bit_test(eva_tick2_feature_handle)) {
			int32_t old = *eva_tick2_level_word;
			int32_t updated = old + (int8_t)pair[1];

			if (updated < 0)
				updated = 0;
			*eva_tick2_level_word = updated;

			level_ramp_drive((int16_t)old, 500, eva_tick2_ramp_channel, 0);
			level_ramp_drive((int16_t)updated, 500, eva_tick2_ramp_channel_b, 0);
		}
	}
}

/* ===========================================================================
 * SECTION 7 - ctouchpanel_event_consumer_tick (FUN_c0005b14).
 *
 * wire_dispatch.c `extern`-declares this as `eva_tick_unk_3` (bit 0x08
 * tail); given a real name here per ctouchpanel.c's OWN citation at its
 * ctouchpanel_pop_event (FUN_c0014d24) definition: "the real caller...
 * outside this file's range - the firmware's touch-event consumer loop,
 * drains this queue every master-dispatcher tick and relays each event to
 * the host through the SAME shared USB-submit primitive already
 * established for crypto_at88.c's AtmelRead event, cobjectmgr.c's
 * host-notify event, and cad.c's calibration-progress pump". This is a
 * DELIBERATE naming divergence from wire_dispatch.c's `eva_tick_unk_3` -
 * flagged, not silently reconciled (see this file's own header note).
 *
 * ctouchpanel_pop_event's real 4-byte output packs as {code, pad, a, b}
 * (confirmed via ctouchpanel.c's own decompile: one dword write to
 * *param_2). Loop body: builds wire event {b, a, code|0x10, 0} and sends
 * it; if a feature-gate test passes, code==1 stores (a,b) as a start
 * position, code==2 reads back the stored start position and drives an
 * interpolated line/cursor move from (start_a,start_b) to (a,b) via two
 * FUN_c00168fc+FUN_c0015650 pairs - the same "old position -> new
 * position" ramp shape Section 6 uses for the pedal level. Real, dense
 * cursor/position-tracking logic; FUN_c00168fc's own internal shape
 * (fixed 0x7fffffff/0x208/0xffff constants feeding FUN_c00169b0) is not
 * independently decoded this pass - cited by address only. @0xc0005b14.
 * =========================================================================== */
extern int ctouchpanel_pop_event(void *tp, uint32_t *out);	/* FUN_c0014d24, full body in ctouchpanel.c */
extern void cursor_move_step(void *a, void *b, uint32_t from_x, uint32_t from_y);	/* FUN_c00168fc, not attributed */
extern void *ctouchpanel_tick_handle;		/* DAT_c0005c48/DAT_c0005c24 - SAME address as ctouchpanel.c's own DAT_c0008498 per that file's citation */
extern void *ctouchpanel_tick_notify_channel;	/* DAT_c0005c4c - resolves to the SAME host-notify channel address as Section 5 */
extern void *ctouchpanel_tick_feature_handle;	/* DAT_c0005c24 */
extern uint32_t *ctouchpanel_tick_start_a;	/* *DAT_c0005c38 */
extern uint32_t *ctouchpanel_tick_start_b;	/* *DAT_c0005c3c */
extern uint32_t *ctouchpanel_tick_cur_a;	/* DAT_c0005c28 */
extern uint32_t *ctouchpanel_tick_cur_b;	/* DAT_c0005c2c */
extern void *ctouchpanel_tick_cursor_obj_a;	/* DAT_c0005c30 */
extern void *ctouchpanel_tick_cursor_obj_b;	/* DAT_c0005c34 */
extern void *ctouchpanel_tick_cursor_obj_c;	/* DAT_c0005c40 */
extern uint32_t ctouchpanel_tick_ramp_timeout;	/* DAT_c0005c44 - fixed value 0x23f (575) */

void ctouchpanel_event_consumer_tick(void *handle)	/* FUN_c0005b14, @0xc0005b14 - wire_dispatch.c's `eva_tick_unk_3` */
{
	uint32_t evt;
	uint8_t *e = (uint8_t *)&evt;	/* [0]=code, [1]=pad, [2]=a, [3]=b, per ctouchpanel.c's own confirmed layout */
	uint8_t wire[4];

	(void)handle;	/* dead parameter, matches wire_dispatch.c's own phantom-param note */

	while (ctouchpanel_pop_event(ctouchpanel_tick_handle, &evt)) {
		uint8_t code = e[0], a = e[2], b = e[3];

		wire[0] = b;
		wire[1] = a;
		wire[2] = code | 0x10;
		wire[3] = 0;
		at88_usb_tx_submit(ctouchpanel_tick_notify_channel, wire, 4);

		if (!board_feature_bit_test(ctouchpanel_tick_feature_handle))
			continue;

		if (code == 1) {
			*ctouchpanel_tick_start_a = a;
			*ctouchpanel_tick_start_b = b;
		}
		if (code == 2) {
			uint32_t start_a = *ctouchpanel_tick_start_a;
			uint32_t start_b = *ctouchpanel_tick_start_b;

			*ctouchpanel_tick_cur_a = a;
			*ctouchpanel_tick_cur_b = b;

			cursor_move_step(ctouchpanel_tick_cursor_obj_b, ctouchpanel_tick_cursor_obj_a,
					  start_a, start_b);
			level_ramp_drive(100, 0x230, ctouchpanel_tick_cursor_obj_b, 0);
			cursor_move_step(ctouchpanel_tick_cursor_obj_b, ctouchpanel_tick_cursor_obj_c,
					  *ctouchpanel_tick_cur_a, *ctouchpanel_tick_cur_b);
			level_ramp_drive(100, ctouchpanel_tick_ramp_timeout, ctouchpanel_tick_cursor_obj_b, 0);
		}
	}
}

/* ===========================================================================
 * SECTION 8 - still-unattributed miscellany (genuinely uncertain ownership,
 * kept generically named per this project's own "location label, not a
 * confirmed subsystem attribution" convention - see omap_l137_addr_gap_
 * misc.c's own FUN_c00032a8 for the precedent phrasing).
 * =========================================================================== */

/* FUN_c0005438 - sole caller FUN_c0000aa4, itself sole callee-of-interest
 * inside eva_link_status_change's (wire_dispatch.c's FUN_c0008a5c, bit
 * 0x100) own irq_save_and_disable/irq_restore-wrapped reset sequence -
 * real, address-confirmed evidence this is part of a link (USB/MIDI/panel
 * bus - not determined) reset/mode-select sequence, not proof of which
 * bus. Writes 3/3/1/[1]/2/[1] into 4 handle fields then spins on
 * `(*(handle+0x600) & 0x1f00) != 0` - a busy-wait for a hardware status
 * field to clear after the mode writes. Structurally a "select mode 1,
 * wait; select mode 2, wait; then wait for busy-clear" 2-phase hardware
 * sequence. @0xc0005438. */
void FUN_c0005438(int handle)		/* @0xc0005438, name is a location label, not a confirmed subsystem attribution */
{
	*(uint32_t *)((uint8_t *)handle + 0x2028) = 3;
	*(uint32_t *)((uint8_t *)handle + 0x2008) = 3;
	*(uint32_t *)((uint8_t *)handle + 0x2070) = 1;
	if (*(int *)((uint8_t *)handle + 0x2068) != 0)
		*(uint32_t *)((uint8_t *)handle + 0x2078) = 1;
	*(uint32_t *)((uint8_t *)handle + 0x2070) = 2;
	if (*(int *)((uint8_t *)handle + 0x2068) != 0)
		*(uint32_t *)((uint8_t *)handle + 0x2078) = 1;

	while ((*(uint32_t *)((uint8_t *)handle + 0x600) & 0x1f00) != 0)
		;	/* spin until hardware status field clears */
}

/* mcasp_clock_step_b - mcasp.c already documents this exact address as
 * "step_b (FUN_c00054a8) is a CONFIRMED empty stub - `{ return; }`" and
 * carries its own `extern void mcasp_clock_step_b(uint32_t base);`
 * declaration; that file deliberately left the body out of its own range
 * (mcasp.c's own "../MCU/Component/OmapL137Mcasp.cpp" anchor does not
 * cover 0xc00054a8). Defined here for real, matching mcasp.c's own name
 * and signature exactly - a genuine no-op at this firmware revision, size
 * 4 bytes (one `mov pc,lr`/`bx lr`). @0xc00054a8. */
void mcasp_clock_step_b(uint32_t base)		/* FUN_c00054a8, @0xc00054a8 */
{
	(void)base;	/* confirmed dead - real body is `{ return; }` */
}

/* FUN_c00054ac - sole caller FUN_c0000a20 (itself one of eva_board_crt0's
 * ten un-traced callees, Section 2 above). Zeroes 3 dword fields (+4/+8/
 * +0xc) and sets a fixed 16-bit field (+0x2a) to the constant 0x292 (658) -
 * a plausible default parameter value, meaning not determined. @0xc00054ac. */
void FUN_c00054ac(void *handle)	/* @0xc00054ac, name is a location label, not a confirmed subsystem attribution */
{
	uint8_t *h = (uint8_t *)handle;

	*(int16_t *)(h + 0x2a) = 0x292;
	*(uint32_t *)(h + 4) = 0;
	*(uint32_t *)(h + 8) = 0;
	*(uint32_t *)(h + 0xc) = 0;
}

/* FUN_c00054cc - "set percent-scaled value": writes a fixed range constant
 * (0x1d4c = 7500) into handle+8, then computes `percent * 7500 / 100`
 * (via FUN_c001e300, a general rational-scale helper with power-of-two
 * fast paths - not itself attributed to any subsystem) into handle+0xc.
 * Net effect: handle+0xc = percent*75, for percent in a caller-supplied
 * 0-100 range (both real callers, FUN_c0014f84 and FUN_c0007ccc, clamp
 * their own input to that range before calling). Plausible role: a
 * percent-to-PWM-duty or percent-to-brightness/volume scale setter; not
 * confirmed which peripheral. @0xc00054cc. */
extern uint32_t scaled_ratio(uint32_t numerator, uint32_t denominator);	/* FUN_c001e300, general helper, not attributed */

void FUN_c00054cc(void *handle, int percent)	/* @0xc00054cc, name is a location label, not a confirmed subsystem attribution */
{
	uint8_t *h = (uint8_t *)handle;

	*(uint32_t *)(h + 8) = 0x1d4c;
	*(uint32_t *)(h + 0xc) = scaled_ratio((uint32_t)percent * 0x1d4c, 100);
}

/* FUN_c00057dc - trivial global busy-flag setter (`*DAT_c00057e8 = value;`).
 * Both real callers (FUN_c000eee4, FUN_c000f0c8, both outside this range)
 * use it as a "mark busy, mutate state, mark free" bracket around a state
 * change - a lightweight non-reentrancy guard, not an IRQ-level primitive
 * (unlike Section 1's irq_save_and_disable/irq_restore). @0xc00057dc. */
extern uint8_t *eva_busy_flag;		/* DAT_c00057e8 */

void FUN_c00057dc(void *unused_param1, uint8_t value)	/* @0xc00057dc, name is a location label, not a confirmed subsystem attribution */
{
	(void)unused_param1;	/* dead - real body only ever touches the fixed DAT_ target, never param_1 */
	*eva_busy_flag = value;
}

/* midi_status_classify - MIDI running-status/message-length classifier.
 * Sole caller FUN_c0006e90, confirmed midi_engine.c territory (that file's
 * own citation: "Sole caller FUN_c0006e90, out of range" at its FUN_c000d0c0
 * neighborhood) - genuinely midi_engine.c-owned in FUNCTION even though
 * physically outside every confirmed anchor range, same situation as
 * Section 4's CAD dispatcher. midi_engine.c is NOT edited here.
 *
 * Pops one 4-byte raw MIDI record {status, d1, d2, rs_mode} from a queue
 * (FUN_c00109dc, not attributed to a subsystem - its own real name/body
 * are out of this range), then classifies `status` into the classic
 * MIDI message-length nibble (0-0xf): realtime bytes (>0xf7) -> 0xf;
 * channel voice/mode bytes (<0xf0, with the top bit set) -> status>>4;
 * a bare data byte with the top bit clear (running-status continuation)
 * -> 4; SysEx start (0xf0) -> 4; MTC quarter-frame/Song Select (0xf1/0xf3)
 * -> 2; Song Position Pointer (0xf2) -> 3; the ambiguous 0xf7-adjacent
 * case (any of the 3 leading bytes IS literally 0xf7) and the small
 * f4-f6 reserved gap both fall through to a shared default of 5. Writes
 * {class, status, d1, d2} to *out_class and returns 1, or 0 if the queue
 * was empty, or 1 (with *out_class untouched) if `*ready_flag == 0`.
 * @0xc00057ec. */
extern int midi_pop_raw_record(void *midi_queue, uint8_t out4[4]);	/* FUN_c00109dc, not attributed to a subsystem */
extern void *midi_classify_queue_handle;	/* DAT_c0005908 */

int midi_status_classify(char *ready_flag, uint8_t out_class[4])	/* FUN_c00057ec, @0xc00057ec */
{
	uint8_t rec[4];		/* [0]=status, [1]=d1, [2]=d2, [3]=rs_mode */
	uint8_t class_nibble;

	if (!midi_pop_raw_record(midi_classify_queue_handle, rec))
		return 0;
	if (*ready_flag == 0)
		return 1;

	if (rec[0] == 0xf7 || rec[1] == 0xf7 || rec[2] == 0xf7) {
		if ((char)rec[3] == 2)
			class_nibble = 6;
		else if ((char)rec[3] != 1)
			class_nibble = 7;
		else
			class_nibble = 5;	/* rs_mode==1: falls through to the shared default in the real body */
	} else if (rec[0] > 0xf7) {
		class_nibble = 0xf;
	} else if (rec[0] < 0xf0) {
		class_nibble = (rec[0] & 0x80) ? (rec[0] >> 4) : 4;
	} else if (rec[0] == 0xf0) {
		class_nibble = 4;
	} else if (rec[0] == 0xf1 || rec[0] == 0xf3) {
		class_nibble = 2;
	} else if (rec[0] == 0xf2) {
		class_nibble = 3;
	} else {
		class_nibble = 5;
	}

	out_class[0] = class_nibble;
	out_class[1] = rec[0];
	out_class[2] = rec[1];
	out_class[3] = rec[2];
	return 1;
}

/* ===========================================================================
 * SECTION 9 - FUN_c00056a4, genuinely uncertain ownership.
 *
 * ZERO callers found anywhere in the 691-function xref data - either dead
 * code, or (more likely, per the precedent already established for
 * eva_board_crt0's own reset-handler neighborhood and eva_board_init_
 * table's own literal-pool boundary issue) reached only through raw,
 * Ghidra-unbound code that this pass's static dump does not capture as a
 * caller edge. Circumstantial support for the latter: DAT_c0005718
 * (0xc0e00068) is the SAME constant eva_board_crt0 itself passes to
 * FUN_c00018e4 (Section 2, via DAT_c000560c) - real, address-confirmed
 * evidence this function is at least THEMATICALLY adjacent to the crt0
 * bring-up chain, not proof it is called from it.
 *
 * Real body: syscfg0_base_get + board_desc_set_pinmux_130_a (both
 * soc_periph.c, real bodies there) + gpio_bank_get_base +
 * gpio_pins_bank2_10_13_variant_a (omap_gpio.c, real body there, per that
 * file's own citation "2 callers (FUN_c00118b4, FUN_c00056a4)") +
 * gpio_pair0_bit20_level toggled 0/1/0 (omap_gpio.c, real body there) +
 * one large 0x900-element (0x2400-byte / 9KB) forward-shifted in-place
 * block copy (dst=DAT_c000571c, src=dst+0x900 bytes) whose purpose is not
 * identified this pass. NEEDS LIVE QUERY: what DAT_c000571c actually is
 * (a large table/buffer at a non-.text-range address per its own resolved
 * value) - not resolvable from the static dump's data section alone since
 * it decodes as a bare pointer value, not inline bytes. @0xc00056a4. */
extern uint32_t syscfg0_base_get(void *chip);			/* FUN_c0001948, full body in soc_periph.c */
extern void board_desc_set_pinmux_130_a(int handle);		/* FUN_c0001e48, full body in soc_periph.c */
extern void *gpio_bank_get_base(void);				/* FUN_c0001990, full body in soc_periph.c/i2c_by_gpio.c */
extern void gpio_pins_bank2_10_13_variant_a(void *bank_base);	/* FUN_c0002350, full body in omap_gpio.c */
extern void gpio_pair0_bit20_level(void *bank_base, int level);	/* FUN_c0002450, full body in omap_gpio.c */
extern uint32_t *eva_unk_block_copy_dst;	/* DAT_c000571c - NEEDS LIVE QUERY: real identity */

void FUN_c00056a4(void)		/* @0xc00056a4, name is a location label, not a confirmed subsystem attribution */
{
	uint32_t *dst, *src;
	int i;

	syscfg0_base_get(0 /* DAT_c0005718, real arg not resolved to a concrete value */);
	board_desc_set_pinmux_130_a(0 /* phantom-forwarded arg, see this project's established pattern */);
	gpio_bank_get_base();
	gpio_pins_bank2_10_13_variant_a(0 /* phantom-forwarded arg */);
	gpio_pair0_bit20_level(gpio_bank_get_base(), 0);
	gpio_pair0_bit20_level(gpio_bank_get_base(), 1);

	dst = eva_unk_block_copy_dst;
	src = dst + 0x240;	/* 0x240 * 4 bytes = 0x900 bytes ahead of dst */
	for (i = 0x900; i != 0; i--)
		*dst++ = *src++;

	gpio_pair0_bit20_level(gpio_bank_get_base(), 0);
}
