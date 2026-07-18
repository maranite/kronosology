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
 *  clcdc_draw_edge - a line-drawing primitive: 4 direction modes advance the
 *  cursor pixel-by-pixel from its current (x,y) to the screen edge (799 or
 *  599), writing through a palette LUT into the framebuffer, and wrapping the
 *  cursor back to its left margin + next row when it overruns the right edge
 *  (a scanning writer, not a Bresenham line - the 4 "directions" are really
 *  4 different fixed starting-geometry setups for the same fill-to-edge
 *  loop). @0xc00150d4.
 *
 *  Also runs a marching-ants-style animation counter on every call
 *  (increments a sub-counter mod 4, then a position counter mod 301, then a
 *  256-entry colour-index counter mod 257) - a cycling border/selection
 *  highlight, not a one-shot draw. The exact `direction` semantics (which of
 *  the 4 cases is "top/bottom/left/right") aren't independently confirmed -
 *  modeled by the real branch structure, not asserted with certainty.
 * ------------------------------------------------------------------------- */
extern uint16_t *clcdc_framebuffer;		/* *DAT_c0015418 / *DAT_c00157c4 etc, real fixed addr */
extern uint16_t *clcdc_palette;		/* *DAT_c001541c / *DAT_c00157c8 etc, real fixed addr */
extern uint32_t  clcdc_fb_pixel_count_limit;	/* DAT_c0015410 / DAT_c00157c0, 479999 (800*600-1) */

void clcdc_draw_edge(struct clcdc_cursor *c, int direction)		/* FUN_c00150d4 */
{
	/* four direction cases each set up a starting geometry via
	 * clcdc_cursor_init, then share one scanning fill-to-edge loop that:
	 *   1. computes a linear pixel index from (x, y)
	 *   2. looks the animation colour index up in the palette, writes it
	 *      into the framebuffer if the index is in bounds
	 *   3. advances x, wrapping to (left_margin, y+1) past right_edge
	 * See FUN_c00150d4's own decompile for the exact per-direction
	 * boundary math (799-x / 599-y edge-distance calculations) - not
	 * simplified here to avoid asserting semantics not yet confirmed. */
	(void)c; (void)direction;
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
 * pass. */
struct clcdc_font {
	const uint8_t *glyph_data;	/* +0x00: base of the glyph bitmap table */
	uint8_t last_char;		/* +0x04: highest char code with a real glyph */
	uint8_t first_char;		/* +0x05: lowest char code with a real glyph (always ' '=0x20 in practice, unconfirmed) */
	uint8_t glyph_height_px;	/* +0x06: bitmap rows per glyph, used as the per-glyph stride */
	uint8_t advance_table_base;	/* +0x07: per-glyph advance-width table, indexed by glyph */
};

const uint8_t *clcdc_font_glyph(const struct clcdc_font *f, uint8_t ch)	/* FUN_c00157cc */
{
	uint32_t idx = (uint8_t)(ch - 0x20);

	if (idx >= (uint32_t)(f->last_char - 0x20))
		return 0;
	return f->glyph_data + f->glyph_height_px * idx;
}

const uint8_t *clcdc_font_advance(const struct clcdc_font *f, uint8_t ch)	/* FUN_c00157f4 */
{
	const uint8_t *g = clcdc_font_glyph(f, ch);

	if (!g)
		return 0;
	return (const uint8_t *)(uintptr_t)(*g + f->advance_table_base);
}

/*
 * clcdc_blit_glyph - the real per-character bitmap blitter. Handles
 * sub-byte-aligned x positions (a glyph column doesn't generally start on a
 * byte boundary in the 1bpp-packed source bitmap) via a 1/2/3-source-byte
 * shift-and-mask scheme, writing the blitted result through the same
 * palette-indexed framebuffer write every other draw primitive in this file
 * uses. Returns the advanced cursor x position (this function BOTH draws
 * and returns the next-glyph cursor - draw_text's own loop below relies on
 * that combined behavior, it isn't a separate measurement pass). @0xc0015820.
 * Bit-level shift/mask logic omitted here (see the real decompile) - the
 * externally-visible contract (glyph lookup -> masked multi-byte blit ->
 * advanced cursor return) is what's reconstructed with confidence; the exact
 * shift-amount derivation for each of the 1/2/3-byte-span cases is dense
 * enough that transcribing it without a live-hardware round-trip risked
 * introducing a transcription bug more easily than it resolved one.
 */
extern int clcdc_blit_glyph(const struct clcdc_font *f, uint8_t ch, int16_t x, int kerning);

/*
 * clcdc_draw_text(x, y, str, ?) - draws a string starting at (x, y), one
 * glyph at a time via clcdc_blit_glyph (which both draws AND advances the
 * cursor), then does a SECOND pass: up to 40 (0x28) rows of a secondary
 * bitmap (row stride 100 bytes) are tested bit-by-bit and any set bit is
 * painted with palette colour index 0xf - functionally an underline/
 * highlight-box overlay drawn under or over the text using a *different*,
 * fixed-stride bitmap source than the font glyphs themselves. The 4th
 * parameter selects which of <3 special-case fonts vs. the general path is
 * used (DAT_c00157b4 is a 3-entry font-pointer table) - not fully resolved
 * which 3 fonts those are. @0xc0015650.
 */
void clcdc_draw_text(uint16_t x, uint16_t y, const char *str, uint32_t font_or_mode)	/* FUN_c0015650 */
{
	(void)x; (void)y; (void)str; (void)font_or_mode;
	/* See clcdc_blit_glyph's own comment - the per-character loop is a
	 * thin driver over that function; the highlight/underline second pass
	 * is a distinct, separately-confirmed bitmap-test loop layered on top.
	 * Left as a documented contract rather than transcribed pixel-shift
	 * arithmetic, consistent with clcdc_blit_glyph above. */
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
 *  percent (0-100, asserts via clcdc_assert if >100) is scaled by
 *  FUN_c001e3f8(percent<<9, 100) - a fixed-point (percent/100)*some-width
 *  computation not yet traced into FUN_c001e3f8 itself - producing a pixel
 *  count that's painted symmetrically into two mirrored framebuffer regions
 *  using two distinct palette entries (a base colour and a highlight colour,
 *  DAT_c00154e0+2 and +0x12).
 *
 *  CORRECTION (re-verification pass, 2026-07-17): the two mirrored regions'
 *  offset difference was previously given as "0x3320, two rows ~13KB apart."
 *  Re-computed from the real disassembly: the actual difference is `0x320`
 *  (800 decimal) - exactly ONE row at the framebuffer's real 800px width,
 *  not two rows or ~13KB. A stray extra digit in the earlier draft.
 * ------------------------------------------------------------------------- */
/* CORRECTION (omap_l108.c pass, 2026-07-17): FUN_c001e3f8 is NOT
 * clcdc-specific - it's also called by omap_l108.c's omap_tick_elapsed_scaled
 * (divisor 0x96/150, a different divisor than this call site's 100), making
 * it a generic, firmware-wide fixed-point scale utility. See omap_l108.c's
 * own note on this cross-file finding. */
extern int clcdc_scale_percent_to_width(int percent_scaled, int max_width);	/* FUN_c001e3f8, shared utility, not yet traced */

void clcdc_progress_bar(struct clcdc_regs *ctl, uint32_t percent)	/* FUN_c0015420 */
{
	if (percent > 100)
		clcdc_assert(0, 0 /* DAT_c00154d4, real address */, 0xcb);
	(void)ctl;
	/* See header comment - the fill-width computation and dual-region
	 * write are confirmed structurally; FUN_c001e3f8's own fixed-point
	 * math isn't traced yet, so the exact pixel-per-percent scale isn't
	 * asserted as a hard number here. */
}
