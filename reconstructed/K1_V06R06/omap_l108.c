/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l108.c - the SoC-level tick timer service: a small, generic
 * free-running-counter API used throughout the firmware for elapsed-time
 * measurement, not anything cad/analog-specific despite `cad.c`'s own
 * `cad_delay_ticks` being its best-known caller.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../MCU/OmapL108.cpp" has exactly one xref, inside the init-guard
 * assert in omap_tick_init below.
 */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* ------------------------------------------------------------------------- *
 *  omap_tick_config_ptr - trivial accessor returning a fixed constant
 *  (DAT_c0001944) - most likely the hardware tick-counter's base address or
 *  a fixed clock-rate constant, not an active reconfiguration call despite
 *  being invoked on every elapsed-time query below. Real identity of the
 *  constant not resolved (no data-segment symbol in this ELF-wrapper
 *  import). @0xc000193c.
 * ------------------------------------------------------------------------- */
extern uint32_t omap_tick_config_ptr(void);	/* FUN_c000193c, returns DAT_c0001944 verbatim */

/* ------------------------------------------------------------------------- *
 *  omap_tick_read_raw - reads the free-running tick counter's current value
 *  from a fixed offset (+0x10) in the handle struct. A plain register/memory
 *  read, no side effects. @0xc0001d68.
 * ------------------------------------------------------------------------- */
uint32_t omap_tick_read_raw(void *handle)	/* FUN_c0001d68 */
{
	return *(uint32_t *)((uint8_t *)handle + 0x10);
}

/* ------------------------------------------------------------------------- *
 *  omap_tick_init - the confirmed anchor. Init-once guard (hard-faults if
 *  the handle's flag byte is already set - double-init is treated as a real
 *  bug, not silently ignored), calls omap_tick_config_ptr (role unclear -
 *  possibly just fetching the fixed base/rate constant into a local, not a
 *  hardware side effect per its own trivial body), then snapshots the
 *  current raw tick count into the handle as the elapsed-time baseline
 *  (offset +4). @0xc0001b38.
 * ------------------------------------------------------------------------- */
void omap_tick_init(void *handle)	/* FUN_c0001b38 */
{
	uint8_t *h = (uint8_t *)handle;

	if (*h != 0)
		crypto_at88_fault(0, 0 /* DAT_c0001b7c */, 0x350);

	*h = 1;
	omap_tick_config_ptr();
	*(uint32_t *)(h + 4) = omap_tick_read_raw(handle);
}

/* ------------------------------------------------------------------------- *
 *  omap_tick_elapsed_scaled - re-arms nothing (just re-fetches the constant
 *  via omap_tick_config_ptr, same as omap_tick_init does, then re-reads the
 *  raw counter), computes ticks elapsed since the baseline omap_tick_init
 *  set, with wraparound correction (adds a fixed wrap constant,
 *  DAT_c0001bcc, if the counter appears to have gone backwards), and
 *  converts the result through a SHARED fixed-point scale helper
 *  (FUN_c001e3f8, divisor 150/0x96) into whatever unit the caller wants
 *  (plausibly milliseconds, not confirmed). @0xc0001b8c.
 *
 *  IMPORTANT CROSS-FILE FINDING: FUN_c001e3f8 was previously flagged in
 *  `clcdc.c` as "clcdc_progress_bar's exact fixed-point scaling math, not
 *  traced into" - it is now confirmed to be a SECOND, unrelated caller of
 *  the exact same function. FUN_c001e3f8 is a generic, firmware-wide
 *  fixed-point tick-to-unit scaler, not clcdc-specific - `clcdc.c`'s own
 *  note should be read with this correction in mind.
 * ------------------------------------------------------------------------- */
extern int32_t omap_tick_scale(int32_t ticks, int divisor);	/* FUN_c001e3f8, shared with clcdc_progress_bar */

int32_t omap_tick_elapsed_scaled(void *handle)		/* FUN_c0001b8c */
{
	uint8_t *h = (uint8_t *)handle;
	extern int32_t omap_tick_wrap_const;	/* DAT_c0001bcc */
	int32_t now, elapsed;

	omap_tick_config_ptr();
	now = omap_tick_read_raw(handle);
	elapsed = now - *(int32_t *)(h + 4);
	if (now < *(int32_t *)(h + 4))
		elapsed += omap_tick_wrap_const;

	return omap_tick_scale(elapsed, 0x96);
}

/* ------------------------------------------------------------------------- *
 *  cad_delay_ticks (cad.c's own name, FUN_c00085a8) - documented here rather
 *  than in cad.c because it's genuinely built entirely out of this file's
 *  primitives, with one cad.c-specific addition. NOT a busy-wait: bounds
 *  the requested delay against a fixed maximum (hard-faults if exceeded),
 *  calls omap_tick_init to arm a fresh baseline, then loops calling (a) a
 *  cad.c-internal per-tick pump function (FUN_c0005a1c - pops pending
 *  calibration/pedal-presence events, sends a 4-byte record through the
 *  SAME generic USB-submit primitive already established in
 *  crypto_at88.c/cobjectmgr.c, FUN_c000acec, and conditionally draws
 *  calibration-progress text via clcdc.c's own FUN_c0015650 text primitive -
 *  this is cad.cpp's own calibration-progress display pump, not anything
 *  OmapL108-specific, and NOT reconstructed as its own function here) and
 *  (b) cobjectmgr_tick (already reconstructed in cobjectmgr.c) - until
 *  omap_tick_elapsed_scaled reports the target has been reached. A real
 *  "delay while continuing to service the dispatch loop and the calibration
 *  UI" pattern, consistent with a cooperatively-scheduled embedded firmware
 *  that can't afford to miss dispatch events during what looks like a
 *  simple delay call. @0xc00085a8.
 * ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- *
 *  cad_calibration_progress_pump (cad.c's own name, FUN_c0005a1c) - RESOLVED
 *  this pass (2026-07-18, static-dump re-query) and reconstructed HERE, in
 *  omap_l108.c, rather than in cad.c: this file owns cad_delay_ticks, the
 *  function's own sole non-dispatcher caller, and the same cross-file-
 *  attribution precedent already established for that function applies
 *  here too. Ownership note: this is genuinely cad.cpp's own compiled code
 *  (it reads/writes cad's 38-slot calibration state via
 *  cad_calibration_pop_changed and cad_calibration_slot_is_raw, both
 *  cad.c's own functions), documented in this file purely because of where
 *  its only two real callers live (cad_delay_ticks here, and
 *  FUN_c0008b64's own conditional dispatch - see below).
 *
 *  Real structure, fully traced: an unbounded loop that keeps draining
 *  cad's changed-slot queue (cad_calibration_pop_changed) until it reports
 *  empty. For each popped slot that is NOT one of the 4 "raw" boundary
 *  slots (cad_calibration_slot_is_raw) - i.e. only for slots that went
 *  through the smoothing filter - it:
 *   1. builds a 4-byte wire-format event record (a lookup/remap through
 *      FUN_c0014338 on the slot index, plus two bit-packed fields
 *      extracted from the popped 16-bit value - see the note on the exact
 *      shift arithmetic below) and submits it via FUN_c000acec, the SAME
 *      generic USB-submit primitive already established across
 *      crypto_at88.c/cobjectmgr.c/omap_l137_usbdc.c.
 *   2. if a pedal-presence probe (FUN_c00094d8, the SAME primitive cad.c's
 *      own cad_pedal_present uses) reports true, ALSO draws live
 *      calibration-progress text at two fixed screen columns (x=300 and
 *      x=0x17c/380) via clcdc.c's own FUN_c0015650 text primitive - a
 *      genuine on-screen calibration progress readout, gated on a pedal
 *      actually being present, not shown unconditionally.
 *
 *  Three helper calls are left uninterpreted rather than guessed at:
 *  FUN_c0014338 (slot -> value transform feeding the event record and the
 *  text draw), FUN_c00145c4 (value -> something drawn as text, plausibly a
 *  number-to-string/glyph-index conversion), and FUN_c00168fc (called only
 *  in the pedal-present branch, arguments include the raw popped value -
 *  real role not traced). The two bit-extraction expressions
 *  (`(v<<16)>>10` and `(v<<16)>>18`, each then truncated to a byte) are
 *  transcribed exactly as decompiled - worked out to extract bits 0-1 and
 *  bits 2-9 of the popped 16-bit value respectively, into two adjacent
 *  wire-record bytes - but their real-world meaning (a packed 10-bit ADC
 *  reading plus 2 flag bits, by shape) is not confirmed, so documented
 *  rather than asserted. @0xc0005a1c.
 * ------------------------------------------------------------------------- */
extern int   cad_calibration_pop_changed(void *cad, void *out);	/* FUN_c0013f5c, see cad.c */
extern int   cad_calibration_slot_is_raw(void *cad, int slot);	/* FUN_c0013910, see cad.c */
extern int   cad_pedal_present(void *probe_handle);			/* FUN_c00094d8, see cad.c */
extern void  omap_usbdc_submit_event(void *handle, const void *rec, int len);	/* FUN_c000acec, see crypto_at88.c/omap_l137_usbdc.c */
extern void  clcdc_draw_text(int x, int y, const void *str, int unused);	/* FUN_c0015650, see clcdc.c */
extern int   cad_progress_slot_remap(int slot);	/* FUN_c0014338, role not traced */
extern void *cad_progress_value_to_text(int value);	/* FUN_c00145c4, role not traced */
extern void  cad_progress_unknown_c168fc(void *a, void *b, uint16_t raw_value);	/* FUN_c00168fc, role not traced */

void cad_calibration_progress_pump(void)	/* FUN_c0005a1c */
{
	extern void *cad_calibration_state;			/* DAT_c0005b10, the cad.c 38-slot state pointer */
	extern void *cad_progress_usb_handle;			/* DAT_c0005b00 */
	extern void *cad_progress_pedal_probe_handle;		/* DAT_c0005b04 */
	extern void *cad_progress_text_buf;			/* DAT_c0005b0c */
	extern void *cad_progress_text_src;			/* DAT_c0005b08 */

	struct { uint8_t slot; int16_t value; } popped;

	while (cad_calibration_pop_changed(cad_calibration_state, &popped)) {
		if (cad_calibration_slot_is_raw(cad_calibration_state, popped.slot))
			continue;

		int remapped = cad_progress_slot_remap(popped.slot);
		uint16_t raw = (uint16_t)popped.value;
		/* 4-byte wire record: tag=3, two bit-packed fields from `raw`
		 * (see the header comment above for the exact, unconfirmed,
		 * bit-range meaning), then the remapped slot byte. */
		uint8_t record[4];
		record[3] = 3;
		record[1] = (uint8_t)((((uint32_t)raw) << 0x10) >> 10);
		record[0] = (uint8_t)((((uint32_t)raw) << 0x10) >> 0x12);
		record[2] = (uint8_t)remapped;
		omap_usbdc_submit_event(cad_progress_usb_handle, &record[0], 4);
		/* real record base pointer is &local_1c i.e. this array's
		 * byte[0] in the decompile's own stack layout - transcribed
		 * with the same relative byte order here. */

		if (cad_pedal_present(cad_progress_pedal_probe_handle)) {
			void *text = cad_progress_value_to_text(remapped);
			uint32_t y = (uint32_t)(remapped * 0xa0000 + 0x640000U) >> 0x10;

			clcdc_draw_text(300, y, text, 0);
			cad_progress_unknown_c168fc(cad_progress_text_buf, cad_progress_text_src, raw);
			clcdc_draw_text(0x17c, y, cad_progress_text_buf, 0);
		}
	}
}

extern void cobjectmgr_tick(void *mgr);		/* FUN_c0007c2c, see cobjectmgr.c */

void cad_delay_ticks(void *unused_param, int32_t target)	/* FUN_c00085a8 */
{
	extern int32_t omap_tick_delay_max;		/* DAT_c0008608 */
	extern uint8_t *cad_delay_tick_handle;		/* DAT_c0008614: fixed handle, ignores unused_param */

	if (target > omap_tick_delay_max)
		crypto_at88_fault(0, 0 /* DAT_c000860c */, 0 /* DAT_c0008610 */);

	omap_tick_init(cad_delay_tick_handle);
	do {
		cad_calibration_progress_pump();
		cobjectmgr_tick((void *)cad_delay_tick_handle);	/* reinterpreted as cobjectmgr_state */
	} while (omap_tick_elapsed_scaled(cad_delay_tick_handle) < target);

	*cad_delay_tick_handle = 0;	/* clears the handle's flag byte on exit */
}

/* -------------------------------------------------------------------------
 * Still genuinely open:
 *  - The real identity of DAT_c0001944 (omap_tick_config_ptr's return
 *    value) and DAT_c0001bcc (the wraparound constant) - no data-segment
 *    symbols available in this ELF-wrapper import to resolve them further.
 *  - Whether omap_tick_read_raw's +0x10 offset is a memory-mapped hardware
 *    timer register or a software counter incremented by a periodic ISR -
 *    not traced either way.
 *  - cad_calibration_progress_pump (FUN_c0005a1c) itself: real cad.c-owned
 *    logic, deliberately not reconstructed in this file's scope. Belongs as
 *    a future cad.c addendum, not here.
 * ------------------------------------------------------------------------- */
