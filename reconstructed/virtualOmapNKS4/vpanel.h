/* SPDX-License-Identifier: GPL-2.0 */
/*
 * vpanel.h - virtualOmapNKS4: a userspace "virtual NKS4 panel" that implements
 * the same logical draw API COmapNKS4VideoAPI (kronosology/reconstructed/
 * OmapNKS4Module/video.cpp) exposes, backed by a real in-memory 800x600 8bpp
 * indexed framebuffer + 256-entry RGB palette, and can render that framebuffer
 * to a viewable image (BMP - zero external dependencies, viewable in any
 * browser/image viewer).
 *
 * Scope decision (2026-07-17): this models the DRAW-LEVEL API (the same one
 * SendPixelDataRegion/UpdateColorPal/SendFillData/InitLCDRegs already present
 * to callers in the real reconstruction), not the raw USB wire byte stream
 * (opcode 0xc6 512-byte chunks with the dword-halfword-swap ContinueProcessingEvent
 * applies). The wire-level framing has one still-unresolved sizing detail (the
 * real packet buffer is declared 0x220 (544) bytes in the decompile, but the
 * documented/expected wire chunk is 512 bytes - see video.cpp's own comments) -
 * building a byte-perfect USB replay device on top of that ambiguity risked
 * getting the "screen" subtly wrong in a way that would be hard to notice
 * visually. The draw-level API has no such ambiguity: it's exactly what this
 * project's own DOOM driver (KronosDoom/doomgeneric_kronos.c) and the real
 * OA.ko/Eva callers already use. A future pass can add a wire-level front end
 * (real USB gadget or a pipe-fed replay) that decodes onto this same panel.
 */
#ifndef VPANEL_H
#define VPANEL_H

#include <stdint.h>

#define VPANEL_WIDTH  800
#define VPANEL_HEIGHT 600

struct vpanel {
	uint8_t  fb[VPANEL_HEIGHT][VPANEL_WIDTH];	/* 8bpp palette indices */
	uint8_t  palette[256][3];			/* RGB, index-addressed  */
	uint8_t  progress_percent;
	uint8_t  progress_color1, progress_color2;
	uint32_t frames_rendered;
};

void vpanel_init(struct vpanel *p);

/* ---- the same logical ops COmapNKS4VideoAPI exposes -------------------- */

/* op 0xc0 - init LCD control register (modeled as inert; no real LCD register
 * side effects known to affect the visible framebuffer). */
void vpanel_init_lcd_regs(struct vpanel *p, char reg, char val, int data);

/* op 0x81 - x-axis byte size (scanline stride hint; modeled as inert for the
 * same reason - this reconstruction always renders at the fixed 800x600
 * geometry COmapNKS4VideoAPI's own constructor hardcodes). */
void vpanel_x_axis_bytesize(struct vpanel *p, int bytes);

/* op 0xc2 - pixel region blit: `pixels` is `width*height` raw palette-index
 * bytes (row-major), written into the framebuffer starting at pixel offset
 * `offset` (linear index into the 800x600 plane), each row advancing by the
 * full screen stride (800) regardless of `width` - matching
 * ContinueProcessingEvent's own row-wraparound math
 * (sCurrentRegionTransferInfo += dwScreenWidth - rowWidth). */
void vpanel_send_pixel_region(struct vpanel *p, int offset, int width, int height,
			      const uint8_t *pixels);

/* op 0xc4 - solid fill: `color` across `width` columns, `height` rows,
 * starting at linear pixel offset `base`. */
void vpanel_send_fill(struct vpanel *p, uint8_t color, int width, int base, int height);

/* op 0xc5 - update one palette entry's RGB (matches UpdateColorPal(a,b,c,d)'s
 * 4-byte grouping: this reconstruction treats it as (index, r, g, b) - the
 * most natural reading of "a palette-entry update op with 4 payload bytes"
 * and the only grouping that lets a caller address all 256 entries; not
 * independently disassembly-confirmed byte-for-byte for THIS specific
 * argument order, flagged here rather than asserted as ground truth). */
void vpanel_update_color_pal(struct vpanel *p, uint8_t index, uint8_t r, uint8_t g, uint8_t b);

void vpanel_set_progress_percent(struct vpanel *p, uint8_t pct);
void vpanel_set_progress_color1(struct vpanel *p, uint8_t c);
void vpanel_set_progress_color2(struct vpanel *p, uint8_t c);

/* Render the current framebuffer+palette to a viewable 24-bit BMP file.
 * Returns 0 on success. */
int vpanel_dump_bmp(const struct vpanel *p, const char *path);

#endif /* VPANEL_H */
