/* SPDX-License-Identifier: GPL-2.0 */
/*
 * midi_engine.c - the panel firmware's USB-MIDI class-compliant transport
 * layer: raw-byte-to-USB-MIDI-event-packet framing (Code Index Number
 * derivation, running status, SysEx segmentation), a 16-virtual-cable
 * per-cable parser state array, two independent 256-slot event ring
 * buffers, and the external hardware bring-up/status layer for what is
 * almost certainly a discrete multi-port MIDI UART/transceiver chip on the
 * SoC's async external-memory bus.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, read from the
 * pre-fetched dump (all_decompiled.json/all_data.json), 2026-07-18. No live
 * Ghidra MCP calls this pass (bridge flagged concurrency-unsafe this round).
 *
 * ASSIGNMENT AND SCOPE: this file covers the address sweep
 * 0xc000ca50-0xc000d6fc (26 real Ghidra function objects, no unaccounted
 * code gaps once two literal-pool-only gaps are excluded - see "GAPS"
 * below). This range sits immediately after cpsoc.c/panelbus_dispatch.c's
 * own low-address functions and immediately before an unassigned
 * continuation (0xc000d6fc onward, callers FUN_c000d6fc/FUN_c000dcdc/
 * FUN_c000da0c/FUN_c000dc4c/FUN_c000dcbc all live there, out of this file's
 * scope - see "STILL OPEN" below).
 *
 * ANCHOR: NONE. The full 14-string `"../<Name>.cpp"` anchor list (grepped
 * fresh this pass via `query_dump.py strings .cpp`) has no entry anywhere
 * near this range, and a direct search for "midi"/"Midi"/"MIDI" in every
 * defined string in the image returns nothing either - this firmware's
 * MIDI engine, if it ever had a debug filename string, does not carry one
 * into the shipped binary. Attribution here rests entirely on structural
 * and cross-file address evidence, per the same no-anchor discipline
 * heap_alloc.c/panelbus_dispatch.c/wire_dispatch.c/aintc.c already
 * established for this project. File named descriptively per the task
 * brief's own suggested fallback for exactly this situation.
 *
 * WHY THIS IS A USB-MIDI ENGINE, not a guess - the evidence, strongest
 * first:
 *
 *  1. midi_event_push_byte (FUN_c000d618) and its three state helpers
 *     (FUN_c000d1d0/_d380/_d3b4) reproduce the USB-MIDI class Code Index
 *     Number (CIN) derivation algorithm byte-for-byte against the real
 *     MIDI status-byte ranges: status < 0x80 is a running-status data byte;
 *     0x80-0xEF (nibble 8-E) uses a lookup table indexed by the status
 *     nibble; 0xF1/0xF3 are 2-byte System Common messages; 0xF2 is the
 *     3-byte Song Position Pointer; 0xF0 starts a SysEx run (CIN 4,
 *     "SysEx starts or continues"); a trailing 0xF7 (EOX, decompiled as
 *     signed byte -9) closes SysEx with CIN 5/6/7 depending on how many
 *     bytes remained in the final group; every other 0xF0-0xFF byte is a
 *     1-byte System Realtime message (CIN 0xF). This is not a coincidental
 *     resemblance - it is the exact branch structure of the official
 *     USB-MIDI 4-byte event-packet encoder.
 *  2. The per-cable dispatch index (`param_2 & 0xff`, rejected if > 0xf)
 *     bounds the channel/cable argument to exactly 0-15 - the USB-MIDI
 *     spec's own Cable Number field is a 4-bit nibble, i.e. exactly 16
 *     virtual cables. midi_context_reset (FUN_c000ce58) and the external
 *     midi_context_hw_init (FUN_c000763c, address-cited, out of this
 *     file's range) both initialize/clear exactly 16 fixed-stride
 *     (0x10-byte) per-cable records.
 *  3. DECISIVE cross-file confirmation, found by reading FUN_c000a9f4
 *     (omap_l137_usbdc_ext.c's own `usbdc_ep_state9_handler`, declared
 *     there as a bare no-argument extern stub, address-cited but not
 *     reconstructed) directly: its real body walks a USB bulk-endpoint
 *     FIFO buffer 4 bytes at a time, reads the Cable Number nibble from
 *     byte 0 of each group EXACTLY as the USB-MIDI spec defines it, forces
 *     CIN to 0xF for realtime status bytes (byte[1] > 0xf7) the same way
 *     the real class spec requires, and - for cable 1 - calls
 *     `FUN_c000d450(DAT_c0006d6c, pbVar13)` passing the raw 4-byte group
 *     directly as the record argument. FUN_c000d450 is THIS file's own
 *     midi_ring1_push_word. A USB endpoint interrupt handler feeding
 *     literal 4-byte USB-MIDI-shaped packets straight into this file's own
 *     ring-push primitive is about as direct as cross-file confirmation
 *     gets. The same function also calls FUN_c000cc14 (midi_hw_is_enabled)
 *     and `FUN_c000cc60(DAT_c0006d68, 4)` (midi_channel_active with a
 *     literal channel index of 4) - confirming both the small (4-channel)
 *     hardware-side indexing this file's low-level status functions use
 *     and that they are reached from the real USB receive path.
 *  4. midi_ring1_push_word/_ring2_push_word (FUN_c000d450/_d4cc) write
 *     into buffers computed from `omap_usbdc_reloc(usbdc_reloc_base)`
 *     (FUN_c0009194 applied to DAT_c000ce44) at offsets +0x6240/+0xa240/
 *     +0xe300. `usbdc_reloc_base` is the EXACT SAME global
 *     omap_l137_usbdc.c's own omap_usbdc_init_ep0 uses (DAT_c0003b50) -
 *     independently re-derived here as DAT_c000ce44, both resolving to the
 *     identical literal 0xC01CCB10. This ties this file's own buffers
 *     concretely to the same USB core object omap_l137_usbdc.c documents,
 *     matching eva_board_main.c's own already-recorded lead ("FUN_c000ecc4/
 *     FUN_c000cdc8 both call FUN_c0009194 ... with register-block offsets
 *     ... distinct from omap_usbdc_init_ep0's own - plausibly a second,
 *     separate USB register-block setup this project hasn't traced").
 *     FUN_c000a9f4's own RX staging copy (`iVar10 + 0x6200 + ...`, iVar10 =
 *     omap_usbdc_reloc(DAT_c0006d64)) lands within 0x40 bytes of this
 *     file's own +0x6240 TX slot table - the same reloc'd USB object
 *     region, RX and TX halves sitting back to back.
 *  5. FUN_c000cdc8 is not a fresh find - eva_board_main.c's own
 *     eva_board_final_setup already forward-declares it, by address, as
 *     `eva_board_usb_ctx_b_init(void *ctx, void *shared)` and calls it as
 *     the last of its own three-call USB bring-up block. This file
 *     supplies that extern's real body (kept as the SAME name, for
 *     consistency with the already-committed call site) rather than
 *     inventing a new one.
 *
 * HARDWARE (midi_hw_* group, FUN_c000ca50/_ca60/_cc14/_cc3c/_cc60/_cc94):
 * all six operate on `*(uint32_t *)handle` as a 16-bit MMIO register base,
 * obtained through FUN_c0000c38/FUN_c0000c6c (16-bit reg write/read, out of
 * this file's range, address-cited below) which themselves resolve their
 * own base through FUN_c0001a98 - a function that unconditionally returns
 * the fixed literal 0x62000000 regardless of any argument (this project's
 * now-repeated "phantom forwarded parameter" pattern, found independently
 * in i2c_by_gpio.c/cdix4192.c/eva_board_main.c/heap_alloc.c, here at one
 * further remove). 0x62000000 is, per the public TI DA850/OMAP-L1x
 * datasheet's async-EMIF memory map (general SoC-family knowledge, NOT
 * independently re-verified against a datasheet PDF this pass), EMIFA
 * Chip Select 3 - i.e. this is a genuine EXTERNAL device on the parallel
 * async bus, not an on-chip peripheral, consistent with this being a
 * discrete multi-port MIDI UART/transceiver chip rather than software
 * bit-banging. Registers observed: 0x32 (global enable, bit 0x20 - polled
 * by midi_hw_is_enabled, toggled by midi_hw_port_enable), 0x6a (single
 * write-only field, midi_hw_reg6a_write), 0x70 (bit 3 read by
 * midi_hw_reg70_bit3, meaning not decoded), 0x74+channel*0x20 (a 2-bit
 * per-channel status pair, midi_hw_channel_active/_ready), and a
 * 0x80/0xa0/0xc0/0xe0 (+0x20 stride, 4 entries) bank toggled as a block by
 * midi_hw_port_enable together with 0x82/0xa2/0xc2/0xe2 config writes -
 * the same 4-channel indexing FUN_c000a9f4 (see point 3 above) itself
 * uses when it calls midi_hw_channel_active with a literal channel of 4.
 * midi_hw_channel_active/_ready's own channel-index mask (0x1fe0, allowing
 * up to 0xff) is wider than the 4 physical channels actually observed in
 * every real caller found this pass - left as-is (transcribed faithfully,
 * not narrowed to match observed call sites).
 *
 * TWO INDEPENDENT RINGS, a recurring pattern in this firmware (matching
 * cpsoc.c's own multi-instance ring documentation): ring 1
 * (midi_ring1_push_word/_has_space/_drain_submit, handle fields +0x128
 * write-index/+0x130 read-index/+0x300 byte-cursor) is UNGUARDED at its
 * own slot-advance step; ring 2 (midi_ring2_push_word/_is_empty, handle
 * fields +0x13c/+0x144/+0x140) wraps its slot-advance in
 * irq_save_and_disable/irq_restore (FUN_c0005500/_c0005510, the same
 * generic primitive pair crypto_at88.c's own correction pass and
 * panelbus_dispatch.c already cite by this name). A real, transcribed-
 * as-found asymmetry, not smoothed into matching behavior.
 *
 * GAPS: two address ranges inside this sweep contain no Ghidra function
 * object. 0xc000cd50-0xc000cd9c is the end-of-function literal pool for
 * midi_subsystem_init_entry itself (its own embedded operand, not a gap at
 * all once the real 56-byte extent of FUN_c000cd18 is used instead of a
 * stale 36-byte figure). 0xc000cd9c-0xc000cdc8 (0xc000cd9c/_cda4/_cdb0/
 * _cdc4, 4 addresses 8-20 bytes apart) is a REAL unresolved gap: these are
 * exactly the 4 addresses midi_handler_slot0..3 get installed with (see
 * "STILL OPEN" below) - genuine code Ghidra never boundary-detected because
 * nothing in the image calls them with a direct CALL instruction (only
 * indirectly, through the handler-slot globals). NOT reconstructed here -
 * no disassembly access this pass, see NEEDS LIVE QUERY.
 *
 * Everything below is additive - no existing file in this project touches
 * any address in 0xc000ca50-0xc000d6fc.
 */

#include <stdint.h>

/* ---------------------------------------------------------------------
 * Hardware register access primitives - out of this file's own range
 * (0xc0000c38/_c6c, base resolution via FUN_c0001a98 further down), cited
 * from every low-level midi_hw_* function below.
 * --------------------------------------------------------------------- */
extern void     midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val);	/* FUN_c0000c38 */
extern uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off);			/* FUN_c0000c6c */

/* midi_hw_reg6a_write - single fixed-register write, sole caller unknown
 * (out of range, above 0xc000d6fc). @0xc000ca50. */
void midi_hw_reg6a_write(uint32_t *handle, uint8_t val)	/* FUN_c000ca50 */
{
	midi_hw_write16(*handle, 0x6a, val);
}

/* midi_hw_port_enable - global hardware enable/disable toggle for the
 * whole 4-channel external MIDI block. `enable == 1`: writes a config
 * triplet (reg 0x82, reg DAT_c000cc08/0x102, reg 0xa2, all cited as-is -
 * both sub-branches below write the SAME triplet, differing only in the
 * 0xc2 value that follows, a real quirk transcribed faithfully rather
 * than collapsed), selects 0x40 or 0x200 for reg 0xc2 based on bit 0x40 of
 * a mode-flag byte at DAT_c000cc04+1 (midi_hw_mode_flags, meaning not
 * decoded), writes reg 0xe2=0x40, then sets bit 0 of each of the 4
 * per-channel registers (0x80/0xa0/0xc0/0xe0). Any other `enable` value:
 * clears bit 0 of the same 4 registers instead (mask DAT_c000cc0c/0xfffe).
 * Both paths converge on a common tail: read-modify-write reg 0x32's own
 * bit 0x20 (the SAME bit midi_hw_is_enabled polls) - set it (enable path)
 * or clear it (disable path) only if it doesn't already match, with an
 * early return if it already does. @0xc000ca60. */
extern uint8_t  midi_hw_mode_flags;		/* DAT_c000cc04, resolved 0xC01CD314 - +1 byte tested against bit 0x40 */
extern uint32_t midi_hw_reg2_offset;		/* DAT_c000cc08, resolved literal 0x102 - a register offset, not a value */
extern uint16_t midi_hw_ch_disable_mask;	/* DAT_c000cc0c, resolved literal 0xfffe */
extern uint16_t midi_hw_enable_bit_mask;	/* DAT_c000cc10, resolved literal 0xffdf - clears bit 0x20 */

int midi_hw_port_enable(uint32_t *handle, uint8_t enable)	/* FUN_c000ca60 */
{
	int want_enabled = 1;
	uint16_t v;
	uint16_t status;
	unsigned reg;
	int i;

	if (enable == 1) {
		uint16_t c2_val = (midi_hw_mode_flags & 0x40) ? 0x200 : 0x40;

		midi_hw_write16(*handle, 0x82, 0x138);
		midi_hw_write16(*handle, midi_hw_reg2_offset, 0x138);
		midi_hw_write16(*handle, 0xa2, 3);
		midi_hw_write16(*handle, 0xc2, c2_val);
		midi_hw_write16(*handle, 0xe2, 0x40);

		for (reg = 0x80, i = 4; i > 0; reg += 0x20, i--) {
			v = midi_hw_read16(*handle, reg);
			midi_hw_write16(*handle, reg, (uint16_t)((v & 0xffff) | 1));
		}
	} else {
		for (reg = 0x80, i = 4; i > 0; reg += 0x20, i--) {
			v = midi_hw_read16(*handle, reg);
			midi_hw_write16(*handle, reg, (uint16_t)(v & midi_hw_ch_disable_mask));
		}
		want_enabled = 0;
	}

	status = midi_hw_read16(*handle, 0x32);
	if (want_enabled == 0) {
		if ((status & 0x20) == 0)
			return 0;
		midi_hw_write16(*handle, 0x32, status & midi_hw_enable_bit_mask);
	} else {
		if ((status & 0x20) != 0)
			return want_enabled;
		midi_hw_write16(*handle, 0x32, (uint16_t)((status & 0xffff) | 0x20));
	}
	return want_enabled;
}

/* midi_hw_is_enabled - polls the same bit 0x20 of reg 0x32
 * midi_hw_port_enable's own tail toggles. Confirmed caller:
 * master_dispatch_tick (wire_dispatch.c, FUN_c0008b64) as part of its own
 * documented-but-untranscribed "USB-adjacent state-machine cluster"; also
 * FUN_c000da0c and FUN_c000a9f4 (usbdc_ep_state9_handler,
 * omap_l137_usbdc_ext.c), both out of this file's range. @0xc000cc14. */
int midi_hw_is_enabled(uint32_t *handle)	/* FUN_c000cc14 */
{
	return (midi_hw_read16(*handle, 0x32) & 0x20) != 0;
}

/* midi_hw_reg70_bit3 - single-bit status read, physical meaning not
 * decoded (candidate: external device present/line-detect). Sole caller
 * FUN_c000e11c, out of range. @0xc000cc3c. */
int midi_hw_reg70_bit3(uint32_t *handle)	/* FUN_c000cc3c */
{
	return (midi_hw_read16(*handle, 0x70) >> 3) & 1;
}

/* midi_hw_channel_active / midi_hw_channel_ready - per-channel 2-bit
 * status pair at reg 0x74 + (channel << 5), mask DAT_c000cc90/_cccc
 * (both resolved to the same literal 0x1fe0). _active is true if either
 * bit is set; _ready is true only when both are (value == 3). Confirmed
 * real callers pass small literal channel indices (FUN_c000a9f4 passes 4
 * - see this file's header), though the mask itself permits up to 0xff.
 * @0xc000cc60 / @0xc000cc94. */
extern uint16_t midi_hw_ch_status_mask;	/* DAT_c000cc90 / DAT_c000cccc, resolved literal 0x1fe0 (shared constant) */

int midi_hw_channel_active(uint32_t *handle, int channel)	/* FUN_c000cc60 */
{
	uint16_t v = midi_hw_read16(*handle, ((channel << 5) & midi_hw_ch_status_mask) + 0x74);
	return (v & 3) != 0;
}

int midi_hw_channel_ready(uint32_t *handle, int channel)	/* FUN_c000cc94 */
{
	uint16_t v = midi_hw_read16(*handle, ((channel << 5) & midi_hw_ch_status_mask) + 0x74);
	return (v & 3) == 3;
}

/* ---------------------------------------------------------------------
 * Context init / reset. midi_context_hw_init (FUN_c000763c) is OUT of
 * this file's own address range (0xc000763c, well below 0xc000ca50) but
 * is the direct caller of 5 functions reconstructed below
 * (midi_handler_slot0..3's installers and midi_context_reset), so its
 * signature and behavior are documented here from its own decompile for
 * context, not redefined.
 *
 * extern void midi_context_hw_init(void *unused_handle, uint8_t reg2,
 *     uint8_t reg3, void *callback, int record_size);   -- FUN_c000763c
 *
 * Body (address-cited, not owned by this file): ignores its own
 * `unused_handle` argument entirely (same phantom-parameter pattern as
 * FUN_c0001a98 above) and instead operates on a FIXED global singleton at
 * DAT_c0007654 (resolved literal 0xC01CAD44 - a real, always-populated
 * pointer literal, not a runtime allocation). Writes reg2/reg3/callback/
 * record_size into that singleton's own +4/+8/+0xc/+0x10 fields (the last
 * rounded up to a multiple of 4), installs the 4 midi_handler_slotN
 * globals via midi_handler_slot0_install..slot3_install below, zero-inits
 * all 16 per-cable records' +0x28/+0x2b bytes and stamps each record's own
 * +0x2c byte with its own base offset, calls midi_context_reset(singleton,
 * 0), then clears the singleton's own +0x54c (the SysEx-mode flag
 * midi_event_push_byte reads)/+0x18/+0x20 fields. Sole call site found
 * this pass: midi_subsystem_init_entry below, which itself forwards a
 * DIFFERENT fixed constant (DAT_c000cd50, resolved 0xC01CAC00) as
 * `unused_handle` - consistent with the "dead handle, real object is a
 * fixed global" pattern already established firmware-wide.
 * --------------------------------------------------------------------- */

/* midi_subsystem_init_entry - thin 4-argument forwarder into
 * midi_context_hw_init (FUN_c000763c), prepending the fixed dead-handle
 * constant DAT_c000cd50. UNLIKE every other function in this file, this
 * one has NO direct-call xref anywhere in the image - its only reference
 * is a DATA read at 0xc001f73c, i.e. it is installed as a function-pointer
 * table entry rather than called directly. That table is NOT
 * eva_board_init_table (eva_board_main.c's own documented table has
 * exactly one, unrelated entry, at a different address) - which table
 * 0xc001f73c belongs to is unresolved, see NEEDS LIVE QUERY.
 * @0xc000cd18 (56 bytes - the trailing word at 0xc000cd50 is this
 * function's own embedded literal-pool operand, not a gap). */
extern void midi_context_hw_init(void *unused_handle, uint8_t reg2, uint8_t reg3,
	void *callback, int record_size);	/* FUN_c000763c, see note above */
extern void *midi_dead_handle_const;		/* DAT_c000cd50, resolved 0xC01CAC00 */

void midi_subsystem_init_entry(uint8_t reg2, uint8_t reg3, void *callback, int record_size)	/* FUN_c000cd18 */
{
	midi_context_hw_init(midi_dead_handle_const, reg2, reg3, callback, record_size);
}

/* eva_board_usb_ctx_b_init - kept under the EXACT name eva_board_main.c's
 * own eva_board_final_setup already declares and calls this address by
 * (`extern void eva_board_usb_ctx_b_init(void *ctx, void *shared); FUN_c000cdc8`).
 * Sets ctx's own +0 field to `shared` (the ctx pointer eva_board_main.c
 * itself already threads through eva_board_usb_ctx_a_init/_shared_setup),
 * then resolves 3 sub-buffer pointers off the SAME usbdc_reloc_base global
 * omap_l137_usbdc.c's own omap_usbdc_init_ep0 uses (independently
 * re-derived here as DAT_c000ce44, both literals resolving to 0xC01CCB10 -
 * see this file's header point 4), storing base+0x6240 into
 * midi_tx_slot_table (read back by midi_ring1_drain_submit below),
 * base+0xa240 into midi_cable_name_table - ring 2's own backing store,
 * NOT a string table (see the naming-correction note at
 * midi_ring2_pop_copy below; the "cable_name_table" identifier is a
 * carried-over misnomer, kept only for internal cross-reference
 * consistency within this file) - and base+0xe300 into a third slot
 * with no reader found anywhere in this file's own range. Finally zeroes 3
 * bytes near the end of `ctx` at a size-derived offset (DAT_c000ce54,
 * resolved literal 0x13a - ctx[0x13a], ctx[0x138], ctx[0x139]).
 * @0xc000cdc8. */
extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c0009194, cited from omap_l137_usbdc.c */
extern uint32_t usbdc_reloc_base;			/* DAT_c000ce44, same global as omap_l137_usbdc.c's DAT_c0003b50/DAT_c0009678 - all three resolve to 0xC01CCB10 */
extern uint8_t *midi_tx_slot_table;			/* DAT_c000ce48 (also DAT_c000d5cc below, same global 0xC01CD328) - usbdc_reloc_base + 0x6240 */
extern uint8_t *midi_cable_name_table;			/* DAT_c000ce4c (also DAT_c000d1ac below, same global 0xC01CD324) - usbdc_reloc_base + 0xa240 */
extern uint8_t *midi_usb_slot_c_unread;		/* DAT_c000ce50, 0xC01CD320 - usbdc_reloc_base + 0xe300, no reader found in this file's range */
extern int      midi_ctx_tail_zero_off;		/* DAT_c000ce54, resolved literal 0x13a */

void eva_board_usb_ctx_b_init(void **ctx, void *shared)	/* FUN_c000cdc8 */
{
	uint8_t *c = (uint8_t *)ctx;

	*ctx = shared;
	midi_tx_slot_table     = (uint8_t *)(omap_usbdc_reloc(usbdc_reloc_base) + 0x6240);
	midi_cable_name_table  = (uint8_t *)(omap_usbdc_reloc(usbdc_reloc_base) + 0xa240);
	midi_usb_slot_c_unread = (uint8_t *)(omap_usbdc_reloc(usbdc_reloc_base) + 0xe300);

	c[midi_ctx_tail_zero_off] = 0;
	c[midi_ctx_tail_zero_off - 2] = 0;
	c[midi_ctx_tail_zero_off - 1] = 0;
}

/* midi_context_reset - full per-context state clear. Sets a busy flag
 * (+0x548), optionally flushes hardware (midi_hw_flush_notify, only when
 * `notify` is non-zero - a real, transcribed-as-found conditional, not
 * always-flush), clears 8 scalar bookkeeping fields (+0x144/+0x550/
 * +0x128/+0x12c/+0x130/+0x134/+0x13c/+0x140 - the write/read-index pairs
 * for both rings plus 4 more), then zeroes one 4-byte field at +0xc within
 * EACH of the 16 per-cable records (stride 0x10, matching the record
 * layout midi_event_push_byte itself uses - a field this file's own
 * per-cable parser (FUN_c000d1d0/_d380/_d3b4) never itself reads or
 * writes, meaning of that field not resolved), and finally clears the
 * busy flag. Confirmed callers: midi_context_hw_init (FUN_c000763c, out
 * of range) and FUN_c000da0c (out of range, the same status-bit poller
 * midi_hw_is_enabled's own header cites). @0xc000ce58. */
extern void midi_hw_flush_notify(uint32_t base_sel, uint8_t arg);	/* FUN_c000bf08 -> FUN_c000bedc, out of range */

void midi_context_reset(uint32_t *ctx, int notify)	/* FUN_c000ce58 */
{
	uint32_t *rec;
	int i;

	ctx[0x152] = 1;
	if (notify != 0)
		midi_hw_flush_notify(ctx[0], ((uint8_t *)ctx)[8]);

	ctx[0x51] = 0;
	ctx[0x154] = 0;
	ctx[0x4a] = 0;
	ctx[0x4b] = 0;
	ctx[0x4c] = 0;
	ctx[0x4d] = 0;
	ctx[0x4f] = 0;
	ctx[0x50] = 0;

	rec = ctx;
	for (i = 0; i < 16; i++) {
		rec[0xd] = 0;	/* per-cable record field +0xc, meaning not resolved */
		rec += 4;	/* advance 0x10 bytes = one per-cable record */
	}

	ctx[0x152] = 0;
}

/* midi_handler_slotN_install - four near-identical 12-byte functions.
 * Each IGNORES its own first argument (yet another phantom-parameter
 * instance) and unconditionally stores its second argument into a fixed
 * global slot. Called consecutively from midi_context_hw_init
 * (FUN_c000763c, out of range) with 4 fixed target addresses -
 * 0xc000cd9c/_cda4/_cdb0/_cdc4 - that fall exactly in this file's own
 * unresolved code gap (see "GAPS" in the file header). Slot 0
 * (0xC01CD334) is READ BACK by midi_stream_decode_step below via
 * DAT_c000d0bc, the strongest evidence these are real callback-function-
 * pointer slots and not plain data. @0xc000cf20 / @0xc000cf30 /
 * @0xc000cf40 / @0xc000cf50. */
extern void *midi_handler_slot0;	/* DAT_c000cf2c (also DAT_c000d0bc below, same global 0xC01CD334) - target 0xc000cd9c, NEEDS LIVE QUERY */
extern void *midi_handler_slot1;	/* DAT_c000cf3c, 0xC01CD33C - target 0xc000cda4, NEEDS LIVE QUERY */
extern void *midi_handler_slot2;	/* DAT_c000cf4c, 0xC01CD32C - target 0xc000cdb0, NEEDS LIVE QUERY */
extern void *midi_handler_slot3;	/* DAT_c000cf5c, 0xC01CD338 - target 0xc000cdc4, NEEDS LIVE QUERY */

void midi_handler_slot0_install(void *unused, void *handler)	/* FUN_c000cf20 */
{
	midi_handler_slot0 = handler;
}

void midi_handler_slot1_install(void *unused, void *handler)	/* FUN_c000cf30 */
{
	midi_handler_slot1 = handler;
}

void midi_handler_slot2_install(void *unused, void *handler)	/* FUN_c000cf40 */
{
	midi_handler_slot2 = handler;
}

void midi_handler_slot3_install(void *unused, void *handler)	/* FUN_c000cf50 */
{
	midi_handler_slot3 = handler;
}

/* ---------------------------------------------------------------------
 * midi_stream_decode_step - a nibble-tagged byte-run decoder driving a
 * per-byte callback through midi_handler_slot0 (see cross-reference
 * above - DAT_c000d0bc resolves to the SAME global 0xC01CD334 that
 * midi_handler_slot0_install writes). Source record layout (param_1,
 * an undefined4* treated as byte-addressable via the casts below):
 * +0x18 = "bytes remaining in current run" counter, +0x1c = source
 * cursor, +0x20 = read-side length remaining, +0x24 = current tag
 * nibble (saved across calls). When the run is exhausted, pulls a new
 * source byte, looks up its low nibble in a 16-entry table
 * (midi_stream_len_table, DAT_c000d0b8) to get the next run's byte
 * count, and saves the byte's own high nibble as the new tag - a
 * classic 4-bit-tag/4-bit-implicit-length framing scheme. For each byte
 * in a run, calls `midi_handler_slot0(tag, *src++)`; if the callback
 * returns 0, the byte is NOT consumed (retried next call) - this makes
 * the whole thing a resumable, callback-driven pull parser, most plausibly
 * feeding decoded bytes onward into midi_event_push_byte's own per-cable
 * state machine, though no direct call from here into that machine is
 * itself present (the indirection is exactly what makes it consistent
 * with the "handler pointer, not a direct call" pattern documented
 * above). Callers FUN_c000dcbc and one unresolved ("None" in the xref
 * data - Ghidra didn't resolve a containing function) are both out of
 * this file's own range. @0xc000cfc8. */
extern uint8_t midi_stream_len_table[16];	/* DAT_c000d0b8, resolved base 0xC001F64C */

void midi_stream_decode_step(uint32_t *param_1)	/* FUN_c000cfc8 */
{
	uint8_t *src;
	uint8_t *tag_byte;
	uint8_t b;
	int consumed;

	tag_byte = (uint8_t *)(param_1 + 9);	/* byte at param_1+0x24 */

	if (param_1[6] != 0)
		goto have_run;
	if (param_1[8] == 0)
		return;

	for (;;) {
		if ((int)param_1[8] > 0) {
			consumed = ((int (*)(uint8_t, uint8_t))midi_handler_slot0)(*tag_byte, *(uint8_t *)param_1[7]);
			if (consumed == 0) {
				if (param_1[6] != 0)
					return;
				goto maybe_flush;
			}
			param_1[7] = param_1[7] + 1;
			param_1[8] = param_1[8] - 1;
		}
have_run:
		if (param_1[8] == 0) {
			if (param_1[6] == 0) {
maybe_flush:
				if (param_1[8] == 0) {
					midi_hw_flush_alt(param_1[0], *(uint8_t *)(param_1 + 1));
					return;
				}
				return;
			}
			src = (uint8_t *)param_1[5];
			param_1[7] = (uint32_t)src;
			b = *src;
			*tag_byte = b >> 4;
			param_1[8] = midi_stream_len_table[b & 0xf];
			param_1[7] = (uint32_t)(src + 1);
			param_1[5] = (uint32_t)(src + 4);
			param_1[6] = param_1[6] - 1;
		}
	}
}

/* ---------------------------------------------------------------------
 * Ring 2's own "headroom check" and "pop one slot, copy it out" pair.
 * midi_ring2_headroom_ok checks whether the write cursor (+0x13c) can
 * advance 8 more slots before catching the read cursor (+0x144) - used
 * as a soft gate. midi_ring2_pop_copy pops one slot (if the ring isn't
 * empty: read index +0x144 != write index +0x13c) and copies its bytes
 * into the caller-supplied buffer.
 *
 * CORRECTED THIS PASS: an earlier draft of this section called
 * midi_cable_name_table a "cable name string table" and named this
 * function accordingly ("_headroom_name_step") - wrong. Reading this
 * function's own body directly settles it: `entry` here is computed as
 * `midi_cable_name_table + read_idx * 0x40`, the EXACT SAME base and
 * 0x40-byte stride midi_ring2_push_word writes into (both resolve
 * through the identical global, DAT_c000d1ac here / DAT_c000d564 there,
 * both 0xC01CD324). This is ring 2 popping its OWN data, not reading a
 * separate name table. eva_board_usb_ctx_b_init's own comment
 * (usbdc_reloc_base+0xa240) is corrected to match.
 *
 * The per-slot BYTE LENGTH read from `ctx[read_idx + 0x52]` (word index,
 * i.e. ctx+0x148 + read_idx*4 - a 256-entry, 4-byte-per-entry table
 * living in the gap right before ctx+0x54c, the SysEx-mode flag
 * midi_event_push_byte reads) is a THIRD, separate parallel array, not
 * ring 1's fixed +0x10 "record size" field - ring 2's slots are treated
 * as variable-length.
 *
 * The copy loop's own NUL-termination check is preserved LITERALLY as
 * decompiled rather than "cleaned up" - see the in-loop comment below;
 * deliberately not simplified, per this project's standing policy after
 * crypto_at88.c's own real byte-order bugs were found specifically by
 * not doing that.
 *
 * After popping, advances the read index (0xff wraparound) and, only
 * when midi_ring2_headroom_ok reports "far from full" (return 1), calls
 * midi_hw_set_reg_d8(ctx, 1) - a reg-0xd8 write via midi_hw_write16
 * (FUN_c000b870). Naming deliberately neutral: the direction implied
 * ("plenty of room" -> write 0x10 to 0xd8) reads backwards for a typical
 * "assert flow control when full" idiom, so no flow-control polarity is
 * claimed here. Sole caller FUN_c0006e90, out of range. @0xc000d0c0 /
 * @0xc000d0fc. */
extern void midi_hw_set_reg_d8(uint32_t *handle, uint8_t val);	/* FUN_c000b870 -> reg 0xd8 via midi_hw_write16 */

int midi_ring2_headroom_ok(uint32_t *ctx)	/* FUN_c000d0c0 */
{
	int steps = 0;
	int cur = ctx[0x4f];		/* +0x13c */

	for (;;) {
		int next = (cur > 0xfe) ? 0 : cur + 1;
		steps++;
		if (next == (int)ctx[0x51])	/* +0x144 */
			break;
		cur = next;
		if (steps > 7)
			return 1;
	}
	return 0;
}

uint32_t midi_ring2_pop_copy(uint32_t *ctx, char *out)	/* FUN_c000d0fc */
{
	int read_idx = ctx[0x51];		/* +0x144 */
	uint32_t copied = 0;

	if (read_idx != (int)ctx[0x4f]) {	/* +0x13c, "ring not empty" */
		uint32_t remain = ctx[read_idx + 0x52];	/* per-slot byte length table, ctx+0x148 */
		char *entry = (char *)(midi_cable_name_table + read_idx * 0x40);
		uint32_t n = 0;

		copied = remain;	/* default: ran to full length */
		while (n < remain) {
			out[n] = entry[n];
			if ((n & 3) == 0) {
				/* NOTE: the raw decompile tests `*entry` (index 0,
				 * NOT entry[n] - the pointer is never advanced for
				 * this check) here, only at 4-byte-aligned n. Kept
				 * literal rather than "corrected" to entry[n] - could
				 * be a genuine firmware quirk (an entry-level "empty"
				 * flag at offset 0) or a decompiler pointer-reuse
				 * artifact; not resolved this pass. */
				if (entry[0] == '\0') {
					copied = n;
					break;
				}
			}
			n++;
		}

		ctx[0x51] = (ctx[0x51] < 0xff) ? ctx[0x51] + 1 : 0;

		if (midi_ring2_headroom_ok(ctx) != 0)
			midi_hw_set_reg_d8(ctx, 1);
	}
	return copied;
}

/* ---------------------------------------------------------------------
 * USB-MIDI event-packet state machine. Per-cable record (16 records,
 * 0x10-byte stride, base = ctx+0x28): +0 state (0=idle/running-status,
 * 1=filling a channel-voice message's data bytes, 2=filling a SysEx
 * run), +1 write index within the 4-byte packet buffer at +5, +2
 * remaining-byte counter, +3 saved status byte (running status), +4
 * saved Cable-Number-nibble seed for the next packet's CIN byte, +5..+8
 * the actual 4-byte USB-MIDI event packet being assembled, +0xc unknown
 * (zeroed by midi_context_reset, never read here).
 * --------------------------------------------------------------------- */

/* midi_evt_record_reset - packet-buffer restart: write index -> 0,
 * carries the saved Cable-Number seed (+4) into the CIN byte (+5),
 * clears the 3 data-byte slots (+6/+7/+8). @0xc000d1b0. */
void midi_evt_record_reset(uint32_t ctx_unused, uint8_t *rec)	/* FUN_c000d1b0 */
{
	rec[1] = 0;
	rec[5] = rec[4];
	rec[6] = 0;
	rec[7] = 0;
	rec[8] = 0;
}

/* midi_evt_state_idle_byte - state 0: a fresh status byte or a
 * running-status data byte has arrived. `ctx` IS a real, used argument
 * here (unlike midi_evt_record_reset/_fill_data/_sysex_byte, which all
 * ignore theirs) - only for one out-of-band side effect: when a global
 * SysEx-mode flag (ctx+0x54c) is set, also stamps `rec+0xc = 1` and
 * clears `ctx+0x134` (meaning of both not resolved further).
 *
 * Status nibble < 8 (byte < 0x80): a bare data byte under running status.
 * Reuses the remembered status byte (rec[3], from a PRIOR call) to derive
 * the CIN and replay the status byte itself into the packet at the OLD
 * write index, then advances the write index by 2 (not 1) - the actual
 * incoming data byte is deliberately NOT written here. This is
 * intentional, not a gap: rec[0] was just set to 1 above, and
 * midi_event_push_byte's own caller-side dispatch (two sequential `if`s,
 * not `if/else if` - see midi_event_push_byte below) unconditionally
 * re-checks state and, seeing 1, ALSO calls midi_evt_state_fill_data with
 * the SAME incoming byte immediately afterward in the same invocation -
 * that second call is what actually stores it. Real double-dispatch,
 * confirmed present in the raw decompile's own if/if (not if/else-if)
 * shape at the call site.
 *
 * Status nibble 8-0xE: standard channel voice/mode message. CIN pulled
 * from midi_cin_table[nibble], only the CIN nibble is written here (one
 * byte, at the current write index, advancing it by 1) - the data bytes
 * arrive via the SAME double-dispatch mechanism as above (fill_data is
 * called right after, since rec[0]==1 here too). Status byte saved to
 * rec[3] for future running-status use.
 *
 * Status 0xF0: starts SysEx (state -> 2, byte itself NOT written here -
 * midi_event_push_byte's double-dispatch instead routes it into
 * midi_evt_state_sysex_byte immediately after, since rec[0] becomes 2 and
 * the SysEx branch, not fill_data, fires - see that function's own note
 * on why replaying 0xF0 as the first SysEx data byte is correct per the
 * USB-MIDI SysEx-packet spec, not a bug).
 * Status 0xF1/0xF3: 2-byte System Common (CIN 2). Status 0xF2: 3-byte
 * Song Position Pointer (CIN 3). Any other 0xF4-0xF7 (0xF8-0xFF never
 * reach this function - midi_event_push_byte's own top-level gate routes
 * those through a separate path): 1-byte System Common (CIN 0xF).
 * @0xc000d1d0. */
extern uint8_t midi_cin_table[16];	/* DAT_c000d37c, resolved base 0xC001F63C - standard USB-MIDI CIN-by-status-nibble table, real byte contents NOT read this pass (needs live query) */

void midi_evt_state_idle_byte(uint32_t *ctx, uint8_t *rec, uint8_t status)	/* FUN_c000d1d0 */
{
	unsigned nibble = status >> 4;

	midi_evt_record_reset(0, rec);
	if (*(uint32_t *)((uint8_t *)ctx + 0x54c) != 0) {
		*(uint32_t *)(rec + 0xc) = 1;
		*(uint32_t *)((uint8_t *)ctx + 0x134) = 0;
	}
	rec[0] = 1;

	if (nibble < 8) {
		/* running-status continuation */
		uint8_t saved_status = rec[3];
		uint8_t widx_old = rec[1];

		if ((uint8_t)(saved_status + 0x80) < 0x70) {
			uint8_t cin, widx_mid;

			rec[1] = widx_old + 1;
			cin = midi_cin_table[saved_status >> 4];
			widx_mid = rec[1];
			rec[widx_old + 5] |= saved_status >> 4;
			rec[2] = cin - 1;
			rec[widx_mid + 5] = saved_status;
			rec[1] = widx_mid + 1;
			return;
		}
		/* no valid remembered status - mark CIN invalid (0xf) */
		rec[widx_old + 5] |= 0xf;
		rec[2] = 1;
		rec[1] = widx_old + 1;
		return;
	}

	{
		unsigned group = nibble - 8;
		if (group < 7) {
			uint8_t widx = rec[1];
			uint8_t cin = midi_cin_table[nibble];

			rec[widx + 5] |= status >> 4;
			rec[2] = cin;
			rec[1] = widx + 1;
			rec[3] = status;	/* save for running status */
			return;
		}

		/* status is 0xF0-0xF7 (0xF8-0xFF never reach this function) */
		if (status == 0xf0) {
			rec[2] = 0;
			rec[0] = 2;
			return;
		}
		{
			uint8_t widx = rec[1];

			if (status == 0xf1 || status == 0xf3) {
				rec[widx + 5] |= 2;
				rec[2] = 2;
			} else if (status == 0xf2) {
				rec[widx + 5] |= 3;
				rec[2] = 3;
			} else {
				rec[widx + 5] |= 0xf;
				rec[2] = 1;
			}
			rec[1] = widx + 1;
		}
	}
}

/* midi_evt_state_fill_data - state 1: fill one more data byte into the
 * in-progress packet, decrementing the remaining counter; hits 0 ->
 * back to state 0 (packet complete). @0xc000d380. */
void midi_evt_state_fill_data(uint32_t ctx_unused, uint8_t *rec, uint8_t data)	/* FUN_c000d380 */
{
	uint8_t remain = rec[2];
	uint8_t widx = rec[1];

	rec[widx + 5] = data;
	rec[1] = widx + 1;
	rec[2] = remain - 1;
	if ((uint8_t)(remain - 1) == 0)
		rec[0] = 0;
}

/* midi_evt_state_sysex_byte - state 2: SysEx run. Starts a fresh 3-byte
 * group (CIN 4, "SysEx starts or continues") whenever the group counter
 * hits 0, then fills bytes into it same as state 1. A trailing EOX
 * (0xF7, decompiled as signed byte -9) closes the SysEx early with CIN
 * 5/6/7 depending on how many bytes landed in the final (possibly
 * partial) group, and returns to state 0. NOTE: this function's very
 * first call for a given SysEx run is the double-dispatch one triggered
 * immediately after midi_evt_state_idle_byte sets state=2 on seeing 0xF0
 * (see that function's own note) - meaning `data` on that first call is
 * the 0xF0 byte itself, correctly captured as the first SysEx data byte
 * per the USB-MIDI packet spec (a SysEx-start packet's payload begins
 * with 0xF0), not a stray re-processing bug. @0xc000d3b4. */
void midi_evt_state_sysex_byte(uint32_t ctx_unused, uint8_t *rec, uint8_t data)	/* FUN_c000d3b4 */
{
	uint8_t widx, remain;

	if (rec[2] == 0) {
		midi_evt_record_reset(0, rec);
		widx = rec[1];
		rec[widx + 5] |= 4;
		rec[1] = widx + 1;
		rec[2] = 3;
	}

	widx = rec[1];
	remain = rec[2];
	rec[widx + 5] = data;
	rec[1] = widx + 1;
	rec[2] = remain - 1;

	if (data != 0xf7)
		return;

	rec[2] = 0;
	rec[5] = (rec[5] & 0xf0) | (rec[1] + 3);
	rec[0] = 0;
}

/* midi_event_push_byte - the top-level entry point: feed one raw MIDI
 * byte for cable `cable` (0-15, out-of-range rejected with return 1).
 * Dispatches by the record's own state byte to one of the three
 * functions above, then hands the (possibly still-in-progress, possibly
 * just-completed) 4-byte packet to midi_ring1_push_word - UNCONDITIONALLY
 * on every byte, not just on packet completion (matches the raw
 * decompile: the ring absorbs partial-packet snapshots too, not only
 * finished ones - transcribed as-found). A separate `+0x54c` global-mode
 * flag byte, when set, instead builds a local 4-byte scratch record
 * (Cable Number packed into the top nibble, tag 0xf) and pushes that
 * SysEx-style through midi_ring1_push_word for out-of-range status bytes
 * (0xf8-0xff) without touching the per-cable record at all. When that
 * same flag is set, also calls midi_ring1_drain_submit once per byte
 * after the ring push - i.e. that mode runs synchronously
 * (push-then-immediately-drain) rather than relying on a separate
 * consumer. Two real callers, both out of this file's range
 * (FUN_c000dcdc, and one Ghidra could not resolve a containing function
 * for). @0xc000d618. */
extern int midi_ring1_push_word(uint32_t *ctx, void *word4);		/* FUN_c000d450, defined below */
extern void midi_ring1_drain_submit(uint32_t *ctx);			/* FUN_c000d568, defined below */

int midi_event_push_byte(uint32_t *ctx, unsigned cable, uint8_t byte)	/* FUN_c000d618 */
{
	uint8_t *rec;
	uint8_t local[4];

	cable &= 0xff;
	if (cable > 0xf)
		return 1;

	if (byte < 0xf8) {
		uint8_t *base = (uint8_t *)ctx + cable * 0x10;
		uint8_t *r = base + 0x28;

		if (r[0] == 0)
			midi_evt_state_idle_byte(ctx, r, byte);
		if (r[0] == 1)
			midi_evt_state_fill_data(0, r, byte);
		else
			midi_evt_state_sysex_byte(0, r, byte);

		if (*(uint32_t *)(base + 0x34) == 0 /* per-cable ctx+0x34 field, see midi_context_reset's own +0xc-per-record field */
		    || base[0x2a] != 0)
			goto submit_if_sysex_mode;
		rec = base + 0x2d;
	} else {
		if (*(uint32_t *)((uint8_t *)ctx + 0x54c) == 0)
			return 1;
		local[0] = (uint8_t)((cable << 4) | 0xf);
		local[3] = byte;
		local[2] = 0;
		local[1] = 0;
		rec = local;
	}

	midi_ring1_push_word(ctx, rec);

submit_if_sysex_mode:
	if (*(uint32_t *)((uint8_t *)ctx + 0x54c) != 0)
		midi_ring1_drain_submit(ctx);
	return 1;
}

/* ---------------------------------------------------------------------
 * Ring buffers. Ring 1: write index ctx+0x128, read index ctx+0x130,
 * byte cursor ctx+0x12c (0x12c decompiled as literal 300), record size
 * ctx+0x10 (per-instance, not fixed at 4 - see crypto_at88.c's own
 * documented pattern of oversized queue-entry capacities for the same
 * kind of "configurable record size" field), backing store
 * midi_tx_slot_table (see eva_board_usb_ctx_b_init above), 256-slot
 * wraparound (index 0xfe -> 0). UNGUARDED - no irq_save around the
 * slot-advance. Ring 2: write index ctx+0x13c, read index ctx+0x144
 * (also used by midi_ring2_headroom_ok/_name_step above), byte cursor
 * ctx+0x140 (decompiled literal 0x140), same 256-slot wraparound, but
 * IRQ-GUARDED (irq_save_and_disable/irq_restore bracket the slot-advance
 * only) - a real, transcribed asymmetry between the two rings, matching
 * this project's now-repeated finding of asymmetric guard patterns
 * across sibling primitives (cpsoc.c's own reg 0x79/0x7b asymmetry is the
 * closest prior instance). @0xc000d450 / @0xc000d4cc / @0xc000d568 /
 * @0xc000d5d0 / @0xc000d600.
 * --------------------------------------------------------------------- */
extern int  irq_save_and_disable(void);	/* FUN_c0005500, cited from panelbus_dispatch.c's own correction of crypto_at88.c */
extern void irq_restore(void);			/* FUN_c0005510 */

int midi_ring1_push_word(uint32_t *ctx, void *word4)	/* FUN_c000d450 */
{
	int byte_off = ctx[0x4b];	/* +0x12c */
	int slot = ctx[0x4a];		/* +0x128 */

	((uint32_t *)midi_tx_slot_table)[(byte_off >> 2) + slot * 0x10] = *(uint32_t *)word4;
	byte_off += 4;
	ctx[0x4b] = byte_off;

	if (byte_off != (int)ctx[4])	/* +0x10, per-instance record size */
		return 0;

	ctx[0x4a] = (slot > 0xfe) ? 0 : slot + 1;
	ctx[0x4b] = 0;
	return 1;
}

/* midi_ring2_push_word - twin of midi_ring1_push_word above, but with an
 * IRQ-guarded slot advance (irq_save_and_disable/irq_restore bracket the
 * index update; ring 1's own push does not). Writes through
 * midi_cable_name_table (DAT_c000d564, resolved to the SAME 0xC01CD324
 * eva_board_usb_ctx_b_init's own DAT_c000ce4c and midi_ring2_pop_copy's
 * own DAT_c000d1ac resolve to) - three independent address derivations
 * agreeing is what confirms this is ring 2's real, single backing store,
 * not a name/string table despite the identifier's own name (see
 * midi_ring2_pop_copy's own correction note below for the full history).
 * @0xc000d4cc. */
int midi_ring2_push_word(uint32_t *ctx, void *word4)	/* FUN_c000d4cc */
{
	int byte_off = ctx[0x50];	/* +0x140 */
	int result = 0;

	((uint32_t *)midi_cable_name_table)[(byte_off >> 2) + ctx[0x4f] * 0x10] = *(uint32_t *)word4;
	byte_off += 4;
	ctx[0x50] = byte_off;

	if (byte_off == (int)ctx[4]) {	/* +0x10 */
		int flags = irq_save_and_disable();
		int slot = (ctx[0x4f] < 0xff) ? ctx[0x4f] + 1 : 0;
		(void)flags;
		ctx[0x4f] = slot;
		irq_restore();
		result = 1;
		ctx[0x50] = 0;
	}
	return result;
}

/* midi_ring1_drain_submit - pop one slot off ring 1 and hand it to the
 * generic submit primitive (midi_hw_submit, FUN_c000c1f0, address-cited,
 * out of range - the same wrapper family panelbus_dispatch.c's own
 * "generic USB-submit primitive" language describes). Also brackets its
 * own index advance in irq_save_and_disable/irq_restore (unlike
 * midi_ring1_push_word's own unguarded write side - guard asymmetry is
 * between ring 1's push and pop, not just between ring 1 and ring 2).
 * @0xc000d568. */
extern uint32_t midi_hw_submit(uint32_t base_sel, uint8_t flag, uint32_t addr, uint32_t len);	/* FUN_c000c1f0, out of range */
extern uint32_t midi_tx_slot_base;	/* DAT_c000d5cc, same global as DAT_c000ce48 (0xC01CD328) */

void midi_ring1_drain_submit(uint32_t *ctx)	/* FUN_c000d568 */
{
	int slot = ctx[0x4c];		/* +0x130 */
	uint32_t base = midi_tx_slot_base;

	ctx[0x154] = 1;
	(void)irq_save_and_disable();
	ctx[0x4c] = (slot < 0xff) ? slot + 1 : 0;
	irq_restore();
	ctx[0x4d] = 0;

	midi_hw_submit(ctx[0], ((uint8_t *)ctx)[8], base + slot * 0x40, ctx[4]);
}

/* midi_ring1_has_space - "write index + 1 != read index" wraparound-
 * aware check, i.e. true when there's room for one more slot. Confirmed
 * caller FUN_c000a9f4 (usbdc_ep_state9_handler) gates its own
 * midi_ring1_push_word call on this exact check - see file header point 3.
 * @0xc000d5d0. */
int midi_ring1_has_space(uint32_t *ctx)	/* FUN_c000d5d0 */
{
	int full;
	if (ctx[0x4a] < 0xff)		/* +0x128 */
		full = (ctx[0x4a] + 1) == (int)ctx[0x4c];	/* +0x130 */
	else
		full = ctx[0x4c] == 0;
	return !full;
}

/* midi_ring2_is_empty - plain index-equality empty check for ring 2.
 * @0xc000d600. */
int midi_ring2_is_empty(uint32_t *ctx)	/* FUN_c000d600 */
{
	return (int)ctx[0x4f] == (int)ctx[0x51];	/* +0x13c == +0x144 */
}

/* ---------------------------------------------------------------------
 * Bare extern for a wrapper reachable from midi_stream_decode_step's
 * own "run just finished, nothing left" tail - out of this file's range,
 * kept distinct from midi_hw_flush_notify above (different callee,
 * FUN_c000c2b0 -> FUN_c000c260, vs FUN_c000bf08 -> FUN_c000bedc) since
 * nothing in this pass's evidence confirms they're the same operation.
 * ------------------------------------------------------------------- */
extern void midi_hw_flush_alt(uint32_t base_sel, uint8_t arg);	/* FUN_c000c2b0 -> FUN_c000c260, out of range */

/* ===========================================================================
 * STILL OPEN
 * ===========================================================================
 * - The 4 midi_handler_slotN targets (0xc000cd9c/_cda4/_cdb0/_cdc4, all in
 *   this file's own unresolved code gap) - no disassembly access this pass.
 *   NEEDS LIVE QUERY: 0xc000cd9c-0xc000cdc4 - what do these 4 tiny routines
 *   actually do, and does slot 0 (called back into by midi_stream_decode_step)
 *   confirm the "(tag, byte) -> consumed?" signature assumed above?
 * - Which function-pointer table 0xc001f73c (midi_subsystem_init_entry's
 *   own sole reference) belongs to, and what/when calls through it - not
 *   eva_board_init_table (that one's contents are already fully accounted
 *   for elsewhere). NEEDS LIVE QUERY: 0xc001f73c.
 * - midi_usb_slot_c_unread (usbdc_reloc_base+0xe300) has no reader anywhere
 *   in this file's own range - real role unknown.
 * - midi_evt_state_idle_byte's running-status branch was initially
 *   misread as a same-index self-overwrite; re-checked directly against
 *   the raw decompile and corrected - it is really two writes at two
 *   DIFFERENT indices (old write index, then old+1), consistent with
 *   "replay the remembered status byte, let the caller's own
 *   double-dispatch store the real data byte next". Noted here as a
 *   worked example of why this pass re-checks against the raw decompile
 *   rather than trusting a first-pass reading, per crypto_at88.c's own
 *   established practice.
 * - midi_ring2_pop_copy's own NUL-check quirk (tests entry[0], not
 *   entry[n], only at 4-byte-aligned n) - preserved literally rather than
 *   "corrected" to the more obviously-intended entry[n]; genuine firmware
 *   behavior or decompiler artifact not resolved this pass.
 * - midi_stream_len_table/midi_cin_table's real byte contents - this
 *   static dump zeroes all such tables (same limitation clcdc_font_table
 *   already documents). NEEDS LIVE QUERY: 0xc001f63c (16 bytes) and
 *   0xc001f64c (16 bytes).
 * - Exact physical identity of the 0x62000000 external device (EMIFA CS3
 *   attribution is general DA850/OMAP-L1x memory-map knowledge, not
 *   independently re-verified against a datasheet this pass).
 *
 * CROSS-FILE FOLLOW-UP (not applied, per this pass's own file-isolation
 * rule):
 * - omap_l137_usbdc_ext.c's own `usbdc_ep_state9_handler` (FUN_c000a9f4)
 *   stub is declared there as taking no effective argument and doing
 *   nothing observable; this file's own reading of its real body (see
 *   header point 3) shows it is actually the confirmed USB-MIDI bulk-OUT
 *   receive handler, calling directly into 3 functions reconstructed here.
 *   A genuine correction opportunity for that file, left to its own owners.
 * - eva_board_main.c's own "FUN_c000ecc4/FUN_c000cdc8 ... plausibly a
 *   second, separate USB register-block setup this project hasn't traced"
 *   lead is now resolved for the FUN_c000cdc8 half (this file's own
 *   eva_board_usb_ctx_b_init) - FUN_c000ecc4 itself remains untraced,
 *   out of this file's own scope.
 * ============================================================================ */
