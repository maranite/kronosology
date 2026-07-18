/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l108_spi.c - the OMAP-L1x SPI peripheral driver's transmit primitive:
 * a single busy-wait-then-write register function that turns out to be the
 * real hardware underneath THREE higher-level subsystems this project has
 * reconstructed - `cad.c`'s cad_send_state_command/cad_hw_config, and
 * cpsoc.c's own switch/LED scan chip AND its separate third-device analog-
 * polling chain (cpsoc_analog_poll_task and friends - see cpsoc.c).
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../MCU/Component/OmapL108Spi.cpp" has 2 xrefs, both hard-fault
 * call sites inside the one function this file reconstructs.
 */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);
extern void omap_spi_retry_delay(void *unused);	/* FUN_c0001aa0 */

/* ------------------------------------------------------------------------- *
 *  omap_spi_write - the confirmed anchor, and the single real primitive
 *  this whole source file amounts to. TWO busy-wait phases against the same
 *  status register (offset +0x40 from the SPI peripheral handle), testing
 *  TWO DIFFERENT bits - not the same condition checked twice:
 *
 *   1. Waits for bit 0x80000000 (the sign bit, tested via a signed `>= 0`
 *      comparison in the real decompile) to become SET - plausibly a
 *      "transfer complete"/"line idle" flag from a PRIOR transaction that
 *      must clear before a new one can start - retrying up to 30 (0x1e)
 *      times via omap_spi_retry_delay before hard-faulting on real timeout
 *      (line 0x5a).
 *   2. Waits for a SEPARATE bit, 0x20000000, to become CLEAR (a "TX FIFO
 *      full"/"not ready to accept" flag, most likely) - same retry bound,
 *      hard-faulting at line 0x68 - then falls through to the actual
 *      register write.
 *
 *  On success, writes the low 16 bits of `value` into the control register
 *  at offset +0x3c, preserving the high 16 bits already there (a
 *  read-modify-write, not a blind overwrite) - consistent with a combined
 *  control/data register where the upper half carries persistent
 *  configuration bits this function must not disturb.
 *
 *  Confirmed real callers - re-verified via `get_xrefs_to`, which returns
 *  8 total call sites (NOT the 6 the first pass of this file accounted
 *  for; corrected below rather than left as an undercount):
 *   - `cad_trigger_calibration` (cad.c, FUN_c00073e8) - sends the
 *     eligibility-selected state value (0 or 4) during pedal calibration.
 *   - `cad_init` (cad.c, FUN_c00136c0) - the hardware reset/config command
 *     (0x9000) at bring-up.
 *   - `FUN_c0013e50` (cad.c range, not yet reconstructed) - 3 of the 8 call
 *     sites, a per-channel processing function that also calls
 *     `cad_configure_group` and `cad_channel_eligible`, plausibly the real
 *     per-channel analog sample routine cad.c's own "still open" list
 *     flagged as undecompiled.
 *   - `cpsoc_analog_poll_channel` (`FUN_c0011534`) - had **zero static
 *     callers** of its own until this pass (confirmed via xref search).
 *     RESOLVED: its real caller is `cpsoc_analog_poll_task` (below), not
 *     `eva_board_main.c`'s init table as an earlier draft guessed - that
 *     table is now known to have exactly one, unrelated entry.
 *   - **A 5th caller**: `cpsoc_analog_poll_task` (`FUN_c0011624`),
 *     accounting for the remaining 2 call sites (`0xc0011658`,
 *     `0xc0011678`) - a standalone, never-returning background polling
 *     task, not a duplicate of `cpsoc_analog_poll_channel`. Both are now
 *     confirmed to be cpsoc.c's own third-device analog-polling chain -
 *     see cpsoc.c's own new section below.
 *
 *  This confirms the AD (analog) chip and the PSoC scan chip share the same
 *  physical SPI bus and the same low-level transmit primitive - the SPI
 *  equivalent of `I2cByGpio.cpp` being a shared bus driver rather than
 *  private to one chip, already established for I2C in crypto_at88.c/
 *  cdix4192.c. @0xc00035a4.
 * ------------------------------------------------------------------------- */
struct omap_spi_handle {
	uint8_t pad0[0x3c];
	uint32_t ctrl;		/* +0x3c: control/data register, low 16 bits = TX data */
	uint32_t status;	/* +0x40: status register, bit 0x20000000 = busy/not-ready */
};

void omap_spi_write(struct omap_spi_handle *spi, uint16_t value)	/* FUN_c00035a4 */
{
	uint32_t ctrl_hi = spi->ctrl;
	int attempt = 0;

	/* phase 1: wait for status bit 0x80000000 (sign bit) to become SET */
	while ((int32_t)spi->status >= 0) {
		if (attempt > 0x1d) {
			crypto_at88_fault(0, 0 /* DAT_c0003668 */, 0x5a);
			break;
		}
		omap_spi_retry_delay(0 /* DAT_c0003664 */);
		attempt++;
	}

	/* phase 2: wait for a DIFFERENT status bit, 0x20000000, to become CLEAR */
	attempt = 0;
	while ((spi->status & 0x20000000) != 0) {
		if (attempt > 0x1d) {
			crypto_at88_fault(0, 0 /* DAT_c0003668 */, 0x68);
			break;
		}
		omap_spi_retry_delay(0 /* DAT_c0003664 */);
		attempt++;
	}

	spi->ctrl = (ctrl_hi & 0xffff0000) | value;
}

/* ------------------------------------------------------------------------- *
 *  RESOLVED (SPI-device closure pass, 2026-07-17): the "5th caller" flagged
 *  above (`FUN_c0011624`) and `FUN_c0011534` are both real, substantial
 *  functions that turned out to be cpsoc.c's OWN third SPI-bus device - a
 *  standalone analog-polling background task and its ADC-read/LED-bargraph-
 *  drive chain. Fully reconstructed as `cpsoc_analog_poll_task`/
 *  `cpsoc_analog_poll_channel`/`cpsoc_event_opcode_dispatch` and their own
 *  LED-driving sub-handlers - see cpsoc.c's own new section for the full
 *  writeup, including the register-encoding evidence (a real 0x79/0x7a
 *  two-bank split matching cpsoc.c's already-documented convention exactly)
 *  that resolved the attribution despite this address range having no
 *  `__FILE__` string anchor of its own.
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Still genuinely open:
 *  - Real bit-level meaning of status bit 0x20000000 (documented as
 *    "not ready"/"busy" by usage pattern, not confirmed against a datasheet).
 *  - omap_spi_retry_delay (FUN_c0001aa0) itself: not traced - a plain
 *    fixed-duration spin/NOP delay is assumed given the surrounding retry
 *    loop shape, not confirmed.
 *  - Whether a corresponding omap_spi_read exists elsewhere in this address
 *    range (this pass only found the confirmed anchor's own transmit path;
 *    no read-side counterpart was searched for).
 *  - FUN_c0013e50 itself - a real candidate for a future reconstruction
 *    pass on cad.c, out of scope for this SPI-driver-focused file.
 * ------------------------------------------------------------------------- */
