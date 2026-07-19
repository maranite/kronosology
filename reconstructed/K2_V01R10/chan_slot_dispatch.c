/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_slot_dispatch.c - K2 port of K1_V06R06/chan_slot_dispatch.c
 * (0xc000bedc-0xc000c39c, 14 functions there). Migrated as part of the
 * MIDI-subsystem cluster pass, 2026-07-19.
 *
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json), queried via query_dump_k2.py.
 * No live Ghidra MCP calls this pass.
 *
 * LOCATION METHOD: found by following K2's own chan_param_ctrl.c watchdog
 * cluster (FUN_c000edec, this pass's own K2 counterpart of
 * chan_link_watchdog_tick - see chan_param_ctrl.c's own K2 port) outward
 * through its externs: FUN_c000d33c (chan_link_set_mode), FUN_c000d564
 * (chan_link_ack), FUN_c000d4f4 (chan_link_tx), FUN_c000d1e0/FUN_c000d20c
 * (chan_port_signal_timeout/chan_link_timeout_notify) all led directly into
 * this cluster's own address neighborhood (0xc000d1e0-0xc000d914), where
 * every remaining K1 function found an exact-size structural match.
 *
 * HEADLINE FINDING: identical to chan_link_hw.c's own K2 port - this whole
 * file is essentially UNCHANGED between K1 and K2. All 14 K1 functions have
 * a confirmed K2 counterpart, EVERY ONE at an identical Ghidra-reported
 * size, including K1's own two documented anomalies reproducing exactly:
 * chan_port_reg60_rx_pump's reg-0x60 ack write still shows only 2 visible
 * arguments in K2's decompile too (same unresolved 3rd-argument gap K1
 * flagged), and chan_port_xfer_slot_rx_pump's own Ghidra-reported size
 * (348 bytes) numerically overlaps its own successor function's start
 * address in K2 exactly as it did in K1 - the same decompiler artifact,
 * independently reproduced.
 *
 * K1 vs K2 function map (all confirmed via decompile + exact size match):
 *   chan_port_signal_timeout    K1 FUN_c000bedc (44B)  -> K2 FUN_c000d1e0 (44B)
 *   chan_link_timeout_notify    K1 FUN_c000bf08 (8B)   -> K2 FUN_c000d20c (8B)
 *   chan_port_reset_reply_len   K1 FUN_c000bf10 (52B)  -> K2 FUN_c000d214 (52B)
 *   chan_port_reg60_rx_pump     K1 FUN_c000bf54 (224B) -> K2 FUN_c000d258 (224B)
 *   chan_link_set_mode          K1 FUN_c000c038 (92B)  -> K2 FUN_c000d33c (92B)
 *   chan_port_slot_notify       K1 FUN_c000c094 (152B) -> K2 FUN_c000d398 (152B)
 *   chan_port_tx_stage_and_pump K1 FUN_c000c158 (164B) -> K2 FUN_c000d45c (164B)
 *   chan_port_xfer_slot_tx_pump K1 FUN_c000c168 (104B) -> K2 FUN_c000d46c (104B)
 *   chan_link_tx_oob            K1 FUN_c000c1d0 (32B)  -> K2 FUN_c000d4d4 (32B)
 *   chan_link_tx                K1 FUN_c000c1f0 (60B)  -> K2 FUN_c000d4f4 (60B)
 *   chan_port_xfer_slot_init    K1 FUN_c000c22c (52B)  -> K2 FUN_c000d530 (52B)
 *   chan_link_ack               K1 FUN_c000c260 (80B)  -> K2 FUN_c000d564 (80B)
 *   chan_link_ack_retry         K1 FUN_c000c2b0 (8B)   -> K2 FUN_c000d5b4 (8B)
 *   chan_port_xfer_slot_rx_pump K1 FUN_c000c2b8 (348B) -> K2 FUN_c000d5bc (348B)
 * All 14 of K1's functions - 14/14, no gaps, no leftovers.
 *
 * "port" object offsets confirmed IDENTICAL to K1 throughout (reg_base +0x00,
 * desc_sel +0x04, ctx +0x08, tx_buf +0x0c, tx_len +0x10, scratch[64] +0x14,
 * xfer_slots +0x54, reply_len +0x58, reply_remaining +0x5c, done_flag +0x60,
 * word[0x19]/+0x64) - not re-derived field-by-field in every function's own
 * comment below, only where a real difference was found.
 *
 * REAL, CONFIRMED DIFFERENCE FROM K1: chan_port_hwctx_global (K1's
 * DAT_c000c034) and midi_hw_mode_flags (K1's own DAT_c000cc04, cited by
 * midi_engine.c-territory's midi_hw_port_enable) are TWO SEPARATE globals in
 * K1. In K2 they resolve to the EXACT SAME address, 0xC01CCD10 (this file's
 * own DAT_c000d338 == midi_engine.c-territory's own mode-flags global,
 * independently re-derived from a completely different call site) - a real,
 * address-confirmed merge of what used to be two distinct globals, not a
 * transcription artifact (verified via query_dump_k2.py dat on both
 * symbols independently).
 *
 * Two shared per-channel tables (chan_index_table_base/chan_bitmask_table_base,
 * chan_param_ctrl.c's own Part C names) confirmed via exact literal match to
 * chan_link_hw.c's own K2 citations: chan_bitmask_table_base ==
 * 0xC0027A44 (this file's own DAT_c000c454/DAT_c000c3e0, both resolve to the
 * identical literal chan_link_hw.c's own DAT_c000c880 already names) and
 * chan_index_table_base == 0xC0027A78 (this file's own DAT_c000c450/
 * DAT_c000c3dc) - 0x34 bytes after the bitmask table, a DIFFERENT gap than
 * K1's own layout but the same two-table relationship.
 *
 * STILL OPEN (same items K1 left open, not independently re-resolved this
 * pass): chan_port_xfer_slot_tx_pump (FUN_c000d46c) and
 * chan_port_xfer_slot_init (FUN_c000d530) both have ZERO static callers in
 * K2 too - the same "plausibly vtable-dispatched" situation K1 documented;
 * the "xfer" slot's own +0x18 field relationship to +0x08 not re-examined;
 * FUN_c000d6a0 (0xc000d6a0, 1836 bytes, the function immediately after this
 * cluster) is confirmed via direct decompile to be K2's own port
 * interrupt-status dispatcher - the exact K2 counterpart of K1's own
 * FUN_c000c39c cross-file finding, calling straight into
 * chan_port_reset_reply_len/_reg60_rx_pump/_slot_notify/_xfer_slot_rx_pump
 * here plus chan_param_ctrl.c's own Part C apply-handlers - NOT
 * reconstructed here (out of this file's own K1-derived scope, same
 * boundary K1 itself drew for FUN_c000c39c).
 */

#include <stdint.h>
#include <stdbool.h>

/* ---- shared low-level hardware primitives, soc_irq_gate.c's own CLUSTER 10 ---- */
extern void     midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val);	/* FUN_c000099c, soc_irq_gate.c (K1: FUN_c0000c38) */
extern uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off);			/* FUN_c00009d0, soc_irq_gate.c (K1: FUN_c0000c6c) */
extern void     midi_hw_fifo_write(uint32_t base_sel, int channel, const uint16_t *data, uint32_t len);	/* FUN_c00009fc, soc_irq_gate.c (K1: FUN_c0000c98/chan_hw_fifo_write) */
extern uint32_t midi_hw_fifo_read(uint32_t base_sel, int channel, uint8_t *out, uint32_t len);		/* FUN_c0000b50, soc_irq_gate.c (K1: FUN_c0000dec/chan_hw_fifo_read) */
extern uint32_t usbdc_min_u32(uint32_t a, uint32_t b);		/* FUN_c000c1ec (K1: FUN_c000aee8) - clamp helper, omap_l137_usbdc_ep0.c territory */

/* chan_decode_result_get - chan_link_hw.c's own K2 port, defined there. */
extern uint32_t chan_decode_result_get(uint32_t *arr, uint32_t idx);	/* FUN_c000c934, chan_link_hw.c (K1: FUN_c000b630) */

/* ---- chan_param_ctrl.c's own Part B accessor, defined there ---- */
extern uint8_t  chan_desc_dispatch_enabled(void *desc);	/* FUN_c000f17c, chan_param_ctrl.c (K1: FUN_c000dd90) */

/* ---- the two shared per-channel tables, re-declared here per this
 * project's own per-file extern-redeclaration convention ---- */
extern uint8_t *chan_index_table_base;		/* DAT_c000c450/DAT_c000c3dc here -> 0xC0027A78 (K1: 0xc001f6c4) */
extern uint8_t *chan_bitmask_table_base;	/* DAT_c000c454/DAT_c000c3e0 here -> 0xC0027A44 (K1: 0xc001f690), SAME literal chan_link_hw.c's own DAT_c000c880 resolves to */

/* ---- out-of-range EP0/link-layer forwarding calls (K1's own
 * FUN_c000e924 family; not reconstructed there either, this file's own
 * scope boundary matches K1's) ---- */
extern uint16_t chan_port_ep_read_setup_len(uint32_t ctx);			/* FUN_c000fd0c (K1: FUN_c000e924) */
extern void      chan_port_ctx_notify_a(uint32_t ctx, void *buf, uint32_t len);	/* FUN_c000ff90 (K1: FUN_c000eba8) */
extern void      chan_port_ctx_notify_b(uint32_t ctx, uint32_t len);			/* FUN_c000fb30 (K1: FUN_c000e748) */

/* module-scope global, a pointer variable. REAL K2 DIFFERENCE FROM K1: this
 * resolves to the SAME address (0xC01CCD10) as midi_engine.c-territory's
 * own midi_hw_mode_flags global - two distinct K1 globals merged into one
 * in K2, see file header. */
extern void *chan_port_hwctx_global;	/* DAT_c000d338 -> 0xC01CCD10 */

/* ===========================================================================
 * chan_link_timeout_notify / chan_port_signal_timeout
 * ===========================================================================
 */

/* chan_port_signal_timeout - FUN_c000d1e0, @0xc000d1e0 (44 bytes, K1:
 * FUN_c000bedc, 44 bytes). Identical: writes 2 into reg 0x60 (subcode 0) or
 * reg (subcode*0x20+0x68) (subcode != 0). */
static void chan_port_signal_timeout(void *port, uint32_t subcode)	/* FUN_c000d1e0 */
{
	uint32_t reg_base = *(uint32_t *)port;
	uint32_t reg_off = 0x60;

	if ((subcode & 0xff) != 0)
		reg_off = (subcode & 0xff) * 0x20 + 0x68;

	midi_hw_write16(reg_base, reg_off, 2);
}

/* chan_link_timeout_notify - FUN_c000d20c, @0xc000d20c (8 bytes, K1:
 * FUN_c000bf08, 8 bytes). Thin wrapper. Confirmed K2 callers: chan_param_ctrl.c's
 * own chan_link_watchdog_tick counterpart (FUN_c000edec) and FUN_c000e15c
 * (chan_link_teardown/midi_context_reset counterpart, chan_param_ctrl.c). */
void chan_link_timeout_notify(uint32_t target, uint8_t subcode)	/* FUN_c000d20c */
{
	chan_port_signal_timeout((void *)(uintptr_t)target, subcode);
}

/* chan_port_reset_reply_len - FUN_c000d214, @0xc000d214 (52 bytes, K1:
 * FUN_c000bf10, 52 bytes). Identical: clears done_flag (+0x60), re-reads
 * SETUP/reply length via chan_port_ep_read_setup_len(ctx), stores into both
 * +0x58 and +0x5c. Sole K2 caller: FUN_c000d6a0 (the port interrupt
 * dispatcher, out of this file's own scope - see file header). */
void chan_port_reset_reply_len(void *port)	/* FUN_c000d214 */
{
	uint8_t *P = (uint8_t *)port;
	uint16_t len;

	*(uint32_t *)(P + 0x60) = 0;
	len = chan_port_ep_read_setup_len(*(uint32_t *)(P + 8));
	*(uint32_t *)(P + 0x5c) = len;
	*(uint32_t *)(P + 0x58) = len;
}

/* chan_port_reg60_rx_pump - FUN_c000d258, @0xc000d258 (224 bytes, K1:
 * FUN_c000bf54, 224 bytes). Identical control flow to K1, including the
 * SAME unresolved-3rd-argument anomaly: the reg-0x60 ack write inside the
 * `status60 & 0x200` branch is a genuinely 2-visible-argument call in K2's
 * own decompile too (`FUN_c000099c(*param_1,0x60)`, no 3rd/value argument
 * shown) - reproduced here exactly as K1 modeled it (writing status60
 * itself as a plausible write-1-to-clear ack), NOT independently confirmed
 * by raw disassembly this pass either. Sole K2 caller: FUN_c000d6a0 (out of
 * this file's own scope). @0xc000d258. */
void chan_port_reg60_rx_pump(void *port)	/* FUN_c000d258 */
{
	uint8_t *P = (uint8_t *)port;
	uint32_t reg_base = *(uint32_t *)P;
	uint16_t status60;
	uint16_t raw_count;
	uint32_t clamped;
	uint32_t got;
	uint8_t *flagbyte;

	status60 = midi_hw_read16(reg_base, 0x60);
	if (status60 & 0x200)
		/* real K2 decompile: FUN_c000099c(*param_1,0x60) - only TWO visible
		 * args, same anomaly K1's own FUN_c000bf54 showed. Written here as
		 * status60 itself, NOT confirmed - see comment above. */
		midi_hw_write16(reg_base, 0x60, status60);

	raw_count = midi_hw_read16(reg_base, 0x68);
	clamped = usbdc_min_u32(raw_count & 0xffff, 0x40);
	got = midi_hw_fifo_read(reg_base, 0, P + 0x14, clamped);

	flagbyte = (uint8_t *)chan_port_hwctx_global;

	if ((flagbyte[1] & 0x20) == 0) {
		if (*(uint32_t *)(P + 0x58) != 0 && *(uint32_t *)(P + 0x60) == 0)
			chan_port_ctx_notify_a(*(uint32_t *)(P + 8), P + 0x14, *(uint32_t *)(P + 0x58) & 0xffff);
	} else {
		chan_port_ctx_notify_b(*(uint32_t *)(P + 8), got & 0xffff);
	}

	if ((flagbyte[1] & 0x20) == 0) {
		if ((raw_count & 0xffff) == got)
			midi_hw_write16(reg_base, 0x60, 0x80);
		return;
	}
	flagbyte[1] &= 0xdf;
}

/* ===========================================================================
 * chan_link_set_mode / chan_link_ack and the callback-vtable dispatch.
 * ===========================================================================
 */

/* chan_link_set_mode - FUN_c000d33c, @0xc000d33c (92 bytes, K1: FUN_c000c038,
 * 92 bytes). Identical reg-0x0e bit clears. 4 confirmed K2 callers
 * (FUN_c000a308 - the K2 counterpart of K1's master_dispatch_tick's own
 * "USB-adjacent cluster", wire_dispatch.c territory; FUN_c000edec - the
 * watchdog; FUN_c000d398/_d5bc - chan_port_slot_notify/_xfer_slot_rx_pump
 * below). */
void chan_link_set_mode(uint32_t target, uint32_t mode)	/* FUN_c000d33c */
{
	uint32_t reg_base = *(uint32_t *)(uintptr_t)target;
	uint16_t r0e = midi_hw_read16(reg_base, 0x0e);

	if ((mode & 0xff) == 4)
		r0e &= ~0x0100;
	else if ((mode & 0xff) == 3)
		r0e &= ~0x0080;

	midi_hw_write16(reg_base, 0x0e, r0e);
}

/* chan_port_slot_notify - FUN_c000d398, @0xc000d398 (152 bytes, K1:
 * FUN_c000c094, 152 bytes). Identical: clears mode bit for `code`, if
 * dispatch-enabled looks up chan_bitmask_table_base[chan*8+4] ("hi" half),
 * decodes to 0-7 slot, calls the matching slot's +0x14 zero-arg function
 * pointer unless 7 (disabled). Sole K2 caller: FUN_c000d6a0 (out of this
 * file's own scope). */
void chan_port_slot_notify(void *port, uint32_t code)	/* FUN_c000d398 */
{
	uint8_t *P = (uint8_t *)port;
	void **desc_sel;
	uint8_t *desc;
	uint8_t chan;
	uint32_t slot;

	chan_link_set_mode((uintptr_t)port, code & 0xff);

	desc_sel = *(void ***)(P + 4);
	desc = (uint8_t *)*desc_sel;
	if (!chan_desc_dispatch_enabled(desc))
		return;

	chan = desc[9];
	slot = (*(uint8_t **)(chan_bitmask_table_base + (uint32_t)chan * 8 + 4))[(code & 0xff) * 4];
	slot = (slot >> 1) & 7;
	if (slot == 7)
		return;

	{
		uint8_t *table = *(uint8_t **)(chan_index_table_base + (uint32_t)chan * 0x44 + 0x28);
		uint8_t *slot_obj = table + slot * 0x20;
		void (*cb)(void) = *(void (**)(void))(slot_obj + 0x14);
		cb();
	}
}

/* chan_port_tx_stage_and_pump - FUN_c000d45c, @0xc000d45c (164 bytes, K1:
 * FUN_c000c158, 164 bytes). Identical stage/clamp/pump/done-flag logic.
 * Sole K2 caller: chan_link_tx's own subcode==0 path, below. */
static void chan_port_tx_stage_and_pump(void *port, void *buf, uint32_t len, uint32_t flag)	/* FUN_c000d45c */
{
	uint8_t *P = (uint8_t *)port;
	uint32_t reg_base;
	uint16_t status60;
	uint32_t clamped;
	uint32_t remaining_before;

	*(uint32_t *)(P + 0x64) = flag;
	*(uint32_t *)(P + 0x10) = len;
	*(uint32_t *)(P + 0x0c) = (uint32_t)(uintptr_t)buf;

	reg_base = *(uint32_t *)P;
	status60 = midi_hw_read16(reg_base, 0x60);
	if ((status60 & 0x200) == 0)
		midi_hw_write16(reg_base, 0x60, 0x200);

	clamped = usbdc_min_u32(*(uint32_t *)(P + 0x10), 0x40);
	midi_hw_fifo_write(reg_base, 0, (const uint16_t *)(uintptr_t)*(uint32_t *)(P + 0x0c), clamped);

	remaining_before = *(uint32_t *)(P + 0x10);
	*(uint32_t *)(P + 0x0c) += clamped;
	*(uint32_t *)(P + 0x10) = remaining_before - clamped;

	if (clamped > 0x3f) {
		if (*(uint32_t *)(P + 0x64) != 0)
			return;
		if (remaining_before - clamped != 0)
			return;
	}
	*(uint32_t *)(P + 0x60) = 1;
}

/* chan_port_xfer_slot_tx_pump - FUN_c000d46c, @0xc000d46c (104 bytes, K1:
 * FUN_c000c168, 104 bytes). Identical per-slot analogue of
 * chan_port_tx_stage_and_pump above. ZERO STATIC CALLERS in K2 too - same
 * "plausibly vtable-dispatched" situation K1 documented, genuinely open. */
bool chan_port_xfer_slot_tx_pump(void *port, uint8_t slot_idx)	/* FUN_c000d46c */
{
	uint8_t *P = (uint8_t *)port;
	uint8_t *slot = (uint8_t *)(uintptr_t)chan_decode_result_get((uint32_t *)*(void **)(P + 0x54), slot_idx);
	uint32_t remaining = *(uint32_t *)(slot + 0x14);
	uint32_t cursor = *(uint32_t *)(slot + 0x10);
	uint32_t clamped = usbdc_min_u32(remaining, *(uint32_t *)(slot + 4));
	uint32_t remaining_after = remaining - clamped;

	*(uint32_t *)(slot + 0x10) = cursor + clamped;
	midi_hw_fifo_write(*(uint32_t *)P, slot_idx, (const uint16_t *)(uintptr_t)cursor, clamped);
	*(uint32_t *)(slot + 0x14) = remaining_after;

	return remaining_after == 0;
}

/* chan_link_tx_oob - FUN_c000d4d4, @0xc000d4d4 (32 bytes, K1: FUN_c000c1d0,
 * 32 bytes). Identical: sends `subcode` as a single OOB byte via
 * midi_hw_fifo_write's own 2-argument call form, always reports success. */
static uint32_t chan_link_tx_oob(void *port, uint8_t subcode)	/* FUN_c000d4d4 */
{
	midi_hw_fifo_write(*(uint32_t *)port, subcode, 0, 0);
	return 1;
}

/* chan_link_tx - FUN_c000d4f4, @0xc000d4f4 (60 bytes, K1: FUN_c000c1f0,
 * 60 bytes). Identical dispatch: subcode==0 queues through
 * chan_port_tx_stage_and_pump, subcode!=0 sends via chan_link_tx_oob. Same
 * RETURN-TYPE DISCREPANCY K1 documented (chan_param_ctrl.c's own extern
 * declares this void - every real K2 call site there discards the result
 * too, harmless). 12 confirmed K2 callers (vs K1's own smaller set - more
 * callers overall in K2, same role). */
uint32_t chan_link_tx(uint32_t target, uint8_t subcode, const void *buf, uint32_t len)	/* FUN_c000d4f4 */
{
	if (subcode == 0) {
		chan_port_tx_stage_and_pump((void *)(uintptr_t)target, (void *)(uintptr_t)buf, len, 0);
		return 1;
	}
	return chan_link_tx_oob((void *)(uintptr_t)target, subcode);
}

/* chan_port_xfer_slot_init - FUN_c000d530, @0xc000d530 (52 bytes, K1:
 * FUN_c000c22c, 52 bytes). Identical: seeds both the mutable cursor/
 * remaining pair AND the preserved orig_buf/orig_len pair from (buf, len).
 * ZERO STATIC CALLERS in K2 too, same as K1. */
void chan_port_xfer_slot_init(void *port, uint8_t slot_idx, void *buf, uint32_t len)	/* FUN_c000d530 */
{
	uint8_t *P = (uint8_t *)port;
	uint8_t *slot = (uint8_t *)(uintptr_t)chan_decode_result_get((uint32_t *)*(void **)(P + 0x54), slot_idx);

	*(uint32_t *)(slot + 0x14) = len;
	*(uint32_t *)(slot + 0x10) = (uint32_t)(uintptr_t)buf;
	*(uint32_t *)(slot + 0x08) = (uint32_t)(uintptr_t)buf;
	*(uint32_t *)(slot + 0x0c) = len;
}

/* chan_link_ack - FUN_c000d564, @0xc000d564 (80 bytes, K1: FUN_c000c260,
 * 80 bytes). Set-bits mirror of chan_link_set_mode above. 6 confirmed K2
 * callers (more than K1's own set - same role, wider K2 reuse). */
void chan_link_ack(uint32_t target, uint8_t code)	/* FUN_c000d564 */
{
	uint32_t reg_base = *(uint32_t *)(uintptr_t)target;
	uint16_t r0e = midi_hw_read16(reg_base, 0x0e);

	if (code == 4)
		r0e |= 0x0100;
	else if (code == 3)
		r0e |= 0x0080;

	midi_hw_write16(reg_base, 0x0e, r0e);
}

/* chan_link_ack_retry - FUN_c000d5b4, @0xc000d5b4 (8 bytes, K1: FUN_c000c2b0,
 * 8 bytes). Thin wrapper over chan_link_ack. */
void chan_link_ack_retry(uint32_t target, uint8_t code)	/* FUN_c000d5b4 */
{
	chan_link_ack(target, code);
}

/* chan_port_xfer_slot_rx_pump - FUN_c000d5bc, @0xc000d5bc (348 bytes, K1:
 * FUN_c000c2b8 - ALSO 348 bytes, reproducing K1's own documented size-field
 * anomaly: this function's own Ghidra-reported size overlaps the START of
 * its own real successor function (FUN_c000d6a0, confirmed the port
 * interrupt dispatcher via direct decompile, see file header) by the SAME
 * kind of margin K1's own file flagged, NOT trusted as this function's true
 * extent - the decompiled body below is self-contained and well-formed).
 * Identical two-shape (remaining==0 "not yet started" vs remaining!=0
 * "continuing") logic, same "lo" half bitmask lookup and +0x10 callback
 * invocation as K1. Sole K2 caller: FUN_c000d6a0 (out of this file's own
 * scope). @0xc000d5bc. */
void chan_port_xfer_slot_rx_pump(void *port, uint32_t chan)	/* FUN_c000d5bc */
{
	uint8_t *P = (uint8_t *)port;
	uint8_t *slot;
	uint32_t reg_base;
	uint16_t avail;
	uint32_t clamped;
	uint32_t remaining;
	void **desc_sel;
	uint8_t *desc;
	uint8_t d_chan;
	uint32_t cbslot;
	uint32_t cb_len;
	void *cb_buf;

	chan &= 0xff;
	chan_link_set_mode((uintptr_t)port, chan);

	slot = (uint8_t *)(uintptr_t)chan_decode_result_get((uint32_t *)*(void **)(P + 0x54), chan);
	remaining = *(uint32_t *)(slot + 0x14);

	reg_base = *(uint32_t *)P;
	avail = midi_hw_read16(reg_base, chan * 0x20 + 0x6e);
	clamped = (avail & 0xffff) <= *(uint32_t *)(slot + 4) ? (avail & 0xffff) : *(uint32_t *)(slot + 4);

	if (remaining == 0) {
		void *start_buf = (void *)(uintptr_t)*(uint32_t *)(slot + 0x18);

		*(uint32_t *)(slot + 0x10) = (uint32_t)(uintptr_t)start_buf;
		cb_len = midi_hw_fifo_read(reg_base, chan, start_buf, clamped);
		cb_buf = start_buf;

		desc_sel = *(void ***)(P + 4);
	} else {
		void *cursor = (void *)(uintptr_t)*(uint32_t *)(slot + 0x10);
		uint32_t new_remaining;

		midi_hw_fifo_read(reg_base, chan, cursor, clamped);
		new_remaining = (remaining <= clamped) ? 0 : (remaining - clamped);
		*(uint32_t *)(slot + 0x10) = (uint32_t)(uintptr_t)cursor + clamped;
		*(uint32_t *)(slot + 0x14) = new_remaining;

		if (new_remaining != 0) {
			chan_link_ack_retry((uintptr_t)port, chan);
			return;
		}

		desc_sel = *(void ***)(P + 4);
		cb_len = *(uint32_t *)(slot + 0x0c);
		cb_buf = (void *)(uintptr_t)*(uint32_t *)(slot + 0x08);
	}

	desc = (uint8_t *)*desc_sel;
	if (!chan_desc_dispatch_enabled(desc))
		return;

	d_chan = desc[9];
	cbslot = (*(uint8_t **)(chan_bitmask_table_base + (uint32_t)d_chan * 8))[chan * 4];
	cbslot = (cbslot >> 1) & 7;
	if (cbslot != 7) {
		uint8_t *table = *(uint8_t **)(chan_index_table_base + (uint32_t)d_chan * 0x44 + 0x28);
		uint8_t *slot_obj = table + cbslot * 0x20;
		void (*cb)(void *, uint32_t) = *(void (**)(void *, uint32_t))(slot_obj + 0x10);
		cb(cb_buf, cb_len);
	}
}
