/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_param_ctrl.c - reconstructs the assigned address range
 * 0xc000d9bc-0xc000e498 (18 functions). Two related-but-distinct pieces
 * living back to back with no gap:
 *
 *   PART A (0xc000d9bc-0xc000ddc0, 7 functions) - a "link" object's own
 *   inactivity watchdog, a 64-byte realtime-byte ring buffer, and the
 *   TX/RX queue-drain pump built on top of it.
 *
 *   PART B (0xc000dd90, 1 function) - a single 1-byte accessor read by an
 *   OUT-OF-RANGE "slot dispatch" cluster (FUN_c000c094/FUN_c000b1c8,
 *   0xc000bxxx-0xc000cxxx, NOT part of this file's own assigned range and
 *   not reconstructed here) - kept in this file purely because of its
 *   address, documented honestly as belonging conceptually to Part C's own
 *   "channel descriptor" object, not to Part A's link object.
 *
 *   PART C (0xc000de0c-0xc000e498, 10 functions) - a per-"channel-index"
 *   parameter/value query-and-notify engine operating on a "desc" object,
 *   reached (per confirmed xrefs_to) from FUN_c000e924 - a USB EP0-shaped
 *   SETUP-packet reader (reads 4 consecutive 16-bit registers at +0x62/
 *   +0x64/+0x66/+0x68, the classic 8-byte SETUP-packet layout) sitting just
 *   past this file's own range and NOT reconstructed here (out of scope -
 *   the task's own assigned range ends at 0xc000e498, FUN_c000e924 starts
 *   at 0xc000e924).
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json), 2026-07-18 pass. No live Ghidra MCP calls this pass (the
 * bridge is flagged concurrency-unsafe under this project's own parallel
 * work).
 *
 * ANCHOR: NONE. All 14 real "../<name>.cpp" __FILE__ strings in this image
 * are already claimed by other subsystems (crypto_at88.c, i2c_by_gpio.c,
 * clcdc.c, cpsoc.c, ctouchpanel.c, cad.c, mcasp.c, cdix4192.c,
 * eva_board_main.c, cobjectmgr.c, omap_l108.c, omap_l108_spi.c,
 * omap_l137_usbdc.c - confirmed via a fresh `strings ".cpp"` sweep this
 * pass, matching the project's own "14 anchors, all claimed" note).
 * Attribution here rests entirely on code-shape/register/xref evidence,
 * same discipline already established by panelbus_dispatch.c/
 * wire_dispatch.c/heap_alloc.c for this exact situation. No fault-call
 * `file` argument in this range resolves to any string either (unlike
 * every anchored subsystem's usual single-assert pattern) - this cluster's
 * error paths are plain early `return`s, not crypto_at88_fault-style hard
 * halts; a genuine, confirmed structural difference from most of this
 * project, not an oversight.
 *
 * FILE NAMING: "chan_param_ctrl" reflects Part C's dominant, best-evidenced
 * role (a per-channel-index parameter query/apply/notify engine feeding a
 * USB EP0 control-request path) - Part A/B are kept in the same file only
 * because they occupy the same contiguous address range with no anchor of
 * their own to justify a second file, per this project's own precedent of
 * folding small address-adjacent unattributed clusters into one file
 * (compare cpsoc.c's own third-SPI-device section before it was
 * conclusively resolved as cpsoc.cpp code).
 *
 * CROSS-FILE FINDING (flagged, not corrected there - out of this file's
 * own scope): omap_l137_usbdc_ext.c's Section 7 declares
 * usbdc_ep_state7_handler (FUN_c000a980) as a bare, zero-arg,
 * unreconstructed extern. Decompiling FUN_c000a980 for
 * this pass's own cross-file tracing shows it is NOT zero-arg (it takes a
 * `dev` handle, forwarded via the same ARM r0-reuse idiom already
 * established elsewhere in this project) and its real body is a 4-byte
 * USB-MIDI-shaped event-packet scanner: for each 4-byte record, byte[1] in
 * range 0xf8-0xff (the USB-MIDI "Single Byte" System-Realtime range) with
 * byte[0]=0x1f (cable-select 1, CIN 0xF) calls straight into THIS file's
 * own chan_link_rt_queue_push(FUN_c000dbac, below) with that realtime
 * byte; byte[0]=0x0f (cable-select 0, CIN 0xF) instead calls FUN_c000fd98,
 * a sibling ring-push primitive for a SEPARATE cable-0 stream, not part of
 * this file's own range. This is the confirmed, sole producer for Part
 * A's ring buffer - real, address-verified, not a guess - but
 * FUN_c000a980/FUN_c000fd98 themselves are outside this file's own
 * assigned range (0xc000a980 < 0xc000d9bc) and are not reconstructed here.
 *
 * CROSS-FILE FINDING (resolves wire_dispatch.c's own still-open item):
 * wire_dispatch.c's master_dispatch_tick documents its own trailing
 * "~80-line USB-adjacent state-machine cluster... calling ... FUN_c000da0c
 * ... - NOT transcribed... genuinely open, not fabricated." FUN_c000da0c
 * is chan_link_watchdog_tick below - now fully reconstructed. Not edited
 * into wire_dispatch.c this pass (out of this file's own scope to modify
 * another file), cited here only.
 *
 * SUGGESTIVE, NOT CONFIRMED: Part A's "link" object and Part C's "desc"
 * object may be the SAME underlying struct at different offset
 * neighborhoods (Part A touches offsets 0x128-0x548+, Part C touches
 * offsets 0x00-0x180ish, and FUN_c000e924's own field writes at
 * 0x17c/0x17e/0x180 sit exactly in the gap dd98 - Part A - clears/inits).
 * Genuinely plausible for a combined "USB EP0 request + channel list"
 * session object, but NOT proven by any single direct pointer-identity
 * comparison this pass found - kept as two logically-separate `void *`
 * parameters (`link`, `desc`) throughout this file rather than asserting
 * one combined struct type.
 */

#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 * PART A - link watchdog, realtime-byte ring buffer, TX/RX queue pump.
 *
 * "link" is an opaque handle; every field below is a raw byte offset into
 * it (no formal struct - offsets are confirmed but non-contiguous / mostly
 * unlabeled, matching this project's own established convention for this
 * situation, e.g. mcasp.c's mcasp_state notes).
 *
 * Confirmed fields (byte offsets):
 *   +0x00  uint32  target handle, passed to chan_link_tx/_ack/etc.
 *   +0x08  uint8   subcode byte, paired with +0x00 at most call sites
 *   +0x10  uint32  reply length (chan_link_send_reset_frame only)
 *   +0x128 uint32  RX queue read-index-ish counter (chan_link_rx_queue_drain)
 *   +0x12c uint32  TX queue pending-count (chan_link_tx_queue_drain)
 *   +0x130 uint32  RX queue capacity/write-index (chan_link_rx_queue_drain)
 *   +0x134 uint32  tick counter AND "tripped" sentinel (value 0x7d/125 means
 *                  "watchdog tripped, don't process further" - read by
 *                  chan_link_tx_queue_drain/chan_link_rx_queue_drain,
 *                  written only by chan_link_watchdog_tick)
 *   +0x138 uint8   realtime-ring write cursor (0-0x3f, wraps)
 *   +0x13a uint8   a second small byte counter (offset is itself a literal
 *                  constant in the real code, DAT_c000dc3c=0x13a - not a
 *                  pointer), cycles 1..0x3f then resets - purpose beyond
 *                  "some kind of buffered-realtime-byte count" not decoded
 *   +0x16f..0x174  6-byte scratch region, cleared by chan_link_obj_init
 *   +0x178 uint32  sentinel, set to 0xffffffff by chan_link_obj_init
 *   +0x548 uint32  reentrancy/"busy" guard (queue-drain pumps)
 *   +0x54c uint32  "armed" flag (chan_link_watchdog_tick's own return value)
 * ===========================================================================
 */

extern void chan_link_tx(uint32_t target, uint8_t subcode, const void *buf, uint32_t len);	/* FUN_c000c1f0 - generic
					 * link transmit primitive, shared by Part A and Part C */
extern void chan_link_timeout_notify(uint32_t target, uint8_t subcode);	/* FUN_c000bf08 */
extern int  chan_link_probe_ready(uint32_t target);				/* FUN_c000b644 */
extern int  chan_link_probe_armed(uint32_t target);				/* FUN_c000cc14 */
extern int  chan_link_start_handshake(uint32_t target, uint8_t subcode);	/* FUN_c000cc60 */
extern void chan_link_set_mode(uint32_t target, uint32_t mode);		/* FUN_c000c038 */
extern void chan_link_ack(uint32_t target, uint8_t code);			/* FUN_c000c260 */
extern void chan_link_teardown(void *link, uint32_t unused);			/* FUN_c000ce58 */
extern void chan_link_uart_pump(void *link);					/* FUN_c000cfc8, out of range -
					 * classic nibble-table byte-framing shape (not independently
					 * confirmed as MIDI-standard framing) */
extern int  chan_link_tx_step(void *link, uint32_t *cursor);			/* FUN_c000d450, out of range */
extern void chan_link_rx_apply_pair(void *link, uint8_t a, uint8_t b);	/* FUN_c000d618, out of range */
extern int  chan_link_rx_pop(uint8_t *out_a, uint8_t *out_b);			/* *DAT_c000dd78 - a GLOBAL FUNCTION
					 * POINTER variable (not a fixed function), called indirectly in the
					 * real decompile; producer/configuration site not traced this pass */
extern int  irq_save_and_disable(void);	/* FUN_c0005500, see crypto_at88.c/cpsoc.c */
extern void irq_restore(int flags);		/* FUN_c0005510, see cpsoc.c */
extern uint8_t *chan_link_rt_ring_base;	/* DAT_c000dc40 - genuine global POINTER variable
					 * (dereferenced via `*piVar1` in the real decompile, unlike the
					 * Part C table-base constants below), 64-byte realtime-byte ring */

/* module-scope (NOT per-link) watchdog state, both literal-pool booleans */
extern uint32_t chan_link_trip_latched;	/* DAT_c000db38 */
extern uint32_t chan_link_handshake_armed;	/* DAT_c000db3c */

/* ------------------------------------------------------------------------- *
 *  chan_link_send_reset_frame - zeroes a 64-byte local frame and transmits
 *  it through the link's own configured target/subcode/length. Called only
 *  from chan_link_watchdog_tick, at both of its state-transition points
 *  (watchdog trip, and handshake-already-armed-but-restarting). The raw
 *  decompile zeroes the frame via a rolling stack-pointer loop (16
 *  iterations); collapsed here to a plain zero-initializer, same net
 *  effect. @0xc000d9bc (76 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_send_reset_frame(void *link)	/* FUN_c000d9bc */
{
	uint8_t *L = (uint8_t *)link;
	uint32_t frame[16] = { 0 };

	chan_link_tx(*(uint32_t *)(L + 0x00),
		     *(uint8_t  *)(L + 0x08),
		     frame,
		     *(uint32_t *)(L + 0x10));
}

/* ------------------------------------------------------------------------- *
 *  chan_link_watchdog_tick - called once per firmware tick from
 *  wire_dispatch.c's own master_dispatch_tick (FUN_c0008b64, confirmed via
 *  xrefs_to at call site 0xc0008ffc), resolving that file's own "not
 *  transcribed" USB-adjacent cluster item for FUN_c000da0c (see file
 *  header). Also confirmed self-called from chan_link_rx_queue_drain
 *  below, via the ARM r0-reuse idiom (zero visible args in that call
 *  site's own decompile).
 *
 *  Two-stage state machine, using TWO module-scope (not per-link) latches
 *  (chan_link_trip_latched, chan_link_handshake_armed):
 *   1. If already armed (+0x54c != 0): increment the tick counter
 *      (+0x134); at exactly 125 (0x7d) ticks, fire a timeout notify, send
 *      a reset frame, and latch chan_link_trip_latched - this is the same
 *      0x7d value chan_link_tx_queue_drain/chan_link_rx_queue_drain both
 *      test at +0x134 as a "link considered down, skip me" sentinel, so
 *      those two pumps naturally stop touching this link once it trips.
 *      Return early either way.
 *   2. Otherwise, probe readiness; if ready, either complete a handshake
 *      (clearing the trip latch, arming +0x54c) or, if a handshake was
 *      already mid-flight, abort it (mode/ack calls, reset frame, clear
 *      chan_link_handshake_armed).
 *   3. Shared tail: disarm (+0x54c=0); if the trip latch is set, tear the
 *      link down and swap the two latches (trip cleared, handshake-armed
 *      set) - primes state for the NEXT tick's probe-and-restart attempt.
 *
 *  @0xc000da0c (300 bytes).
 * ------------------------------------------------------------------------- */
uint32_t chan_link_watchdog_tick(void *link)	/* FUN_c000da0c */
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
			/* handshake start failed - fall through to shared tail */
		} else {
			chan_link_set_mode(target, 4);
			chan_link_ack(target, 3);
			chan_link_send_reset_frame(link);
			chan_link_handshake_armed = 0;
			/* fall through to shared tail */
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

/* ------------------------------------------------------------------------- *
 *  chan_link_rt_queue_push - pushes one realtime byte into the 64-slot
 *  ring buffer at chan_link_rt_ring_base. Sole confirmed caller (via
 *  cross-file xrefs_to tracing, see file header) is FUN_c000a980
 *  (omap_l137_usbdc_ext.c's own "usbdc_ep_state7_handler" bare extern) -
 *  the cable-1 CIN-0xF USB-MIDI-shaped single-byte event path.
 *
 *  CORRECTION-STYLE NOTE (same reduction precedent as crypto_at88.c's
 *  at88_relay_read_result): the raw decompile expresses the "should I
 *  clear two lookback bytes and reset the +0x13a counter" test via
 *  SBORROW4(next_off, 0x3f) combined with a `!=` guard. For the byte-sized
 *  values this field actually holds (0-255), that subtraction never
 *  overflows 32-bit signed arithmetic, so SBORROW4() is always false here
 *  and the whole compound condition reduces algebraically to the single
 *  `raw_cnt >= 0x3f` test below - verified by hand, not assumed.
 *
 *  Purpose of the two "lookback" bytes cleared at a rolling offset
 *  (0x01-0x3f from the link base, or the fixed pair 0x138/0x139 once the
 *  counter maxes out) is NOT decoded - transcribed faithfully, flagged
 *  open.
 *
 *  @0xc000dbac (144 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_rt_queue_push(void *link, uint8_t byte)	/* FUN_c000dbac */
{
	uint8_t *L = (uint8_t *)link;
	uint8_t raw_cnt;
	uint32_t irq_flags;

	if (*(uint32_t *)(L + 0x54c) == 0)	/* not armed */
		return;

	raw_cnt = L[0x13a];

	if (raw_cnt >= 0x3f) {
		L[0x138] = 0;
		L[0x139] = 0;
		L[0x13a] = 0;
	}
	/* raw_cnt < 0x3f: the raw decompile computes but never meaningfully
	 * uses a "next_off" lookback offset in this branch - dead in practice,
	 * see note above. */

	chan_link_rt_ring_base[L[0x138]] = byte;
	L[0x138] = (uint8_t)((L[0x138] + 1) & 0x3f);

	/* real decompile: FUN_c0005500()'s return value (still live in r0) is
	 * implicitly forwarded to FUN_c0005510() with no intervening
	 * register-clobbering call - same ARM r0-reuse idiom already
	 * established in mcasp.c/wire_dispatch.c for this project. */
	irq_flags = irq_save_and_disable();
	L[0x13a] = L[0x13a] + 1;
	irq_restore(irq_flags);

	chan_link_ack(*(uint32_t *)(L + 0x00), *(uint8_t *)(L + 0x08));
}

/* ------------------------------------------------------------------------- *
 *  chan_link_tx_queue_drain - drains the link's TX queue by repeatedly
 *  calling chan_link_tx_step until it signals "done" (nonzero). Guarded by
 *  the watchdog-trip sentinel, the armed flag, a pending-count check, and
 *  a reentrancy flag. Sole caller: chan_link_service_tick, below.
 *  @0xc000dc4c (112 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_tx_queue_drain(void *link)	/* FUN_c000dc4c */
{
	uint8_t *L = (uint8_t *)link;
	uint32_t cursor;

	if (*(uint32_t *)(L + 0x134) == 0x7d)	/* watchdog-tripped sentinel */
		return;
	if (*(uint32_t *)(L + 0x54c) == 0)	/* not armed */
		return;
	if (*(uint32_t *)(L + 0x12c) == 0)	/* nothing pending */
		return;
	if (*(uint32_t *)(L + 0x548) != 0)	/* already draining */
		return;

	*(uint32_t *)(L + 0x548) = 1;
	cursor = 0;
	while (chan_link_tx_step(link, &cursor) == 0)
		;
	*(uint32_t *)(L + 0x548) = 0;
}

/* ------------------------------------------------------------------------- *
 *  chan_link_service_tick - thin combo of the (out-of-range) UART/frame
 *  pump and chan_link_tx_queue_drain. Not called from anywhere else in
 *  this file's own range - real caller not traced this pass.
 *  @0xc000dcbc (32 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_service_tick(void *link)	/* FUN_c000dcbc */
{
	/* zero-arg call in the decompile - r0-reuse, `link` (this function's
	 * sole parameter) is still live in r0 at the call site. */
	chan_link_uart_pump(link);
	chan_link_tx_queue_drain(link);
}

/* ------------------------------------------------------------------------- *
 *  chan_link_rx_queue_drain - pops (a,b) byte pairs from a queue via an
 *  indirect callback (chan_link_rx_pop, a global function-pointer
 *  variable, not traced further this pass) and forwards each pair to
 *  chan_link_rx_apply_pair, gated by the same watchdog-trip sentinel and a
 *  reentrancy flag chan_link_tx_queue_drain also uses. The trailing
 *  0xff/read-vs-write-index check is a ring-buffer "caught up to
 *  producer" test over a DIFFERENT index pair (+0x128/+0x130) than the RT
 *  ring buffer above. Real caller not traced this pass.
 *  @0xc000dcdc (156 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_rx_queue_drain(void *link)	/* FUN_c000dcdc */
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

/* ------------------------------------------------------------------------- *
 *  chan_link_obj_init - clears a 6-byte scratch region and sets a
 *  sentinel dword to -1, as part of constructing a new link object. Both
 *  real callers (FUN_c000afe0, a linked-list-insert-shaped constructor,
 *  and FUN_c000b8a0, an 820-byte MMIO register bring-up sequence
 *  structurally similar to this project's other hardware init routines -
 *  neither in this file's own range) are out of scope here; cited only to
 *  confirm this is genuine object-construction code. Raw decompile zeroes
 *  via a rolling-pointer loop (6 iterations); collapsed to a direct range
 *  clear, same net effect. @0xc000dd98 (40 bytes).
 * ------------------------------------------------------------------------- */
void chan_link_obj_init(void *link)	/* FUN_c000dd98 */
{
	uint8_t *L = (uint8_t *)link;
	int i;

	for (i = 0x16f; i <= 0x174; i++)
		L[i] = 0;

	*(uint32_t *)(L + 0x178) = 0xffffffff;
}

/* ===========================================================================
 * PART B - shared with Part C's own "desc" object, address-adjacent to
 * Part A only by coincidence (see file header).
 * ===========================================================================
 */

/* ------------------------------------------------------------------------- *
 *  chan_desc_dispatch_enabled - trivial accessor, reads a flag byte at
 *  +0x0a of a "desc" object (see Part C below for the rest of that
 *  object's confirmed fields). ALL FOUR real callers are OUTSIDE this
 *  file's own assigned range (FUN_c000c2b8, FUN_c000c094, one unresolved
 *  caller, FUN_c000b1c8 - all in 0xc000bxxx-0xc000cxxx, a "slot dispatch"
 *  cluster not reconstructed here): each dereferences a pointer chain
 *  ending at the SAME `desc` object Part C's own functions receive
 *  directly as their own first parameter (confirmed by identical +0x09
 *  channel-index-byte usage on both sides), then gates its own
 *  vtable-style slot callback on this flag before dispatching. Also used
 *  by chan_class0_notify_flag (Part C, below) as the VALUE it reports back
 *  to the host, not merely as a gate - the same byte serves both roles at
 *  different call sites. @0xc000dd90 (8 bytes).
 * ------------------------------------------------------------------------- */
uint8_t chan_desc_dispatch_enabled(void *desc)	/* FUN_c000dd90 */
{
	return *((uint8_t *)desc + 10);
}

/* ===========================================================================
 * PART C - per-channel-index parameter query/apply/notify engine.
 *
 * "desc" is an opaque handle passed in from FUN_c000e924 (out of range,
 * see file header); confirmed byte offsets:
 *   +0x00  uint32  a secondary "selection" pointer, forwarded (never
 *                  dereferenced by any function in THIS file) to the
 *                  out-of-range chan_slot_dispatch - the same value
 *                  chan_desc_dispatch_enabled's own callers dereference
 *   +0x04  uint32  target handle for chan_link_tx and the chan_slot_...
 *                  / chan_class0_send_value calls
 *   +0x08  uint8   a small state/mode byte (chan_desc_query_dispatch only)
 *   +0x09  uint8   channel-index, 0-based, into the two fixed tables below
 *   +0x0a  uint8   see chan_desc_dispatch_enabled above
 *   +0x0c  uint32  "class 0" input value (chan_class2_poll_and_notify only
 *                  - despite the name, read regardless of +0x10's class)
 *   +0x10  uint8   low 5 bits = "class" selector (0, 1, 2 observed; other
 *                  values simply don't match any handler's own gate)
 *   +0x12  ushort  chan_desc_query_dispatch's own request word (high byte
 *                  = sub-command 1/2/3/6/7, low byte = index/length) OR
 *                  (class==0 apply path only) a signed one-byte value at
 *                  the SAME address - two different call families use
 *                  this same offset for different purposes, not resolved
 *                  further (see chan_desc_query_dispatch's own note)
 *   +0x14  ushort  "encoded id": bits[6:0] = value-index (0-127), bit 7 =
 *                  hi/lo table selector
 *   +0x16  short   chan_desc_query_dispatch: reply-length cap. Class-0/1/2
 *                  notify functions: a message "subtype" gate (1 or 2).
 *                  SAME struct offset, two different apparent meanings -
 *                  flagged, not reconciled (matches this project's own
 *                  established practice of leaving such mismatches open,
 *                  e.g. clcdc_draw_text's signature note).
 *   +0x15f..0x160  2-byte outgoing reply staging buffer
 *   +0x3c..0x44    scratch region used only by chan_desc_query_dispatch's
 *                  own "cache table locally" path
 *
 * Two fixed, channel-index-keyed global tables (both confirmed via
 * all_data.json - multiple DAT_ symbols resolving to the identical
 * address, not assumed from proximity):
 *   chan_index_table_base (DAT_c000ded8/DAT_c000df4c/DAT_c000e118, all
 *     resolve to 0xc001f6c4) - 0x44 bytes per channel-index entry.
 *   chan_bitmask_table_base (DAT_c000ded4/DAT_c000df54, both resolve to
 *     0xc001f690) - 8 bytes per channel-index entry: {lo_table, hi_table},
 *     each a pointer to a table of uint16_t bitmask words indexed by
 *     chan_class_wire_code()'s own 1-6 output.
 * ===========================================================================
 */

extern uint8_t *chan_index_table_base;		/* DAT_c000ded8/df4c/e118 -> 0xc001f6c4 */
extern uint8_t *chan_bitmask_table_base;	/* DAT_c000ded4/df54 -> 0xc001f690 */
extern uint8_t *chan_global_hi_mode_flags;	/* DAT_c000e114 -> 0xc001cd14, a small global
					 * flags array; only byte [1] (bit 0x40) is read in this file */

extern uint32_t chan_class2_default_value(uint32_t target);		/* FUN_c000cc3c, out of range */
extern uint32_t chan_class2_read_value(uint32_t target, uint8_t idx);	/* FUN_c000b68c, out of range */
extern void chan_slot_apply_code(uint32_t target, uint8_t code);	/* FUN_c000b64c, out of range */
extern void chan_slot_echo_code(uint32_t target, uint8_t code);	/* FUN_c000b66c, out of range */
extern void chan_class0_send_value(uint32_t target, uint8_t value);	/* FUN_c000ca50, out of range */
/* chan_slot_dispatch (FUN_c000b1c8, out of range) - the "slot dispatch"
 * cluster's own vtable-call primitive: looks up a per-channel-index,
 * per-value-code slot object (0x20 bytes each, up to 7 slots, base at
 * chan_index_table_base[chan]+0x28 - NOT accessed by anything in this
 * file directly) and calls through a function pointer at that slot's own
 * +0x14 or +0x1c. Only chan_class2_apply_hi_or_lo (below) calls it. */
extern void chan_slot_dispatch(uint32_t selection, int hi, uint8_t code);

/* ------------------------------------------------------------------------- *
 *  chan_class_wire_code - maps a small index (1, 2, or 3 - despite every
 *  real caller passing a full 0-127 value, only 1/2/3 have a defined
 *  mapping) plus a hi/lo flag to one of 6 wire codes.
 *
 *  PHANTOM PARAMETER NOTE: the real ABI takes 4 arguments; param_3 is
 *  never referenced anywhere in the body (fully dead) and param_4 is a
 *  fall-through default value that NONE of the 5 real call sites in this
 *  file ever supply - same pattern already found in
 *  eva_board_watchdog_fault_wrapper (eva_board_main.c) and cdix4192.c's
 *  register wrappers. Dropped from this signature rather than kept
 *  unusable; the fall-through here returns 0, which is NOT a faithful
 *  transcription of the real (garbage-register) behavior for any idx
 *  outside {1,2,3} - flagged, not fabricated.
 *
 *  @0xc000de0c (68 bytes).
 * ------------------------------------------------------------------------- */
uint32_t chan_class_wire_code(int8_t idx, int hi)	/* FUN_c000de0c */
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
	return 0;	/* NOT faithful for idx outside {1,2,3} - see note above */
}

/* ------------------------------------------------------------------------- *
 *  chan_class2_test_hi / chan_class2_test_lo - bitmask-membership tests
 *  for the "class 2" value-index space: bounds-check `idx` against the
 *  per-channel hi_bound/lo_bound byte (chan_index_table_base[chan]+0x1f
 *  or +0x1e), then test a bit in the matching bitmask table (masked with
 *  the literal 0xffc0 - a fixed constant baked into the code, not a
 *  further pointer, per all_data.json). Original return type is `uint`
 *  but every real path yields exactly 0 or 1 - simplified to bool here,
 *  consistent with this project's own convention for genuinely-binary
 *  results.
 *  @0xc000de64 (112 bytes) / @0xc000dee0 (108 bytes).
 * ------------------------------------------------------------------------- */
bool chan_class2_test_hi(void *desc, uint8_t idx)	/* FUN_c000de64 */
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

bool chan_class2_test_lo(void *desc, uint8_t idx)	/* FUN_c000dee0 */
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

/* ------------------------------------------------------------------------- *
 *  chan_desc_query_dispatch - a 7-case "get a field from this channel's
 *  descriptor" responder, keyed by the high byte of a request word at
 *  +0x12. Cases 1/2 select between two field pairs based on a GLOBAL
 *  (not per-channel) mode flag (chan_global_hi_mode_flags[1] & 0x40).
 *  Case 2 additionally, when a per-entry flag (the returned table's own
 *  byte[7] high bit) is set and desc+0xc is nonzero, copies the table's
 *  own content into this desc's local +0x3c scratch area and redirects
 *  the reply to point there instead - real meaning of that "negative"
 *  flag and of desc+0xc is NOT decoded, transcribed faithfully. Case 3
 *  indexes a 7-entry (0-6) sub-object pointer array at
 *  chan_index_table_base[chan]+0x18. Cases 6/7 read fixed fields
 *  directly. Default (any other high byte): return 0.
 *
 *  Reply length is capped against +0x16 (see this struct's own note above
 *  about that offset's dual apparent meaning across this file) and sent
 *  via chan_link_tx. Sole confirmed caller: FUN_c000e924 (out of range,
 *  call site 0xc000ea7c).
 *
 *  @0xc000df58 (416 bytes).
 * ------------------------------------------------------------------------- */
uint32_t chan_desc_query_dispatch(void *desc)	/* FUN_c000df58 */
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

/* ------------------------------------------------------------------------- *
 *  chan_class2_poll_and_notify - "class"-dispatched value poll: class 1
 *  always yields 0 (change detected unconditionally); class 0 yields 1
 *  only when desc+0xc == 0 (0 otherwise, via the same 1-minus-clamp
 *  arithmetic the raw decompile uses - transcribed literally); class 2
 *  looks up either a default value (idx==0) or a hi/lo table value,
 *  bailing out (returning false overall) if the bitmask test fails. If a
 *  real change is found (value != the 0xffff "no change" sentinel) AND
 *  the message-subtype gate at +0x16 == 2, stages a big-endian 2-byte
 *  reply at +0x15f/+0x160 and transmits it. Sole confirmed caller:
 *  FUN_c000e924 (call site 0xc000ea94).
 *  @0xc000e11c (264 bytes).
 * ------------------------------------------------------------------------- */
bool chan_class2_poll_and_notify(void *desc)	/* FUN_c000e11c */
{
	uint8_t *D = (uint8_t *)desc;
	uint32_t value = 0xffff;	/* DAT_c000e224 - "no change" sentinel */
	uint8_t class = D[0x10] & 0x1f;
	bool is_subtype2, changed;

	if (class == 1) {
		value = 0;
	} else if (class == 0) {
		uint32_t c = *(uint32_t *)(D + 0xc);

		value = 1 - c;		/* only meaningful for c==0 (->1) or c==1 (->0) */
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

/* ------------------------------------------------------------------------- *
 *  chan_class0_notify_flag - class 0, subtype 1: copies the
 *  chan_desc_dispatch_enabled byte (+0x0a) into the reply stage and sends
 *  1 byte. Sole confirmed caller: FUN_c000e924 (call site 0xc000ea70).
 *  @0xc000e22c (96 bytes).
 * ------------------------------------------------------------------------- */
bool chan_class0_notify_flag(void *desc)	/* FUN_c000e22c */
{
	uint8_t *D = (uint8_t *)desc;

	if ((D[0x10] & 0x1f) != 0 || *(int16_t *)(D + 0x16) != 1)
		return false;

	D[0x15f] = D[10];
	chan_link_tx(*(uint32_t *)(D + 4), 0, D + 0x15f, 1);
	return true;
}

/* ------------------------------------------------------------------------- *
 *  chan_class1_notify_zero - class 1, subtype 1: always stages and sends
 *  a single zero byte. Sole confirmed caller: FUN_c000e924 (call site
 *  0xc000ea88).
 *  @0xc000e290 (100 bytes).
 * ------------------------------------------------------------------------- */
bool chan_class1_notify_zero(void *desc)	/* FUN_c000e290 */
{
	uint8_t *D = (uint8_t *)desc;

	if ((D[0x10] & 0x1f) != 1 || *(int16_t *)(D + 0x16) != 1)
		return false;

	D[0x15f] = 0;
	chan_link_tx(*(uint32_t *)(D + 4), 0, D + 0x15f, 1);
	return true;
}

/* ------------------------------------------------------------------------- *
 *  chan_class2_apply_hi_or_lo - class 2 "apply an incoming value" path:
 *  bounds/bitmask-tests idx via the hi or lo table (per +0x14 bit 7), then
 *  both applies the mapped wire code to the target (chan_slot_apply_code)
 *  AND dispatches through the external per-slot vtable
 *  (chan_slot_dispatch, out of range - see its own extern comment above).
 *  Sole confirmed caller: FUN_c000c39c (out of range, call site
 *  0xc000c83c - itself outside this file's own assigned range).
 *  @0xc000e2f8 (208 bytes).
 * ------------------------------------------------------------------------- */
uint32_t chan_class2_apply_hi_or_lo(void *desc)	/* FUN_c000e2f8 */
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

/* ------------------------------------------------------------------------- *
 *  chan_class2_apply_readback - class 2 "confirm/echo" path: same
 *  idx/hi-lo test as chan_class2_apply_hi_or_lo above, but only echoes
 *  the mapped wire code back to the target (chan_slot_echo_code) - no
 *  chan_slot_dispatch call. Sole confirmed caller: FUN_c000c39c (out of
 *  range, call site 0xc000c830).
 *  @0xc000e3c8 (140 bytes).
 * ------------------------------------------------------------------------- */
uint32_t chan_class2_apply_readback(void *desc)	/* FUN_c000e3c8 */
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

/* ------------------------------------------------------------------------- *
 *  chan_class0_apply_value - class 0 "apply a direct value" path: sends
 *  the signed byte at +0x12 (same struct offset chan_desc_query_dispatch
 *  uses for its own, unrelated request word - see this file's struct
 *  note) straight to the target via chan_class0_send_value, gated on that
 *  byte being non-negative. Sole confirmed caller: FUN_c000c39c (out of
 *  range, call site 0xc000c824).
 *  @0xc000e454 (68 bytes).
 * ------------------------------------------------------------------------- */
uint32_t chan_class0_apply_value(void *desc)	/* FUN_c000e454 */
{
	uint8_t *D = (uint8_t *)desc;

	if ((D[0x10] & 0x1f) == 0 && *(int8_t *)(D + 0x12) >= 0) {
		chan_class0_send_value(*(uint32_t *)(D + 4), D[0x12]);
		return 1;
	}
	return 0;
}

/* -------------------------------------------------------------------------
 * Still genuinely open in this file:
 *  - Part A/Part C object-identity question (same underlying struct at
 *    different offset neighborhoods, or two separate objects one embeds a
 *    pointer to) - suggestive, not proven, see file header.
 *  - chan_link_rx_pop's real producer/configuration site (DAT_c000dd78,
 *    the function-pointer global chan_link_rx_queue_drain calls through).
 *  - chan_link_service_tick's, chan_link_rx_queue_drain's, and
 *    chan_desc_query_dispatch's own real callers beyond the ones
 *    confirmed above - not all traced this pass.
 *  - The +0x13a byte counter's purpose beyond "buffered-realtime-byte
 *    count, cycling 1-0x3f"; the two rolling "lookback" bytes
 *    chan_link_rt_queue_push conditionally clears.
 *  - desc+0x00's own role beyond "forwarded opaquely to
 *    chan_slot_dispatch" - never dereferenced by anything in this file.
 *  - desc+0x16's dual apparent meaning (reply-length cap in
 *    chan_desc_query_dispatch vs. message-subtype gate in the three
 *    notify functions) - flagged, not reconciled, per this project's own
 *    convention for such mismatches.
 *  - chan_index_table_base's own field_18 sub-object array contents, and
 *    every DAT_ table's actual runtime contents (all zeroed in this
 *    static image dump, per this project's usual caveat for
 *    runtime-populated data).
 * -------------------------------------------------------------------------
 */
