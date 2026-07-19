/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_desc_notify_misc.c - reconstructs the 4 genuinely-uncovered
 * functions in this task's assigned range "0xc000e748-0xc000e8c0 (4
 * fns)": FUN_c000e748, FUN_c000e76c, FUN_c000e8a8, FUN_c000e8c0.
 * Confirmed genuinely uncovered: grepped for each address across every
 * *.c file in this project first. FUN_c000e748 IS cited - as a bare
 * extern, `chan_port_ctx_notify_b`, in chan_slot_dispatch.c, matching
 * name/signature reused here per this project's own "supply the real body
 * for an already-named extern" convention (omap_l137_usbdc_ep0.c did the
 * same for usbdc_ep0_class5_handler etc.). The other three have no prior
 * name anywhere.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18 pass), read via query_dump.py. No live Ghidra
 * MCP calls this pass (bridge flagged concurrency-unsafe under this
 * project's own parallel-agent constraint).
 *
 * ANCHOR: NONE - same situation as chan_param_ctrl.c/chan_slot_dispatch.c
 * (no `__FILE__` string reachable from any call site in this range).
 *
 * CROSS-FILE STRUCT CONFIRMATION (the reason this file is placed here,
 * next to chan_slot_dispatch.c, rather than treated as unattributed):
 * every function below operates on the SAME "desc" object chan_param_
 * ctrl.c's own Part C already documents in detail (+0x09 channel-index,
 * +0x0c class-0 value, +0x10 class selector, +0x12 request word, +0x15f/
 * +0x160 reply-staging buffer) PLUS the further fields chan_param_ctrl.c's
 * own file header flags as "may be the same struct... Part C touches
 * offsets 0x00-0x180ish" (+0x170/+0x171/+0x178/+0x17c/+0x17e/+0x180) -
 * confirmed here by direct data-value comparison (query_dump.py dat):
 *   - FUN_c000e76c's literal reply offset (DAT_c000e890 = 0x15f) is the
 *     IDENTICAL numeric value chan_param_ctrl.c's own chan_desc_query_
 *     dispatch/chan_class2_poll_and_notify use for their own D[0x15f]
 *     reply stage, and it calls chan_link_tx (FUN_c000c1f0) with the
 *     EXACT same (target, subcode, buf, len) argument shape those
 *     functions use.
 *   - FUN_c000e8a8/FUN_c000e8c0 both resolve their own "ci" base
 *     (DAT_c000b05c / DAT_c000b368) to 0xC001F6C4 - the identical address
 *     chan_param_ctrl.c's own chan_index_table_base resolves to (also
 *     confirmed via query_dump.py dat, not assumed from proximity).
 *
 * =============================================================================
 * chan_port_ctx_notify_b - FUN_c000e748 (52 bytes)
 * =============================================================================
 * Clears three desc fields at +0x17c (u8), +0x17e (u16), +0x180 (u16) to
 * zero, then writes a fixed register (through TWO levels of pointer
 * indirection off desc+0x04 - i.e. `**(desc+4)`, NOT the single-deref
 * "target handle" chan_param_ctrl.c's own Part A/C document for that same
 * offset - a real, direct hardware base address reached via one more
 * level of indirection than the "target" ID used elsewhere) - reg 0x60,
 * value 0x80 (matches midi_hw_fifo_write's own channel-0 status-trigger
 * register/value in soc_irq_gate.c).
 *
 * SAME 3-FIELD CLEAR CONFIRMED ELSEWHERE: FUN_c000afe0 (an out-of-range
 * desc-object initializer, called from FUN_c000bc1c, itself out of range)
 * inlines this EXACT SAME 0x17c/0x17e/0x180 clear directly in its own body
 * instead of calling this function - i.e. this really is a reusable
 * "reset these three notify-state fields (+ pulse reg 0x60)" helper, not
 * a one-off.
 *
 * REAL CALL-SITE ARITY MISMATCH: chan_slot_dispatch.c's own citation
 * declares `chan_port_ctx_notify_b(uint32_t ctx, uint32_t len)` (2 args) -
 * matching the real call site in chan_port_reg60_rx_pump
 * (`FUN_c000e748(param_1[2], uVar5 & 0xffff)`), but this function's own
 * decompiled body has only ONE formal parameter; `len` is a phantom
 * unused 2nd argument, matching this project's already-established
 * "phantom forwarded parameter" idiom (cdix4192.c, eva_board_watchdog_
 * fault_wrapper, chan_hw_fifo_write). Kept as a 2-arg signature to match
 * chan_slot_dispatch.c's own extern exactly; `len` is simply unused here.
 *
 * @0xc000e748.
 *
 * =============================================================================
 * chan_desc_setup_reply_notify - FUN_c000e76c (276 bytes)
 * =============================================================================
 * Dispatches on the HIGH BYTE of desc+0x12 (the same "request word" field
 * chan_param_ctrl.c's own chan_desc_query_dispatch reads, here tested as
 * `& 0xff00`):
 *   - high byte == 1: if desc+0x16 (the "message subtype" field per
 *     chan_param_ctrl.c) == 1, clears desc+DAT_c000e890 (desc+0x15f) and
 *     transmits 1 byte from desc+0x15f via chan_link_tx - i.e. an
 *     "acknowledge, subtype 1" reply.
 *   - high byte == 2: switches on desc+0x11 (a byte NOT documented by
 *     chan_param_ctrl.c's own Part C field list - a genuinely new field
 *     this function adds evidence for) with cases 0x81-0x84: 0x81/0x82
 *     stage a fixed reply byte (0xf8/0x80) at desc+0x160 before falling
 *     into the common "clear+send 1 byte at desc+0x15f" tail; 0x83/0x84
 *     instead clear a DIFFERENT desc field (desc+DAT_c000e890,
 *     desc+DAT_c000e890+1 for case 0x83; desc+0x160=0x40 then desc+
 *     DAT_c000e890+1 for case 0x84) and send 2 bytes from desc+0x15f.
 *     Any other value of desc+0x11 (or high byte outside {1,2}): no-op.
 * Returns 1 (via `uVar5 ^ 1` where uVar5 was set to 0) whenever a reply
 * was actually sent, 0 otherwise - transcribed exactly, including the
 * slightly indirect `uVar5 ^ 1` return idiom (real Ghidra output, not
 * simplified further since the exact original condition-flag shape is
 * not independently reconstructed).
 *
 * Sole caller (per xrefs_to): FUN_c000e924 at call site 0xc000eb80 -
 * FUN_c000e924 is chan_param_ctrl.c's own documented "PART C" caller
 * (large USB EP0 SETUP-packet reader, out of range for every file so
 * far).
 *
 * @0xc000e76c.
 *
 * =============================================================================
 * chan_desc_slot_vtable_notify - FUN_c000e8a8 (136 bytes)
 * =============================================================================
 * First re-initializes 4 of desc's own fields: +0x08=0 (state/mode byte),
 * +0x0c=param_3&0xff (class-0 value), +0x09=param_2 (channel-index),
 * +0x0a=0 - matching chan_param_ctrl.c's own Part C field list byte-for-
 * byte (confirms this really is the SAME desc struct). Resolves its own
 * "chan" via `*(int *)*desc + 9` - desc+0x00 (the "selection pointer") is
 * NOT the channel index directly here; ONE MORE dereference is needed
 * (desc+0x00 holds the address of a fixed global cell which itself holds
 * desc's own address - a self-referential indirection real and confirmed
 * by tracing FUN_c000afe0/FUN_c000bc1c's own construction of this same
 * desc object, both out of range and not reconstructed here) - the net
 * effect is still "read the channel-index byte at desc's own +0x09",
 * consistent with chan_param_ctrl.c's documented field, just reached via
 * an extra hop this function's own callers set up.
 *
 * Then walks a per-channel, up-to-N-entry, 0x20-byte-stride slot table at
 * chan_index_table_base[chan]+0x28 (count at +0x21) - the SAME 0x20-byte
 * "slot" object and +0x28 base chan_param_ctrl.c's own file header cites
 * for FUN_c000b1c8's "slot dispatch" (out of range there too) - calling
 * each slot's own function pointer at slot+0x0c with (slot[0], slot[4],
 * slot+2 as u16, slot+6 as u16). NOT independently confirmed to be the
 * SAME slot-table use as FUN_c000b1c8's own +0x14/+0x1c call (different
 * offset, +0x0c here) - presented as a related but distinct vtable call
 * into the same table, per this project's "additive, not force-fit"
 * convention.
 *
 * Sole caller (per xrefs_to): FUN_c000bc1c at call site 0xc000bc90 (out
 * of range, an desc-object constructor - see chan_port_ctx_notify_b's own
 * note above for how it builds this same desc).
 *
 * @0xc000e8a8.
 *
 * =============================================================================
 * chan_desc_enable_vtable_notify - FUN_c000e8c0 (100 bytes)
 * =============================================================================
 * Same "resolve chan via desc's own +9 through one indirection, then walk
 * a chan_index_table_base[chan]+0x44-strided entry's own sub-array" shape
 * as chan_desc_slot_vtable_notify above, but: (a) writes only ONE field
 * first (+0x0a = param_2, the enable/state byte - NOT the 3-field reset
 * that function does), (b) uses count field +0x25 and array base +0x3c
 * (vs +0x21/+0x28), and (c) the array here is RAW CODE-POINTERS (each
 * entry called directly as `entry(param_2)`, a plain 1-arg function
 * pointer - not a 0x20-byte struct with an offset call like the other
 * function). Genuinely a different, smaller callback list, not the same
 * table reused - presented together only for the shared "walk per-channel
 * callback array keyed by chan_index_table_base" idiom, per this
 * project's own "additive, not force-fit" convention.
 *
 * Sole caller (per xrefs_to): FUN_c000e8d0 at call site 0xc000e910 -
 * FUN_c000e8d0 (immediately following this cluster, out of range) itself
 * calls FUN_c000ca60 (midi_hw_port_enable, midi_engine.c - already
 * reconstructed) before calling this function, real confirmed context:
 * this is a "port enable state changed, notify the per-channel enable
 * callback list" step.
 *
 * @0xc000e8c0.
 */

#include <stdint.h>
#include <stdbool.h>

extern void midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val);	/* FUN_c0000c38, soc_irq_gate.c */
extern void chan_link_tx(uint32_t target, uint8_t subcode, const void *buf, uint32_t len);	/* FUN_c000c1f0, chan_param_ctrl.c */
extern uint8_t *chan_index_table_base;		/* DAT_.../ci base, chan_param_ctrl.c -> 0xc001f6c4 */

/* chan_port_ctx_notify_b - see file header for the real 2-arg vs 1-formal-
 * parameter arity mismatch (`len` is an unused phantom argument, kept to
 * match chan_slot_dispatch.c's own already-established extern exactly). */
void chan_port_ctx_notify_b(uint32_t *ctx, uint32_t len)	/* FUN_c000e748 */
{
	uint8_t *D = (uint8_t *)ctx;
	(void)len;	/* phantom, unused in the real body - see header note */

	*(uint16_t *)(D + 0x180) = 0;
	D[0x17c] = 0;
	*(uint16_t *)(D + 0x17e) = 0;

	/* **((uint32_t **)(D + 4)) - two levels of indirection, a raw hw
	 * base address, NOT the single-deref "target" id chan_param_ctrl.c
	 * documents for this same offset elsewhere - see header note. */
	midi_hw_write16(**(uint32_t **)(D + 4), 0x60, 0x80);
}

/* chan_desc_setup_reply_notify - desc+0x11 sub-command byte, desc+0x12
 * high-byte request-class gate, desc+0x15f/0x160 reply stage. See file
 * header for the full case breakdown. */
bool chan_desc_setup_reply_notify(void *desc)	/* FUN_c000e76c */
{
	uint8_t *D = (uint8_t *)desc;
	uint16_t req_class = *(uint16_t *)(D + 0x12) & 0xff00u;
	uint32_t target;
	uint8_t reply_byte;
	unsigned clear_off;

	if (req_class == 0x0100) {
		if (*(int16_t *)(D + 0x16) != 1)
			return false;
		D[0x15f] = 0;
		chan_link_tx(*(uint32_t *)(D + 4), 0, D + 0x15f, 1);
		return true;
	}

	if (req_class != 0x0200)
		return false;

	switch (D[0x11]) {
	case 0x81:
		target = *(uint32_t *)(D + 4);
		reply_byte = 0xf8;
		D[0x160] = reply_byte;
		clear_off = 0x15f;
		D[clear_off] = 0;
		break;
	case 0x82:
		target = *(uint32_t *)(D + 4);
		reply_byte = 0x80;
		D[0x160] = reply_byte;
		clear_off = 0x15f;
		D[clear_off] = 0;
		break;
	case 0x83:
		target = *(uint32_t *)(D + 4);
		D[0x15f] = 0;
		D[0x160] = 0;
		chan_link_tx(target, 0, D + 0x15f, 2);
		return true;
	case 0x84:
		target = *(uint32_t *)(D + 4);
		D[0x15f] = 0x40;
		D[0x160] = 0;
		chan_link_tx(target, 0, D + 0x15f, 2);
		return true;
	default:
		return false;
	}

	chan_link_tx(target, 0, D + 0x15f, 1);
	return true;
}

/* chan_desc_slot_vtable_notify - re-inits desc+0x08/+0x09/+0x0a/+0x0c,
 * then calls each of the channel's per-slot function pointers
 * (chan_index_table_base[chan]+0x28, count at +0x21, 0x20-byte stride,
 * fn at slot+0x0c). See file header for the +9 double-indirection note. */
void chan_desc_slot_vtable_notify(void *desc, uint8_t index, uint32_t class0_val)	/* FUN_c000e8a8 */
{
	uint8_t *D = (uint8_t *)desc;
	uint8_t chan;
	uint8_t *ci;
	uint32_t count;
	uint8_t *slot;
	void (*fn)(uint32_t, uint32_t, uint16_t, uint16_t);

	D[8]  = 0;
	*(uint32_t *)(D + 0xc) = class0_val & 0xff;
	D[9]  = index;
	D[10] = 0;

	/* desc+0x00 holds &fixed_global_cell, whose own stored value is
	 * desc's own address - one extra hop vs. chan_param_ctrl.c's
	 * direct D[9] read; see header note. */
	chan = *((uint8_t *)(*(uint32_t **)D) + 9);
	ci = chan_index_table_base + (uint32_t)chan * 0x44;
	count = ci[0x21];
	slot = *(uint8_t **)(ci + 0x28);

	while (count != 0) {
		fn = *(void (**)(uint32_t, uint32_t, uint16_t, uint16_t))(slot + 0xc);
		fn(*(uint32_t *)slot, slot[4], *(uint16_t *)(slot + 2), *(uint16_t *)(slot + 6));
		count--;
		slot += 0x20;
	}
}

/* chan_desc_enable_vtable_notify - sets desc+0x0a = state, then calls a
 * raw 1-arg function-pointer array (chan_index_table_base[chan]+0x3c,
 * count at +0x25) with `state` as the sole argument. See file header for
 * why this is a genuinely different, smaller table than the function
 * above, not the same one reused. */
void chan_desc_enable_vtable_notify(void *desc, uint8_t state)	/* FUN_c000e8c0 */
{
	uint8_t *D = (uint8_t *)desc;
	uint8_t chan;
	uint8_t *ci;
	uint32_t count;
	void (**fn)(uint8_t);

	D[10] = state;

	chan = *((uint8_t *)(*(uint32_t **)D) + 9);
	ci = chan_index_table_base + (uint32_t)chan * 0x44;
	count = ci[0x25];
	if (count == 0)
		return;

	fn = *(void (***)(uint8_t))(ci + 0x3c);
	while (count != 0) {
		(*fn)(state);
		fn++;
		count--;
	}
}
