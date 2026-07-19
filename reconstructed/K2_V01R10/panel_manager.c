/* SPDX-License-Identifier: GPL-2.0 */
/*
 * panel_manager.c - KRONOS2S_V01R10.VSB's "../PanelManager.cpp" - the
 * consolidated switch/knob/LED naming and bit-level LED-state driver for
 * the Kronos 2 front panel.
 *
 * This is NEW architecture with no K1 equivalent: on Kronos 1
 * (KRONOS_V06R06.VSB), the switch/LED name table and diagnostic screen
 * lived in cpsoc.cpp (the PSoC scan-chip driver, see
 * reconstructed/K1_V06R06/cpsoc.c), and the analog control names
 * (knobs/sliders/pedals) lived in cad.cpp (the A/D converter driver). Both
 * cpsoc.cpp and cad.cpp are ENTIRELY ABSENT from K2's string table - this
 * file (plus panel_scan_updater.c and switch_chattering_detector.c) is
 * their replacement. PanelManager.cpp appears to unify what used to be two
 * separate translation units (switch/LED naming from cpsoc.cpp, analog
 * control naming from cad.cpp) into one.
 *
 * GROUND TRUTH AND METHODOLOGY, this file specifically: the pre-fetched
 * Ghidra static dump (all_decompiled_k2.json, 581 functions spanning only
 * 0xc0000000-0xc001b794) does NOT include any of the functions transcribed
 * below - despite them living well inside that same address range
 * (0xc0005784-0xc0005978), in one of the dump's own documented address
 * gaps. Ghidra's auto-analysis evidently never turned this code into
 * "Function" objects, so it's absent from a function-only export. Per this
 * task's own instructions (no live Ghidra - the MCP bridge is
 * concurrency-unsafe under parallel sessions), the functions below were
 * recovered by a DIFFERENT static method: capstone (ARM32, no Thumb seen
 * anywhere in this cluster) disassembly of the raw wrapped-ELF image
 * (kronos2s_v01r10_panel.elf, single PT_LOAD, file offset 0x54 = vaddr
 * 0xC0000000 - see KRONOS_V06R06.VSB.md's "Ghidra setup" section for the
 * wrapping convention this image also follows), driven by literal byte-
 * pattern search for the string-pointer table rather than Ghidra's own
 * function/xref graph. This is a genuinely different, lower-level
 * technique than every other file in this project - flagged explicitly so
 * a future pass knows to re-verify against a live Ghidra decompile once
 * available, rather than assume Ghidra-grade confidence.
 *
 * ANCHOR: "../PanelManager.cpp" @ 0xc002af98. As with several K1 files
 * (cpsoc.cpp being the canonical precedent), this string has NO resolved
 * static xref in the function-only dump - consistent with it simply not
 * covering this code at all (see above), not a constant-propagation false
 * negative this time.
 *
 * 2026-07-19 UPDATE - live Ghidra MCP follow-up (read-only: get_function_info,
 * decompile_function, get_disassembly, get_xrefs_to, read_memory only, no
 * mutating calls). The live Ghidra database has substantially more Function
 * objects than the pre-fetched 581-function static dump this file's first
 * pass was limited to - several "manually transcribed, no decompile
 * available" items below turned out to have real Ghidra decompiles after
 * all. Resolved this pass: the full 73-entry LED bitmap table (byte-exact,
 * via read_memory), the 77th name-table string (confirmed blank), both
 * previously-uninvestigated anchor xrefs (0xc00061cc/0xc00066f8 - see
 * panel_manager_encode_scan_event()/panel_manager_dispatch_scan_byte()
 * below), and real confirmed callers for both LED functions.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  73-entry LED bit-position lookup table - @0xc0027460, 2 bytes/entry
 *  (word_slot, bit_pos). Confirmed by direct disassembly of
 *  panel_manager_set_led_bit() (below): word_slot==0xff marks "no LED for
 *  this index" (switch-only entries); word_slot<=7 selects a 16-bit
 *  register at base+0x54+word_slot*2, word_slot>7 selects a single fixed
 *  16-bit register at base+0x64 (every word_slot>7 entry collapses to the
 *  SAME register). 2026-07-19: full table RESOLVED via live
 *  read_memory(0xc0027460, 146) - all 73 entries transcribed byte-exact
 *  below. CORRECTS the prior-pass speculation: the 0xff "no LED /
 *  switch-only" sentinel does NOT occur anywhere in the real table -
 *  every one of the 73 entries has word_slot in 0..8 and bit_pos in 0..8.
 *  word_slot==8 DOES occur (index 22), confirming the ">7 collapses to
 *  the single fixed base+0x64 register" arm in panel_manager_set_led_bit()
 *  is real, reachable code, not an unreachable template leftover as
 *  previously guessed.
 * ------------------------------------------------------------------------- */
struct panel_led_bitpos {
	uint8_t word_slot;	/* <=7 selects base+0x54+word_slot*2; ==8 selects the single fixed base+0x64 register (confirmed reachable, see above) */
	uint8_t bit_pos;	/* bit shift within the 16-bit state word */
};

/* @0xc0027460..0xc00274ce, 73*2 = 146 bytes - confirmed byte-exact via live
 * read_memory, 2026-07-19. */
const struct panel_led_bitpos panel_manager_led_bitmap[73] = {
	{0,5},{0,6},{0,7},{1,5},{1,6},{1,7},{2,5},{3,5},
	{4,5},{5,5},{5,6},{5,7},{5,8},{2,6},{3,6},{4,6},
	{6,5},{6,6},{6,7},{6,8},{4,7},{4,8},{8,0},{7,8},
	{3,8},{2,8},{7,6},{7,7},{2,7},{3,7},{0,8},{1,8},
	{7,5},{4,4},{4,3},{4,2},{4,1},{3,4},{3,3},{3,2},
	{3,1},{5,4},{5,3},{5,2},{5,1},{6,4},{6,3},{6,2},
	{6,1},{0,0},{1,0},{3,0},{4,0},{5,0},{6,0},{7,0},
	{1,1},{1,2},{2,0},{0,1},{0,2},{2,1},{0,3},{0,4},
	{7,4},{2,2},{2,3},{1,3},{2,4},{1,4},{7,2},{7,1},
	{7,3},
};

/* ------------------------------------------------------------------------- *
 *  77-entry switch/LED name table - a flat const char* array @
 *  0xc0005784-0xc00058b4 pointing into the string pool starting at
 *  0xc002a7b4 ("Combi"). Directly analogous to K1 cpsoc.c's own 73-entry
 *  name table (reconstructed/K1_V06R06/cpsoc.c) - same "named entries +
 *  one trailing default/blank sentinel" shape, just grown from 73 to 77
 *  entries. Trailing 40 bytes of each string are literal ASCII space
 *  padding in the binary (fixed-width %15s-style display field, same
 *  convention K1 used) - trimmed here since it's not semantically part of
 *  the name.
 *
 *  The trailing (77th, index 76) entry points to 0xc002b274, NOT part of
 *  the contiguous "Combi..Solo" string run. 2026-07-19: RESOLVED via live
 *  read_memory - confirmed literal ASCII spaces terminated by a NUL byte,
 *  i.e. genuinely the same "blank / out-of-range default" sentinel role
 *  K1's own trailing entry played, exactly as speculated.
 *
 *  IMPORTANT, confirmed but NOT yet attributed to a located pointer table:
 *  the .rodata string pool in this immediate neighborhood holds THREE
 *  distinct groups, only the first of which this pass found the owning
 *  pointer array for:
 *    1. "Combi".."Solo" (76 strings, 0xc002a7b4-0xc002ad90) - THIS table.
 *    2. "Knob 1".."Foot Switch" (24 strings, 0xc002afac-0xc002b178,
 *       between PanelManager.cpp's own anchor and PanelScanUpdater.cpp's)
 *       - analog control names (knobs/sliders/pedals/VJ-X/VJ-Y/damper),
 *       i.e. exactly K1 cad.cpp's domain. No owning pointer array located
 *       this pass - NEEDS LIVE QUERY.
 *    3. "SET LIST".."Individual Pan" (~35 strings, 0xc002ada4-0xc002af84,
 *       after cobjectmgr.cpp's own anchor) - looks like bi-color-LED /
 *       extended-mode name variants (c.f. K1's "Sampling Green"/"Sampling
 *       Rec" bi-color pair, matched here by "Sampling White"/"Sampling
 *       Rec" and "Seq White"/"Seq Red" - same bi-color-LED pattern, just
 *       renamed Green->White). No owning pointer array located either -
 *       NEEDS LIVE QUERY.
 *  Groups 2 and 3 are real, confirmed string content; their consumer code
 *  is not identified. Do not assume they belong to PanelManager.cpp just
 *  because they sit in its neighborhood - K1's own precedent (cpsoc.cpp's
 *  string pool sitting between CDix4192.cpp's and cad.cpp's anchors) shows
 *  this kind of adjacency is real but attribution still needs a genuine
 *  code xref, which neither group has here.
 * ------------------------------------------------------------------------- */
extern const char *const panel_manager_control_names[77];	/* @0xc0005784, ends 0xc00058b4 */

/*
 * Actual string content of panel_manager_control_names[0..75] (index 76 is
 * the unresolved sentinel above), transcribed directly from the string
 * pool for documentation - NOT wired up as an initializer here since the
 * array itself is a real binary data table (extern, above), not something
 * this file defines:
 *
 *  0 Combi        10 TenKey 0    20 TenKey -    30 Bank U-D    40 Rec
 *  1 Prog         11 TenKey 1    21 TenKey .    31 Bank U-E    41 Seq Start
 *  2 Seq          12 TenKey 2    22 Enter       32 Bank U-F    42 Tap Tepo
 *  3 Sampling     13 TenKey 3    23 Bank I-A    33 Bank U-G    43 Sampling Rec
 *  4 Global       14 TenKey 4    24 Bank I-B    34 Pause       44 Sampling Start
 *  5 Disk         15 TenKey 5    25 Bank I-C    35 Rew         45 KARMA Module Ctr
 *  6 Live         16 TenKey 6    26 Bank I-D    36 FF          46 KARMA On/Off
 *  7 Exit         17 TenKey 7    27 Bank I-E    37 Locate      47 KARMA Latch
 *  8 Help         18 TenKey 8    28 Bank I-F    38 (Rec, see 40) 48 DrumTrack On/Off
 *  9 Compare      19 TenKey 9    29 Bank I-G    39 Bank U-A    49 Inc
 * 50 Dec  51 Timbre  52 Audio  53 External  54 Knobs/KARMA  55 ToneAdjust
 * 56-63 Play/Mute 1..8   64-71 Select 1..8   72 Mixer Knobs
 * 73 Reset Controller    74 (unlisted duplicate check - see string dump)
 * 75 Solo
 *
 * (Exact 0-based mapping between "TenKey 6..9 / Bank I-G / Bank U-A"
 * boundary and indices 16-39 not independently re-verified digit-by-digit
 * against the table - the *set* of 76 names and their order in the string
 * pool is ground truth; this numbered list is a best-effort re-index for
 * readability, not itself re-checked against a live pointer dump. NEEDS
 * LIVE QUERY if an exact index->name mapping matters for a specific call
 * site.)
 */

/* ------------------------------------------------------------------------- *
 *  panel_manager_set_led_bit - FUN_c00058b8, @0xc00058b8.
 *
 *  Sets or clears one bit in a packed 16-bit LED-state register, addressed
 *  indirectly via panel_manager_led_bitmap[index]. Directly reconstructed
 *  from ARM disassembly (register-level transcription, no Ghidra
 *  decompile available for this address - see file header).
 *
 *  Bounds check is `index > 0x48` (72), i.e. only the FIRST 73 of the
 *  77-entry name table are ever valid targets here - entries 73-76 (the
 *  tail of the name table, including the unresolved sentinel) can never
 *  reach this function. Not explained further this pass (plausible: those
 *  4 entries are switch-only, covered by a name-table lookup elsewhere,
 *  not LED state).
 * ------------------------------------------------------------------------- */
void panel_manager_set_led_bit(void *base, int index, int state)
{
	unsigned idx = (unsigned)index & 0xffu;

	if (idx > 0x48)			/* 72 - out of bounds for THIS table */
		return;

	uint8_t word_slot = panel_manager_led_bitmap[idx].word_slot;
	uint8_t bit_pos    = panel_manager_led_bitmap[idx].bit_pos;

	if (word_slot == 0xff)		/* no LED backing this index */
		return;

	volatile uint16_t *reg;
	if (word_slot <= 7)
		reg = (volatile uint16_t *)((uint8_t *)base + 0x54 + word_slot * 2u);
	else
		/* every word_slot > 7 collapses to the SAME fixed register -
		 * transcribed faithfully, not smoothed into a per-slot formula. */
		reg = (volatile uint16_t *)((uint8_t *)base + 0x64);

	uint16_t cur = *reg;
	uint16_t next = (state == 1) ? (uint16_t)(cur | (1u << bit_pos))
				      : (uint16_t)(cur & ~(1u << bit_pos));
	*reg = next;
}

/* ------------------------------------------------------------------------- *
 *  panel_manager_apply_led_group - FUN_c0005914, @0xc0005914.
 *
 *  Calls panel_manager_set_led_bit() 16 times in a row, extracting one bit
 *  per call from a caller-supplied 16-bit mask, for LED indices
 *  [group*16 .. group*16+15]. The obvious "write a whole bank of LEDs from
 *  one packed word" bulk primitive.
 *
 *  2026-07-19: real caller CONFIRMED via live get_xrefs_to(0xc0005914) -
 *  FUN_c0009b54, a large incoming wire-protocol opcode dispatcher (opcode
 *  5 -> apply_led_group(dev, group=byte[2], mask=16-bit combine of
 *  byte[1]/byte[0])). This directly confirms the old speculation below
 *  about "whatever wire-protocol opcode replaces K1's cpsoc.cpp LED-
 *  bargraph handlers" - FUN_c0009b54 is that opcode dispatcher. Its own
 *  file attribution is NOT resolved (it sits just past
 *  system_info_holder_print_status()'s own address range in
 *  system_info_holder.c and could belong there or to an undiscovered
 *  neighbor) - cited here by address only, not claimed for this file.
 * ------------------------------------------------------------------------- */
void panel_manager_apply_led_group(void *base, int group, uint16_t bitmask)
{
	int start = group * 16;

	for (int i = 0; i < 16; i++) {
		int bit = (bitmask >> i) & 1;
		panel_manager_set_led_bit(base, start + i, bit);
	}
}

/* ------------------------------------------------------------------------- *
 *  panel_manager_encode_scan_event - FUN_c00061d4, @0xc00061d4.
 *
 *  2026-07-19: RESOLVES the old "0xc00061cc" NEEDS LIVE QUERY item. Live
 *  get_function_info(0xc00061cc) reports no Function object there - it is
 *  a literal-pool data slot; the real code is a full function starting 8
 *  bytes later at 0xc00061d4 (confirmed real APCS prologue via
 *  get_disassembly), ending at 0xc0006290. No Ghidra decompile is
 *  available for it either (same "never function-ified" gap as this
 *  file's other functions) - hand-transcribed from the raw disassembly,
 *  same technique as the rest of this file.
 *
 *  This independently confirms - without having to re-derive it - the
 *  characterization already recorded by the LATER, 2026-07-19 "cpsoc.cpp-
 *  adjacent stragglers" pass (see this file's own README section and
 *  omap_l137_addr_gap_misc.c's write-up): "one is a switch/knob scan-event
 *  encoder" - that pass investigated this same address while ruling out a
 *  relocated clcdc_test_dispatch.c, but never folded the finding back into
 *  this file. This is that fold-back.
 *
 *  Behavior: reads a "busy" flag at ctx+0x15. If idle, retries a raw
 *  scan-read (FUN_c0006008, uncharacterized - likely the actual hardware
 *  switch/knob matrix read) up to 3 times looking for a stable (zero)
 *  result; on success writes 0xff (no-change marker) into the target
 *  device's status byte at [*ctx+0x38] and clears a retry-count field at
 *  ctx+0x24. On an unstable read, instead arms the busy flag (ctx+0x15=1),
 *  stashes the raw scan byte at ctx+0x2c, and clears ctx+0x24 (retry
 *  counter) for the deferred path below. If busy was ALREADY set on entry,
 *  takes the deferred branch instead: advances the retry counter at
 *  ctx+0x24 against a limit at ctx+0x20, always writing byte
 *  *(ctx+0x2c+0x2d... i.e. offset 0x2d of the retry-array entry) to
 *  [*ctx+0x38]; once the counter reaches the limit, clears the busy flag
 *  (gives up and accepts whatever value was last seen).
 * ------------------------------------------------------------------------- */
void panel_manager_encode_scan_event(void *ctx)
{
	uint8_t *c = (uint8_t *)ctx;

	if (c[0x15] == 0) {
		int attempt;
		int unstable = 0;

		for (attempt = 0; attempt <= 2; attempt++) {
			/* FUN_c0006008(ctx, ctx+0x2c) - raw scan-matrix read,
			 * result in r0; uncharacterized this pass. */
			extern int FUN_c0006008(void *ctx, void *out);
			if (FUN_c0006008(ctx, c + 0x2c) != 0) {
				unstable = 1;
				break;
			}
		}

		uint8_t *devp = *(uint8_t **)ctx;
		if (!unstable) {
			devp[0x38] = 0xff;	/* stable across all 3 reads: "no change" marker; retry counter untouched here */
			return;
		}

		/* unstable: accept the raw scan byte now, but arm busy/retry
		 * state so the NEXT call takes the deferred path below instead
		 * of immediately re-reading. */
		devp[0x38] = c[0x2c];
		c[0x15] = 1;
		*(uint32_t *)(c + 0x24) = 0;
		return;
	}

	/* busy already set - deferred/retry path */
	uint32_t counter = *(uint32_t *)(c + 0x24);
	uint32_t limit   = *(uint32_t *)(c + 0x20);
	uint8_t *devp = *(uint8_t **)ctx;
	uint8_t val = c[counter + 0x2d];	/* ldrb r1,[r2,#0x2d] where r2 = ctx+counter */

	counter++;
	*(uint32_t *)(c + 0x24) = counter;
	devp[0x38] = val;
	if (counter == limit)
		c[0x15] = 0;	/* give up, accept last-seen value */
}

/* ------------------------------------------------------------------------- *
 *  panel_manager_dispatch_scan_byte - FUN_c0006700, @0xc0006700.
 *
 *  2026-07-19: RESOLVES the old "0xc00066f8" NEEDS LIVE QUERY item.
 *  0xc00066f8 is also a literal-pool data slot (holding the
 *  "../PanelManager.cpp" anchor string address plus a line-number
 *  constant DAT_c00066fc), referenced from INSIDE this function's own
 *  body via a live Ghidra decompile (312 instructions, fully obtained).
 *
 *  A stateful multi-byte message assembler + dispatcher - the "wire-
 *  protocol opcode" panel_manager_apply_led_group()'s own old comment
 *  speculated about (that speculation actually resolved to a DIFFERENT
 *  function, FUN_c0009b54, see apply_led_group() above; this function is
 *  a sibling, lower-level byte-stream assembler feeding switch/knob
 *  events instead). Accumulates incoming bytes into ctx+0x29.. keyed by a
 *  "receiving" flag at ctx+0x14, expected length at ctx+0x18, running
 *  count at ctx+0x1c; once the expected count is reached, dispatches on
 *  the FIRST assembled byte (status byte, ctx+0x28):
 *
 *    0x80          - per-bit-changed loop over up to 8 switches: calls
 *                    switch_chattering_remove() (see
 *                    switch_chattering_detector.c) on a note-off-shaped
 *                    bit transition, switch_chattering_register() on a
 *                    note-on-shaped one, then FUN_c0005284(ctx, switch_id,
 *                    !was_register) - uncharacterized "act on confirmed
 *                    switch value" callback - iff the register/remove call
 *                    returned true (fast-path/state==2 in the remove()
 *                    case).
 *    0x90          - knob/analog-control display update (draws a value
 *                    via panel_draw_status_text-family calls).
 *    0xa0          - analog-control smoothing/center-tracking update into
 *                    a per-control 16-bit table at ctx+0x78+idx*2.
 *    0xa1/other    - a two-axis (X/Y) position filter feeding a small
 *                    on-screen crosshair/graph display (calls
 *                    FUN_c001ac94, an uncharacterized scale/clamp helper).
 *    0xa3 or less  - stores a 4-entry array slot at ctx+0x108+idx*4.
 *    0xa4          - forwards a value to a status-text display.
 *    0xf0          - resets a 16-word analog-center table (ctx+0x64..) and
 *                    sets all-1s into a bitmask at ctx+0xb8, plus an array
 *                    of 0x7fff default values.
 *    0xf1          - sets a "ready"/"calibration done" flag at ctx+5 (this
 *                    is the SAME flag FUN_c00067f4, below, tests before
 *                    calling this dispatcher on each tick).
 *    unrecognized  - panel_fault(0, "../PanelManager.cpp", <line>) - the
 *                    resolved 0xc00066f8/0xc00066fc reference.
 *
 *  This is the CONFIRMED real caller of both switch_chattering_register()
 *  and switch_chattering_remove() (see switch_chattering_detector.c) -
 *  resolves that file's own "no caller located" item too.
 *
 *  NOT fully transcribed byte-for-byte here (it is a real Ghidra decompile
 *  of a 312-instruction function with several uncharacterized callees
 *  (FUN_c0005284, FUN_c001ac94's two call sites' exact field semantics,
 *  the 0xa1 two-axis filter's own display wiring) - written up as an
 *  accurate structural summary of confirmed real logic rather than a
 *  line-for-line port, since several of its own callees are themselves
 *  out of this file's scope. extern-declared below for callers that need
 *  the entry point; a full line-for-line port is future work if a later
 *  pass needs it.
 * ------------------------------------------------------------------------- */
extern void panel_manager_dispatch_scan_byte(void *ctx, uint8_t status_byte);	/* FUN_c0006700, @0xc0006700 - see above for confirmed structural summary */

/*
 * Still open, this pass:
 *  - panel_manager_dispatch_scan_byte() (FUN_c0006700) itself is only
 *    summarized, not fully line-for-line ported - several of its own
 *    callees (FUN_c0005284, FUN_c001ac94, FUN_c0007454, FUN_c000704c) are
 *    uncharacterized and out of this file's own scope.
 *  - panel_manager_encode_scan_event()'s own callee FUN_c0006008 (the raw
 *    scan-matrix read) is uncharacterized.
 *  - Real caller chain CONFIRMED via live decompile: FUN_c00067f4 is a
 *    periodic per-tick service function - checks a "ready" flag at ctx+5
 *    (set by dispatch_scan_byte()'s own 0xf1 opcode above), and when both
 *    ready-bits at [*ctx+0x10]&0x300==0x300 are set, calls
 *    panel_manager_dispatch_scan_byte(ctx, byte) then
 *    panel_manager_encode_scan_event(ctx) then FUN_c000704c(ctx+0x47)
 *    (uncharacterized) each tick - ties both resolved functions together
 *    under one common driver. FUN_c000704c and FUN_c00067f4's own further
 *    callers were not traced past this one level (out of budget).
 *  - Groups 2 and 3 of the string pool (analog control names; extended/
 *    bi-color LED names) still have no located owning pointer table - see
 *    the big comment above panel_manager_control_names[].
 */
