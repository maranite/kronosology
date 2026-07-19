/* SPDX-License-Identifier: GPL-2.0 */
/*
 * panelbus_table_byte.c - reconstructs a single genuinely-uncovered
 * function, FUN_c0009204 (0xc0009204, 20 bytes), that BOTH panelbus_
 * dispatch.c and omap_l137_usbdc_ep0.c independently cite by name and
 * exact signature as an extern - each one explicitly disclaiming
 * ownership ("generic, multi-caller, not this file's scope" /
 * "generic table byte read") rather than supplying a body. Confirmed via
 * a fresh grep of every *.c file in this project: `panelbus_table_byte`
 * appears only as an `extern` declaration in both places, never as a
 * definition - a real gap, not a mis-tracked citation.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18 pass), read via query_dump.py. No live Ghidra
 * MCP calls this pass (bridge flagged concurrency-unsafe under this
 * project's own parallel-agent constraint).
 *
 * ANCHOR: NONE - see panelbus_dispatch.c's own file header for why (all 14
 * real "../<Name>.cpp" strings in this image are already claimed
 * elsewhere). Named/placed as its own tiny file rather than folded into
 * either citing file, per this task's own "do not edit existing files"
 * constraint.
 *
 * REAL SIGNATURE AND BODY (per direct Ghidra decompile of FUN_c0009204):
 *
 *   undefined1 FUN_c0009204(int param_1,int param_2)
 *   {
 *     undefined1 uVar1;
 *     uVar1 = 0;
 *     if (param_2 < 0x4f) {
 *       uVar1 = *(undefined1 *)(param_2 + param_1 + 0x10);
 *     }
 *     return uVar1;
 *   }
 *
 * i.e. a bounds-checked (index < 0x4f == 79) byte read at base+index+0x10,
 * returning 0 out-of-bounds instead of faulting - exactly matching both
 * citing files' own description ("generic bounds-checked (<0x4f) byte
 * accessor"). 18 real callers firmware-wide per xrefs_to (most inside
 * FUN_c0009218/_94d8/_9480, all clustered in the same 0xc0009204-
 * 0xc0009474 neighborhood as omap_l137_usbdc_ep0.c's own assigned sweep -
 * consistent with that file's framing of this as a shared primitive
 * rather than something owned by any one caller). Both `panelbus_
 * hw_negotiate_ready` (panelbus_dispatch.c) and `cad_pedal_present`
 * (omap_l137_usbdc_ep0.c) call it via `base + 0x1ac00` - the fixed +0x10
 * table offset this body applies lands those two real call sites on
 * table+0x1ac10, consistent with both files' own commentary.
 *
 * @0xc0009204.
 */

#include <stdint.h>

uint8_t panelbus_table_byte(uint32_t base, int index)	/* FUN_c0009204 */
{
	if (index < 0x4f)
		return *(uint8_t *)(base + (uint32_t)index + 0x10);
	return 0;
}
