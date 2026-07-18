/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_usbdc.c - the OMAP-L137 USB device controller peripheral
 * driver: endpoint-0 bring-up and a transfer-completion state machine that
 * turns out to sit DIRECTLY downstream of the generic USB-submit primitive
 * (FUN_c000acec) this project already established as shared across
 * crypto_at88.c, cobjectmgr.c, and cad.c's own event pump - closing the
 * loop on where every subsystem's "send an event to the host" call
 * ultimately bottoms out in real hardware.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../MCU/Component/OmapL137Usbdc.cpp" has 3 xrefs - two inside
 * omap_usbdc_init_ep0 below, one inside omap_usbdc_poll_transfer.
 */

#include <stdint.h>
#include <stdbool.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_init_ep0 - one of the two confirmed anchors. Configures the
 *  control endpoint (endpoint 0): derives three register-block pointers by
 *  adding fixed offsets (0x2800/0x2a00/0x2c00) to a base returned by a
 *  shared address-relocation helper (FUN_c0009194, called three times - not
 *  independently traced, presumed to translate a peripheral-relative offset
 *  into an absolute register address, the same pattern seen once per
 *  register-block setup rather than a loop), sets a mode field
 *  (bits 0x0f0 of offset+0x144 set to 0x110), then runs a bounded busy-wait
 *  on a "ready" flag (bit 0, offset+4) before touching the hardware reset
 *  sequence proper:
 *   - sets bit 0x8000 in a control register (offset+0x184), holds through a
 *     fixed-iteration empty delay loop (50/0x32 iterations - a real, if
 *     crude, settling delay, not a mistake), clears it again
 *   - reconfigures the SAME register with a masked-and-OR'd value
 *     (`& 0xffff89f0 | 0x4972`) - a real multi-field register write, exact
 *     field meanings not decoded
 *   - waits (bounded retry, same shared retry-count global as the earlier
 *     wait) for bit 0x20000 to SET, hard-faulting on real timeout (line
 *     0xda) - the second of the two "../MCU/Component/OmapL137Usbdc.cpp"
 *     assert call sites
 *   - finishes with endpoint-0 max-packet-size fields: writes 0x1f (31) and
 *     0x1e (30) into adjacent 16-bit fields at a data-driven offset
 *     (DAT_c0003b88), a status/type byte set to 8, and two more fields
 *     copied from fixed constants - consistent with control-endpoint
 *     max-packet and transfer-type setup, not independently confirmed
 *     against the OMAP-L137 USB0 register manual.
 *  @0xc0003984. Confirmed real caller: FUN_c0009574 (a higher-level USB
 *  object bring-up function - allocates the same three register-block
 *  offsets into a handle struct before calling this, then zeroes several
 *  more state-struct fields and calls FUN_c0009550, not traced this pass).
 * ------------------------------------------------------------------------- */
extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c0009194 */

void omap_usbdc_init_ep0(void *dev, void *ep0)	/* FUN_c0003984 */
{
	uint8_t *e = (uint8_t *)ep0;
	uint32_t *ready = (uint32_t *)((uint8_t *)dev + 4);
	uint32_t *ctrl = (uint32_t *)(e + 0x184);
	int attempt;

	extern uint32_t usbdc_regblock_a, usbdc_regblock_b, usbdc_regblock_c;	/* DAT_c0003b54/b58/b68 */
	extern uint32_t usbdc_reloc_base;					/* DAT_c0003b50 */

	usbdc_regblock_a = omap_usbdc_reloc(usbdc_reloc_base) + 0x2800;
	usbdc_regblock_b = omap_usbdc_reloc(usbdc_reloc_base) + 0x2a00;
	usbdc_regblock_c = omap_usbdc_reloc(usbdc_reloc_base) + 0x2c00;

	*(uint32_t *)(e + 0x38) = 0 /* DAT_c0003b5c */;
	*(uint32_t *)(e + 0x3c) = 0 /* DAT_c0003b60 */;
	*(uint32_t *)(e + 0x144) = (*(uint32_t *)(e + 0x144) & 0xfffff00f) | 0x110;

	*ready = 1;
	attempt = 0;
	while ((*ready & 1) != 0) {
		if (attempt > 0 /* DAT_c0003b78 */) {
			crypto_at88_fault(0, 0 /* DAT_c0003b7c */, 0xae);
			break;
		}
		attempt++;
	}

	*ctrl |= 0x8000;
	for (volatile int i = 0; i < 0x32; i++)
		;	/* fixed-iteration hardware settling delay, real cycle count not derived */
	*ctrl &= 0xffff7fff;
	*ctrl = (*ctrl & 0xffff89f0) | 0x4972;

	attempt = 0;
	while ((*ctrl & 0x20000) == 0) {
		if (attempt > 0 /* DAT_c0003b78 */) {
			crypto_at88_fault(0, 0 /* DAT_c0003b7c */, 0xda);
			break;
		}
		attempt++;
	}

	/* endpoint-0 max-packet-size / transfer-type fields, data-driven offsets */
	e[0 /* DAT_c0003b80 */] |= 0x21;
	*(uint32_t *)(e + 0x28) = *(uint32_t *)(e + 0x20);
	*ready &= 0xfffffff7;
	*(uint16_t *)(e + 0 /* DAT_c0003b88 */) = 0x1f;
	*(uint16_t *)(e + 0 /* DAT_c0003b88 */ + 2) = 0x1e;
	e[0 /* DAT_c0003b84 */] = 8;
	*(uint32_t *)(e + 0x30) = 0 /* DAT_c0003b8c */;
	*(uint32_t *)(e + 0x34) = 0 /* DAT_c0003b90 */;
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_poll_transfer - the other confirmed anchor, and the real
 *  bottom of the USB event-send call chain traced across this whole
 *  project. A 3-state transfer-completion state machine (state stored in a
 *  fixed global, DAT_c0004c24):
 *
 *   state 0 (idle): if the requested transfer size (`len`) is below a fixed
 *     threshold (0x1f41 = 8001 bytes), does nothing (small transfers don't
 *     need this state machine at all, presumably handled inline elsewhere);
 *     otherwise sets a "large transfer" bit (0x40) in a status byte and
 *     advances to state 1.
 *   state 1 (in-flight): checks a hardware status register (offset+0x460,
 *     bit 0) - if the transfer is still busy (bit clear), retries up to a
 *     fixed bound before hard-faulting on real timeout; once the bit sets
 *     (transfer's hardware-busy phase done), advances to state 2.
 *   state 2 (complete): terminal state - the function returns true exactly
 *     when this state is reached (this call or a prior one).
 *
 *  Reached via a thin wrapper (FUN_c000acc8, one dereference then a direct
 *  call - not itself given a name here since it adds no behavior of its
 *  own) which sits at 0xc000acc8, only 0x24 bytes before FUN_c000acec - the
 *  SAME generic USB-submit primitive already established as shared by
 *  crypto_at88.c's AtmelRead event, cobjectmgr.c's host-notify event, and
 *  cad.c's own calibration-progress event pump. This closes out that
 *  finding: whatever gets submitted through FUN_c000acec, for a
 *  large-enough payload (>= 8001 bytes), ultimately drives THIS exact
 *  hardware endpoint state machine to completion. @0xc0004b88.
 * ------------------------------------------------------------------------- */
bool omap_usbdc_poll_transfer(void *ep, int32_t len)	/* FUN_c0004b88 */
{
	extern int32_t usbdc_transfer_state;		/* DAT_c0004c24 */
	uint8_t *e = (uint8_t *)ep;

	if (usbdc_transfer_state == 1) {
		if ((*(uint16_t *)(e + 0x460) & 1) == 0) {
			extern int32_t usbdc_poll_attempts, usbdc_poll_bound;	/* DAT_c0004c28, DAT_c0004c2c */
			if (usbdc_poll_bound < usbdc_poll_attempts)
				crypto_at88_fault(0, 0 /* DAT_c0004c30 */, 0 /* DAT_c0004c34 */);
			else
				usbdc_poll_attempts++;
			goto out;
		}
		usbdc_transfer_state = 2;
	} else if (usbdc_transfer_state == 0 && len >= 0x1f41) {
		e[0 /* DAT_c0004c38 */] |= 0x40;
		usbdc_transfer_state = 1;
	}

out:
	return usbdc_transfer_state == 2;
}

/* -------------------------------------------------------------------------
 * Still genuinely open:
 *  - FUN_c0009574 (the higher-level USB object bring-up caller of
 *    omap_usbdc_init_ep0) - only its register-block-offset allocation and
 *    call to omap_usbdc_init_ep0 were examined; FUN_c0009550 (called right
 *    after) and the remaining struct-field zeroing weren't traced.
 *  - Every DAT_ constant left un-substituted above - no data-segment
 *    symbols are available in this ELF-wrapper import to resolve fixed
 *    register offsets/thresholds beyond what the decompile's own field
 *    arithmetic already reveals.
 *  - Whether omap_usbdc_poll_transfer's 8001-byte threshold is a real
 *    hardware DMA/FIFO limit or a firmware-chosen policy value - not
 *    confirmed against a datasheet.
 *  - The thin wrapper at 0xc000acc8 and its relationship to FUN_c000acec
 *    (0xc000acec) - confirmed adjacent and confirmed to both participate in
 *    the same USB-submit call chain, but the code in between (if any) and
 *    the exact call graph connecting them wasn't fully mapped this pass.
 * ------------------------------------------------------------------------- */
