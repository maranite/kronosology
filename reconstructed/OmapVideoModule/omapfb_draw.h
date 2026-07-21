/*
 * omapfb_draw.h - shared bit-twiddling helpers for the statically-linked
 * cfb-style blit routines (omapfb_fillrect.c / omapfb_copyarea.c /
 * omapfb_imgblt.c).
 *
 * This is a trimmed-down, x86/little-endian-only version of the kernel's
 * drivers/video/fb_draw.h. Two things were confirmed against the binary
 * before writing this (see kronosology/docs/modules/OmapVideoModule.ko.md):
 *
 *  1. bitfill_aligned/_rev and bitfill_unaligned/_rev decompile with NO
 *     bswapmask parameter at all (6/8 args, not 7/9) - this matches the
 *     upstream CONFIG_FB_CFB_REV_PIXELS_IN_BYTE=n code path exactly,
 *     where fb_shifted_pixels_mask_long()/fb_compute_bswapmask() collapse
 *     to macros that drop the bswapmask argument entirely. This module's
 *     source predates (or never adopted) the byte-swap-aware rework.
 *
 *  2. cfbimgblt's fast_imageblit() selects a *single* per-bpp table
 *     (cfb_tab8/cfb_tab16/cfb_tab32 - no _be/_le suffix, one symbol each,
 *     confirmed via nm), not a runtime fb_be_math() pick between two
 *     tables. This matches the pre-runtime-endian-check era of
 *     cfbimgblt.c where the LE/BE table was chosen at compile time via
 *     #ifdef __LITTLE_ENDIAN, and only the LE table survives on this
 *     x86 build.
 *
 * Consequently FB_SHIFT_HIGH/FB_SHIFT_LOW/FB_LEFT_POS below are hardcoded
 * to their little-endian forms (no fb_be_math() indirection) rather than
 * pulled from linux/fb.h.
 */
#ifndef OMAPFB_DRAW_H
#define OMAPFB_DRAW_H

#include <asm/types.h>
#include <linux/fb.h>

#define FB_WRITEL fb_writel
#define FB_READL  fb_readl

/* linux/fb.h's own FB_SHIFT_HIGH/FB_SHIFT_LOW/FB_LEFT_POS take an extra
 * struct fb_info * argument and go through the runtime fb_be_math(p)
 * check; undef and replace with the fixed little-endian forms this
 * binary actually uses (see file header comment above). */
#undef FB_SHIFT_HIGH
#undef FB_SHIFT_LOW
#undef FB_LEFT_POS
#define FB_SHIFT_HIGH(val, bits)  ((val) << (bits))
#define FB_SHIFT_LOW(val, bits)   ((val) >> (bits))
#define FB_LEFT_POS(bpp)          (0)

static inline unsigned long comp(unsigned long a, unsigned long b, unsigned long mask)
{
	return ((a ^ b) & mask) ^ b;
}

static inline unsigned long pixel_to_pat(u32 bpp, u32 pixel)
{
	switch (bpp) {
	case 1:  return 0xfffffffful * pixel;
	case 2:  return 0x55555555ul * pixel;
	case 4:  return 0x11111111ul * pixel;
	case 8:  return 0x01010101ul * pixel;
	case 12: return 0x01001001ul * pixel;
	case 16: return 0x00010001ul * pixel;
	case 24: return 0x01000001ul * pixel;
	case 32: return 0x00000001ul * pixel;
	default:
		panic("pixel_to_pat(): unsupported pixelformat\n");
	}
}

static inline unsigned long rolx(unsigned long word, unsigned int shift, unsigned int x)
{
	return (word << shift) | (word >> (x - shift));
}

#define cpu_to_le_long _cpu_to_le_long(BITS_PER_LONG)
#define _cpu_to_le_long(x) __cpu_to_le_long(x)
#define __cpu_to_le_long(x) cpu_to_le##x

#define le_long_to_cpu _le_long_to_cpu(BITS_PER_LONG)
#define _le_long_to_cpu(x) __le_long_to_cpu(x)
#define __le_long_to_cpu(x) le##x##_to_cpu

#endif /* OMAPFB_DRAW_H */
