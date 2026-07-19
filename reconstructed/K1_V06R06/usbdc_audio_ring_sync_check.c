/* SPDX-License-Identifier: GPL-2.0 */
/*
 * usbdc_audio_ring_sync_check.c - reconstructs FUN_c000f174 (0xc000f174,
 * 184 bytes), the single function this task assigned as "0xc000f174 (1
 * fn)". Confirmed genuinely uncovered: grepped "c000f174" across every
 * *.c file in this project first - zero hits.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18 pass), read via query_dump.py. No live Ghidra
 * MCP calls this pass (bridge flagged concurrency-unsafe under this
 * project's own parallel-agent constraint).
 *
 * ANCHOR: NONE - lands in the same "0xc000e000-0xc000f800 out-of-range
 * caller cluster" soc_irq_gate.c's own CLUSTER 11 section already
 * describes as "none reconstructed by any file so far".
 *
 * WHAT IT DOES AND WHY, worked out from its real callees (all cross-file
 * confirmed, none redefined here):
 *   - FUN_c0005500/FUN_c0005510 = irq_save_and_disable/irq_restore
 *     (crypto_at88.c / eva_crt0_tick_glue.c - already reconstructed).
 *     Both used here as plain interrupt-disable/restore brackets around
 *     two single-word reads - no shared state protected beyond the reads
 *     themselves.
 *   - FUN_c0000484 = soc_irq_gate_usbdc_ring_index_update(char commit)
 *     (soc_irq_gate.c, CLUSTER 11 - already reconstructed). Called here
 *     with commit=0, i.e. per that function's own doc, "just returns the
 *     clamped-to-zero current index" without recomputing anything.
 *   - FUN_c0000570 = soc_irq_gate_slot0x1c_get(void) (soc_irq_gate.c -
 *     already reconstructed, itself one of this task's own "already
 *     covered" clusters). Reads table+0x1c (0xC00E001C).
 *   - FUN_c000efa8/FUN_c000eff0 - cited (not defined) by cdix_autoswitch.c
 *     as "two out-of-range helpers" it traced far enough to confirm are
 *     real CDIX-format-adjacent resync/recompute steps, but explicitly
 *     left unreconstructed there (outside that file's own 0xc000fa90-
 *     0xc000fb90 sweep). NOT reconstructed here either (outside this
 *     task's own assigned span) - declared as bare externs with the
 *     signature their own direct decompile shows.
 *
 * Reads two ring-position values off its own `state` argument (a ushort
 * word at offset 0x00, and a ushort word at offset 0x1a / index 0xd),
 * compares each against a live index scaled by 6 (soc_irq_gate_usbdc_
 * ring_index_update(0)*6 for the first, soc_irq_gate_slot0x1c_get()*6 for
 * the second) with a 192 (0xc0 = 32*6, the SAME 32-slot ring soc_irq_
 * gate.c's own CLUSTER 11 doc already establishes) wraparound correction
 * on each side, then checks whether BOTH resulting distances fall in the
 * narrow window [7, 41) (`(distance - 7) < 0x23` as an unsigned
 * underflow-safe range test - true only for distance in [7, 40]). If
 * either distance is outside that tolerance window, calls the two
 * out-of-range resync helpers; if both are within tolerance, returns
 * immediately, doing nothing. In short: a per-tick "has this ring's
 * shadow index drifted too far from its live counterpart" tolerance
 * check, resyncing only when needed - not itself independently confirmed
 * against real hardware timing, but the shape (32-slot ring wraparound,
 * live-index comparison, conditional resync) is consistent with every
 * other USB/audio ring-tracking function this project has already
 * documented (soc_irq_gate.c's own CLUSTER 11, usbdc_ep_notify_ring.c's
 * notify ring).
 *
 * SIGNED-COMPARE NOTE (worked out explicitly, easy to get backwards): the
 * raw decompile expresses the second distance test via Ghidra's
 * `SBORROW4`-based idiom, `(int)(a - b) < 0 == SBORROW4(a, b)`. The
 * canonical flag identity is SF != OF <=> signed a < b, i.e. this
 * expression's "==" form (SF == OF) is the NEGATION, signed a >= b.
 * Combined with the preceding `a != b` guard, the real condition is
 * `a > b` (strict, signed) - i.e. wrap `scaled_b` forward by one ring
 * cycle only when the raw shadow value has ALREADY moved past the
 * unwrapped live-index*6 value. (An earlier draft of this file
 * transcribed this as `a < b`, the wrong direction - corrected here after
 * re-deriving the SF/OF identity by hand rather than guessing.) The FIRST
 * distance test instead uses a plain UNSIGNED `<=` comparison in the raw
 * decompile - a real asymmetry between the two branches, not smoothed
 * over.
 *
 * Sole caller (per xrefs_to): FUN_c0008b64 (master_dispatch_tick,
 * wire_dispatch.c - already-covered caller), call site 0xc0008f8c -
 * i.e. this really is a per-tick poll, consistent with the read above.
 *
 * @0xc000f174 (184 bytes).
 */

#include <stdint.h>

extern int32_t  irq_save_and_disable(void);				/* FUN_c0005500, crypto_at88.c */
extern void     irq_restore(int flags);				/* FUN_c0005510, eva_crt0_tick_glue.c */
extern int32_t  soc_irq_gate_usbdc_ring_index_update(char commit);	/* FUN_c0000484, soc_irq_gate.c */
extern uint32_t soc_irq_gate_slot0x1c_get(void);			/* FUN_c0000570, soc_irq_gate.c */
/* FUN_c000efa8/FUN_c000eff0 - cited by cdix_autoswitch.c, not defined
 * there or here (out of this task's own assigned span). Signature per
 * their own direct decompile; the real call sites here pass only the
 * first argument, `param_2`/`unused_flag` is a phantom 2nd argument not
 * set by this function's own call sites (same idiom as this project's
 * other phantom-parameter cases) - passed as 0, not confirmed. */
extern void FUN_c000efa8(uint16_t *state, uint8_t unused_flag);
extern void FUN_c000eff0(void *state);

/* usbdc_audio_ring_sync_check - `state` is a ushort-indexed structure;
 * state[0] and state[0xd] (byte offset 0x1a) are the two ring-position
 * shadow values checked against the live indices. */
void usbdc_audio_ring_sync_check(uint16_t *state)	/* FUN_c000f174 */
{
	int32_t flags;
	uint32_t a, b;
	uint32_t idx_a, idx_b;
	uint32_t scaled_a, scaled_b;

	flags = irq_save_and_disable();
	a = state[0];
	idx_a = (uint32_t)soc_irq_gate_usbdc_ring_index_update(0);
	irq_restore(flags);

	flags = irq_save_and_disable();
	b = state[0xd];
	idx_b = soc_irq_gate_slot0x1c_get();
	irq_restore(flags);

	scaled_a = idx_a * 6u;
	if (scaled_a != a && a <= scaled_a)		/* real asymmetric unsigned test, not simplified */
		a += 0xc0;

	scaled_b = idx_b * 6u;
	if (b != scaled_b && (int32_t)b > (int32_t)scaled_b)	/* signed a > b, see header note */
		scaled_b += 0xc0;

	if ((uint32_t)(scaled_b - b - 7u) < 0x23u &&
	    (uint32_t)(a - scaled_a - 7u) < 0x23u)
		return;		/* both ring positions within tolerance */

	FUN_c000efa8(state, 0);
	FUN_c000eff0(state);
}
