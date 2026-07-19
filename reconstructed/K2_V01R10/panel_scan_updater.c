/* SPDX-License-Identifier: GPL-2.0 */
/*
 * panel_scan_updater.c - KRONOS2S_V01R10.VSB's "../PanelScanUpdater.cpp".
 *
 * NEW architecture, no direct K1 file equivalent, but a clear functional
 * descendant of a specific K1 FEATURE: KRONOS_V06R06.VSB's own string
 * table has "Updating the panel scan system... / Completed! / Cannot
 * update it." (see docs/modules/KRONOS_V06R06.VSB.md's "Two-tier control
 * system" section) mixed into cpsoc.cpp alongside the rest of the PSoC
 * driver. K2 keeps the exact same feature (field-updating the secondary
 * "Panel Scan System" microcontroller's own firmware) but has split it out
 * into its own dedicated translation unit - a real, confirmed refactor,
 * not a guess: this file's own strings are "Update panel scan system." /
 * "->Now writing..." / "->->Completed. Tern power off." (0xc002b1a4,
 * 0xc002b1c0, 0xc002b1d4 - "Tern" is presumably a typo for "Turn", exactly
 * as K1's own string table had at least one clear typo, "Tepo" for
 * "Tempo" - Korg's own firmware text, transcribed faithfully).
 *
 * METHODOLOGY NOTE: see panel_manager.c's file header for the full
 * explanation of why this file's functions were recovered via manual
 * capstone disassembly of the raw wrapped ELF rather than a Ghidra
 * decompile - the pre-fetched static dump's function list does not cover
 * this address range at all.
 *
 * ANCHOR: "../PanelScanUpdater.cpp" @ 0xc002b18c. Like PanelManager.cpp,
 * has no resolved xref in the function-only static dump (the dump simply
 * doesn't include this code, not a constant-propagation false negative).
 *
 * 2026-07-19 UPDATE - live Ghidra MCP follow-up (read-only: get_function_info,
 * decompile_function, get_disassembly, get_xrefs_to only, no mutating
 * calls). All four sub-step callees below now have REAL Ghidra decompiles
 * (the live database has more Function objects than the pre-fetched
 * 581-function static dump this file's first pass was limited to), and a
 * real caller for panel_scan_updater_run() itself was found. See each
 * function's own header below for details.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Shared cross-file primitives this function calls into, not owned by
 *  this file. Addresses only - not independently attributed to another
 *  reconstructed .c file this pass (may already be covered by the
 *  concurrent shared-driver-layer pass; not verified against it here to
 *  avoid touching files out of this file's lane).
 * ------------------------------------------------------------------------- */
extern void panel_fault(const void *unused_arg1, const char *file, int line);		/* FUN_c000a730 - shared assert/hard-fault handler, same family as K1's crypto_at88_fault/FUN_c000919c; referenced from every one of the 5 new K2 files this pass covers */
extern void panel_draw_status_text(int col, int row, const char *fmt, int color);	/* FUN_c000a704 - screen-print helper, (col, row, fmt, color)-ish; NOT independently confirmed against clcdc.c's own draw-text primitives */
extern void *FUN_c00017d0(void);							/* callee at @0xc00017d0, return value stored into *ctx - not identified this pass; ALSO independently confirmed in soc_periph.c as spi0_base_get() - this call very likely fetches the SPI0 peripheral base into the ctx handle */
extern void  FUN_c000303c(void);							/* callee at @0xc000303c, no-arg - not identified this pass */
extern void *FUN_c0003150(void *dev, unsigned cmd_or_val, void *resp_out);		/* shared SPI/TWI command-response primitive: send cmd_or_val (or read into *resp_out if resp_out non-NULL) - real signature confirmed via decompile of all 4 sub-steps below; NOT independently attributed to a reconstructed file this pass */
extern void  FUN_c00069a4(void *ctx, int ticks);					/* delay/wait helper, confirmed via decompile - not independently attributed */
extern int   FUN_c00069b0(void *ctx);							/* poll-ready helper, confirmed via decompile (returns 0/nonzero) - not independently attributed */
extern void  FUN_c0006a04(void *ctx, unsigned cmd);					/* send-command helper (distinct from FUN_c0003150), confirmed via decompile - not independently attributed */
extern void  FUN_c00068d4(void *ctx, uint32_t arg_a, uint32_t arg_b);			/* uncharacterized helper called by both step_verify and apply with FIXED global arguments (not the hexstream/len params) - confirmed via decompile, not identified further */
extern uint32_t DAT_c0006c50, DAT_c0006c54;	/* apply()'s own fixed literal-pool args to FUN_c00068d4 - values not resolved this pass */
extern uint32_t DAT_c0006d1c, DAT_c0006d20;	/* step_verify()'s own fixed literal-pool args to FUN_c00068d4 - values not resolved this pass */

void panel_scan_updater_step_erase(void *ctx);		/* FUN_c0006dc4, @0xc0006dc4 - see below */
void panel_scan_updater_step_write(void *ctx);		/* FUN_c0006d28, @0xc0006d28 - see below */
void panel_scan_updater_step_verify(void *ctx);	/* FUN_c0006c5c, @0xc0006c5c - see below */
void panel_scan_updater_apply(void *ctx, const uint8_t *hexstream, int len);	/* FUN_c0006aa8, @0xc0006aa8 - see below; CORRECTS the prior signature guess (int arg_a, int arg_b) - real decompile shows a (pointer, length) pair */

/* ------------------------------------------------------------------------- *
 *  panel_scan_updater_run - FUN_c0006eec, @0xc0006eec.
 *
 *  The top-level PSoC/"Panel Scan System" firmware-update sequencer.
 *  Directly transcribed from ARM disassembly (register-level, no
 *  decompile available - see file header). Real APCS-style calling
 *  convention evidence: prologue is `mov ip,sp; push {r4-r8,fp,ip,lr,pc}`
 *  with `sub fp,ip,#4`, and a 5th argument is read from `[fp,#4]` - i.e.
 *  this is a (ctx, r1, r2, r3_as_byte, stack_arg5) 5-parameter function.
 *
 *  2026-07-19 UPDATE: real caller CONFIRMED via live get_xrefs_to(0xc0006ee8)
 *  - sole caller is FUN_c000685c, decompiled in full:
 *      panel_scan_updater_run(local_ctx, DAT_c000689c, DAT_c00068a0, 0xb3, 0x39);
 *      FUN_c0005088(param_1);   // uncharacterized cleanup/notify, out of scope
 *  This confirms expect_ver=0xb3 (179) and expect_rev=0x39 (57) as real
 *  constants (matches the raw disassembly already partially visible at
 *  this call site: `mov r12,#0x39` / `mov r3,#0xb3`).
 *
 *  b1/b2's real purpose is now STRONGLY suggested (not fully proven) by
 *  panel_scan_updater_apply()'s own decompiled signature below: apply()'s
 *  2nd/3rd params are dereferenced as a (const uint8_t *hexstream, int
 *  len) pair, not plain integers, and b1/b2 pass straight through to
 *  apply() unchanged - i.e. b1 is very likely a pointer to an ASCII
 *  hex-encoded firmware/version stream and b2 its length, not a
 *  "progress-report byte pair" as previously guessed. FUN_c000685c's own
 *  DAT_c000689c/DAT_c00068a0 literal-pool values were not resolved this
 *  pass (would need one more read_memory + string dump to pin down the
 *  actual hex stream content).
 *
 *  Prints "Update panel scan system." at screen row 0x64 (100), then
 *  "->Now writing..." at row 0x73 (115), runs the erase/write/verify
 *  sub-steps, checks two version/revision bytes read out of the ctx
 *  struct (offsets +0xb and +0xc, confirmed written by step_verify() AND
 *  apply() below - both independently write the same two offsets) against
 *  the caller-supplied expected values - faulting via panel_fault() at
 *  line 0x53 (83) if they mismatch after the update - and finally prints
 *  "->->Completed. Tern power off." at row 0x82 (130) as a tail call (the
 *  function's own return IS this final print call's return, standard ARM
 *  leaf-tail-call idiom).
 * ------------------------------------------------------------------------- */
void panel_scan_updater_run(void *ctx, const uint8_t *hexstream, int len, uint8_t expect_ver, uint8_t expect_rev)
{
	*(void **)ctx = FUN_c00017d0();
	FUN_c000303c();

	panel_draw_status_text(10, 0x64, "Update panel scan system.", 0);
	panel_draw_status_text(10, 0x73, "->Now writing...", 0);

	panel_scan_updater_step_erase(ctx);
	panel_scan_updater_step_write(ctx);
	panel_scan_updater_step_verify(ctx);
	panel_scan_updater_apply(ctx, hexstream, len);

	uint8_t got_ver = *((uint8_t *)ctx + 0xb);
	uint8_t got_rev = *((uint8_t *)ctx + 0xc);
	if (got_ver != expect_ver || got_rev != expect_rev)
		panel_fault(0, "../PanelScanUpdater.cpp", 0x53);

	panel_draw_status_text(10, 0x82, "->->Completed. Tern power off.", 0);
}

/* ------------------------------------------------------------------------- *
 *  panel_scan_updater_step_erase - FUN_c0006dc4, @0xc0006dc4.
 *
 *  2026-07-19: RESOLVED via live decompile_function (a real Ghidra
 *  decompile was available - the live database has more Function objects
 *  than the pre-fetched static dump this file's first pass was limited
 *  to). Confirmed sole caller: panel_scan_updater_run() itself, matching
 *  this file's existing top-level sequencer exactly.
 *
 *  A 6-step SPI/TWI command-response handshake: send command 0x30
 *  ("erase"?), wait ~400 ticks, poll-ready; read one response byte and
 *  assert it equals ASCII '0'; send command 0, poll-ready (no wait);
 *  send command 0xcf, wait ~200 ticks, poll-ready; read one response byte
 *  and assert it equals 0xcf (device echoes the command byte back); wait
 *  ~200 more ticks, poll-ready one final time, faulting via panel_fault()
 *  at line 0x53 (83) if that last poll fails.
 *
 *  HONEST FLAG: Ghidra's decompile shows 6 total panel_fault() call
 *  sites but only recovered the LINE NUMBER argument for the LAST one
 *  (0x53) - the other 5 show only 2 of panel_fault()'s 3 real arguments
 *  (elided by the decompiler, not by this transcription). Rather than
 *  invent plausible sequential line numbers, each is marked explicitly
 *  below as not recovered.
 * ------------------------------------------------------------------------- */
void panel_scan_updater_step_erase(void *ctx)
{
	char resp;

	FUN_c0003150(*(void **)ctx, 0x30, 0);
	FUN_c00069a4(ctx, 400);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c0003150(*(void **)ctx, 0, &resp);
	if (resp != '0')
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c00069a4(ctx, 0);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c0003150(*(void **)ctx, 0xcf, 0);
	FUN_c00069a4(ctx, 200);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c0003150(*(void **)ctx, 0, &resp);
	if ((uint8_t)resp != 0xcf)
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c00069a4(ctx, 200);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0x53);
}

/* ------------------------------------------------------------------------- *
 *  panel_scan_updater_step_write - FUN_c0006d28, @0xc0006d28.
 *
 *  2026-07-19: RESOLVED via live decompile_function. Confirmed sole
 *  caller: panel_scan_updater_run().
 *
 *  Sends command 0xc3 ("write"?), then reads 7 consecutive response bytes
 *  one at a time into ctx+4..ctx+10, asserting poll-ready after each
 *  (fault at line 0x61 - the only one of this function's 3 assert sites
 *  whose line number Ghidra's decompile recovered). After the 7 bytes,
 *  asserts byte ctx+4 == ':' (line not recovered) and ctx+5 == 4 (line
 *  0x68).
 * ------------------------------------------------------------------------- */
void panel_scan_updater_step_write(void *ctx)
{
	uint8_t *c = (uint8_t *)ctx;
	int i;

	FUN_c0006a04(ctx, 0xc3);

	for (i = 0; i < 7; i++) {
		FUN_c0003150(*(void **)ctx, 0, c + 4 + i);
		FUN_c00069a4(ctx, 200);
		if (!FUN_c00069b0(ctx))
			panel_fault(0, "../PanelScanUpdater.cpp", 0x61);
	}

	if (c[4] != ':')
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);
	if (c[5] != 4)
		panel_fault(0, "../PanelScanUpdater.cpp", 0x68);
}

/* ------------------------------------------------------------------------- *
 *  panel_scan_updater_step_verify - FUN_c0006c5c, @0xc0006c5c.
 *
 *  2026-07-19: RESOLVED via live decompile_function - ALL THREE
 *  panel_fault() call sites in this function had their line numbers
 *  recovered cleanly (0x77, 0x7b, 0x7e), unlike step_erase/step_write
 *  above. Confirmed sole caller: panel_scan_updater_run().
 *
 *  Sends command 0xf0 ("verify"?), calls the uncharacterized FUN_c00068d4
 *  helper with two FIXED global arguments, sends 0xff, polls-ready
 *  (asserting at 0x77); reads one response byte into ctx+0xb (this is
 *  CONFIRMED to be the same "got_ver" byte panel_scan_updater_run() reads
 *  and compares against expect_ver), waits+polls (asserting at 0x7b);
 *  reads one more response byte into ctx+0xc (CONFIRMED "got_rev",
 *  likewise cross-checked by panel_scan_updater_run()), waits+polls
 *  (asserting at 0x7e on failure - inverted success/return-first idiom
 *  matching the ARM tail-branch shape).
 * ------------------------------------------------------------------------- */
void panel_scan_updater_step_verify(void *ctx)
{
	uint8_t *c = (uint8_t *)ctx;

	FUN_c0006a04(ctx, 0xf0);
	FUN_c00068d4(ctx, DAT_c0006d1c, DAT_c0006d20);

	FUN_c0003150(*(void **)ctx, 0xff, 0);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0x77);

	FUN_c0003150(*(void **)ctx, 0, c + 0xb);
	FUN_c00069a4(ctx, 200);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0x7b);

	FUN_c0003150(*(void **)ctx, 0, c + 0xc);
	FUN_c00069a4(ctx, 200);
	if (FUN_c00069b0(ctx))
		return;
	panel_fault(0, "../PanelScanUpdater.cpp", 0x7e);
}

/* ------------------------------------------------------------------------- *
 *  panel_scan_updater_apply - FUN_c0006aa8, @0xc0006aa8.
 *
 *  2026-07-19: RESOLVED via live decompile_function. CORRECTS this file's
 *  own prior signature guess of "(ctx, int arg_a, int arg_b)" - the real
 *  decompile dereferences param_2 as a byte pointer and uses param_3 only
 *  in length comparisons, i.e. the real signature is (ctx, const uint8_t
 *  *hexstream, int len). This also revises panel_scan_updater_run()'s own
 *  b1/b2 documentation above (they pass straight through to this
 *  function's hexstream/len).
 *
 *  Sends command 0x30 (the SAME command byte step_erase's own first step
 *  uses - possibly a shared "select region" framing byte, not
 *  independently decoded), calls FUN_c00068d4 with two FIXED global
 *  arguments (NOT the hexstream/len params - a separate, uncharacterized
 *  side effect), then parses `hexstream` as an ASCII hex-nibble-pair
 *  stream separated by ':' bytes: two hex nibbles accumulate into one
 *  byte value (high<<4 | low), and encountering a completed pair OR a
 *  literal ':' byte both converge on the SAME FUN_c0003150(dev, byte, 0)
 *  send call in the real ARM control flow - meaning a ':' in the stream
 *  is sent to the device VERBATIM (0x3a) when encountered, not just used
 *  as a separator. Transcribed exactly as Ghidra's decompile shows, not
 *  "cleaned up" to what might seem like more sensible intent, per this
 *  project's convention of preserving confirmed real behavior over
 *  assumed intent.
 *
 *  After the stream is consumed, runs the SAME response handshake shape
 *  as step_verify() (poll-ready, expect 'U', poll, read ctx+0xb, poll,
 *  read ctx+0xc, poll), faulting via panel_fault() at line 0xc4 on the
 *  final failure - the only one of this function's 4 assert sites whose
 *  line number Ghidra's decompile recovered.
 * ------------------------------------------------------------------------- */
void panel_scan_updater_apply(void *ctx, const uint8_t *hexstream, int len)
{
	uint8_t *c = (uint8_t *)ctx;
	unsigned accum = 0;
	int expect_high_nibble = 1;	/* Ghidra's bVar1: 1 = next hex digit starts a new pair (high nibble) */
	char resp;
	int i;

	FUN_c0006a04(ctx, 0x30);
	FUN_c00068d4(ctx, DAT_c0006c50, DAT_c0006c54);	/* uncharacterized; fixed-global args, NOT hexstream/len */

	for (i = 0; i < len; i++) {
		uint8_t ch = hexstream[i];
		unsigned nibble;

		if (ch == ':') {
			FUN_c0003150(*(void **)ctx, ch, 0);	/* sends ':' VERBATIM - see header note */
			accum = ch;
			continue;
		}

		if ((unsigned)(ch - 0x30u) < 10)
			nibble = ch - 0x30u;
		else if ((unsigned)(ch - 0x41u) < 6)
			nibble = ch - 0x37u;
		else
			continue;	/* not a hex digit or ':' - no action, matches real fallthrough */

		if (expect_high_nibble) {
			accum = (nibble & 0xf) << 4;
			expect_high_nibble = 0;
		} else {
			unsigned byte_val = accum | (nibble & 0xff);
			FUN_c0003150(*(void **)ctx, byte_val, 0);
			accum = byte_val;
			expect_high_nibble = 1;
		}
	}

	FUN_c00069a4(ctx, 100);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c0003150(*(void **)ctx, 0, &resp);
	if (resp != 'U')
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c00069a4(ctx, 200);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c0003150(*(void **)ctx, 0, c + 0xb);	/* got_ver - matches panel_scan_updater_run()'s own check */
	FUN_c00069a4(ctx, 200);
	if (!FUN_c00069b0(ctx))
		panel_fault(0, "../PanelScanUpdater.cpp", 0 /* line not recovered by decompile */);

	FUN_c0003150(*(void **)ctx, 0, c + 0xc);	/* got_rev - matches panel_scan_updater_run()'s own check */
	FUN_c00069a4(ctx, 200);
	if (FUN_c00069b0(ctx))
		return;
	panel_fault(0, "../PanelScanUpdater.cpp", 0xc4);
}

/*
 * Still open, this pass:
 *  - FUN_c00017d0/FUN_c000303c's real purpose: FUN_c00017d0 is ALSO
 *    independently confirmed in soc_periph.c as spi0_base_get() - this
 *    call very likely fetches the SPI0 peripheral base into the ctx
 *    handle at function entry; FUN_c000303c remains uncharacterized.
 *  - FUN_c0003150/FUN_c00069a4/FUN_c00069b0/FUN_c0006a04/FUN_c00068d4 are
 *    real, confirmed-via-decompile shared primitives, but not
 *    independently attributed to any reconstructed file this pass (likely
 *    a shared SPI/TWI helper file not yet reconstructed).
 *  - Several panel_fault() call sites' line-number arguments were elided
 *    by Ghidra's own decompile (not recovered) - marked explicitly above
 *    rather than guessed.
 *  - FUN_c000685c's own DAT_c000689c/DAT_c00068a0 literal-pool values
 *    (the actual hex-stream pointer/length passed to panel_scan_updater_run)
 *    were not resolved - would need one more read_memory + string dump.
 *  - FUN_c00068d4's own two fixed-global arguments (DAT_c0006c50/
 *    DAT_c0006c54 in apply(), DAT_c0006d1c/DAT_c0006d20 in step_verify())
 *    were not resolved to real values.
 *  - 0xc0006fb0 (the struct-array init function once flagged as ambiguous
 *    between this file and switch_chattering_detector.c): RESOLVED IN THE
 *    NEGATIVE for both files. Real confirmed callers via
 *    get_function_info are FUN_c00050f0 and FUN_c0004f70 - neither
 *    belongs to panel_scan_updater.c's or switch_chattering_detector.c's
 *    own territory (FUN_c0004f70 is already independently cited in
 *    soc_periph.c as one of spi0_base_get()'s own two confirmed callers -
 *    a general board-bringup init routine). 0xc0006fb0 is real code
 *    belonging to a shared board-bringup/init file not yet reconstructed,
 *    not either of these two files.
 */
