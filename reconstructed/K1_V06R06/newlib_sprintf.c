/* SPDX-License-Identifier: GPL-2.0 */
/*
 * newlib_sprintf.c - two-function gap-fill: FUN_c001689c (@0xc001689c, 96
 * bytes) and FUN_c00168fc (@0xc00168fc, 104 bytes), a matched
 * reentrant/non-reentrant `sprintf()` wrapper pair built on top of the
 * firmware's shared core formatter, FUN_c00169b0 (the 6680-byte function
 * newlib_dtoa_bigint.c's own header already identifies as
 * `_vfprintf_r`-shaped: "opens with `piVar4 = FUN_c001aa64(); ... =
 * *piVar4;` - fetch-then-dereference a global reentrancy struct pointer,
 * the exact classic-newlib `_impure_ptr`/`__getreent()` idiom").
 *
 * Assignment context: assigned gap cluster 0xc001689c-0xc0016968. FIVE
 * different existing files each independently `extern`-declare
 * FUN_c00168fc under a DIFFERENT guessed name/signature, none of them
 * providing a body:
 *   - crypto_at88.c:        crypto_at88_format_fault_text(dst, fmt, arg1, arg3)
 *   - cpsoc.c:               (same, cited "see crypto_at88.c")
 *   - panelbus_dispatch.c:   crypto_at88_format_fault_text  (cited from crypto_at88.c)
 *                            AND crypto_at88_format_fault_text2 (a SECOND,
 *                            locally-named extern for the SAME address,
 *                            that file's own note: "this file's own call
 *                            sites show 2 AND 4 visible arguments across
 *                            different uses - same inconsistency ... not
 *                            forced into one shape")
 *   - omap_l108.c:           cad_progress_unknown_c168fc(a, b, raw_value)
 *   - eva_crt0_tick_glue.c:  cursor_move_step(a, b, from_x, from_y)
 * FUN_c001689c has no existing citation anywhere and zero static callers
 * in the full 691-function xref data (a genuine, confirmed "zero callers"
 * case, same category as this project's other such findings, e.g.
 * crypto_at88_self_test).
 *
 * THIS NAMING SPLIT IS NOW RESOLVED, not by picking one of the five guesses,
 * but by recognizing they're all the SAME shared libc primitive used for
 * unrelated purposes: FUN_c00168fc is newlib's non-reentrant `sprintf()`
 * (through the fixed global `_impure_ptr`-equivalent, DAT_c0016964); the
 * DIFFERENT visible-argument counts every citing file already flagged as
 * "inconsistent" are exactly what a variadic printf-family function looks
 * like from different call sites with different format strings - not a
 * bug, not five different subsystem-specific functions. Flagged here as a
 * likely resolution to that cross-file discrepancy; the five citing files
 * are NOT edited (collision-avoidance rule).
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled.json
 * / all_data.json), queried via query_dump.py, 2026-07-19 pass. No live
 * Ghidra MCP calls this pass.
 *
 * Real decompiles:
 *
 *     void FUN_c001689c(undefined4 param_1,undefined1 *param_2,undefined4 param_3,undefined4 param_4)
 *     {
 *       local_64 = 0x7fffffff; local_70 = 0x7fffffff;
 *       local_6c = 0x208; local_6a = 0xffff;
 *       local_78[0] = param_2; local_68 = param_2;
 *       uStack_4 = param_4;
 *       FUN_c00169b0(param_1,local_78,param_3,&uStack_4);
 *       *local_78[0] = 0;
 *     }
 *
 *     void FUN_c00168fc(undefined1 *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)
 *     {
 *       local_68 = 0x7fffffff; local_74 = 0x7fffffff;
 *       local_70 = 0x208; local_6e = 0xffff;
 *       local_7c[0] = param_1; local_6c = param_1;
 *       uStack_8 = param_3; uStack_4 = param_4;
 *       FUN_c00169b0(*DAT_c0016964,local_7c,param_2,&uStack_8);
 *       *local_7c[0] = 0;
 *     }
 *
 * IDENTIFICATION: the stack-local "descriptor" both functions build
 * (8 bytes of dest-pointer + 0x7fffffff + 0x208 + 0xffff + dest-pointer
 * again + 0x7fffffff, 0x18/24 bytes total) and hand to FUN_c00169b0 BY
 * POINTER matches newlib's own classic `_sprintf_r()` shape closely enough
 * to type field-by-field:
 *
 *     int _sprintf_r(struct _reent *ptr, char *buf, const char *fmt, ...) {
 *         FILE f;
 *         f._flags = __SWR | __SSTR;      // matches the 0x208-valued field
 *         f._bf._base = f._p = buf;       // matches the two buf-pointer fields
 *         f._bf._size = f._w = 0x7fffffff;// matches BOTH 0x7fffffff fields
 *         f._file = -1;                   // matches the 0xffff-valued field (-1 as int16)
 *         va_start(ap, fmt);
 *         ret = _svfprintf_r(ptr, &f, fmt, ap);
 *         *f._p = 0;                      // matches the trailing NUL-terminate write
 *     }
 *
 * (reference newlib `sprintf.c` shape, cited as a reading aid per this
 * project's own established discipline for dlmalloc/dtoa - NOT license to
 * substitute reference source; the field OFFSETS/VALUES below are exactly
 * what THIS binary's decompile shows). Exact `_flags` bit values for this
 * particular newlib configuration are not independently confirmed beyond
 * "the constant 0x208 written here", so the struct below uses a plain
 * `flags` field rather than asserting `__SWR|__SSTR`'s specific bit
 * pattern.
 *
 * FUN_c001689c takes an explicit first argument (`param_1`) forwarded
 * directly as FUN_c00169b0's own first (reentrancy-struct) argument -
 * i.e. the REENTRANT variant, `_sprintf_r(reent, buf, fmt, arg)`.
 * FUN_c00168fc instead has NO such argument; it dereferences the fixed
 * global `DAT_c0016964` (resolves to 0xc0098fbc - the SAME address every
 * one of `DAT_c001a2fc`/`DAT_c001a9c4`/`DAT_c00185e4`/`DAT_c00187c0`/
 * `DAT_c0018cb0` independently resolves to, tying this file directly to
 * the newlib stdio cluster reconstructed in newlib_stdio_streams.c - one
 * single shared global reentrancy-struct pointer used image-wide) - i.e.
 * the NON-REENTRANT variant, plain `sprintf(buf, fmt, arg1, arg2)`, which
 * classic newlib implements as `_sprintf_r(_impure_ptr, buf, fmt, ...)`.
 *
 * Each wrapper only forwards as many format-value words into the
 * `uStack_*` scratch (passed to FUN_c00169b0 by address, standing in for
 * a real `va_list`) as its own real call sites use (one word for
 * FUN_c001689c, two for FUN_c00168fc) - consistent with every citing
 * file's own observation of "2 vs 4 visible arguments" being a genuine,
 * call-site-dependent variadic count, not a bug.
 *
 * FUN_c00169b0 itself (the shared `_vfprintf_r`-shaped core formatter) is
 * NOT reconstructed here - out of this file's own assigned gap, and
 * already flagged as out-of-scope by every file that calls it ("its own
 * internal shape ... not independently decoded this pass - cited by
 * address only", eva_crt0_tick_glue.c's own words). Declared here with a
 * minimal, purposely opaque signature; its real return value is discarded
 * by both wrappers in the real decompile, so declared as returning int
 * matching printf-family convention, though never captured here either.
 */

#include <stdint.h>

/* The one shared "current reentrancy struct" global pointer, confirmed by
 * address-match across five other independent citations firmware-wide
 * (crypto_at88.c/cpsoc.c/panelbus_dispatch.c/omap_l108.c/
 * eva_crt0_tick_glue.c and this file all resolve their own respective
 * DAT_ constants to the SAME runtime address, 0xc0098fbc) - see
 * newlib_stdio_streams.c for the fuller newlib-reentrancy-cluster picture. */
extern void *newlib_sprintf_impure_ptr;	/* DAT_c0016964 == 0xc0098fbc */

/* FUN_c00169b0 - the shared core formatter (`_vfprintf_r`-shaped, per
 * newlib_dtoa_bigint.c's own identification). NOT reconstructed here -
 * see header. `stream_desc` is the small on-stack "fake FILE" descriptor
 * both wrappers below build; `args` is a small on-stack scratch array
 * standing in for a real va_list, sized by each wrapper to however many
 * format values its own callers actually use. */
extern int newlib_vfprintf_core(void *reent, void *stream_desc,
				 const char *fmt, void *args);	/* FUN_c00169b0, out of scope, cited by address only */

/*
 * newlib_sprintf_r - FUN_c001689c, the reentrant `_sprintf_r`-shaped
 * variant: `_sprintf_r(reent, buf, fmt, arg)`. Zero static callers found
 * anywhere in the 691-function xref data - genuinely unreachable in this
 * image's own static call graph (or reached only via a mechanism this
 * static dump can't see), same honesty standard as this project's other
 * confirmed zero-caller findings. @0xc001689c.
 */
void newlib_sprintf_r(void *reent, char *buf, const char *fmt, uint32_t arg)	/* FUN_c001689c */
{
	struct {
		char     *p;		/* +0x00, local_78[0] - dest write pointer */
		uint32_t  pad_04;	/* +0x04, local_78[1] - never explicitly written in the real decompile */
		uint32_t  w;		/* +0x08, local_70 - 0x7fffffff, "unlimited" write-count sentinel */
		uint16_t  flags;	/* +0x0c, local_6c - 0x208 */
		int16_t   file;		/* +0x0e, local_6a - 0xffff (-1) */
		char     *buf_base;	/* +0x10, local_68 - == buf, same value as +0x00 */
		uint32_t  bf_size;	/* +0x14, local_64 - a SECOND 0x7fffffff */
	} desc;
	uint32_t args[1];

	desc.p        = buf;
	desc.w        = 0x7fffffff;
	desc.flags    = 0x208;
	desc.file     = -1;
	desc.buf_base = buf;
	desc.bf_size  = 0x7fffffff;
	args[0] = arg;

	newlib_vfprintf_core(reent, &desc, fmt, args);
	*desc.p = 0;
}

/*
 * newlib_sprintf - FUN_c00168fc, the non-reentrant `sprintf`-shaped
 * variant: `sprintf(buf, fmt, arg1, arg2)`, using the fixed global
 * reentrancy struct (`newlib_sprintf_impure_ptr`) in place of an explicit
 * `reent` argument. 12 real callers firmware-wide (crypto_at88_fault,
 * cpsoc_queue_command_with_retry, panelbus_cmd_dispatch x3,
 * cpsoc_diag_menu_input x2, panelbus_tx_send_retry, and others) - see the
 * five conflicting extern citations this file's header lists; all are
 * genuinely this same function, not five different subsystem primitives.
 * @0xc00168fc.
 */
void newlib_sprintf(char *buf, const char *fmt, uint32_t arg1, uint32_t arg2)	/* FUN_c00168fc */
{
	struct {
		char     *p;		/* +0x00, local_7c[0] - dest write pointer */
		uint32_t  pad_04;	/* +0x04, local_7c[1] - never explicitly written */
		uint32_t  w;		/* +0x08, local_74 - 0x7fffffff */
		uint16_t  flags;	/* +0x0c, local_70 - 0x208 */
		int16_t   file;		/* +0x0e, local_6e - 0xffff (-1) */
		char     *buf_base;	/* +0x10, local_6c - == buf */
		uint32_t  bf_size;	/* +0x14, local_68 - a SECOND 0x7fffffff */
	} desc;
	uint32_t args[2];

	desc.p        = buf;
	desc.w        = 0x7fffffff;
	desc.flags    = 0x208;
	desc.file     = -1;
	desc.buf_base = buf;
	desc.bf_size  = 0x7fffffff;
	args[0] = arg1;
	args[1] = arg2;

	newlib_vfprintf_core(newlib_sprintf_impure_ptr, &desc, fmt, args);
	*desc.p = 0;
}
