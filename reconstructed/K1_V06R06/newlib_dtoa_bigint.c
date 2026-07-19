/* SPDX-License-Identifier: GPL-2.0 */
/*
 * newlib_dtoa_bigint.c - reconstruction of the address range
 * 0xc001aa64-0xc001b9d0 (21 functions), plus one address-adjacent function
 * at 0xc001ba38 kept here for range-completeness (see its own section below).
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB (static dump
 * all_decompiled.json/all_data.json), 2026-07-19.
 *
 * NOT anchored by a `__FILE__` string - a full image string search (all 14
 * real `.cpp` anchors in this firmware, cross-checked against every existing
 * K1_V06R06 source file) found none for this range, same as heap_alloc.c's own
 * precedent: this is C-library code (newlib), not one of the firmware's own
 * instrumented C++ translation units, so it was never compiled with a
 * `__FILE__`-citing assert macro in the first place. Attribution here rests
 * on code-shape evidence instead, at a confidence level well past
 * "plausible guess" for most of this file - see below.
 *
 * This is a newlib C-runtime cluster, not a single subsystem, spanning (in
 * address order) three genuinely distinct pieces:
 *
 *   1. A reentrancy-struct accessor + a stdio FILE buffer-setup routine
 *      (0xc001aa64-0xc001abf3). Corroborated by a direct read of their one
 *      caller, FUN_c00169b0 (0xc00169b0, size 6680, OUT OF this file's
 *      range - not reconstructed here): that function's own local variable
 *      shapes (a %-format scan loop, an fp/va_list pair of parameters, an
 *      internal FILE-shaped local object with a fixed 1026-byte scratch
 *      buffer) are unmistakably `_vfprintf_r`-shaped. It opens with
 *      `piVar4 = FUN_c001aa64(); ppppppppiVar5 = *piVar4;` - fetch-then-
 *      dereference a global "reentrancy struct" pointer, the exact
 *      classic-newlib `_impure_ptr`/`__getreent()` idiom.
 *
 *   2. Three generic libc string primitives (memchr/memmove/memset,
 *      0xc001abf4-0xc001ae17) - the well-known SWAR/word-at-a-time
 *      constant-folding idioms (`+ 0xfefefeff & ~x & 0x80808080` for
 *      byte-in-word zero-detection in memchr; word-aligned fast paths with
 *      byte fallback in memmove/memset) are unambiguous, textbook glibc/
 *      newlib string.c shapes. Confirmed shared firmware-wide already (both
 *      via this pass's own caller lists below and via heap_alloc.c's own
 *      prior note that its callers "span at least 5 unrelated functions").
 *
 *   3. The full David Gay `dtoa.c`/`gdtoa` Bigint arbitrary-precision
 *      arithmetic library (0xc001ae18-0xc001b9d0, 15 functions) - the
 *      library every libc double-to-ASCII / ASCII-to-double conversion
 *      (printf %f/%e/%g, strtod/atof) is built on. Identification
 *      confidence here is very high: every function's field layout, control
 *      flow, and even literal magic constants (the `(nd+8)/9` reciprocal-
 *      multiply constant in dtoa_s2b, the exact hi0bits/lo0bits bit-ladder
 *      shape, the classic Balloc/Bfree freelist-by-k pattern) match the
 *      reference dtoa.c source structure closely enough that this is almost
 *      certainly a compiled derivative of it - noted as a reading aid, not
 *      license to substitute reference source: every claim below is
 *      grounded in what Ghidra actually shows for THIS binary, same
 *      discipline heap_alloc.c's own header already established for its
 *      dlmalloc identification.
 *
 *      The `struct dtoa_bigint` layout below (next@0x00, k@0x04,
 *      maxwds@0x08, sign@0x0c, wds@0x10, x[]@0x14) is a byte-for-byte match
 *      to the reference `Bigint` struct's own field order and is used
 *      throughout in place of raw offset casts, per this project's
 *      established convention for structures this clearly evidenced.
 *
 * Every dtoa.c-family function here threads a `handle` first argument that
 * genuinely IS used (unlike the "phantom forwarded parameter" pattern found
 * repeatedly elsewhere in this project - cdix4192.c, eva_board_main.c,
 * i2c_by_gpio.c, heap_alloc.c): dtoa_balloc reads/writes
 * `*(handle+0x4c)` as this library's own freelist-table pointer. Nothing
 * else about `handle`'s layout is confirmed - modeled as an opaque
 * `void *dtoa_handle` with one known field, not a full struct.
 *
 * The two "structural, not literal" call-outs below (dtoa_mult and
 * heap_realloc) follow this project's now-established precedent (clcdc.c's
 * clcdc_draw_edge/clcdc_blit_glyph, cobjectmgr.c's cobjectmgr_handle_type_b,
 * heap_alloc.c's heap_malloc/heap_free) for code this dense with no way to
 * verify against real hardware.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 * Shared calloc-style allocator this whole file's Bigint family uses to get
 * fresh Bigint storage. FUN_c001c98c, OUT OF this file's range - not
 * reconstructed here, only cited as an extern dependency. Confirmed (by its
 * own decompile) to be `heap_malloc(handle, nmemb*size)` immediately
 * followed by a zero-fill of the result using the exact same word-fill
 * shape as libc_memset below - i.e. a real calloc(handle, nmemb, size)
 * built on heap_alloc.c's own heap_malloc, not a separate allocator.
 * ------------------------------------------------------------------------- */
extern void *dtoa_calloc(void *handle, int32_t nmemb, int32_t size);	/* FUN_c001c98c */

/* Plain word-aligned memcpy (no overlap handling) - OUT OF this file's
 * range, distinct from libc_memmove below (which DOES handle overlap).
 * Cited only as an extern dependency of dtoa_multadd/dtoa_bigint growth. */
extern void dtoa_memcpy(void *dst, const void *src, uint32_t n);	/* FUN_c0016804 */

/* ========================================================================= *
 *  Section 1 - reentrancy accessor + stdio FILE buffer setup
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 *  newlib_getreent (FUN_c001aa64) - trivial global-pointer fetch. Its own
 *  sole caller, FUN_c00169b0 (the vfprintf-shaped function described in
 *  this file's header comment), immediately dereferences the result
 *  (`*piVar4`) before doing anything else - the classic newlib
 *  `_impure_ptr`/`__getreent()` idiom (fetch the per-task/per-process
 *  reentrancy struct pointer, then pull a field - most plausibly
 *  `_stdout`/`_stdin` - out of it). Not independently confirmed beyond that
 *  one call site. @0xc001aa64.
 * ------------------------------------------------------------------------- */
extern void *newlib_impure_ptr;	/* DAT_c001aa44 - unresolved static value (BSS, zeroed in this dump) */

void *newlib_getreent(void)	/* FUN_c001aa64 */
{
	return newlib_impure_ptr;
}

/* ------------------------------------------------------------------------- *
 *  newlib_file_smakebuf (FUN_c001aa74) - a newlib/BSD-libc stdio
 *  `__smakebuf`-shaped buffer setup routine for a FILE-like object. Field
 *  offsets on `param_1` match the classic `struct __sFILE` layout closely
 *  enough to type them directly:
 *    +0x00 _p      (current read/write pointer)
 *    +0x0c _flags  (ushort)
 *    +0x0e _file   (short, the underlying fd)
 *    +0x10 _bf._base (buffer pointer)
 *    +0x14 _bf._size (buffer size)
 *    +0x28 _seek   (a function-pointer field, compared against a fixed
 *                   constant here - plausibly "is this the standard
 *                   file-backed _seek implementation, not a custom cookie
 *                   stream")
 *    +0x43 an inline one-byte fallback buffer used when unbuffered or on
 *          allocation failure (`_nbuf`-equivalent)
 *
 *  Control flow matches `__smakebuf` almost exactly:
 *    - `_flags & 2` (the classic `__SNBF`, unbuffered) short-circuits
 *      straight to the 1-byte inline buffer.
 *    - otherwise, for a valid fd (`_file >= 0`), calls an fstat-shaped
 *      helper (`newlib_fstat_stub`, OUT OF range, see extern note below)
 *      into a local stat buffer and inspects `st_mode & 0xf000`:
 *        0x8000 (S_IFREG) + a specific `_seek` match -> sets an "optimize"
 *          flag (0x400) and skips straight to allocating a buffer.
 *        0x2000 (S_IFCHR) -> remembered as `is_char_device` for later
 *          (used below to conditionally probe `newlib_isatty_stub`).
 *        anything else / fstat failure -> falls through with the "force
 *          unbuffered" flag (0x800) set instead.
 *    - allocates a buffer via `heap_malloc(*handle, 0x400)` (1024 bytes,
 *      always - this build does not consult `st_blksize`); on failure,
 *      falls back to the inline 1-byte buffer and sets the "no buffering
 *      possible" flag (0x80); on success, sets `_bf._size = 0x400`,
 *      `_bf._base = _p = malloc'd pointer`, and - only for the S_IFCHR
 *      case above - probes `newlib_isatty_stub`, setting one more flag bit
 *      (1, line-buffered) if it returns nonzero.
 *
 *  `*handle` (DAT_c001abc0) is the SAME shared global pointer
 *  `newlib_file_free_buf` below dereferences (DAT_c001abf0, identical
 *  address) - both plausibly operate on the same default/global FILE
 *  object rather than a caller-supplied one, though no confirmed caller
 *  passes a different one in this static dump. @0xc001aa74.
 * ------------------------------------------------------------------------- */
struct newlib_file {
	void     *p;			/* +0x00 */
	uint8_t   pad_04[0x0c - 0x04];	/* +0x04, unconfirmed (_r, _w) */
	uint16_t  flags;		/* +0x0c */
	int16_t   file;			/* +0x0e */
	void     *buf_base;		/* +0x10 */
	uint32_t  buf_size;		/* +0x14 */
	uint8_t   pad_18[0x28 - 0x18];	/* +0x18, unconfirmed */
	void     *seek_fn;		/* +0x28 */
	uint8_t   pad_2c[0x43 - 0x2c];	/* +0x2c, unconfirmed */
	uint8_t   nbuf[1];		/* +0x43: inline 1-byte fallback buffer */
};

extern void      **newlib_default_file_handle;	/* DAT_c001abc0 == DAT_c001abf0: shared global, dereferenced for both heap_malloc's `handle` and (here) the "file" itself */
extern void       *newlib_seek_optimize_const;	/* DAT_c001abc4 */
extern void       *newlib_seek_std_marker;	/* DAT_c001abc8: compared against _seek to detect the standard (non-cookie) seek implementation */
extern int  newlib_fstat_stub(void *fd_owner, int fd, void *stat_buf);	/* FUN_c0015b68 - decompile shows only `*DAT_c0015b78 = 0;` in this static dump, real body likely elided/not fully resolved; NOT confirmed to be a real fstat() beyond the call-site's own error-check-on-negative convention */
extern int  newlib_isatty_stub(int fd);				/* FUN_c001c93c - unconditionally returns 1 in this dump; either a stubbed-out isatty() or dead code on this build */
extern void *heap_malloc(void *handle, uint32_t size);			/* heap_alloc.c, FUN_c0016164 */

void newlib_file_smakebuf(struct newlib_file *fp)	/* FUN_c001aa74 */
{
	uint8_t  statbuf[24];		/* auStack_5c - only the mode word (offset 4) is read */
	uint32_t st_mode;		/* local_58 */
	int      is_char_device;
	void    *newbuf;

	if (fp->flags & 2) {			/* __SNBF - caller wants unbuffered */
		fp->buf_size = 1;
		fp->buf_base = (uint8_t *)fp + 0x43;
		fp->p        = (uint8_t *)fp + 0x43;
		return;
	}

	if (fp->file < 0) {
		is_char_device = 0;
		fp->flags |= 0x800;		/* force-unbuffered: no valid fd */
	} else {
		int rc = newlib_fstat_stub(*newlib_default_file_handle, fp->file, statbuf);

		if (rc < 0) {
			is_char_device = 0;
			fp->flags |= 0x800;
		} else {
			st_mode = *(uint32_t *)(statbuf + 4);
			is_char_device = (st_mode & 0xf000) == 0x2000;

			if ((st_mode & 0xf000) == 0x8000 &&
			    fp->seek_fn == newlib_seek_std_marker) {
				fp->flags |= 0x400;
				fp->buf_size = 0x400;	/* re-set below too; matches real control flow's shared tail */
				goto alloc_buf;
			}
			fp->flags |= 0x800;
		}
	}

alloc_buf:
	newbuf = heap_malloc(*newlib_default_file_handle, 0x400);
	if (newbuf == (void *)0) {
		fp->flags |= 2;
		fp->buf_size = 1;
		fp->buf_base = (uint8_t *)fp + 0x43;
		fp->p        = (uint8_t *)fp + 0x43;
	} else {
		*(void **)((uint8_t *)fp + 0x3c) = newlib_seek_optimize_const;	/* DAT_c001abc4, exact field offset not independently confirmed */
		fp->flags   |= 0x80;
		fp->buf_base = newbuf;
		fp->buf_size = 0x400;
		fp->p        = newbuf;
		if (is_char_device && newlib_isatty_stub((int)fp->file))
			fp->flags |= 1;
	}
}

/* ------------------------------------------------------------------------- *
 *  newlib_file_free_buf (FUN_c001abe0) - one-line wrapper: frees `ptr`
 *  using the same shared default handle newlib_file_smakebuf allocates
 *  through. Two call sites, both inside FUN_c00169b0 (the vfprintf-shaped
 *  function, OUT OF this file's range) - plausibly releasing a
 *  temporarily-malloc'd conversion buffer once formatting completes.
 *  @0xc001abe0.
 * ------------------------------------------------------------------------- */
extern void heap_free(void *handle, void *ptr);	/* heap_alloc.c, FUN_c0015f30 */

void newlib_file_free_buf(void *ptr)	/* FUN_c001abe0 */
{
	heap_free(*newlib_default_file_handle, ptr);
}

/* ========================================================================= *
 *  Section 2 - generic libc string primitives
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 *  libc_memchr (FUN_c001abf4) - textbook word-at-a-time memchr: a 4-byte-
 *  aligned fast path using the classic `(x^pattern)+0xfefefeff & ~(x^pattern)
 *  & 0x80808080` zero-byte-detection trick, falling back to a byte loop for
 *  the head/tail. Fully transcribed - unambiguous. @0xc001abf4.
 * ------------------------------------------------------------------------- */
void *libc_memchr(const void *s, int c, uint32_t n)	/* FUN_c001abf4 */
{
	const uint8_t *p = (const uint8_t *)s;
	uint8_t byte = (uint8_t)c;

	if (n > 3 && ((uintptr_t)p & 3) == 0) {
		uint32_t pattern = byte | (byte << 8) | (byte << 16) | (byte << 24);

		while (n > 3) {
			uint32_t word = *(const uint32_t *)p;

			if (((word ^ pattern) + 0xfefefeffU) & ~(word ^ pattern) & 0x80808080U) {
				uint32_t i;
				for (i = 0; i < 4; i++) {
					if (p[i] == byte)
						return (void *)(p + i);
				}
			}
			n -= 4;
			p += 4;
		}
	}

	while (n != 0) {
		n--;
		if (*p == byte)
			return (void *)p;
		p++;
	}
	return (void *)0;
}

/* ------------------------------------------------------------------------- *
 *  libc_memmove (FUN_c001accc) - overlap-safe move: detects destination
 *  inside the source range and copies backward in that case, otherwise
 *  falls through to a forward word-aligned copy (16-byte unrolled, then
 *  4-byte, then byte tail) - textbook memmove. Fully transcribed.
 *  @0xc001accc.
 * ------------------------------------------------------------------------- */
void *libc_memmove(void *dst, const void *src, uint32_t n)	/* FUN_c001accc */
{
	uint8_t       *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	if (s < d && d < s + n) {
		/* Overlap with dst > src: copy backward. */
		uint8_t       *dend = d + n;
		const uint8_t *send = s + n;

		while (n-- != 0)
			*--dend = *--send;
		return dst;
	}

	if (n > 0xf && (((uintptr_t)s | (uintptr_t)d) & 3) == 0) {
		uint32_t *dw = (uint32_t *)d;
		const uint32_t *sw = (const uint32_t *)s;

		while (n > 0xf) {
			dw[0] = sw[0];
			dw[1] = sw[1];
			dw[2] = sw[2];
			dw[3] = sw[3];
			dw += 4;
			sw += 4;
			n -= 0x10;
		}
		while (n > 3) {
			*dw++ = *sw++;
			n -= 4;
		}
		d = (uint8_t *)dw;
		s = (const uint8_t *)sw;
	}

	while (n-- != 0)
		*d++ = *s++;
	return dst;
}

/* ------------------------------------------------------------------------- *
 *  libc_memset (FUN_c001ada0) - word-aligned fast fill (16-byte unrolled,
 *  then 4-byte, then byte tail), pattern replicated into all 4 bytes of a
 *  word up front. Fully transcribed. @0xc001ada0.
 * ------------------------------------------------------------------------- */
void *libc_memset(void *dst, int c, uint32_t n)	/* FUN_c001ada0 */
{
	uint8_t *d = (uint8_t *)dst;
	uint8_t  byte = (uint8_t)c;

	if (n > 3 && ((uintptr_t)d & 3) == 0) {
		uint32_t pattern = byte | (byte << 8) | (byte << 16) | (byte << 24);
		uint32_t *dw = (uint32_t *)d;

		while (n > 0xf) {
			dw[0] = pattern;
			dw[1] = pattern;
			dw[2] = pattern;
			dw += 4;
			dw[-1] = pattern;	/* matches decompile's out-of-order 3rd store */
			n -= 0x10;
		}
		while (n > 3) {
			*dw++ = pattern;
			n -= 4;
		}
		d = (uint8_t *)dw;
	}

	while (n-- != 0)
		*d++ = byte;
	return dst;
}

/* ========================================================================= *
 *  Section 3 - dtoa.c / gdtoa Bigint arbitrary-precision arithmetic
 * ========================================================================= */

struct dtoa_bigint {
	struct dtoa_bigint *next;	/* +0x00: freelist link, valid only while on a freelist */
	int32_t  k;			/* +0x04: Balloc's own size-class index */
	int32_t  maxwds;		/* +0x08: capacity of x[], = 1 << k */
	int32_t  sign;			/* +0x0c: 0 = non-negative, 1 = negative (dtoa_diff only) */
	int32_t  wds;			/* +0x10: number of valid words in x[] */
	uint32_t x[1];			/* +0x14: variable-length digit array, base 2^32, also read/written as paired 16-bit halves by the multiply-family functions below */
};

extern void *newlib_freelist_table_alloc(void *handle);	/* the DAT_ freelist-table pointer at *(handle+0x4c) is allocated lazily via dtoa_calloc(handle,4,0x10) inline in dtoa_balloc below - no separate function, kept inline rather than factored out to match the real control flow */

/* ------------------------------------------------------------------------- *
 *  dtoa_balloc (FUN_c001ae18) - classic dtoa.c `Balloc(k)`: lazily
 *  allocates this handle's own 16-entry freelist table (`*(handle+0x4c)`,
 *  16 * 4 bytes via dtoa_calloc) on first use, then either pops a free
 *  Bigint off `freelist[k]` (reusing its existing storage) or allocates a
 *  fresh one sized for `1 << k` words via dtoa_calloc. Either way, resets
 *  sign and wds to 0 before returning. Fully transcribed.
 *
 *  Ghidra's own signature shows two extra trailing parameters (param_3,
 *  param_4) that the real body never reads anywhere - the same "phantom
 *  forwarded parameter" artifact this project has repeatedly found
 *  elsewhere, here just a register-liveness artifact of ARM's entry block
 *  rather than a real 4-argument call; modeled as the real 2-argument
 *  `(handle, k)`. @0xc001ae18.
 * ------------------------------------------------------------------------- */
struct dtoa_bigint *dtoa_balloc(void *handle, int32_t k)	/* FUN_c001ae18 */
{
	struct dtoa_bigint **freelist = *(struct dtoa_bigint ***)((uint8_t *)handle + 0x4c);
	struct dtoa_bigint  *b;
	int32_t maxwds;

	if (freelist == (struct dtoa_bigint **)0) {
		freelist = (struct dtoa_bigint **)dtoa_calloc(handle, 4, 0x10);
		*(struct dtoa_bigint ***)((uint8_t *)handle + 0x4c) = freelist;
		if (freelist == (struct dtoa_bigint **)0)
			return (struct dtoa_bigint *)0;
	}

	maxwds = 1 << (k & 0xff);
	b = freelist[k];
	if (b != (struct dtoa_bigint *)0) {
		freelist[k] = b->next;
	} else {
		b = (struct dtoa_bigint *)dtoa_calloc(handle, 1, maxwds * 4 + 0x14);
		if (b == (struct dtoa_bigint *)0)
			return (struct dtoa_bigint *)0;
		b->k = k;
		b->maxwds = maxwds;
	}
	b->sign = 0;
	b->wds = 0;
	return b;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_bfree (FUN_c001aeac) - classic `Bfree(b)`: NULL-safe, pushes `b`
 *  back onto `freelist[b->k]`. Fully transcribed. @0xc001aeac.
 * ------------------------------------------------------------------------- */
void dtoa_bfree(void *handle, struct dtoa_bigint *b)	/* FUN_c001aeac */
{
	if (b != (struct dtoa_bigint *)0) {
		struct dtoa_bigint **freelist = *(struct dtoa_bigint ***)((uint8_t *)handle + 0x4c);

		b->next = freelist[b->k];
		freelist[b->k] = b;
	}
}

/* ------------------------------------------------------------------------- *
 *  dtoa_multadd (FUN_c001aec8) - classic `multadd(b, m, a)`: multiplies
 *  every word of `b` by `m` and adds a running carry, initialized to `a`;
 *  operates in 16-bit halves per word (base-65536 schoolbook multiply,
 *  avoiding a 32x32->64 widening multiply). If a carry survives past the
 *  last word, grows `b` via dtoa_balloc(k+1), then dtoa_memcpy's the
 *  CONTIGUOUS `sign`+`wds`+`x[]` tail (offset 0xc onward, `wds*4+8`
 *  bytes - deliberately NOT just `x[]` alone: the real copy's source
 *  range starts 8 bytes before `x[0]`, at `sign`, matching this struct's
 *  own contiguous field order) from the old Bigint to the new one before
 *  freeing the old one, then appends the carry as one more word.
 *  Fully transcribed. @0xc001aec8.
 * ------------------------------------------------------------------------- */
struct dtoa_bigint *dtoa_multadd(void *handle, struct dtoa_bigint *b, int32_t m, uint32_t a)	/* FUN_c001aec8 */
{
	int32_t   wds = b->wds;
	uint32_t *x = b->x;
	int32_t   i;
	uint32_t  carry = a;

	for (i = 0; i < wds; i++) {
		uint32_t lo = (uint32_t)m * (x[i] & 0xffff) + carry;
		uint32_t hi = (uint32_t)m * (x[i] >> 16) + (lo >> 16);

		x[i] = (lo & 0xffff) | (hi << 16);
		carry = hi >> 16;
	}

	if (carry != 0) {
		if (wds >= b->maxwds) {
			struct dtoa_bigint *grown = dtoa_balloc(handle, b->k + 1);

			dtoa_memcpy(&grown->sign, &b->sign, (uint32_t)(wds * 4 + 8));
			dtoa_bfree(handle, b);
			b = grown;
		}
		b->x[wds] = carry;
		b->wds = wds + 1;
	}
	return b;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_s2b (FUN_c001af98) - classic `s2b(s, nd0, nd, y9)`: converts a
 *  decimal digit string into a freshly-Balloc'd Bigint. `k` is computed
 *  from `(nd+8)/9` (via a reciprocal-multiply by DAT_c001b06c =
 *  0x38e38e39, the standard divide-by-9 magic constant) rounded up to the
 *  next power of 2, matching the reference `for (k=0,y=1; x>y; y<<=1,k++)`
 *  loop exactly. Seeds `x[0] = y9`, `wds = 1`, then folds in remaining
 *  digits via dtoa_multadd, with the reference's own "skip a decimal
 *  point at digit 9" special case (`nd0 > 9`) preserved. Fully
 *  transcribed. @0xc001af98.
 * ------------------------------------------------------------------------- */
extern int32_t dtoa_div9_recip_const;	/* DAT_c001b06c = 0x38e38e39, the reciprocal-multiply constant for (nd+8)/9 */

struct dtoa_bigint *dtoa_s2b(void *handle, const char *s, int32_t nd0, int32_t nd, uint32_t y9)	/* FUN_c001af98 */
{
	int32_t x = (int32_t)(((int64_t)dtoa_div9_recip_const * (int64_t)(nd + 8)) >> 33);
	int32_t k = 0, y = 1;
	struct dtoa_bigint *b;
	int32_t i = 9;
	const char *p;

	while (x > y) {
		y <<= 1;
		k++;
	}

	b = dtoa_balloc(handle, k);
	b->x[0] = y9;
	b->wds = 1;

	if (nd0 < 10) {
		p = s + 10;
	} else {
		p = s + 9;
		do {
			b = dtoa_multadd(handle, b, 10, (uint32_t)(*p - '0'));
			i++;
			p++;
		} while (i < nd0);
		p++;	/* skip the decimal point */
	}

	for (; i < nd; i++) {
		b = dtoa_multadd(handle, b, 10, (uint32_t)(*p - '0'));
		p++;
	}
	return b;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_hi0bits (FUN_c001b070) - classic `hi0bits(x)`: counts leading
 *  zero bits in a 32-bit word via the standard binary-search bit-ladder
 *  (16/8/4/2/1). Fully transcribed, exact match to the reference shape.
 *  @0xc001b070.
 * ------------------------------------------------------------------------- */
int32_t dtoa_hi0bits(uint32_t x)	/* FUN_c001b070 */
{
	int32_t k = 0;

	if ((x & 0xffff0000U) == 0) {
		k = 16;
		x <<= 16;
	}
	if ((x & 0xff000000U) == 0) {
		x <<= 8;
		k += 8;
	}
	if ((x & 0xf0000000U) == 0) {
		x <<= 4;
		k += 4;
	}
	if ((x & 0xc0000000U) == 0) {
		x <<= 2;
		k += 2;
	}
	if ((int32_t)x >= 0) {
		k++;
		if ((x & 0x40000000U) == 0)
			return 0x20;
	}
	return k;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_lo0bits (FUN_c001b0cc) - classic `lo0bits(*y)`: counts trailing
 *  zero bits, shifting `*y` right by that count in place (0 if the input
 *  is already odd). Fully transcribed, exact match to the reference
 *  shape (including its own "low 3 bits nonzero" fast-path special case).
 *  @0xc001b0cc.
 * ------------------------------------------------------------------------- */
int32_t dtoa_lo0bits(uint32_t *y)	/* FUN_c001b0cc */
{
	uint32_t x = *y;
	int32_t  k;

	if (x & 7) {
		if (x & 1)
			return 0;
		if (x & 2) {
			*y = x >> 1;
			return 1;
		}
		*y = x >> 2;
		return 2;
	}

	k = 0;
	if ((x & 0xffffU) == 0) {
		x >>= 16;
		k = 16;
	}
	if ((x & 0xffU) == 0) {
		x >>= 8;
		k += 8;
	}
	if ((x & 0xfU) == 0) {
		x >>= 4;
		k += 4;
	}
	if ((x & 3U) == 0) {
		x >>= 2;
		k += 2;
	}
	if ((x & 1U) == 0) {
		x >>= 1;
		k++;
		if (x == 0)
			return 0x20;
	}
	*y = x;
	return k;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_i2b (FUN_c001b160) - classic `i2b(i)`: Balloc(1), x[0]=i, wds=1.
 *  Fully transcribed. @0xc001b160.
 * ------------------------------------------------------------------------- */
struct dtoa_bigint *dtoa_i2b(void *handle, int32_t i)	/* FUN_c001b160 */
{
	struct dtoa_bigint *b = dtoa_balloc(handle, 1);

	b->wds = 1;
	b->x[0] = (uint32_t)i;
	return b;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_mult (FUN_c001b188) - classic `mult(a, b)`: full bignum multiply,
 *  swapping operands so the shorter one drives the outer loop, then a
 *  16-bit-half schoolbook double loop identical in shape to
 *  dtoa_multadd's own per-word arithmetic. Documented STRUCTURALLY rather
 *  than transcribed line-for-line (per this project's established
 *  practice for code this dense with no way to verify against real
 *  hardware - same treatment as heap_alloc.c's heap_malloc/heap_free):
 *
 *    - result size = shorter->wds + longer->wds (+1 word if the sum
 *      could overflow the top digit), Balloc'd and zero-filled up front.
 *    - outer loop walks the shorter operand one 32-bit word at a time,
 *      split into its low and high 16-bit halves; each half (if nonzero)
 *      drives an inner loop over the ENTIRE longer operand, accumulating
 *      into the result at the appropriate word offset with carry
 *      propagation through 16-bit temporaries stored via `short*` writes
 *      (matching dtoa_multadd's own halves-in-a-32-bit-word convention).
 *    - trailing zero words are trimmed from the result's `wds` before
 *      returning.
 *
 *  @0xc001b188.
 * ------------------------------------------------------------------------- */
struct dtoa_bigint *dtoa_mult(void *handle, struct dtoa_bigint *a, struct dtoa_bigint *b);	/* FUN_c001b188, structure only - not transcribed, see above */

/* ------------------------------------------------------------------------- *
 *  dtoa_pow5mult (FUN_c001b378) - classic `pow5mult(b, k)`: multiplies
 *  `b` by 5^k, using a cached table of `5^(2^n)` Bigints
 *  (`*(handle+0x48)`, lazily seeded to 5^1=625... - actually 0x271=625
 *  decimal, i.e. 5^4 - the reference dtoa.c's own `p5s` table's first
 *  entry) built up on demand via dtoa_mult, walking the bits of `k` from
 *  LSB to MSB and squaring the running power-of-5 each step (the standard
 *  binary-exponentiation shape). The low 2 bits of `k` are handled first
 *  via a small fixed lookup table of `5^1..5^3` (`DAT_c001b488`) folded in
 *  through one dtoa_multadd call.
 *
 *  Fully transcribed, but re-expressed in the classic reference's own
 *  for(;;)-loop shape rather than Ghidra's own while/goto/label soup for
 *  this same loop - control-flow-equivalent (checked bit-by-bit against
 *  the decompile's own LAB_c001b400 entry/exit points below), far more
 *  readable: check the current low bit of the (already >>2'd) `k`,
 *  multiply `b` by the running power-of-5 if set, shift `k` right,
 *  break once it's zero, else lazily square the running power-of-5
 *  (caching the result into its own `next` field so repeat calls at the
 *  same exponent don't redo the work) and advance to it. @0xc001b378.
 * ------------------------------------------------------------------------- */
extern uint32_t dtoa_pow5_small_table[3];	/* DAT_c001b488: {5, 25, 125} - dtoa_multadd's multiplier for (k & 3) == 1/2/3 */

struct dtoa_bigint *dtoa_pow5mult(void *handle, struct dtoa_bigint *b, uint32_t k)	/* FUN_c001b378 */
{
	struct dtoa_bigint **p5s_slot;
	struct dtoa_bigint  *p5;

	if (k & 3)
		b = dtoa_multadd(handle, b, (int32_t)dtoa_pow5_small_table[(k & 3) - 1], 0);

	k >>= 2;
	if (k == 0)
		return b;

	p5s_slot = (struct dtoa_bigint **)((uint8_t *)handle + 0x48);
	p5 = *p5s_slot;
	if (p5 == (struct dtoa_bigint *)0) {
		p5 = dtoa_i2b(handle, 0x271);	/* 625 = 5^4 */
		*p5s_slot = p5;
		p5->next = (struct dtoa_bigint *)0;
	}

	for (;;) {
		if (k & 1) {
			struct dtoa_bigint *b1 = dtoa_mult(handle, b, p5);

			dtoa_bfree(handle, b);
			b = b1;
		}
		k >>= 1;
		if (k == 0)
			break;
		if (p5->next == (struct dtoa_bigint *)0) {
			struct dtoa_bigint *squared = dtoa_mult(handle, p5, p5);

			p5->next = squared;
			squared->next = (struct dtoa_bigint *)0;
		}
		p5 = p5->next;
	}
	return b;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_lshift (FUN_c001b48c) - classic `lshift(b, k)`: shifts `b` left
 *  by `k` bits (word shift `k>>5` plus a sub-word bit shift `k&0x1f`),
 *  growing into a freshly Balloc'd result sized for the extra word(s),
 *  freeing the original. Fully transcribed. @0xc001b48c.
 * ------------------------------------------------------------------------- */
struct dtoa_bigint *dtoa_lshift(void *handle, struct dtoa_bigint *b, uint32_t k)	/* FUN_c001b48c */
{
	int32_t   word_shift = (int32_t)(k >> 5);
	int32_t   bit_shift = (int32_t)(k & 0x1f);
	int32_t   src_wds = b->wds;
	int32_t   dst_wds = src_wds + word_shift + 1;
	struct dtoa_bigint *r;
	uint32_t *dst;
	uint32_t *src = b->x;
	int32_t   i;
	int32_t   final_wds = dst_wds;

	{
		/* Balloc(k') sizing ladder, matches dtoa_balloc's own shape. */
		int32_t rk = b->k;
		int32_t cap = b->maxwds;
		while (cap < dst_wds) {
			rk++;
			cap <<= 1;
		}
		r = dtoa_balloc(handle, rk);
	}

	dst = r->x;
	for (i = 0; i < word_shift; i++)
		dst[i] = 0;
	dst += word_shift;

	if (bit_shift == 0) {
		for (i = 0; i < src_wds; i++)
			dst[i] = src[i];
	} else {
		uint32_t carry = 0;

		for (i = 0; i < src_wds; i++) {
			dst[i] = carry | (src[i] << bit_shift);
			carry = src[i] >> (32 - bit_shift);
		}
		dst[i] = carry;
		if (carry != 0)
			final_wds = dst_wds + 1;
	}

	r->wds = final_wds - 1;
	dtoa_bfree(handle, b);
	return r;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_cmp (FUN_c001b588) - classic `cmp(a, b)`: compares word counts
 *  first (short-circuit), else compares words from most-significant down.
 *  Fully transcribed. @0xc001b588.
 * ------------------------------------------------------------------------- */
int32_t dtoa_cmp(struct dtoa_bigint *a, struct dtoa_bigint *b)	/* FUN_c001b588 */
{
	int32_t wds_diff = a->wds - b->wds;
	int32_t i;

	if (wds_diff != 0)
		return wds_diff;

	for (i = a->wds - 1; i >= 0; i--) {
		if (a->x[i] != b->x[i])
			return a->x[i] < b->x[i] ? -1 : 1;
	}
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_diff (FUN_c001b5dc) - classic `diff(a, b)`: computes |a-b| as a
 *  new Bigint, recording which operand was larger via dtoa_cmp into the
 *  result's own `sign` field (this is the one function in this cluster
 *  that actually uses `sign` for anything). Zero special-case returns a
 *  fresh zero-valued Bigint rather than reusing either operand. Fully
 *  transcribed. @0xc001b5dc.
 * ------------------------------------------------------------------------- */
struct dtoa_bigint *dtoa_diff(void *handle, struct dtoa_bigint *a, struct dtoa_bigint *b)	/* FUN_c001b5dc */
{
	int32_t cmp = dtoa_cmp(a, b);
	struct dtoa_bigint *big, *small_, *r;
	int32_t i, borrow = 0;
	uint16_t *dst;
	uint32_t *bx, *sx;
	int32_t big_wds, small_wds, wds;

	if (cmp == 0) {
		r = dtoa_balloc(handle, 0);
		r->wds = 1;
		r->x[0] = 0;
		return r;
	}

	if (cmp < 0) {
		big = b;
		small_ = a;
	} else {
		big = a;
		small_ = b;
	}

	r = dtoa_balloc(handle, big->k);
	r->sign = cmp < 0;

	big_wds = big->wds;
	small_wds = small_->wds;
	bx = big->x;
	sx = small_->x;
	dst = (uint16_t *)r->x;

	for (i = 0; i < small_wds; i++) {
		int32_t lo = (int32_t)(bx[i] & 0xffff) - (int32_t)(sx[i] & 0xffff) + borrow;
		int32_t hi = (int32_t)(bx[i] >> 16) - (int32_t)(sx[i] >> 16) + (lo >> 16);

		*dst++ = (uint16_t)lo;
		*dst++ = (uint16_t)hi;
		borrow = hi >> 16;
	}
	for (wds = big_wds; i < wds; i++) {
		int32_t lo = (int32_t)(bx[i] & 0xffff) + borrow;
		int32_t hi = (int32_t)(bx[i] >> 16) + (lo >> 16);

		*dst++ = (uint16_t)lo;
		*dst++ = (uint16_t)hi;
		borrow = hi >> 16;
	}

	/* Trim leading (most-significant) zero words. */
	{
		int32_t *top = (int32_t *)dst - 1;

		while (*top == 0) {
			top--;
			big_wds--;
		}
	}
	r->wds = big_wds;
	return r;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_b2d (FUN_c001b778) - classic `b2d(a, *e)`: extracts the top ~53
 *  bits of a Bigint into an IEEE double's raw 64-bit bit pattern (packed
 *  into the r0:r1 register pair per AAPCS - modeled here as a `uint64_t`
 *  return), OR-ing in the fixed exponent-field bias constant `0x3ff00000`
 *  in the high word (i.e. returns a value whose real exponent still needs
 *  the caller, dtoa_ratio, to add `*e`). Fully transcribed. @0xc001b778.
 * ------------------------------------------------------------------------- */
uint64_t dtoa_b2d(struct dtoa_bigint *a, int32_t *e)	/* FUN_c001b778 */
{
	int32_t   wds = a->wds;
	uint32_t *x = a->x;
	uint32_t  y = x[wds - 1];			/* top (most-significant) word */
	int32_t   k = dtoa_hi0bits(y);
	uint32_t  w = (wds > 1) ? x[wds - 2] : 0;	/* 2nd-from-top word, 0 if none */
	uint32_t  word0, word1;

	*e = 32 - k;

	if (k < 11) {
		word0 = (y >> (11 - k)) | 0x3ff00000U;
		word1 = (y << (32 - 11 + k)) | (w >> (11 - k));
	} else {
		uint32_t shift = (uint32_t)(k - 11);

		if (shift != 0) {
			uint32_t z = (wds > 2) ? x[wds - 3] : 0;	/* 3rd-from-top word, 0 if none */

			word0 = (y << shift) | (w >> (32 - shift)) | 0x3ff00000U;
			word1 = (w << shift) | (z >> (32 - shift));
		} else {
			word0 = y | 0x3ff00000U;
			word1 = w;
		}
	}

	/* Packed as word0 | (word1 << 32) - i.e. the LOW 32 bits of the
	 * returned 64-bit value are word0 (the exponent-biased top word, the
	 * expression this function's own visible decompile actually returns -
	 * confirmed against dtoa_ratio's caller-side use: `(int)` truncation
	 * of this call's result is what receives the exponent correction
	 * below, and `(int)` truncation means the LOW 32 bits per AAPCS
	 * r0:r1 = low:high register convention). The HIGH 32 bits (word1,
	 * the second mantissa word) are read via `>> 0x20` at the same call
	 * site - not independently observable from this function's own
	 * decompile (Ghidra's local per-function analysis only detected the
	 * r0-returning expression), but required for FUN_c001ebe8's own
	 * 4-word calling convention to make sense. */
	return (uint64_t)word0 | ((uint64_t)word1 << 32);
}

/* ------------------------------------------------------------------------- *
 *  dtoa_d2b (FUN_c001b82c) - classic `d2b(d, *e, *bits)`: the reverse of
 *  dtoa_b2d - unpacks an IEEE double's raw bit pattern (`param_2`:`param_3`
 *  as hi:lo 32-bit halves, per AAPCS) into a freshly Balloc'd Bigint,
 *  writing the *unbiased* binary exponent to `*e` and the significant bit
 *  count to `*bits`. Handles the denormal case (`biased exponent == 0`)
 *  by normalizing via dtoa_lo0bits instead of assuming an implicit leading
 *  1 bit. Fully transcribed. @0xc001b82c.
 * ------------------------------------------------------------------------- */
struct dtoa_bigint *dtoa_d2b(void *handle, uint32_t d_hi, uint32_t d_lo, int32_t *e, int32_t *bits)	/* FUN_c001b82c */
{
	struct dtoa_bigint *b = dtoa_balloc(handle, 1);
	uint32_t biased_exp = (d_hi & 0x7fffffffU) >> 20;
	uint32_t frac_hi = d_hi & 0xfffffU;
	int32_t  wds;
	uint32_t top_word;

	if (biased_exp != 0)
		frac_hi |= 0x100000U;

	if (d_lo == 0) {
		int32_t lo_bits = dtoa_lo0bits(&frac_hi);

		b->x[0] = frac_hi;
		b->wds = 1;
		wds = 1;
		top_word = (uint32_t)lo_bits + 0x20;
	} else {
		uint32_t lo = d_lo;
		int32_t  lo_bits = dtoa_lo0bits(&lo);

		if (lo_bits == 0) {
			b->x[0] = lo;
		} else {
			b->x[0] = lo | (frac_hi << (32 - lo_bits));
			frac_hi >>= lo_bits;
		}
		wds = (frac_hi == 0) ? 1 : 2;
		b->x[1] = frac_hi;
		b->wds = wds;
		top_word = (uint32_t)lo_bits;
	}

	if (biased_exp == 0) {
		*e = (int32_t)top_word - 0x432;
		*bits = wds * 0x20 - dtoa_hi0bits(b->x[wds - 1]);
	} else {
		*e = (int32_t)(biased_exp + top_word) - 0x433;
		*bits = 0x35 - (int32_t)top_word;
	}
	return b;
}

/* ------------------------------------------------------------------------- *
 *  dtoa_ratio (FUN_c001b950) - classic `ratio(a, b)`: computes a/b as a
 *  real double by converting each Bigint to a double mantissa via
 *  dtoa_b2d, adjusting whichever operand's exponent field is smaller so
 *  the two are exponent-aligned (adding the word-count difference scaled
 *  by 32 bits into the raw exponent-field units of `0x100000` = 2^20 per
 *  step, matching an IEEE double's exponent field position), then calls
 *  a raw software double-divide primitive (`FUN_c001ebe8`, size 620,
 *  4x uint32 args / uint64 return - OUT OF this file's range, plausibly a
 *  soft-float `__aeabi_ddiv`-shaped routine, NOT independently confirmed)
 *  to actually perform the division. Fully transcribed except for that
 *  one opaque extern. @0xc001b950.
 * ------------------------------------------------------------------------- */
extern uint64_t dtoa_softfloat_ddiv(uint32_t num_hi, uint32_t num_lo, uint32_t den_hi, uint32_t den_lo);	/* FUN_c001ebe8 */

uint64_t dtoa_ratio(struct dtoa_bigint *a, struct dtoa_bigint *b)	/* FUN_c001b950 */
{
	int32_t  ea, eb;
	uint64_t da = dtoa_b2d(a, &ea);
	uint64_t db = dtoa_b2d(b, &eb);
	int32_t  k = (ea - eb) + (a->wds - b->wds) * 0x20;
	/* Per dtoa_b2d's own packing note: low 32 bits = word0 (exponent),
	 * high 32 bits = word1 (mantissa). */
	uint32_t da_word0 = (uint32_t)da,        da_word1 = (uint32_t)(da >> 32);
	uint32_t db_word0 = (uint32_t)db,        db_word1 = (uint32_t)(db >> 32);

	if (k > 0)
		da_word0 += (uint32_t)k * 0x100000U;
	else
		db_word0 += (uint32_t)(-k) * 0x100000U;

	return dtoa_softfloat_ddiv(da_word0, da_word1, db_word0, db_word1);
}

/* ========================================================================= *
 *  Cross-file note - FUN_c001ba38 (heap_realloc), 0xc001ba38
 * ========================================================================= *
 *
 * This task's assigned range nominally ends at 0xc001ba38, and a real
 * function DOES start exactly there (1212 bytes) - but it is NOT part of
 * this file's dtoa/newlib cluster. It is `realloc()`, a third dlmalloc
 * entry point alongside heap_alloc.c's own heap_malloc (FUN_c0016164) and
 * heap_free (FUN_c0015f30): it calls both of those directly, plus
 * heap_lock/heap_unlock (FUN_c0016894/FUN_c0016898), and reads a `heap_state`
 * -shaped global (DAT_c001bef4) whose top/wilderness-chunk-at-offset-8
 * access pattern is identical in shape to heap_alloc.c's own heap_trim.
 *
 * Kept OUT of this file (wrong subsystem) and NOT added to heap_alloc.c
 * either, per this task's explicit instruction not to edit existing files.
 * Recommended follow-up: reconstruct FUN_c001ba38 as `heap_realloc` inside
 * heap_alloc.c itself (structurally documented, not transcribed literally,
 * matching that file's own precedent for heap_malloc/heap_free - the
 * in-place grow/shrink-into-neighboring-free-chunk logic here is at least
 * as dense as either of those). Its own single caller is FUN_c001a534,
 * itself uncharacterized and out of both this range and any existing file.
 */
