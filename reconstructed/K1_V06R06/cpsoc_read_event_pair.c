/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cpsoc_read_event_pair.c - single-function gap-fill inside cpsoc.c's own
 * documented address range (0xc0010f00-0xc00117ff): `cpsoc_read_event_pair`
 * (FUN_c0010f60, @0xc0010f60, 76 bytes).
 *
 * Assignment context: this address was flagged "remaining" by a coverage
 * tracker for the sweep range 0xc0010f60-0xc001150c. On inspection, 16 of
 * the 17 real Ghidra function objects in that range already have genuine
 * compilable bodies - 14 in cpsoc.c, 3 in clcdc_test_dispatch.c
 * (FUN_c001120c/_c001121c/_c001123c). FUN_c0010f60 is the sole real
 * exception: cpsoc.c (see its own text around line 718, "cpsoc_read_event_pair
 * - RESOLVED/RENAMED this pass") already fully decompiled and analyzed this
 * function's real behavior in a prose/pseudocode comment, but only ever
 * emitted an `extern` forward declaration for it there - never a compilable
 * definition. Per this project's collision-avoidance rule, cpsoc.c is not
 * edited to add one; this file supplies it instead, in the same spirit as
 * clcdc_test_dispatch.c already supplying real bodies for functions cpsoc.c
 * only extern-declares.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled.json
 * / all_data.json), queried via query_dump.py, 2026-07-19 pass. No live
 * Ghidra MCP calls this pass (bridge flagged concurrency-unsafe with 2
 * concurrent agent sessions; only read-only tools would have been
 * permitted regardless, and the static dump was sufficient).
 *
 * ANCHOR: `"../cpsoc.cpp"` (same string this file's own sibling cpsoc.c
 * already anchors on, DAT_c0010fb0 here resolving to the identical
 * 0xc0023190 literal cpsoc.c cites at several of its own call sites, e.g.
 * its line ~707 `DAT_c0010d40 = 0xc0023190`). Genuinely cpsoc.cpp's own
 * code, not a "no anchor" file.
 *
 * Real decompile (query_dump.py func c0010f60):
 *
 *     void FUN_c0010f60(undefined4 param_1,undefined4 param_2,
 *                        undefined4 param_3,undefined4 param_4)
 *     {
 *       char cVar1;
 *       undefined4 uVar2;
 *
 *       uVar2 = FUN_c0001a00(DAT_c0010fac,0);
 *       cVar1 = FUN_c00033f0(uVar2,param_2,param_3,param_4);
 *       if (cVar1 != '\0') {
 *         return;
 *       }
 *       FUN_c000919c(0,DAT_c0010fb0,DAT_c0010fb4);
 *       return;
 *     }
 *
 * Symbol resolution, all cross-checked against cpsoc.c's own existing
 * text/citations for the same addresses (not guessed fresh here):
 *   - FUN_c0001a00 = cpsoc_get_scan_handle (cpsoc.c line ~665: "a fresh
 *     handle, same selector idiom" - cpsoc.c's own comment on THIS exact
 *     call site already says so). DAT_c0010fac resolves (query_dump.py dat)
 *     to -0x3ff1ff98 = 0xC00E0068 - the SAME shared "phantom dead handle"
 *     table slot soc_irq_gate.c's header extensively documents being fed,
 *     ignored, into similar handle-selector calls throughout the image;
 *     consistent with cpsoc_get_scan_handle's own other call sites in
 *     cpsoc.c also passing a value from that same table neighborhood.
 *   - FUN_c00033f0 = cpsoc_spi_submit_read (cpsoc.c line 605, real body
 *     present there) - the actual PSoC-chip read transaction: reads `len`
 *     fresh bytes at register `opcode` (here forwarded as `param_2`) into
 *     `dest` (`param_3`), returning nonzero on success.
 *   - FUN_c000919c = crypto_at88_fault (shared hard-halt/assert handler,
 *     crypto_at88.c; already extern-declared by cpsoc.c itself at its own
 *     line 20). DAT_c0010fb0 = 0xc0023190 = "../cpsoc.cpp"; DAT_c0010fb4 =
 *     0x11d = 285 decimal, matching cpsoc.c's own comment "line 285,
 *     RESOLVED this pass" for this exact fault call site. First argument
 *     is the same constant `0` every other cpsoc.c fault call site uses.
 *
 * Behavior: fetches a fresh `len`-byte record from the PSoC chip at
 * register `opcode` into `dest`, hard-faulting (never returning) if the
 * underlying SPI read transaction fails. Confirmed (via cpsoc.c's own
 * text and xrefs_to, 11 real callers) to be the actual data-fetch primitive
 * behind cpsoc_event_opcode_dispatch's fresh 2-byte read into cpsoc+0x820/
 * +0x821 before opcode routing, and behind cpsoc_led_ramp_redraw's/
 * FUN_c00116d8's own repeated 2-byte polls - i.e. genuinely a READ, not a
 * log, exactly as cpsoc.c's own "RESOLVED/RENAMED" note already concluded.
 * Not re-deriving that conclusion here, just supplying the compilable body
 * cpsoc.c's own analysis stopped short of emitting.
 *
 * STILL OPEN: the real saved-flags/interrupt-masking state around this
 * call (none is visible in the decompile itself - unlike cpsoc.c's own
 * cpsoc_event_queue_push, which does wrap irq_save_and_disable/irq_restore
 * around its own hardware access, this function does not appear to);
 * whether that's a genuine asymmetry or an artifact of inlining at this
 * call depth is not resolved by this file.
 */

#include <stdint.h>

/* --- cross-file externs, cpsoc.c --- */
extern void   *cpsoc_get_scan_handle(void *ctx, int which);		/* FUN_c0001a00, @cpsoc.c:665 */
extern int     cpsoc_spi_submit_read(void *handle, uint8_t reg, uint8_t *data, int len);	/* FUN_c00033f0, @cpsoc.c:605 */

/* --- cross-file extern, crypto_at88.c --- */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000919c */

/* the same table+0x68 "dead phantom handle" slot documented at length in
 * soc_irq_gate.c's header - passed through unused by cpsoc_get_scan_handle
 * per that file's own established pattern. */
extern uint32_t cpsoc_read_event_pair_handle_arg;	/* DAT_c0010fac, table+0x68 = 0xC00E0068 */

uint8_t cpsoc_read_event_pair(void *cpsoc_unused, int opcode, void *dest, int len)	/* FUN_c0010f60 */
{
	void *handle;

	(void)cpsoc_unused;

	handle = cpsoc_get_scan_handle((void *)(uintptr_t)cpsoc_read_event_pair_handle_arg, 0);
	if (cpsoc_spi_submit_read(handle, (uint8_t)opcode, (uint8_t *)dest, len))
		return 1;

	crypto_at88_fault(0, "../cpsoc.cpp" /* DAT_c0010fb0 = 0xc0023190 */, 0x11d);
	/* NORETURN in the real firmware (crypto_at88_fault never returns per
	 * every other call site in this project); no fall-through value is
	 * produced by the real decompile either. */
	return 0;
}
