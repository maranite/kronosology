/* SPDX-License-Identifier: GPL-2.0 */
/*
 * midi_engine.c - K2 port of K1_V06R06/midi_engine.c (0xc000ca50-0xc000d6fc,
 * 26 functions there: USB-MIDI CIN framer, per-cable event state machine,
 * two ring buffers, hardware bring-up/status layer). Migrated as part of the
 * MIDI-subsystem cluster pass, 2026-07-19.
 *
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json), queried via query_dump_k2.py.
 * No live Ghidra MCP calls this pass.
 *
 * LOCATION METHOD: soc_irq_gate.c's own CLUSTER 10 (midi_hw_write16/_read16/
 * _fifo_write/_fifo_read) is this project's own already-committed anchor for
 * the whole MIDI hardware layer (K1's own caller-count evidence - 130/53 -
 * carries over exactly). This pass followed that cluster's own xrefs_to
 * outward through chan_link_hw.c/chan_slot_dispatch.c/chan_param_ctrl.c's
 * own K2 ports (already committed earlier in this same pass) into the
 * remaining hardware leaves (midi_hw_reg6a_write/_port_enable/_is_enabled/
 * _reg70_bit3/_channel_active/_ready) and, via chan_param_ctrl.c's own
 * FUN_c000e984 (chan_link_rx_apply_pair)/FUN_c000e750 (chan_link_tx_step)/
 * FUN_c000e2cc (chan_link_uart_pump) cross-file aliases, into the entire
 * event-state-machine and ring-buffer cluster below.
 *
 * HEADLINE FINDING: same as every other file in this pass - the whole
 * engine is essentially UNCHANGED between K1 and K2, just at new addresses.
 * 24 of K1's 26 functions have a confirmed K2 counterpart (14 of them at an
 * EXACT Ghidra-reported byte size). The remaining 2 (midi_hw_flush_notify/
 * midi_hw_flush_alt, both already out-of-range/address-cited-only in K1's
 * own file) were not independently re-traced this pass - not needed for any
 * function actually reconstructed here.
 *
 * K1 vs K2 function map:
 *   midi_hw_reg6a_write         K1 FUN_c000ca50 (n/a)  -> K2 FUN_c000dd54 (16B)
 *   midi_hw_port_enable         K1 FUN_c000ca60 (n/a)  -> K2 FUN_c000dd64 (420B)
 *   midi_hw_is_enabled          K1 FUN_c000cc14 (n/a)  -> K2 FUN_c000df18 (40B)
 *   midi_hw_reg70_bit3          K1 FUN_c000cc3c (n/a)  -> K2 FUN_c000df40 (36B)
 *   midi_hw_channel_active      K1 FUN_c000cc60 (n/a)  -> K2 FUN_c000df64 (48B)
 *   midi_hw_channel_ready       K1 FUN_c000cc94 (n/a)  -> K2 FUN_c000df98 (56B)
 *   midi_subsystem_init_entry   K1 FUN_c000cd18 (56B)  -> K2 FUN_c000e01c (56B)
 *   midi_context_hw_init        K1 FUN_c000763c (n/a)  -> K2 FUN_c0008cc4 (200B)
 *   midi_handler_slot0_install  K1 FUN_c000cf20 (12B)  -> K2 FUN_c000e224 (12B)
 *   midi_handler_slot1_install  K1 FUN_c000cf30 (12B)  -> K2 FUN_c000e234 (12B)
 *   midi_handler_slot2_install  K1 FUN_c000cf40 (12B)  -> K2 FUN_c000e244 (12B)
 *   midi_handler_slot3_install  K1 FUN_c000cf50 (12B)  -> K2 FUN_c000e254 (12B)
 *   eva_board_usb_ctx_b_init    K1 FUN_c000cdc8 (n/a)  -> K2 FUN_c000e0cc (124B)
 *   midi_context_reset          K1 FUN_c000ce58 (n/a)  -> K2 FUN_c000e15c (112B, chan_param_ctrl.c's own chan_link_teardown alias)
 *   midi_stream_decode_step     K1 FUN_c000cfc8 (n/a)  -> K2 FUN_c000e2cc (240B, chan_param_ctrl.c's own chan_link_uart_pump alias)
 *   midi_ring2_headroom_ok      K1 FUN_c000d0c0 (n/a)  -> K2 FUN_c000e3c4 (60B)
 *   midi_ring2_pop_copy         K1 FUN_c000d0fc (n/a)  -> K2 FUN_c000e400 (172B)
 *   midi_evt_record_reset       K1 FUN_c000d1b0 (n/a)  -> K2 FUN_c000e4b0 (32B)
 *   midi_evt_state_idle_byte    K1 FUN_c000d1d0 (n/a)  -> K2 FUN_c000e4d0 (428B)
 *   midi_evt_state_fill_data    K1 FUN_c000d380 (n/a)  -> K2 FUN_c000e680 (52B)
 *   midi_evt_state_sysex_byte   K1 FUN_c000d3b4 (n/a)  -> K2 FUN_c000e6b4 (156B)
 *   midi_event_push_byte        K1 FUN_c000d618 (n/a)  -> K2 FUN_c000e984 (228B, chan_param_ctrl.c's own chan_link_rx_apply_pair alias)
 *   midi_ring1_push_word        K1 FUN_c000d450 (n/a)  -> K2 FUN_c000e750 (120B, chan_param_ctrl.c's own chan_link_tx_step alias)
 *   midi_ring2_push_word        K1 FUN_c000d4cc (n/a)  -> K2 FUN_c000e838 (152B)
 *   midi_ring1_drain_submit     K1 FUN_c000d568 (n/a)  -> K2 FUN_c000e8d4 (100B)
 *   midi_ring1_has_space        K1 FUN_c000d5d0 (n/a)  -> K2 FUN_c000e93c (48B)
 *   midi_ring2_is_empty         K1 FUN_c000d600 (n/a)  -> K2 FUN_c000e96c (24B)
 *   midi_hw_flush_alt           K1 FUN_c000??? (n/a, out of range in K1 too) -> K2 FUN_c000d5b4 (n/a) - RESOLVED, see below
 * midi_hw_flush_notify - NOT independently re-traced this pass (see
 * "2026-07-19 follow-on live pass" section below for what WAS resolved and
 * why the sibling remains open).
 *
 * REAL, CONFIRMED NEW FUNCTION, no K1 counterpart: FUN_c000e7cc (104 bytes),
 * sitting immediately after midi_ring1_push_word in the same address
 * cluster and sharing its exact same ring1 field layout (+300 byte cursor,
 * +0x128 slot index, +0x10 record size) - a "push N zero-filled words into
 * ring 1" loop primitive with no analogue anywhere in K1's own
 * midi_engine.c. Confirmed real (not a misattribution): its own sole caller
 * is FUN_c000bcf8 (conditional call, out of this file's own scope) -
 * genuinely new K2 functionality layered on the otherwise-unchanged ring
 * mechanism, reconstructed here as midi_ring1_push_zeros.
 *
 * REAL, CONFIRMED DIFFERENCE: midi_ring2_push_word's own irq-guard pair
 * resolves to FUN_c0004f40/FUN_c0004f50 - the SAME functions crypto_at88.c's
 * own K2 port already names irq_save_and_disable (FUN_c0004f40, K1:
 * FUN_c0005500) - re-declared locally here per this project's own per-file
 * extern-redeclaration convention. irq_restore's own K2 address
 * (FUN_c0004f50, K1: FUN_c0005510) had not been named anywhere else in this
 * tree before this pass; named here for the first time.
 *
 * POSSIBLE CROSS-FILE LEAD, NOT CONFIRMED: midi_cin_table (this file's own
 * DAT_c000e67c -> 0xC00279F0) resolves to the EXACT SAME literal address as
 * chan_link_hw.c's own chan_table2_base (its own DAT_c000c930, also
 * 0xC00279F0) - that file's own "genuinely unresolved which table this is"
 * open item. The two tables are used very differently (midi_cin_table as a
 * flat 16-byte array indexed by status nibble; chan_table2_base as a
 * pointer-to-word-chain table indexed by channel), so this is flagged as a
 * real address coincidence worth a future look, NOT asserted as proof
 * they're the same table - could equally be two adjacent/overlapping static
 * data objects in the same rodata neighborhood. NEEDS LIVE QUERY if a future
 * pass wants to resolve this definitively.
 *
 * Every other struct offset (per-cable record layout, ring 1/2 field
 * offsets, context singleton field offsets) confirmed IDENTICAL to K1
 * throughout - not re-derived field-by-field in each function's own comment
 * below except where a real difference was found.
 *
 * ===========================================================================
 * 2026-07-19 follow-on live pass (dedicated single-session live Ghidra MCP
 * bridge, same "2-agent cap, no further fan-out" authorization as
 * usbdc_midi_status_glue.c's own second pass this same day):
 *
 * midi_hw_flush_alt RESOLVED - get_disassembly on midi_stream_decode_step's
 * own tail (the "maybe_flush" label, 0xc000e3ac-0xc000e3b8) shows the real
 * `bl` target for the call this file already reconstructed as
 * `midi_hw_flush_alt(param_1[0], *(uint8_t *)(param_1 + 1))`: 0xc000d5b4.
 * decompile_function on that address returns a ONE-LINE body -
 * `FUN_c000d5b4(a,b) { FUN_c000d564(a,b); return; }` - i.e. midi_hw_flush_alt
 * is a bare, unconditional thin forwarder to chan_link_ack
 * (chan_param_ctrl.c's own FUN_c000d564), same two arguments, no added
 * logic of its own. A real, confirmed finding, not a coverage gap: whatever
 * independent "flush" behavior K1's own midi_hw_flush_alt may have had
 * (never decompiled by K1's own pass either - it was address-cited-only
 * there too), K2's counterpart has been reduced to pure delegation.
 *
 * midi_hw_flush_notify NOT RESOLVED - get_xrefs_to on chan_link_ack's own
 * K2 address (0xc000d564) returns exactly 6 real callers: chan_link_hw.c's
 * chan_link_watchdog_tick (FUN_c000edec, already documented there),
 * FUN_c000bcf8 (midi_ring1_push_zeros's own known caller, see that
 * function's note above), chan_link_rt_queue_push
 * (FUN_c000ea68, chan_param_ctrl.c, resolved the same day), midi_hw_flush_alt
 * itself (FUN_c000d5b4), and two NOT independently identified this pass -
 * FUN_c000a308 and FUN_c000ef8c. Neither of those two is a bare one-line
 * forwarder shaped like midi_hw_flush_alt (both have real callers of their
 * own per the same xrefs sweep, i.e. they are not themselves thin aliases),
 * so there is no positive evidence either one IS midi_hw_flush_notify -
 * flagged as open rather than guessed. NEEDS LIVE QUERY: decompile
 * FUN_c000a308/FUN_c000ef8c directly if a future pass wants to settle this;
 * neither was attempted this pass (out of this pass's own time budget,
 * chan_param_ctrl.c's own chan_link_rt_queue_push task took priority).
 * ===========================================================================
 */

#include <stdint.h>

/* ---------------------------------------------------------------------
 * Hardware register access primitives - soc_irq_gate.c's own CLUSTER 10.
 * --------------------------------------------------------------------- */
extern void     midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val);	/* FUN_c000099c, soc_irq_gate.c (K1: FUN_c0000c38) */
extern uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off);			/* FUN_c00009d0, soc_irq_gate.c (K1: FUN_c0000c6c) */

/* irq_save_and_disable - crypto_at88.c's own K2 port (FUN_c0004f40, K1:
 * FUN_c0005500). irq_restore (FUN_c0004f50, K1: FUN_c0005510) named here
 * for the first time in this tree. */
extern int  irq_save_and_disable(void);	/* FUN_c0004f40, crypto_at88.c */
extern void irq_restore(void);			/* FUN_c0004f50 */

/* midi_hw_reg6a_write - FUN_c000dd54, @0xc000dd54 (16 bytes, K1:
 * FUN_c000ca50). Sole K2 caller: FUN_c000f83c (chan_param_ctrl.c's own
 * chan_class0_apply_value - reproduces K1's own chan_class0_send_value
 * aliasing exactly). */
void midi_hw_reg6a_write(uint32_t *handle, uint8_t val)	/* FUN_c000dd54 */
{
	midi_hw_write16(*handle, 0x6a, val);
}

/* midi_hw_port_enable - FUN_c000dd64, @0xc000dd64 (420 bytes, K1:
 * FUN_c000ca60). Identical logic to K1: both enable sub-branches write the
 * same 0x82/reg2_offset/0xa2 triplet (reg2_offset DAT_c000df0c == 0x102,
 * EXACT match to K1's own literal), c2_val selected by mode-flags bit 0x40,
 * reg 0xe2=0x40, 4-port RMW loop OR'ing bit 0; disable path ANDs mask
 * DAT_c000df10 == 0xfffe (EXACT match); reg-0x32 bit-0x20 tail toggle masked
 * DAT_c000df14 == 0xffdf (EXACT match). @0xc000dd64. */
extern uint8_t  midi_hw_mode_flags;		/* DAT_c000df08 -> 0xC01CCD10 - SAME address as chan_slot_dispatch.c's own
						 * chan_port_hwctx_global and chan_param_ctrl.c's own chan_global_hi_mode_flags,
						 * a real 3-way K2 merge - see those files' own headers */
extern uint32_t midi_hw_reg2_offset;		/* DAT_c000df0c, resolved literal 0x102 (K1: 0x102, exact) */
extern uint16_t midi_hw_ch_disable_mask;	/* DAT_c000df10, resolved literal 0xfffe (K1: 0xfffe, exact) */
extern uint16_t midi_hw_enable_bit_mask;	/* DAT_c000df14, resolved literal 0xffdf (K1: 0xffdf, exact) */

int midi_hw_port_enable(uint32_t *handle, uint8_t enable)	/* FUN_c000dd64 */
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

/* midi_hw_is_enabled - FUN_c000df18, @0xc000df18 (40 bytes, K1:
 * FUN_c000cc14). Confirmed K2 caller: FUN_c000edec (chan_param_ctrl.c's own
 * chan_link_watchdog_tick, as chan_link_probe_armed - reproduces K1's own
 * aliasing exactly). @0xc000df18. */
int midi_hw_is_enabled(uint32_t *handle)	/* FUN_c000df18 */
{
	return (midi_hw_read16(*handle, 0x32) & 0x20) != 0;
}

/* midi_hw_reg70_bit3 - FUN_c000df40, @0xc000df40 (36 bytes, K1:
 * FUN_c000cc3c). Also aliased chan_class2_default_value by
 * chan_param_ctrl.c's own chan_class2_poll_and_notify - reproduces K1's own
 * aliasing exactly. @0xc000df40. */
int midi_hw_reg70_bit3(uint32_t *handle)	/* FUN_c000df40 */
{
	return (midi_hw_read16(*handle, 0x70) >> 3) & 1;
}

/* midi_hw_channel_active - FUN_c000df64, @0xc000df64 (48 bytes, K1:
 * FUN_c000cc60). Also aliased chan_link_start_handshake by
 * chan_param_ctrl.c's own chan_link_watchdog_tick - reproduces K1's own
 * aliasing exactly. Mask DAT_c000df94 == 0x1fe0 (EXACT match to K1).
 * @0xc000df64. */
extern uint16_t midi_hw_ch_status_mask;	/* DAT_c000df94 / DAT_c000dfd0, resolved literal 0x1fe0 (K1: 0x1fe0, exact) */

int midi_hw_channel_active(uint32_t *handle, int channel)	/* FUN_c000df64 */
{
	uint16_t v = midi_hw_read16(*handle, ((channel << 5) & midi_hw_ch_status_mask) + 0x74);
	return (v & 3) != 0;
}

/* midi_hw_channel_ready - FUN_c000df98, @0xc000df98 (56 bytes, K1:
 * FUN_c000cc94). Mask DAT_c000dfd0 == 0x1fe0 (EXACT match to K1). */
int midi_hw_channel_ready(uint32_t *handle, int channel)	/* FUN_c000df98 */
{
	uint16_t v = midi_hw_read16(*handle, ((channel << 5) & midi_hw_ch_status_mask) + 0x74);
	return (v & 3) == 3;
}

/* ---------------------------------------------------------------------
 * Context init / reset.
 * --------------------------------------------------------------------- */

/* midi_context_hw_init - FUN_c0008cc4, @0xc0008cc4 (200 bytes, K1:
 * FUN_c000763c). Identical to K1: ignores its own `unused_handle` (phantom
 * parameter, same pattern), operates on a FIXED global singleton
 * (DAT_c0008cdc -> 0xC01CBAB4), writes reg2/reg3/callback/record_size into
 * +4/+8/+0xc/+0x10 (record_size rounded up to a multiple of 4), installs
 * the 4 midi_handler_slotN globals, zero-inits all 16 per-cable records'
 * +0x28/+0x2b bytes and stamps +0x2c, calls midi_context_reset(singleton,0)
 * (FUN_c000e15c, chan_param_ctrl.c's own chan_link_teardown alias), clears
 * +0x54c/+0x18/+0x20. Sole K2 caller: midi_subsystem_init_entry below. */
extern void midi_context_reset(uint32_t *ctx, int notify);	/* FUN_c000e15c, chan_param_ctrl.c (K1: FUN_c000ce58, also chan_link_teardown) */
extern void midi_handler_slot0_install(void *unused, void *handler);	/* FUN_c000e224 */
extern void midi_handler_slot1_install(void *unused, void *handler);	/* FUN_c000e234 */
extern void midi_handler_slot2_install(void *unused, void *handler);	/* FUN_c000e244 */
extern void midi_handler_slot3_install(void *unused, void *handler);	/* FUN_c000e254 */
extern void *midi_ctx_singleton;	/* DAT_c0008cdc -> 0xC01CBAB4 */
/* 2026-07-19 RESOLVED (live Ghidra pass, after a full auto-analysis run
 * still didn't boundary-detect these 4 addresses on its own): manually
 * created real Function objects at all 4 slot targets via CreateFunctionCmd
 * - unlike a similar-looking cluster in omap_l108_syscfg.c, these DO have
 * real callers once bounded (all 4 resolve to a single PARAM-type xref from
 * midi_context_hw_init/FUN_c0008cc4 itself, exactly matching the
 * `midi_handler_slotN_install(ctx, midi_handler_slotN_target)` call sites
 * already documented above - the "target" globals are genuinely these
 * functions' own addresses, passed as function-pointer arguments, not a
 * stored table). All 4 are trivial default/no-op handler stubs, consistent
 * with "this MIDI cable/channel slot has no real handler installed yet". */
void *midi_handler_slot0_target(void)	/* FUN_c000e0a0, @0xc000e0a0 (8 bytes) */
{
	return (void *)1;	/* always "ready"/"true" - real decompile: `return 1;` */
}

extern void midi_hw_write16_variant(uint32_t arg);	/* FUN_c000e8d4 - a DIFFERENT function from this file's own midi_hw_write16 (FUN_c000099c) despite the similar name; single-argument forwarder, not independently decompiled here */
extern uint32_t midi_handler_slot1_arg;	/* DAT_c0008d20 - fixed argument this slot forwards */

void midi_handler_slot1_target(void)	/* FUN_c000e0a8, @0xc000e0a8 (16 bytes) */
{
	midi_hw_write16_variant(midi_handler_slot1_arg);	/* real decompile: `FUN_c000e8d4(DAT_c0008d20);` */
}

void midi_handler_slot2_target(void *out_a, void *out_b)	/* FUN_c000e0b4, @0xc000e0b4 (20 bytes) */
{
	*(uint8_t *)out_a = 0;
	*(uint8_t *)out_b = 0;
	/* real decompile returns 0 (undefined4) - modeled as void since no
	 * caller here reads its return value */
}

void midi_handler_slot3_target(void)	/* FUN_c000e0c8, @0xc000e0c8 (1 byte - a bare `mov pc,lr`) */
{
	/* pure no-op stub */
}

void midi_context_hw_init(void *unused_handle, uint8_t reg2, uint8_t reg3,
	void *callback, int record_size)	/* FUN_c0008cc4 */
{
	uint8_t *ctx = (uint8_t *)midi_ctx_singleton;
	uint32_t i;

	(void)unused_handle;
	*(uint32_t *)(ctx + 4)  = reg2 & 0xff;
	*(uint32_t *)(ctx + 8)  = reg3 & 0xff;
	*(void   **)(ctx + 0xc) = callback;
	*(uint32_t *)(ctx + 0x10) = (record_size + 3U) & ~3U;

	midi_handler_slot0_install(ctx, midi_handler_slot0_target);
	midi_handler_slot1_install(ctx, midi_handler_slot1_target);
	midi_handler_slot2_install(ctx, midi_handler_slot2_target);
	midi_handler_slot3_install(ctx, midi_handler_slot3_target);

	for (i = 0; i < 16; i++) {
		uint8_t *rec = ctx + i * 0x10;

		rec[0x2c] = (uint8_t)(i * 0x10);
		rec[0x28] = 0;
		rec[0x2b] = 0;
	}

	midi_context_reset((uint32_t *)ctx, 0);
	*(uint32_t *)(ctx + 0x54c) = 0;
	*(uint32_t *)(ctx + 0x18) = 0;
	*(uint32_t *)(ctx + 0x20) = 0;
}

/* midi_subsystem_init_entry - FUN_c000e01c, @0xc000e01c (56 bytes, K1:
 * FUN_c000cd18, 56 bytes - EXACT match). Thin 4-argument forwarder
 * prepending the fixed dead-handle constant (DAT_c000e054 -> 0xC01CB2EC).
 * Sole K2 reference is a DATA read (0xc0027af0, per xrefs_to
 * "DATA"-classified entry) - same "installed as a function-pointer table
 * entry, not called directly" situation K1 documented, table not
 * independently identified this pass either. */
extern void *midi_dead_handle_const;	/* DAT_c000e054 -> 0xC01CB2EC */

void midi_subsystem_init_entry(uint8_t reg2, uint8_t reg3, void *callback, int record_size)	/* FUN_c000e01c */
{
	midi_context_hw_init(midi_dead_handle_const, reg2, reg3, callback, record_size);
}

/* eva_board_usb_ctx_b_init - FUN_c000e0cc, @0xc000e0cc (124 bytes, K1:
 * FUN_c000cdc8). Identical: sets ctx's +0 field to `shared`, resolves 3
 * sub-buffer pointers off the SAME usbdc_reloc_base global
 * omap_l137_usbdc.c uses (DAT_c000e148, via FUN_c000a728/omap_usbdc_reloc),
 * storing base+0x6240/+0xa240/+0xe300 - IDENTICAL offsets to K1. Confirmed
 * K2 caller: FUN_c0009838 (a bring-up sequence, out of this file's own
 * scope). @0xc000e0cc. */
extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c000a728, omap_l137_usbdc.c (K1: FUN_c0009194) */
extern uint32_t usbdc_reloc_base;		/* DAT_c000e148 */
extern uint8_t *midi_tx_slot_table;		/* DAT_c000e14c - usbdc_reloc_base + 0x6240 */
extern uint8_t *midi_cable_name_table;		/* DAT_c000e150 - usbdc_reloc_base + 0xa240 */
extern uint8_t *midi_usb_slot_c_unread;	/* DAT_c000e154 - usbdc_reloc_base + 0xe300 */
extern int      midi_ctx_tail_zero_off;	/* DAT_c000e158 */

void eva_board_usb_ctx_b_init(void **ctx, void *shared)	/* FUN_c000e0cc */
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

/* ---------------------------------------------------------------------
 * midi_stream_decode_step - FUN_c000e2cc, @0xc000e2cc (240 bytes, K1:
 * FUN_c000cfc8). Also aliased chan_link_uart_pump by chan_param_ctrl.c's
 * own chan_link_service_tick - reproduces K1's own aliasing exactly.
 * Identical nibble-tagged byte-run decoder: run counter +0x18, source
 * cursor +0x1c, length remaining +0x20, tag nibble +0x24, callback through
 * midi_handler_slot0 (DAT_c000e3c0 -> 0xC01CCD30, SAME address
 * midi_handler_slot0_install/FUN_c000e224 writes), 16-entry length table
 * (midi_stream_len_table, DAT_c000e3bc -> 0xC0027A00). @0xc000e2cc.
 * --------------------------------------------------------------------- */
extern uint8_t midi_stream_len_table[16];	/* DAT_c000e3bc -> 0xC0027A00 (K1: 0xc001f64c) */
extern void   *midi_handler_slot0;		/* DAT_c000e3c0 -> 0xC01CCD30, SAME target FUN_c000e224 installs */

/* midi_hw_flush_alt - FUN_c000d5b4, @0xc000d5b4. RESOLVED 2026-07-19 follow-
 * on live pass - see this file's own header for the get_disassembly/
 * decompile_function evidence. Real body is a bare, unconditional thin
 * forwarder to chan_link_ack - no independent flush logic of its own in K2. */
extern void chan_link_ack(uint32_t target, uint8_t code);	/* FUN_c000d564, chan_param_ctrl.c/chan_slot_dispatch.c */

void midi_hw_flush_alt(uint32_t base_sel, uint8_t arg)	/* FUN_c000d5b4 */
{
	chan_link_ack(base_sel, arg);
}

void midi_stream_decode_step(uint32_t *param_1)	/* FUN_c000e2cc */
{
	uint8_t *src;
	uint8_t *tag_byte;
	uint8_t b;
	int consumed;

	tag_byte = (uint8_t *)(param_1 + 9);

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
 * Ring 2's "headroom check" and "pop one slot, copy it out" pair.
 * --------------------------------------------------------------------- */
extern void midi_hw_set_reg_d8(uint32_t *handle, uint8_t val);	/* FUN_c000cb74, chan_link_hw.c (K1: FUN_c000b870) */

/* midi_ring2_headroom_ok - FUN_c000e3c4, @0xc000e3c4 (60 bytes, K1:
 * FUN_c000d0c0). Identical logic. */
int midi_ring2_headroom_ok(uint32_t *ctx)	/* FUN_c000e3c4 */
{
	int steps = 0;
	int cur = ctx[0x4f];

	for (;;) {
		int next = (cur > 0xfe) ? 0 : cur + 1;
		steps++;
		if (next == (int)ctx[0x51])
			break;
		cur = next;
		if (steps > 7)
			return 1;
	}
	return 0;
}

/* midi_ring2_pop_copy - FUN_c000e400, @0xc000e400 (172 bytes, K1:
 * FUN_c000d0fc). Identical, INCLUDING K1's own documented NUL-check quirk
 * (tests entry[0], not entry[n], only at 4-byte-aligned n - reproduced
 * exactly, confirmed present in K2's own decompile too). Backing store
 * midi_cable_name_table, DAT_c000e4ac -> 0xC01CCD20. */
uint32_t midi_ring2_pop_copy(uint32_t *ctx, char *out)	/* FUN_c000e400 */
{
	extern uint8_t *midi_ring2_backing_store;	/* DAT_c000e4ac -> 0xC01CCD20 */
	int read_idx = ctx[0x51];
	uint32_t copied = 0;

	if (read_idx != (int)ctx[0x4f]) {
		uint32_t remain = ctx[read_idx + 0x52];
		char *entry = (char *)(midi_ring2_backing_store + read_idx * 0x40);
		uint32_t n = 0;

		copied = remain;
		while (n < remain) {
			out[n] = entry[n];
			if ((n & 3) == 0) {
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
 * USB-MIDI event-packet state machine. Per-cable record layout confirmed
 * IDENTICAL to K1 throughout.
 * --------------------------------------------------------------------- */

/* midi_evt_record_reset - FUN_c000e4b0, @0xc000e4b0 (32 bytes, K1:
 * FUN_c000d1b0, 32 bytes - EXACT match). */
void midi_evt_record_reset(uint32_t ctx_unused, uint8_t *rec)	/* FUN_c000e4b0 */
{
	rec[1] = 0;
	rec[5] = rec[4];
	rec[6] = 0;
	rec[7] = 0;
	rec[8] = 0;
}

/* midi_evt_state_idle_byte - FUN_c000e4d0, @0xc000e4d0 (428 bytes, K1:
 * FUN_c000d1d0). Identical to K1's own state-0 handler: running-status
 * replay-at-old-index/blank-at-new-index for nibble<8, standard CIN lookup
 * for nibble 8-0xE, 0xF0 SysEx-start / 0xF1-0xF7 System Common for the rest.
 * CIN table midi_cin_table, DAT_c000e67c -> 0xC00279F0 - SAME literal
 * address as chan_link_hw.c's own chan_table2_base, see file header's own
 * "POSSIBLE CROSS-FILE LEAD" note (not asserted as proven identity). */
extern uint8_t midi_cin_table[16];	/* DAT_c000e67c -> 0xC00279F0 (K1: 0xc001f63c) */

void midi_evt_state_idle_byte(uint32_t *ctx, uint8_t *rec, uint8_t status)	/* FUN_c000e4d0 */
{
	unsigned nibble = status >> 4;

	midi_evt_record_reset(0, rec);
	if (*(uint32_t *)((uint8_t *)ctx + 0x54c) != 0) {
		*(uint32_t *)(rec + 0xc) = 1;
		*(uint32_t *)((uint8_t *)ctx + 0x134) = 0;
	}
	rec[0] = 1;

	if (nibble < 8) {
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
			rec[3] = status;
			return;
		}

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

/* midi_evt_state_fill_data - FUN_c000e680, @0xc000e680 (52 bytes, K1:
 * FUN_c000d380, 52 bytes - EXACT match). */
void midi_evt_state_fill_data(uint32_t ctx_unused, uint8_t *rec, uint8_t data)	/* FUN_c000e680 */
{
	uint8_t remain = rec[2];
	uint8_t widx = rec[1];

	rec[widx + 5] = data;
	rec[1] = widx + 1;
	rec[2] = remain - 1;
	if ((uint8_t)(remain - 1) == 0)
		rec[0] = 0;
}

/* midi_evt_state_sysex_byte - FUN_c000e6b4, @0xc000e6b4 (156 bytes, K1:
 * FUN_c000d3b4). Identical, including the EOX (-9/0xf7) close logic. */
void midi_evt_state_sysex_byte(uint32_t ctx_unused, uint8_t *rec, uint8_t data)	/* FUN_c000e6b4 */
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

/* midi_event_push_byte - FUN_c000e984, @0xc000e984 (228 bytes, K1:
 * FUN_c000d618). Also aliased chan_link_rx_apply_pair by chan_param_ctrl.c's
 * own chan_link_rx_queue_drain - reproduces K1's own aliasing exactly.
 * Identical top-level dispatch, including the double-dispatch shape (two
 * sequential `if`s, not `if/else if`) and the SysEx-mode-flag global-mode
 * path. @0xc000e984. */
extern int  midi_ring1_push_word(uint32_t *ctx, void *word4);		/* FUN_c000e750, defined below */
extern void midi_ring1_drain_submit(uint32_t *ctx);			/* FUN_c000e8d4, defined below */

int midi_event_push_byte(uint32_t *ctx, unsigned cable, uint8_t byte)	/* FUN_c000e984 */
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

		if (*(uint32_t *)(base + 0x34) == 0 || base[0x2a] != 0)
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
 * Ring buffers. Field offsets confirmed IDENTICAL to K1: ring 1 write
 * index +0x128, read index +0x130, byte cursor +300(=0x12c), record size
 * +0x10, backing store midi_tx_slot_table; ring 2 write index +0x13c, read
 * index +0x144, byte cursor +0x140, backing store midi_ring2_backing_store.
 * --------------------------------------------------------------------- */

/* midi_ring1_push_word - FUN_c000e750, @0xc000e750 (120 bytes, K1:
 * FUN_c000d450). Also aliased chan_link_tx_step by chan_param_ctrl.c's own
 * chan_link_tx_queue_drain - reproduces K1's own aliasing exactly. UNGUARDED
 * slot advance, same as K1. Backing store DAT_c000e7c8 -> 0xC01CCD24. */
int midi_ring1_push_word(uint32_t *ctx, void *word4)	/* FUN_c000e750 */
{
	extern uint8_t *midi_tx_slot_table;	/* DAT_c000e7c8 -> 0xC01CCD24 */
	int byte_off = ctx[0x4b];
	int slot = ctx[0x4a];

	((uint32_t *)midi_tx_slot_table)[(byte_off >> 2) + slot * 0x10] = *(uint32_t *)word4;
	byte_off += 4;
	ctx[0x4b] = byte_off;

	if (byte_off != (int)ctx[4])
		return 0;

	ctx[0x4a] = (slot > 0xfe) ? 0 : slot + 1;
	ctx[0x4b] = 0;
	return 1;
}

/* midi_ring1_push_zeros - FUN_c000e7cc, @0xc000e7cc (104 bytes). REAL,
 * CONFIRMED NEW K2 FUNCTION, no K1 counterpart - see file header. Pushes
 * `count` zero-filled 4-byte words into ring 1 using the exact same field
 * layout as midi_ring1_push_word above (+300 byte cursor, +0x128 slot
 * index, +0x10 record size, same backing store DAT_c000e834 ->
 * 0xC01CCD24, IDENTICAL to midi_ring1_push_word's own DAT_c000e7c8). Sole
 * K2 caller: FUN_c000bcf8 (conditional call, out of this file's own
 * scope). */
void midi_ring1_push_zeros(uint32_t *ctx, int count)	/* FUN_c000e7cc */
{
	extern uint8_t *midi_tx_slot_table_zf;	/* DAT_c000e834 -> 0xC01CCD24, SAME global as midi_ring1_push_word's own backing store */
	int byte_off, slot;

	if (count < 1)
		return;

	do {
		byte_off = ctx[0x4b];
		slot = ctx[0x4a];
		((uint32_t *)midi_tx_slot_table_zf)[(byte_off >> 2) + slot * 0x10] = 0;
		byte_off += 4;
		ctx[0x4b] = byte_off;

		if (byte_off == (int)ctx[4]) {
			ctx[0x4a] = (slot > 0xfe) ? 0 : slot + 1;
			ctx[0x4b] = 0;
		}
		count--;
	} while (count > 0);
}

/* midi_ring2_push_word - FUN_c000e838, @0xc000e838 (152 bytes, K1:
 * FUN_c000d4cc). Identical IRQ-guarded slot advance (irq_save_and_disable/
 * irq_restore, crypto_at88.c's own K2 port - see file header). Backing
 * store DAT_c000e8xx family -> 0xC01CCD20 (SAME as midi_ring2_pop_copy's
 * own backing store, three independent derivations agreeing exactly as in
 * K1). */
int midi_ring2_push_word(uint32_t *ctx, void *word4)	/* FUN_c000e838 */
{
	extern uint8_t *midi_ring2_backing_store2;	/* DAT_c000e8d0 -> 0xC01CCD20 */
	int byte_off = ctx[0x50];
	int result = 0;

	((uint32_t *)midi_ring2_backing_store2)[(byte_off >> 2) + ctx[0x4f] * 0x10] = *(uint32_t *)word4;
	byte_off += 4;
	ctx[0x50] = byte_off;

	if (byte_off == (int)ctx[4]) {
		(void)irq_save_and_disable();
		ctx[0x4f] = (ctx[0x4f] < 0xff) ? ctx[0x4f] + 1 : 0;
		irq_restore();
		result = 1;
		ctx[0x50] = 0;
	}
	return result;
}

/* midi_ring1_drain_submit - FUN_c000e8d4, @0xc000e8d4 (100 bytes, K1:
 * FUN_c000d568). Also aliased midi_hw_submit's own callee is chan_link_tx
 * (FUN_c000d4f4, chan_slot_dispatch.c - SAME address K1's own midi_hw_submit
 * used, reproducing K1's own aliasing exactly). IRQ-guarded index advance,
 * same asymmetry vs. midi_ring1_push_word's unguarded push K1 documented. */
extern uint32_t chan_link_tx(uint32_t target, uint8_t subcode, const void *buf, uint32_t len);	/* FUN_c000d4f4, chan_slot_dispatch.c */

void midi_ring1_drain_submit(uint32_t *ctx)	/* FUN_c000e8d4 */
{
	extern uint32_t midi_tx_slot_base;	/* DAT_c000e938 -> 0xC01CCD24, SAME global as midi_ring1_push_word's own */
	int slot = ctx[0x4c];
	uint32_t base = midi_tx_slot_base;

	ctx[0x154] = 1;
	(void)irq_save_and_disable();
	ctx[0x4c] = (slot < 0xff) ? slot + 1 : 0;
	irq_restore();
	ctx[0x4d] = 0;

	chan_link_tx(ctx[0], ((uint8_t *)ctx)[8], (const void *)(base + slot * 0x40), ctx[4]);
}

/* midi_ring1_has_space - FUN_c000e93c, @0xc000e93c (48 bytes, K1:
 * FUN_c000d5d0, 48 bytes - EXACT match). */
int midi_ring1_has_space(uint32_t *ctx)	/* FUN_c000e93c */
{
	int full;
	if (ctx[0x4a] < 0xff)
		full = (ctx[0x4a] + 1) == (int)ctx[0x4c];
	else
		full = ctx[0x4c] == 0;
	return !full;
}

/* midi_ring2_is_empty - FUN_c000e96c, @0xc000e96c (24 bytes, K1:
 * FUN_c000d600, 24 bytes - EXACT match). */
int midi_ring2_is_empty(uint32_t *ctx)	/* FUN_c000e96c */
{
	return (int)ctx[0x4f] == (int)ctx[0x51];
}

/* ===========================================================================
 * STILL OPEN
 * ===========================================================================
 * - midi_hw_flush_alt RESOLVED 2026-07-19 (K2 FUN_c000d5b4, a bare thin
 *   forwarder to chan_link_ack - see header). midi_hw_flush_notify remains
 *   open - no positive evidence identifies it among chan_link_ack's own 6
 *   confirmed K2 callers; two (FUN_c000a308, FUN_c000ef8c) were not
 *   independently decompiled this pass, see header for the concrete lead.
 * - The midi_cin_table / chan_table2_base address coincidence (both
 *   0xC00279F0) - flagged, not resolved, see header.
 * - midi_stream_len_table/midi_cin_table's real byte contents - zeroed in
 *   this static dump, same limitation K1 documented. NEEDS LIVE QUERY:
 *   0xC0027A00 (16 bytes), 0xC00279F0 (16 bytes).
 * - RESOLVED 2026-07-19: the 4 midi_handler_slotN install TARGETS
 *   (0xC000E0A0/_A8/_B4/_C8) - a full Ghidra auto-analysis pass still
 *   didn't boundary-detect them, but manually creating Function objects
 *   there (CreateFunctionCmd) revealed all 4 have a real, findable caller
 *   (a single PARAM-type xref each, from midi_context_hw_init itself) -
 *   unlike a similar-looking cluster in omap_l108_syscfg.c which remained
 *   genuinely caller-less even after the same treatment. All 4 are trivial
 *   default/no-op handler stubs (see their own definitions above).
 * - midi_subsystem_init_entry's own function-pointer table (0xc0027af0,
 *   this file's own DATA xref) not independently identified, same open
 *   item K1 left for its own equivalent table.
 */
