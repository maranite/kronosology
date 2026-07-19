/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ctouchpanel.c - the resistive touch panel driver: raw ADC channel sampling,
 * a down/move/up debounce state machine, timeout-based release detection, a
 * jitter-filtered event queue feeding the rest of the firmware, and the
 * ratiometric per-axis calibration math that turns 6 raw ADC channels into
 * an (x, y) coordinate pair.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17/18.
 * Anchor: "../ctouchpanel.cpp" has exactly one xref (like CryptoAt88.cpp and
 * clcdc.cpp - a single assert call site), inside the event-push function
 * below. The confirmed function range (0xc0014010-0xc0014f84) was first
 * noticed while mapping clcdc.cpp's boundary in an earlier pass and left
 * unattributed then - this pass ties it to the real source file.
 *
 * CORRECTION (closure pass, 2026-07-18): the earlier "25 functions" count
 * for this range was never re-verified against a real function sweep. A
 * fresh `range 0xc0014010 0xc0014f84` query finds exactly 23 real Ghidra
 * function objects with no address gaps that aren't accounted for by
 * literal-pool data (switch-table constants, sentinel words, a global
 * config-object pointer) - see the lookup-table section below for exactly
 * which addresses those are. 23 is now the confirmed count; the earlier
 * "25" was an overcount from an early pass, not a sign of missing bodies.
 * Every one of the 23 is reconstructed or explicitly characterized below.
 */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_sample_raw - reads 6 raw ADC channel values (offsets +0x40
 *  through +0x4a, 16-bit each, right-shifted by 2 - a 10-bit ADC reading
 *  truncated to 8 bits) into a compact 7-byte record: [0]=validity flag,
 *  [1..6]=the 6 shifted channel values. Only succeeds (returns 1) if either
 *  a "touch currently active" flag (+0xd6) is set, or none of the 6 raw
 *  values equal a fixed "no contact" sentinel (DAT_c00140d0) - i.e. it
 *  refuses to report a sample built from partially-invalid ADC readings
 *  unless a touch is already known to be in progress. @0xc0014010.
 *
 *  CHANNEL MAPPING (resolved this pass, see ctouchpanel_update's own
 *  comment for the call-order evidence): ch[0]/ch[1] are the raw X/Y
 *  position readings; ch[2]/ch[3] and ch[4]/ch[5] are NOT a second axis
 *  pair or pressure/Z sensing (the project's earlier guess) - they feed
 *  the running min/max calibration brackets that ch[0]/ch[1] are
 *  ratiometrically normalized against (see ctouchpanel_x_compute_and_push/
 *  ctouchpanel_y_compute_and_push below). This is a real, evidence-backed
 *  reading (the exact call order in ctouchpanel_update, cross-checked
 *  against which bracket fields each setter reads/writes), not a re-guess -
 *  but the underlying reason a 4-wire-style panel needs 4 reference
 *  channels rather than fixed rail voltages is still not independently
 *  confirmed against a datasheet.
 * ------------------------------------------------------------------------- */
struct ctouchpanel_state {
	uint8_t  pad0[0x40];
	uint16_t adc_ch[6];		/* +0x40..+0x4a */
	uint8_t  pad1[0x8c];
	uint8_t  touch_active;		/* +0xd6 */
};

int ctouchpanel_sample_raw(struct ctouchpanel_state *tp, uint8_t out[7])	/* FUN_c0014010 */
{
	extern uint16_t ctouchpanel_no_contact_sentinel;	/* DAT_c00140d0 */

	if (!tp->touch_active) {
		for (int i = 0; i < 6; i++)
			if (tp->adc_ch[i] == ctouchpanel_no_contact_sentinel)
				return 0;
	}
	out[0] = tp->touch_active ? 1 : 0;
	for (int i = 0; i < 6; i++)
		out[i + 1] = (uint8_t)(tp->adc_ch[i] >> 2);
	return 1;
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_watch_idle_scalar - CORRECTION (closure pass, 2026-07-18):
 *  this function was previously declared as an unexamined extern named
 *  "ctouchpanel_finalize_release" and documented as "the debounce state
 *  machine's own release path". That was never actually decompiled and is
 *  WRONG. The real release path is inline inside ctouchpanel_update itself
 *  (see below) - this function has nothing to do with touch release.
 *
 *  Real behavior, now decompiled: calls cad_calibration_smooth_sample
 *  (cad.c's shared 2-tap smoothing primitive, @0xc0013cc4) with a FIXED
 *  slot index of 0x1e (30) - the exact slot cad.c's own
 *  cad_calibration_capture_sample independently documents as excluded from
 *  CAD's own 38-channel sweep ("bails ... on the 'no valid slot' sentinel
 *  (slot==0x1e/30)"). That is real, independent cross-file confirmation:
 *  slot 30 is deliberately reserved/unused by CAD's own engine, and this
 *  function is ctouchpanel.cpp reusing the shared smoothing primitive at
 *  that reserved slot against ITS OWN scratch fields (+0x158 primary /
 *  +0x1f0 secondary, i.e. cad+0xe0+30*4 / cad+0x178+30*4 relative to
 *  whatever pointer is passed in - here, tp itself), not against CAD's own
 *  38-slot engine object. Same "borrow a shared primitive, keep the data
 *  local" pattern already established for the I2C/SPI framing code.
 *
 *  After smoothing, compares the result against a cached value at +0x3c:
 *  on the very first call (cache == the sentinel) it just seeds the cache.
 *  Otherwise it computes the absolute delta from the cached value and,
 *  IF a gate flag (+0x54 bit 0x40) is clear AND the delta exceeds a
 *  threshold byte at +0x78, updates the cache, sets a "changed" flag
 *  (+0x4f bit 0x40), refreshes the threshold from a preset byte at +0x9e,
 *  and returns 1 (changed) instead of 0. A generic "watch a smoothed
 *  scalar, report when it moves past a threshold" primitive - structurally
 *  confirmed, but WHAT scalar it's watching (some other ADC channel? a
 *  board-health value sampled during touch-panel idle ticks?) is not
 *  traced this pass. @0xc00140d4.
 * ------------------------------------------------------------------------- */
extern uint32_t cad_calib_default;					/* DAT_c0013d28, see cad.c */
extern int16_t cad_calibration_smooth_sample(void *cad, int slot, int16_t raw);	/* FUN_c0013cc4, see cad.c */

int ctouchpanel_watch_idle_scalar(struct ctouchpanel_state *tp, int16_t new_value)	/* FUN_c00140d4 */
{
	uint8_t *base = (uint8_t *)tp;
	extern uint16_t ctouchpanel_idle_scalar_sentinel;	/* DAT_c0014180, same value (0xffff) as cad_calib_default */
	uint16_t smoothed = (uint16_t)cad_calibration_smooth_sample(tp, 0x1e, new_value);
	uint16_t *cache = (uint16_t *)(base + 0x3c);

	if (*cache == ctouchpanel_idle_scalar_sentinel) {
		*cache = smoothed;
		return 0;
	}

	uint16_t delta = (smoothed < *cache) ? (uint16_t)(*cache - smoothed)
					      : (uint16_t)(smoothed - *cache);

	if ((base[0x54] & 0x40) == 0 && base[0x78] < delta) {
		*cache = smoothed;
		base[0x4f] |= 0x40;
		base[0x78] = base[0x9e];
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_check_timeout - release-by-timeout debounce, called every
 *  master-dispatcher tick (confirmed caller: master_dispatch_tick's
 *  bit-0x1000 handler, FUN_c0008b64). If the "last sample tick" field
 *  (+0x210) still matches a fixed reference, a fresh sample arrived
 *  recently and nothing happens. If it's gone stale, a counter (+0x214)
 *  increments; once it reaches 5 consecutive stale ticks, calls
 *  ctouchpanel_watch_idle_scalar (see correction above - NOT a release
 *  path) and marks the tick field as timed-out (0xffff sentinel).
 *
 *  NEEDS LIVE QUERY: 0xc0014264 - the real call site here passes NO
 *  visible second argument to ctouchpanel_watch_idle_scalar in Ghidra's
 *  decompile (`FUN_c00140d4();`), even though the callee's real signature
 *  takes a second `short` parameter. This is either a genuine decompiler
 *  display gap (the value is already sitting in a register from earlier
 *  context Ghidra didn't track through the call) or evidence the real
 *  argument is a fixed immediate baked into the call site itself - live
 *  disassembly of the r1 value at 0xc0014264 is needed to resolve what
 *  scalar is actually being fed into the shared smoother here.
 *  @0xc001422c.
 * ------------------------------------------------------------------------- */
void ctouchpanel_check_timeout(struct ctouchpanel_state *tp)	/* FUN_c001422c */
{
	extern uint16_t ctouchpanel_tick_reference;		/* DAT_c0014274 */
	uint16_t *last_tick = (uint16_t *)((uint8_t *)tp + 0x210);
	int *stale_count = (int *)((uint8_t *)tp + 0x214);

	if (*last_tick == ctouchpanel_tick_reference)
		return;
	if (++(*stale_count) < 5)
		return;
	ctouchpanel_watch_idle_scalar(tp, 0 /* NEEDS LIVE QUERY, see comment above */);
	*last_tick = 0xffff;
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_push_event - the confirmed anchor function (the one real xref
 *  to "../ctouchpanel.cpp"). Pushes a 4-byte touch event into a 128-entry
 *  ring buffer, with a genuine hard-fault guard on overflow (a status bit
 *  set elsewhere, checked here rather than a silent drop) - same shape as
 *  cobjectmgr.c's/crypto_at88.c's own ring-buffer push primitives, but a
 *  distinct instance, not the same shared pool.
 *
 *  Real jitter/hysteresis filtering confirmed: for a non-"type 1" event, if
 *  the new (x, y) coordinates are within +/-4 of the current reference
 *  position, they're SNAPPED back to the exact reference values before
 *  queuing - i.e. small resistive-panel noise near a stationary touch point
 *  is suppressed rather than generating spurious move events. "Type 1"
 *  events (first byte == 1, confirmed as the touch-DOWN transition - see
 *  ctouchpanel_x_compute_and_push/_y_compute_and_push below) bypass this
 *  filter entirely and are cached directly into a separate slot (+0x22c) -
 *  that slot's own bytes [2]/[3] ARE ref_x/ref_y (DAT_c0014ab4/ab8 resolve
 *  to +0x22e/+0x22f, i.e. literally the x/y fields of the cached type-1
 *  event record, not a separately-maintained reference pair).
 *
 *  Note on the ring storage itself, confirmed by direct re-read of this
 *  function's disassembly this pass: the write is
 *  `*(uint32_t *)(tp + widx*4) = event` - i.e. the 128*4-byte ring is
 *  stored starting at struct offset 0, the SAME region ctouchpanel_state's
 *  own adc_ch/touch_active fields (offsets 0x40-0xd6) live in. This is a
 *  real, confirmed overlap, not a transcription slip - left as an honest
 *  open question (most likely explanation: adc_ch is refreshed from
 *  hardware far more often than events are pushed, so the rare ring slots
 *  that alias it get overwritten again before ctouchpanel_pop_event or
 *  sample_raw would notice, but this is not independently confirmed).
 *  @0xc00149cc.
 * ------------------------------------------------------------------------- */
int ctouchpanel_push_event(struct ctouchpanel_state *tp, uint8_t event[4])	/* FUN_c00149cc */
{
	extern int ctouchpanel_overflow_flag_offset;	/* DAT_c0014ab0 */
	extern int ctouchpanel_ref_x_offset;		/* DAT_c0014ab4 */
	extern int ctouchpanel_ref_y_offset;		/* DAT_c0014ab8 */
	uint8_t *base = (uint8_t *)tp;

	if (base[ctouchpanel_overflow_flag_offset] & 0x80) {
		crypto_at88_fault(0, 0 /* DAT_c0014abc */, 0xf2);
		return 0;
	}

	if (event[0] == 1) {
		*(uint32_t *)(base + 0x22c) = *(uint32_t *)event;
	} else {
		uint8_t ref_x = base[ctouchpanel_ref_x_offset];
		uint8_t ref_y = base[ctouchpanel_ref_y_offset];

		if ((int8_t)(ref_x - 4) < event[2] && event[2] < ref_x + 4 &&
		    (int8_t)(ref_y - 4) < event[3] && event[3] < ref_y + 4) {
			event[2] = ref_x;
			event[3] = ref_y;
		}
	}

	uint8_t *ring = base;	/* confirmed struct-offset-0 overlap, see comment above */
	uint8_t widx = base[0x200];
	*(uint32_t *)(ring + widx * 4) = *(uint32_t *)event;
	base[ctouchpanel_overflow_flag_offset]++;
	base[0x200] = (widx + 1) & 0x7f;
	*(uint16_t *)(base + 0x210) = 0;	/* reset the timeout/hold counter on any real event */
	return 1;
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_pop_event - NEW this pass: the real dequeue counterpart to
 *  ctouchpanel_push_event, previously undocumented (not even cited by the
 *  earlier "core done" pass). Confirmed via its one real caller,
 *  FUN_c0005b14 (outside this file's range - the firmware's touch-event
 *  consumer loop, drains this queue every master-dispatcher tick and
 *  relays each event to the host through the SAME shared USB-submit
 *  primitive already established for crypto_at88.c's AtmelRead event,
 *  cobjectmgr.c's host-notify event, and cad.c's calibration-progress
 *  pump - the same global handle (DAT_c0005c48/DAT_c0008498 resolve to the
 *  identical address, -0x3fe337ec) confirms cpsoc.c's central dispatcher
 *  and this consumer both operate on the exact same touch-panel handle).
 *
 *  Reads a read-cursor (+0x201, distinct from push's own write-cursor at
 *  +0x200) and a pending count (+0x202 - the SAME byte push_event
 *  increments and checks `&0x80` on overflow). If count != 0: copies the
 *  4-byte event at ring offset (read-cursor*4) into *out, decrements the
 *  count, and advances/wraps the read cursor mod 0x80. Returns whether an
 *  event was popped. @0xc0014d24.
 * ------------------------------------------------------------------------- */
int ctouchpanel_pop_event(struct ctouchpanel_state *tp, uint32_t *out)	/* FUN_c0014d24 */
{
	uint8_t *base = (uint8_t *)tp;
	extern int ctouchpanel_ring_count_offset;	/* DAT_c0014d78, = 0x202, same field push_event's overflow-flag offset resolves to */
	extern int ctouchpanel_ring_ridx_offset;	/* DAT_c0014d7c, = 0x201 */

	int have_event = base[ctouchpanel_ring_count_offset] != 0;
	if (have_event) {
		uint8_t ridx = base[ctouchpanel_ring_ridx_offset];
		*out = *(uint32_t *)(base + (uint32_t)ridx * 4);
		base[ctouchpanel_ring_count_offset]--;
		base[ctouchpanel_ring_ridx_offset] = (ridx + 1) & 0x7f;
	}
	return have_event;
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_bound_reset_a / _b - shared "widen the calibration bracket
 *  back to defaults" helpers, called by the small per-channel setters below
 *  whenever a new reading is inconsistent with the current [lo, hi] order.
 *  _a resets the X-axis bracket (+0x20b hi, +0x20c lo) from baseline source
 *  fields +0x218 (complemented -> hi) / +0x214 (direct -> lo); _b resets
 *  the Y-axis bracket (+0x20d lo, +0x20e hi) from +0x220 (complemented) /
 *  +0x21c (direct). Both baseline pairs are zeroed by ctouchpanel_init and
 *  (for the X-axis pair only) +0x214 is the SAME field
 *  ctouchpanel_check_timeout uses as its own stale-tick counter - a real,
 *  confirmed offset reuse. Since +0x214 only grows while no fresh sample is
 *  arriving (i.e. while no touch is active), and this reset path only
 *  matters while a touch calibration IS active, the two roles appear
 *  temporally disjoint in practice, but this is observed, not proven safe.
 *  +0x218/+0x21c/+0x220 have no other writers found in this address range,
 *  so in practice _a's hi-reset is always ~0=0xff and _b's lo-reset is
 *  always 0 - i.e. these resets widen the bracket to "wide open, no
 *  calibration yet" rather than to any meaningful saved baseline.
 *  @0xc0014818 (_a), 0xc0014838 (_b).
 * ------------------------------------------------------------------------- */
void ctouchpanel_bound_reset_a(struct ctouchpanel_state *tp)	/* FUN_c0014818 */
{
	uint8_t *base = (uint8_t *)tp;
	base[0x20c] = (uint8_t)*(uint32_t *)(base + 0x214);
	base[0x20b] = (uint8_t)~(*(uint32_t *)(base + 0x218));
}

void ctouchpanel_bound_reset_b(struct ctouchpanel_state *tp)	/* FUN_c0014838 */
{
	uint8_t *base = (uint8_t *)tp;
	extern int ctouchpanel_y_hi_offset;	/* DAT_c0014858, = 0x20e */
	int hi_off = ctouchpanel_y_hi_offset;

	base[hi_off] = (uint8_t)*(uint32_t *)(base + 0x21c);
	base[hi_off - 1] = (uint8_t)~(*(uint32_t *)(base + 0x220));	/* = +0x20d */
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_x_bound_hi_update / _x_bound_lo_update / _y_bound_hi_update /
 *  _y_bound_lo_update - the four SMALL per-channel setters ctouchpanel_update
 *  calls (previously "cited, not individually decompiled"). Each maintains
 *  one side of a running [lo, hi] calibration bracket: if the new sample
 *  extends the bracket the right way, the corresponding field is updated;
 *  if it's inconsistent with the bracket's own opposite side, the whole
 *  pair is widened back to defaults via ctouchpanel_bound_reset_a/_b.
 *
 *  Fields: X-axis bracket is +0x20b (hi, fed by adc_ch[2]) / +0x20c (lo,
 *  fed by adc_ch[3]); Y-axis bracket is +0x20d (hi, fed by adc_ch[4]) /
 *  +0x20e (lo, fed by adc_ch[5]) - call order and argument-to-channel
 *  mapping confirmed directly from ctouchpanel_update's disassembly below.
 *  @0xc0014934 (x hi), 0xc0014958 (x lo), 0xc001497c (y hi), 0xc00149a4
 *  (y lo).
 * ------------------------------------------------------------------------- */
void ctouchpanel_x_bound_hi_update(struct ctouchpanel_state *tp, uint8_t value)	/* FUN_c0014934 */
{
	uint8_t *base = (uint8_t *)tp;
	if (value <= base[0x20c]) {
		ctouchpanel_bound_reset_a(tp);
		return;
	}
	base[0x20b] = value;
}

void ctouchpanel_x_bound_lo_update(struct ctouchpanel_state *tp, uint8_t value)	/* FUN_c0014958 */
{
	uint8_t *base = (uint8_t *)tp;
	if (base[0x20b] <= value) {
		ctouchpanel_bound_reset_a(tp);
		return;
	}
	base[0x20c] = value;
}

void ctouchpanel_y_bound_hi_update(struct ctouchpanel_state *tp, uint8_t value)	/* FUN_c001497c */
{
	uint8_t *base = (uint8_t *)tp;
	if (value <= base[0x20e]) {
		ctouchpanel_bound_reset_b(tp);
		return;
	}
	base[0x20d] = value;
}

void ctouchpanel_y_bound_lo_update(struct ctouchpanel_state *tp, uint8_t value)	/* FUN_c00149a4 */
{
	uint8_t *base = (uint8_t *)tp;
	if (base[0x20d] <= value) {
		ctouchpanel_bound_reset_b(tp);
		return;
	}
	base[0x20e] = value;
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_x_compute_and_push / _y_compute_and_push - the two LARGE
 *  per-channel setters (212/224 bytes - the re-verification pass's own
 *  correction that these two are NOT interchangeable with the four small
 *  setters above is confirmed and now fully explained). Each:
 *   1. Rejects the raw sample if it's below the axis's own lo-bound.
 *   2. Ratiometrically normalizes it to 0-255 via omap_tick_scale (the
 *      SAME generic scaled-divide primitive already documented in
 *      omap_l108.c for tick-to-unit and clcdc_progress_bar's percent math -
 *      reused here as (sample-lo)*255/(hi-lo), not tick-related at all in
 *      this call site): omap_tick_scale((sample-lo)*0xff, hi-lo).
 *   3. _y additionally applies a conditional axis-flip: if a byte at
 *      +0x2c inside a GLOBAL config object (DAT_c0014ba4, a real resolved
 *      pointer - not a struct-local field) is set, inverts the result
 *      (0xff-x). _x instead inverts UNCONDITIONALLY, no flag check - a
 *      real, confirmed asymmetry between the two axes, not an
 *      inconsistency in this reconstruction.
 *   4. Dedups against a per-axis cached coordinate byte; if unchanged and
 *      already valid, returns without touching the event state machine.
 *   5. On a real change: marks the axis "valid" (+0x208 for X, +0x209 for
 *      Y), stores the new coordinate into the shared event record's X/Y
 *      byte (+0x206 / +0x207), and - gated on the shared "armed" flag
 *      (+0x20a, the SAME field ctouchpanel_update itself sets) AND the
 *      other axis already having a valid value too - transitions the
 *      shared state byte (+0x204) from idle(2) to down(1), or - if
 *      already down/moving and a per-axis move-gate flag allows it - to
 *      move(3), then calls ctouchpanel_push_event with the event record.
 *      This is the real down/move state-transition logic: state 1 (down)
 *      bypasses push_event's jitter filter and seeds the reference
 *      position; state 3 (move) goes through it - exactly matching
 *      push_event's own confirmed event[0]==1 special case.
 *  @0xc0014bb8 (_x), 0xc0014ac0 (_y).
 * ------------------------------------------------------------------------- */
extern int32_t omap_tick_scale(int32_t value, int32_t range);	/* FUN_c001e3f8, see omap_l108.c - generic scaled-divide, reused here for a ratiometric percent, not a tick conversion */

void ctouchpanel_x_compute_and_push(struct ctouchpanel_state *tp, uint32_t raw)	/* FUN_c0014bb8 */
{
	uint8_t *base = (uint8_t *)tp;
	extern int ctouchpanel_x_move_gate_offset;	/* DAT_c0014c94, = 0x20f */
	uint32_t lo = base[0x20c];

	if ((raw & 0xff) < lo)
		return;

	uint32_t coord = 0xff - (uint32_t)omap_tick_scale((int32_t)(((raw & 0xff) - lo) * 0xff),
							    (int32_t)(base[0x20b] - lo));

	if (base[0x208] != 0 && base[0x206] == coord)
		return;

	base[0x208] = 1;
	base[0x206] = (uint8_t)coord;

	if (base[0x20a] == 0)
		return;
	if (base[0x209] == 0)	/* the other axis (Y) must also have a valid value */
		return;

	if (base[0x204] == 2) {
		base[0x204] = 1;
	} else {
		if (base[ctouchpanel_x_move_gate_offset] != 0)
			return;
		base[0x204] = 3;
	}
	ctouchpanel_push_event(tp, base + 0x204);
}

void ctouchpanel_y_compute_and_push(struct ctouchpanel_state *tp, uint32_t raw)	/* FUN_c0014ac0 */
{
	uint8_t *base = (uint8_t *)tp;
	extern uint8_t *ctouchpanel_axis_config;	/* DAT_c0014ba4, real resolved global pointer, +0x2c = axis-flip flag */
	extern int ctouchpanel_y_move_gate_offset;	/* DAT_c0014bb4, = 0x20f (same flag ctouchpanel_x_compute_and_push's own gate uses) */
	uint32_t lo = base[0x20e];

	if ((raw & 0xff) < lo)
		return;

	uint32_t coord = (uint32_t)omap_tick_scale((int32_t)(((raw & 0xff) - lo) * 0xff),
						    (int32_t)(base[0x20d] - lo));

	if (ctouchpanel_axis_config[0x2c] != 0)
		coord = 0xff - coord;

	if (base[0x209] != 0 && base[0x207] == coord)
		return;

	base[0x209] = 1;
	base[0x207] = (uint8_t)coord;

	if (base[0x20a] == 0)
		return;
	if (base[0x208] == 0)	/* the other axis (X) must also have a valid value */
		return;

	if (base[0x204] == 2) {
		base[0x204] = 1;
	} else {
		if (base[ctouchpanel_y_move_gate_offset] != 0)
			return;
		base[0x204] = 3;
	}
	ctouchpanel_push_event(tp, base + 0x204);
}

/*
 * ctouchpanel_update - the central down/move/up debounce state machine,
 * tying ctouchpanel_sample_raw's readings to ctouchpanel_push_event's
 * queue. FULLY RECONSTRUCTED this pass (previously a stub - the earlier
 * "core done" claim covered only the structural description below, not an
 * actual transcription).
 *
 * new_sample is the 8-byte record ctouchpanel_sample_raw's 7-byte output
 * gets copied/padded into: [0]=validity, [1]=ch0(X raw), [2]=ch1(Y raw),
 * [3]=ch2, [4]=ch3, [5]=ch4, [6]=ch5, [7]=pad.
 *
 * Real logic:
 *  1. A GLOBAL (not per-instance) debounce counter gates initial arming:
 *     while not yet armed (+0x20a == 0), each consecutive valid sample
 *     increments the global counter; only the 3rd consecutive valid
 *     sample proceeds to step 2 (arms the touch). This debounces the
 *     initial touch-down against a single noisy ADC glitch.
 *  2. Once armed (or on the arming sample itself) - if the CURRENT sample
 *     is still valid: marks +0x20a armed, then calls the six per-channel
 *     setters, IN THIS EXACT ORDER (the real evidence for the channel
 *     mapping documented in ctouchpanel_sample_raw's own header):
 *       ctouchpanel_x_bound_hi_update(ch2)   - X bracket hi
 *       ctouchpanel_x_bound_lo_update(ch3)   - X bracket lo
 *       ctouchpanel_y_bound_hi_update(ch4)   - Y bracket hi
 *       ctouchpanel_y_bound_lo_update(ch5)   - Y bracket lo
 *       ctouchpanel_x_compute_and_push(ch0)  - X coordinate + event
 *       ctouchpanel_y_compute_and_push(ch1)  - Y coordinate + event
 *     i.e. both calibration brackets are refreshed BEFORE the position
 *     channels are normalized against them, every single tick.
 *  3. THE REAL RELEASE PATH (not ctouchpanel_watch_idle_scalar, see that
 *     function's own correction note): if the current sample is INVALID
 *     (sample_raw returned "no contact"), whether or not the touch was
 *     already armed, falls through to a shared tail: forces the shared
 *     state byte to idle(2), and - only if the state wasn't already idle
 *     AND both axes had a valid coordinate (+0x208 and +0x209 both set) -
 *     pushes one final release event via ctouchpanel_push_event. Then
 *     clears the armed flag, both per-axis valid flags, and the global
 *     debounce counter.
 *  4. Either way, caches the raw 8-byte sample into +0x224..+0x22b for the
 *     next call's debounce-arm comparison.
 * @0xc0014d80.
 */
void ctouchpanel_update(struct ctouchpanel_state *tp, uint8_t new_sample[8])	/* FUN_c0014d80 */
{
	uint8_t *base = (uint8_t *)tp;
	extern int ctouchpanel_debounce_counter;	/* DAT_c0014ecc, a GLOBAL (not per-instance) int, real address */
	int armed = base[0x20a] != 0;
	int proceed = armed;

	if (!armed && new_sample[0] != 0) {
		base[0x224] = new_sample[0]; base[0x225] = new_sample[1];
		base[0x226] = new_sample[2]; base[0x227] = new_sample[3];
		base[0x228] = new_sample[4]; base[0x229] = new_sample[5];
		base[0x22a] = new_sample[6]; base[0x22b] = new_sample[7];
		if (ctouchpanel_debounce_counter < 2) {
			ctouchpanel_debounce_counter++;
			return;
		}
		proceed = 1;
	}

	if (proceed && new_sample[0] != 0) {
		base[0x20a] = 1;
		ctouchpanel_x_bound_hi_update(tp, base[0x227]);	/* ch2 */
		ctouchpanel_x_bound_lo_update(tp, base[0x228]);	/* ch3 */
		ctouchpanel_y_bound_hi_update(tp, base[0x229]);	/* ch4 */
		ctouchpanel_y_bound_lo_update(tp, base[0x22a]);	/* ch5 */
		ctouchpanel_x_compute_and_push(tp, base[0x225]);	/* ch0 */
		ctouchpanel_y_compute_and_push(tp, base[0x226]);	/* ch1 */
		goto cache_sample;
	}

	/* release tail: reached when invalid sample, whether or not armed */
	{
		int have_xy = base[0x208] != 0 && base[0x209] != 0;
		int was_idle = base[0x204] == 2;
		base[0x204] = 2;
		if (!was_idle && have_xy)
			ctouchpanel_push_event(tp, base + 0x204);
		base[0x20a] = 0;
		base[0x209] = 0;
		base[0x208] = 0;
		ctouchpanel_debounce_counter = 0;
	}

cache_sample:
	base[0x224] = new_sample[0]; base[0x225] = new_sample[1];
	base[0x226] = new_sample[2]; base[0x227] = new_sample[3];
	base[0x228] = new_sample[4]; base[0x229] = new_sample[5];
	base[0x22a] = new_sample[6]; base[0x22b] = new_sample[7];
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_init - the state-struct initializer. Zeroes the event-state
 *  byte block (+0x200..+0x20a except +0x204, which is set to 2/idle - not
 *  0), calls both bound-reset helpers, zeroes the tick-threshold/last-tick
 *  pair (+0x210/+0x212) and the four calibration-bracket baseline source
 *  fields (+0x214/+0x218/+0x21c/+0x220).
 *
 *  CONFIRMED CALLER this pass: eva_board_final_setup (FUN_c00074bc,
 *  eva_board_main.c), at 0xc0007520 - immediately followed at 0xc0007528
 *  by a call to ctouchpanel_hw_init (see below). This resolves
 *  eva_board_main.c's own still-open question of what
 *  eva_board_final_setup actually does, at least for this one subsystem;
 *  not this file's place to edit that file, noted here for the record.
 *  @0xc001485c.
 * ------------------------------------------------------------------------- */
void ctouchpanel_init(struct ctouchpanel_state *tp)	/* FUN_c001485c */
{
	uint8_t *base = (uint8_t *)tp;

	base[0x204] = 2;
	base[0x201] = 0;
	base[0x200] = 0;
	base[0x202] = 0;
	base[0x206] = 0;
	base[0x207] = 0;
	base[0x20a] = 0;
	base[0x209] = 0;
	base[0x208] = 0;
	ctouchpanel_bound_reset_a(tp);
	ctouchpanel_bound_reset_b(tp);
	*(uint32_t *)(base + 0x220) = 0;
	*(uint16_t *)(base + 0x212) = 0;
	*(uint16_t *)(base + 0x210) = *(uint16_t *)(base + 0x212);
	base[0x20f] = 0;
	*(uint32_t *)(base + 0x214) = 0;
	*(uint32_t *)(base + 0x218) = 0;
	*(uint32_t *)(base + 0x21c) = 0;
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_hw_init - a small (0x14-byte) hardware descriptor
 *  initializer, DISTINCT from ctouchpanel_state (different, much smaller
 *  field layout: [0]=type tag, [4]/[8]=zeroed state, [0xc]=register base,
 *  [0x10]=GPIO bank base). Called immediately after ctouchpanel_init from
 *  the SAME caller (eva_board_final_setup, back-to-back call sites
 *  0xc0007520/0xc0007528) - plausible (not certain, given the differing
 *  layout) that this is the touch panel's own ADC/mux hardware handle,
 *  separate from the debounce/event state object above.
 *
 *  Acquires a register base via a generic 4-entry "register base by index"
 *  lookup used elsewhere in the image (index 1) and a GPIO bank base via
 *  gpio_bank_get_base (i2c_by_gpio.c's own confirmed name for
 *  FUN_c0001990 - a real zero-argument function with a fixed return value;
 *  the apparent argument Ghidra shows here is almost certainly a stale
 *  register value at the call site, not a real parameter, per
 *  i2c_by_gpio.c's own signature). @0xc0014ee8.
 * ------------------------------------------------------------------------- */
extern void *gpio_bank_get_base(void);					/* FUN_c0001990, see i2c_by_gpio.c */
extern uint32_t ctouchpanel_reg_base_lookup(void *unused, int index);	/* FUN_c00018fc, ignores its first argument entirely */

void ctouchpanel_hw_init(uint8_t descriptor[0x14])	/* FUN_c0014ee8 */
{
	*(uint32_t *)(descriptor + 4) = 0;
	*(uint32_t *)(descriptor + 8) = 0;
	descriptor[0] = 1;
	*(uint32_t *)(descriptor + 0xc) = ctouchpanel_reg_base_lookup(0, 1);
	*(uint32_t *)(descriptor + 0x10) = (uint32_t)gpio_bank_get_base();
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_set_mode_flag / ctouchpanel_set_tick_threshold - the
 *  host-writable half of ctouchpanel_state's wire surface, both confirmed
 *  called from cpsoc.c's own central command dispatcher (FUN_c0007d1c) on
 *  the SAME global touch-panel handle (DAT_c0008498, resolves to the exact
 *  same address, -0x3fe337ec, as ctouchpanel_pop_event's own consumer-side
 *  handle DAT_c0005c48 - independent confirmation both paths share one
 *  object). _set_mode_flag writes +0x20f directly (the same flag
 *  ctouchpanel_x_compute_and_push/_y_compute_and_push both gate their move
 *  transition on). _set_tick_threshold writes +0x212 (a 2-byte value) and,
 *  as a real, confirmed side effect, ALSO zeroes +0x210 - i.e. writing
 *  this field forces ctouchpanel_check_timeout's own "last sample tick"
 *  comparison to go stale on the very next tick, effectively re-arming the
 *  timeout debounce as part of setting a new threshold. @0xc00148fc (mode
 *  flag), 0xc001490c (tick threshold).
 * ------------------------------------------------------------------------- */
void ctouchpanel_set_mode_flag(struct ctouchpanel_state *tp, uint8_t value)	/* FUN_c00148fc */
{
	extern int ctouchpanel_mode_flag_offset;	/* DAT_c0014908, = 0x20f */
	((uint8_t *)tp)[ctouchpanel_mode_flag_offset] = value;
}

void ctouchpanel_set_tick_threshold(struct ctouchpanel_state *tp, uint16_t value)	/* FUN_c001490c */
{
	uint8_t *base = (uint8_t *)tp;
	extern int ctouchpanel_tick_threshold_offset;	/* DAT_c0014930, = 0x212 */
	int off = ctouchpanel_tick_threshold_offset;

	*(uint16_t *)(base + off) = 0;
	*(uint16_t *)(base + 0x210) = 0;
	*(uint16_t *)(base + off) = value;
}

/* ------------------------------------------------------------------------- *
 *  ctouchpanel_cad_channels_enable / _disable - thin wrappers (6 calls each
 *  to cad.c's own shared bit-primitives, cad_calibration_unmask_slot /
 *  cad_calibration_mask_slot) toggling CAD engine slots 0x20-0x25 (32-37) -
 *  the LAST 6 slots of CAD's 38-slot engine, outside the 12-channel
 *  {knob,slider,pedal} grouping cad_channel_group covers.
 *
 *  REAL CROSS-FILE FINDING (this pass): cad.c's own
 *  cad_calibration_select_slot documents its step-5 sweep override as
 *  picking "one of 6 fixed values (0x20-0x25)" without knowing what they
 *  are ("not confirmed which physical calibration point each corresponds
 *  to"). These are the same 6 slot numbers this file enables/disables as a
 *  group - real, address-confirmed evidence that CAD's own 38-channel
 *  analog engine is what actually captures the touch panel's 6 raw ADC
 *  channels (this file's own ctouchpanel_sample_raw just reads the
 *  results back out of tp->adc_ch), rather than the touch panel driving
 *  its own independent ADC hardware. NOT this file's place to edit cad.c
 *  with this finding - noted here for whichever pass reconciles the two.
 *  The exact mechanism connecting CAD's captured sample at slot N to
 *  ctouchpanel_state's adc_ch[N-0x20] is not traced this pass.
 *
 *  Called from FUN_c0005738 (outside this file's range, a broader
 *  multi-subsystem mode-toggle function, itself called from cpsoc.c's
 *  central dispatcher) gated on bit 6 of an incoming mode byte - bit
 *  clear enables (calls _enable, cad_calibration_unmask_slot), bit set
 *  disables (calls _disable, cad_calibration_mask_slot). @0xc0014288
 *  (enable), 0xc00142e0 (disable).
 * ------------------------------------------------------------------------- */
extern void cad_calibration_unmask_slot(void *cad, int slot);	/* FUN_c00138b8, see cad.c */
extern void cad_calibration_mask_slot(void *cad, int slot);	/* FUN_c00138e4, see cad.c */

void ctouchpanel_cad_channels_enable(void *cad)	/* FUN_c0014288 */
{
	for (int slot = 0x20; slot <= 0x25; slot++)
		cad_calibration_unmask_slot(cad, slot);
}

void ctouchpanel_cad_channels_disable(void *cad)	/* FUN_c00142e0 */
{
	for (int slot = 0x20; slot <= 0x25; slot++)
		cad_calibration_mask_slot(cad, slot);
}

/* ------------------------------------------------------------------------- *
 *  Lookup/remap tables - three switch-shaped functions physically in this
 *  address range, each resolved via `dat`/`range` queries this pass, but
 *  of genuinely uncertain ownership - none of the three touch any
 *  ctouchpanel_state field, and each is called externally by a different
 *  subsystem's own code. Kept here (not renamed with a ctouchpanel_
 *  prefix) because they sit inside the confirmed 0xc0014010-0xc0014f84
 *  anchor range, same "found unattributed, contents resolved, ownership
 *  still open" treatment cpsoc.c/omap_l108_spi.c gave their own
 *  cross-boundary code.
 *
 *  FUN_c0014488 (item 3 of this pass's assignment) - a pure index-remap
 *  switch, case range 5-0x1d (29), returning small integers 0-0x1e (30).
 *  Two real callers, BOTH inside cpsoc.c's own central command dispatcher
 *  (FUN_c0007d1c, NOT cobjectmgr.c's FUN_c0007220 as an earlier pass's
 *  README note guessed - re-checked directly against fresh decompiles of
 *  both candidate callers this pass), invoked as
 *  `FUN_c0014488(param_2[1])` on an incoming wire-protocol byte - i.e.
 *  this is a wire-opcode-to-internal-index remap table, most likely
 *  shared/generic rather than touch-panel-specific.
 *
 *  A THIRD call site exists at 0xc0014780, attributed to "no containing
 *  function" in Ghidra's analysis (from_func: None) - the SAME "Ghidra
 *  failed to bound this region as a function" pattern eva_board_main.c's
 *  own reconstruction already ran into at 0xc0005610-0xc0005698. That
 *  unbound code sits directly in the c0014700-c0014760 literal-pool gap
 *  this pass identified while reconciling the "23 vs 25 functions" count
 *  (see this file's header) - i.e. there IS live, uncounted glue code in
 *  this address range, just not one Ghidra boxed as its own function.
 *  NEEDS LIVE QUERY: 0xc0014760-0xc0014818 - raw disassembly of this
 *  unbound stretch, to determine what it does with FUN_c0014488's and
 *  FUN_c00145c4's return values.
 *
 *  FUN_c00145c4 - a structurally IDENTICAL switch (same case range 5-0x1d)
 *  but returning DAT_ values instead of immediates, called from the same
 *  unbound glue code (0xc0014794) plus one call from cad.c's own
 *  cad_calibration_progress_pump (FUN_c0005a1c). The resolved DAT_ values
 *  for its case bodies (data dump queried this pass) decode as plausible
 *  .text-range addresses (~0xc0023000-0xc0024000) rather than small
 *  integers, which would make this a companion "index -> handler pointer"
 *  table alongside FUN_c0014488's "opcode -> index" - but no Ghidra
 *  function boundary exists at any of those addresses, so this is NOT
 *  confirmed. NEEDS LIVE QUERY: whether 0xc00239d8 (one resolved case
 *  value) and its siblings are real code, real data, or a decompiler
 *  misread - not resolvable from the static dump alone.
 *
 *  FUN_c0014338 - a third, separately-cased switch (same shape, 24 cases)
 *  in this same address range, called only from cad.c's own
 *  cad_calibration_progress_pump. Contents dumped below for the record;
 *  real meaning unresolved.
 *  @0xc0014488, 0xc00145c4, 0xc0014338.
 * ------------------------------------------------------------------------- */
int FUN_c0014338(int index)
{
	switch (index) {
	case 0:  return 8;
	case 1:  return 0xc;
	case 2:  return 0x10;
	case 3:  return 0x14;
	case 4:  return 5;
	case 6:  return 0x1d;
	case 8:  return 9;
	case 9:  return 0xd;
	case 10: return 0x11;
	case 0xb: return 0x15;
	case 0xc: return 6;
	case 0xe: return 0x1b;
	case 0x10: return 10;
	case 0x11: return 0xe;
	case 0x12: return 0x12;
	case 0x13: return 0x16;
	case 0x14: return 0x19;
	case 0x16: return 0x1c;
	case 0x18: return 0xb;
	case 0x19: return 0xf;
	case 0x1a: return 0x13;
	case 0x1b: return 0x17;
	case 0x1c: return 0x18;
	case 0x1e: return 0x1a;
	default: return 0;
	}
}

int FUN_c0014488(int wire_opcode)	/* wire-protocol opcode -> internal index remap, see comment above */
{
	switch (wire_opcode) {
	case 5:  return 4;
	case 6:  return 0xc;
	case 8:  return 0;
	case 9:  return 8;
	case 10: return 0x10;
	case 0xb: return 0x18;
	case 0xc: return 1;
	case 0xd: return 9;
	case 0xe: return 0x11;
	case 0xf: return 0x19;
	case 0x10: return 2;
	case 0x11: return 0xa;
	case 0x12: return 0x12;
	case 0x13: return 0x1a;
	case 0x14: return 3;
	case 0x15: return 0xb;
	case 0x16: return 0x13;
	case 0x17: return 0x1b;
	case 0x18: return 0x1c;
	case 0x19: return 0x14;
	case 0x1a: return 0x1e;
	case 0x1b: return 0xe;
	case 0x1c: return 0x16;
	case 0x1d: return 6;
	default: return -1;
	}
}

/* FUN_c00145c4 - see comment above; case bodies return DAT_ values whose
 * real meaning is unresolved, cited here as opaque externs rather than
 * guessed integers (DAT_c0014700..DAT_c0014760). */
int FUN_c00145c4(int index)
{
	extern int32_t ctp_tbl2_case_default, ctp_tbl2_case_5, ctp_tbl2_case_6,
		ctp_tbl2_case_8, ctp_tbl2_case_9, ctp_tbl2_case_a, ctp_tbl2_case_b,
		ctp_tbl2_case_c, ctp_tbl2_case_d, ctp_tbl2_case_e, ctp_tbl2_case_f,
		ctp_tbl2_case_10, ctp_tbl2_case_11, ctp_tbl2_case_12, ctp_tbl2_case_13,
		ctp_tbl2_case_14, ctp_tbl2_case_15, ctp_tbl2_case_16, ctp_tbl2_case_17,
		ctp_tbl2_case_18, ctp_tbl2_case_19, ctp_tbl2_case_1a, ctp_tbl2_case_1b,
		ctp_tbl2_case_1c, ctp_tbl2_case_1d;

	switch (index) {
	case 5:  return ctp_tbl2_case_5;
	case 6:  return ctp_tbl2_case_6;
	case 8:  return ctp_tbl2_case_8;
	case 9:  return ctp_tbl2_case_9;
	case 10: return ctp_tbl2_case_a;
	case 0xb: return ctp_tbl2_case_b;
	case 0xc: return ctp_tbl2_case_c;
	case 0xd: return ctp_tbl2_case_d;
	case 0xe: return ctp_tbl2_case_e;
	case 0xf: return ctp_tbl2_case_f;
	case 0x10: return ctp_tbl2_case_10;
	case 0x11: return ctp_tbl2_case_11;
	case 0x12: return ctp_tbl2_case_12;
	case 0x13: return ctp_tbl2_case_13;
	case 0x14: return ctp_tbl2_case_14;
	case 0x15: return ctp_tbl2_case_15;
	case 0x16: return ctp_tbl2_case_16;
	case 0x17: return ctp_tbl2_case_17;
	case 0x18: return ctp_tbl2_case_18;
	case 0x19: return ctp_tbl2_case_19;
	case 0x1a: return ctp_tbl2_case_1a;
	case 0x1b: return ctp_tbl2_case_1b;
	case 0x1c: return ctp_tbl2_case_1c;
	case 0x1d: return ctp_tbl2_case_1d;
	default: return ctp_tbl2_case_default;
	}
}

/* ------------------------------------------------------------------------- *
 *  FUN_c0014f4c - found while sweeping this address range, structurally a
 *  small generic value-cache setter (guarded by a "valid" flag at [0],
 *  writes a 4-byte value at [8], and on first use only, seeds a small
 *  flag/counter block at [4..7]). NOT confirmed to be touch-panel-specific:
 *  its two real callers (FUN_c0005f5c, FUN_c0007d1c) are both generic/
 *  shared code elsewhere in the image, and it doesn't touch any
 *  ctouchpanel_state field. Left un-prefixed and un-renamed rather than
 *  forced into this file's naming scheme on a guess - same treatment
 *  cobjectmgr.c gave its own three anchor-adjacent-but-not-owned
 *  functions. @0xc0014f4c.
 * ------------------------------------------------------------------------- */
void FUN_c0014f4c(uint8_t *obj, uint32_t value)
{
	if (obj[0] == 0)
		return;
	*(uint32_t *)(obj + 8) = value;
	if (*(uint32_t *)(obj + 4) == 0) {
		obj[4] = 1;
		obj[5] = 0;
		obj[6] = 0;
		obj[7] = 0;
	}
}

/* ------------------------------------------------------------------------- *
 *  Still genuinely open (closure pass, 2026-07-18):
 *   - The exact physical meaning of ctouchpanel_watch_idle_scalar's own
 *     scalar (what it samples, and what consumes its "changed" flag at
 *     +0x4f bit 0x40 - no reader found in this address range).
 *   - The NEEDS LIVE QUERY items called out above: the real second
 *     argument at ctouchpanel_check_timeout's call site (0xc0014264), the
 *     unbound glue code at 0xc0014760-0xc0014818 that ties FUN_c0014488
 *     and FUN_c00145c4 together, and whether FUN_c00145c4's case values
 *     are genuinely code pointers.
 *   - How CAD's own captured samples for slots 0x20-0x25 actually land in
 *     ctouchpanel_state's adc_ch[6] array (same object vs. a copy) - the
 *     cross-file finding above (ctouchpanel_cad_channels_enable/_disable)
 *     establishes THAT the channels are shared, not exactly how.
 *   - The push_event/pop_event ring-buffer's confirmed overlap with
 *     adc_ch/touch_active (struct offset 0-0x1FC) - real, not a
 *     transcription error, but not independently explained either.
 *   - FUN_c0014f4c's real ownership/subsystem.
 *   - Event byte [1] (+0x205 in the shared event record) - never observed
 *     written by any function reconstructed in this file; role unknown.
 * ------------------------------------------------------------------------- */
