/* SPDX-License-Identifier: GPL-2.0 */
/*
 * clcdc.c - the NKS4 panel firmware's LCD controller driver: raw hardware
 * register access, a drawing-cursor abstraction, line/box primitives, a
 * proportional bitmap-font text renderer, a built-in multi-mode test-pattern
 * generator, and a fixed-point progress-bar drawer.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: the literal "../clcdc.cpp" string (only one xref in the whole image,
 * inside crypto_at88_self_test-style code's sibling assert call in the test-
 * pattern generator below) confirms this address range is the real
 * compilation unit. See this project's README.md for the full function list
 * and status.
 *
 * CORRECTION, same pass: the address range immediately following this
 * subsystem (0xc0015bf8 onward) turned out NOT to be clcdc.cpp - it's a
 * generic segregated-free-list heap allocator (malloc/free/sbrk-style, with
 * free-block coalescing and size-class binning) plus a C++ object destructor
 * that happens to call into it, both shared firmware-wide runtime code, not
 * LCD-specific. An earlier working assumption that the whole 0xc0015010-
 * 0xc0015f30+ range was one unit was wrong; this file only contains what's
 * actually confirmed to be the LCD subsystem.
 *
 * CORRECTION (coverage-sweep pass, 2026-07-18): the exclusion above didn't
 * go far enough. A full `range` sweep of 0xc0015010-0xc0015bf8 turned up
 * FOUR more functions (0xc0015b68, 0xc0015b8c, 0xc0015bb4, 0xc0015bc8) sitting
 * in the gap between clcdc_blit_glyph's real end (0xc0015afc) and the
 * heap allocator's confirmed 0xc0015bf8 start. All four are FALSE PROXIMITY,
 * the exact same mistake already caught once for the heap allocator itself:
 * three are trivial one-line "zero a global" stubs whose only callers
 * (0xc001aa74, 0xc001bffc/0xc001c030, 0xc001bf54) all live deep past
 * 0xc0019000 - nowhere near any confirmed clcdc.cpp code - and the fourth
 * (0xc0015bc8) is a self-recursive helper that calls FUN_c0015f30 directly,
 * which is already inside the documented heap allocator. Real clcdc.cpp code
 * ends at clcdc_blit_glyph (0xc0015afc); 0xc0015afc-0xc0015bf7 is a mix of
 * clcdc's own trailing data globals and this other unit's boundary
 * functions, not LCD-specific. Excluded from this file accordingly. The
 * sweep also turned up ONE genuine clcdc.cpp function this file was missing
 * - see clcdc_dispatch_set_palette_hook below.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Raw LCD controller register access - a classic set/or-bits/and-bits(clear)
 *  trio over a base-pointer-plus-8-bit-offset register file. @0xc0015094/
 *  0xc00150a4/0xc00150bc.
 * ------------------------------------------------------------------------- */
struct clcdc_regs { uint32_t *base; /* real layout beyond this not traced */ };

void clcdc_reg_write(struct clcdc_regs *ctl, uint32_t reg_offset, uint32_t value)	/* FUN_c0015094 */
{
	*(uint32_t *)((uint8_t *)ctl->base + (reg_offset & 0xff)) = value;
}
void clcdc_reg_set_bits(struct clcdc_regs *ctl, uint32_t reg_offset, uint32_t mask)	/* FUN_c00150a4 */
{
	uint32_t *r = (uint32_t *)((uint8_t *)ctl->base + (reg_offset & 0xff));
	*r |= mask;
}
void clcdc_reg_clear_bits(struct clcdc_regs *ctl, uint32_t reg_offset, uint32_t mask)	/* FUN_c00150bc */
{
	uint32_t *r = (uint32_t *)((uint8_t *)ctl->base + (reg_offset & 0xff));
	*r &= mask;
}

/* ------------------------------------------------------------------------- *
 *  Drawing cursor - a small state struct: x (+8), y (+10), left-margin/wrap
 *  column (+0xc), right-edge/wrap-trigger column (+0xe). Real struct also has
 *  a row-stride field at +4 (used by clcdc_cursor_set_from_offset below).
 *  @0xc0015010 (set field+4), 0xc0015028 (init cursor), 0xc001505c (init
 *  cursor from a linear framebuffer offset, dividing by the stride at +4).
 * ------------------------------------------------------------------------- */
struct clcdc_cursor {
	uint8_t  pad0[4];
	uint32_t stride;		/* +4: row width for offset->(x,y) conversion */
	uint8_t  pad1[2];
	int16_t  x, y;			/* +8, +10 */
	int16_t  left_margin;		/* +0xc: x to wrap back to */
	int16_t  right_edge;		/* +0xe: x that triggers a wrap */
};

void clcdc_cursor_set_stride(struct clcdc_cursor *c, uint32_t stride)	/* FUN_c0015010 */
{
	c->stride = stride;
}

void clcdc_cursor_init(struct clcdc_cursor *c, int16_t x, int16_t y, int width)	/* FUN_c0015028 */
{
	c->x = x;
	if (width != 0)
		width -= 1;
	c->y = y;
	c->right_edge = (int16_t)(c->x + width);
	c->left_margin = c->x;
}

/* Set the cursor from a linear pixel offset (offset = y*stride + x) rather
 * than explicit (x, y). @0xc001505c */
void clcdc_cursor_init_from_offset(struct clcdc_cursor *c, uint32_t offset, int width)
{
	uint32_t stride = c->stride;
	int16_t row = 0;

	while (stride <= offset) {
		offset -= stride;
		row++;
	}
	clcdc_cursor_init(c, (int16_t)offset, row, width);
}

/* ------------------------------------------------------------------------- *
 *  clcdc_dispatch_set_palette_hook - found this pass (coverage sweep,
 *  2026-07-18, item 5) while confirming every function in the anchored
 *  clcdc.cpp address range has a real reconstructed body: this one was
 *  simply missed before. It sits at 0xc0015018, physically BETWEEN
 *  clcdc_cursor_set_stride (0xc0015010) and clcdc_cursor_init (0xc0015028)
 *  in a gap-free instruction stream, so it belongs to this compilation unit
 *  despite not fitting any of the categories above or below it.
 *
 *  Its own body is a thin one-line wrapper: `FUN_c00037b0(*param_1)`.
 *  FUN_c00037b0 (0xc00037b0 - NOT part of clcdc.cpp, well outside this
 *  address range, owning file not identified) is a 5-argument RGB->RGB565
 *  palette-entry-set primitive: `lut[index] = (g>>2)<<5 | (b>>3)<<11 |
 *  (r>>3)`. This wrapper's sole caller is FUN_c0007d1c - cpsoc.c's own
 *  central host-command dispatcher (see cpsoc.c, not edited here; confirmed
 *  by re-checking FUN_c0007d1c's own decompile this pass) - so this is
 *  almost certainly a "set a custom palette colour" opcode handler that
 *  clcdc registers with that shared dispatcher.
 *
 *  CAUTION: the decompile shows only ONE argument reaching a 5-parameter
 *  callee (`FUN_c00037b0(*param_1)`), which is very likely a Ghidra
 *  argument-resolution artifact (param_1 probably points at a packed
 *  {handle, index, r, g, b}-style block the decompiler didn't fully unpack)
 *  rather than a real "4 arguments silently dropped" call. Left as a
 *  documented contract rather than a guessed 5-argument transcription, same
 *  treatment as clcdc_draw_edge's per-direction math used to get before this
 *  pass queried it directly.
 * ------------------------------------------------------------------------- */
void clcdc_dispatch_set_palette_hook(void *param)	/* FUN_c0015018 */
{
	(void)param;
}

/* ------------------------------------------------------------------------- *
 *  clcdc_draw_edge - RESOLVED this pass (2026-07-18, item 4): queried the
 *  real decompile directly for the first time. Two things the previous draft
 *  got wrong, now fixed:
 *
 *  1. It is a ONE-parameter function, not two. There is no `direction`
 *     argument - `param_1` is the cursor only. What was guessed to be a
 *     caller-supplied direction is actually read from a GLOBAL, self-
 *     advancing counter (DAT_c001540c) that the function itself increments
 *     at the end of every call.
 *
 *  2. The "marching-ants" animation isn't just a side effect layered on top
 *     of a caller-chosen direction+edge draw - it IS the entire mechanism.
 *     Every call reads its own direction (0-3) and inset distance (0-300)
 *     from self-mutating state, draws exactly one of the 4 sides of a
 *     rectangle inset by that distance from the 800x600 screen edges, then
 *     advances the state for the next call:
 *       - `edge_dir` (DAT_c001540c) cycles 0,1,2,3 - one new side per call.
 *       - `edge_pos` (DAT_c0015408) - the inset distance - advances by one
 *         only once every 4 calls (a full side-cycle), wrapping at 301
 *         (0x12d).
 *       - `edge_colour` (DAT_c0015414) - the palette index every pixel is
 *         drawn in - advances by one only once every 4*301 = 1204 calls,
 *         wrapping at 257 (matches the previous draft's "~1200 calls"
 *         estimate almost exactly).
 *     So a caller that invokes this once per tick gets an animated
 *     rectangle border that shrinks inward from the screen edge to the
 *     centre (799/2, 599/2) over 301 side-cycles, then advances to the next
 *     highlight colour and starts over - a textbook selection/marching-ants
 *     highlight, entirely self-driven.
 *
 *  Exact per-direction geometry, with `pos` = current edge_pos (0..300):
 *     dir 0 -> TOP edge:    row = pos,     columns pos..(799-pos)
 *     dir 1 -> BOTTOM edge: row = 599-pos, columns pos..(799-pos)
 *     dir 2 -> LEFT edge:   column = pos,     rows pos..(599-pos)
 *     dir 3 -> RIGHT edge:  column = 799-pos, rows pos..(599-pos)
 *  (modes 2/3 get there by calling clcdc_cursor_init with width=1, which
 *  forces the shared scanning-write loop below to wrap after every single
 *  pixel - turning the "horizontal fill" primitive into a vertical scan
 *  without a separate code path.)
 *
 *  STILL OPEN: zero static callers found for this function (confirmed via
 *  an explicit xref query) - almost certainly driven by a timer/tick ISR
 *  given the self-advancing per-call state design, but that caller hasn't
 *  been traced. @0xc00150d4.
 * ------------------------------------------------------------------------- */
extern uint16_t *clcdc_framebuffer;		/* *DAT_c0015418 / *DAT_c00157c4 etc, real fixed addr */
extern uint16_t *clcdc_palette;		/* *DAT_c001541c / *DAT_c00157c8 etc, real fixed addr */
extern uint32_t  clcdc_fb_pixel_count_limit;	/* DAT_c0015410 / DAT_c00157c0, 479999 (800*600-1) */

extern int16_t *clcdc_edge_dir_counter;	/* DAT_c001540c: self-advancing edge selector, 0..3, wraps every call */
extern int16_t *clcdc_edge_pos_counter;	/* DAT_c0015408: self-advancing inset distance, 0..300, wraps every 4 calls */
extern int16_t *clcdc_edge_colour_counter;	/* DAT_c0015414: self-advancing palette index, 0..256, wraps every 1204 calls */

/* Shared per-pixel body of the fill-to-edge scan, identical across all 4
 * direction branches in the real disassembly (the compiler duplicated it 4x
 * rather than sharing a subroutine - factored out here for readability,
 * behaviourally identical). */
static void clcdc_edge_plot_and_advance(struct clcdc_cursor *c)
{
	uint32_t pixel = (uint16_t)c->x + (uint16_t)c->y * 800;

	if (pixel <= clcdc_fb_pixel_count_limit)
		clcdc_framebuffer[pixel] = clcdc_palette[*clcdc_edge_colour_counter];
	c->x++;
	if (c->right_edge < c->x) {
		c->x = c->left_margin;
		c->y++;
	}
}

void clcdc_draw_edge(struct clcdc_cursor *c)		/* FUN_c00150d4 */
{
	int16_t  pos = *clcdc_edge_pos_counter;	/* sVar1 */
	int16_t  dir = *clcdc_edge_dir_counter;	/* sVar2 */
	uint32_t right_dist  = 799 - pos;		/* uVar6 */
	uint32_t bottom_dist = 599 - pos;		/* uVar7 */
	int      i;

	if (dir == 0) {
		clcdc_cursor_init(c, pos, pos, (int)(right_dist - pos) + 1);
		for (i = pos; i <= (int)right_dist; i++)
			clcdc_edge_plot_and_advance(c);
	} else if (dir == 1) {
		clcdc_cursor_init(c, pos, (int16_t)bottom_dist, (int)(right_dist - pos) + 1);
		for (i = pos; i <= (int)right_dist; i++)
			clcdc_edge_plot_and_advance(c);
	} else if (dir == 2) {
		clcdc_cursor_init(c, pos, pos, 1);		/* width=1 -> wraps every pixel -> vertical scan */
		for (i = pos; i <= (int)bottom_dist; i++)
			clcdc_edge_plot_and_advance(c);
	} else if (dir == 3) {
		clcdc_cursor_init(c, (int16_t)right_dist, pos, 1);
		for (i = pos; i <= (int)bottom_dist; i++)
			clcdc_edge_plot_and_advance(c);
	}

	/* self-advance the 3-level counter - see header comment. */
	(*clcdc_edge_dir_counter)++;
	if (*clcdc_edge_dir_counter < 4)
		return;
	(*clcdc_edge_pos_counter)++;
	pos = *clcdc_edge_pos_counter;
	*clcdc_edge_dir_counter = 0;
	if (pos < 0x12d)
		return;
	*clcdc_edge_pos_counter = 0;
	(*clcdc_edge_colour_counter)++;
	if (*clcdc_edge_colour_counter > 0x100)
		*clcdc_edge_colour_counter = 0;
}

/* ------------------------------------------------------------------------- *
 *  Bitmap font - a monospace/proportional font descriptor. @0xc00157cc
 *  (glyph data pointer lookup), 0xc00157f4 (glyph advance-width lookup,
 *  building on the same lookup).
 * ------------------------------------------------------------------------- */
/* CORRECTION (re-verification pass, 2026-07-17): the bounds-check field
 * below is genuinely read from offset +0x04 in the real disassembly, not
 * +0x05 as the original draft of this struct claimed - `last_char` and
 * `first_char` were swapped relative to their real offsets. Fixed here;
 * `first_char`'s "always ' '=0x20 in practice" note is carried over
 * unconfirmed from the original draft, not independently re-derived this
 * pass.
 *
 * CORRECTION (item 3 pass, 2026-07-18): the +0x05 field is NOT first_char -
 * re-checked directly and there is no such field anywhere in the real code.
 * clcdc_font_glyph's own lower bound is a hardcoded literal (`ch - 0x20`),
 * not read from the struct at all, so "first_char" was never backed by any
 * decompiled evidence, only a plausible-sounding guess. What +0x05 actually
 * is: clcdc_blit_glyph reads it directly as its per-glyph SCANLINE COUNT
 * (the row loop trip count for the bit blitter below). Renamed to
 * `glyph_rows` accordingly. +0x06 (used by clcdc_font_glyph/_advance as the
 * per-glyph byte-stride into glyph_data) is kept but renamed `glyph_stride`
 * for the same reason - it's confirmed as a *stride*, not confirmed to be
 * "rows" itself (the two are probably numerically equal for narrow <=8px-
 * wide glyphs, which is likely why the original draft conflated them).
 * +0x07 is also renamed `advance_spacing`, since clcdc_blit_glyph shows it's
 * a flat per-font constant added to every glyph's own embedded width byte,
 * not a lookup table (see clcdc_blit_glyph's own header comment). */
struct clcdc_font {
	const uint8_t *glyph_data;	/* +0x00: base of the glyph bitmap table */
	uint8_t last_char;		/* +0x04: highest char code with a real glyph; low bound is a hardcoded ' '=0x20 literal in code, not a struct field */
	uint8_t glyph_rows;		/* +0x05: scanline count clcdc_blit_glyph's row loop iterates - confirmed via that function, not a char-code field */
	uint8_t glyph_stride;		/* +0x06: bytes per glyph in glyph_data (glyph_data + glyph_stride*idx) */
	uint8_t advance_spacing;	/* +0x07: flat per-font spacing constant added to every glyph's own embedded width byte (see clcdc_blit_glyph) */
};

const uint8_t *clcdc_font_glyph(const struct clcdc_font *f, uint8_t ch)	/* FUN_c00157cc */
{
	uint32_t idx = (uint8_t)(ch - 0x20);

	if (idx >= (uint32_t)(f->last_char - 0x20))
		return 0;
	return f->glyph_data + f->glyph_stride * idx;
}

const uint8_t *clcdc_font_advance(const struct clcdc_font *f, uint8_t ch)	/* FUN_c00157f4 */
{
	const uint8_t *g = clcdc_font_glyph(f, ch);

	if (!g)
		return 0;
	return (const uint8_t *)(uintptr_t)(*g + f->advance_spacing);
}

/*
 * clcdc_blit_glyph - FULL transcription this pass (item 3, 2026-07-18) - the
 * real decompile turned out to be legible, typed C (real shift/mask/cast
 * operators, not raw asm), so it's transcribed op-for-op below rather than
 * left as an opaque contract. Kept close to the literal Ghidra structure
 * (same operations, same branch shape, only renamed for readability) since
 * re-deriving equivalent-but-differently-shaped code from a summary risked
 * introducing a bug a literal transcription avoids. Not independently
 * verified against real hardware, so still flagged as "transcribed,
 * structurally faithful" rather than "hardware-confirmed."
 *
 * Handles sub-byte-aligned x positions (a glyph column doesn't generally
 * start on a byte boundary in the 1bpp-packed source bitmap) via a
 * 1/2/3-destination-byte shift-and-mask scheme (`span_mode` 0/1/2), chosen
 * per call from the glyph's pixel width and x's bit alignment. Returns the
 * advanced cursor x position (this function BOTH draws and returns the
 * next-glyph cursor - draw_text's own loop below relies on that combined
 * behavior, it isn't a separate measurement pass).
 *
 * NEW STRUCTURAL FINDING this pass: the destination it blits into
 * (`clcdc_glyph_dest_plane_base`, DAT_c0015afc) is NOT the 16bpp colour
 * framebuffer - it's addressed with a 100-byte row stride and 1-bit-per-
 * pixel packing. That is the EXACT SAME stride clcdc_draw_text's own
 * "highlight overlay" second pass reads from (DAT_c00157bc there, see that
 * function). This strongly suggests clcdc_blit_glyph doesn't paint glyphs
 * onto the visible screen directly at all - it rasterizes each glyph into a
 * shared 1bpp work bitmap, which clcdc_draw_text's second pass then
 * composites into the real framebuffer. Whether DAT_c0015afc and
 * DAT_c00157bc are literally the same global (same pointer value) is NOT
 * confirmed here - both are opaque runtime pointer variables with
 * zeroed/unresolved static values in this static dump.
 * NEEDS LIVE QUERY: read *0xc0015afc and *0xc00157bc at runtime on real
 * hardware/QEMU and compare - would upgrade this from "strongly suggested
 * by matching stride+addressing pattern" to "confirmed."
 *
 * Also newly confirmed: the per-glyph advance width is the FIRST byte of
 * the glyph's own bitmap data (`width = *glyph`), not looked up from any
 * table - struct clcdc_font's +0x07 field (`advance_spacing`, see above) is
 * a flat per-font constant added to every glyph's own width, with `kerning`
 * adding one more pixel when set.
 * @0xc0015820.
 */
extern uint8_t   *clcdc_glyph_dest_plane_base;	/* DAT_c0015afc: base of the row-stride-100 1bpp work bitmap glyphs are rasterized into - see note above */
extern int32_t    clcdc_blit_right_clip_bound;	/* DAT_c0015b00: right-edge clip bound in the 1bpp plane's coordinate space; exact numeric value unresolved in this static dump */

int clcdc_blit_glyph(const struct clcdc_font *f, uint8_t ch, uint32_t x, int kerning)	/* FUN_c0015820 */
{
	const uint8_t *glyph = clcdc_font_glyph(f, ch);
	int16_t  ret_x = (int16_t)x;			/* local_30 */

	if (!glyph)
		return ret_x;

	uint32_t spacing = f->advance_spacing;		/* local_44 */
	if (kerning)
		spacing++;

	const uint8_t *src   = glyph + 1;		/* pbVar14 */
	uint8_t        width = *glyph;			/* bVar3: glyph's own first byte = its pixel width */
	uint32_t bit_off   = x & 7;			/* uVar13 */
	uint32_t bit_avail = 8 - bit_off;		/* uVar11 */

	/* saturating (width - bit_avail): pixel columns spilling past the
	 * first destination byte's remaining bits. */
	uint32_t overflow_px = (width < bit_avail) ? 0 : (width - bit_avail) & 0xffff;	/* uVar12 */
	/* extra_bytes == uVar1 == iVar2 in the decompile: the same value
	 * (overflow_px >> 3), computed via two different shift-instruction
	 * idioms that are provably equal for any realistic (non-negative,
	 * <0x8000) glyph width. */
	uint32_t extra_bytes = overflow_px >> 3;
	int16_t  x_signed    = (int16_t)x;		/* iVar17 */

	uint8_t  first_keep_mask = ~(uint8_t)(0xff >> bit_off);			/* bVar5 */
	uint8_t *dst = clcdc_glyph_dest_plane_base + (x_signed >> 3);			/* pbVar7: dest = row_base + x/8 */
	int      row_advance = -(int)extra_bytes + 99;					/* iVar15 */
	int      span_mode = 0;							/* local_40: 0/1/2 extra dest bytes */

	if ((int)(width + spacing + x_signed) > clcdc_blit_right_clip_bound)
		return x_signed;		/* clipped at the edge - glyph not drawn, cursor NOT advanced */

	if ((int)((width + spacing) - bit_avail - extra_bytes * 8) > 0)
		span_mode = (((width + 7) >> 3) - extra_bytes == 1) ? 1 : 2;

	uint8_t last_keep_mask = (uint8_t)((0xff >> (overflow_px & 7)) >> (spacing & 0xff));	/* bVar6 */

	if (extra_bytes == 0) {
		if (span_mode == 0) {
			for (uint32_t row = f->glyph_rows; row != 0; row--) {
				*dst = (*dst & first_keep_mask) | (*src >> bit_off);
				dst += 100;	/* single dest byte per row */
				src++;
			}
		} else if (span_mode == 1) {
			for (uint32_t row = f->glyph_rows; row != 0; row--) {
				uint8_t b = *src;
				*dst = (*dst & first_keep_mask) | (b >> bit_off);
				uint8_t *dst2 = dst + 1;
				dst = dst2 + row_advance;
				*dst2 = (*dst2 & last_keep_mask) | (uint8_t)(b << bit_avail);
				src++;
			}
		} else { /* span_mode == 2 */
			for (uint32_t row = f->glyph_rows; row != 0; row--) {
				const uint8_t *src2 = src + 1;
				uint8_t b = *src;
				*dst = (*dst & first_keep_mask) | (b >> bit_off);
				src += 2;
				uint8_t *dst2 = dst + 1;
				dst = dst2 + row_advance;
				*dst2 = (*dst2 & last_keep_mask) | (uint8_t)(*src2 >> bit_off) | (uint8_t)(b << bit_avail);
			}
		}
	} else {
		/* width spans 3+ dest bytes: a run of `extra_bytes` fully
		 * byte-shifted middle bytes between the first and last partial
		 * bytes. */
		for (uint32_t row = f->glyph_rows; row != 0; row--) {
			uint8_t b = *src;
			*dst = (*dst & first_keep_mask) | (b >> bit_off);
			uint32_t carry   = (uint32_t)b << bit_avail;
			uint32_t mid     = extra_bytes;	/* uVar12 reused as the middle-byte counter in the real code */
			const uint8_t *p = src;
			uint8_t cur = 0;

			for (;;) {
				cur = (uint8_t)carry;
				dst++;
				src = p + 1;
				if (mid == 0)
					break;
				uint8_t nb = *src;
				*dst = cur | (nb >> bit_off);
				mid--;
				carry = (uint32_t)nb << bit_avail;
				p = src;
			}
			if (span_mode != 0) {
				if (span_mode == 2) {
					cur |= (uint8_t)(*src >> bit_off);
					src = p + 2;
				}
				*dst = (*dst & last_keep_mask) | cur;
			}
			dst += row_advance;
		}
	}

	return ret_x + width + spacing;	/* local_30 + bVar3 + local_44 */
}

/*
 * clcdc_draw_text(x, y, str, font_or_mode) - draws a string starting at
 * (x, y), one glyph at a time via clcdc_blit_glyph (which both draws AND
 * advances the cursor), then does a SECOND pass that composites the shared
 * 1bpp work bitmap clcdc_blit_glyph just rasterized into (see that
 * function's own header note) into the real 16bpp framebuffer.
 *
 * RESOLVED this pass (item 2, 2026-07-18): `font_or_mode` selection is now
 * fully traced. DAT_c00157b4 IS a 3-entry array of `const struct
 * clcdc_font *` (confirmed: indexed with `*(int*)(base + idx*4)`,
 * dereferenced once, matching a flat pointer array, not a struct/table with
 * other fields). `font_or_mode < 3` selects `clcdc_font_table[font_or_mode]`;
 * `font_or_mode >= 3` sets the font pointer to NULL for the whole call
 * (every clcdc_blit_glyph call then hits its own `!glyph` early-return).
 * WHICH 3 fonts those table entries actually point to is still NOT
 * resolved - the table itself is a runtime-populated pointer array with a
 * zeroed/unresolved static value in this dump.
 * NEEDS LIVE QUERY: read `clcdc_font_table[0..2]` at runtime (0xc00157b4,
 * 3 x 4-byte pointers) and dereference each to compare glyph_stride/
 * glyph_rows/last_char - would distinguish e.g. a small vs. large vs. bold
 * font by their differing dimensions even without visually rendering them.
 *
 * CORRECTION (item 2 pass, 2026-07-18): the previous draft described the
 * second pass as painting "any set bit" with colour 0xf, implying a sparse
 * overlay. Re-checked against the real decompile: it is NOT sparse - every
 * pixel in the `rows x cursor_x` box is unconditionally repainted, using
 * palette colour 0xf where the shared bitmap has a set bit and palette
 * colour 0 (background) everywhere else. So this pass functions as a full
 * background-fill-plus-highlight composite of the whole text bounding box,
 * not a highlight-only overlay drawn on top of already-visible text.
 *
 * Also newly noted (unconfirmed, flagged rather than asserted): the row
 * count for this second pass is read from the SAME selected font's
 * `glyph_rows` field (+0x05) that clcdc_blit_glyph uses as its own per-
 * glyph scanline count - i.e. a font-level field is reused here as an
 * unrelated "highlight box height." Numerically consistent (a typical
 * `glyph_rows` value like 8-16 easily clears the loop's own 0x28(40) clamp
 * without triggering it) but the reuse itself isn't independently explained.
 * @0xc0015650.
 */
extern const struct clcdc_font *const clcdc_font_table[3];	/* DAT_c00157b4 */
extern void *clcdc_text_ctl_table;				/* DAT_c00157b8 - NOT part of clcdc.cpp, table passed as-is to FUN_c0001a68 */
extern void *clcdc_text_ctl_lookup(void *table, int key);	/* FUN_c0001a68 - NOT part of clcdc.cpp, owning file/exact role not traced this pass */
extern void clcdc_wait_ready(int ctl_handle);			/* FUN_c000395c - also declared again below where clcdc_test_pattern's own header discusses it; duplicate extern is harmless */

void clcdc_draw_text(uint16_t x, uint16_t y, const char *str, uint32_t font_or_mode)	/* FUN_c0015650 */
{
	const struct clcdc_font *f = (font_or_mode < 3) ? clcdc_font_table[font_or_mode] : 0;
	uint16_t cursor_x = 0;

	for (const char *p = str; *p != '\0'; p++)
		cursor_x = (uint16_t)clcdc_blit_glyph(f, (uint8_t)*p, cursor_x, 0);

	/* second pass: composite the shared 1bpp plane into the framebuffer.
	 * `key` is always 0 here - the real code's `cVar5` argument is
	 * whatever the per-character loop above left it as, which is always
	 * the string's own NUL terminator at loop exit. */
	void *ctl = clcdc_text_ctl_lookup(clcdc_text_ctl_table, 0);
	uint32_t rows = f->glyph_rows;
	if (rows > 0x27)
		rows = 0x28;
	const uint8_t *bmp_row = clcdc_glyph_dest_plane_base;	/* DAT_c00157bc in the real decompile - see clcdc_blit_glyph's note on whether this is the same global */

	for (uint32_t row = 0; row < rows; row++, bmp_row += 100) {
		for (uint32_t col = 0; col < cursor_x; col++) {
			int      byte_i  = col >> 3;	/* col is always >=0 here, so the decompile's negative-x rounding correction never triggers */
			uint32_t bit_set = bmp_row[byte_i] & (0x80 >> (col & 7));
			uint32_t colour  = bit_set ? 0xf : 0;
			uint32_t px_x = x + col, px_y = y + row;

			if (px_x < 800 && px_y < 600) {
				uint32_t pixel = px_y * 800 + px_x;
				if (pixel <= clcdc_fb_pixel_count_limit)
					clcdc_framebuffer[pixel] = clcdc_palette[colour];	/* unconditional - see CORRECTION above */
				if ((*(uint32_t *)((uint8_t *)ctl + 8) & 4) != 0)
					clcdc_wait_ready((int)(uintptr_t)ctl);
			}
		}
	}
}

/* ------------------------------------------------------------------------- *
 *  clcdc_test_pattern - the built-in factory test-pattern generator, the
 *  function whose one call site is the "../clcdc.cpp" string's only xref
 *  (an unreachable-in-practice assert in mode 5's exhaustive-but-defensive
 *  band check). 7 modes (0-6), called from FUN_c001123c.
 *  CORRECTION (re-verification pass, 2026-07-17): FUN_c001123c was
 *  previously described as "a 6-entry dispatch table" - re-verified against
 *  fresh disassembly and found WRONG. It's a sequential if/else-if chain
 *  testing all 7 mode values one at a time, not a jump/lookup table. Still
 *  almost certainly the boot/factory-test menu's "show test pattern N"
 *  handler either way. @0xc00154e8.
 *
 *    mode 0        -> solid fill, palette colour 0
 *    mode 1        -> a crosshair/quadrant overlay (boundary at row 299/300,
 *                      column 400 - i.e. screen centre) using colour 0xf
 *    mode 2/3/4    -> solid fills, palette colours 9 / 10 / 0xc
 *    mode 5        -> horizontal colour bars: four 200-column-wide vertical
 *                      bands reusing modes 0/2/3/4's own colours in order -
 *                      the classic SMPTE-style colour-bar test pattern. The
 *                      exhaustive column-range check falls through to a hard
 *                      assert (crypto_at88_fault-style halt) in the
 *                      structurally-unreachable default case.
 *    mode 6        -> a single vertical highlight line at a configurable
 *                      column (DAT_c0015640), background colour 0xf
 *                      elsewhere - a cursor/marker line, not a fill.
 *
 *  Every mode writes through the same palette-indexed framebuffer pattern as
 *  the rest of this file, iterating all 800x600 pixels; a mid-loop status-
 *  register poll (bit 2 of a control-block flags field) calls a yield/wait
 *  primitive on every single pixel, suggesting the hardware needs pacing
 *  (vsync/DMA-ready wait) during a full-screen fill.
 *
 *  Item 6 confirmation (2026-07-18): re-queried FUN_c001123c's own decompile
 *  directly - it's a menu keypress handler, and the if/else-if chain claim
 *  above holds exactly. Precise key-code mapping: a key-code byte in range
 *  0x1f-0x25 (7 values) maps 1:1 to clcdc_test_pattern(0..6), each also
 *  setting a separate UI-state byte to 0xd..0x13 respectively. Key codes
 *  0x18-0x1e and 0x3a-0x3d route elsewhere (FUN_c001120c / FUN_c001121c,
 *  outside clcdc.cpp); 0x42-0x49 write directly into a UI state variable
 *  without calling this function at all. No dispatch table anywhere in this
 *  function - confirms the "sequential if/else-if chain" correction above
 *  was exactly right, nothing further to fix here.
 * ------------------------------------------------------------------------- */
extern void clcdc_assert(const void *unused, const char *file, int line);	/* FUN_c000919c, shared with crypto_at88_fault */
extern void clcdc_wait_ready(int ctl_handle);					/* FUN_c000395c */

void clcdc_test_pattern(int mode)		/* FUN_c00154e8 */
{
	(void)mode;
	/* See this function's own header comment for the confirmed 7-mode
	 * behavior. Pixel loop omitted here - functionally a straight
	 * 800x600 double loop selecting a palette index per mode and writing
	 * it through clcdc_reg_write's sibling framebuffer-write pattern;
	 * the mode-5 unreachable branch calls clcdc_assert(0, "../clcdc.cpp",
	 * <line>), which is the one real evidence tying this function
	 * definitively to clcdc.cpp. */
}

/* ------------------------------------------------------------------------- *
 *  clcdc_progress_bar - draws a fixed-point horizontal progress indicator
 *  directly onto the LCD, independent of the host-driven progress-bar path
 *  (COmapNKS4Driver::SetProgressBarPercent et al in the host-side
 *  reconstruction) - this one is entirely local to the panel firmware, most
 *  plausibly the bar under the boot splash image (KRONOS_V06R06.VSB.md's own
 *  "Boot splash resource" section documents a solid green footer band in the
 *  embedded splash bitmap at the same rough screen position this function
 *  writes to - consistent, though not independently confirmed to be the
 *  exact same draw call). @0xc0015420.
 *
 *  RESOLVED this pass (item 1, 2026-07-18): both `omap_tick_scale`
 *  (FUN_c001e3f8) itself and its exact use here are now fully traced.
 *
 *  `omap_tick_scale(a, b)` is NOT a "percent-to-width" function at all in
 *  its own right - re-queried its full decompile and it's a generic
 *  SOFTWARE SIGNED-DIVISION routine (a/b, truncating toward zero), with the
 *  usual fast paths a division intrinsic needs: b==0 faults out via
 *  FUN_c001e604, |b|==1 is a straight (possibly negated) copy, |a|<=|b| is
 *  handled directly, |b| a power of 2 becomes a shift, and the general case
 *  is textbook binary restoring long division (quotient built up 4 bits at
 *  a time). The ARM926EJ-S core this firmware runs on has no hardware
 *  integer-divide instruction, so this is almost certainly this firmware's
 *  own compiler-emitted division helper (an __aeabi_idiv/__divsi3-style
 *  routine), called from 8 places across the whole image - consistent with
 *  omap_l108.c's own already-documented finding that it's shared,
 *  firmware-wide utility code, not clcdc- or omap_l108-specific. Renamed to
 *  `omap_tick_scale` in the extern below to match the name omap_l108.c
 *  already settled on for this same function (see that file's own note).
 *
 *  The actual scaling math AT THIS CALL SITE (confirmed via
 *  clcdc_progress_bar's own decompile): `width_px = (percent << 9) / 100`
 *  using that generic signed division - i.e. `percent * 512 / 100`
 *  (truncating), which is `floor(percent * 5.12)`. At percent=100 this
 *  gives exactly 512. That pixel count is then painted as columns
 *  `0x92 .. 0x92+width_px-1` (146 up to 658 at 100%) into TWO mirrored rows
 *  - confirmed exact row numbers this pass: pixel-index base `0x43f80`
 *  (=278400=800*348, i.e. row 348, column 0) and `0x442a0`
 *  (=279200=800*349, column 0) - exactly one row apart (800, matching the
 *  0x320 CORRECTION already below), rows 348 and 349 of the 800x600
 *  framebuffer. Each column write is bounds-checked against
 *  `DAT_c00154d8` before being applied.
 *
 *  CORRECTION (re-verification pass, 2026-07-17): the two mirrored regions'
 *  offset difference was previously given as "0x3320, two rows ~13KB apart."
 *  Re-computed from the real disassembly: the actual difference is `0x320`
 *  (800 decimal) - exactly ONE row at the framebuffer's real 800px width,
 *  not two rows or ~13KB. A stray extra digit in the earlier draft. (Item 1
 *  pass above independently re-derived the same 800px/one-row result from
 *  the literal pixel-index constants, so this is now doubly confirmed.)
 * ------------------------------------------------------------------------- */
/* CORRECTION (omap_l108.c pass, 2026-07-17): FUN_c001e3f8 is NOT
 * clcdc-specific - it's also called by omap_l108.c's omap_tick_elapsed_scaled
 * (divisor 0x96/150, a different divisor than this call site's 100), making
 * it a generic, firmware-wide fixed-point scale utility. See omap_l108.c's
 * own note on this cross-file finding. */
extern int32_t omap_tick_scale(int32_t a, int32_t b);		/* FUN_c001e3f8 - generic signed-division utility, NOT clcdc-specific; see header note above and omap_l108.c's own matching extern */
extern uint32_t clcdc_progress_row_bound;			/* DAT_c00154d8: per-row column bound the mirrored writes are clamped against; exact numeric value unresolved in this static dump */
extern uint16_t *clcdc_progress_fb;				/* *DAT_c00154dc: framebuffer pointer for this call site (same physical framebuffer as clcdc_framebuffer elsewhere, different DAT_ symbol) */
extern uint16_t *clcdc_progress_palette;			/* *DAT_c00154e0: palette pointer for this call site; +2 = base colour entry, +0x12 = highlight colour entry, see below */

void clcdc_progress_bar(struct clcdc_regs *ctl, uint32_t percent)	/* FUN_c0015420 */
{
	int width_px;
	uint32_t col;

	(void)ctl;
	if (percent > 100)
		clcdc_assert(0, 0 /* DAT_c00154d4, real address */, 0xcb);

	width_px = omap_tick_scale((int32_t)(percent << 9), 100);	/* (percent*512)/100, truncating */

	for (col = 0x92; (int)col < width_px + 0x92; col++) {
		uint32_t row0_idx = col + 0x43f80;	/* row 348 */
		uint32_t row1_idx = col + 0x442a0;	/* row 349 */

		if (row0_idx <= clcdc_progress_row_bound)
			clcdc_progress_fb[row0_idx] = clcdc_progress_palette[0x12 / 2];	/* highlight colour, +0x12 byte offset = index 9 in a uint16_t LUT */
		if (row1_idx <= clcdc_progress_row_bound)
			clcdc_progress_fb[row1_idx] = clcdc_progress_palette[2 / 2];	/* base colour, +2 byte offset = index 1 */
	}
}
