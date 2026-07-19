/* SPDX-License-Identifier: GPL-2.0 */
/*
 * system_info_holder.c - KRONOS2S_V01R10.VSB's "../SystemInfoHolder.cpp".
 *
 * NEW architecture, no K1 equivalent as a dedicated file (K1's version/
 * revision reporting strings - "Main System Version:%02d Revision:%02d" /
 * "Panel Scan System Version:%02d Revision:%02d" - are not attributed to
 * any specific K1 .cpp file in the existing K1_V06R06 reconstruction).
 * K2's version strings are RENAMED but structurally identical:
 * "Sub System Version:%02d Revision:%02d" / "Panel Scanner
 * Version:%02d Revision:%02d" (0xc002b3d4, 0xc002b404) - "Main System" ->
 * "Sub System", "Panel Scan System" -> "Panel Scanner". Plausibly reflects
 * the OMAP-L1x board being demoted from "Main" to "Sub" relative to
 * whatever now owns the "Main System" label in K2's architecture (not
 * determined this pass - PanelManager.cpp is a candidate but not
 * confirmed).
 *
 * Functionally, "SystemInfoHolder" fits its name well: the one function
 * recovered this pass is a version/health-status DISPLAY routine, not a
 * scan/protocol driver - consistent with a singleton that holds and
 * reports system identification info, as the name suggests.
 *
 * METHODOLOGY NOTE: see panel_manager.c's file header - recovered via
 * manual capstone disassembly of the raw wrapped ELF, not a Ghidra
 * decompile (the static function dump doesn't cover this address range).
 *
 * ANCHOR: "../SystemInfoHolder.cpp" @ 0xc002b5d0. Has a real xref this
 * time (found via raw byte-pattern search, not Ghidra) at 0xc000a7c8 -
 * NOT disassembled this pass, see "Still open" below. The function
 * transcribed below was instead found via its OWN format-string xrefs
 * (SyncErrorCount / Panel Scanner Version), which sit in the .rodata gap
 * between cobjectmgr.cpp's anchor (0xc002b2d4) and this file's own anchor
 * (0xc002b5d0) - i.e. per the K1-established "strings after a file's own
 * anchor belong to that file" convention, this content is nominally
 * cobjectmgr.cpp's OWN string pool, not SystemInfoHolder.cpp's. Flagged
 * as attribution-by-content-semantics (a version/health/sync-status
 * report obviously fits "SystemInfoHolder" far better than "cobjectmgr",
 * a generic object manager) rather than attribution-by-position - the
 * same kind of honest tension this project's K1 docs flag elsewhere (e.g.
 * cpsoc.c's SPI-vs-I2C register-shape note) rather than silently picking
 * one. Do not treat the function below as confirmed to physically compile
 * from SystemInfoHolder.cpp's own translation unit - it's this file's
 * best current home on functional grounds only.
 *
 * 2026-07-19 UPDATE - live Ghidra MCP follow-up (read-only:
 * get_function_info, decompile_function, get_disassembly, read_memory
 * only, no mutating calls). RESOLVED both of this file's own previously
 * open items: 0xc000a7c8 (this file's own anchor xref - a real bounds-
 * checked accessor function, see system_info_holder_get_field16() below,
 * now this file's STRONGEST confirmed real xref) and 0xc0009b0c (the
 * SYSTEM STARTUP FAILED fault-screen cluster - a real function, its full
 * text recovered verbatim via read_memory, see
 * system_info_holder_startup_failed_screen() below). Also CORRECTS
 * system_info_holder_print_status()'s own call structure - the live
 * decompile shows it is two genuinely separate steps (format-into-buffer,
 * then draw-buffer), not one combined 4-arg draw call as originally
 * modeled.
 */

#include <stdint.h>

extern void FUN_c0013824(void *ctx, const char *fmt, int a, int b);	/* real signature confirmed via live decompile - formats fmt/a/b INTO ctx's own text buffer; does NOT draw anything itself */
extern void FUN_c0012578(int col, int row, void *ctx, int flags);	/* real signature confirmed via live decompile - draws ctx's ALREADY-FORMATTED buffer at (col,row); genuinely separate step from FUN_c0013824 above */
extern int  FUN_c00068ac(void *dev);		/* @0xc00068ac - returns a value used as the "version" arg below; not identified */
extern int  FUN_c00068b8(void *dev);		/* @0xc00068b8 - returns a value used as the "revision" arg below; not identified */

/* ------------------------------------------------------------------------- *
 *  system_info_holder_print_status - FUN_c00097ac, @0xc00097ac.
 *
 *  2026-07-19 UPDATE: a real Ghidra decompile was obtained live (the
 *  static dump this file's first pass was limited to doesn't cover this
 *  address). CORRECTS the prior model: FUN_c0013824(ctx, fmt, a, b) is a
 *  "format fmt/a/b INTO ctx's own text buffer" step with NO col/row
 *  arguments at all, and FUN_c0012578(col, row, ctx, flags) is a
 *  SEPARATE "draw ctx's already-formatted buffer at col,row" step -
 *  genuinely two distinct calls in sequence, not one combined 4-arg draw
 *  call as the prior pass's hand-disassembly modeled it. Confirmed real
 *  caller: FUN_c0009838 (calls FUN_c0011eb4 then this function then
 *  FUN_c0005064 - neither further callee characterized this pass).
 *
 *  Real call sequence (both format/draw pairs confirmed via decompile +
 *  disassembly, literal-pool addresses read out directly):
 *
 *    FUN_c0013824(ctx, DAT_c0009830 /"SyncErrorCount %d"/, 1, 10);
 *    FUN_c0012578(0x14, 0x28, ctx, 0);
 *    version  = FUN_c00068ac(dev);
 *    revision = FUN_c00068b8(dev);
 *    FUN_c0013824(ctx, DAT_c0009834 /"Panel Scanner Version:%02d Revision:%02d"/, version, revision);
 *    FUN_c0012578(0x14, 0x3c, ctx, 0);
 *
 *  `ctx`/`dev` are two DIFFERENT registers in the real disassembly (r4 vs
 *  r6) - both loaded from fixed literal-pool globals at function entry
 *  (`ldr r4,[0xc0009828]` / `ldr r6,[0xc000982c]`), i.e. this function
 *  takes NO real parameters of its own - both "context" values are fixed
 *  firmware globals. The "Panel Scanner Version" call now correctly
 *  passes BOTH version and revision (previously flagged as a known
 *  simplification gap - now fixed for real, not just documented).
 * ------------------------------------------------------------------------- */
extern void *system_info_holder_ctx;		/* DAT_c0009828, fixed global, literal-pool constant at fn entry - real target address not resolved this pass */
extern void *system_info_holder_dev;		/* DAT_c000982c, fixed global, literal-pool constant at fn entry - real target address not resolved this pass */

void system_info_holder_print_status(void)
{
	void *ctx = system_info_holder_ctx;
	void *dev = system_info_holder_dev;

	/* "SyncErrorCount %d" - value/attribute args (1, 10) as seen in the
	 * live decompile; NOT confirmed whether these are genuinely constant
	 * in the real firmware or an artifact of this being one inlined call
	 * site among several. */
	FUN_c0013824(ctx, "SyncErrorCount %d", 1, 10);
	FUN_c0012578(0x14, 0x28, ctx, 0);

	int version  = FUN_c00068ac(dev);
	int revision = FUN_c00068b8(dev);
	FUN_c0013824(ctx, "Panel Scanner Version:%02d Revision:%02d", version, revision);
	FUN_c0012578(0x14, 0x3c, ctx, 0);
}

/* ------------------------------------------------------------------------- *
 *  system_info_holder_get_field16 - FUN_c000a794, @0xc000a794.
 *
 *  2026-07-19: RESOLVES this file's own "0xc000a7c8" NEEDS LIVE QUERY item
 *  - this file's own confirmed real anchor xref, now the STRONGEST
 *  evidence tying this file to a real function (upgrading from the prior
 *  "real xref, not disassembled" status). 0xc000a7c8 is a literal-pool
 *  data slot holding the "../SystemInfoHolder.cpp" anchor string address;
 *  it is referenced from inside a real function whose true entry point is
 *  0xc000a794 (confirmed via widening the disassembly window - the real
 *  APCS prologue `cpy r12,sp; cmp r1,#0x7; stmdb sp!,{r4,r5,r11,r12,lr,pc}`
 *  starts there, 0x10 bytes before the literal-pool reference at 0xc000a7b0).
 *  No Ghidra decompile is available for it (same "never function-ified"
 *  gap as this file's other functions) - hand-transcribed from raw
 *  disassembly.
 *
 *  Bounds-checked accessor: if index > 7, panel_fault(0,
 *  "../SystemInfoHolder.cpp", <line>) (the recovered `mov r2,#0x20`
 *  constant loaded before the conditional call is almost certainly the
 *  line-number argument, i.e. line 0x20 = 32 - not independently
 *  cross-checked against another confirmed line number in this file, so
 *  flagged as a reasonable but not 100%-certain read of the disassembly);
 *  otherwise returns a 16-bit field read from `base + index*2 + 4` - an
 *  8-entry table of >=6-byte-stride records, this file's own version/
 *  capability table is a plausible but unconfirmed home for it.
 * ------------------------------------------------------------------------- */
extern void panel_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 - shared assert/hard-fault handler, see panel_scan_updater.c */

uint16_t system_info_holder_get_field16(void *base, int index)
{
	if (index > 7)
		panel_fault(0, "../SystemInfoHolder.cpp", 0x20 /* recovered constant, read as line number - see header note */);

	uint8_t *p = (uint8_t *)base + index * 2;
	return *(uint16_t *)(p + 4);
}

/* ------------------------------------------------------------------------- *
 *  system_info_holder_startup_failed_screen - @0xc0009aa0.
 *
 *  2026-07-19: RESOLVES this file's own "0xc0009b0c" NEEDS LIVE QUERY
 *  item. 0xc0009b0c is a literal-pool data slot, not a function address -
 *  confirmed via read_memory(0xc0009b00, 28) to be the THIRD word of a
 *  7-word literal pool belonging to a real function at 0xc0009aa0 (no
 *  Ghidra Function object exists for it - same "never function-ified" gap
 *  as elsewhere in this pass - hand-transcribed from raw disassembly).
 *  The pool decodes to:
 *
 *    +0x00  0xc01cc104   RAM "already shown" guard-flag byte address
 *    +0x04  334          plain integer (col/coordinate arg)
 *    +0x08  453          plain integer
 *    +0x0c  0xc002b434   string pointer  <- the resolved "0xc0009b0c" item
 *    +0x10  0xc002b44c   string pointer
 *    +0x14  495          plain integer
 *    +0x18  0xc002b488   string pointer
 *
 *  read_memory(0xc002b434, 120) recovered the actual text VERBATIM:
 *
 *    "SYSTEM STARTUP FAILED"
 *    "Please turn the KRONOS off, wait, and then turn it back on."
 *    "(If any USB devices are connected, p[lease...]" (truncated by the
 *    120-byte read window - full text not recovered past this point)
 *
 *  This directly confirms the prior pass's speculation, paralleling K1's
 *  own hard-fault screen (docs/modules/KRONOS_V06R06.VSB.md's
 *  crypto_at88.c section) almost verbatim. Behavior: checks the RAM
 *  guard-flag byte first (show-once semantics - returns immediately if
 *  already shown), else sets the flag and calls FUN_c0012578 three times
 *  (once per string) with the paired integer coordinates, i.e. a 3-line
 *  fault screen using the SAME draw primitive
 *  system_info_holder_print_status() uses above.
 * ------------------------------------------------------------------------- */
/* NOTE: this function's own draw callee is declared SEPARATELY from the
 * confirmed FUN_c0012578(col,row,ctx,flags) signature above -
 * print_status()'s own FUN_c0012578 draws a PRE-FORMATTED ctx buffer, not
 * a raw string, and no decompile is available for this fault-screen
 * function to confirm whether it genuinely reuses that same callee with
 * different semantics or calls something else entirely. Modeled as its
 * own distinctly-named extern rather than force-reusing FUN_c0012578's
 * confirmed signature under an unconfirmed assumption. */
extern void panel_draw_fault_line(int arg0, int arg1, const char *text, int flags);	/* callee identity/exact signature NOT independently confirmed via decompile for this call site - inferred only from the literal-pool layout */

void system_info_holder_startup_failed_screen(void)
{
	extern uint8_t system_info_holder_shown_flag;	/* @0xc01cc104, RAM show-once guard */

	if (system_info_holder_shown_flag != 0)
		return;
	system_info_holder_shown_flag = 1;

	panel_draw_fault_line(334, 0, "SYSTEM STARTUP FAILED", 0);
	panel_draw_fault_line(453, 0, "Please turn the KRONOS off, wait, and then turn it back on.", 0);
	panel_draw_fault_line(495, 0, "(If any USB devices are connected, please...", 0);
	/* NOTE: the exact (col,row) vs (row,col) argument order for these 3
	 * calls, and the third string's full text past the 120-byte
	 * read_memory window, were not independently re-verified - the 3
	 * integer/string pairs are transcribed in the order the literal pool
	 * stores them, matching this function's own disassembly, but this is
	 * NOT claimed to be a byte-exact transcription of the real call
	 * sequence (no decompile was available to cross-check argument
	 * order or the real callee identity). Flagged rather than asserted
	 * with full confidence. */
}

/*
 * Still open, this pass:
 *  - FUN_c0013824/FUN_c0012578's real signatures are now confirmed for
 *    THIS file's own call sites, but not independently cross-checked
 *    against clcdc.c's own draw-text primitives.
 *  - system_info_holder_ctx/system_info_holder_dev's real target
 *    addresses (what device/context they point to) were not resolved -
 *    only their USE was confirmed.
 *  - system_info_holder_get_field16()'s own line-number constant (0x20)
 *    is a reasonable but not independently cross-checked read of the
 *    disassembly (see its own header note); the real register/capability
 *    map its 8-entry table describes was not identified.
 *  - system_info_holder_startup_failed_screen()'s own argument order
 *    (col/row vs row/col, and per-call additional args beyond what's
 *    shown) was not independently verified against a decompile - no
 *    Ghidra Function object exists for it. The third string's full text
 *    (past "please...") was not recovered - would need one more
 *    read_memory call past the 120-byte window already read.
 *  - The "(If any USB devices are connected...)" third fault-screen line's
 *    continuation, and any further lines beyond the 3 recovered, were not
 *    swept (the read_memory window was 120 bytes; the full cluster spans
 *    0xc002b434-0xc002b5ac per the prior pass's own citation, so more
 *    text likely exists past what was read this pass).
 */
