/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_param_ctrl.c - K2 port of K1_V06R06/chan_param_ctrl.c
 * (0xc000d9bc-0xc000e498, 18 functions there: PART A link watchdog/queue
 * pump, PART B one shared accessor, PART C per-channel parameter engine).
 * Migrated as part of the MIDI-subsystem cluster pass, 2026-07-19.
 *
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json), queried via query_dump_k2.py.
 * No live Ghidra MCP calls this pass.
 *
 * LOCATION METHOD: chan_link_hw.c's own K2 port (chan_link_probe_ready,
 * FUN_c000c948) has exactly one K2 caller, FUN_c000edec - the direct K2
 * counterpart of chan_link_watchdog_tick, reproducing K1's own CROSS-FILE
 * FINDING #1 exactly. Every other function below was found by walking that
 * function's own externs outward (same technique chan_slot_dispatch.c's own
 * K2 port used), then PART C was found independently via
 * chan_class2_read_value's (chan_link_hw.c) own literal-mask evidence
 * (0x1fe0, 0xffc0) leading to chan_class2_test_hi/_lo, and from there to the
 * rest of the per-channel dispatch cluster.
 *
 * HEADLINE FINDING: same as chan_link_hw.c/chan_slot_dispatch.c - this
 * cluster is essentially UNCHANGED between K1 and K2. 17 of PART A/B/C's 18
 * K1 functions have a confirmed K2 counterpart (9 of them at an EXACT
 * Ghidra-reported byte size, the rest within a few bytes - see map below).
 * PART C in particular is a near-perfect port: every one of its 9 functions
 * matches, 7 at exact byte size.
 *
 * K1 vs K2 function map:
 *   PART A:
 *   chan_link_send_reset_frame  K1 FUN_c000d9bc (76B)  -> K2 FUN_c000ed9c (76B)
 *   chan_link_watchdog_tick     K1 FUN_c000da0c (300B) -> K2 FUN_c000edec (300B)
 *   chan_link_rt_queue_push     K1 FUN_c000dbac (144B) -> K2 FUN_c000ea68 (356B) - REAL RESTRUCTURING, RESOLVED - see "REAL, CONFIRMED DIFFERENCE" below
 *   chan_link_tx_queue_drain    K1 FUN_c000dc4c (112B) -> K2 FUN_c000f038 (112B)
 *   chan_link_service_tick      K1 FUN_c000dcbc (32B)  -> K2 FUN_c000f0a8 (32B)
 *   chan_link_rx_queue_drain    K1 FUN_c000dcdc (156B) -> K2 FUN_c000f0c8 (156B)
 *   chan_link_obj_init          K1 FUN_c000dd98 (40B)  -> K2 FUN_c000f184 (40B)
 *   PART B:
 *   chan_desc_dispatch_enabled  K1 FUN_c000dd90 (8B)   -> K2 FUN_c000f17c (8B)
 *   PART C:
 *   chan_class_wire_code        K1 FUN_c000de0c (68B)  -> K2 FUN_c000f1f8 (88B)
 *   chan_class2_test_hi         K1 FUN_c000de64 (112B) -> K2 FUN_c000f250 (112B)
 *   chan_class2_test_lo         K1 FUN_c000dee0 (108B) -> K2 FUN_c000f2cc (108B)
 *   chan_desc_query_dispatch    K1 FUN_c000df58 (416B) -> K2 FUN_c000f344 (412B)
 *   chan_class2_poll_and_notify K1 FUN_c000e11c (264B) -> K2 FUN_c000f504 (264B)
 *   chan_class0_notify_flag     K1 FUN_c000e22c (96B)  -> K2 FUN_c000f614 (96B)
 *   chan_class1_notify_zero     K1 FUN_c000e290 (100B) -> K2 FUN_c000f678 (100B)
 *   chan_class2_apply_hi_or_lo  K1 FUN_c000e2f8 (208B) -> K2 FUN_c000f6e0 (208B)
 *   chan_class2_apply_readback  K1 FUN_c000e3c8 (140B) -> K2 FUN_c000f7b0 (140B)
 *   chan_class0_apply_value     K1 FUN_c000e454 (68B)  -> K2 FUN_c000f83c (68B)
 * 17/18 K1 functions matched (chan_link_rt_queue_push's own K2 role is real
 * but genuinely restructured, not a 1:1 port - see below).
 *
 * REAL, CONFIRMED DIFFERENCE FROM K1 - chan_link_rt_queue_push: RESOLVED
 * 2026-07-19 via a dedicated live Ghidra MCP session (decompile_function
 * directly on FUN_c000ea68 - it decompiled cleanly on the first attempt,
 * unlike several other addresses this same session hit "no function
 * found"/transient-failure quirks on). K1's own function is a small (144B),
 * self-contained "push one realtime byte into a 64-slot ring" primitive.
 * The K2 function occupying the equivalent cross-file role (FUN_c000ea68,
 * 356 bytes) is a GENUINE BEHAVIORAL RESTRUCTURING, not a push at all
 * anymore: it now POPS the next pending byte out of its own internal
 * 64-slot realtime ring (byte at *ring_base + read_idx, read_idx a NEW
 * field at link+0x139 wrapping mod 0x40 - one byte before the +0x13a
 * pending-count field K1 already had), packages it as a 4-byte USB-MIDI
 * Realtime CIN frame ({0x0f, byte, 0, 0} - CIN 0xf is the standard
 * single-byte System Realtime code), transmits it via chan_link_tx, THEN
 * separately drains ring 1 via midi_ring1_drain_submit when the TX queue
 * has pending data, AND arms/re-arms two IRQ-enable lines (3 and 4) based
 * on two independent readiness checks - a combined "realtime service tick"
 * that subsumes what used to be a standalone push primitive. Full real body
 * transcribed below - not left as an unfilled extern. See STILL OPEN for
 * the one callee (FUN_c0008674) whose own subsystem attribution is still
 * open.
 *
 * chan_class_wire_code (K1 68B -> K2 88B) and chan_desc_query_dispatch (K1
 * 416B -> K2 412B) are NOT exact size matches but ARE confirmed identical
 * in logic/branch structure (verified via direct decompile comparison,
 * function-by-function below) - the size deltas are ordinary codegen
 * variance, not a behavioral difference.
 *
 * Two shared per-channel tables confirmed via exact literal match to
 * chan_link_hw.c's/chan_slot_dispatch.c's own K2 citations:
 * chan_bitmask_table_base == 0xC0027A44, chan_index_table_base ==
 * 0xC0027A78 - same two globals, same roles, throughout this file too.
 *
 * REAL, CONFIRMED K2-WIDE MERGE: chan_global_hi_mode_flags (this file's own
 * DAT_c000f4fc) resolves to 0xC01CCD10 - the EXACT SAME address as
 * midi_engine.c-territory's own midi_hw_mode_flags AND chan_slot_dispatch.c's
 * own chan_port_hwctx_global (both independently re-derived from completely
 * separate call sites - see chan_slot_dispatch.c's own header for the first
 * half of this finding). THREE distinct K1 globals collapse into ONE K2
 * global - a real, address-confirmed simplification, not three coincidental
 * matches.
 *
 * STILL OPEN: chan_link_rt_ring_base's own real K2 address (DAT_c000ebd4 ->
 * 0xC01CCD1C, not independently cross-checked against any other consumer
 * this pass); chan_link_trip_latched/chan_link_handshake_armed's own K2
 * addresses (DAT_c000ef18 -> 0xC01CCD2C, DAT_c000ef1c -> 0xC00A09E0 - NOT
 * adjacent to each other in K2, unlike whatever relationship K1's own two
 * module-scope booleans had - not independently examined further);
 * chan_link_rx_pop's own K2 producer (DAT_c000f164 -> 0xC01CCD28, a
 * function-pointer global, not traced this pass either, same open item K1
 * left); FUN_c0008674's own subsystem attribution - chan_link_rt_queue_push
 * calls it as a readiness gate (dead-arg call, same phantom-forwarded
 * pattern as chan_status_byte_msb below), and raw disassembly shows it reads
 * a 16-bit field off chan_status_obj (usbdc_midi_status_glue.c's own
 * 0xC01CB344) plus a small fixed offset and reports `(field+0x40 >= 0x400)`
 * - a headroom/fill-level check structurally similar to
 * midi_ring2_headroom_ok, but it sits at 0xc0008674, well before
 * usbdc_midi_status_glue.c's own assigned range starts (chan_irq_toggle,
 * 0xc00087d8) - not claimed by this pass, NEEDS LIVE QUERY for whichever
 * file's territory it really belongs to; chan_dispatch_probe's own K2 body
 * (FUN_c00117c8, called from usbdc_midi_status_glue.c's own
 * chan_maybe_enable_irq4) was not decompiled this pass either - see that
 * file's own STILL OPEN note on chan_status_promote_on_flag for why this is
 * a live lead there too.
 */

#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 * PART A - link watchdog, realtime-byte ring buffer, TX/RX queue pump.
 * Struct offsets on "link" confirmed IDENTICAL to K1 throughout.
 * ===========================================================================
 */

extern uint32_t chan_link_tx(uint32_t target, uint8_t subcode, const void *buf, uint32_t len);	/* FUN_c000d4f4, chan_slot_dispatch.c (K1: FUN_c000c1f0) */
extern void chan_link_timeout_notify(uint32_t target, uint8_t subcode);	/* FUN_c000d20c, chan_slot_dispatch.c (K1: FUN_c000bf08) */
extern int  chan_link_probe_ready(uint32_t target);				/* FUN_c000c948, chan_link_hw.c (K1: FUN_c000b644) */
extern int  chan_link_probe_armed(uint32_t target);				/* FUN_c000df18 (K1: FUN_c000cc14/midi_hw_is_enabled) */
extern int  chan_link_start_handshake(uint32_t target, uint8_t subcode);	/* FUN_c000df64 (K1: FUN_c000cc60/midi_hw_channel_active) */
extern void chan_link_set_mode(uint32_t target, uint32_t mode);		/* FUN_c000d33c, chan_slot_dispatch.c (K1: FUN_c000c038) */
extern void chan_link_ack(uint32_t target, uint8_t code);			/* FUN_c000d564, chan_slot_dispatch.c (K1: FUN_c000c260) */
extern void chan_link_teardown(void *link, uint32_t unused);			/* FUN_c000e15c (K1: FUN_c000ce58/midi_context_reset) */

/* module-scope (NOT per-link) watchdog state. K2 addresses NOT adjacent to
 * each other (see file header STILL OPEN), unlike K1's own layout. */
extern uint32_t chan_link_trip_latched;	/* DAT_c000ef18 -> 0xC01CCD2C */
extern uint32_t chan_link_handshake_armed;	/* DAT_c000ef1c -> 0xC00A09E0 */

/* chan_link_send_reset_frame - FUN_c000ed9c, @0xc000ed9c (76 bytes, K1:
 * FUN_c000d9bc, 76 bytes). Identical zero-frame-and-transmit. */
void chan_link_send_reset_frame(void *link)	/* FUN_c000ed9c */
{
	uint8_t *L = (uint8_t *)link;
	uint32_t frame[16] = { 0 };

	chan_link_tx(*(uint32_t *)(L + 0x00),
		     *(uint8_t  *)(L + 0x08),
		     frame,
		     *(uint32_t *)(L + 0x10));
}

/* chan_link_watchdog_tick - FUN_c000edec, @0xc000edec (300 bytes, K1:
 * FUN_c000da0c, 300 bytes - EXACT match). Byte-for-byte identical two-stage
 * state machine to K1: tick counter at +0x134, 0x7d trip threshold, probe/
 * handshake/abort logic, shared disarm+teardown tail. Confirmed K2 callers:
 * FUN_c000a58c (wire_dispatch.c-territory's master_dispatch_tick
 * counterpart), a call site with no containing function object (same
 * "unboxed call site" artifact this project repeatedly documents), and
 * FUN_c000f0c8 (chan_link_rx_queue_drain below, self-calls exactly as K1
 * does). @0xc000edec. */
uint32_t chan_link_watchdog_tick(void *link)	/* FUN_c000edec */
{
	uint8_t *L = (uint8_t *)link;
	uint32_t target = *(uint32_t *)(L + 0x00);
	uint8_t  subcode = *(uint8_t  *)(L + 0x08);

	if (*(uint32_t *)(L + 0x54c) != 0) {
		if (chan_link_trip_latched == 0) {
			*(uint32_t *)(L + 0x134) += 1;
			if (*(uint32_t *)(L + 0x134) == 0x7d) {
				chan_link_timeout_notify(target, subcode);
				chan_link_send_reset_frame(link);
				chan_link_trip_latched = 1;
			}
			return *(uint32_t *)(L + 0x54c);
		}
		*(uint32_t *)(L + 0x134) = 0;
	}

	if (chan_link_probe_ready(target) && chan_link_probe_armed(target)) {
		if (chan_link_handshake_armed == 0) {
			if (chan_link_start_handshake(target, subcode) == 0) {
				if (chan_link_trip_latched != 0)
					chan_link_trip_latched = 0;
				*(uint32_t *)(L + 0x54c) = 1;
				return *(uint32_t *)(L + 0x54c);
			}
		} else {
			chan_link_set_mode(target, 4);
			chan_link_ack(target, 3);
			chan_link_send_reset_frame(link);
			chan_link_handshake_armed = 0;
		}
	}

	*(uint32_t *)(L + 0x54c) = 0;
	if (chan_link_trip_latched != 0) {
		chan_link_teardown(link, 0);
		chan_link_trip_latched = 0;
		chan_link_handshake_armed = 1;
	}
	return *(uint32_t *)(L + 0x54c);
}

/* chan_link_rt_queue_push - FUN_c000ea68, @0xc000ea68 (356 bytes, K1:
 * FUN_c000dbac, 144 bytes). RESOLVED 2026-07-19 via a dedicated live Ghidra
 * MCP session - real decompile below, not a 1:1 K1 port. See file header's
 * own "REAL, CONFIRMED DIFFERENCE" section for the full narrative.
 *
 * Real signature: ONE parameter (`link`), not K1's (link, byte) two - this
 * K2 version no longer takes an incoming byte to push, it POPS its own
 * ring internally instead (see below). Field offsets confirmed: link+0x00
 * = target (SAME as every PART A function's L+0x00), link+0x08 = subcode
 * (SAME as PART A's L+0x08, tested here as `subcode==4`), link+0x139 =
 * realtime-ring READ index (byte, wraps mod 0x40 - a NEW field immediately
 * before link+0x13a), link+0x13a = pending-realtime-byte COUNT (SAME offset
 * K1 uses, DAT_c000ebcc == 0x13a exact), link+0x4a/+0x4c = ring-1 write/
 * read index (SAME fields midi_ring1_push_word/midi_ring1_drain_submit use
 * in midi_engine.c - this function operates on the SAME shared ctx struct
 * those functions do), link+0x154 = ring-1 drain-in-progress flag (SAME
 * field midi_ring1_drain_submit sets to 1 at its own entry - cleared back
 * to 0 here immediately after calling it), link+0x4d = a flag cleared
 * unconditionally whenever a pending realtime byte was actually drained.
 *
 * chan_link_rt_ring_base (DAT_c000ebd4 -> 0xC01CCD1C) is dereferenced once
 * then indexed by the read index - the realtime ring's own backing store,
 * a plain byte array (not the 4-byte-word ring-1/ring-2 layout midi_engine.c
 * uses).
 *
 * midi_dead_handle_const (DAT_c000ebd8 -> 0xC01CB2EC) - CONFIRMED the EXACT
 * SAME literal midi_engine.c's own midi_subsystem_init_entry uses
 * (DAT_c000e054 in that file) - passed as a phantom-forwarded dead first
 * argument to chan_status_byte_msb (usbdc_midi_status_glue.c,
 * FUN_c0008c14), FUN_c0008674 (an unclaimed readiness-check leaf, see file
 * header STILL OPEN), and chan_irq_toggle (FUN_c00087d8) - all three
 * confirmed (per their own real decompiles/signatures) to ignore whatever
 * first argument they're handed, same phantom-forwarded-argument pattern
 * this whole project has repeatedly documented.
 *
 * Real flow: if no realtime byte is pending (link+0x13a byte == 0), only
 * drains ring 1 (via midi_ring1_drain_submit) when its own write/read
 * indices differ, optionally poking midi_hw_set_reg_f6 first when
 * subcode==4. If a realtime byte IS pending: pokes midi_hw_set_reg_f6 on
 * the same subcode==4 condition, pops one byte from the realtime ring
 * (IRQ-guarded index advance AND IRQ-guarded pending-count decrement -
 * separate guarded sections, not one combined critical section), packs it
 * as a CIN-0xf single-byte-realtime USB-MIDI frame ({0x0f, byte, 0, 0}) and
 * transmits it via chan_link_tx, then (IRQ-guarded) re-arms IRQ line 3 if
 * chan_status_byte_msb reports false. Regardless of which branch ran, then
 * (IRQ-guarded) re-arms IRQ line 4 if ring 1 has space AND FUN_c0008674
 * reports false. Finally, unless BOTH ring 1 is empty (write==read) AND no
 * realtime byte remains pending, calls chan_link_ack to acknowledge the
 * link. Sole caller: a call site at 0xc0008cf0 with no containing function
 * object in this static dump (same class of gap as several other files in
 * this project). */
extern void     midi_hw_set_reg_f6(uint32_t target);			/* FUN_c000cb54, chan_link_hw.c */
extern void     midi_ring1_drain_submit(uint32_t *ctx);		/* FUN_c000e8d4, midi_engine.c */
extern int      midi_ring1_has_space(uint32_t *ctx);			/* FUN_c000e93c, midi_engine.c */
extern bool     chan_status_byte_msb(void);				/* FUN_c0008c14, usbdc_midi_status_glue.c - dead-arg callee, see note above */
extern int      chan_link_rt_readiness_probe(void *unused_ctx);	/* FUN_c0008674, subsystem NOT independently attributed this pass - dead-arg callee, see STILL OPEN */
extern void     chan_irq_toggle(void *unused_ctx, uint8_t line, uint8_t which, bool enable);	/* FUN_c00087d8, usbdc_midi_status_glue.c */
extern void     irq_guard_enter_k2(void);	/* FUN_c0004f40, usbdc_midi_status_glue.c/crypto_at88.c - re-declared locally, same convention as that file */
extern void     irq_guard_exit_k2(void);	/* FUN_c0004f50 */

void chan_link_rt_queue_push(void *link)	/* FUN_c000ea68 - real decompile is void, K1's own uint32_t-return "push" naming no longer applies */
{
	/* link+0x139 = realtime-ring read index (DAT_c000ebd0, offset only, not
	 * a separate global); link+0x13a = pending-realtime-byte count
	 * (DAT_c000ebcc == 0x13a, exact match to K1's own offset) - both are
	 * fields ON `link`, addressed directly below rather than via a fake
	 * extern. */
	extern uint8_t *chan_link_rt_ring_base;	/* DAT_c000ebd4 -> 0xC01CCD1C */
	extern void    *midi_dead_handle_const;	/* DAT_c000ebd8 -> 0xC01CB2EC, SAME literal midi_engine.c's own midi_subsystem_init_entry uses */
	uint8_t *L = (uint8_t *)link;
	uint8_t frame[4];

	if (L[0x13a] == 0) {
		if (*(uint32_t *)(L + 0x4c) != *(uint32_t *)(L + 0x4a)) {
			if (L[8] == 4)
				midi_hw_set_reg_f6(*(uint32_t *)L);
			midi_ring1_drain_submit((uint32_t *)link);
			*(uint32_t *)(L + 0x154) = 0;
		}
	} else {
		uint8_t byte;

		if (L[8] == 4)
			midi_hw_set_reg_f6(*(uint32_t *)L);

		byte = chan_link_rt_ring_base[L[0x139]];
		L[0x139] = (L[0x139] + 1) & 0x3f;

		irq_guard_enter_k2();
		L[0x13a]--;
		irq_guard_exit_k2();

		*(uint32_t *)(L + 0x4d) = 0;
		frame[0] = 0x0f;
		frame[1] = byte;
		frame[2] = 0;
		frame[3] = 0;
		chan_link_tx(*(uint32_t *)L, L[8], frame, 4);

		irq_guard_enter_k2();
		if (!chan_status_byte_msb())
			/* real decompile shows only 2 visible args here (dead ctx, line=3) -
			 * `which`/`enable` are NOT independently recoverable from this call
			 * site (phantom-forwarded from whatever's already live in r2/r3 at
			 * this point) - 0/true kept as an unverified placeholder, not a
			 * confirmed value. NEEDS LIVE QUERY: a register-level trace back
			 * from this exact call site to pin down the real r2/r3 contents. */
			chan_irq_toggle(midi_dead_handle_const, 3, 0 /* unverified */, true /* unverified */);
		irq_guard_exit_k2();
	}

	irq_guard_enter_k2();
	if (midi_ring1_has_space((uint32_t *)link) &&
	    !chan_link_rt_readiness_probe(midi_dead_handle_const))
		/* same "only 2 visible args" caveat as above - which/enable unverified */
		chan_irq_toggle(midi_dead_handle_const, 4, 0 /* unverified */, true /* unverified */);
	irq_guard_exit_k2();

	if (*(uint32_t *)(L + 0x4c) == *(uint32_t *)(L + 0x4a) && L[0x13a] == 0)
		return;

	chan_link_ack(*(uint32_t *)L, L[8]);
}

/* chan_link_tx_queue_drain - FUN_c000f038, @0xc000f038 (112 bytes, K1:
 * FUN_c000dc4c, 112 bytes - EXACT match). Identical drain loop and guard
 * chain (+0x134 tripped sentinel, +0x54c armed, +300/+0x12c pending count,
 * +0x548 reentrancy). chan_link_tx_step is the SAME address as
 * midi_ring1_push_word (FUN_c000e750, midi_engine.c-territory) - reproduces
 * K1's own aliasing exactly. Sole K2 caller: chan_link_service_tick below. */
extern int chan_link_tx_step(void *link, uint32_t *cursor);	/* FUN_c000e750 (K1: FUN_c000d450, also midi_ring1_push_word) */

void chan_link_tx_queue_drain(void *link)	/* FUN_c000f038 */
{
	uint8_t *L = (uint8_t *)link;
	uint32_t cursor;

	if (*(uint32_t *)(L + 0x134) == 0x7d)
		return;
	if (*(uint32_t *)(L + 0x54c) == 0)
		return;
	if (*(uint32_t *)(L + 0x12c) == 0)
		return;
	if (*(uint32_t *)(L + 0x548) != 0)
		return;

	*(uint32_t *)(L + 0x548) = 1;
	cursor = 0;
	while (chan_link_tx_step(link, &cursor) == 0)
		;
	*(uint32_t *)(L + 0x548) = 0;
}

/* chan_link_service_tick - FUN_c000f0a8, @0xc000f0a8 (32 bytes, K1:
 * FUN_c000dcbc, 32 bytes - EXACT match). Thin combo, calls
 * chan_link_uart_pump (SAME address as midi_stream_decode_step,
 * FUN_c000e2cc - reproduces K1's own aliasing) then chan_link_tx_queue_drain. */
extern void chan_link_uart_pump(void *link);	/* FUN_c000e2cc (K1: FUN_c000cfc8, also midi_stream_decode_step) */

void chan_link_service_tick(void *link)	/* FUN_c000f0a8 */
{
	chan_link_uart_pump(link);
	chan_link_tx_queue_drain(link);
}

/* chan_link_rx_queue_drain - FUN_c000f0c8, @0xc000f0c8 (156 bytes, K1:
 * FUN_c000dcdc, 156 bytes - EXACT match). Identical: self-calls
 * chan_link_watchdog_tick, pops (a,b) pairs via an indirect callback
 * (chan_link_rx_pop, DAT_c000f164 -> 0xC01CCD28, a function-pointer global,
 * producer not traced - same open item K1 left), forwards each pair to
 * chan_link_rx_apply_pair (SAME address as midi_event_push_byte,
 * FUN_c000e984 - reproduces K1's own aliasing exactly). */
extern int  chan_link_rx_pop(uint8_t *out_a, uint8_t *out_b);			/* *DAT_c000f164 -> 0xC01CCD28, indirect */
extern void chan_link_rx_apply_pair(void *link, uint8_t a, uint8_t b);	/* FUN_c000e984 (K1: FUN_c000d618, also midi_event_push_byte) */

void chan_link_rx_queue_drain(void *link)	/* FUN_c000f0c8 */
{
	uint8_t *L = (uint8_t *)link;
	uint8_t a, b;
	uint32_t armed;

	armed = chan_link_watchdog_tick(link);

	if (*(uint32_t *)(L + 0x134) == 0x7d)
		return;
	if (armed == 0)
		return;
	if (*(uint32_t *)(L + 0x548) != 0)
		return;

	do {
		if (chan_link_rx_pop(&a, &b) == 0)
			return;
		chan_link_rx_apply_pair(link, a, b);

		if (*(uint32_t *)(L + 0x128) < 0xff) {
			if (*(uint32_t *)(L + 0x128) + 1 == *(uint32_t *)(L + 0x130))
				return;
		} else if (*(uint32_t *)(L + 0x130) == 0) {
			return;
		}
	} while (*(uint32_t *)(L + 0x548) == 0);
}

/* chan_link_obj_init - FUN_c000f184, @0xc000f184 (40 bytes, K1: FUN_c000dd98,
 * 40 bytes - EXACT match). Clears a 6-byte scratch region at +0x16f..+0x174
 * (SAME offsets as K1) and sets a sentinel dword to -1 at +0x178 (SAME
 * offset as K1). Confirmed K2 callers: FUN_c000c2e4 (a linked-list-insert-
 * shaped constructor, matching K1's own FUN_c000afe0 role) and FUN_c000cba4
 * (K2's own 820-byte MMIO port-bringup sequence - confirmed the exact K2
 * counterpart of K1's own out-of-range FUN_c000b8a0, same 820-byte size,
 * same caller role - see chan_link_hw.c's own header for how this function
 * was independently found). Neither reconstructed here (out of this file's
 * own scope, matching K1's own boundary). */
void chan_link_obj_init(void *link)	/* FUN_c000f184 */
{
	uint8_t *L = (uint8_t *)link;
	int i;

	for (i = 0x16f; i <= 0x174; i++)
		L[i] = 0;

	*(uint32_t *)(L + 0x178) = 0xffffffff;
}

/* ===========================================================================
 * PART B
 * ===========================================================================
 */

/* chan_desc_dispatch_enabled - FUN_c000f17c, @0xc000f17c (8 bytes, K1:
 * FUN_c000dd90, 8 bytes - EXACT match). Identical trivial flag-byte
 * accessor. Confirmed K2 callers: chan_slot_dispatch.c's own
 * chan_port_slot_notify/_xfer_slot_rx_pump (both already cite this
 * directly), reproducing K1's own cross-file relationship exactly. */
uint8_t chan_desc_dispatch_enabled(void *desc)	/* FUN_c000f17c */
{
	return *((uint8_t *)desc + 10);
}

/* ===========================================================================
 * PART C - per-channel-index parameter query/apply/notify engine.
 * "desc" struct offsets confirmed IDENTICAL to K1 throughout.
 * ===========================================================================
 */

extern uint8_t *chan_index_table_base;		/* DAT_c000f500/... here -> 0xC0027A78 (K1: 0xc001f6c4) */
extern uint8_t *chan_bitmask_table_base;	/* DAT_c000f2c0/... here -> 0xC0027A44 (K1: 0xc001f690) */
extern uint8_t *chan_global_hi_mode_flags;	/* DAT_c000f4fc -> 0xC01CCD10 - SAME address as chan_slot_dispatch.c's own
						 * chan_port_hwctx_global AND midi_engine.c-territory's own
						 * midi_hw_mode_flags - a real 3-way K2 merge, see file header */

extern uint32_t chan_class2_default_value(uint32_t target);		/* FUN_c000df40 (K1: FUN_c000cc3c/midi_hw_reg70_bit3) */
extern uint32_t chan_class2_read_value(uint32_t target, uint8_t idx);	/* FUN_c000c990, chan_link_hw.c (K1: FUN_c000b68c) */
extern void chan_slot_apply_code(uint32_t target, uint8_t code);	/* FUN_c000c950, chan_link_hw.c (K1: FUN_c000b64c) */
extern void chan_slot_echo_code(uint32_t target, uint8_t code);	/* FUN_c000c970, chan_link_hw.c (K1: FUN_c000b66c) */
extern void chan_class0_send_value(uint32_t target, uint8_t value);	/* FUN_c000dd54 (K1: FUN_c000ca50/midi_hw_reg6a_write) */
extern void chan_slot_dispatch(uint32_t selection, int hi, uint8_t code);	/* FUN_c000c4cc, out of range (K1: FUN_c000b1c8) */

/* chan_class_wire_code - FUN_c000f1f8, @0xc000f1f8 (88 bytes - NOT K1's 68
 * bytes, but logically identical: idx in {1,2,3} plus hi flag maps to
 * 1/3/5 (hi==0) or 2/4/6 (hi!=0), default 0). Confirmed called with only 2
 * args at every K2 call site (param_3/param_4 phantom, exact same pattern
 * K1 documented). @0xc000f1f8. */
uint32_t chan_class_wire_code(int8_t idx, int hi)	/* FUN_c000f1f8 */
{
	if (hi == 0) {
		if (idx == 1) return 1;
		if (idx == 2) return 3;
		if (idx == 3) return 5;
	} else {
		if (idx == 1) return 2;
		if (idx == 2) return 4;
		if (idx == 3) return 6;
	}
	return 0;	/* NOT faithful for idx outside {1,2,3} - see K1's own note */
}

/* chan_class2_test_hi / chan_class2_test_lo - FUN_c000f250/@0xc000f250 (112
 * bytes, K1: FUN_c000de64, 112 bytes - EXACT match) / FUN_c000f2cc/
 * @0xc000f2cc (108 bytes, K1: FUN_c000dee0, 108 bytes - EXACT match).
 * Identical bounds-check + bitmask-test logic, mask 0xffc0 confirmed
 * identical to K1. */
bool chan_class2_test_hi(void *desc, uint8_t idx)	/* FUN_c000f250 */
{
	uint8_t chan = *((uint8_t *)desc + 9);
	uint8_t *ci = chan_index_table_base + (uint32_t)chan * 0x44;
	uint8_t *bt = chan_bitmask_table_base + (uint32_t)chan * 8;
	uint16_t bit;

	if (idx > ci[0x1f])
		return false;

	bit = *(uint16_t *)(*(uint8_t **)(bt + 4) + (idx & 0xff) * 4) & 0xffc0;
	return bit != 0;
}

bool chan_class2_test_lo(void *desc, uint8_t idx)	/* FUN_c000f2cc */
{
	uint8_t chan = *((uint8_t *)desc + 9);
	uint8_t *ci = chan_index_table_base + (uint32_t)chan * 0x44;
	uint8_t *bt = chan_bitmask_table_base + (uint32_t)chan * 8;
	uint16_t bit;

	if (idx > ci[0x1e])
		return false;

	bit = *(uint16_t *)(*(uint8_t **)(bt + 0) + (idx & 0xff) * 4) & 0xffc0;
	return bit != 0;
}

/* chan_desc_query_dispatch - FUN_c000f344, @0xc000f344 (412 bytes - NOT
 * K1's 416 bytes, but a direct case-by-case structural match: case 1/2
 * select field pairs based on chan_global_hi_mode_flags[1]&0x40, case 2's
 * own "negative flag + desc+0xc" local-copy redirect, case 3's 7-entry
 * sub-object array, cases 6/7 fixed fields, default 0 - all identical to
 * K1). Reply capped against +0x16, sent via chan_link_tx. Sole K2 caller:
 * FUN_c000fd0c (K2's own counterpart of K1's out-of-range FUN_c000e924,
 * out of this file's own scope). @0xc000f344. */
uint32_t chan_desc_query_dispatch(void *desc)	/* FUN_c000f344 */
{
	uint8_t *D = (uint8_t *)desc;
	uint8_t chan = D[9];
	uint8_t *ci = chan_index_table_base + (uint32_t)chan * 0x44;
	uint16_t req = *(uint16_t *)(D + 0x12);
	uint32_t len = req & 0xff;
	uint8_t *reply;

	switch (req >> 8) {
	case 1:
		if (D[8] == 0)
			D[8] = 1;
		reply = (chan_global_hi_mode_flags[1] & 0x40) == 0
			  ? *(uint8_t **)(ci + 0x00)
			  : *(uint8_t **)(ci + 0x08);
		break;

	case 2:
		if (D[8] < 2)
			D[8] = 2;
		reply = (chan_global_hi_mode_flags[1] & 0x40) == 0
			  ? *(uint8_t **)(ci + 0x04)
			  : *(uint8_t **)(ci + 0x0c);
		len = *(uint16_t *)(reply + 2);

		if (*(uint32_t *)(D + 0xc) != 0 && (int8_t)reply[7] < 0) {
			uint32_t i;

			for (i = 0; i < len; i++)
				D[0x3c + i] = reply[i];
			reply = D + 0x3c;
			D[0x43] = (D[0x43] & 0x7f) | 0x40;
			D[0x44] = 0;
		}
		goto have_len;

	case 3:
		if (len > 6)
			return 0;
		reply = (*(uint8_t ***)(ci + 0x18))[len];
		break;

	case 6:
		reply = *(uint8_t **)(ci + 0x10);
		break;

	case 7:
		reply = *(uint8_t **)(ci + 0x14);
		len = *(uint16_t *)(reply + 2);
		goto have_len;

	default:
		return 0;
	}

	len = *reply;

have_len:
	if (len != 0) {
		uint32_t cap = *(uint16_t *)(D + 0x16);
		uint32_t n = (len < cap) ? len : cap;

		chan_link_tx(*(uint32_t *)(D + 4), 0, reply, n);
	}
	return 1;
}

/* chan_class2_poll_and_notify - FUN_c000f504, @0xc000f504 (264 bytes, K1:
 * FUN_c000e11c, 264 bytes - EXACT match). Identical class-dispatched value
 * poll, 0xffff "no change" sentinel confirmed identical to K1. Sole K2
 * caller: FUN_c000fd0c (out of this file's own scope). */
bool chan_class2_poll_and_notify(void *desc)	/* FUN_c000f504 */
{
	uint8_t *D = (uint8_t *)desc;
	uint32_t value = 0xffff;
	uint8_t class = D[0x10] & 0x1f;
	bool is_subtype2, changed;

	if (class == 1) {
		value = 0;
	} else if (class == 0) {
		uint32_t c = *(uint32_t *)(D + 0xc);

		value = 1 - c;
		if (c > 1)
			value = 0;
	} else if (class == 2) {
		uint16_t raw = *(uint16_t *)(D + 0x14);
		uint8_t idx = raw & 0x7f;

		if (idx == 0) {
			value = chan_class2_default_value(*(uint32_t *)(D + 4));
		} else {
			bool present = (raw & 0x80) ? chan_class2_test_hi(desc, idx)
						     : chan_class2_test_lo(desc, idx);
			if (!present)
				goto done;
			value = chan_class2_read_value(*(uint32_t *)(D + 4), idx);
		}
		value &= 0xffff;
	}

done:
	is_subtype2 = (*(int16_t *)(D + 0x16) == 2);
	changed = (value != 0xffff);

	if (changed && is_subtype2) {
		D[0x160] = (uint8_t)(value >> 8);
		D[0x15f] = (uint8_t)value;
		chan_link_tx(*(uint32_t *)(D + 4), 0, D + 0x15f, 2);
	}
	return changed && is_subtype2;
}

/* chan_class0_notify_flag - FUN_c000f614, @0xc000f614 (96 bytes, K1:
 * FUN_c000e22c, 96 bytes - EXACT match). Identical: class 0/subtype 1,
 * copies chan_desc_dispatch_enabled's own byte (+0x0a) into the reply
 * stage. Sole K2 caller: FUN_c000fd0c (out of this file's own scope). */
bool chan_class0_notify_flag(void *desc)	/* FUN_c000f614 */
{
	uint8_t *D = (uint8_t *)desc;

	if ((D[0x10] & 0x1f) != 0 || *(int16_t *)(D + 0x16) != 1)
		return false;

	D[0x15f] = D[10];
	chan_link_tx(*(uint32_t *)(D + 4), 0, D + 0x15f, 1);
	return true;
}

/* chan_class1_notify_zero - FUN_c000f678, @0xc000f678 (100 bytes, K1:
 * FUN_c000e290, 100 bytes - EXACT match). Identical: class 1/subtype 1,
 * always sends a single zero byte. */
bool chan_class1_notify_zero(void *desc)	/* FUN_c000f678 */
{
	uint8_t *D = (uint8_t *)desc;

	if ((D[0x10] & 0x1f) != 1 || *(int16_t *)(D + 0x16) != 1)
		return false;

	D[0x15f] = 0;
	chan_link_tx(*(uint32_t *)(D + 4), 0, D + 0x15f, 1);
	return true;
}

/* chan_class2_apply_hi_or_lo - FUN_c000f6e0, @0xc000f6e0 (208 bytes, K1:
 * FUN_c000e2f8, 208 bytes - EXACT match). Identical class 2 apply path:
 * bitmask-tests idx, applies the mapped wire code (chan_slot_apply_code)
 * AND dispatches through chan_slot_dispatch (out of range, K2's own
 * FUN_c000c4cc). Sole K2 caller: FUN_c000d6a0 (chan_slot_dispatch.c's own
 * documented port interrupt dispatcher, out of this file's own scope). */
uint32_t chan_class2_apply_hi_or_lo(void *desc)	/* FUN_c000f6e0 */
{
	uint8_t *D = (uint8_t *)desc;
	uint16_t raw;
	uint8_t idx;
	uint32_t code;
	int hi;

	if ((D[0x10] & 0x1f) != 2)
		return 0;

	raw = *(uint16_t *)(D + 0x14);
	idx = raw & 0x7f;
	if (idx == 0)
		return 1;

	hi = (raw & 0x80) != 0;
	if (!hi) {
		if (!chan_class2_test_lo(desc, idx))
			return 0;
		code = chan_class_wire_code((int8_t)idx, 0);
	} else {
		if (!chan_class2_test_hi(desc, idx))
			return 0;
		code = chan_class_wire_code((int8_t)idx, 1);
	}

	chan_slot_apply_code(*(uint32_t *)(D + 4), (uint8_t)code);
	chan_slot_dispatch(*(uint32_t *)(D + 0), hi, (uint8_t)code);
	return 1;
}

/* chan_class2_apply_readback - FUN_c000f7b0, @0xc000f7b0 (140 bytes, K1:
 * FUN_c000e3c8, 140 bytes - EXACT match). Identical "confirm/echo" path,
 * chan_slot_echo_code only, no chan_slot_dispatch call. Sole K2 caller:
 * FUN_c000d6a0 (out of this file's own scope). */
uint32_t chan_class2_apply_readback(void *desc)	/* FUN_c000f7b0 */
{
	uint8_t *D = (uint8_t *)desc;
	uint16_t raw;
	uint8_t idx;
	int lo;
	bool present;

	if ((D[0x10] & 0x1f) != 2)
		return 0;

	raw = *(uint16_t *)(D + 0x14);
	idx = raw & 0x7f;
	if (idx == 0)
		return 1;

	lo = (raw & 0x80) == 0;
	present = lo ? chan_class2_test_lo(desc, idx) : chan_class2_test_hi(desc, idx);

	if (present) {
		uint32_t code = chan_class_wire_code((int8_t)idx, lo ? 0 : 1);

		chan_slot_echo_code(*(uint32_t *)(D + 4), (uint8_t)code);
		return 1;
	}
	return 0;
}

/* chan_class0_apply_value - FUN_c000f83c, @0xc000f83c (68 bytes, K1:
 * FUN_c000e454, 68 bytes - EXACT match). Identical: class 0, sends the
 * signed byte at +0x12 via chan_class0_send_value (SAME address as
 * midi_hw_reg6a_write, chan_link_hw.c - reproduces K1's own aliasing
 * exactly), gated non-negative. Sole K2 caller: FUN_c000d6a0 (out of this
 * file's own scope). */
uint32_t chan_class0_apply_value(void *desc)	/* FUN_c000f83c */
{
	uint8_t *D = (uint8_t *)desc;

	if ((D[0x10] & 0x1f) == 0 && *(int8_t *)(D + 0x12) >= 0) {
		chan_class0_send_value(*(uint32_t *)(D + 4), D[0x12]);
		return 1;
	}
	return 0;
}
