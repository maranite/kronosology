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
