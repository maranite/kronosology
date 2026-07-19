/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_slot_dispatch.c - reconstructs the assigned address range
 * 0xc000bedc-0xc000c39c (14 real Ghidra function objects, no gaps: last
 * function FUN_c000c2b8 sits directly against the range's own exclusive
 * upper bound, FUN_c000c39c, which is itself the very "port interrupt
 * dispatcher" this file's own functions are called from - see "CROSS-FILE
 * FINDING" below).
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json), 2026-07-18 pass. No live Ghidra MCP calls this pass (the
 * bridge is flagged concurrency-unsafe under this project's own parallel
 * work).
 *
 * THIS IS chan_param_ctrl.c's OWN "SLOT DISPATCH CLUSTER" - CONFIRMED, not
 * a guess. chan_param_ctrl.c's own file header and its Part B comment
 * (chan_desc_dispatch_enabled) explicitly flag "FUN_c000c094/FUN_c000b1c8,
 * 0xc000bxxx-0xc000cxxx" as an out-of-range cluster that calls into its
 * own Part B accessor. Direct confirmation, traced this pass:
 *   - FUN_c000c094 and FUN_c000c2b8 (both in THIS file's range) call
 *     FUN_c000dd90 (chan_desc_dispatch_enabled, chan_param_ctrl.c) on a
 *     "desc" pointer reached via the exact same double-indirection chain
 *     ("port+4 -> pointer -> desc") documented there.
 *   - Both also index the SAME two globals chan_param_ctrl.c's own Part C
 *     already names chan_index_table_base (DAT_c000ded8/df4c/e118) and
 *     chan_bitmask_table_base (DAT_c000ded4/df54): this file's own
 *     DAT_c000b14c/DAT_c000b0d8 and DAT_c000b150/DAT_c000b0dc resolve to
 *     the IDENTICAL literal pointer values (0xc001f6c4 / 0xc001f690,
 *     verified by direct data_value comparison via query_dump.py dat) as
 *     chan_param_ctrl.c's own chan_slot_dispatch (FUN_c000b1c8, address
 *     0xc000b1c8, just BELOW this file's own range and not reconstructed
 *     here) - which itself uses DAT_c000b264/DAT_c000b268, also confirmed
 *     bit-for-bit identical to the same two globals.
 *   - FUN_c000b1c8 is cited by chan_param_ctrl.c as reading a 0x20-byte
 *     "slot" struct at chan_index_table_base[chan]+0x28 and calling a
 *     function pointer at that slot's own +0x14 or +0x1c. This file's own
 *     FUN_c000c094 calls the SAME table's +0x14 slot (zero args); its own
 *     FUN_c000c2b8 calls the SAME table's +0x10 slot (two args). Three
 *     different call sites into the same per-channel, per-value-code
 *     callback vtable, differing only in which of (at least) three
 *     function-pointer fields (+0x10/+0x14/+0x1c) they invoke and with
 *     what arguments - genuinely one shared dispatch mechanism, not three
 *     coincidentally-similar ones.
 *
 * ANCHOR: NONE, independently reconfirmed this pass via a fresh
 * `query_dump.py strings .cpp` sweep (14 anchors total, all already
 * claimed by other files - matches chan_param_ctrl.c's own "no anchor"
 * finding). No fault-call `file` argument in this range resolves to any
 * string either - every error/early-out path in this cluster is a plain
 * early `return`, same structural note chan_param_ctrl.c already made
 * about its own address-adjacent range.
 *
 * WHAT THIS FILE ACTUALLY IS: the low-level "port" object underneath
 * chan_param_ctrl.c's own "link" object. chan_param_ctrl.c's Part A
 * describes link+0x00 only as an opaque "target handle, passed to
 * chan_link_tx/_ack/etc." - this file resolves what that handle concretely
 * points to. Every function here takes that same value (renamed `port` in
 * this file) and dereferences it directly: word[0] (+0x00) is fed straight
 * into midi_hw_write16/midi_hw_read16 (FUN_c0000c38/_c6c, already named in
 * midi_engine.c) as their own MMIO base-selector argument - i.e. a `port`
 * is a handle onto ONE per-channel register bank of the SAME external
 * multi-port MIDI-shaped UART chip midi_engine.c's own header documents
 * (EMIFA CS3, base 0x62000000, per-channel +0x20-stride register groups).
 * The register offsets touched directly in this file (0x0e, 0x32/0x34,
 * 0x36/0x38, 0x60/0x68 (+channel*0x20), 0x6e (+channel*0x20), 0x72/0x74,
 * 0x80/0xa0/0xc0/0xe0, 0x96/0xb6/0xd6/0xf6, 0x120/0x140/0x160/0x180) sit
 * in the same register file midi_engine.c and (via FUN_c000c39c, see
 * below) the not-yet-reconstructed interrupt dispatcher both touch -
 * consistent with one shared hardware block, not independently verified
 * against a datasheet.
 *
 * "port" object - confirmed byte offsets (opaque handle, matches this
 * project's established convention for large non-contiguous structs, see
 * chan_param_ctrl.c's own "link"/"desc" notes):
 *   +0x00  uint32  reg_base     - MMIO base/selector for midi_hw_write16/
 *                                 _read16 and the two FIFO helpers below
 *   +0x04  uint32  desc_sel     - a pointer whose OWN target is the "desc"
 *                                 object chan_param_ctrl.c's Part B/C
 *                                 operates on (`*(*(port+4))` == desc,
 *                                 the same double-indirection FUN_c000b1c8
 *                                 performs on its own first parameter)
 *   +0x08  uint32  ctx          - opaque handle forwarded, unexamined, to
 *                                 three out-of-range functions
 *                                 (chan_port_ep_read_setup_len/_ctx_notify_a/
 *                                 _ctx_notify_b below) - FUN_c000e924's own
 *                                 body double-dereferences ctx+4 to reach a
 *                                 reg_base-shaped value again, so `ctx` is
 *                                 plausibly one more wrapper layer around
 *                                 (or containing) this same port, NOT
 *                                 independently confirmed as a distinct
 *                                 object
 *   +0x0c  uint32  tx_buf       - staged TX buffer pointer
 *   +0x10  uint32  tx_len       - staged TX remaining length
 *   +0x14  uint8[64] scratch    - inline RX scratch buffer
 *   +0x54  void*   xfer_slots   - pointer to a 1-BASED array of per-slot
 *                                 "xfer" object pointers (index*4-4 -
 *                                 confirmed by chan_decode_result_get's
 *                                 own arithmetic)
 *   +0x58  uint32  reply_len       (word index 0x16)
 *   +0x5c  uint32  reply_remaining (word index 0x17)
 *   +0x60  uint32  done_flag       (word index 0x18)
 *   +0x64  uint32  unused-in-this-file (word index 0x19) - written 0 by
 *                                 chan_port_tx_stage_and_pump's own 4th
 *                                 (always-literal-0) argument, never read
 *                                 anywhere in this file's own range
 *
 * "xfer" slot object (chan_port_xfer_slot, 0x20 bytes, reached via
 * port->xfer_slots[idx]) - DIFFERENT table from the callback-vtable slot
 * below, despite the superficial resemblance (do not conflate):
 *   +0x04  uint32  max_chunk  (clamp bound, usbdc_min_u32)
 *   +0x08  uint32  orig_buf   (preserved across a multi-call pump)
 *   +0x0c  uint32  orig_len   (preserved across a multi-call pump)
 *   +0x10  uint32  cursor     (mutable, advances)
 *   +0x14  uint32  remaining  (mutable, decreases to 0)
 *   +0x18  uint32  start_buf  - read only by chan_port_xfer_slot_rx_pump's
 *                              own "not yet started" branch; NOT
 *                              independently confirmed distinct from
 *                              +0x08 (may be a second, RX-specific seed
 *                              pointer) - flagged, not resolved
 *
 * "value callback" slot object (chan_value_callback_slot, 0x20 bytes,
 * reached via chan_index_table_base[chan]+0x28 + code*0x20, code coming
 * from a >>1&7 decode of a bit read out of chan_bitmask_table_base -
 * SAME mechanism chan_param_ctrl.c's own chan_slot_dispatch, FUN_c000b1c8,
 * uses): confirmed function-pointer fields +0x10 (2-arg, "value
 * delivered"), +0x14 (0-arg, "notify"); chan_param_ctrl.c's own
 * FUN_c000b1c8 additionally documents +0x1c (1-arg bool). A `code` value
 * of 7 is the documented "no slot"/disabled sentinel throughout.
 *
 * CROSS-FILE FINDING: FUN_c000c39c (0xc000c39c, 1836 bytes per Ghidra's
 * own size field, though that field is unreliable here - see note on
 * chan_port_xfer_slot_rx_pump below), the function immediately AFTER this
 * file's own range, is confirmed (by direct decompile, not merely by
 * address adjacency) to be the real per-port interrupt-status dispatcher:
 * it reads reg 0x0c, masks with DAT_c000ca08, and branches through nested
 * bit tests on regs 0x0e/0x36/0x38/0x72/0x74, calling STRAIGHT into four
 * of this file's own functions (chan_port_reset_reply_len,
 * chan_port_reg60_rx_pump, chan_port_slot_notify,
 * chan_port_xfer_slot_rx_pump) as well as chan_param_ctrl.c's own Part C
 * apply-handlers (FUN_c000e2f8/_e3c8/_e454, i.e.
 * chan_class2_apply_hi_or_lo/_apply_readback, chan_class0_apply_value) and
 * two further out-of-range functions (FUN_c000e8d0, FUN_c000e498). This
 * is the single missing piece that ties this file, chan_param_ctrl.c, and
 * (transitively, via the shared register file) midi_engine.c together as
 * one hardware subsystem - genuinely useful for whichever future pass
 * covers 0xc000c39c onward, NOT reconstructed here (out of this file's
 * own assigned range).
 *
 * STILL OPEN (honest, not fabricated):
 *  - chan_port_xfer_slot_tx_pump (FUN_c000c168) and
 *    chan_port_xfer_slot_init (FUN_c000c22c) both have ZERO static
 *    callers anywhere in the full 691-function xref data - the same
 *    "plausibly vtable-dispatched, not visible to static analysis"
 *    pattern already documented repeatedly elsewhere in this project
 *    (crypto_at88.c, clcdc.c, omap_l137_usbdc_ep0.c). Not asserted to be
 *    the callback-vtable's own +0x10/+0x14/+0x1c targets - their
 *    signatures don't cleanly match any of those call sites' argument
 *    counts - genuinely unresolved.
 *  - FUN_c000c2b8's own Ghidra-reported size (348 bytes) numerically
 *    overlaps FUN_c000c39c's start address (0xc000c2b8+348 = 0xc000c414,
 *    past 0xc000c39c) despite FUN_c000c39c being independently confirmed
 *    a real, separate function object. The decompiled body transcribed
 *    below is self-contained and well-formed; the size field itself is
 *    not trusted here (almost certainly includes an unreachable/
 *    deadcode tail Ghidra's own decompiler comments elsewhere in this
 *    project have flagged with "Removing unreachable block" warnings on
 *    neighboring functions) - flagged, not silently corrected away.
 *  - chan_port_reg60_rx_pump's own dispatch on the global flag byte at
 *    chan_port_hwctx_global+1 (bit 0x20) - mechanically transcribed, but
 *    what distinguishes the two forwarding modes (chan_port_ctx_notify_a
 *    vs _b) is not decoded.
 *  - The "xfer" slot's own +0x18 field's precise relationship to +0x08
 *    (see struct note above).
 *  - FUN_c000b1c8 (chan_slot_dispatch) and FUN_c000b630's own callers
 *    other than the ones cited here sit below this file's own range
 *    (0xc000b1c8/0xc000b630 < 0xc000bedc) and are not reconstructed here.
 *  - chan_port_reg60_rx_pump's own RX-ready ack write (reg 0x60, inside
 *    the `status60 & 0x200` branch) is a genuinely 2-visible-argument
 *    Ghidra call (`FUN_c0000c38(*param_1,0x60)`) with no 3rd (value)
 *    argument shown; no raw disassembly was available this pass to
 *    recover the true register content. Modeled here as writing back
 *    status60 itself (a plausible write-1-to-clear idiom) - NOT
 *    confirmed, see that function's own inline comment.
 */

#include <stdint.h>
#include <stdbool.h>

/* ---- shared low-level hardware primitives (out of this file's range) ---- */
extern void     midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val);	/* FUN_c0000c38, see midi_engine.c */
extern uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off);			/* FUN_c0000c6c, see midi_engine.c */
/* chan_hw_fifo_write - real call arity varies: 4 args (base, chan, buf, len)
 * at chan_port_tx_stage_and_pump's own call site, but only 2 (base, byte)
 * at chan_link_tx_oob's - same function address both times. Declared here
 * at its widest observed arity; the 2-arg call site simply leaves the
 * upper two AAPCS registers (r2/r3) with whatever the caller last set
 * them to, per this project's own established "phantom parameter" /
 * unused-argument precedent (see cdix4192.c, eva_board_watchdog_fault_wrapper). */
extern void     chan_hw_fifo_write(uint32_t base_sel, uint32_t chan_or_byte, const void *buf, uint32_t len);	/* FUN_c0000c98 */
extern uint32_t chan_hw_fifo_read(uint32_t base_sel, uint32_t chan, void *buf, uint32_t len);			/* FUN_c0000dec */
extern uint32_t usbdc_min_u32(uint32_t a, uint32_t b);		/* FUN_c000aee8, see omap_l137_usbdc_ep0.c - clamp helper */
/* chan_decode_result_get - FUN_c000b630, defined in chan_link_hw.c (this
 * project's own reconstruction of the 0xc000b414-0xc000b898 range, done
 * concurrently with this file this same pass). That file's own doc frames
 * its return value generically as one slot of a "6-word decode-result
 * array"; every real caller found there IS this file's own
 * chan_port_xfer_slot_tx_pump/_init/_rx_pump (confirmed by that file's own
 * citation) - the returned uint32_t is, from this file's own vantage
 * point, always a POINTER to one of THIS file's 0x20-byte xfer_slot
 * objects (dereferenced with +0x04/+0x08/+0x0c/+0x10/+0x14/+0x18 offsets
 * below). The two framings are consistent, not contradictory: the shared
 * "decode-result array" is this file's own port->xfer_slots table, and
 * each of its 6 words is itself one of these slot pointers. Reused here
 * under chan_link_hw.c's own name rather than inventing a second one for
 * the same address. */
extern uint32_t chan_decode_result_get(uint32_t *arr, uint32_t idx);	/* FUN_c000b630 */

/* ---- chan_param_ctrl.c's own Part B accessor, defined there ---- */
extern uint8_t  chan_desc_dispatch_enabled(void *desc);	/* FUN_c000dd90, chan_param_ctrl.c */

/* ---- the two shared per-channel tables, confirmed identical to
 * chan_param_ctrl.c's own chan_index_table_base/chan_bitmask_table_base
 * (see file header) - re-declared here under the SAME names for
 * cross-file readability; each .c file in this project is compiled as
 * its own standalone translation unit (-fsyntax-only, no link step), so
 * the duplicate extern declaration is harmless. ---- */
extern uint8_t *chan_index_table_base;		/* DAT_c000b14c/DAT_c000b0d8 here -> 0xc001f6c4 */
extern uint8_t *chan_bitmask_table_base;	/* DAT_c000b150/DAT_c000b0dc here -> 0xc001f690 */

/* ---- out-of-range EP0/link-layer forwarding calls (chan_param_ctrl.c's
 * own "FUN_c000e924" family; not reconstructed there either) ---- */
extern uint16_t chan_port_ep_read_setup_len(uint32_t ctx);			/* FUN_c000e924 */
extern void      chan_port_ctx_notify_a(uint32_t ctx, void *buf, uint32_t len);	/* FUN_c000eba8 */
extern void      chan_port_ctx_notify_b(uint32_t ctx, uint32_t len);			/* FUN_c000e748 */

/* module-scope global, a pointer variable (not a fixed constant) - see
 * chan_port_reg60_rx_pump below. Real purpose of the object it points to
 * is not decoded. */
extern void *chan_port_hwctx_global;	/* DAT_c000c034 */

/* ===========================================================================
 * chan_link_timeout_notify / chan_port_signal_timeout
 * ===========================================================================
 */

/* ------------------------------------------------------------------------- *
 *  chan_port_signal_timeout - writes the literal value 2 into either reg
 *  0x60 (subcode 0) or reg (subcode*0x20 + 0x68) (subcode != 0) of the
 *  port's own register bank. Sole caller: chan_link_timeout_notify, below.
 *  @0xc000bedc (44 bytes).
 * ------------------------------------------------------------------------- */
static void chan_port_signal_timeout(void *port, uint32_t subcode)	/* FUN_c000bedc */
{
	uint32_t reg_base = *(uint32_t *)port;
	uint32_t reg_off = 0x60;

	if ((subcode & 0xff) != 0)
		reg_off = (subcode & 0xff) * 0x20 + 0x68;

	midi_hw_write16(reg_base, reg_off, 2);
}

/* ------------------------------------------------------------------------- *
 *  chan_link_timeout_notify - thin wrapper over chan_port_signal_timeout.
 *  Named/typed to match chan_param_ctrl.c's own extern declaration for
 *  this exact address (FUN_c000bf08), which is chan_link_watchdog_tick's
 *  own timeout-path call. Also called (3 further sites, out of this
 *  file's range) from FUN_c000e498 - the function immediately following
 *  chan_param_ctrl.c's own Part C, not reconstructed by either file.
 *  @0xc000bf08 (8 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_timeout_notify(uint32_t target, uint8_t subcode)	/* FUN_c000bf08 */
{
	chan_port_signal_timeout((void *)(uintptr_t)target, subcode);
}

/* ------------------------------------------------------------------------- *
 *  chan_port_reset_reply_len - clears the port's own "done" flag (+0x60),
 *  re-reads the current SETUP/reply length via the out-of-range
 *  chan_port_ep_read_setup_len (called with the port's own `ctx` field,
 *  +0x08), and stores that same 16-bit value into BOTH +0x58
 *  (reply_len) and +0x5c (reply_remaining) - i.e. arms a fresh
 *  reply-length countdown. Sole caller: FUN_c000c39c (the port interrupt
 *  dispatcher, out of range - see file header), reg-0x72 bit-0x100000
 *  branch.
 *  @0xc000bf10 (52 bytes).
 * ------------------------------------------------------------------------- */
void chan_port_reset_reply_len(void *port)	/* FUN_c000bf10 */
{
	uint8_t *P = (uint8_t *)port;
	uint16_t len;

	*(uint32_t *)(P + 0x60) = 0;
	len = chan_port_ep_read_setup_len(*(uint32_t *)(P + 8));
	*(uint32_t *)(P + 0x5c) = len;
	*(uint32_t *)(P + 0x58) = len;
}

/* ------------------------------------------------------------------------- *
 *  chan_port_reg60_rx_pump - services reg 0x60/0x68's RX side: if reg
 *  0x60 bit 0x200 (RX-ready) is set, acks it by clearing the bit; reads
 *  up to 64 bytes from reg 0x68 into the port's own 64-byte inline
 *  scratch buffer (+0x14); then, gated on a module-scope global flag byte
 *  (*(chan_port_hwctx_global+1), bit 0x20 - purpose not decoded),
 *  forwards either:
 *    - flag clear: the SAME scratch buffer, with length taken from the
 *      port's own CACHED reply_len (+0x58, set by chan_port_reset_reply_len
 *      earlier) rather than the byte count just read - via
 *      chan_port_ctx_notify_a - but only if reply_len is nonzero and the
 *      done flag (+0x60) is still clear;
 *    - flag set: the JUST-READ byte count, via chan_port_ctx_notify_b.
 *  Tail: re-reads the same global flag byte; if still clear, compares the
 *  raw reg-0x68 count against the actual bytes read and, if they match
 *  (i.e. the whole pending FIFO content was drained in one shot), sets
 *  reg 0x60 bit 0x80; if the flag was set, clears it (one-shot
 *  consumption). Sole caller: FUN_c000c39c (reg-0x72 bit-0x400000
 *  branch, out of range).
 *  @0xc000bf54 (224 bytes).
 * ------------------------------------------------------------------------- */
void chan_port_reg60_rx_pump(void *port)	/* FUN_c000bf54 */
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
		/* real decompile: FUN_c0000c38(*param_1,0x60) - only TWO visible
		 * args, no raw disassembly available to recover the true 3rd
		 * (value) argument register. Written here as status60 itself
		 * (a plausible write-1-to-clear ack of the just-read status,
		 * matching this register's own W1C-shaped use elsewhere in this
		 * file), NOT confirmed - flagged, not guessed-and-hidden. */
		midi_hw_write16(reg_base, 0x60, status60);

	raw_count = midi_hw_read16(reg_base, 0x68);
	clamped = usbdc_min_u32(raw_count & 0xffff, 0x40);
	got = chan_hw_fifo_read(reg_base, 0, P + 0x14, clamped);

	flagbyte = (uint8_t *)chan_port_hwctx_global;

	if ((flagbyte[1] & 0x20) == 0) {
		if (*(uint32_t *)(P + 0x58) != 0 && *(uint32_t *)(P + 0x60) == 0)
			chan_port_ctx_notify_a(*(uint32_t *)(P + 8), P + 0x14, *(uint32_t *)(P + 0x58) & 0xffff);
	} else {
		chan_port_ctx_notify_b(*(uint32_t *)(P + 8), got & 0xffff);
	}

	/* real decompile re-reads the same global flag byte a second time -
	 * transcribed faithfully rather than reusing the first read, in case
	 * chan_port_ctx_notify_a/_b themselves mutate it. */
	if ((flagbyte[1] & 0x20) == 0) {
		if ((raw_count & 0xffff) == got)
			midi_hw_write16(reg_base, 0x60, 0x80);
		return;
	}
	flagbyte[1] &= 0xdf;
}

/* ===========================================================================
 * chan_link_set_mode / chan_link_ack and the callback-vtable dispatch
 * built on top of them.
 * ===========================================================================
 */

/* ------------------------------------------------------------------------- *
 *  chan_link_set_mode - clears reg-0x0e bit 0x100 (mode 4) or bit 0x80
 *  (mode 3); any other mode value is a no-op. Named/typed to match
 *  chan_param_ctrl.c's own extern declaration for this address
 *  (FUN_c000c038).
 *  @0xc000c038 (92 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_set_mode(uint32_t target, uint32_t mode)	/* FUN_c000c038 */
{
	uint32_t reg_base = *(uint32_t *)(uintptr_t)target;
	uint16_t r0e = midi_hw_read16(reg_base, 0x0e);

	if ((mode & 0xff) == 4)
		r0e &= ~0x0100;
	else if ((mode & 0xff) == 3)
		r0e &= ~0x0080;

	midi_hw_write16(reg_base, 0x0e, r0e);
}

/* ------------------------------------------------------------------------- *
 *  chan_port_slot_notify - the FUN_c000c094 half of the "slot dispatch
 *  cluster". Clears the mode bit for `code` (via chan_link_set_mode),
 *  then, if the desc reached through port->desc_sel is dispatch-enabled,
 *  looks up a channel index at desc+9, reads a bit out of
 *  chan_bitmask_table_base[chan*8+4] (the "hi" half - see
 *  chan_class2_test_hi's own bt+4 use in chan_param_ctrl.c) for `code`,
 *  decodes it to a 0-7 slot number, and - unless 7 (disabled) - calls
 *  the matching chan_value_callback_slot's own +0x14 function pointer
 *  with ZERO arguments (an ARM r0-reuse-shaped "notify/poll" callback,
 *  same idiom this project has repeatedly documented elsewhere). Sole
 *  caller: FUN_c000c39c (reg-0x0e bit-0x100 branch, out of range).
 *  @0xc000c094 (152 bytes).
 * ------------------------------------------------------------------------- */
void chan_port_slot_notify(void *port, uint32_t code)	/* FUN_c000c094 */
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

/* ------------------------------------------------------------------------- *
 *  chan_port_tx_stage_and_pump - stages (buf, len, flag) into the port's
 *  own tx_buf/tx_len/word[0x19] fields, ensures reg-0x60 bit 0x200 (TX
 *  enable) is set, clamps the pending length to 64 bytes
 *  (usbdc_min_u32), writes that clamp's worth of bytes from tx_buf into
 *  the hardware FIFO (chan_hw_fifo_write, channel 0), advances
 *  tx_buf/decrements tx_len, and - unless a full 64-byte chunk went out
 *  with more still pending (i.e. genuinely mid-burst) - marks the port's
 *  own done_flag (+0x60). Sole caller: chan_link_tx's own subcode==0
 *  path, below.
 *  @0xc000c158 (164 bytes).
 * ------------------------------------------------------------------------- */
static void chan_port_tx_stage_and_pump(void *port, void *buf, uint32_t len, uint32_t flag)	/* FUN_c000c158 */
{
	uint8_t *P = (uint8_t *)port;
	uint32_t reg_base;
	uint16_t status60;
	uint32_t clamped;
	uint32_t remaining_before;

	*(uint32_t *)(P + 0x64) = flag;	/* word[0x19] - never read elsewhere in this file */
	*(uint32_t *)(P + 0x10) = len;		/* tx_len */
	*(uint32_t *)(P + 0x0c) = (uint32_t)(uintptr_t)buf;	/* tx_buf */

	reg_base = *(uint32_t *)P;
	status60 = midi_hw_read16(reg_base, 0x60);
	if ((status60 & 0x200) == 0)
		midi_hw_write16(reg_base, 0x60, 0x200);

	clamped = usbdc_min_u32(*(uint32_t *)(P + 0x10), 0x40);
	chan_hw_fifo_write(reg_base, 0, (void *)(uintptr_t)*(uint32_t *)(P + 0x0c), clamped);

	remaining_before = *(uint32_t *)(P + 0x10);
	*(uint32_t *)(P + 0x0c) += clamped;
	*(uint32_t *)(P + 0x10) = remaining_before - clamped;

	if (clamped > 0x3f) {
		if (*(uint32_t *)(P + 0x64) != 0)
			return;
		if (remaining_before - clamped != 0)
			return;
	}
	*(uint32_t *)(P + 0x60) = 1;	/* done_flag */
}

/* ------------------------------------------------------------------------- *
 *  chan_port_xfer_slot_tx_pump - the per-slot analogue of
 *  chan_port_tx_stage_and_pump above, operating on one entry of the
 *  port's own xfer_slots array (chan_decode_result_get, defined in
 *  chan_link_hw.c) instead of
 *  the port's own primary tx_buf/tx_len fields, and using the hardware
 *  channel number `slot_idx` itself as the FIFO channel selector rather
 *  than a fixed 0. Returns true only when this call drains the slot's
 *  own `remaining` to exactly 0.
 *
 *  ZERO STATIC CALLERS anywhere in the full 691-function xref data - the
 *  same "plausibly reached only through a function pointer this static
 *  dump can't see" situation documented repeatedly elsewhere in this
 *  project. NOT asserted to be one of the callback-vtable's own
 *  +0x10/+0x14/+0x1c slots (its 2-arg (port, uint8) signature doesn't
 *  cleanly match any of those observed call shapes) - genuinely open.
 *  @0xc000c168 (104 bytes).
 * ------------------------------------------------------------------------- */
bool chan_port_xfer_slot_tx_pump(void *port, uint8_t slot_idx)	/* FUN_c000c168 */
{
	uint8_t *P = (uint8_t *)port;
	uint8_t *slot = (uint8_t *)(uintptr_t)chan_decode_result_get((uint32_t *)*(void **)(P + 0x54), slot_idx);
	uint32_t remaining = *(uint32_t *)(slot + 0x14);
	uint32_t cursor = *(uint32_t *)(slot + 0x10);
	uint32_t clamped = usbdc_min_u32(remaining, *(uint32_t *)(slot + 4));
	uint32_t remaining_after = remaining - clamped;

	*(uint32_t *)(slot + 0x10) = cursor + clamped;
	chan_hw_fifo_write(*(uint32_t *)P, slot_idx, (void *)(uintptr_t)cursor, clamped);
	*(uint32_t *)(slot + 0x14) = remaining_after;

	return remaining_after == 0;
}

/* ------------------------------------------------------------------------- *
 *  chan_link_tx_oob - subcode!=0 path of chan_link_tx: sends `subcode`
 *  itself as a single out-of-band byte via chan_hw_fifo_write's own
 *  2-argument call form (see that extern's own note above), rather than
 *  queuing through chan_port_tx_stage_and_pump. Always reports success.
 *  Private helper - chan_param_ctrl.c only needs chan_link_tx's own
 *  extern, not this one directly.
 *  @0xc000c1d0 (32 bytes).
 * ------------------------------------------------------------------------- */
static uint32_t chan_link_tx_oob(void *port, uint8_t subcode)	/* FUN_c000c1d0 */
{
	chan_hw_fifo_write(*(uint32_t *)port, subcode, 0, 0);
	return 1;
}

/* ------------------------------------------------------------------------- *
 *  chan_link_tx - generic link transmit primitive. Named/typed to match
 *  chan_param_ctrl.c's own extern declaration for this address
 *  (FUN_c000c1f0), which is chan_link_send_reset_frame's own transmit
 *  call. subcode==0 queues (buf, len) through
 *  chan_port_tx_stage_and_pump; subcode!=0 instead sends subcode itself
 *  as a single OOB byte via chan_link_tx_oob.
 *
 *  RETURN-TYPE DISCREPANCY (documented, not silently fixed): the real
 *  decompile returns a value (1 on both paths, or chan_link_tx_oob's own
 *  return) but chan_param_ctrl.c's own extern declares this `void` -
 *  every real call site there (chan_link_send_reset_frame) discards the
 *  result, so the mismatch is harmless in practice, matching this
 *  project's own established precedent for this exact situation (see
 *  clcdc_draw_text's void-vs-return-value note in clcdc.c/
 *  panelbus_dispatch.c).
 *
 *  @0xc000c1f0 (60 bytes).
 * ------------------------------------------------------------------------- */
uint32_t chan_link_tx(uint32_t target, uint8_t subcode, const void *buf, uint32_t len)	/* FUN_c000c1f0 */
{
	if (subcode == 0) {
		chan_port_tx_stage_and_pump((void *)(uintptr_t)target, (void *)(uintptr_t)buf, len, 0);
		return 1;
	}
	return chan_link_tx_oob((void *)(uintptr_t)target, subcode);
}

/* ------------------------------------------------------------------------- *
 *  chan_port_xfer_slot_init - resets one xfer_slot entry: both the
 *  mutable cursor/remaining pair (+0x10/+0x14) AND the preserved
 *  orig_buf/orig_len pair (+0x08/+0x0c) are seeded from the same (buf,
 *  len) arguments - i.e. "start a fresh transfer on this slot".
 *
 *  ZERO STATIC CALLERS anywhere in the full 691-function xref data - same
 *  situation as chan_port_xfer_slot_tx_pump above, genuinely open.
 *  @0xc000c22c (52 bytes).
 * ------------------------------------------------------------------------- */
void chan_port_xfer_slot_init(void *port, uint8_t slot_idx, void *buf, uint32_t len)	/* FUN_c000c22c */
{
	uint8_t *P = (uint8_t *)port;
	uint8_t *slot = (uint8_t *)(uintptr_t)chan_decode_result_get((uint32_t *)*(void **)(P + 0x54), slot_idx);

	*(uint32_t *)(slot + 0x14) = len;
	*(uint32_t *)(slot + 0x10) = (uint32_t)(uintptr_t)buf;
	*(uint32_t *)(slot + 0x08) = (uint32_t)(uintptr_t)buf;
	*(uint32_t *)(slot + 0x0c) = len;
}

/* ------------------------------------------------------------------------- *
 *  chan_link_ack - sets reg-0x0e bit 0x100 (code 4) or bit 0x80 (code
 *  3); any other code is a no-op. The set-bits mirror of
 *  chan_link_set_mode above. Named/typed to match chan_param_ctrl.c's
 *  own extern declaration for this address (FUN_c000c260), which is
 *  chan_link_watchdog_tick's own handshake-abort call.
 *  @0xc000c260 (80 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_ack(uint32_t target, uint8_t code)	/* FUN_c000c260 */
{
	uint32_t reg_base = *(uint32_t *)(uintptr_t)target;
	uint16_t r0e = midi_hw_read16(reg_base, 0x0e);

	if (code == 4)
		r0e |= 0x0100;
	else if (code == 3)
		r0e |= 0x0080;

	midi_hw_write16(reg_base, 0x0e, r0e);
}

/* ------------------------------------------------------------------------- *
 *  chan_link_ack_retry - thin wrapper over chan_link_ack, used by
 *  chan_port_xfer_slot_rx_pump below to re-signal "more data to come"
 *  when a multi-call slot pump isn't finished yet. Kept as a distinct
 *  named function (rather than inlined) purely to mirror the real
 *  decompile's own separate function object.
 *  @0xc000c2b0 (8 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_ack_retry(uint32_t target, uint8_t code)	/* FUN_c000c2b0 */
{
	chan_link_ack(target, code);
}

/* ------------------------------------------------------------------------- *
 *  chan_port_xfer_slot_rx_pump - the RX/completion-callback counterpart
 *  of chan_port_xfer_slot_tx_pump: reads from the hardware FIFO (reg
 *  (chan*0x20 + 0x6e) for the available-count, chan_hw_fifo_read for the
 *  data itself - NOTE: chan_hw_fifo_read, not chan_hw_fifo_write, unlike
 *  every other function in this file) into an xfer_slot's own
 *  cursor/remaining pair. Two shapes:
 *    - slot->remaining == 0 (not yet started): seeds the cursor from
 *      slot->start_buf (+0x18, see struct note in file header), reads
 *      one clamped chunk, and falls straight through to the completion
 *      callback below using slot->start_buf and the just-read byte count
 *      as its two arguments - regardless of whether the whole transfer
 *      actually completed in this one read. Transcribed exactly as
 *      decompiled; flagged as a possible real firmware quirk, not
 *      resolved further.
 *    - slot->remaining != 0 (continuing): reads one clamped chunk into
 *      the existing cursor, advances/decrements; if still not fully
 *      drained, re-signals via chan_link_ack_retry and returns; once
 *      fully drained, falls through to the completion callback using the
 *      slot's own PRESERVED orig_buf/orig_len (+0x08/+0x0c).
 *  Completion callback: same desc-dispatch-enabled gate and
 *  chan_bitmask_table_base lookup as chan_port_slot_notify above, but
 *  using the "lo" half of the table (chan_bitmask_table_base[chan*8],
 *  no +4 offset - the mirror of chan_port_slot_notify's own "hi" half
 *  use) and invoking the matching chan_value_callback_slot's own +0x10
 *  function pointer (not +0x14) with the (buf, len) pair as its two
 *  arguments.
 *
 *  Sole caller: FUN_c000c39c (reg-0x0e bit-0x80 branch, out of range,
 *  passing subcode 3). See file header for the size-field caveat on this
 *  function's own @-citation.
 *  @0xc000c2b8.
 * ------------------------------------------------------------------------- */
void chan_port_xfer_slot_rx_pump(void *port, uint32_t chan)	/* FUN_c000c2b8 */
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
		cb_len = chan_hw_fifo_read(reg_base, chan, start_buf, clamped);
		cb_buf = start_buf;

		desc_sel = *(void ***)(P + 4);
	} else {
		void *cursor = (void *)(uintptr_t)*(uint32_t *)(slot + 0x10);
		uint32_t new_remaining;

		chan_hw_fifo_read(reg_base, chan, cursor, clamped);
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
