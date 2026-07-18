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
extern void cad_calibration_progress_pump(void);	/* FUN_c0005a1c, cad.c-internal, not reconstructed here */
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
