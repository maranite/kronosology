/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_usbdc_ep0_misc.c - reconstructs 4 genuinely-uncovered
 * functions clustered in the address range this task assigned as
 * "0xc000aee8-0xc000af88 (3 fns, near omap_l137_usbdc_ep0.c)": FUN_c000aee8,
 * FUN_c000af24, FUN_c000af80, FUN_c000af88 (a real 4th function inside the
 * same span the task's own count missed - confirmed genuinely present and
 * genuinely uncovered, not force-fit; see each function's own note below).
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18 pass), read via query_dump.py. No live Ghidra
 * MCP calls this pass (bridge flagged concurrency-unsafe under this
 * project's own parallel-agent constraint).
 *
 * WHY THESE ARE GENUINE GAPS (checked before writing anything): both
 * omap_l137_usbdc_ep0.c and omap_l137_usbdc_ext.c cite FUN_c000aee8 as
 * `usbdc_min_u32` and FUN_c000af24 as `usbdc_fifo_read_u32_unaligned`
 * respectively, each time as a bare `extern` with a comment explicitly
 * disclaiming ownership ("out of this project's range entirely - clamp
 * helper" / see that file's own FIFO-read section) - never a body,
 * confirmed via a fresh grep of every *.c file in this project.
 * FUN_c000af80 and FUN_c000af88 aren't cited anywhere at all yet.
 *
 * ANCHOR: NONE - same situation as omap_l137_usbdc_ep0.c's own file
 * header (this whole EP0 cluster uses "stall EP0" as its error path, not
 * the crypto_at88_fault-style hard-halt with a __FILE__ string). Kept as
 * a separate small file (not folded into omap_l137_usbdc_ep0.c or _ext.c)
 * per this task's own "do not edit existing files" constraint.
 *
 * =============================================================================
 * usbdc_min_u32 - FUN_c000aee8 (12 bytes)
 * =============================================================================
 * Trivial unsigned clamp/minimum: return (b < a) ? b : a. Real body:
 *
 *   uint FUN_c000aee8(uint param_1, uint param_2)
 *   { if (param_2 < param_1) param_1 = param_2; return param_1; }
 *
 * 4 confirmed callers per xrefs_to: FUN_c0009768 (usbdc_ep0_state3_handler,
 * omap_l137_usbdc_ep0.c - already-covered caller, confirms this cluster's
 * EP0 attribution), FUN_c000bf54 (chan_port_reg60_rx_pump,
 * chan_slot_dispatch.c), FUN_c000c158/FUN_c000c168 (also chan_slot_
 * dispatch.c). Genuinely shared between the USB EP0 cluster and the
 * channel/link cluster - consistent with both citing files independently
 * calling it a generic helper, not owned by either.
 *
 * =============================================================================
 * usbdc_fifo_read_u32_unaligned - FUN_c000af24 (40 bytes)
 * =============================================================================
 * Unaligned little-endian 32-bit read from a byte pointer (explicit
 * byte-at-a-time reassembly, not a plain `*(uint32_t *)`, consistent with
 * an ARM926 unaligned-access-unsafe compile target). Real body:
 *
 *   int FUN_c000af24(byte *param_1)
 *   { return (uint)param_1[2]*0x10000 + (uint)param_1[3]*0x1000000
 *          + (uint)param_1[1]*0x100 + (uint)*param_1; }
 *
 * Sole caller (per xrefs_to): FUN_c00047d4 at call site 0xc0004824 -
 * FUN_c00047d4 is itself out of range for every file in this project so
 * far (not independently confirmed to be EP0-specific beyond omap_l137_
 * usbdc_ext.c's own citation of this function as "FIFO register [read],
 * 4 bytes at a time via FUN_c000af24").
 *
 * =============================================================================
 * usbdc_ep0_desc_field_set - FUN_c000af80 (8 bytes)
 * =============================================================================
 * Trivial single-word store: *dst = value. Real body:
 *
 *   void FUN_c000af80(undefined4 *param_1, undefined4 param_2)
 *   { *param_1 = param_2; return; }
 *
 * Sole caller (per xrefs_to): FUN_c000afe0 at call site 0xc000dddc, itself
 * just past this file's own range (0xc000af88+84 = 0xc000afdc, and
 * FUN_c000afe0 starts at 0xc000afe0 - immediately adjacent, not
 * reconstructed here, out of this task's assigned span). FUN_c000afe0's
 * own real call site is `FUN_c000af80(DAT_c000de08, param_2)` - a fixed
 * global destination, not a per-call pointer, so this really is just a
 * one-off "write this fixed global" indirection rather than a generic
 * setter; kept generically-typed since the real formal signature IS a
 * generic `(undefined4 *, undefined4)` pair.
 *
 * =============================================================================
 * usbdc_ep0_session_flag_check - FUN_c000af88 (84 bytes)
 * =============================================================================
 * Gate function: true only if (a) a hardware status bit (bit 0, via
 * midi_hw_read16-style register 0x34 off the handle's own first field)
 * is set, (b) the caller-supplied mode/class selector == 2, AND (c) a
 * byte flag at a FIXED global handle (DAT_c000afdc, resolved 0xC01CCE64)
 * + offset 0x171 == 1. Real body:
 *
 *   bool FUN_c000af88(undefined4 *param_1, int param_2)
 *   {
 *     uint uVar1 = FUN_c0000c6c(*(undefined4 *)*param_1, 0x34);
 *     if ((uVar1 & 1) == 0) return false;
 *     if (param_2 != 2) return false;
 *     return *(char *)(*DAT_c000afdc + 0x171) == '\x01';
 *   }
 *
 * FUN_c0000c6c is midi_hw_read16 (soc_irq_gate.c) - real, confirmed cross-
 * file call. The +0x171 byte offset is SUGGESTIVE (not proven - different
 * fixed handle) of chan_param_ctrl.c's own Part A "link" object, whose
 * own documented +0x16f..0x174 field is an "6-byte scratch region, cleared
 * by chan_link_obj_init" - the SAME byte range, but chan_param_ctrl.c's
 * own handle is chan_link_ctx (0xC01CC750, per usbdc_ep_notify_ring.c),
 * NOT this function's 0xC01CCE64. Flagged as a plausible but unconfirmed
 * structural echo, not asserted as the same struct - per this project's
 * own "additive, not force-fit" convention.
 *
 * Zero callers found anywhere in the full 691-function xrefs_to data
 * (query_dump.py's own xrefs_to list for this address is empty) - same
 * "referenced from a table/init-list, not a real CALL instruction" or
 * genuinely-dead-code situation this project has already flagged
 * elsewhere (e.g. crypto_at88_self_test, soc_irq_gate.c's ring3 reset).
 * NOT independently resolved which of the two explanations applies here.
 *
 * @0xc000aee8 / @0xc000af24 / @0xc000af80 / @0xc000af88.
 */

#include <stdint.h>
#include <stdbool.h>

extern uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off);	/* FUN_c0000c6c, soc_irq_gate.c */

uint32_t usbdc_min_u32(uint32_t a, uint32_t b)	/* FUN_c000aee8 */
{
	return (b < a) ? b : a;
}

uint32_t usbdc_fifo_read_u32_unaligned(const uint8_t *src)	/* FUN_c000af24 */
{
	return (uint32_t)src[2] * 0x10000u + (uint32_t)src[3] * 0x1000000u +
	       (uint32_t)src[1] * 0x100u + (uint32_t)src[0];
}

void usbdc_ep0_desc_field_set(uint32_t *dst, uint32_t value)	/* FUN_c000af80 */
{
	*dst = value;
}

/* usbdc_ep0_ctx_handle_link - DAT_c000afdc, resolved 0xC01CCE64. NOT
 * confirmed to be the same object as chan_param_ctrl.c's chan_link_ctx
 * (0xC01CC750) - see note above. */
extern void **usbdc_ep0_ctx_handle_link;	/* DAT_c000afdc -> 0xC01CCE64 */

bool usbdc_ep0_session_flag_check(uint32_t **handle, int mode)	/* FUN_c000af88 */
{
	uint32_t status = midi_hw_read16(**handle, 0x34);

	if ((status & 1) == 0)
		return false;
	if (mode != 2)
		return false;
	return *((char *)(*usbdc_ep0_ctx_handle_link) + 0x171) == 1;
}
