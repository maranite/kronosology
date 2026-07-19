/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cobjectmgr.c - the firmware's small C++ "active object" manager: a single
 * current-object slot, polled once per dispatch-loop tick, dispatching on a
 * type tag and releasing the object when done.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17/18.
 * Anchor: "../cobjectmgr.cpp" has 6 real xrefs (more than CryptoAt88.cpp's or
 * clcdc.cpp's single anchor each) - this subsystem's actual boundary is less
 * clean than those two. Only the functions with a genuinely confirmed
 * "object manager" role are reconstructed here; the other anchor xrefs are
 * documented as separate, honestly-scoped findings below rather than forced
 * into this file under a label they may not deserve - see README.md's own
 * status section for the full accounting.
 *
 * Anchor-sweep pass, 2026-07-18: re-swept every literal-pool reference to the
 * "../cobjectmgr.cpp" string address (0xc0022dcc) directly, rather than
 * relying on the earlier "6 xrefs" count alone. Found NINE functions with
 * their own local copy of that string pointer, not six:
 * cobjectmgr_tick (FUN_c0007c2c), cobjectmgr_handle_type_a (FUN_c0007ad0),
 * FUN_c0005c50, FUN_c00090b8, FUN_c0007220 (all previously known), plus
 * THREE not previously catalogued: FUN_c0005cc4, FUN_c0005d34 (a small
 * family of host-notify-event senders, same shape as FUN_c0005c50, all
 * three called from the master dispatcher FUN_c0008b64 and all sharing one
 * output channel constant - see the note below FUN_c0005c50's own
 * reconstruction), and FUN_c0007d1c (the firmware's central wire-protocol
 * command dispatcher, already cited by name in cad.c/cpsoc.c - genuinely
 * important new finding: it's the actual PRODUCER of this file's own
 * "current object" slot, see the note above cobjectmgr_tick). Neither of
 * these expands cobjectmgr.c's own reconstructed function set beyond what
 * was already planned, but the discrepancy itself (9 vs. 6) is recorded
 * here rather than silently left at the old count.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Shared LCD drawing primitives, duplicated here from clcdc.c per this
 *  project's own per-file convention (no shared headers - see clcdc.c's own
 *  struct clcdc_cursor for the authoritative version). Confirmed identical
 *  underlying globals: DAT_c0007c24/DAT_c0007c28/DAT_c0007c20 (this file's
 *  own literal-pool copies, used by cobjectmgr_handle_type_a below) resolve
 *  to the exact same addresses as clcdc.c's DAT_c0015418/DAT_c0015410 etc
 *  (clcdc_framebuffer / clcdc_fb_pixel_count_limit), and cobjectmgr_handle_
 *  type_b's own DAT_c0007ac8 resolves to the same address as clcdc_palette -
 *  i.e. BOTH of this file's handlers draw directly into clcdc's own
 *  framebuffer/palette, not some independent lookup table. A genuinely new
 *  structural finding this pass: cobjectmgr sits on top of clcdc as a
 *  renderer for at least two wire-triggered object types.
 * ------------------------------------------------------------------------- */
struct clcdc_cursor {
	uint8_t  pad0[4];
	uint32_t stride;		/* +4 */
	uint8_t  pad1[2];
	int16_t  x, y;			/* +8, +10 */
	int16_t  left_margin;		/* +0xc */
	int16_t  right_edge;		/* +0xe */
};
extern void clcdc_cursor_init_from_offset(struct clcdc_cursor *c, uint32_t offset, int width);	/* FUN_c001505c */
extern uint16_t *clcdc_framebuffer;		/* *DAT_c0007c24 here, same target as clcdc.c's DAT_c0015418 */
extern uint16_t *clcdc_palette;		/* *DAT_c0007c28/DAT_c0007ac8 here, same target as clcdc.c's DAT_c0015418c */
extern uint32_t  clcdc_fb_pixel_count_limit;	/* DAT_c0007c20 here, same target as clcdc.c's DAT_c0015410 (=479999=800*600-1) */

extern struct clcdc_cursor *cobjectmgr_draw_cursor;	/* DAT_c0007c1c - this file's own cursor instance, not clcdc.c's */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000919c, shared firmware-wide hard-halt assert (see crypto_at88.c) */

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_state - the shared per-tick dispatch struct. Confirmed used
 *  identically by cobjectmgr_tick, cobjectmgr_handle_type_a AND
 *  cobjectmgr_handle_type_b (all three read/write the same +8/+0xc/+0x10
 *  offsets) - see the register-continuity note below cobjectmgr_tick for how
 *  this was established despite Ghidra showing the two handler calls with no
 *  visible arguments.
 * ------------------------------------------------------------------------- */
struct cobjectmgr_state {
	uint8_t  pad0[8];
	int32_t  stream_remaining;	/* +8: remaining byte count in the pending command payload */
	int32_t  scratch_count;	/* +0xc: type_a's decoded run-length / general scratch */
	uint8_t *current_object;	/* +0x10: payload read-cursor; NULL when idle (doubles as the "occupied" flag) */
};

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_handle_type_a - tag 0xc4 (-0x3c) handler: a solid-colour
 *  pixel-run drawer. @0xc0007ad0.
 *
 *  Re-validates the tag itself (redundant with cobjectmgr_tick's own check -
 *  a genuine, confirmed asymmetry vs. type_b below, which does NOT
 *  re-validate), then decodes an 11-byte record from the payload stream:
 *    bytes [0..2]  -> a 24-bit run-length count (byte 3, the tag, excluded)
 *    bytes [4..6]  -> a linear framebuffer offset (same byte-reversed-first-3
 *                     convention already reverse engineered on the AT88 side,
 *                     see crypto_at88.c's at88_relay_read_result)
 *    byte  [7]     -> a palette colour index
 *    bytes [8..10] -> a "width" value fed to clcdc_cursor_init_from_offset
 *  and plots `run count` pixels of that colour starting at that offset,
 *  advancing left-to-right and wrapping to (left_margin, y+1) at the cursor's
 *  right_edge - the exact same wrap rule clcdc.c's own clcdc_draw_edge uses,
 *  just a single fixed direction instead of clcdc_draw_edge's 4 modes.
 *
 *  Genuinely open: bytes [8..10] (the width fed to clcdc_cursor_init_from_
 *  offset) are read from the ORIGINAL, not-yet-advanced payload pointer and
 *  are never separately marked as consumed against stream_remaining or the
 *  current_object cursor (both bookkeeping fields are only ever advanced by
 *  8, covering bytes [0..7]) - i.e. the record functionally spans 11 bytes
 *  but only the first 8 are accounted for in the object's own read-cursor.
 *  Not resolved whether this is deliberate (a fixed trailing field outside
 *  the normal stream accounting) or this project's now-familiar "register/
 *  bookkeeping value that looks used but isn't actually load-bearing"
 *  pattern (see cobjectmgr_release_object below for another instance) -
 *  transcribed exactly as observed rather than guessed at.
 * ------------------------------------------------------------------------- */
void cobjectmgr_handle_type_a(struct cobjectmgr_state *mgr)	/* FUN_c0007ad0 */
{
	uint8_t *p = mgr->current_object;
	int32_t orig_remaining = mgr->stream_remaining;	/* iVar11 - captured ONCE; both writes below derive from this, not from each other */
	uint32_t offset, width;
	uint8_t color;

	if (p[3] != 0xc4)
		crypto_at88_fault(0, 0 /* DAT_c0007c14, "../cobjectmgr.cpp" */, 0xbb8 /* line 3000 */);

	mgr->stream_remaining = orig_remaining - 4;
	mgr->scratch_count = ((uint32_t)p[1] << 8) | ((uint32_t)p[0] << 16) | (uint32_t)p[2];
	mgr->current_object = p + 4;

	mgr->stream_remaining = orig_remaining - 8;
	mgr->current_object = p + 8;

	offset = ((uint32_t)p[5] << 8) | ((uint32_t)p[4] << 16) | (uint32_t)p[6];
	color  = p[7];
	width  = ((uint32_t)p[9] << 8) | ((uint32_t)p[8] << 16) | (uint32_t)p[10];	/* see "genuinely open" note above */

	clcdc_cursor_init_from_offset(cobjectmgr_draw_cursor, offset, (int)width);

	if (mgr->scratch_count > 0) {
		do {
			uint32_t idx = (uint32_t)(uint16_t)cobjectmgr_draw_cursor->x
				     + (uint32_t)(uint16_t)cobjectmgr_draw_cursor->y * 800u;
			if (idx <= clcdc_fb_pixel_count_limit)
				clcdc_framebuffer[idx] = clcdc_palette[color];

			uint16_t right_edge = (uint16_t)cobjectmgr_draw_cursor->right_edge;
			cobjectmgr_draw_cursor->x++;
			if (right_edge < (uint16_t)cobjectmgr_draw_cursor->x) {
				cobjectmgr_draw_cursor->x = cobjectmgr_draw_cursor->left_margin;
				cobjectmgr_draw_cursor->y++;
			}
			mgr->scratch_count--;
		} while (mgr->scratch_count > 0);
	}
}

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_handle_type_b - tag 0xc6 (-0x3a) handler. @0xc000769c.
 *
 *  NOT transcribed as executable C - same treatment this project already
 *  gives clcdc.c's own clcdc_draw_edge/clcdc_blit_glyph and this file's own
 *  cobjectmgr_object_destroy: the real logic is too dense (a four-way
 *  wraparound pointer walk nested inside a tight per-source-dword unpack
 *  loop, with two structurally-similar-but-not-identical code paths) to
 *  transcribe with confidence without a way to verify it against real
 *  hardware. Documented structurally instead:
 *
 *  - Confirmed to receive the SAME cobjectmgr_state pointer as
 *    cobjectmgr_tick and cobjectmgr_handle_type_a (identical +8/+0xc reads
 *    at entry) - see the register-continuity note below cobjectmgr_tick.
 *  - Genuinely new finding this pass: `DAT_c0007ac8`, dereferenced once
 *    (`iVar8 = *piVar2`), resolves to the exact same target address as
 *    clcdc.c's own `clcdc_palette` global (cross-checked: both equal
 *    -0x3ffe1088). So this handler ALSO draws through clcdc's palette, not
 *    an independent table - both cobjectmgr object types are clcdc-consuming
 *    renderers, not just type_a.
 *  - Unlike cobjectmgr_handle_type_a, this handler does NOT internally
 *    re-validate its own tag (no crypto_at88_fault call anywhere in its
 *    body) - a real, confirmed asymmetry between the two handlers, not an
 *    oversight in this reconstruction.
 *  - Structurally: consumes source data from the object's own payload
 *    (offset+0x10, one 32-bit word per group of 4 output entries) and
 *    writes palette-indexed uint16 pixels into a SEPARATE circular output
 *    buffer bounded by a write cursor (offset+0x14) and a wrap sentinel
 *    (offset+0x18), wrapping by a fixed `(0x321 - width)` distance where
 *    width lives at offset+0x20 - 0x321 (801) is one more than the panel's
 *    800px screen width, suggesting a dedicated scanline/shift buffer
 *    distinct from clcdc's own framebuffer, though no consumer of it was
 *    identified this pass. Each output entry is one of four packed
 *    sub-fields extracted per source dword (shifts 0x18, 0xf, 7, and a
 *    plain low byte; the middle two masked against DAT_c0007acc=0x1fe/510),
 *    each used as a palette index - the same "several packed sub-fields per
 *    source word, each palette-indexed" shape clcdc.c's own clcdc_blit_glyph
 *    already has, and given the same treatment there for the same reason.
 *    A length threshold (DAT_c0007ac4=0x1ff/511) selects between the two
 *    code paths; the real difference between them (beyond "one is a fixed
 *    511-iteration unrolled-by-4 loop, the other a general remaining-count
 *    loop") is not resolved.
 *
 *  Still genuinely open: the exact per-dword bit-field extraction (traced
 *  structurally, not verified bit-for-bit); what actually produces tag-0xc6
 *  objects and what consumes the output ring buffer; and whether the 801-
 *  entry buffer is itself clcdc-adjacent state or fully private to this
 *  handler.
 * ------------------------------------------------------------------------- */
void cobjectmgr_handle_type_b(struct cobjectmgr_state *mgr);	/* FUN_c000769c, structure only - not transcribed, see above */

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_tick - the confirmed core: called unconditionally once per
 *  master-dispatcher tick (FUN_c0008b64 calls this every single invocation,
 *  not gated by any status bit - see KRONOS_V06R06.VSB.md's "wire-protocol
 *  command dispatcher" section for that context). @0xc0007c2c.
 *
 *  If the "current object" slot (this+0x10) is occupied, reads a one-byte
 *  type tag from the object (offset+3) and dispatches to one of exactly two
 *  handlers (tag 0xc4/-0x3c -> cobjectmgr_handle_type_a, tag 0xc6/-0x3a ->
 *  cobjectmgr_handle_type_b) - any other tag value is a hard fault
 *  (unrecognized object type). Either way, the slot is then cleared and
 *  cobjectmgr_release_object/cobjectmgr_object_cleanup are called (see their
 *  own reconstructions below - despite the names, neither one actually does
 *  meaningful per-object cleanup work; the real "release" is just the NULL
 *  assignment already inline here).
 *
 *  CONFIRMED this pass (register-continuity evidence): although Ghidra shows
 *  both handler calls with zero visible arguments (`FUN_c0007ad0();`,
 *  `FUN_c000769c();`), both handlers' own decompiled bodies dereference
 *  param_1+8/+0xc/+0x10 with the exact same struct-offset pattern this
 *  function itself uses on ITS OWN param_1 - the standard ARM-calling-
 *  convention signature of "r0 was already correct from the caller's own
 *  argument, so the compiler emitted no separate load before the branch,
 *  and Ghidra's decompiler didn't reconstruct the implicit argument." This
 *  is the mirror image of this project's already-documented "phantom
 *  forwarded parameter" pattern (cdix4192.c, eva_board_main.c - where a
 *  callee ignores an argument the caller DOES load): here, the argument
 *  IS real and IS used, just never explicitly loaded because it didn't
 *  need to change. Both handler calls below are corrected to pass `mgr`
 *  explicitly, matching real runtime behaviour.
 *
 *  Genuinely new finding this pass: cobjectmgr's own "current object" slot
 *  (this+0x10, this+8) is populated by FUN_c0007d1c - the firmware's
 *  central wire-protocol command dispatcher, already cited by name in
 *  cad.c/cpsoc.c (opcode 0x50/0x51/0x52 etc.) but not previously connected
 *  to cobjectmgr.c. Its own opcode switch has a case for bytes 0xc4 and 0xc6
 *  (the exact two tag values this function dispatches on) that does nothing
 *  but `*(byte**)(state+0x10) = payload_ptr; *(uint*)(state+8) = payload_len;
 *  return 0;` - i.e. FUN_c0007d1c hands a queued wire command straight into
 *  this struct's own current_object/stream_remaining fields for
 *  cobjectmgr_tick to pick up on its next poll, using the SAME state struct
 *  (identical +8/+0x10 offsets). FUN_c0007d1c itself is NOT reconstructed
 *  here (it's a large, separately-owned 1904-byte dispatcher already cited
 *  by other files, and distinct from FUN_c0007220 which a different agent
 *  owns this round) - cited only for this one confirmed connection.
 * ------------------------------------------------------------------------- */
extern void cobjectmgr_release_object(void *slot);	/* FUN_c0001a80 - see own reconstruction below; NOT actually object-specific */
extern void cobjectmgr_object_cleanup(uint32_t implicit_state);	/* FUN_c0003e04 - see own reconstruction below */
extern void cobjectmgr_wait_ready(void);		/* FUN_c000395c, shared with clcdc.c */

void cobjectmgr_tick(struct cobjectmgr_state *mgr)	/* FUN_c0007c2c */
{
	if (mgr->current_object) {
		int8_t tag = *((int8_t *)mgr->current_object + 3);

		if (tag == -0x3c)
			cobjectmgr_handle_type_a(mgr);
		else if (tag == -0x3a)
			cobjectmgr_handle_type_b(mgr);
		else
			crypto_at88_fault(0, 0 /* DAT_c0007cbc */, 0 /* DAT_c0007cc0 */);

		mgr->current_object = 0;
		cobjectmgr_release_object(0 /* DAT_c0007cc4 */);
		/* real call site is `FUN_c0003e04();` with no visible argument -
		 * per the register-continuity finding above, r0 at this point is
		 * cobjectmgr_release_object's own (discarded) return value
		 * (0x1e00000, a fixed constant - see that function's own note) -
		 * NOT `mgr`. Passed through explicitly here for clarity even
		 * though the callee ignores it too (see cobjectmgr_object_cleanup
		 * below). */
		cobjectmgr_object_cleanup(0x1e00000);
	}

	/* unconditional per-tick hardware-ready poll, independent of the
	 * object-slot handling above - real bookkeeping struct/counter not
	 * further typed here (DAT_c0007cc8, a 5-int-wide state block per the
	 * real decompile's [4] index access). */
	extern int32_t *cobjectmgr_tick_state;		/* DAT_c0007cc8 */
	if ((*(uint32_t *)(*cobjectmgr_tick_state + 8) & 4) != 0) {
		cobjectmgr_wait_ready();
		cobjectmgr_tick_state[4]++;
	}
}

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_release_object (FUN_c0001a80) - CORRECTION, 2026-07-18: despite
 *  the name this project's earlier pass gave it (kept below for call-site
 *  continuity rather than renamed, per this project's own established
 *  practice of layering corrections onto existing names - see eva_board_
 *  main.c's own precedent), the real function is NOT object-specific and
 *  does NOT release anything:
 *
 *      undefined4 FUN_c0001a80(void) { return 0x1e00000; }
 *
 *  It takes NO real parameter (any register a caller loads into r0 before
 *  branching is simply never read) and unconditionally returns the fixed
 *  32-bit constant 0x01e00000, with no other observable side effect - no
 *  memory write, no branch, no hardware access. It has 8 call sites across
 *  at least 5 completely unrelated functions elsewhere in the firmware
 *  (FUN_c0000654, FUN_c0009574, FUN_c0009afc x2, FUN_c0009b68, plus 2
 *  further sites whose containing function wasn't resolved in this pass) -
 *  a shared, generic constant accessor, not cobjectmgr's own code, much
 *  like this project's earlier findings that FUN_c000acec (USB submit) and
 *  irq_save_and_disable are shared primitives rather than subsystem-local
 *  ones. 0x01e00000 falls inside the general OMAP-L1x/DA8xx SoC peripheral
 *  bus address range - plausible but NOT confirmed here that this is "get a
 *  specific hardware register block base".
 *
 *  Consequence for cobjectmgr_tick: the real call site in the dispatch loop
 *  (`FUN_c0001a80(DAT_c0007cc4);`) passes an argument the callee ignores,
 *  AND the callee's own return value is discarded by the caller (a void-
 *  context call) - so despite the "clears the slot and releases the object"
 *  description this project's earlier pass gave it, the ACTUAL slot release
 *  is entirely the `mgr->current_object = 0;` NULL assignment already
 *  inline in cobjectmgr_tick. This call has zero observable effect on the
 *  object lifecycle here; it's almost certainly this shared accessor being
 *  invoked for a reason unrelated to object management (a side effect
 *  elsewhere not visible in this decompile, or simply dead/vestigial code),
 *  not a real "release" step. Modeled here as a bodyless extern rather than
 *  redefined, since its true (non-cobjectmgr) home is unresolved.
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_object_cleanup (FUN_c0003e04) - a thin, fully-confirmed
 *  one-line wrapper: @0xc0003e04.
 *
 *      void FUN_c0003e04(undefined4 param_1) { FUN_c0003d7c(param_1,1,1); }
 *
 *  FUN_c0003d7c itself (@0xc0003d7c) is NOT cobjectmgr-owned - it has 7
 *  callers spread across FUN_c0003e24/FUN_c00048f8/FUN_c0004984, none of
 *  which are anywhere near this file's own address range, and its body
 *  (`*(uint*)(param_1 + param_2*0x10 + DAT_c0003da0) = *DAT_c0003d9c +
 *  param_3*0x20 | 2;`) reads like a fixed-stride hardware descriptor-table
 *  write (channel/index-selected register or DMA-style parameter-block
 *  setup) belonging to a different, not-yet-identified subsystem. Cited
 *  here only for the one confirmed fact this file needs: cobjectmgr_
 *  object_cleanup always calls it with slot index 1 and sub-index 1,
 *  hardcoded, every single time - not itself object-specific despite being
 *  called from the object-slot cleanup path.
 * ------------------------------------------------------------------------- */
extern void FUN_c0003d7c(uint32_t param_1, int slot_index, int sub_index);	/* not cobjectmgr-owned, see note above */

void cobjectmgr_object_cleanup(uint32_t implicit_state)	/* FUN_c0003e04 */
{
	FUN_c0003d7c(implicit_state, 1, 1);
}

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_notify_host (FUN_c0005c50) - one of a small family of host-
 *  notify event senders (siblings FUN_c0005cc4/FUN_c0005d34, all three
 *  called from the master dispatcher FUN_c0008b64 and all sharing the same
 *  output channel constant, -0x3fe35340 - not individually reconstructed
 *  this pass, cited here as the "sweep" finding that explains this file's
 *  own 9-vs-6 anchor xref discrepancy noted at the top of this file).
 *  @0xc0005c50.
 *
 *  Builds a fixed 4-byte wire event - byte0=0, byte1=0, byte2=flags,
 *  byte3=7 (the event's own tag/opcode byte, kept in the same "last field in
 *  program order, high wire byte" position already reverse engineered for
 *  the AT88 relay path in crypto_at88.c) - and submits it via at88_usb_tx_
 *  submit (FUN_c000acec, fully reconstructed in crypto_at88.c). `mode` must
 *  be 0 or 1 (anything else is a hard fault); `extra_flag` sets an extra
 *  flag bit. The first parameter is dead - never read anywhere in the real
 *  body - the same "phantom forwarded parameter" pattern already found in
 *  cdix4192.c/eva_board_main.c, here confirmed independently a third time.
 * ------------------------------------------------------------------------- */
extern void at88_usb_tx_submit(void *dest_channel, const void *buf, int len);	/* FUN_c000acec, full body in crypto_at88.c */
extern void *cobjectmgr_notify_channel;	/* DAT_c0005cc0 */

void cobjectmgr_notify_host(void *unused_param1, int mode, char extra_flag)	/* FUN_c0005c50 */
{
	uint8_t wire[4];
	uint8_t flags = 0;

	(void)unused_param1;	/* dead parameter, see note above */

	if (mode != 0) {
		if (mode == 1)
			flags = 2;
		else
			crypto_at88_fault(0, 0 /* DAT_c0005cbc, "../cobjectmgr.cpp" */, 0xc30 /* line 3120 */);
	}
	if (extra_flag != 0)
		flags |= 1;

	wire[0] = 0;
	wire[1] = 0;
	wire[2] = flags;
	wire[3] = 7;

	at88_usb_tx_submit(cobjectmgr_notify_channel, wire, 4);
}

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_hardware_fault_watchdog (FUN_c00090b8) - matches the extern
 *  declaration already in eva_board_main.c (`cobjectmgr_hardware_fault_
 *  watchdog(void *handle)`, called from eva_board_watchdog_fault_wrapper).
 *  @0xc00090b8.
 *
 *  Real body:
 *      do {
 *          FUN_c001d4c0(2, 0x4000, 1, &scratch, 0xffffffff);
 *          FUN_c001d318(2, 0x4000);
 *          FUN_c000919c(0, ..., ...);
 *      } while (true);
 *
 *  CONFIRMED this pass, not just "an infinite loop that immediately
 *  faults": FUN_c001d4c0/FUN_c001d318 are a real wait/acknowledge primitive
 *  pair, independently confirmed via the master dispatcher itself -
 *  FUN_c0008b64 (already reconstructed via its call sites throughout this
 *  project) opens with `FUN_c001d3a8(1, DAT_c000903c, 1, &local_2c);
 *  FUN_c001d318(1, DAT_c000903c);` then fans out on local_2c's bits to
 *  ~10 subsystem handlers - i.e. FUN_c001d3a8/FUN_c001d4c0 are the same
 *  event-group wait primitive (FUN_c001d3a8 the non-blocking/poll form used
 *  by the master dispatcher on group 1's ~10-bit status word, FUN_c001d4c0
 *  the blocking form - note the 0xffffffff timeout, i.e. block forever -
 *  used here on group 2's single 0x4000 bit), and FUN_c001d318 is the
 *  shared acknowledge/clear-bits companion both call afterward.
 *
 *  This means cobjectmgr_hardware_fault_watchdog is not buggy/vestigial: it
 *  blocks indefinitely for a genuine hardware-fault event bit (group 2, bit
 *  0x4000), and only when that real event actually fires does it
 *  acknowledge it and deliberately escalate to the firmware's own software
 *  fault path (crypto_at88_fault) - i.e. it's the bridge from a real
 *  hardware fault interrupt/event to the firmware's own hard-halt assert
 *  mechanism, exactly as eva_board_main.c's own fault-wrapper treats it
 *  ("should never return under normal operation").
 *
 *  `handle` is dead - never read anywhere in the real body (matches eva_
 *  board_main.c's own already-documented finding for this exact call site).
 * ------------------------------------------------------------------------- */
extern int  FUN_c001d4c0(int group, unsigned int mask, int flags, void *out_value, unsigned int timeout_ticks);	/* blocking wait-for-event-bits; real name not independently confirmed */
extern void FUN_c001d318(int group, unsigned int mask);	/* ack/clear-event-bits companion, shared with FUN_c0008b64's own group-1 poll */

void cobjectmgr_hardware_fault_watchdog(void *handle)	/* FUN_c00090b8 */
{
	uint8_t scratch[4];

	(void)handle;	/* dead parameter, see note above */

	for (;;) {
		FUN_c001d4c0(2, 0x4000, 1, scratch, 0xffffffffu);
		FUN_c001d318(2, 0x4000);
		crypto_at88_fault(0, 0 /* DAT_c0009104, "../cobjectmgr.cpp" */, 0xd06 /* line 3334 */);
	}
}

/*
 * cobjectmgr_free_list_recursive (FUN_c0015bc8) - a small recursive helper:
 * walks a singly-linked list (next-pointer at offset 0 of each node) to its
 * end via recursion, then frees each node on the way back up (post-order,
 * i.e. the LAST node in the list is freed first). @0xc0015bc8. The recursive
 * call itself shows zero visible arguments in Ghidra's decompile
 * (`FUN_c0015bc8();`) - the same register-continuity pattern documented
 * above cobjectmgr_tick, safe to resolve here since this is a textbook
 * self-recursive list walk (both parameters are simply forwarded/advanced,
 * not independently re-derived).
 */
extern void heap_free(void *unused_handle, void *ptr);	/* FUN_c0015f30, see heap_alloc.c - structure only there, safe to call here */

void cobjectmgr_free_list_recursive(void *unused_handle, void **node)	/* FUN_c0015bc8 */
{
	if (*node != 0)
		cobjectmgr_free_list_recursive(unused_handle, (void **)*node);
	heap_free(unused_handle, node);
}

/*
 * cobjectmgr_object_destroy - a real C++ virtual destructor. @0xc0015bf8.
 * Zero static callers found (confirmed via xref search) - consistent with
 * this being reached only through vtable/virtual dispatch (`delete obj`),
 * not a direct call. See eva_board_main.c for the still-unconfirmed guess
 * that eva_board_init_table's own indirection is how this and similar
 * "zero static callers" functions are actually reached.
 *
 * Early-return guard (re-verification pass, 2026-07-17): `if (obj == a
 * fixed self-reference constant, DAT_c0015cd8) return;` before any of the
 * walking below begins - a sentinel "this is the shared empty/default
 * object, never actually destroy it" check.
 *
 * Body, in order: (1) walks a 15-bucket child-widget hash table at
 * offset+0x4c, freeing every chained entry (next-pointer at each entry's
 * own offset 0) via heap_free, then frees the bucket array itself; (2)
 * walks a second, circular embedded list rooted at offset+0x148 (head
 * pointer) terminating at offset+0x14c (not independently confirmed
 * whether this is a real 2-word list_head or a simpler head/sentinel pair -
 * transcribed exactly as observed), freeing every node; (3) frees a single
 * extra pointer at offset+0x54 if present; (4) if a vtable-style pointer is
 * present at offset+0x38, calls through the function pointer stored at
 * offset+0x3c (passing `obj` itself) - a virtual method invocation, the
 * strongest evidence in the whole firmware that it's genuinely C++ with
 * real virtual dispatch; (5) if offset+0x25c is then non-zero, recurses
 * into a sibling destroy pass via cobjectmgr_free_list_recursive over the
 * list rooted there (real call site shows only one visible argument,
 * `FUN_c0015bc8(param_1)` - the second argument is inferred via the same
 * register-continuity evidence as elsewhere in this file: the just-tested
 * `*(obj+0x25c)` value is still live in a register at the call).
 *
 * heap_free's OWN internals are NOT transcribed here (see heap_alloc.c) -
 * this function's own logic IS transcribed, using heap_free as an opaque,
 * already-characterized dependency, consistent with clcdc.c's own treatment
 * of the neighboring allocator code.
 */
extern void *cobjectmgr_object_destroy_self_ref;	/* DAT_c0015cd8 */

struct cobjectmgr_widget {
	uint8_t  pad0[0x38];
	void    *vtable_thunk_holder;			/* +0x38: only ever checked non-zero, never itself called */
	void   (*vtable_slot)(void *self);		/* +0x3c: the actual function pointer called */
	uint8_t  pad1[0x4c - 0x40];
	void   **hash_buckets;				/* +0x4c: base of a 15-entry chained hash table */
	uint8_t  pad2[0x54 - 0x50];
	void    *extra_ptr;				/* +0x54: single extra freed pointer, if non-zero */
	uint8_t  pad3[0x148 - 0x58];
	void    *list_head;				/* +0x148 */
	void    *list_sentinel;			/* +0x14c: real relationship to list_head not fully confirmed, see above */
	uint8_t  pad4[0x25c - 0x150];
	void    *sibling_destroy_list;			/* +0x25c: gates a recursive cobjectmgr_free_list_recursive pass */
};

void cobjectmgr_object_destroy(struct cobjectmgr_widget *obj)	/* FUN_c0015bf8 */
{
	int i;
	void **link;

	if ((void *)obj == cobjectmgr_object_destroy_self_ref)
		return;

	if (obj->hash_buckets != 0) {
		for (i = 0; i < 0xf; i++) {
			link = (void **)obj->hash_buckets[i];
			while (link != 0) {
				void *next = *link;
				heap_free(obj, link);
				link = (void **)next;
			}
		}
		heap_free(obj, obj->hash_buckets);
	}

	if (obj->list_head != 0) {
		link = (void **)obj->list_head;
		while ((void *)link != obj->list_sentinel) {
			void *next = *link;
			heap_free(obj, link);
			link = (void **)next;
		}
	}

	if (obj->extra_ptr != 0)
		heap_free(obj, obj->extra_ptr);

	if (obj->vtable_thunk_holder != 0) {
		obj->vtable_slot(obj);
		if (obj->sibling_destroy_list != 0) {
			cobjectmgr_free_list_recursive(obj, (void **)obj->sibling_destroy_list);
			return;
		}
		return;
	}
}
