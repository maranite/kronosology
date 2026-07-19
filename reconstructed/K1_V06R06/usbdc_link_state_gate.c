/* SPDX-License-Identifier: GPL-2.0 */
/*
 * usbdc_link_state_gate.c - reconstructs the 3 real functions in this
 * task's assigned range "0xc000eba8-0xc000ec68 (2 fns)" (the task's own
 * count is one short - FUN_c000eba8, FUN_c000ec0c, AND FUN_c000ec68 are
 * all genuinely present and genuinely uncovered in this span; confirmed
 * by grepping each address across every *.c file in this project first).
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18 pass), read via query_dump.py. No live Ghidra
 * MCP calls this pass (bridge flagged concurrency-unsafe under this
 * project's own parallel-agent constraint).
 *
 * ANCHOR: NONE - same no-anchor situation as usbdc_ep_notify_ring.c/
 * chan_slot_dispatch.c.
 *
 * CROSS-FILE CONFIRMATION: usbdc_ep_notify_ring.c's own file header
 * already declares FUN_c000ec0c as a bare extern - `uint8_t FUN_c000ec0c
 * (void *link_state_obj, int selector)` - operating on its own named
 * singleton `USBDC_LINK_STATE_OBJ` (0xC01CC0F4), and calls it from two
 * already-reconstructed functions there (usbdc_ep_rate_update /
 * FUN_c0006790, mcasp_ep_rate_check / FUN_c00069c0) with selector values
 * 1 and 0. That file explicitly leaves the body "not attributed to any
 * file yet" - this file supplies it, reusing usbdc_ep_notify_ring.c's own
 * chosen name/signature exactly. chan_slot_dispatch.c similarly bare-
 * externs FUN_c000eba8 as `chan_port_ctx_notify_a(uint32_t ctx, void
 * *buf, uint32_t len)` - reused here too, though (as with this cluster's
 * sibling chan_port_ctx_notify_b in chan_desc_notify_misc.c) the real
 * body ignores every argument.
 *
 * =============================================================================
 * chan_port_ctx_notify_a - FUN_c000eba8 (8 bytes)
 * =============================================================================
 * Trivial stub: zero formal parameters in the real decompile, always
 * returns 0. Real body:
 *
 *   undefined4 FUN_c000eba8(void) { return 0; }
 *
 * chan_slot_dispatch.c's own citation declares a 3-arg signature (ctx,
 * buf, len) matching its real call site in chan_port_reg60_rx_pump
 * (`FUN_c000eba8(param_1[2], param_1 + 5, param_1[0x16] & 0xffff)`,
 * itself a CONDITIONAL_CALL per xrefs_to) - all three arguments are
 * phantom/unused, same idiom as this cluster's sibling chan_port_ctx_
 * notify_b. Kept as the 3-arg signature to match chan_slot_dispatch.c's
 * own extern exactly.
 *
 * Sole caller (per xrefs_to): FUN_c000bf54 (chan_port_reg60_rx_pump,
 * chan_slot_dispatch.c - already-covered caller), call site 0xc000c008.
 *
 * @0xc000eba8.
 *
 * =============================================================================
 * usbdc_link_state_gate / usbdc_link_state_gate_alt - FUN_c000ec0c (92
 * bytes) / FUN_c000ec68 (92 bytes)
 * =============================================================================
 * Both are "is this sub-object still pending, auto-expiring after a fixed
 * tick threshold" checks: for the selected sub-object (`selector == 0` vs
 * != 0 chooses between two field pairs at fixed offsets off the same
 * handle), if a "pending" flag byte is set AND a paired tick-count field
 * exceeds a fixed threshold, the flag is cleared; either way the function
 * returns the (possibly-just-cleared) flag XORed with 1 - i.e. "true"
 * means "idle / no longer pending", "false" means "still pending, not yet
 * expired". Real bodies:
 *
 *   byte FUN_c000ec0c(int param_1, char param_2)
 *   {
 *     if (param_2 == '\0') {
 *       if (*(char*)(param_1+0x14) != '\0' && 0x35 < *(ushort*)(param_1+0x10))
 *         *(char*)(param_1+0x14) = 0;
 *       return *(byte*)(param_1+0x14) ^ 1;
 *     }
 *     if (*(char*)(param_1+0x2c) != '\0' && 0x8f < *(ushort*)(param_1+0x28))
 *       *(char*)(param_1+0x2c) = 0;
 *     return *(byte*)(param_1+0x2c) ^ 1;
 *   }
 *
 *   byte FUN_c000ec68(int param_1, char param_2)
 *   {
 *     if (param_2 == '\0') {
 *       if (*(char*)(param_1+8) != '\0' && 0x17 < *(ushort*)(param_1+4))
 *         *(char*)(param_1+8) = 0;
 *       return *(byte*)(param_1+8) ^ 1;
 *     }
 *     if (*(char*)(param_1+0x20) != '\0' && 0x17 < *(ushort*)(param_1+0x1c))
 *       *(char*)(param_1+0x20) = 0;
 *     return *(byte*)(param_1+0x20) ^ 1;
 *   }
 *
 * Field offsets and thresholds DIFFER between the two functions (0x10/
 * 0x14 + threshold 0x35=53 and 0x28/0x2c + threshold 0x8f=143 for
 * usbdc_link_state_gate; 4/8 + threshold 0x17=23 and 0x1c/0x20 + the SAME
 * threshold 0x17=23 for usbdc_link_state_gate_alt) - genuinely two
 * separate field families on (per usbdc_ep_notify_ring.c's own citation
 * for FUN_c000ec0c) the SAME USBDC_LINK_STATE_OBJ singleton (0xC01CC0F4),
 * NOT independently confirmed whether usbdc_link_state_gate_alt's own
 * caller (FUN_c0006578, itself out of range per usbdc_ep_notify_ring.c's
 * own file header - "the gap between the two ranges... FUN_c0006578
 * itself remains out of range") passes that SAME object or a different
 * one - presented separately, per this project's "additive, not
 * force-fit" convention.
 *
 * Callers (per xrefs_to): usbdc_link_state_gate (FUN_c000ec0c) - 2, both
 * already-covered (FUN_c0006790/usbdc_ep_rate_update and FUN_c00069c0/
 * mcasp_ep_rate_check, both usbdc_ep_notify_ring.c). usbdc_link_state_
 * gate_alt (FUN_c000ec68) - 1, FUN_c0006578 (out of range, not
 * reconstructed by any file in this project so far).
 *
 * @0xc000ec0c / @0xc000ec68.
 */

#include <stdint.h>
#include <stdbool.h>

/* chan_port_ctx_notify_a - all 3 arguments are phantom/unused, see header
 * note (matches chan_slot_dispatch.c's own already-established extern). */
uint32_t chan_port_ctx_notify_a(uint32_t ctx, void *buf, uint32_t len)	/* FUN_c000eba8 */
{
	(void)ctx;
	(void)buf;
	(void)len;
	return 0;
}

/* usbdc_link_state_gate - operates on USBDC_LINK_STATE_OBJ (0xC01CC0F4,
 * per usbdc_ep_notify_ring.c); selector 0 -> fields +0x10/+0x14, selector
 * != 0 -> fields +0x28/+0x2c. */
uint8_t usbdc_link_state_gate(void *link_state_obj, int selector)	/* FUN_c000ec0c */
{
	uint8_t *base = (uint8_t *)link_state_obj;

	if (selector == 0) {
		if (base[0x14] != 0 && *(uint16_t *)(base + 0x10) > 0x35)
			base[0x14] = 0;
		return base[0x14] ^ 1;
	}

	if (base[0x2c] != 0 && *(uint16_t *)(base + 0x28) > 0x8f)
		base[0x2c] = 0;
	return base[0x2c] ^ 1;
}

/* usbdc_link_state_gate_alt - same shape as usbdc_link_state_gate, on a
 * different (not confirmed identical) handle/field family: selector 0 ->
 * fields +0x04/+0x08, selector != 0 -> fields +0x1c/+0x20, both gated by
 * the same threshold 0x17 (23). See header note on the sole caller,
 * FUN_c0006578, remaining out of range for this project. */
uint8_t usbdc_link_state_gate_alt(void *handle, int selector)	/* FUN_c000ec68 */
{
	uint8_t *base = (uint8_t *)handle;

	if (selector == 0) {
		if (base[8] != 0 && *(uint16_t *)(base + 4) > 0x17)
			base[8] = 0;
		return base[8] ^ 1;
	}

	if (base[0x20] != 0 && *(uint16_t *)(base + 0x1c) > 0x17)
		base[0x20] = 0;
	return base[0x20] ^ 1;
}
