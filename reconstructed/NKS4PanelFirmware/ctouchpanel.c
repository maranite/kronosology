/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ctouchpanel.c - the resistive touch panel driver: raw ADC channel sampling,
 * a down/move/up debounce state machine, timeout-based release detection, and
 * a jitter-filtered event queue feeding the rest of the firmware.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../ctouchpanel.cpp" has exactly one xref (like CryptoAt88.cpp and
 * clcdc.cpp - a single assert call site), inside the event-push function
 * below. The confirmed function range (0xc0014010-0xc0014f84, 25 functions)
 * was first noticed while mapping clcdc.cpp's boundary in an earlier pass
 * and left unattributed then - this pass ties it to the real source file.
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
 *  unless a touch is already known to be in progress. Six channels for a
 *  resistive panel's X+/X-/Y+/Y- drive/sense pairs plus two more (pressure/Z
 *  sensing, or a second axis pair for a 5-wire-style panel) is a real,
 *  higher-than-4-wire channel count - not independently confirmed which
 *  physical signal each of the 6 maps to. @0xc0014010.
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
 *  ctouchpanel_check_timeout - release-by-timeout detector, called every
 *  master-dispatcher tick (confirmed caller: FUN_c0008b64's bit-0x1000
 *  handler). If the "last sample tick" field (+0x210) still matches a fixed
 *  reference, a fresh sample arrived recently and nothing happens. If it's
 *  gone stale, a counter (+0x214) increments; once it reaches 5 consecutive
 *  stale ticks, calls ctouchpanel_finalize_release (the debounce state
 *  machine's own release path) and marks the tick field as timed-out
 *  (0xffff sentinel). Classic "no fresh samples in N ticks = treat as
 *  released" debounce for a resistive panel that can't distinguish "no
 *  touch" from "touch signal briefly dropped out" any other way.
 *  @0xc001422c.
 * ------------------------------------------------------------------------- */
extern void ctouchpanel_finalize_release(void);	/* FUN_c00140d4 */

void ctouchpanel_check_timeout(struct ctouchpanel_state *tp)	/* FUN_c001422c */
{
	extern uint16_t ctouchpanel_tick_reference;		/* DAT_c0014274 */
	uint16_t *last_tick = (uint16_t *)((uint8_t *)tp + 0x210);
	int *stale_count = (int *)((uint8_t *)tp + 0x214);

	if (*last_tick == ctouchpanel_tick_reference)
		return;
	if (++(*stale_count) < 5)
		return;
	ctouchpanel_finalize_release();
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
 *  events (first byte == 1, presumably touch-down) bypass this filter
 *  entirely and are cached directly into a separate slot (+0x22c).
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

	uint8_t *ring = base;	/* real base offset for the ring itself not separately named */
	uint8_t widx = base[0x200];
	*(uint32_t *)(ring + widx * 4) = *(uint32_t *)event;
	base[ctouchpanel_overflow_flag_offset]++;
	base[0x200] = (widx + 1) & 0x7f;
	*(uint16_t *)(base + 0x210) = 0;	/* reset the timeout/hold counter on any real event */
	return 1;
}

/*
 * ctouchpanel_update - the central down/move/up debounce state machine,
 * tying ctouchpanel_sample_raw's readings to ctouchpanel_push_event's queue.
 * Tracks a "touch active" flag and a 2-tick debounce counter; on a real
 * state transition, calls a sequence of 6 per-channel setter functions
 * (mirroring ctouchpanel_sample_raw's own 6-channel layout) to feed the
 * validated sample into whatever calibration/processing pipeline those
 * setters own (not individually decompiled this pass), and on the falling
 * edge (touch released) calls ctouchpanel_push_event to enqueue the final
 * release event - gated on three separate validity flags all being set.
 * @0xc0014d80. The six setter calls' own real names/roles are not
 * transcribed here (FUN_c0014934/958/97c/9a4/bb8/ac0) - documented as a
 * confirmed sequence, not individually reconstructed.
 *
 * CORRECTION (re-verification pass, 2026-07-17): the "6 per-channel setter
 * functions, mirroring ctouchpanel_sample_raw's own 6-channel layout"
 * framing above overstates how uniform these six actually are. Re-checked
 * function sizes: FUN_c0014934/958/97c/9a4 are tiny (32 bytes each), while
 * FUN_c0014bb8/ac0 are far larger (212/224 bytes) - not six interchangeable
 * per-channel setters, but a mix of thin ones and at least two doing
 * substantially more work. Which channels the two large ones correspond to,
 * and what that extra work is, remains untraced.
 */
extern void ctouchpanel_apply_channel(struct ctouchpanel_state *tp, uint8_t value);	/* x6, distinct real functions, see above */

void ctouchpanel_update(struct ctouchpanel_state *tp, uint8_t new_sample[8])	/* FUN_c0014d80 */
{
	(void)tp; (void)new_sample;
	/* See this function's own header comment - the debounce/transition
	 * logic is confirmed structurally; the six per-channel setter calls
	 * and their exact target fields are cited but not individually
	 * traced, consistent with this file's treatment of
	 * ctouchpanel_sample_raw's own six ADC channels. */
}
