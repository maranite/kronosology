/* SPDX-License-Identifier: GPL-2.0 */
/*
 * eva_board_main.c - K2 (KRONOS2S_V01R10.VSB) counterpart of K1_V06R06's
 * file of the same name: the panel board's real entry point (ARM reset
 * vector -> crt0 -> init-table walker -> board bring-up -> main loop).
 *
 * Ground truth: fresh Ghidra decompile of KRONOS2S_V01R10.VSB, migration
 * pass from K1_V06R06, 2026-07-18 (static dumps only, query_dump_k2.py -
 * no live Ghidra bridge this pass, per project policy on concurrent access).
 *
 * METHOD: K1's own file has NO __FILE__ string anchor for this code either
 * (the "../EvaBoardMain.cpp" string's only K1 xref sits inside
 * eva_board_watchdog_fault_wrapper, which in K2 could not be independently
 * located - see "still open" at the bottom). The K2 code range was located
 * by CODE SHAPE, not address proximity to the anchor string
 * (0xc002b218 in K2's data segment, far from any of the addresses below):
 * starting from the K2 ARM exception-vector literal pool (confirmed by
 * direct `dat` lookup of 0xc0000020, matching K1's DAT_c0000020 idiom
 * exactly) and walking the confirmed call chain down from there.
 *
 * HEADLINE FINDING, differs sharply from K1: eva_board_final_setup's own
 * K2 counterpart is called ONCE, followed immediately by a SECOND one-time
 * call (FUN_c000a58c) that structurally resembles K1's master_dispatch_tick
 * (hardware-status-bit fan-out), and ONLY THEN does the real forever-loop
 * begin - calling a much smaller, structurally DIFFERENT function
 * (FUN_c00092b4, reconstructed as master_dispatch_tick in wire_dispatch.c)
 * every iteration. K1's eva_board_start_task(1, 4) call has NO confirmed
 * K2 counterpart at all - see the "still open" section and wire_dispatch.c's
 * own header for the full analysis of what moved where.
 */

#include <stdint.h>

/* ============================================================================
 *  RESET VECTOR / CRT0 CHAIN
 * ============================================================================
 *
 *  CONFIRMED via direct `dat c0000020` lookup on the K2 dump: the ARM
 *  exception vector table's literal-pool slot at 0xC0000020 (same idiom as
 *  K1's DAT_c0000020, same offset from 0xC0000000) resolves to
 *  0xC000A860 - K2's own reset-handler target. K2's `entry` function
 *  (@0xC0000000) decompiles identically to K1's own entry: an indirect call
 *  through this one literal-pool slot.
 *
 *  eva_board_reset_handler (FUN_c000a860, @0xC000A860) - CONFIRMED as the
 *  sole callee of `entry` (xrefs_to: "c0000000 in entry (COMPUTED_JUMP)").
 *  Decompiles to the exact same "corrupted tail" shape K1 already diagnosed
 *  for FUN_c0009534/FUN_c0009540 - a real call into eva_board_crt0 (below,
 *  never returns) immediately followed by 4 bytes of unreachable literal-
 *  pool data that linear disassembly misreads as a second, self-recursive
 *  "function" (FUN_c000a86c, size 4, "WARNING: Control flow encountered bad
 *  instruction data" / "Bad instruction - Truncating control flow here" -
 *  the identical Ghidra artifact signature K1's own writeup for this exact
 *  situation already catalogued). FUN_c000a86c is NOT real code and is not
 *  cited as an extern below - same treatment K1 gave its own FUN_c0009540.
 *
 *  eva_board_crt0 (FUN_c0007268, @0xC0007268, 164 bytes) - CONFIRMED,
 *  STRUCTURALLY IDENTICAL to K1's own eva_board_crt0 (FUN_c00055b8): same
 *  exact shape, statement-for-statement - zero a BSS-style region via a
 *  start/end DAT pointer pair (DAT_c00072b0/DAT_c00072b4, K1's own
 *  DAT_c0005600/DAT_c0005604), ELEVEN back-to-back calls (K1 had exactly
 *  eleven too), set a global flag (*DAT_c0019a30 = 1, K1's *DAT_c001cb28),
 *  then fall DIRECTLY into the byte-for-byte-identical scheduler idle/
 *  dispatch tail (poll a "next ready task" global, WFI when none ready,
 *  indirect call through the ready task's own offset+0x1c function pointer
 *  when one is) - never returns. The individual eleven callees
 *  (FUN_c00071e0, FUN_c000a71c, FUN_c0000800, FUN_c0001664, FUN_c0001a44,
 *  FUN_c0019b34, FUN_c0019ab0, FUN_c0019aac, FUN_c0019d48, FUN_c00199dc,
 *  FUN_c001995c) were NOT individually traced this pass, same treatment K1
 *  gave its own eleven - none are address-adjacent to any already-ported
 *  K2 peripheral driver file discovered this pass.
 * ============================================================================
 */
extern void eva_board_crt0_subsystem_calls(void);	/* placeholder - see note: the 11 real callees are cited structurally above, not individually declared, matching K1's own treatment */

void eva_board_reset_handler(void)	/* FUN_c000a860, @0xc000a860 - CONFIRMED */
{
	extern void eva_board_crt0(void);	/* FUN_c0007268, tail-called, never returns - see below */
	eva_board_crt0();
	/* unreachable: the bytes immediately following are a literal pool
	 * misread as code (FUN_c000a86c in Ghidra's own output) - not real
	 * control flow, same artifact K1 diagnosed for its own FUN_c0009540. */
}

void eva_board_crt0(void)	/* FUN_c0007268, @0xc0007268 - CONFIRMED structurally identical to K1's FUN_c00055b8 */
{
	extern uint32_t *eva_board_crt0_bss_start, *eva_board_crt0_bss_end;	/* DAT_c00072b0/DAT_c00072b4 */
	uint32_t *p;

	for (p = eva_board_crt0_bss_start; p < eva_board_crt0_bss_end; p++)
		*p = 0;

	/* eleven subsystem/scheduler-table-constructor calls, not individually
	 * traced this pass - see header note above (FUN_c00071e0, FUN_c000a71c,
	 * FUN_c0000800, FUN_c0001664, FUN_c0001a44, FUN_c0019b34, FUN_c0019ab0,
	 * FUN_c0019aac, FUN_c0019d48, FUN_c00199dc, FUN_c001995c) */

	/* falls into the scheduler idle/dispatch tail, never returns - same
	 * WFI/indirect-dispatch idiom K1 confirmed for its own eva_board_crt0's
	 * tail and eva_board_start_task's own eva_board_sched_dispatch callee.
	 * Not transcribed further here, consistent with K1's own treatment of
	 * this dense scheduler-primitive code. */
}

/* ============================================================================
 *  eva_board_init_table / eva_board_init_table_entry_0
 * ============================================================================
 *
 *  Neither eva_board_init_table's own body nor eva_board_main's own body has
 *  a Ghidra-recognized function boundary in K2 either - the SAME situation
 *  K1's own file documents for this exact code region. What IS independently
 *  confirmed, via `callers`/xrefs_to lookups (not address-offset guessing):
 *
 *   - 0xC00072EC: a COMPUTED_CALL (indirect call through a function-pointer
 *     table entry, the same idiom as K1's eva_board_init_table walker)
 *     resolving to FUN_c000a6f8 below - this is eva_board_init_table's own
 *     dispatch instruction, CONFIRMED by xref type and target.
 *   - 0xC00072F8: an UNCONDITIONAL_CALL into FUN_c0009838
 *     (eva_board_final_setup below) - CONFIRMED.
 *   - 0xC0007300: an UNCONDITIONAL_CALL into FUN_c000a58c, a ONE-TIME
 *     status-dispatch call (see wire_dispatch.c's own header for the full
 *     analysis of why this is NOT eva_board_start_task) - CONFIRMED.
 *   - 0xC0007324: an UNCONDITIONAL_CALL into FUN_c000a6dc, CONFIRMED (via
 *     its own decompiled body, `do { FUN_c00092b4(param_1); } while(true);`)
 *     to be the real, un-exited main loop - the K2 counterpart of K1's own
 *     `for (;;) master_dispatch_tick(eva_board_handle);` tail.
 *
 *  These four addresses all sit inside the numeric span Ghidra attributes
 *  to eva_board_crt0 (0xC0007268-0xC000730C) - the SAME apparent overlap K1's
 *  own file exhibits between its crt0 (0xc00055b8-0xc000565c) and its own
 *  init-table/main addresses (0xc0005610/0xc0005644) - confirming this is a
 *  structural property of how this firmware's linear disassembly/function-
 *  boundary detection works, not a K2-specific anomaly. eva_board_init_table
 *  and eva_board_main's own PRECISE start addresses are therefore NOT
 *  independently confirmed in either image; only the confirmed call-site
 *  addresses above are asserted as fact here.
 *
 *  eva_board_init_table_entry_0 (FUN_c000a6f8, @0xC000A6F8, 20 bytes) -
 *  CONFIRMED as the init table's one real entry via the computed-call xref
 *  above. DIFFERS from K1: no lazy-singleton flag comparison, no two-step
 *  sub-call, no returned handle - just `*DAT_c000a700 = 0; return;`. A real
 *  simplification versus K1's FUN_c0009168, not a transcription gap - K2's
 *  full decompile was read in full and is genuinely this short.
 * ------------------------------------------------------------------------- */
extern uint8_t *eva_board_init_table_entry_0_target;	/* DAT_c000a700 */

void eva_board_init_table_entry_0(void)	/* FUN_c000a6f8, @0xc000a6f8 - CONFIRMED, simplified vs K1's FUN_c0009168 */
{
	*eva_board_init_table_entry_0_target = 0;
}

/* ------------------------------------------------------------------------- *
 *  eva_board_main - K2's own un-bounded counterpart of K1's eva_board_main.
 *  Body reconstructed from the four confirmed call-site addresses above
 *  (0xC00072EC/F8/0xC0007300/0xC0007324). Its own start address is NOT
 *  independently confirmed (see note above) - the sequence below begins at
 *  the first confirmed instruction (the init-table dispatch).
 *
 *  DIFFERS FROM K1 in the middle step: K1's eva_board_main called
 *  eva_board_start_task(1, 4) - a genuine priority-scheduler primitive -
 *  between eva_board_final_setup and the main loop. K2's corresponding call
 *  site (0xC0007300) instead calls FUN_c000a58c, a function taking a single
 *  HANDLE-typed argument (not `(int task_id, unsigned priority_code)`) whose
 *  own body dereferences handle fields (+4, +8, +0x2d, +0x34, +0x38, ...)
 *  the same way a board-handle consumer would, not the way K1's
 *  eva_board_start_task's own TCB-table code did. No K2 call site anywhere
 *  in this pass's data matches eva_board_start_task's (int, unsigned)
 *  calling shape - see "still open" below. FUN_c000a58c is NOT declared as
 *  eva_board_start_task here; it is described fully in wire_dispatch.c's own
 *  header, since its content structurally resembles that file's
 *  master_dispatch_tick far more than it resembles a scheduler primitive.
 * ------------------------------------------------------------------------- */
extern void eva_board_init_table(void);		/* un-bounded, walks a table via the computed call @0xc00072ec - CONFIRMED to exist, not independently disassembled */
extern void eva_board_final_setup(void *handle);	/* FUN_c0009838, @0xc0009838 - CONFIRMED, see full reconstruction below */
extern void eva_board_boot_status_dispatch(void *handle);	/* FUN_c000a58c, @0xc000a58c - CONFIRMED called once here; full body described in wire_dispatch.c, NOT eva_board_start_task (see note above) */
extern void eva_board_main_loop(void *handle);		/* FUN_c000a6dc, @0xc000a6dc - CONFIRMED: do { master_dispatch_tick(handle); } while (true); see wire_dispatch.c */

void eva_board_main(void)
{
	extern void *eva_board_handle;	/* handle pointer, real fixed address not independently resolved this pass (no DAT_ token referencing it was captured outside this un-bounded region) */

	eva_board_init_table();	/* ends in the computed call @0xc00072ec, CONFIRMED */

	eva_board_final_setup(eva_board_handle);		/* call @0xc00072f8, CONFIRMED */
	eva_board_boot_status_dispatch(eva_board_handle);	/* call @0xc0007300, CONFIRMED - NOT eva_board_start_task, see note above */

	eva_board_main_loop(eva_board_handle);	/* call @0xc0007324, CONFIRMED; internally `do { master_dispatch_tick(handle); } while (true);` - never returns */
}

/* ============================================================================
 *  eva_board_final_setup - FUN_c0009838, @0xC0009838 (236 bytes) - CONFIRMED
 * ============================================================================
 *
 *  CONFIRMED as K1's eva_board_final_setup counterpart by: (1) its sole
 *  caller sits at 0xC00072F8, an un-bounded ("None") region matching K1's
 *  own eva_board_main call-site pattern exactly; (2) one of its own callees
 *  (FUN_c0008c84 below) is called with `handle` passed directly rather than
 *  a per-subsystem DAT_ context pointer - the SAME single distinguishing
 *  trait K1's own file used to identify eva_board_compat_check among final_
 *  setup's twelve callees; (3) it ends by unconditionally setting `*handle
 *  = 1`, the identical "mark handle initialized" tail K1's own version has.
 *
 *  DIFFERS SUBSTANTIALLY from K1, all confirmed from K2's own full decompile
 *  (not inferred):
 *   - TEN subsystem-init calls here, not twelve. Several take a DAT_ context
 *     pointer with no arguments beyond that (FUN_c0011eb4, FUN_c0005064,
 *     FUN_c000a7dc, FUN_c0000c48, FUN_c00100ac, FUN_c0011010), two take an
 *     extra literal 0 (FUN_c000a8a0, FUN_c000cf20), one takes two DAT_
 *     pointers (FUN_c000e0cc(DAT_c0009944, DAT_c0009940)) - not individually
 *     attributed to any other K2 file this pass (out of this file's scope,
 *     same policy K1 applied to its own seven unattributed callees).
 *   - eva_board_compat_check (FUN_c0008c84, see below) is called via
 *     `FUN_c0008c84(param_1)` at the SOURCE level, but its own decompiled
 *     signature is `void FUN_c0008c84(void)` - a PHANTOM FORWARDED
 *     PARAMETER, the same recurring pattern this project has now found
 *     repeatedly (K1's cdix4192.c register wrappers, K1's own eva_board_
 *     watchdog_fault_wrapper). The handle argument is not actually used by
 *     the callee.
 *   - The FOUR-BANK cpsoc_i2c_dispatch(0x78/0x79/0x7b/0x7a, 0xb0) block K1's
 *     final_setup ends with is ABSENT here entirely - confirmed absent by
 *     exhaustive search of this function's own decompiled text (searched
 *     for all four bank literals together, and for the repeated-single-DAT-
 *     arg-call shape more generally; no match anywhere in the K2 function
 *     set). This board bring-up step was removed or moved elsewhere in K2.
 *   - Instead, after the ten calls, this function: zeroes ELEVEN handle
 *     fields (+8 as u32, +0x3d, +0xc, +0x2c, +0x2d, +0x3c as single bytes,
 *     +0x44/+0x45/+0x46 as single bytes, +0x48/+0x4c as u32, +4 as u32) -
 *     NONE of these offsets match K1's own final-touches field set
 *     (+0x3c=2, +0x40=0xff, +0x28=0, +0x24=0) at all, meaning K2's board
 *     handle struct layout has genuinely diverged from K1's, not just been
 *     renumbered;
 *   - calls FUN_c0001710(DAT_c0009948);
 *   - calls a NEW gate check absent from K1 entirely: `if (FUN_c0002078() ==
 *     0) FUN_c000a730(0, DAT_c000994c, 0x7a);` - FUN_c000a730 is CONFIRMED
 *     (63 callers project-wide, always a two-text-line draw then an
 *     unconditional empty infinite loop) to be K2's own crypto_at88_fault/
 *     clcdc_assert-equivalent hard-halt handler, so this is a genuinely NEW
 *     fail-fast check with no K1 counterpart. FUN_c0002078 itself is also
 *     called with a phantom/dropped argument at this call site (its own
 *     declared signature takes one int param, but the call site here passes
 *     none) - a SECOND phantom-parameter instance in this same function;
 *   - conditionally calls FUN_c00097ac(handle) when a table-indexed global
 *     (the same pointer FUN_c000a7dc above was called with) equals 2 - no
 *     K1 counterpart, not attributed further;
 *   - reads a small value via FUN_c000a728(DAT_c0009950) and stores it into
 *     a NEW handle field, `*(int *)(handle + 0x54) = value + 0x6200` - no
 *     field at this offset exists in K1's final_setup at all; 0x6200 is
 *     suggestively close to the USB register-block offsets K1's own
 *     eva_board_final_setup notes flagged near FUN_c000cdc8 (+0x6240), but
 *     this is NOT independently confirmed as USB-related here, only flagged
 *     as a lead;
 *   - finally sets `*param_1 = 1` (matches K1's own `*h = 1` tail exactly).
 * ------------------------------------------------------------------------- */
extern void *eva_board_final_setup_ctx_a;	/* DAT_c0009924 */
extern void *eva_board_final_setup_ctx_b;	/* DAT_c000992c */
extern void *eva_board_final_setup_ctx_c;	/* DAT_c0009928, passed to FUN_c000a7dc and re-read for the ==2 check */
extern void *eva_board_final_setup_ctx_d;	/* DAT_c0009930 */
extern void *eva_board_final_setup_ctx_e;	/* DAT_c0009934 */
extern void *eva_board_final_setup_ctx_f;	/* DAT_c0009938 */
extern void *eva_board_final_setup_ctx_g;	/* DAT_c000993c */
extern void *eva_board_final_setup_ctx_h;	/* DAT_c0009940 */
extern void *eva_board_final_setup_ctx_i;	/* DAT_c0009944 */
extern void *eva_board_final_setup_ctx_j;	/* DAT_c0009948 */
extern const char *eva_board_final_setup_fault_file;	/* DAT_c000994c */
extern void *eva_board_final_setup_extra_ctx;		/* DAT_c0009950, source for the new +0x54 field */

extern void eva_board_ctx_c0009924_init(void *ctx);	/* FUN_c0011eb4 */
extern void eva_board_ctx_c000992c_init(void *ctx);	/* FUN_c0005064 */
extern void eva_board_ctx_c0009928_init(void *ctx);	/* FUN_c000a7dc */
extern void eva_board_compat_check(void);	/* FUN_c0008c84, defined below - PHANTOM PARAMETER, ignores handle, see note above; real call site in K2 passes handle in r0 but the callee's own body never reads it */
extern void eva_board_ctx_c0009930_init(void *ctx);	/* FUN_c0000c48 */
extern void eva_board_ctx_c0009934_init(void *ctx);	/* FUN_c00100ac */
extern void eva_board_ctx_c0009938_init(void *ctx);	/* FUN_c0011010 */
extern void eva_board_ctx_c000993c_init(void *ctx, int unused_zero);	/* FUN_c000a8a0 */
extern void eva_board_ctx_c0009940_init(void *ctx, int unused_zero);	/* FUN_c000cf20 */
extern void eva_board_ctx_c0009944_init(void *ctx_i, void *ctx_h);	/* FUN_c000e0cc */
extern void eva_board_final_setup_tail_a(void *ctx);			/* FUN_c0001710 */
extern uint32_t eva_board_final_setup_gate_check(int unused_arg);	/* FUN_c0002078 - phantom param, real call site passes no arg */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 - CONFIRMED K2 fault/halt handler, 63 callers */
extern void eva_board_final_setup_conditional_extra(void *handle);	/* FUN_c00097ac, gated on *eva_board_final_setup_ctx_c == 2 */
extern int eva_board_final_setup_extra_read(void *ctx);		/* FUN_c000a728 */

void eva_board_final_setup(void *handle)	/* FUN_c0009838, @0xc0009838 */
{
	uint8_t *h = (uint8_t *)handle;
	int *ctx_c_ptr;

	eva_board_ctx_c0009924_init(eva_board_final_setup_ctx_a);
	eva_board_ctx_c000992c_init(eva_board_final_setup_ctx_b);
	ctx_c_ptr = (int *)eva_board_final_setup_ctx_c;
	eva_board_ctx_c0009928_init(eva_board_final_setup_ctx_c);
	eva_board_compat_check();	/* real K2 call site loads handle into r0 but the callee's own signature is void(void) - phantom parameter, see declaration above */
	eva_board_ctx_c0009930_init(eva_board_final_setup_ctx_d);
	eva_board_ctx_c0009934_init(eva_board_final_setup_ctx_e);
	eva_board_ctx_c0009938_init(eva_board_final_setup_ctx_f);
	eva_board_ctx_c000993c_init(eva_board_final_setup_ctx_g, 0);
	eva_board_ctx_c0009940_init(eva_board_final_setup_ctx_h, 0);
	eva_board_ctx_c0009944_init(eva_board_final_setup_ctx_i, eva_board_final_setup_ctx_h);

	*(uint32_t *)(h + 8) = 0;
	h[0x3d] = 0;
	h[0xc] = 0;
	h[0x2c] = 0;
	h[0x2d] = 0;
	h[0x3c] = 0;
	h[0x44] = 0;
	h[0x45] = 0;
	h[0x46] = 0;
	*(uint32_t *)(h + 0x48) = 0;
	*(uint32_t *)(h + 0x4c) = 0;
	*(uint32_t *)(h + 4) = 0;

	eva_board_final_setup_tail_a(eva_board_final_setup_ctx_j);

	if (eva_board_final_setup_gate_check(0) == 0)	/* real call site passes no arg despite the declared signature - phantom parameter */
		crypto_at88_fault(0, eva_board_final_setup_fault_file, 0x7a);

	if (*ctx_c_ptr == 2)
		eva_board_final_setup_conditional_extra(handle);

	*(int *)(h + 0x54) = eva_board_final_setup_extra_read(eva_board_final_setup_extra_ctx) + 0x6200;

	*h = 1;
}

/* ============================================================================
 *  eva_board_compat_check - FUN_c0008c84, @0xC0008C84 (56 bytes) - CONFIRMED
 * ============================================================================
 *
 *  CONFIRMED as K2's eva_board_compat_check counterpart by call position
 *  (only final_setup callee invoked with `handle` directly - see above) and
 *  by structural shape (a hardware self-test gate that draws an error and
 *  hangs forever on failure - the SAME fail-fast idiom K1's own version and
 *  crypto_at88.c's own assert handler both use). Takes NO real parameter
 *  (phantom-forwarded, see final_setup's own note above) - uses its own
 *  fixed DAT_ globals instead.
 *
 *  MUCH SIMPLER than K1's version: no four-bank bus-probe loop, no
 *  eva_board_probe_summary-style secondary message selection. Just:
 *   - pass if a global flag (DAT_c0008cbc) already equals 1, OR if a byte
 *     at a fixed context's +4 offset equals 0xFF (FUN_c00068c4's own body:
 *     `return (*(byte*)(ctx+4) - 0xff) != 0 ? 1 : 0;` i.e. TRUE means
 *     "mismatch", so the check here is really "flag==1 OR byte==0xff ->
 *     pass silently");
 *   - otherwise, draw one error box (FUN_c000685c: builds a short message
 *     via FUN_c0006ee8 with fixed coordinate-shaped literals 0xb3/0x39,
 *     then calls FUN_c0005088(ctx) to render it) and hang forever - the
 *     same genuine empty `do {} while(true);` idiom K1's version (and this
 *     project's crypto_at88.c assert handler) already use.
 * ------------------------------------------------------------------------- */
extern uint8_t eva_board_compat_flag;		/* DAT_c0008cbc */
extern void *eva_board_compat_ctx;		/* DAT_c0008cc0 */
extern int eva_board_compat_byte_check(void *ctx);	/* FUN_c00068c4 - returns 0 if byte at ctx+4 == 0xff (pass), 1 otherwise (fail) */
extern void eva_board_compat_draw_error_and_hang(void *ctx);	/* FUN_c000685c - builds+draws one error message then hangs forever internally? see note: actually only draws, hang is in THIS function per real decompile */

void eva_board_compat_check(void)	/* FUN_c0008c84, @0xc0008c84 - phantom parameter, see final_setup's own call site above */
{
	if (eva_board_compat_flag != 1 && eva_board_compat_byte_check(eva_board_compat_ctx) != 0)
		return;		/* pass: either already flagged ok, or the byte check reports a real mismatch (see note: this OR-of-conditions is transcribed exactly as decompiled, not simplified) */

	eva_board_compat_draw_error_and_hang(eva_board_compat_ctx);
	for (;;)
		;	/* confirmed genuine infinite hang, same idiom as K1's own compat_check and crypto_at88.c's FUN_c000919c/K2's FUN_c000a730 */
}

/* -------------------------------------------------------------------------
 * Still genuinely open in this file, K1->K2 migration pass, 2026-07-18:
 *
 *  - eva_board_start_task (K1's FUN_c001cfd8, a real priority-scheduler
 *    primitive) has NO confirmed K2 counterpart. The call slot in
 *    eva_board_main where K1 places `eva_board_start_task(1, 4)` is occupied
 *    in K2 by a call to FUN_c000a58c (@0xc000a58c) taking a single HANDLE
 *    argument - a calling-convention mismatch with K1's (int, unsigned)
 *    scheduler primitive that rules out a direct port. FUN_c000a58c's own
 *    content is described in wire_dispatch.c's header (it structurally
 *    resembles master_dispatch_tick, not a scheduler primitive). Whether
 *    K2 removed the priority-scheduler task-readying step entirely, folded
 *    it into eva_board_crt0's own eleven-call init sequence, or handles it
 *    via a mechanism this pass didn't locate, is NOT determined - flagged
 *    as a real gap, not fabricated. NEEDS LIVE QUERY: is there a K2 function
 *    taking exactly (int, unsigned) with a body indexing a table by a small
 *    integer id and testing a bit-0/bit-0x20 state byte, matching K1's own
 *    eva_board_start_task shape at FUN_c001cfd8? A handful of K2 functions
 *    with a similar 0x10-byte (not K1's 0x20-byte) table stride were found
 *    during this pass (FUN_c0019f30, FUN_c001a01c) but both were
 *    independently confirmed instead to be K2's wire_status_ack and
 *    eva_wire_report_code counterparts (see wire_dispatch.c) - a real,
 *    verified false trail, not left ambiguous.
 *  - eva_board_watchdog_fault_wrapper (K1's own confirmed anchor for the
 *    "../EvaBoardMain.cpp" string) has NO confirmed K2 location. The string
 *    itself IS present in K2 (0xC002B218, confirmed via `strings` lookup),
 *    but no K2 function's decompiled text references it directly (K1's own
 *    version was similarly invisible to `xrefs_to` on the string itself -
 *    it was found by raw disassembly instead, a tool not available this
 *    pass). Given eva_board_start_task's own fate is unresolved (see above),
 *    and K1's watchdog wrapper is only reachable via that same scheduler's
 *    indirect task-dispatch mechanism, its K2 status is left genuinely
 *    open rather than guessed at.
 *  - eva_board_init_table's and eva_board_main's own precise start
 *    addresses are not independently confirmed (no Ghidra function
 *    boundary in either image) - only the four call-site addresses cited
 *    above (0xC00072EC/F8, 0xC0007300, 0xC0007324) are asserted as fact.
 *  - Ten of eva_board_final_setup's own subsystem-init callees have no
 *    cross-file attribution this pass (FUN_c0011eb4, FUN_c0005064,
 *    FUN_c000a7dc, FUN_c0000c48, FUN_c00100ac, FUN_c0011010, FUN_c000a8a0,
 *    FUN_c000cf20, FUN_c000e0cc, plus FUN_c00097ac's conditional extra) -
 *    left as bare externs, matching K1's own treatment of its seven
 *    unattributed callees.
 *  - The new handle field at +0x54 (set from FUN_c000a728's return value
 *    plus 0x6200) and the entirely-absent four-bank cpsoc_i2c_dispatch
 *    block are both confirmed-real structural differences with no further
 *    attribution attempted this pass.
 * ------------------------------------------------------------------------- */
