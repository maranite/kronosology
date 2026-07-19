/* SPDX-License-Identifier: GPL-2.0 */
/*
 * switch_chattering_detector.c - KRONOS2S_V01R10.VSB's
 * "../SwitchOnChatteringDetector.cpp".
 *
 * NEW architecture, no K1 equivalent. The name is self-explanatory: a
 * dedicated switch-debounce ("chattering") tracker, split out into its own
 * translation unit rather than being folded into the scan-chip driver the
 * way K1's cpsoc.cpp mixed everything together. This is the strongest
 * "genuinely redesigned, not just renamed" signal among the 5 new files -
 * K1 never had a standalone debounce module.
 *
 * ANCHOR: "../SwitchOnChatteringDetector.cpp" @ 0xc002b1f4. UNLIKE every
 * other one of the 5 new files, this one has NO string literals of its
 * own between its anchor and the next file's anchor (EvaBoardMain.cpp @
 * 0xc002b218, only 36 bytes later - exactly this file's own anchor length
 * plus alignment padding, confirmed via the K1-established convention
 * that a file's own extra string literals live in the gap immediately
 * after its anchor - see clcdc.c/cpsoc.c's own writeups for the same
 * pattern). A small, focused logic-only module needing no debug/UI text
 * is a functionally sensible match for a pure debounce state machine.
 *
 * METHODOLOGY NOTE: see panel_manager.c's file header - originally
 * recovered via manual capstone disassembly of the raw wrapped ELF, not a
 * Ghidra decompile (the pre-fetched 581-function static dump doesn't
 * cover this address range). Found via raw byte-pattern search for the
 * anchor string address itself (2 real hits, both are actual assert call
 * sites, not a Ghidra constant-propagation false negative this time - see
 * below).
 *
 * 2026-07-19 UPDATE - live Ghidra MCP follow-up (read-only:
 * get_function_info, decompile_function, get_xrefs_to only, no mutating
 * calls). The live Ghidra database has real Function objects and real
 * decompiles for BOTH functions below (unlike the pre-fetched static
 * dump) - the hand-transcriptions turned out to be byte-for-byte accurate
 * against the live decompile, and the register()/remove() indexing
 * discrepancy is now CONFIRMED REAL (not a hand-disassembly misread - see
 * "Still open" below). Real callers for both functions and a resolution
 * of the 0xc0006fb0 file-ownership ambiguity were also found.
 */

#include <stdint.h>

extern void panel_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 - shared assert/hard-fault handler, see panel_scan_updater.c */

/* ------------------------------------------------------------------------- *
 *  Inferred layout, NOT independently confirmed against a struct
 *  definition - reconstructed purely from the field-offset access pattern
 *  in the two functions below:
 *
 *   struct switch_entry {          /★ base + index*4, 4 bytes/entry ★/
 *       ...                         /★ 3 unidentified leading bytes ★/
 *       uint8_t  state;             /★ +0xc from array start... ★/
 *   };
 *
 *  Both functions below actually address TWO different arrays off the
 *  same `base` pointer: a 4-byte-stride "state" array (indexed directly
 *  by switch index) and a 12-byte-stride "list node" array whose own
 *  index differs between the two functions (see the "Still open" note at
 *  the bottom - this is a real, confirmed discrepancy, not reconciled
 *  here).
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 *  switch_chattering_register - FUN_c00070c0, @0xc00070c0.
 *
 *  Marks a switch index as "actively debouncing" (state = 1) and links a
 *  list node for it onto a per-switch doubly-linked list rooted at
 *  entry->0x238 - asserting (panel_fault, line 0x66 = 102) that the list
 *  was previously empty (both the entry's own 0x238 head pointer AND the
 *  new node's own "prev" field must already be 0) before doing so, i.e.
 *  this function is meant to be called at most once per switch while it
 *  is inactive.
 *
 *  The list-node array is indexed by a running counter read from
 *  *(base+4) - i.e. nodes are allocated in registration order from a
 *  shared pool, NOT by switch index (contrast switch_chattering_remove()
 *  below, which indexes the "12-byte" array by switch index directly -
 *  see "Still open").
 *
 *  Returns 0 unconditionally in every observed path.
 * ------------------------------------------------------------------------- */
int switch_chattering_register(void *base, int index)
{
	uint8_t *entry = (uint8_t *)base + index * 4;
	uint32_t already_active = *(uint32_t *)(entry + 0xc);

	if (already_active != 0)
		return 0;

	uint32_t pool_index = *(uint32_t *)((uint8_t *)base + 4);
	uint8_t *node = (uint8_t *)base + pool_index * 12;
	uint8_t *entry_head = entry + 0x238;		/* entry's own list-head field, 4 bytes at entry+0x238 */
	uint8_t *node_prev  = node + 0x148;		/* node's own "prev" field group, +0x148/+0x14c */

	uint32_t entry_head_next = *(uint32_t *)(entry_head + 4);
	uint32_t node_prev_val   = *(uint32_t *)(node_prev + 4);

	if (entry_head_next != 0 || *(uint32_t *)entry_head != 0)
		panel_fault(0, "../SwitchOnChatteringDetector.cpp", 0x66);

	/* doubly-linked insert of `node` at the head rooted at entry->0x238 */
	*(uint32_t *)(entry_head + 4) = node_prev_val;
	*(uint32_t *)(node_prev + 4)  = (uint32_t)(uintptr_t)entry_head;
	uint32_t old_head = *(uint32_t *)(entry_head + 4);
	*entry_head        = (uint32_t)(uintptr_t)node_prev;
	if (old_head != 0)
		*(uint32_t *)old_head = (uint32_t)(uintptr_t)entry_head;

	*(uint32_t *)(entry + 0xc) = 1;	/* state = 1 (actively debouncing) */
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  switch_chattering_remove - FUN_c0007158, @0xc0007158.
 *
 *  The counterpart to switch_chattering_register(). Three-state machine,
 *  confirmed by the branch structure (not by any independent struct/enum
 *  definition):
 *
 *    state == 2  ("confirmed"?) - fast path, no list unlink, return 1.
 *    state == 1  ("actively debouncing") - unlink from the doubly-linked
 *                list (asserting the link is non-NULL first, panel_fault
 *                line 0x84 = 132), then fall through to state reset.
 *    state == 0  (already idle) - fall straight through, harmless no-op
 *                on the already-zero state field.
 *
 *  Either way, state is unconditionally reset to 0 before returning.
 *  Return value: 1 iff state was 2 on entry, 0 otherwise.
 * ------------------------------------------------------------------------- */
int switch_chattering_remove(void *base, int index)
{
	uint8_t *entry = (uint8_t *)base + index * 4;
	uint32_t state = *(uint32_t *)(entry + 0xc);

	int fast_path_result = 1;

	if (state != 2) {
		fast_path_result = 0;

		if (state == 1) {
			/* NOTE: this 12-byte-stride array is indexed by `index`
			 * directly here, NOT by the *(base+4) running counter
			 * switch_chattering_register() uses - see "Still open"
			 * below, deliberately not reconciled. */
			uint8_t *node = (uint8_t *)base + index * 12;
			uint8_t *node_head = node + 0x238;

			uint32_t link = *(uint32_t *)node_head;
			uint32_t link_prev = *(uint32_t *)(node_head + 4);

			if (link == 0)
				panel_fault(0, "../SwitchOnChatteringDetector.cpp", 0x84);

			*(uint32_t *)(link + 4) = link_prev;
			if (link_prev != 0)
				*(uint32_t *)link_prev = link;

			*(uint32_t *)(node_head + 4) = 0;
			*(uint32_t *)node_head = 0;
		}
	}

	*(uint32_t *)(entry + 0xc) = 0;	/* state = 0 (idle) */
	return fast_path_result;
}

/*
 * Still open, this pass:
 *  - CONFIRMED, UNRESOLVED DISCREPANCY (upgraded from "suspected" to
 *    "Ghidra-decompile-confirmed" 2026-07-19): live decompile_function on
 *    BOTH 0xc00070c0 and 0xc0007158 reproduces this exact same discrepancy
 *    from real Ghidra pseudocode, not just this project's own hand
 *    transcription - switch_chattering_register() really does index its
 *    12-byte "list node" array by the running pool counter *(base+4);
 *    switch_chattering_remove() really does index the same-shaped array
 *    directly by the caller's switch `index` instead. This rules out a
 *    hand-disassembly misread as the explanation - it is either two
 *    genuinely different arrays sharing a stride/field-offset by
 *    coincidence, or a real firmware inconsistency. Left unreconciled per
 *    this project's own convention (see K1_V06R06/README.md's "Known
 *    cross-file discrepancies" section for precedent).
 *  - Callers of switch_chattering_register()/_remove() RESOLVED via live
 *    get_function_info: both have a SINGLE real caller,
 *    panel_manager_dispatch_scan_byte() (FUN_c0006700, see
 *    panel_manager.c) - a message dispatcher living in PanelManager.cpp's
 *    own address range, NOT a dedicated "read the switch matrix" function
 *    as speculated. Specifically, for status byte 0x80 (digital switch
 *    update), a per-bit-changed loop calls switch_chattering_remove() on
 *    a note-off-shaped bit transition and switch_chattering_register() on
 *    a note-on-shaped one, then (iff the call returned true)
 *    FUN_c0005284(ctx, switch_id, !was_register) - an uncharacterized
 *    "act on confirmed switch value" callback. This is a genuine
 *    cross-file relationship: this file's own two functions are called
 *    from code physically living in panel_manager.c's own address range -
 *    noted in both files rather than silently claimed by one.
 *  - The function at 0xc0006fb0 (a 19-iteration loop zeroing fields at
 *    +0xc/+0x148/+0x14c/+0x150 of a 12-byte-stride array) - RESOLVED IN
 *    THE NEGATIVE. Real confirmed callers via live get_function_info are
 *    FUN_c00050f0 and FUN_c0004f70 - NEITHER belongs to this file's own
 *    territory (FUN_c0004f70 is already independently cited in
 *    soc_periph.c as one of spi0_base_get()'s own two confirmed callers -
 *    a general board-bringup init routine). 0xc0006fb0 is real code
 *    belonging to a shared board-bringup/init file not yet reconstructed,
 *    not this file's own init/constructor as previously speculated (same
 *    resolution recorded in panel_scan_updater.c).
 */
