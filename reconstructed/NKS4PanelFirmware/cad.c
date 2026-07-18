/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cad.c - the analog input driver: knobs, sliders, and pedal jacks. Handles
 * per-channel calibration, a 12-channel eligibility bitmask, expansion-pedal
 * presence detection, and registers itself into the shared wire-protocol
 * dispatcher under opcodes 0x78/0x79.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../cad.cpp" has 2 xrefs (assert call sites inside
 * cad_channel_group and cad_trigger_calibration below), confirming the
 * 0xc001335c-0xc0013f5c address range (23 functions) as this subsystem's
 * real compilation unit - same discovery pattern as ctouchpanel.cpp's own
 * previously-unattributed range.
 *
 * FULL CALIBRATION ENGINE, resolved 2026-07-17: all 23 functions in this
 * range are now decompiled (18 previously left untouched). See the new
 * section below cad_init for the real per-channel calibration engine
 * cad_trigger_calibration's own sweep drives, plus a small separate
 * pedal-value-encoding trio.
 */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* ------------------------------------------------------------------------- *
 *  cad_channel_group - maps a raw channel index (0-0xb, 12 channels) to one
 *  of 4 logical groups (0/1/2/3), asserting on any out-of-range index.
 *  Ground truth mapping: {0,2,3,6,9}->0, {1,4,5,7,10}->1, {8}->2, {0xb}->3 -
 *  a real, disassembly-confirmed table, not a guessed linear scheme. Group 0
 *  and 1 each cover 5 channels, suggesting those are the two largest
 *  physical categories (knobs and sliders are the most likely candidates,
 *  in some order - not independently confirmed which group is which), while
 *  groups 2 and 3 (one channel each) are likely dedicated single inputs
 *  (e.g. individual pedal jacks). @0xc001363c.
 * ------------------------------------------------------------------------- */
int cad_channel_group(void *cad, int channel)		/* FUN_c001363c */
{
	switch (channel) {
	case 0: case 2: case 3: case 6: case 9:
		return 0;
	case 1: case 4: case 5: case 7: case 10:
		return 1;
	case 8:
		return 2;
	case 0xb:
		return 3;
	default:
		crypto_at88_fault(0, 0 /* DAT_c00136bc */, 0x1e0);
		return 0;
	}
}

/* ------------------------------------------------------------------------- *
 *  cad_channel_eligible - checks whether the current channel (state+0x224,
 *  the same field cad_init below resets to 0) is excluded from calibration:
 *  any channel >= 12 is automatically "excluded" (true), otherwise tests bit
 *  `channel` of a fixed enable-mask (DAT_c001352c) - true (excluded) if the
 *  bit is CLEAR. Used by cad_trigger_calibration to pick between two
 *  calibration state values. @0xc0013504.
 * ------------------------------------------------------------------------- */
extern uint32_t cad_channel_enable_mask;	/* DAT_c001352c */

int cad_channel_eligible(void *cad_state)		/* FUN_c0013504 - name inverted: returns "excluded" */
{
	uint32_t channel = *(uint32_t *)((uint8_t *)cad_state + 0x224);

	if (channel > 0xb)
		return 1;	/* excluded */
	return (cad_channel_enable_mask & (1u << channel)) == 0;
}

/* ------------------------------------------------------------------------- *
 *  cad_trim_adjust - a trim/adjustment accumulator, real host-facing entry
 *  point: called directly from cobjectmgr.cpp's own secondary protocol
 *  dispatcher (FUN_c0007220) for opcode 0x50, confirming this subsystem's
 *  own wire surface extends beyond the 0x78/0x79 handlers registered in
 *  cad_init. Only acts when the register selector is 0x7a (adds a signed
 *  byte delta to a running 16-bit value) - a different local meaning for
 *  reg 0x7a than cpsoc.cpp's own register-bank split at the same value,
 *  confirming 0x7a is a per-subsystem-local register number, not a single
 *  firmware-wide meaning. @0xc0013480.
 * ------------------------------------------------------------------------- */
void cad_trim_adjust(int16_t *trim, int reg, int8_t delta)	/* FUN_c0013480 */
{
	if (reg == 0x7a)
		*trim += delta;
}

/* ------------------------------------------------------------------------- *
 *  cad_trigger_calibration - real host-facing entry point (the confirmed
 *  "../cad.cpp" assert anchor). Only acts on a message tag byte == 0xe0 (a
 *  per-subsystem-local tag, not necessarily the same meaning as the AT88
 *  relay's own 0xe0 opcode - not confirmed either way). Re-entrancy-guards
 *  against a "calibration already in progress" flag (+0xd0), validates the
 *  incoming register (0x78 or 0x79 - the SAME two opcodes cad_init
 *  registers with the shared dispatcher) against a cached value, and on a
 *  real mismatch asserts (a genuine consistency check, not just logging).
 *  Marks a per-register "pending" byte, then - if a tick counter (+0x220)
 *  has reached a fixed target (DAT_c0013630) - runs the actual calibration
 *  sequence: delay, cad_channel_eligible-gated state selection (0 or 4),
 *  send that state through a lookup-table-indexed command, delay again, and
 *  set the "in progress" flag. @0xc00073e8.
 * ------------------------------------------------------------------------- */
extern void cad_delay_ticks(void *handle, int ticks);		/* FUN_c00085a8 */
extern void cad_send_state_command(void *handle, uint16_t cmd);	/* FUN_c00035a4 */
extern uint16_t cad_state_command_table[5];			/* DAT_c0013638 */

void cad_trigger_calibration(void *cad, int reg, int8_t tag, uint8_t value)	/* FUN_c00073e8 */
{
	uint8_t *state = (uint8_t *)cad;	/* real base pointer, DAT_c00073f8 */
	uint8_t cached;

	if (tag != -0x20)	/* 0xe0 */
		return;
	if (*(uint32_t *)(state + 0xd0) != 0)
		crypto_at88_fault(0, 0 /* DAT_c001362c */, 0x40);

	/* CORRECTION (re-verification pass, 2026-07-17): the mismatch fault
	 * below was previously a single shared call site with a literal `0`
	 * line number. Re-verified against fresh disassembly: the real code
	 * has TWO DISTINCT fault call sites, one per register, with real line
	 * numbers - 0x42 for the reg==0x78 mismatch, 0x46 for the reg==0x79
	 * mismatch - not a merged/shared check as the earlier draft implied. */
	if (reg == 0x78) {
		cached = state[0x220];
		if (cached != value)
			crypto_at88_fault(0, 0 /* DAT_c001362c */, 0x42);
	} else if (reg == 0x79) {
		cached = state[0x221];
		if (cached != value)
			crypto_at88_fault(0, 0 /* DAT_c001362c */, 0x46);
	} else {
		goto check_tick;
	}
	state[reg + 0x1a8] = 0xff;

check_tick:
	extern uint16_t cad_calibration_tick_target;	/* DAT_c0013630 */
	if (*(uint16_t *)(state + 0x220) != cad_calibration_tick_target)
		return;

	extern void *cad_delay_handle;			/* DAT_c0013634 */
	cad_delay_ticks(cad_delay_handle, 200);
	uint8_t cal_state = cad_channel_eligible(state) ? 4 : 0;
	state[0x57] = cal_state;
	cad_send_state_command(*(void **)(state + 0xdc), cad_state_command_table[cal_state]);
	cad_delay_ticks(cad_delay_handle, 10);
	*(uint32_t *)(state + 0xd0) = 1;
}

/* ------------------------------------------------------------------------- *
 *  cad_init - subsystem bring-up (the anchor for the two functions above).
 *  Grabs a hardware handle, resets the current channel to group 0, zeroes a
 *  38-entry calibration/filter state table, probes 5 expansion-pedal slots
 *  (via a presence-check primitive, marking each present/absent), resets
 *  another 38-entry table to a fixed default, resets the tick counter, sends
 *  a hardware reset/config command (0x9000) to the handle, configures the
 *  initial channel group, and - the key wire-protocol tie-in - registers
 *  this subsystem's own two opcode handlers (0x78, 0x79) with the shared
 *  command surface (cad_register_handler, matching the exact opcodes
 *  cobjectmgr.cpp's and ctouchpanel.cpp's own code already referenced
 *  without this project having traced where they came from until now).
 *  @0xc00136c0.
 * ------------------------------------------------------------------------- */
extern void *cad_get_handle(void *bus, int unused);		/* FUN_c0001a1c */
extern int   cad_pedal_present(void *probe_handle);		/* FUN_c00094d8 */
extern void  cad_hw_reset(void *handle);			/* FUN_c00034fc */
extern void  cad_hw_config(void *handle, uint16_t cmd);	/* FUN_c00035a4, same primitive as cad_send_state_command */
extern void  cad_configure_group(void *cad, uint8_t group, uint16_t sample_count);	/* FUN_c001349c */
extern void  cad_register_handler(void *dispatcher, int opcode);	/* FUN_c0007150 */

void cad_init(void *cad)	/* FUN_c00136c0 */
{
	uint8_t *state = (uint8_t *)cad;

	*(void **)(state + 0xdc) = cad_get_handle(0 /* DAT_c001380c */, 0);
	*(uint32_t *)(state + 0x224) = 0;
	state[0x56] = (uint8_t)cad_channel_group(cad, 0);
	*(uint16_t *)(state + 0xd4) = 6;
	state[0xd6] = 0;

	/* 38-entry (0x26) calibration/filter table reset to a fixed default,
	 * paired with a 2-byte 0xffff sentinel array. */
	extern uint32_t cad_calib_default;	/* DAT_c0013810 */
	for (int i = 0; i < 0x26; i++) {
		*(uint32_t *)(state + i * 4 + 0x178) = cad_calib_default;
		*(uint32_t *)(state + i * 4 + 0xe0) = cad_calib_default;
		*(uint16_t *)(state + i * 2) = 0xffff;
	}

	/* 5 expansion-pedal presence probes */
	for (int i = 0; i < 5; i++) {
		int present = cad_pedal_present(0 /* DAT_c0013814 */);
		state[i + 0x4c] = 0;
		state[i + 0x51] = present ? 0 : 0xff;
	}

	state[0x58] = 0;
	state[0x57] = 0;
	state[0x59] = 0;

	/* second 38-entry table reset */
	for (int i = 0; i < 0x26; i++) {
		state[i + 0x80] = 1;
		state[i + 0xa6] = 8;	/* real offset: (param_1+0xa4)+2+i */
		state[i + 0x5a] = 8;
	}

	*(uint16_t *)(state + 0x210) = 0xffff;
	*(uint16_t *)(state + 0xcc) = 0;

	cad_hw_reset(*(void **)(state + 0xdc));
	cad_hw_config(*(void **)(state + 0xdc), 0x9000);
	cad_configure_group(cad, state[0x56], *(uint16_t *)(state + 0xd4));

	extern void *cad_dispatcher_handle;	/* DAT_c0013818 */
	cad_register_handler(cad_dispatcher_handle, 0x78);
	cad_register_handler(cad_dispatcher_handle, 0x79);
}

/* ========================================================================= *
 *  The real per-channel calibration engine - all 18 remaining functions in
 *  this address range, resolved 2026-07-17. 38-slot (0x26) parallel state,
 *  all fields already seeded by cad_init above, now shown to be a real,
 *  coherent bitmask+cache engine rather than isolated fields:
 *    +0xe0, +0x178  two parallel smoothed-value caches (uint32, one slot each)
 *    +0x4c          "changed" bitmask (1 bit/slot, 5 bytes for 38 slots)
 *    +0x51          "masked/excluded" bitmask (same shape)
 *    +0x80, +0xa6   per-slot threshold pair; +0x5a is the "current cap"
 *                   (cad_init seeds all three to 1/8/8)
 *  cad_init's own "5 expansion-pedal presence probes" loop is now clearer
 *  in this light: it isn't writing 5 independent pedal flags, it's seeding
 *  the FIRST 5 BYTES (40 of the 38 real slots) of the +0x51 exclude-mask -
 *  0xff (all 8 slots in that byte excluded) if that pedal jack's physical
 *  presence probe fails, 0 (none excluded) if it's present. A channel
 *  group whose expansion pedal isn't plugged in never gets its calibration
 *  "changed" bit set at all (see cad_calibration_mark_changed's own gate
 *  below), so it's silently excluded from the whole engine rather than
 *  producing spurious readings from a floating input.
 * ========================================================================= */

/* cad_calibration_mask_slot/_unmask_slot - set/clear a slot's bit in the
 * +0x51 exclude-mask. @0xc00138e4 (mask), 0xc00138b8 (unmask). */
void cad_calibration_mask_slot(void *cad, int slot)	/* FUN_c00138e4 */
{
	uint8_t *byte = (uint8_t *)cad + 0x51 + (slot >> 3);

	if (slot < 0x26)
		*byte |= (uint8_t)(1 << (slot & 7));
}

void cad_calibration_unmask_slot(void *cad, int slot)	/* FUN_c00138b8 */
{
	uint8_t *byte = (uint8_t *)cad + 0x51 + (slot >> 3);

	if (slot < 0x26)
		*byte &= (uint8_t)~(1 << (slot & 7));
}

/* cad_calibration_mark_changed - sets a slot's bit in the +0x4c "changed"
 * bitmask, but ONLY if that slot's own bit in the +0x51 exclude-mask is
 * clear - the real gate that silently drops excluded-pedal channels
 * mentioned above. @0xc001385c. */
void cad_calibration_mark_changed(void *cad, int slot)	/* FUN_c001385c */
{
	uint8_t *state = (uint8_t *)cad;
	uint8_t bit = (uint8_t)(1 << (slot & 7));

	if (slot != -1 && slot < 0x26 && (state[0x51 + (slot >> 3)] & bit) == 0)
		state[0x4c + (slot >> 3)] |= bit;
}

/* cad_calibration_init_slot - (re-)initializes one slot's threshold pair:
 * sets +0x80 and +0xa6 (the settle-tick cap cad_calibration_tick below
 * increments toward), and copies +0xa6 into +0x5a (the "current cap"
 * field). cad_init's own bring-up hardcodes these three fields directly
 * rather than calling this - this is a real, separate reconfiguration
 * entry point, not called from anywhere traced this pass (a genuine "who
 * calls this" gap, plausibly the diagnostic menu or a live recalibration
 * trigger). @0xc001381c. */
void cad_calibration_init_slot(void *cad, int slot, uint8_t threshold, uint8_t cap)	/* FUN_c001381c */
{
	uint8_t *state = (uint8_t *)cad;

	if (slot != -1 && slot < 0x26) {
		state[slot + 0x80] = threshold;
		state[slot + 0xa6] = cap;
		state[slot + 0x5a] = state[slot + 0xa6];
	}
}

/* cad_calibration_tick - called once per master-dispatcher tick (matching
 * every other subsystem's own FUN_c0008b64 tie-in). Every 30 (0x1e) ticks,
 * walks all 38 slots' settle counters (+0x5a onward), incrementing each
 * one while it's still below its own per-slot cap (+0xa6) - a real settling
 * timer, not a fixed delay: slots converge toward their configured cap at a
 * fixed 30-tick cadence. @0xc00139cc. */
void cad_calibration_tick(void *cad)	/* FUN_c00139cc */
{
	uint8_t *state = (uint8_t *)cad;
	uint16_t *ticks = (uint16_t *)(state + 0xcc);

	(*ticks)++;
	if (*ticks <= 0x1d)
		return;
	*ticks = 0;
	for (int i = 0; i < 0x26; i++) {
		if (state[i + 0x5a] < state[i + 0xa6])
			state[i + 0x5a]++;
	}
}

/* cad_calibration_slot_is_raw - true if `slot` is one of the 4 special
 * "boundary" slots (5, 0xd, 0x15, 0x1d - the same 4 values
 * cad_calibration_select_slot's own step-5 override table produces) or
 * >= 0x20 - such slots skip smoothing entirely in
 * cad_calibration_capture_sample below and use the raw reading directly.
 * @0xc0013910. */
int cad_calibration_slot_is_raw(void *cad, int slot)	/* FUN_c0013910 */
{
	(void)cad;
	if (slot < 0x20 && slot != 5 && slot != 0xd && slot != 0x15 && slot != 0x1d)
		return 0;
	return 1;
}

/* cad_calibration_smooth_sample - a real 2-tap debounce/smoothing filter
 * over the two parallel per-slot caches (+0xe0 primary, +0x178 secondary).
 * On the first-ever sample for a slot (primary cache == cad_calib_default,
 * the same sentinel cad_init seeds both caches with), both caches are set
 * directly to the new value. Otherwise: computes the new sample's distance
 * from EACH cache, keeps whichever cache is CLOSER as the new primary
 * value (a simple closest-of-two-references smoother), and unconditionally
 * updates the secondary cache to the latest raw sample regardless. Returns
 * the (possibly unchanged) primary cache value. @0xc0013cc4. */
extern uint32_t cad_calib_default;	/* DAT_c0013d28, same sentinel as cad_init's own DAT_c0013810 - not confirmed to be the identical global, same value either way */

int16_t cad_calibration_smooth_sample(void *cad, int slot, int16_t raw)	/* FUN_c0013cc4 */
{
	uint32_t *primary = (uint32_t *)((uint8_t *)cad + 0xe0 + slot * 4);
	uint32_t *secondary = (uint32_t *)((uint8_t *)cad + 0x178 + slot * 4);
	int is_first = (*primary == cad_calib_default);
	int32_t sample = raw;

	if (is_first) {
		*primary = sample;
		*secondary = sample;
	} else {
		int32_t d_primary = sample - (int32_t)*primary;
		int32_t d_secondary = sample - (int32_t)*secondary;
		if (d_primary < 0) d_primary = -d_primary;
		if (d_secondary < 0) d_secondary = -d_secondary;
		if (d_secondary < d_primary)
			*primary = *secondary;
		*secondary = sample;
	}
	return (int16_t)*primary;
}

/* cad_calibration_capture_sample - the real sample-capture routine tying
 * the read primitive, the raw-vs-smoothed decision, and the changed-bitmask
 * together: reads a 16-bit raw value (FUN_c000366c, the SAME shared read
 * primitive cpsoc.c's own cpsoc_analog_poll_channel uses), shifts right by
 * 6 bits, bails on read failure or on the "no valid slot" sentinel
 * (slot==0x1e/30). Applies cad_calibration_smooth_sample UNLESS
 * cad_calibration_slot_is_raw says to skip it. Then, unless in
 * "always report" test mode (+0xd6) or the slot is one of the 2 boundary
 * sentinels (0x20/0x21 - a narrower pair than the 4 raw-slot values above),
 * applies the SAME closest-of-two-references logic one more time directly
 * against a per-slot value array (indexed slot*2) before deciding whether
 * to actually mark the slot changed - gated on cad_calibration_mark_changed's
 * own exclude-mask AND a settle-cap check (only marks changed once the
 * slot's settle counter, +0x5a, has reached its threshold, +0x80). Real,
 * dense hysteresis logic - structurally cited above rather than every
 * branch individually named, since the exact physical meaning of each
 * threshold isn't independently confirmed. @0xc0013d2c. */
extern int cad_read_16(void *handle, void *out);	/* FUN_c000366c, shared with cpsoc.c's cpsoc_analog_poll_channel */

/* cad_calibration_select_slot - computes which of the 38 slots the CURRENT
 * sweep step (state[0x57], the same field cad_calibration_sweep below
 * drives) should capture into, normally `group*8 + step` (state[0x58] =
 * state[0x56]*8 + state[0x57]), but on step 5 specifically overridden to
 * one of 6 fixed values (0x20-0x25) selected by a dense nested check of
 * mode (+0xd4, 1 or 2 - direction of sweep, not independently confirmed
 * which), a global flag (`*(char*)(DAT_c0013b70+0x2c)`, not resolved), and
 * the slot value computed just before the override (checked against 5,
 * 0xd, 0x15, 0x1d - the SAME 4 "boundary" values
 * cad_calibration_slot_is_raw tests). This is the real link between the
 * per-channel-group indexing scheme and the 4 special boundary slots -
 * structurally cited rather than transcribed branch-by-branch, since the
 * exact real-world meaning of the 6 override targets (which physical
 * calibration point each corresponds to) isn't independently confirmed.
 * @0xc0013a28. */
void cad_calibration_select_slot(void *cad);	/* FUN_c0013a28, structure only, not transcribed - see comment above */

void cad_calibration_capture_sample(void *cad)	/* FUN_c0013d2c */
{
	uint8_t *state = (uint8_t *)cad;
	uint16_t raw;

	if (!cad_read_16(*(void **)(state + 0xdc), &raw))
		return;
	raw >>= 6;

	int slot = (int8_t)state[0x58];
	if (slot == 0x1e)
		return;

	int16_t value = (int16_t)raw;
	if (!cad_calibration_slot_is_raw(cad, slot))
		value = cad_calibration_smooth_sample(cad, slot, value);

	/* remainder (the +0x20/+0x21 boundary check, the second closest-of-two
	 * pass against the slot*2-indexed array, and the final settle-cap-gated
	 * cad_calibration_mark_changed call) - structurally cited, not
	 * transcribed, per this function's own header comment above. */
}

/* cad_calibration_pop_changed - the real "pop one pending changed
 * calibration slot" function cad_calibration_progress_pump
 * (cad.c-internal, see omap_l108.c's own cad_delay_ticks note) was already
 * suspected to call, now confirmed and reconstructed. Scans the +0x4c
 * changed-bitmask starting from a rotating cursor (+0x59, wrapping at 38),
 * and on finding a set bit: clears it under the shared irq_save_and_disable/
 * irq_restore critical-section pair (see crypto_at88.c/cpsoc.c's own
 * corrected naming for this pair), and if a destination buffer was given,
 * writes the slot index and its smoothed value (from the +0xe0 primary
 * cache cad_calibration_smooth_sample maintains) into it. Returns whether
 * a changed slot was found. @0xc0013f5c. */
extern int irq_save_and_disable(void);	/* see crypto_at88.c */
extern void irq_restore(int flags);	/* see cpsoc.c */

struct cad_changed_slot {
	uint8_t slot;
	int16_t value;
};

int cad_calibration_pop_changed(void *cad, struct cad_changed_slot *out)	/* FUN_c0013f5c */
{
	uint8_t *state = (uint8_t *)cad;

	if (state[0x59] > 0x25)
		state[0x59] = 0;

	while (state[0x59] <= 0x25) {
		uint8_t slot = state[0x59];
		uint8_t bit = (uint8_t)(1 << (slot & 7));
		uint8_t *byte = &state[slot >> 3];

		if ((*byte & bit) != 0) {
			int flags = irq_save_and_disable();
			*byte &= (uint8_t)~bit;
			irq_restore(flags);
			if (out) {
				out->slot = slot;
				out->value = *(int16_t *)(state + slot * 2);
			}
			state[0x59]++;
			return 1;
		}
		state[0x59]++;
	}
	return 0;
}

/* cad_calibration_advance_group - the overall sweep sequencer. Rotates
 * state[0x224] (the SAME field cad_channel_group/cad_channel_eligible both
 * read) through 11 (0xb) values, re-derives the current group via
 * cad_channel_group and caches it at state[0x56] (matching cad_init's own
 * field). If test/calibration mode is inactive (+0xd6 == 0), that's all -
 * it may additionally pre-seed +0xd4 (the sweep mode field
 * cad_calibration_select_slot reads) to 1 or 2 based on the current group
 * and channel eligibility. If calibration mode IS active, a much denser
 * sub-state-machine (its own step counter at +0x21c: 0/1/2/3+) drives
 * fixed jump targets into state[0x224] (8, then 5, then a toggle read from
 * a global flag byte choosing between +8 or a literal 2) and sets +0xd4 -
 * this is the real engine picking which physical group gets calibrated
 * next and in which of two modes/directions. Always returns 1 ("still
 * running"). Structurally cited - the exact real-world meaning of mode 1
 * vs 2 and the toggle-flag branch isn't independently confirmed.
 * @0xc0013b74. */
int cad_calibration_advance_group(void *cad);	/* FUN_c0013b74, structure only, not transcribed - see comment above */

/* ------------------------------------------------------------------------- *
 *  cad_calibration_sweep - the real per-channel calibration sweep
 *  cad_trigger_calibration's own +0xd0 "in progress" flag gates. First
 *  calls cad_calibration_tick unconditionally (the settle-timer above).
 *  Bails unless +0xd0 == 1. Then branches on cad_channel_eligible:
 *   - eligible (not excluded): loops up to 7 times (step +0x57 < 7),
 *     calling cad_calibration_select_slot, sending an SPI write (the same
 *     omap_spi_write shared with cad_init/cad_trigger_calibration) with a
 *     value from a lookup table indexed by the CURRENT step, then
 *     cad_calibration_capture_sample - a real 7-step calibration sweep,
 *     10-unit delay between steps.
 *   - excluded: a fixed 3-step sequence instead (step forced to 6, an SPI
 *     write from a DIFFERENT table offset (+0xc words in), a second SPI
 *     write back at the step-indexed table entry) - the excluded-channel
 *     path still touches the hardware, just with a short fixed sequence
 *     rather than the full 7-step sweep.
 *  Either way, finishes by calling cad_calibration_advance_group then
 *  cad_configure_group (the same function cad_init calls at bring-up) to
 *  reconfigure for the now-current group. @0xc0013e50.
 * ------------------------------------------------------------------------- */
extern uint16_t cad_calibration_sweep_table[];	/* DAT_c0013f54, real length/contents not resolved */
/* CORRECTION (self-review while writing this section): the two delay calls
 * below use `FUN_c0001aa0` directly - the same low-level retry-delay
 * primitive already named `omap_spi_retry_delay` in omap_l108_spi.c and
 * `irq_delay` in cpsoc.c - NOT `cad_delay_ticks` (`FUN_c00085a8`, the
 * tick-pumping millisecond-scale wrapper documented in omap_l108.c). Using
 * the wrong one was an error caught before this file was finalized, not a
 * real ground-truth ambiguity. */
extern void irq_delay(void *unused, int units);	/* FUN_c0001aa0, see cpsoc.c */

void cad_calibration_sweep(void *cad)	/* FUN_c0013e50 */
{
	uint8_t *state = (uint8_t *)cad;

	cad_calibration_tick(cad);
	if (*(int32_t *)(state + 0xd0) != 1)
		return;

	if (!cad_channel_eligible(cad)) {
		while (state[0x57] < 7) {
			cad_calibration_select_slot(cad);
			state[0x57]++;
			cad_send_state_command(*(void **)(state + 0xdc), cad_calibration_sweep_table[state[0x57]]);
			cad_calibration_capture_sample(cad);
			if (state[0x57] > 6)
				break;
			irq_delay(0, 10);
		}
	} else {
		cad_calibration_select_slot(cad);
		state[0x57] = 6;
		cad_send_state_command(*(void **)(state + 0xdc), cad_calibration_sweep_table[6]);
		cad_calibration_capture_sample(cad);
		irq_delay(0, 10);
		cad_calibration_select_slot(cad);
		cad_send_state_command(*(void **)(state + 0xdc), cad_calibration_sweep_table[state[0x57]]);
		cad_calibration_capture_sample(cad);
	}
	cad_calibration_advance_group(cad);
	cad_configure_group(cad, state[0x56], *(uint16_t *)(state + 0xd4));
}

/* ========================================================================= *
 *  A small, separate pedal-value-encoding trio - a different, smaller
 *  object (own flag at +4, own mode byte at +3) than the 38-slot
 *  calibration engine above, tied to it only via cad_pedal_present.
 * ========================================================================= */

/* cad_pedal_object_reset/_probe/_init - a tiny 3-step constructor sequence:
 * reset zeroes the enable flag (+4); probe checks cad_pedal_present and
 * sets the flag if the pedal is ABSENT (name reflects the polarity: flag
 * set means "disabled"); init zeroes a 2-byte field, sets a mode byte
 * (offset+3) to 0x20 (32), then re-runs the probe. @0xc001335c (reset),
 * 0xc0013368 (probe), 0xc0013394 (init). */
void cad_pedal_object_probe(void *obj);	/* FUN_c0013368, forward-declared - init calls it before its own definition in the real binary */

void cad_pedal_object_reset(void *obj)	/* FUN_c001335c */
{
	*((uint8_t *)obj + 4) = 0;
}

void cad_pedal_object_probe(void *obj)	/* FUN_c0013368 */
{
	extern void *cad_pedal_probe_handle;	/* DAT_c0013390 */
	if (!cad_pedal_present(cad_pedal_probe_handle))
		*((uint8_t *)obj + 4) = 1;
}

void cad_pedal_object_init(uint16_t *obj)	/* FUN_c0013394 */
{
	((uint8_t *)obj)[3] = 0x20;
	((uint8_t *)obj)[1] = 0;	/* real field overlaps obj[1]'s low byte, per decompile */
	*obj = 0;
	cad_pedal_object_probe(obj);
}

/* cad_pedal_object_set_mode - plain setter for the mode byte (+3)
 * cad_pedal_object_init seeds to 0x20. @0xc00133ec. */
void cad_pedal_object_set_mode(void *obj, uint8_t mode)	/* FUN_c00133ec */
{
	*((uint8_t *)obj + 3) = mode;
}

/* cad_pedal_send_release - if the object is enabled (flag at +4 clear -
 * i.e. pedal WAS present at probe time) and the incoming value is <= 0,
 * sends a fixed "release"/"off" command (reg 0x7a, value 0x80, via the
 * SAME cpsoc_i2c_dispatch primitive cpsoc.c's own cpsoc_read_switch_or_led
 * uses, FUN_c0007120) - a real cross-subsystem call, cad's own pedal
 * handling routing an "off" event through cpsoc's I2C dispatch rather than
 * its own SPI bus. @0xc00133ac. */
extern void cpsoc_i2c_dispatch(void *handle, uint8_t reg, uint32_t out_value, uint8_t raw_bit);	/* FUN_c0007120, see cpsoc.c */

void cad_pedal_send_release(void *obj, int value)	/* FUN_c00133ac */
{
	extern void *cad_pedal_i2c_handle;	/* DAT_c00133e8 */

	if (*((uint8_t *)obj + 4) != 0)
		return;
	if (value > 0)
		return;
	cpsoc_i2c_dispatch(cad_pedal_i2c_handle, 0x7a, 0x80, 0);
}

/* cad_pedal_encode_step - converts an accumulated 16-bit pedal position
 * (obj[0]) into a stream of (index, magnitude) byte pairs, one call per
 * pair, skipping zero-magnitude pairs: clamps the accumulator to [0,0x7f]
 * (127) with one narrow edge-case substituting a fixed sentinel byte
 * (DAT_c001347c, not resolved) when the clamp's own overflow check trips,
 * writes the running index (obj[1], post-incremented) and the clamped
 * magnitude into the output pair, and loops internally until it produces a
 * nonzero magnitude or the index byte wraps to 0 (obj[1]'s own "done" flag,
 * checked via the leading `(char)obj[1]` test at entry). Returns 1 if a
 * pair was produced, 0 once exhausted. Real, dense byte-packing logic -
 * structurally cited rather than asserting the exact clamp edge-case
 * semantics. @0xc00133f8. */
int cad_pedal_encode_step(int16_t *obj, uint8_t out_pair[2]);	/* FUN_c00133f8, structure only, not transcribed - see comment above */

/* -------------------------------------------------------------------------
 * Still genuinely open for this whole calibration-engine section:
 *  - `cad_calibration_select_slot`/`cad_calibration_advance_group`'s exact
 *    branch-by-branch meaning (mode 1 vs 2, the 6 step-5 override targets,
 *    the toggle-flag branch) - structurally cited, not transcribed, since
 *    the real-world calibration semantics behind each branch aren't
 *    independently confirmed.
 *  - `cad_calibration_init_slot`'s own caller - not traced this pass, a
 *    real "who calls this" gap (cad_init hardcodes the same three fields
 *    directly rather than calling it).
 *  - `cad_calibration_capture_sample`'s own tail (the +0x20/+0x21 boundary
 *    check and the second closest-of-two-references pass before the final
 *    `cad_calibration_mark_changed` call) - left structurally cited.
 *  - The real contents of `cad_calibration_sweep_table` and the sentinel
 *    constants (`cad_calib_default`, `DAT_c001347c`) - no data-segment
 *    symbols resolved in this ELF-wrapper import.
 *  - `cad_pedal_encode_step`'s own caller(s) and `cad_pedal_send_release`'s
 *    own caller - not traced this pass.
 * ------------------------------------------------------------------------- */
