// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * STGGmpModule.c  -  Korg Kronos "GMP Shell" kernel glue
 *
 * STGGmp.ko is the GNU MP (GMP) bignum library, version 4.2.x, compiled as a Linux
 * 2.6.32.11 kernel module.  Almost the entire module is *stock GMP* (mpz/mpn) - see
 * README.md for how to assemble it.  This file is the only Korg-specific part: the
 * pieces that let GMP run in kernel space.
 *
 *   1. Memory allocators backed by kmalloc/ksize/kzfree, wired into GMP's global
 *      __gmp_allocate_func / __gmp_reallocate_func / __gmp_free_func pointers.
 *   2. printk + BUG() replacements for GMP's stderr/abort error paths
 *      (__gmp_assert_fail, __gmp_divide_by_zero) - i.e. GMP's assert.c / errno.c.
 *   3. A libc-compat __ctype_b_loc() shim (GMP's *_set_str use isspace/isdigit).
 *   4. Module init/exit (the original init/exit do nothing).
 *
 * Reconstructed by reverse engineering the shipping STGGmp.ko; functionally faithful.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/bug.h>
#include <linux/types.h>

/* For section 5 below: real prototypes/renaming macros for the staged
 * mpz_* / mpn_* arithmetic + generated-table symbols (gmp.h has the public
 * mpz_* / mpn_add-family API; gmp-impl.h additionally has the "internal use"
 * ones this project's own Makefile still stages -- kara_mul_n, toom3_* ,
 * mul_fft* , dc_divrem_n, sb_divrem_mn, tal-reent's tmp_reentrant_alloc+free --
 * plus the mp_bases/modlimb_invert_table extern declarations). Safe to
 * include here: same ccflags-y (-I$(src)/gmp, __GMP_WITHIN_GMP,
 * -fgnu89-inline, HAVE_CONFIG_H) as every other .c file the Makefile
 * compiles into this module, so config.h/fib_table.h/mp_bases.h resolve
 * identically. */
#include "gmp.h"
#include "gmp-impl.h"

/* ========================================================================= *
 *  1. Kernel-backed GMP memory allocators
 *
 *  GMP hook prototypes (from gmp.h):
 *      void *(*__gmp_allocate_func)   (size_t);
 *      void *(*__gmp_reallocate_func) (void *, size_t old, size_t new);
 *      void  (*__gmp_free_func)       (void *, size_t);
 * ========================================================================= */

void *do_gmp_alloc(size_t size)
{
	/* binary: __kmalloc(size, GFP_KERNEL)  (0xd0 == GFP_KERNEL) */
	return kmalloc(size, GFP_KERNEL);
}

void do_gmp_free(void *ptr, size_t size)
{
	/* size is ignored; kzfree() uses ksize() internally and scrubs the buffer */
	kzfree(ptr);
}

void *do_gmp_realloc(void *ptr, size_t old_size, size_t new_size)
{
	size_t cur = ptr ? ksize(ptr) : 0;
	void *n;

	if (new_size == 0) {		/* realloc(ptr, 0) == free */
		kzfree(ptr);
		return NULL;
	}
	if (cur >= new_size)		/* already fits in the current slab object */
		return ptr;

	n = do_gmp_alloc(new_size);
	if (n && ptr) {
		memcpy(n, ptr, cur);
		kzfree(ptr);
	}
	return n;
}

/* GMP's global allocation pointers, statically pre-wired to the kernel allocators. */
void *(*__gmp_allocate_func)(size_t) = do_gmp_alloc;
void *(*__gmp_reallocate_func)(void *, size_t, size_t) = do_gmp_realloc;
void  (*__gmp_free_func)(void *, size_t) = do_gmp_free;
EXPORT_SYMBOL(__gmp_allocate_func);
EXPORT_SYMBOL(__gmp_reallocate_func);
EXPORT_SYMBOL(__gmp_free_func);

/* ========================================================================= *
 *  2. Error paths (replace GMP's fprintf(stderr,...)+abort()).
 * ========================================================================= */

void __gmp_assert_fail(const char *filename, int linenum, const char *expr)
{
	if (filename && filename[0]) {
		printk("%s:", filename);
		if (linenum != -1)
			printk("%d: ", linenum);
	}
	printk("GNU MP assertion failed: %s\n", expr);
	BUG();				/* binary executes INT3 */
}
EXPORT_SYMBOL(__gmp_assert_fail);

void __gmp_divide_by_zero(void)
{
	printk("__gmp_divide_by_zero\n");
	BUG();
}
EXPORT_SYMBOL(__gmp_divide_by_zero);

/* ========================================================================= *
 *  3. libc-compat: __ctype_b_loc()
 *
 *  GMP's string parsers call the glibc isspace()/isdigit()/... macros, which expand
 *  to  (*__ctype_b_loc())[c] & _ISxxx .  There is no libc in the kernel, so we ship a
 *  384-entry classification table (indices -128..255) filled with glibc-compatible
 *  bit values and return a pointer biased to index 0, exactly like glibc.
 * ========================================================================= */

/* glibc bit positions, with glibc's endianness fix-up baked in. */
#define _ISbit(bit)  ((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8))
enum {
	_ISupper  = _ISbit(0),  _ISlower  = _ISbit(1),  _ISalpha = _ISbit(2),
	_ISdigit  = _ISbit(3),  _ISxdigit = _ISbit(4),  _ISspace = _ISbit(5),
	_ISprint  = _ISbit(6),  _ISgraph  = _ISbit(7),  _ISblank = _ISbit(8),
	_IScntrl  = _ISbit(9),  _ISpunct  = _ISbit(10), _ISalnum = _ISbit(11),
};

static unsigned short ctype_b_table[384];		/* [-128 .. 255] */
static const unsigned short *ctype_b_ptr = ctype_b_table + 128;

static void ctype_b_init(void)
{
	int c;

	for (c = 0; c < 256; c++) {
		unsigned short v = 0;
		int u = (c >= 'A' && c <= 'Z');
		int l = (c >= 'a' && c <= 'z');
		int d = (c >= '0' && c <= '9');
		int xd = d || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		int sp = (c == ' ' || (c >= '\t' && c <= '\r'));
		int bl = (c == ' ' || c == '\t');
		int cn = (c < 0x20 || c == 0x7f);
		int pr = (c >= 0x20 && c < 0x7f);
		int al = u || l;
		int an = al || d;
		int gr = pr && c != ' ';
		int pu = gr && !an;

		if (u)  v |= _ISupper;
		if (l)  v |= _ISlower;
		if (al) v |= _ISalpha;
		if (d)  v |= _ISdigit;
		if (xd) v |= _ISxdigit;
		if (sp) v |= _ISspace;
		if (pr) v |= _ISprint;
		if (gr) v |= _ISgraph;
		if (bl) v |= _ISblank;
		if (cn) v |= _IScntrl;
		if (pu) v |= _ISpunct;
		if (an) v |= _ISalnum;
		ctype_b_table[128 + c] = v;
	}
}

const unsigned short **__ctype_b_loc(void)
{
	return &ctype_b_ptr;
}
EXPORT_SYMBOL(__ctype_b_loc);

/* ========================================================================= *
 *  5. Export the staged GMP mpz_* / mpn_* arithmetic + generated-table
 *     symbols this module's own Makefile compiles in.
 *
 *  GAP FOUND while closing a live boot-test blocker (2026-07-11, OA.ko's
 *  own `insmod` failed with 9 unresolved `__gmpz_*` symbols): this file
 *  previously only exported its own genuinely Korg-specific glue (sections
 *  1-3 above) -- despite README.md's own "exports the full GMP C API it
 *  uses" claim, NONE of the actual mpz_* / mpn_* arithmetic functions
 *  compiled in from the staged GMP sources (see fetch-gmp.sh, Makefile's
 *  STGGmp-objs) ever had their own EXPORT_SYMBOL. They were real, defined
 *  (T) symbols inside STGGmp.o, just never added to the kernel's export
 *  table -- so no other module (OA.ko included) could ever actually link
 *  against them, exactly the failure mode the live boot test hit.
 *
 *  Exports every symbol this module's own Makefile currently stages/
 *  compiles (confirmed via a real `nm STGGmp.ko` pass, not guessed from
 *  the README's own file list) -- the public mpz_* / select-mpn_* API via
 *  gmp.h's own name-rewrite macros (`#define mpz_add __gmpz_add` etc, so
 *  EXPORT_SYMBOL(mpz_add) below genuinely exports `__gmpz_add`, matching
 *  the compiled symbol name exactly) plus the "internal use" mpn_*
 *  helpers/tables gmp-impl.h declares under the same rewrite convention
 *  (`__MPN(name)` macro). `__gmp_digit_value_tab` has no rewrite macro in
 *  either header (already named that way in dv_tab.c itself) -- hand-
 *  declared directly. `__clz_tab` (mp_clz_tab.c) is DELIBERATELY not
 *  exported here: confirmed via direct inspection to compile to ZERO
 *  bytes on x86 (guarded by `COUNT_LEADING_ZEROS_NEED_CLZ_TAB`, which
 *  x86's own longlong.h inline `bsr`-based count_leading_zeros macro never
 *  needs) -- not a bug, matches real upstream GMP behavior on this CPU
 *  family, and the real Korg-shipped x86 STGGmp.ko would have the same gap.
 * ========================================================================= */

extern const unsigned char __gmp_digit_value_tab[];

EXPORT_SYMBOL(mpz_add);
EXPORT_SYMBOL(mpz_add_ui);
EXPORT_SYMBOL(mpz_clear);
EXPORT_SYMBOL(mpz_gcdext);
EXPORT_SYMBOL(mpz_init);
EXPORT_SYMBOL(mpz_init_set_str);
EXPORT_SYMBOL(mpz_init_set_ui);
EXPORT_SYMBOL(mpz_invert);
EXPORT_SYMBOL(mpz_mul);
EXPORT_SYMBOL(mpz_mul_2exp);
EXPORT_SYMBOL(mpz_powm);
EXPORT_SYMBOL(_mpz_realloc);
EXPORT_SYMBOL(mpz_set);
EXPORT_SYMBOL(mpz_set_str);
EXPORT_SYMBOL(mpz_sub);
EXPORT_SYMBOL(mpz_tdiv_q);
EXPORT_SYMBOL(mpz_tdiv_r);

EXPORT_SYMBOL(mpn_add_n);
EXPORT_SYMBOL(mpn_addmul_1);
EXPORT_SYMBOL(mpn_submul_1);
EXPORT_SYMBOL(mpn_sub_n);
EXPORT_SYMBOL(mpn_lshift);
EXPORT_SYMBOL(mpn_rshift);
EXPORT_SYMBOL(mpn_mul);
EXPORT_SYMBOL(mpn_mul_1);
EXPORT_SYMBOL(mpn_mul_n);
EXPORT_SYMBOL(mpn_mul_basecase);
EXPORT_SYMBOL(mpn_sqr_basecase);
EXPORT_SYMBOL(mpn_set_str);
EXPORT_SYMBOL(mpn_tdiv_qr);
EXPORT_SYMBOL(mpn_divrem_1);
EXPORT_SYMBOL(mpn_divrem_2);
EXPORT_SYMBOL(mpn_gcdext);
EXPORT_SYMBOL(mpn_kara_mul_n);
EXPORT_SYMBOL(mpn_kara_sqr_n);
EXPORT_SYMBOL(mpn_toom3_mul_n);
EXPORT_SYMBOL(mpn_toom3_sqr_n);
EXPORT_SYMBOL(mpn_fft_best_k);
EXPORT_SYMBOL(mpn_fft_next_size);
EXPORT_SYMBOL(mpn_mul_fft);
EXPORT_SYMBOL(mpn_mul_fft_full);
EXPORT_SYMBOL(mpn_sb_divrem_mn);
EXPORT_SYMBOL(mpn_dc_divrem_n);
EXPORT_SYMBOL(mpn_divexact_by3c);

EXPORT_SYMBOL(__gmp_tmp_reentrant_alloc);
EXPORT_SYMBOL(__gmp_tmp_reentrant_free);
EXPORT_SYMBOL(mp_bases);		/* -> __gmpn_bases */
EXPORT_SYMBOL(modlimb_invert_table);	/* -> __gmp_modlimb_invert_table */
EXPORT_SYMBOL(__gmp_digit_value_tab);

/* ========================================================================= *
 *  4. Module init / exit  ("Korg GMP Shell")
 * ========================================================================= */

static int __init STGGmp_init(void)
{
	ctype_b_init();
	return 0;
}

static void __exit STGGmp_exit(void)
{
}

module_init(STGGmp_init);
module_exit(STGGmp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Korg GMP Shell - GNU MP 4.2.x as a Linux kernel module");
