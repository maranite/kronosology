/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_port_cmd_stage.c - reconstructs the 2 functions this task assigned
 * as "0xc000f5c8-0xc000f600 (2 fns)": FUN_c000f5c8 and FUN_c000f600.
 * Confirmed genuinely uncovered: grepped both addresses across every *.c
 * file in this project first - zero hits.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18 pass), read via query_dump.py. No live Ghidra
 * MCP calls this pass (bridge flagged concurrency-unsafe under this
 * project's own parallel-agent constraint).
 *
 * ANCHOR: NONE - both functions' sole callers are, respectively,
 * FUN_c000c39c (the "port interrupt dispatcher" chan_slot_dispatch.c's
 * own file header identifies as a 1836-byte function deliberately out of
 * range for every file so far) and FUN_c000f69c (an out-of-range extern
 * usbdc_ep_notify_ring.c already cites, `int FUN_c000f69c(void
 * *link_state_obj, uint32_t param2, uint32_t code)`, itself "not
 * attributed to any file yet"). Placed here rather than folded into
 * either citing file, per this task's own "do not edit existing files"
 * constraint.
 *
 * =============================================================================
 * chan_port_cmd_stage - FUN_c000f5c8 (40 bytes)
 * =============================================================================
 * Stages a deferred 3-field command into 4 fixed globals and sets a
 * "pending" flag - real body:
 *
 *   void FUN_c000f5c8(undefined4 param_1, undefined4 param_2,
 *                      undefined4 param_3, undefined1 param_4)
 *   {
 *     *DAT_c000f5f0 = param_2;	// 0xC01CD4DC
 *     *DAT_c000f5f4 = param_3;	// 0xC01CD4D8
 *     *DAT_c000f5f8 = param_4;	// 0xC01CD4D5
 *     *DAT_c000f5fc = 1;		// 0xC01CD4E0
 *     return;
 *   }
 *
 * `param_1` is a genuinely PHANTOM first argument - never read anywhere
 * in the real body (same idiom as several already-documented cases
 * elsewhere in this project). The 3 staged fields have no further
 * consumer traced this pass (whatever reads DAT_c000f5f0/_f5f4/_f5f8/
 * gated by DAT_c000f5fc was not located in this project's data yet) - a
 * genuinely open item, not fabricated.
 *
 * Sole caller (per xrefs_to): FUN_c000c39c at call site 0xc0006f6c.
 *
 * @0xc000f5c8.
 *
 * =============================================================================
 * chan_port_ring768_append - FUN_c000f600 (152 bytes)
 * =============================================================================
 * Appends packed 3-byte little-endian values, two per 6-byte input record,
 * into a 128-entry (0-0x7f), 8-byte-stride ring table (base DAT_c000f698,
 * resolved 0xC01CD4C4) at the ring index held in the state object's own
 * +0x24 field, incrementing a companion counter at +0x28 each iteration,
 * and wrapping the +0x24 index modulo 0x300 (768 = 128 entries * 6 - the
 * SAME "*6" scale this cluster's sibling usbdc_audio_ring_sync_check.c
 * uses for its own two ring indices, a real, address-independent
 * corroboration that this project's several *6-scaled ring counters share
 * one underlying convention). Loops while more than 5 input bytes remain
 * (param_3 > 5, consuming 6 bytes and decrementing param_3 by 6 each
 * pass); if the caller-supplied `param_4` flag is set, ALSO marks a
 * separate "dirty"/"changed" byte at +0x2c before the loop, unconditionally.
 *
 * Real body (transcribed, with the index-wrap arithmetic simplified from
 * its raw double-shift/zero-select form to the equivalent, much more
 * readable modulo-0x300 increment - verified equivalent by hand-tracing
 * both branches of the raw decompile: when the incremented index is
 * < 0x300 the raw code does a `<<0x10` immediately undone by a matching
 * `>>0x10`, a no-op; when it reaches exactly 0x300 the raw code discards
 * the shift result entirely and force-zeroes the stored value instead -
 * net effect in both cases is exactly `(old + 1) % 0x300`):
 *
 *   undefined4 FUN_c000f600(int param_1, byte *param_2, int param_3, char param_4)
 *   {
 *     if (param_4 != '\0') *(byte *)(param_1 + 0x2c) = 1;
 *     while (param_3 > 5) {   // see SIGNED-COMPARE NOTE below
 *       ushort idx = *(ushort *)(param_1 + 0x24);
 *       int *table = *DAT_c000f698;
 *       for (i = 0; i < 2; i++, param_2 += 3)
 *         table[idx*2 + i] = param_2[0] + param_2[1]*0x100 + param_2[2]*0x10000;
 *       *(ushort *)(param_1 + 0x24) = (idx + 1) % 0x300;
 *       *(short *)(param_1 + 0x28) += 1;
 *       param_3 -= 6;
 *     }
 *     return 0;
 *   }
 *
 * SIGNED-COMPARE NOTE: the raw loop condition is expressed via Ghidra's
 * SBORROW4 idiom on `(param_3 - 5)`, which per the SF!=OF signed-less-
 * than identity (see usbdc_audio_ring_sync_check.c's own note for the
 * full derivation) resolves to `param_3 >= 5`; combined with the
 * accompanying `param_3 != 5` guard, the real condition is `param_3 > 5`
 * - transcribed directly as that simplified, equivalent form.
 *
 * Sole caller (per xrefs_to): FUN_c000f69c at call site 0xc000f754 -
 * FUN_c000f69c is usbdc_ep_notify_ring.c's own cited (not defined)
 * `int FUN_c000f69c(void *link_state_obj, uint32_t param2, uint32_t
 * code)`; NOT independently confirmed here whether this function's own
 * `param_1` state object is the SAME USBDC_LINK_STATE_OBJ singleton that
 * file documents, or a distinct object merely reached through the same
 * caller - presented without asserting identity, per this project's
 * "additive, not force-fit" convention.
 *
 * @0xc000f600 (152 bytes).
 */

#include <stdint.h>

/* chan_port_cmd_stage_a/_b/_c/_pending - DAT_c000f5f0/_f5f4/_f5f8/_f5fc,
 * resolved 0xC01CD4DC/0xC01CD4D8/0xC01CD4D5/0xC01CD4E0. No confirmed
 * consumer traced this pass - see header note. */
extern uint32_t *chan_port_cmd_stage_a;	/* DAT_c000f5f0 -> 0xC01CD4DC */
extern uint32_t *chan_port_cmd_stage_b;	/* DAT_c000f5f4 -> 0xC01CD4D8 */
extern uint8_t  *chan_port_cmd_stage_c;	/* DAT_c000f5f8 -> 0xC01CD4D5 */
extern uint32_t *chan_port_cmd_pending;	/* DAT_c000f5fc -> 0xC01CD4E0 */

/* chan_port_cmd_stage - `unused` is a genuinely phantom first argument,
 * never read by the real body - see header note. */
void chan_port_cmd_stage(uint32_t unused, uint32_t val_b, uint32_t val_c, uint8_t val_d)	/* FUN_c000f5c8 */
{
	(void)unused;
	*chan_port_cmd_stage_a = val_b;
	*chan_port_cmd_stage_b = val_c;
	*chan_port_cmd_stage_c = val_d;
	*chan_port_cmd_pending = 1;
}

/* chan_port_ring768_table - DAT_c000f698, resolved 0xC01CD4C4: a pointer
 * TO the real 128-entry (8 bytes each = two packed 24-bit values) ring
 * buffer, not the buffer itself (real code does `*DAT_c000f698` to fetch
 * the base each iteration). */
extern uint32_t **chan_port_ring768_table;	/* DAT_c000f698 -> 0xC01CD4C4 */

/* chan_port_ring768_append - state+0x24 = ring write index (0-0x2ff,
 * i.e. 0-767, in raw units - divide by 6 for the entry index, matching
 * usbdc_audio_ring_sync_check.c's own *6-scaled sibling indices);
 * state+0x28 = running append counter; state+0x2c = "dirty" flag, set
 * whenever `mark_dirty` is true, independent of the loop below. */
uint32_t chan_port_ring768_append(void *state, const uint8_t *src, int len, uint8_t mark_dirty)	/* FUN_c000f600 */
{
	uint8_t *S = (uint8_t *)state;

	if (mark_dirty)
		S[0x2c] = 1;

	while (len > 5) {	/* real condition len>5, see SIGNED-COMPARE NOTE above */
		uint16_t idx = *(uint16_t *)(S + 0x24);
		uint32_t *table = *chan_port_ring768_table;
		int i;

		for (i = 0; i < 2; i++, src += 3) {
			table[(uint32_t)idx * 2 + (uint32_t)i] =
				(uint32_t)src[0] + (uint32_t)src[1] * 0x100u + (uint32_t)src[2] * 0x10000u;
		}

		*(uint16_t *)(S + 0x24) = (uint16_t)((idx + 1) % 0x300);
		*(int16_t *)(S + 0x28) += 1;
		len -= 6;
	}

	return 0;
}
