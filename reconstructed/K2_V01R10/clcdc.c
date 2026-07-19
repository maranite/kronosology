/* SPDX-License-Identifier: GPL-2.0 */
/*
 * clcdc.c - K2 (KRONOS2S_V01R10.VSB / "KRONOS II") port of the LCD
 * controller driver already reconstructed for K1 in K1_V06R06/clcdc.c: raw
 * hardware register access, a drawing-cursor abstraction, line/box
 * primitives, a proportional bitmap-font text renderer, a built-in
 * multi-mode test-pattern generator, and a fixed-point progress-bar drawer.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB
 * (query_dump_k2.py), 2026-07-18. Anchor: the literal "../clcdc.cpp" string
 * lives at 0xc002b5f8 (K1: 0xc0023bac). Unlike K1 (whose own README describes
 * only one confirmed xref to this string), TWO independent DAT_ globals in
 * K2 resolve to this address (via each one's Ghidra data_value field):
 * DAT_c0012400 (inside the progress-bar function below) and DAT_c0012564
 * (inside the test-pattern function below). Cross-checked against K1 itself
 * this pass: K1's own clcdc_progress_bar has the identical second reference
 * (DAT_c00154d4 resolves to 0xc0023bac too, K1's own anchor address) -
 * K1's README undercounted its own xrefs by one; not corrected in that
 * read-only file, noted here for anyone cross-referencing.
 *
 * RESOLUTION FINDING (see task instructions' explicit ask): K2's LCD is
 * CONFIRMED to use the SAME 800x600 pixel resolution as K1. Evidence: the
 * fixed literal 800/600 screen-edge bounds appear unchanged in clcdc_draw_edge
 * (799/599), clcdc_draw_text (800/600 clip checks), and clcdc_test_pattern
 * (800x600 double loop, crosshair centred at column 399/400, row 299/300 -
 * i.e. exactly screen centre for an 800x600 panel); and the shared
 * fb-pixel-count-limit global independently resolves to 479999 (=800*600-1)
 * at FOUR different call sites (draw_edge, draw_text, test_pattern,
 * progress_bar) via each site's own DAT_ symbol. No register-layout or
 * dimension change found for this subsystem.
 *
 * COMPILATION-UNIT BOUNDARY: mirrors K1's own boundary-correction exactly.
 * The address range immediately following clcdc_blit_glyph (0xc0012a24
 * onward in K2) is NOT clcdc.cpp - confirmed by direct decompile inspection
 * to be the SAME neighbouring heap-allocator/cobjectmgr unit K1's own
 * README describes at its 0xc0015bf8 (three trivial "zero a global" stubs,
 * a self-recursive free-list walker, a bump/size-class allocator) - just
 * shifted to a different absolute address. clcdc.cpp's real K2 code ends at
 * clcdc_blit_glyph (0xc0012a24), same relative position as K1's 0xc0015afc.
 *
 * ONE GENUINE LOGIC DIFFERENCE found this pass: clcdc_progress_bar is NOT a
 * structural port - K2's version is a self-paced, tick-driven redesign with
 * different screen geometry. See that function's own header comment below
 * for the full diff. Every other function in this file is a verified
 * structurally-identical port.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Raw LCD controller register access (clcdc_reg_write/_set_bits/_clear_bits
 *  in K1, @0xc0015094/0xc00150a4/0xc00150bc) - NOT FOUND in K2.
 *
 *  Searched the full K2 decompiled-function set for the exact body shape
 *  (`*(uint32_t *)(*ctl_base + (offset & 0xff)) = value` and its
 *  or-bits/and-bits siblings) - zero matches anywhere in the image, not just
 *  near the rest of this cluster.
 *
 *  Corroborating evidence this is a real removal, not just a relocation:
 *  K1's wire-protocol dispatcher (FUN_c0007d1c) routes opcode 0xc0 (with a
 *  sub-selector byte choosing set-bits/clear-bits/write) to exactly these
 *  three functions. K2's own dispatcher counterpart (FUN_c0009b54, verified
 *  structurally analogous to FUN_c0007d1c - same per-opcode byte-code shape,
 *  same surrounding opcodes 0x81/0xc5/0xc6/0xc4/0xe0/0xe1 handled
 *  identically) instead routes opcode 0xc0 straight to the shared
 *  fault/assert helper (`if (bVar8 == 0xc0) goto LAB_c0009eb4;` ->
 *  `FUN_c000a730(0, DAT_c000a000, uVar6)`). I.e. the raw-register-poke wire
 *  opcode that used to reach these three functions is now treated as an
 *  invalid/faulting opcode in K2's dispatcher - consistent with the
 *  functions themselves having been removed from this firmware revision,
 *  not merely moved or inlined elsewhere.
 * ------------------------------------------------------------------------- */
struct clcdc_regs { uint32_t *base; /* real layout beyond this not traced - carried from K1, unused now that no K2 caller of this shape was found */ };

/* ------------------------------------------------------------------------- *
 *  Drawing cursor - PORTED from K1, verified structurally identical. Same
 *  struct layout (x +8, y +10, left_margin +0xc, right_edge +0xe, stride +4).
 *  K2 addresses: clcdc_cursor_set_stride @0xc0011f34 (K1: 0xc0015010, exact
 *  8-byte size match), clcdc_cursor_init @0xc0011f4c (K1: 0xc0015028, exact
 *  52-byte size match, identical body), clcdc_cursor_init_from_offset
 *  @0xc0011f80 (K1: 0xc001505c, exact 56-byte size match, identical body).
 * ------------------------------------------------------------------------- */
struct clcdc_cursor {
	uint8_t  pad0[4];
	uint32_t stride;		/* +4 */
	uint8_t  pad1[2];
	int16_t  x, y;			/* +8, +10 */
	int16_t  left_margin;		/* +0xc */
	int16_t  right_edge;		/* +0xe */
};

void clcdc_cursor_set_stride(struct clcdc_cursor *c, uint32_t stride)	/* FUN_c0011f34 */
{
	c->stride = stride;
}

void clcdc_cursor_init(struct clcdc_cursor *c, int16_t x, int16_t y, int width)	/* FUN_c0011f4c */
{
	c->x = x;
	if (width != 0)
		width -= 1;
	c->y = y;
	c->right_edge = (int16_t)(c->x + width);
	c->left_margin = c->x;
}

void clcdc_cursor_init_from_offset(struct clcdc_cursor *c, uint32_t offset, int width)	/* FUN_c0011f80 */
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
 *  clcdc_dispatch_set_palette_hook - PORTED from K1, verified structurally
 *  identical (16-byte exact size match): a one-line wrapper,
 *  `FUN_c0003288(*param_1)`, over a shared RGB->RGB565 palette-entry-set
 *  primitive (K2's counterpart of K1's FUN_c00037b0, NOT part of clcdc.cpp,
 *  owning file not traced here either). Same CAUTION as K1: the real K2
 *  dispatcher call site (FUN_c0009b54, opcode 0xc5) passes 5 arguments
 *  (`FUN_c0011f3c(DAT_c000a020,*pbVar1,*pbVar2,bVar8,*pbVar3)`) into what
 *  decompiles here as a 1-parameter function - the same packed-struct-
 *  pointer decompiler artifact K1 already documented for this exact
 *  function, re-confirmed independently against K2's own disassembly.
 *  @0xc0011f3c (K1: 0xc0015018).
 * ------------------------------------------------------------------------- */
void clcdc_dispatch_set_palette_hook(void *param)	/* FUN_c0011f3c */
{
	(void)param;
}

/* ------------------------------------------------------------------------- *
 *  clcdc_draw_edge - PORTED from K1, verified structurally identical. Same
 *  fully self-driven "marching ants" mechanism: three self-mutating global
 *  counters (edge_pos/edge_dir/edge_colour) with the SAME wrap thresholds as
 *  K1 (dir wraps at 4, pos wraps at 0x12d/301, colour wraps at >0x100/256),
 *  same 799/599 screen-edge geometry, same per-direction cursor-init calls.
 *  @0xc0011ff8 (K1: 0xc00150d4, exact 820-byte size match).
 *
 *  Zero static callers found in K2 either (confirmed via callers query) -
 *  same "almost certainly a timer/tick ISR" open question as K1.
 * ------------------------------------------------------------------------- */
extern uint16_t *clcdc_framebuffer;		/* *DAT_c001233c (K1: *DAT_c0015418 etc) */
extern uint16_t *clcdc_palette;		/* *DAT_c0012340 (K1: *DAT_c001541c etc) */
extern uint32_t  clcdc_fb_pixel_count_limit;	/* DAT_c0012334, CONFIRMED 479999 (=800*600-1) via resolved data_value - matches K1's own (unresolved-in-static-dump) 479999 exactly */

extern int16_t *clcdc_edge_dir_counter;	/* DAT_c0012330 (K1: DAT_c001540c) */
extern int16_t *clcdc_edge_pos_counter;	/* DAT_c001232c (K1: DAT_c0015408) */
extern int16_t *clcdc_edge_colour_counter;	/* DAT_c0012338 (K1: DAT_c0015414) */

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

void clcdc_draw_edge(struct clcdc_cursor *c)		/* FUN_c0011ff8 */
{
	int16_t  pos = *clcdc_edge_pos_counter;
	int16_t  dir = *clcdc_edge_dir_counter;
	uint32_t right_dist  = 799 - pos;
	uint32_t bottom_dist = 599 - pos;
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
		clcdc_cursor_init(c, pos, pos, 1);
		for (i = pos; i <= (int)bottom_dist; i++)
			clcdc_edge_plot_and_advance(c);
	} else if (dir == 3) {
		clcdc_cursor_init(c, (int16_t)right_dist, pos, 1);
		for (i = pos; i <= (int)bottom_dist; i++)
			clcdc_edge_plot_and_advance(c);
	}

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
 *  Bitmap font - PORTED from K1, verified structurally identical, including
 *  the same struct field offsets (last_char +0x04, glyph_rows +0x05,
 *  glyph_stride +0x06, advance_spacing +0x07) re-confirmed directly against
 *  K2's own decompile (`*(byte*)(param_1+1)` = offset+4 for last_char via
 *  int* indexing, `*(byte*)((int)param_1+6)` = offset+6 for glyph_stride,
 *  `*(byte*)(param_1+7)`... i.e. byte offset +7, for advance_spacing).
 *  @0xc00126f4 (K1: 0xc00157cc, exact 40-byte size match), @0xc001271c
 *  (K1: 0xc00157f4, exact 44-byte size match).
 * ------------------------------------------------------------------------- */
struct clcdc_font {
	const uint8_t *glyph_data;	/* +0x00 */
	uint8_t last_char;		/* +0x04 */
	uint8_t glyph_rows;		/* +0x05 */
	uint8_t glyph_stride;		/* +0x06 */
	uint8_t advance_spacing;	/* +0x07 */
};

const uint8_t *clcdc_font_glyph(const struct clcdc_font *f, uint8_t ch)	/* FUN_c00126f4 */
{
	uint32_t idx = (uint8_t)(ch - 0x20);

	if (idx >= (uint32_t)(f->last_char - 0x20))
		return 0;
	return f->glyph_data + f->glyph_stride * idx;
}

const uint8_t *clcdc_font_advance(const struct clcdc_font *f, uint8_t ch)	/* FUN_c001271c */
{
	const uint8_t *g = clcdc_font_glyph(f, ch);

	if (!g)
		return 0;
	return (const uint8_t *)(uintptr_t)(*g + f->advance_spacing);
}

/*
 * clcdc_blit_glyph - PORTED from K1, verified structurally identical op-for-
 * op against K2's real decompile (exact 732-byte size match with K1's
 * 0xc0015820-0xc0015afc span, and the same shift/mask/span_mode(0/1/2)
 * control-flow shape throughout). @0xc0012748 (K1: 0xc0015820).
 *
 * NEW THIS PASS (stronger than what K1's own static dump could show): the
 * destination-plane global (K1: DAT_c0015afc) and the right-clip-bound
 * global (K1: DAT_c0015b00) both resolve to REAL, non-zero values in K2's
 * static dump - 0xc01ccef0 and 798 (0x31e) respectively - rather than being
 * zeroed/unresolved as they were for K1. This is consistent with these
 * being literal-pool immediates baked into the instruction stream (readable
 * directly from the binary bytes) rather than true runtime BSS pointers;
 * K1's own dump apparently just didn't happen to capture them. The dest-
 * plane value (0xc01ccef0) is address-identical to the value clcdc_draw_text
 * below's own second-pass read pointer resolves to (see that function's own
 * note) - for K2 specifically this is now a literal-pool-level confirmation
 * that both symbols carry the same fixed constant, a step beyond K1's own
 * "strongly suggested, not confirmed" status for the equivalent pair. Still
 * not confirmed as the *same runtime memory location* by a live pointer
 * read - both remain independent compile-time constants that merely happen
 * to encode the same address.
 */
extern uint8_t   *clcdc_glyph_dest_plane_base;	/* DAT_c0012a24 (K1: DAT_c0015afc). CONFIRMED value 0xc01ccef0 via resolved data_value - K1's equivalent was unresolved in that static dump. */
extern int32_t    clcdc_blit_right_clip_bound;	/* DAT_c0012a28 (K1: DAT_c0015b00). CONFIRMED value 798 (0x31e) via resolved data_value - K1's equivalent was unresolved in that static dump. */

int clcdc_blit_glyph(const struct clcdc_font *f, uint8_t ch, uint32_t x, int kerning)	/* FUN_c0012748 */
{
	const uint8_t *glyph = clcdc_font_glyph(f, ch);
	int16_t  ret_x = (int16_t)x;

	if (!glyph)
		return ret_x;

	uint32_t spacing = f->advance_spacing;
	if (kerning)
		spacing++;

	const uint8_t *src   = glyph + 1;
	uint8_t        width = *glyph;
	uint32_t bit_off   = x & 7;
	uint32_t bit_avail = 8 - bit_off;

	uint32_t overflow_px = (width < bit_avail) ? 0 : (width - bit_avail) & 0xffff;
	uint32_t extra_bytes = overflow_px >> 3;
	int16_t  x_signed    = (int16_t)x;

	uint8_t  first_keep_mask = ~(uint8_t)(0xff >> bit_off);
	uint8_t *dst = clcdc_glyph_dest_plane_base + (x_signed >> 3);
	int      row_advance = -(int)extra_bytes + 99;
	int      span_mode = 0;

	if ((int)(width + spacing + x_signed) > clcdc_blit_right_clip_bound)
		return x_signed;

	if ((int)((width + spacing) - bit_avail - extra_bytes * 8) > 0)
		span_mode = (((width + 7) >> 3) - extra_bytes == 1) ? 1 : 2;

	uint8_t last_keep_mask = (uint8_t)((0xff >> (overflow_px & 7)) >> (spacing & 0xff));

	if (extra_bytes == 0) {
		if (span_mode == 0) {
			for (uint32_t row = f->glyph_rows; row != 0; row--) {
				*dst = (*dst & first_keep_mask) | (*src >> bit_off);
				dst += 100;
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
		for (uint32_t row = f->glyph_rows; row != 0; row--) {
			uint8_t b = *src;
			*dst = (*dst & first_keep_mask) | (b >> bit_off);
			uint32_t carry   = (uint32_t)b << bit_avail;
			uint32_t mid     = extra_bytes;
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

	return ret_x + width + spacing;
}

/*
 * clcdc_draw_text(x, y, str, font_or_mode) - PORTED from K1, verified
 * structurally identical: per-character clcdc_blit_glyph loop, then a
 * second pass compositing the shared 1bpp work bitmap into the real 16bpp
 * framebuffer (unconditional full-box repaint, palette colour 0xf for set
 * bits / 0 for clear, exactly like K1). @0xc0012578 (K1: 0xc0015650, exact
 * 356-byte size match).
 *
 * font_or_mode selection logic identical to K1: <3 indexes a 3-entry
 * `const struct clcdc_font *` table, >=3 forces NULL (every blit_glyph call
 * then early-returns via its own `!glyph` check). WHICH 3 fonts the table
 * points to remains unresolved in K2 too (runtime-populated pointer array,
 * zeroed in this static dump) - same open item as K1.
 *
 * NOTE: K2 has substantially more static callers of this function than K1
 * (14 vs the K1 file's own callers list) - consistent with K2's boot/status
 * UI code (test-pattern setup, the fault/assert helper's own error-message
 * drawing, etc.) using text rendering more heavily, not a change to this
 * function's own behaviour.
 */
extern const struct clcdc_font *const clcdc_font_table[3];	/* DAT_c00126dc (K1: DAT_c00157b4) */
extern void *clcdc_text_ctl_table;				/* DAT_c00126e0 (K1: DAT_c00157b8) - not part of clcdc.cpp */
extern void *clcdc_text_ctl_lookup(void *table, int key);	/* FUN_c0001818 (K1: FUN_c0001a68) - not part of clcdc.cpp */
extern void clcdc_wait_ready(int ctl_handle);			/* FUN_c0003434 (K1: FUN_c000395c) */

void clcdc_draw_text(uint16_t x, uint16_t y, const char *str, uint32_t font_or_mode)	/* FUN_c0012578 */
{
	const struct clcdc_font *f = (font_or_mode < 3) ? clcdc_font_table[font_or_mode] : 0;
	uint16_t cursor_x = 0;

	for (const char *p = str; *p != '\0'; p++)
		cursor_x = (uint16_t)clcdc_blit_glyph(f, (uint8_t)*p, cursor_x, 0);

	void *ctl = clcdc_text_ctl_lookup(clcdc_text_ctl_table, 0);
	uint32_t rows = f->glyph_rows;
	if (rows > 0x27)
		rows = 0x28;
	/* DAT_c00126e4 in the real decompile - CONFIRMED (via resolved
	 * data_value) to be address-identical to clcdc_blit_glyph's own
	 * DAT_c0012a24 - see that function's own header note. */
	const uint8_t *bmp_row = clcdc_glyph_dest_plane_base;

	for (uint32_t row = 0; row < rows; row++, bmp_row += 100) {
		for (uint32_t col = 0; col < cursor_x; col++) {
			int      byte_i  = col >> 3;
			uint32_t bit_set = bmp_row[byte_i] & (0x80 >> (col & 7));
			uint32_t colour  = bit_set ? 0xf : 0;
			uint32_t px_x = x + col, px_y = y + row;

			if (px_x < 800 && px_y < 600) {
				uint32_t pixel = px_y * 800 + px_x;
				if (pixel <= clcdc_fb_pixel_count_limit)
					clcdc_framebuffer[pixel] = clcdc_palette[colour];
				if ((*(uint32_t *)((uint8_t *)ctl + 8) & 4) != 0)
					clcdc_wait_ready((int)(uintptr_t)ctl);
			}
		}
	}
}

/* ------------------------------------------------------------------------- *
 *  clcdc_test_pattern - PORTED from K1's documented BEHAVIOUR and fully
 *  transcribed from K2's real decompile (K1's own file left the pixel loop
 *  as a documented contract rather than a literal transcription; K2's
 *  decompile turned out to be legible enough to transcribe directly, so
 *  this version is MORE complete than K1's). Sequential if/else-if chain
 *  over 7 modes (0-6), exactly matching K1's documented mode table:
 *
 *    mode 0        -> solid fill, palette colour 0
 *    mode 1        -> crosshair overlay, colour 0xf: row 299 or row 300, OR
 *                      column 399 or column 400 (K2 decompile shows BOTH
 *                      399 (DAT_c0012568=0x18f) AND 400 as literal column
 *                      matches, i.e. a genuine 2px-wide crosshair line -
 *                      more precise than K1's own "column 400" summary,
 *                      consistent with an 800px-wide screen's true centre
 *                      seam sitting between columns 399 and 400)
 *    mode 2/3/4    -> solid fills, palette colours 9 / 10 / 0xc
 *    mode 5        -> four 200-column vertical colour bands reusing modes
 *                      0/2/3/4's colours in order; the unreachable-default
 *                      column range hard-faults via the shared fault helper
 *    mode 6        -> single vertical highlight line at column
 *                      DAT_c0012568... wait: mode 6 uses a DIFFERENT DAT
 *                      than mode 1 in the real decompile (mode 6's compare
 *                      target is DAT_c0012568, CONFIRMED value 799/0x31f -
 *                      the screen's rightmost column), background colour
 *                      0xf elsewhere
 *
 *  Every mode writes through the shared clcdc_framebuffer/clcdc_palette
 *  pair, iterating all 800x600 pixels; a mid-loop status-register poll
 *  (bit 2 of a control-block flags field, same as clcdc_draw_text's own
 *  check) calls clcdc_wait_ready on every pixel.
 *
 *  @0xc0012414 (K1: 0xc00154e8, exact 300-byte size match). Fault call site
 *  CONFIRMED: file-string argument resolves to 0xc002b5f8 (this file's own
 *  anchor), line 0x114 (276 decimal) - K1's own file did not record a line
 *  number for this call site, so this is new information rather than a
 *  divergence from a known K1 value.
 * ------------------------------------------------------------------------- */
extern void clcdc_fault(const void *unused, const char *file, int line);	/* FUN_c000a730 - K2's shared fault/assert/hang helper, same role as K1's FUN_c000919c/clcdc_assert; confirmed via decompile shape (draws 2 text lines via clcdc_draw_text, then loops forever) and 63 static callers spanning the whole image */
extern void *clcdc_test_ctl_table;					/* DAT_c001255c - ctl-lookup key table for this function's own clcdc_wait_ready gating, not further traced */
extern uint32_t clcdc_test_crosshair_col_a;				/* DAT_c0012560, CONFIRMED value 399 (0x18f) */
extern uint32_t clcdc_test_line_col;					/* DAT_c0012568, CONFIRMED value 799 (0x31f) - mode 6's highlight column */
extern uint32_t clcdc_test_fb_pixel_count_limit;			/* DAT_c001256c, CONFIRMED value 479999 (=800*600-1) */

void clcdc_test_pattern(int mode)		/* FUN_c0012414 */
{
	void *ctl = clcdc_text_ctl_lookup(clcdc_test_ctl_table, 0);
	int   row = 0;
	uint32_t pixel = 0;

	do {
		uint32_t col = 0;
		do {
			int colour;

			switch (mode) {
			case 0:
			default_zero:
				colour = 0;
				break;
			case 1: {
				int on_row = (col == clcdc_test_crosshair_col_a || row == 299 ||
					      row == 300 || col == 400);
				colour = on_row ? 0xf : 0;
				break;
			}
			case 2: colour = 9; break;
			case 3: colour = 10; break;
			case 4: colour = 0xc; break;
			case 5:
				if (col < 200)
					colour = 0;
				else if (col - 200 < 200)
					colour = 9;
				else if (col - 400 < 200)
					colour = 10;
				else if (col - 600 < 200)
					colour = 0xc;
				else {
					colour = 0;
					/* structurally unreachable given the
					 * 4-band exhaustive check above (col
					 * is always < 800) - same defensive
					 * assert idiom K1 documented. */
					clcdc_fault(0, (const char *)0xc002b5f8, 0x114);
				}
				break;
			case 6:
				colour = (col == clcdc_test_line_col) ? 0 : 0xf;
				break;
			default:
				goto default_zero;
			}

			if (pixel <= clcdc_test_fb_pixel_count_limit)
				clcdc_framebuffer[pixel] = clcdc_palette[colour];
			if ((*(uint32_t *)((uint8_t *)ctl + 8) & 4) != 0)
				clcdc_wait_ready((int)(uintptr_t)ctl);

			col++;
			pixel++;
		} while (col < 800);
		row++;
	} while (row <= 599);
}

/* ------------------------------------------------------------------------- *
 *  clcdc_progress_bar - REWRITTEN for K2, NOT a structural port. This is the
 *  one confirmed logic difference in this file.
 *
 *  K1's version (@0xc0015420) is a caller-driven, stateless function:
 *  `clcdc_progress_bar(ctl, percent)` - takes an explicit 0-100 percent
 *  argument from its one conditional caller, asserts if percent>100, draws
 *  a bar of width `percent*512/100` starting at column 0x92 (146) into rows
 *  348/349.
 *
 *  K2's version (@0xc0009954, K1-equivalent-slot only by virtue of embedding
 *  the same clcdc.cpp anchor string) is a SELF-PACED, TICK-DRIVEN function
 *  taking NO effective argument (decompiles as `void FUN_c0009954(void)`;
 *  its one caller, FUN_c000a58c, does pass an argument at the call site but
 *  it's never read inside - the same "phantom forwarded parameter" pattern
 *  already catalogued elsewhere in this project for cdix_reg_write/read and
 *  K1's eva_board_watchdog_fault_wrapper). Confirmed differences:
 *
 *    - State is now two persistent globals this function owns exclusively
 *      (confirmed via a full-image search: no other K2 function references
 *      either): a 0-14 step counter (gated `if (step > 14) return;`) and a
 *      tick counter that increments every call and only advances the step
 *      once every 2001 calls (`if (2000 < tick+1) { step++; ... }`).
 *    - Width formula is `step * 650 / 100` (truncating), i.e. *6.5, not
 *      K1's *5.12 (`percent * 512 / 100`).
 *    - Bar starts at column 0x4b (75), not K1's 0x92 (146).
 *    - Bar rows are 434/435 (pixel-index bases 0x54c40/0x54f60), not K1's
 *      348/349 - still exactly ONE row (800px) apart, same as K1, just at a
 *      different vertical screen position.
 *    - Palette offsets are UNCHANGED from K1: +0x12 (highlight, index 9)
 *      for the first row, +2 (base, index 1) for the second - confirmed
 *      identical in the real K2 decompile.
 *    - The `percent > 100` assert-style guard is structurally still present
 *      (`if (100 < step+1) clcdc_fault(...)`) but is now unreachable in
 *      practice given the entry gate already caps the step value at 14 -
 *      transcribed faithfully as genuine (if vestigial) dead code rather
 *      than removed, same treatment K1's own file gives similar
 *      structurally-unreachable branches (e.g. clcdc_test_pattern's own
 *      mode-5 fallthrough).
 *
 *  Net effect: this reads as a boot-splash progress indicator re-tuned for
 *  K2's own boot sequence pacing (a fixed number of ticks per boot stage,
 *  15 stages) and a different on-screen position, rather than a general-
 *  purpose percent-driven bar a host command can drive arbitrarily. Whether
 *  K1's caller-driven form still exists ANYWHERE else in K2 under a
 *  different address was not found - the only other clcdc.cpp-anchored
 *  progress/fill-shaped function in the image is this one.
 *
 *  @0xc0009954 (size 264, vs K1's 180 - consistent with the extra tick-
 *  pacing logic K1's version doesn't have).
 * ------------------------------------------------------------------------- */
extern void omap_tick_scale_stub(void);	/* not used here - K2's width math (step*650/100) is inlined generic division, same shared signed-divide helper family as K1's omap_tick_scale/FUN_c001ac94, confirmed via FUN_c001ac94's OTHER K2 caller (FUN_c000194c, divisor 0x96/150) matching K1's cross-file omap_l108.c finding exactly */
extern int32_t omap_tick_scale(int32_t a, int32_t b);		/* FUN_c001ac94 (K1: FUN_c001e3f8) - generic signed-division utility, not clcdc-specific */
extern uint32_t clcdc_progress_row_bound;			/* DAT_c0012404 (K1: DAT_c00154d8), CONFIRMED value 479999 */
extern uint16_t *clcdc_progress_fb;				/* *DAT_c0012408 (K1: *DAT_c00154dc) */
extern uint16_t *clcdc_progress_palette;			/* *DAT_c001240c (K1: *DAT_c00154e0) */
extern int16_t  *clcdc_progress_step;				/* DAT_c00099a0: self-advancing 0..14 step counter, private to this function */
extern int32_t  *clcdc_progress_tick;				/* DAT_c00099a4: self-advancing tick counter, private to this function, wraps every 2001 calls */

void clcdc_progress_bar(void)	/* FUN_c0009954 */
{
	int width_px;
	uint32_t col;

	if (*clcdc_progress_step > 0xe)
		return;

	int32_t tick = *clcdc_progress_tick;
	int16_t next_step = (int16_t)(*clcdc_progress_step + 1);
	*clcdc_progress_tick = tick + 1;

	if (2000 >= tick + 1)
		return;

	*clcdc_progress_tick = 0;
	*clcdc_progress_step = next_step;

	if ((uint16_t)next_step > 100)
		/* structurally unreachable given the 0-14 entry gate above -
		 * see this function's own header note. */
		clcdc_fault(0, (const char *)0xc002b5f8, 200);

	width_px = omap_tick_scale((int32_t)next_step * 650, 100);

	for (col = 0x4b; (int)col < width_px + 0x4b; col++) {
		uint32_t row0_idx = col + 0x54c40;	/* row 434 */
		uint32_t row1_idx = col + 0x54f60;	/* row 435 */

		if (row0_idx <= clcdc_progress_row_bound)
			clcdc_progress_fb[row0_idx] = clcdc_progress_palette[0x12 / 2];
		if (row1_idx <= clcdc_progress_row_bound)
			clcdc_progress_fb[row1_idx] = clcdc_progress_palette[2 / 2];
	}
}
