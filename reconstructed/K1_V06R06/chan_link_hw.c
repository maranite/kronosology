/* SPDX-License-Identifier: GPL-2.0 */
/*
 * chan_link_hw.c - reconstructs the assigned address range
 * 0xc000b414-0xc000b898 (17 real Ghidra function objects; the task's own
 * range end 0xc000b8a0 is the START of the next, much larger, unassigned
 * function FUN_c000b8a0 - not part of this range, not touched here).
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json), 2026-07-18 pass. No live Ghidra MCP calls this pass (the
 * bridge is flagged concurrency-unsafe under this project's own parallel
 * work).
 *
 * ANCHOR: NONE. All 14 real "../<name>.cpp" __FILE__ strings in this image
 * are already claimed by other subsystems (see README's Subsystems table).
 * None of the 17 functions here call any assert/fault-style function with a
 * `file` argument at all - every error/skip path is a plain early `return`,
 * the same structural signature chan_param_ctrl.c already documented for
 * its own immediately-following address range. File named descriptively,
 * per this project's established fallback for exactly this situation
 * (compare panelbus_dispatch.c/wire_dispatch.c/heap_alloc.c).
 *
 * WHY THIS FILE EXISTS AS ITS OWN UNIT, not folded into chan_param_ctrl.c
 * or midi_engine.c: it sits in the address gap directly BELOW
 * chan_param_ctrl.c's own assigned range (0xc000d9bc-0xc000e498) and
 * BELOW midi_engine.c's own assigned range (0xc000ca50-0xc000d6fc), and is
 * the concrete missing definition site for FIVE functions those two files
 * already forward-declared `extern` and cited as "out of range" while
 * reconstructing their own callers - reproduced verbatim here so the
 * signatures line up exactly:
 *
 *   chan_param_ctrl.c line 131: extern int  chan_link_probe_ready(uint32_t target);            -> FUN_c000b644
 *   chan_param_ctrl.c line 490: extern uint32_t chan_class2_read_value(uint32_t target, uint8_t idx); -> FUN_c000b68c
 *   chan_param_ctrl.c line 491: extern void chan_slot_apply_code(uint32_t target, uint8_t code);      -> FUN_c000b64c
 *   chan_param_ctrl.c line 492: extern void chan_slot_echo_code(uint32_t target, uint8_t code);       -> FUN_c000b66c
 *   midi_engine.c    line 552: extern void midi_hw_set_reg_d8(uint32_t *handle, uint8_t val);         -> FUN_c000b870
 *
 * NOT edited into either of those files this pass (out of this file's own
 * scope to modify another file) - cited here only, per this project's
 * standing cross-file-finding convention.
 *
 * CROSS-FILE FINDING #1 (struct-field identity): chan_link_probe_ready
 * (FUN_c000b644) reads byte `target+0x69` - the EXACT SAME field
 * chan_link_hw_service (FUN_c000b6c4, below) WRITES after testing hardware
 * link-detect status. This is concrete, address-verified confirmation that
 * chan_param_ctrl.c Part A's opaque "link" object (offsets documented
 * there as 0x00/0x08/0x10/0x128.../0x548/0x54c) and this file's hardware
 * bring-up routine operate on the SAME struct instance - chan_link_hw.c is
 * that struct's missing hardware-facing half.
 *
 * CROSS-FILE FINDING #2 (table identity, exact): FUN_c000b4d8's own table
 * pointer DAT_c000b57c resolves (all_data.json) to 0xc001f690 - the EXACT
 * SAME address chan_param_ctrl.c independently derived and named
 * `chan_bitmask_table_base` (DAT_c000ded4/DAT_c000df54, "8 bytes per
 * channel-index entry: {lo_table, hi_table}, each a pointer to a table of
 * uint16_t bitmask words indexed by chan_class_wire_code()'s own 1-6
 * output"). chan_bitmask_decode (FUN_c000b4d8, below) is the real CONSUMER
 * of that same table this project had not yet located: for a given
 * channel-index byte and hi/lo bank selector, it walks the bank's word
 * chain (continuation-bit-terminated, sentinel-skipped) and materializes
 * up to a handful of decoded (bank, wire_code) results into a shared
 * scratch pool (see POOL LAYOUT below) - i.e., this is the table-WALKING
 * counterpart to chan_class2_test_hi/_test_lo's table-TESTING logic in
 * chan_param_ctrl.c.
 *
 * chan_table2_decode (FUN_c000b588) is structurally IDENTICAL to
 * chan_bitmask_decode but sources from DAT_c000b62c, which resolves to
 * 0xc001f65c - 0x68 bytes before chan_index_table_base (0xc001f6c4), NOT
 * an exact match to any table chan_param_ctrl.c names. Genuinely
 * unresolved which real table this is; flagged, not guessed.
 *
 * CROSS-FILE FINDING #3 (register-family / instance-count corroboration):
 * chan_link_hw_service's bring-up path touches FIVE register pairs at a
 * fixed 0x20 stride (0x80/0xa0/0xc0/0xe0/0x100 and 0x96/0xb6/0xd6/0xf6/
 * 0x116) - i.e. real hardware evidence of exactly 5 physical port/channel
 * instances. This matches, independently, chan_slot_alloc_and_init's own
 * hard cap of 5 entries ("*pool < 5") in the decode-result pool below -
 * concrete, address-verified corroboration that both numbers describe the
 * same 5-instance hardware object, not a coincidence.
 *
 * CROSS-FILE FINDING #4 (adjacent global evidence, suggestive not proven):
 * chan_link_hw_service's "pending" flag pointer (DAT_c000b820) resolves to
 * 0xc01cd31c - 8 bytes before midi_engine.c's own independently-derived
 * `midi_cable_name_table` global (0xc01cd324, DAT_c000d1ac/DAT_c000d564).
 * Suggestive that this whole file's hardware layer and midi_engine.c's
 * ring-buffer state share one contiguous runtime data block, but NOT
 * confirmed by any direct struct-field walk - kept as an open observation.
 *
 * REGISTER FAMILY: every hardware access in this file goes through
 * midi_hw_write16/midi_hw_read16 (FUN_c0000c38/FUN_c0000c6c, out of this
 * file's range, already named and extern-declared by midi_engine.c) over
 * even-byte register offsets in the same 0x00-0x116 window midi_engine.c's
 * own header already attributes to "an external hardware bring-up/status
 * layer for what is almost certainly a discrete multi-port MIDI
 * UART/transceiver chip" - this file IS that layer.
 *
 * POOL LAYOUT (chan_bitmask_decode / chan_table2_decode / chan_slot_alloc_and_init
 * / chan_slot_scratch_alloc / chan_decode_result_get all operate on this
 * one shared global structure - address not extracted this pass, cited via
 * DAT_c000b578, which EQUALS DAT_c000b628 exactly, i.e. both decode
 * functions clear/allocate from the identical single-instance pool):
 *
 *   struct chan_decode_pool {
 *       uint32_t count;          // +0x000, gates chan_slot_alloc_and_init's 5-entry cap
 *       uint32_t reserved;       // +0x004, cleared, never read in this file
 *       uint8_t  scratch[2][0x200]; // +0x008..+0x407, two 512B buffers, doled out
 *                                   //   one-shot by chan_slot_scratch_alloc
 *       struct chan_decode_entry {  // +0x408, 5 * 0x1c = 0x8c bytes, entries[5]
 *           uint32_t bank;       // 0 or 1, the hi/lo bank selector
 *           uint32_t wire_code;  // decoded (word >> 6) value
 *           uint32_t unused_08;
 *           uint32_t unused_0c;
 *           uint32_t unused_10;
 *           uint32_t unused_14;
 *           void    *scratch_buf; // only non-NULL for bank==0 && wire_code!=0
 *       } entries[5];
 *   };
 *   Total size 0x408 + 5*0x1c = 0x494 bytes. Field layout confirmed by
 *   chan_slot_entry_init's own 7-word store pattern; unused_08.._14 never
 *   written by anything in this file - plausibly filled in by an
 *   out-of-range consumer (FUN_c000c168/_c22c/_c2b8, the same "slot
 *   dispatch cluster" chan_param_ctrl.c's own PART B note already flags as
 *   unassigned/unclaimed 0xc000bxxx-0xc000cxxx code).
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Out-of-scope dependencies - generic, firmware-wide primitives this file
 *  calls into but does not itself define.
 * ------------------------------------------------------------------------- */
extern void     midi_hw_write16(uint32_t base_sel, unsigned reg_off, uint16_t val);	/* FUN_c0000c38, out of range - see midi_engine.c */
extern uint16_t midi_hw_read16(uint32_t base_sel, unsigned reg_off);			/* FUN_c0000c6c, out of range - see midi_engine.c */
extern void     hw_timer_busy_wait(void *timer_base, int units);			/* FUN_c0001aa0, out of range - see i2c_by_gpio.c */

/* ------------------------------------------------------------------------- *
 *  Shared decode-result pool primitives. Address of the pool itself not
 *  extracted this pass (DAT_c000b578 == DAT_c000b628, single instance) -
 *  modeled as an opaque `uint32_t *pool` handle throughout, matching this
 *  project's convention for un-extracted global addresses elsewhere
 *  (compare cobjectmgr.c's own free-list globals).
 * ------------------------------------------------------------------------- */

/* chan_slot_scratch_alloc - FUN_c000b448, @0xc000b448 (36 bytes).
 * Doles out one of exactly 2 fixed 512-byte scratch buffers living at
 * pool+0x008/+0x208, tracked by a one-shot counter at pool+0x004 (NOT
 * pool+0x000 - the decompile's own `param_1 + 4` byte offset lands on the
 * pool's SECOND word, confirmed distinct from the `count` word
 * chan_slot_alloc_and_init tests/increments at pool+0x000). Returns NULL
 * (0) once both buffers are handed out. Sole caller: chan_slot_alloc_and_init,
 * below, only when bank==0. */
static void *chan_slot_scratch_alloc(uint32_t *pool)	/* FUN_c000b448 */
{
	uint8_t *base = (uint8_t *)pool;
	int32_t slot = *(int32_t *)(base + 4);

	if (slot >= 2)
		return (void *)0;

	*(int32_t *)(base + 4) = slot + 1;
	return base + slot * 0x200 + 8;
}

/* chan_slot_entry_init - FUN_c000b414, @0xc000b414 (52 bytes).
 * Initializes one 7-word chan_decode_entry: `buf` is kept ONLY when
 * bank==0 AND wire_code!=0 (double-guarded - the same gate is also applied
 * by chan_slot_alloc_and_init's own caller-side `if (bank == 0)` before it
 * even calls chan_slot_scratch_alloc, so this is a real, confirmed
 * belt-and-suspenders check in the original code, not simplified away). */
static void chan_slot_entry_init(uint32_t *entry, uint32_t bank, uint32_t wire_code, void *buf)	/* FUN_c000b414 */
{
	int keep_buf = (bank <= 1) ? (int)(1 - bank) : 0;

	if (wire_code == 0)
		keep_buf = 0;
	if (keep_buf == 0)
		buf = (void *)0;

	entry[6] = (uint32_t)(uintptr_t)buf;
	entry[5] = 0;
	entry[0] = bank;
	entry[1] = wire_code;
	entry[2] = 0;
	entry[3] = 0;
	entry[4] = 0;
}

/* chan_slot_alloc_and_init - FUN_c000b46c, @0xc000b46c (108 bytes).
 * Allocates the next free chan_decode_entry (cap 5, pool+0x000 as the live
 * count) at pool+0x408+entry*0x1c, grabs a scratch buffer for bank==0
 * only, and initializes it via chan_slot_entry_init. Returns NULL once 5
 * entries are already allocated. */
static uint32_t *chan_slot_alloc_and_init(uint32_t *pool, uint32_t bank, uint32_t wire_code)	/* FUN_c000b46c */
{
	uint32_t *entry = (uint32_t *)0;

	if (pool[0] < 5) {
		void *buf;

		entry = pool + pool[0] * 7 + 0x102;	/* element stride 7 ints, base offset 0x102 ints (0x408 bytes) */
		buf = (bank == 0) ? chan_slot_scratch_alloc(pool) : (void *)0;
		chan_slot_entry_init(entry, bank, wire_code, buf);
		pool[0] = pool[0] + 1;
	}
	return entry;
}

/* chan_bitmask_decode - FUN_c000b4d8, @0xc000b4d8 (160 bytes).
 * For channel-index byte `idx`, walks BOTH banks (0=lo, 1=hi) of
 * chan_bitmask_table_base[idx] (confirmed exact address match - see
 * CROSS-FILE FINDING #2 above), materializing decoded (bank, wire_code)
 * results as allocated pool entries whose POINTERS are written into the
 * caller-supplied 6-word `out` array at out[0..]. Each bank's word chain is
 * 4-byte-strided; a byte's low 3 bits (& 0xe == 0xe) mark a sentinel/skip
 * entry (not allocated); the chain's low bit (& 1) is the "last word"
 * terminator (do-while continues while this bit is 0). The decoded value
 * itself is the aligned uint16 at that same offset, shifted right 6. */
void chan_bitmask_decode(uint32_t *out, uint32_t idx)	/* FUN_c000b4d8 */
{
	extern uint32_t *chan_decode_pool;		/* DAT_c000b578, un-extracted single-instance global, see POOL LAYOUT above */
	extern uint8_t  *chan_bitmask_table_base;	/* DAT_c000b57c -> 0xc001f690, SAME global chan_param_ctrl.c already names chan_bitmask_table_base */
	uint32_t *pool = chan_decode_pool;
	uint32_t bank;

	pool[0] = 0;
	pool[1] = 0;
	for (bank = 0; bank < 6; bank++)
		out[bank] = 0;

	for (bank = 0; bank < 2; bank++) {
		uint8_t *chain = *(uint8_t **)(chan_bitmask_table_base + (bank + (idx & 0xff) * 2) * 4);
		int off = 4;

		for (;;) {
			if ((chain[off] & 0xe) != 0xe) {
				uint16_t word = *(uint16_t *)(chain + off);
				uint32_t *entry = chan_slot_alloc_and_init(pool, bank, word >> 6);

				out[off / 4 - 1] = (uint32_t)(uintptr_t)entry;
			}
			if (chain[off] & 1)
				break;
			off += 4;
		}
	}
}

/* chan_bitmask_decode_u8 - FUN_c000b580, @0xc000b580 (8 bytes).
 * Plain byte-width-truncating wrapper, sole caller FUN_c000bc1c (out of
 * range, unclaimed "slot dispatch" cluster). */
void chan_bitmask_decode_u8(uint32_t *out, uint8_t idx)	/* FUN_c000b580 */
{
	chan_bitmask_decode(out, idx);
}

/* chan_table2_decode - FUN_c000b588, @0xc000b588 (160 bytes).
 * Byte-for-byte structural twin of chan_bitmask_decode above, EXCEPT it
 * sources its per-bank word-chain pointer from DAT_c000b62c (resolves to
 * 0xc001f65c) instead of chan_bitmask_table_base (0xc001f690) - 0x68 bytes
 * apart, NOT a clean stride of chan_index_table_base's own 0x44-byte
 * entries either. Genuinely unresolved WHICH real table this is; the two
 * decode functions share the identical pool/output-array protocol
 * (confirmed: DAT_c000b628 == DAT_c000b578, the exact same pool). */
void chan_table2_decode(uint32_t *out, uint32_t idx)	/* FUN_c000b588 */
{
	extern uint32_t *chan_decode_pool;	/* DAT_c000b628, == chan_decode_pool above, same global */
	extern uint8_t  *chan_table2_base;	/* DAT_c000b62c -> 0xc001f65c, NOT chan_bitmask_table_base, NOT chan_index_table_base - open item */
	uint32_t *pool = chan_decode_pool;
	uint32_t bank;

	pool[0] = 0;
	pool[1] = 0;
	for (bank = 0; bank < 6; bank++)
		out[bank] = 0;

	for (bank = 0; bank < 2; bank++) {
		uint8_t *chain = *(uint8_t **)(chan_table2_base + (bank + (idx & 0xff) * 2) * 4);
		int off = 4;

		for (;;) {
			if ((chain[off] & 0xe) != 0xe) {
				uint16_t word = *(uint16_t *)(chain + off);
				uint32_t *entry = chan_slot_alloc_and_init(pool, bank, word >> 6);

				out[off / 4 - 1] = (uint32_t)(uintptr_t)entry;
			}
			if (chain[off] & 1)
				break;
			off += 4;
		}
	}
}

/* chan_decode_result_get - FUN_c000b630, @0xc000b630 (20 bytes).
 * Reads back slot `idx` from a caller-held 6-word decode-result array (the
 * SAME `out` array chan_bitmask_decode/chan_table2_decode populate above -
 * `arr` here is passed already offset +4 by every real caller, matching
 * those functions' own `param_1 + iVar1 - 4` store addressing). Callers:
 * FUN_c000c168/_c22c/_c2b8, all out of range (unclaimed slot-dispatch
 * cluster). */
uint32_t chan_decode_result_get(uint32_t *arr, uint32_t idx)	/* FUN_c000b630 */
{
	return arr[(idx & 0xff) - 1];
}

/* ------------------------------------------------------------------------- *
 *  Per-link hardware register access. `target`/`link` here is confirmed
 *  (chan_param_ctrl.c Part A) an opaque handle whose first word (*target)
 *  is the real midi_hw_write16/read16 `base_sel` argument.
 * ------------------------------------------------------------------------- */

/* chan_link_probe_ready - FUN_c000b644, @0xc000b644 (8 bytes).
 * Reads back the link-up flag byte at target+0x69 - written by
 * chan_link_hw_service below (CROSS-FILE FINDING #1). Signature MUST match
 * chan_param_ctrl.c's own forward declaration exactly (see file header). */
int chan_link_probe_ready(uint32_t target)	/* FUN_c000b644 */
{
	return *(uint8_t *)(target + 0x69);
}

/* chan_slot_apply_code - FUN_c000b64c, @0xc000b64c (28 bytes).
 * Writes value 4 to register ((code << 5) & DAT_c000b668) + 0x68 on the
 * link's own hw base (*target). DAT_c000b668 = 0x1fe0, i.e. code is
 * effectively masked to bits [8:5] before becoming a register offset -
 * consistent with the 5-port, 0x20-stride register family
 * chan_link_hw_service's bring-up path establishes (CROSS-FILE FINDING
 * #3). Signature MUST match chan_param_ctrl.c's own forward declaration. */
void chan_slot_apply_code(uint32_t target, uint8_t code)	/* FUN_c000b64c */
{
	uint32_t base = *(uint32_t *)(uintptr_t)target;
	unsigned reg = ((uint32_t)code << 5 & 0x1fe0) + 0x68;

	midi_hw_write16(base, reg, 4);
}

/* chan_slot_echo_code - FUN_c000b66c, @0xc000b66c (28 bytes).
 * Identical register addressing to chan_slot_apply_code above, but writes
 * value 8 instead of 4 - a real, confirmed bit-flag distinction (apply vs.
 * echo/readback-request) at the SAME register, not a duplicate. Signature
 * MUST match chan_param_ctrl.c's own forward declaration. */
void chan_slot_echo_code(uint32_t target, uint8_t code)	/* FUN_c000b66c */
{
	uint32_t base = *(uint32_t *)(uintptr_t)target;
	unsigned reg = ((uint32_t)code << 5 & 0x1fe0) + 0x68;

	midi_hw_write16(base, reg, 8);
}

/* chan_class2_read_value - FUN_c000b68c, @0xc000b68c (52 bytes).
 *
 * CROSS-FILE MISMATCH, flagged not silently reconciled: chan_param_ctrl.c
 * forward-declares this as `uint32_t chan_class2_read_value(uint32_t
 * target, uint8_t idx)` and uses its result as a general 0-0xffff VALUE
 * (masked `& 0xffff`, compared against a 0xffff "no change" sentinel). The
 * REAL decompiled body is a pure bit-3 presence TEST on register
 * ((idx<<5)&0x1fe0)+0x74, returning only 0 or 1 - it cannot itself produce
 * the 0xffff sentinel or any value other than 0/1. Implemented here
 * exactly as decompiled (faithful over convenient); chan_param_ctrl.c's
 * own consumer of this return value should be treated as suspect until
 * independently re-checked - not corrected here (out of this file's scope
 * to edit that file). Signature kept identical for link compatibility. */
uint32_t chan_class2_read_value(uint32_t target, uint8_t idx)	/* FUN_c000b68c */
{
	uint32_t base = *(uint32_t *)(uintptr_t)target;
	unsigned reg = ((uint32_t)idx << 5 & 0x1fe0) + 0x74;
	uint16_t val = midi_hw_read16(base, reg);

	return (val & 8) != 0;
}

/* chan_link_hw_base_get - FUN_c000b898, @0xc000b898 (12 bytes).
 * Trivial dereference: returns the link's own stored hw base (the same
 * first word every other function in this file dereferences directly as
 * `*target`). Sole caller: FUN_c000bc1c (out of range). */
uint32_t chan_link_hw_base_get(uint32_t *handle)	/* FUN_c000b898 */
{
	return handle[0];
}

/* ------------------------------------------------------------------------- *
 *  midi_hw_set_reg_XX family - plain fixed-value register pokes on the
 *  same 5-port, 0x20-stride register block chan_link_hw_service configures
 *  at bring-up. Naming follows midi_engine.c's own established convention
 *  for this exact pattern (midi_hw_set_reg_d8).
 * ------------------------------------------------------------------------- */

/* midi_hw_set_reg_60 - FUN_c000b840, @0xc000b840 (16 bytes).
 * Callers: FUN_c000e924 (chan_param_ctrl.c PART C's own out-of-range USB
 * EP0 SETUP-packet reader) and FUN_c000c39c (conditional call, unclaimed
 * slot-dispatch cluster). */
void midi_hw_set_reg_60(uint32_t *handle)	/* FUN_c000b840 */
{
	midi_hw_write16(handle[0], 0x60, 8);
}

/* midi_hw_set_reg_f6 - FUN_c000b850, @0xc000b850 (16 bytes).
 * Reg 0xf6 is the SAME register chan_link_hw_service's bring-up sequence
 * writes 0x11fd to (the 4th of the 5-port 0x96/0xb6/0xd6/0xf6/0x116
 * family, i.e. "port 3") - this function re-pokes it with value 1 at
 * runtime, confirmed called from master_dispatch_tick (wire_dispatch.c,
 * FUN_c0008b64, call site 0xc0008fe4, part of its own documented
 * "NOT transcribed" USB-adjacent cluster) and conditionally from
 * FUN_c000d6fc (out of range). */
void midi_hw_set_reg_f6(uint32_t *handle)	/* FUN_c000b850 */
{
	midi_hw_write16(handle[0], 0xf6, 1);
}

/* midi_hw_set_reg_e8 - FUN_c000b860, @0xc000b860 (16 bytes).
 * Sole caller: master_dispatch_tick (wire_dispatch.c, FUN_c0008b64, call
 * site 0xc0008fec, same undocumented USB-adjacent cluster as
 * midi_hw_set_reg_f6 above). */
void midi_hw_set_reg_e8(uint32_t *handle)	/* FUN_c000b860 */
{
	midi_hw_write16(handle[0], 0xe8, 0x80);
}

/* midi_hw_set_reg_d8 - FUN_c000b870, @0xc000b870 (32 bytes).
 * Writes 0x10 if `enable` is nonzero, else 0, to reg 0xd8 - the same
 * register midi_engine.c's midi_ring2_pop_copy (FUN_c000d0fc) calls this
 * exact function against, after popping a ring-2 entry, whenever
 * midi_ring2_headroom_ok reports "far from full". Signature MUST match
 * midi_engine.c's own forward declaration exactly (see file header).
 * Second caller: FUN_c000d0fc's own sibling at call site 0xc000d9a4
 * (out of range). */
void midi_hw_set_reg_d8(uint32_t *handle, uint8_t enable)	/* FUN_c000b870 */
{
	midi_hw_write16(handle[0], 0xd8, enable ? 0x10 : 0);
}

/* ------------------------------------------------------------------------- *
 *  chan_link_hw_service - FUN_c000b6c4, @0xc000b6c4 (348 bytes). The link
 *  bring-up/status-service routine: deferred work triggered by a pending
 *  flag (*DAT_c000b820), called once per master_dispatch_tick pass
 *  (wire_dispatch.c FUN_c0008b64, call site 0xc0009014, inside its own
 *  documented-but-not-transcribed USB-adjacent cluster).
 *
 *  Sequence:
 *   1. Bail immediately if nothing pending.
 *   2. Fixed ~100000-unit busy-wait (DAT_c000b828 = 0x186a0) via
 *      hw_timer_busy_wait(DAT_c000b824, ...) - DAT_c000b824 resolves to
 *      0xc00e0068, an opaque per-instance value whose exact role (timer
 *      channel selector vs. something else) is not decoded this pass.
 *   3. Strobe reg 0x36 = 0x80 (reset/latch pulse).
 *   4. Read reg 0x34 bit 0 -> link_up. Store into handle+0x69 (read back
 *      by chan_link_probe_ready, CROSS-FILE FINDING #1).
 *   5a. If link DOWN: run the full bring-up sequence - reg 0x72 =
 *       DAT_c000b82c (0x20ff); clear bit 0 (AND DAT_c000b830=0xfffe) on
 *       each of the 5 read-modify-write registers 0x80/0xa0/0xc0/0xe0/
 *       0x100; write DAT_c000b834 (0x11fd) to each of the 5 sibling
 *       registers 0x96/0xb6/0xd6/0xf6/DAT_c000b838(0x116) (CROSS-FILE
 *       FINDING #3: this 5-instance stride matches the decode pool's own
 *       5-entry cap exactly); then, only if reg 0x32 bit 2 is already set,
 *       clear bits 2/4/5 of it (AND DAT_c000b83c=0xffcb) and write back.
 *   5b. If link UP: read reg 0x32; if bit 2 already set, skip straight to
 *       step 6 (already configured); else OR in bit 2 (0x4) and write
 *       back.
 *   6. Clear the pending flag and return.
 * ------------------------------------------------------------------------- */
void chan_link_hw_service(uint32_t *handle)	/* FUN_c000b6c4 */
{
	extern uint32_t *chan_link_hw_pending;	/* DAT_c000b820 -> 0xc01cd31c, see CROSS-FILE FINDING #4 */
	extern void     *chan_link_hw_timer_ctx;	/* DAT_c000b824 -> 0xc00e0068, opaque, not decoded */
	uint32_t base = handle[0];
	int link_up;
	uint16_t status;

	if (*chan_link_hw_pending == 0)
		return;

	hw_timer_busy_wait(chan_link_hw_timer_ctx, 0x186a0);

	midi_hw_write16(base, 0x36, 0x80);
	link_up = midi_hw_read16(base, 0x34) & 1;
	*((uint8_t *)handle + 0x69) = (uint8_t)link_up;

	if (!link_up) {
		static const unsigned rmw_regs[5]  = { 0x80, 0xa0, 0xc0, 0xe0, 0x100 };
		static const unsigned fixed_regs[5] = { 0x96, 0xb6, 0xd6, 0xf6, 0x116 };
		int i;

		midi_hw_write16(base, 0x72, 0x20ff);

		for (i = 0; i < 5; i++) {
			uint16_t v = midi_hw_read16(base, rmw_regs[i]);

			midi_hw_write16(base, rmw_regs[i], v & 0xfffe);
		}

		for (i = 0; i < 5; i++)
			midi_hw_write16(base, fixed_regs[i], 0x11fd);

		status = midi_hw_read16(base, 0x32);
		if ((status & 4) == 0)
			goto done;
		status = status & 0xffff & 0xffcb;
	} else {
		status = midi_hw_read16(base, 0x32);
		if (status & 4)
			goto done;
		status = (status & 0xffff) | 4;
	}

	midi_hw_write16(base, 0x32, status);
done:
	*chan_link_hw_pending = 0;
}
