/* SPDX-License-Identifier: GPL-2.0 */
#include <string.h>
#include <stdio.h>
#include "vpanel.h"

void vpanel_init(struct vpanel *p)
{
	memset(p, 0, sizeof(*p));
	/* A plausible boot-time default palette (grayscale ramp) so an unset
	 * screen renders as something other than solid black - the real
	 * driver's own power-on palette state isn't reconstructed here (Eva
	 * sets it during boot in the real system), this is just a sane
	 * placeholder for a panel nothing has painted yet. */
	for (int i = 0; i < 256; i++) {
		p->palette[i][0] = (uint8_t)i;
		p->palette[i][1] = (uint8_t)i;
		p->palette[i][2] = (uint8_t)i;
	}
}

void vpanel_init_lcd_regs(struct vpanel *p, char reg, char val, int data)
{
	(void)p; (void)reg; (void)val; (void)data;
}

void vpanel_x_axis_bytesize(struct vpanel *p, int bytes)
{
	(void)p; (void)bytes;
}

void vpanel_send_pixel_region(struct vpanel *p, int offset, int width, int height,
			      const uint8_t *pixels)
{
	int y0 = offset / VPANEL_WIDTH;
	int x0 = offset % VPANEL_WIDTH;

	for (int row = 0; row < height; row++) {
		int y = y0 + row;
		if (y < 0 || y >= VPANEL_HEIGHT)
			continue;
		for (int col = 0; col < width; col++) {
			int x = x0 + col;
			if (x < 0 || x >= VPANEL_WIDTH)
				continue;
			p->fb[y][x] = pixels[row * width + col];
		}
	}
	p->frames_rendered++;
}

void vpanel_send_fill(struct vpanel *p, uint8_t color, int width, int base, int height)
{
	int y0 = base / VPANEL_WIDTH;
	int x0 = base % VPANEL_WIDTH;

	for (int row = 0; row < height; row++) {
		int y = y0 + row;
		if (y < 0 || y >= VPANEL_HEIGHT)
			continue;
		for (int col = 0; col < width; col++) {
			int x = x0 + col;
			if (x < 0 || x >= VPANEL_WIDTH)
				continue;
			p->fb[y][x] = color;
		}
	}
	p->frames_rendered++;
}

void vpanel_update_color_pal(struct vpanel *p, uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
	p->palette[index][0] = r;
	p->palette[index][1] = g;
	p->palette[index][2] = b;
}

void vpanel_set_progress_percent(struct vpanel *p, uint8_t pct) { p->progress_percent = pct; }
void vpanel_set_progress_color1(struct vpanel *p, uint8_t c)    { p->progress_color1 = c; }
void vpanel_set_progress_color2(struct vpanel *p, uint8_t c)    { p->progress_color2 = c; }

/* Minimal, dependency-free 24-bit uncompressed BMP writer (BITMAPFILEHEADER +
 * BITMAPINFOHEADER, bottom-up row order, 4-byte row padding) - viewable in any
 * browser/image tool without needing libpng or any other library. */
int vpanel_dump_bmp(const struct vpanel *p, const char *path)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return -1;

	int w = VPANEL_WIDTH, h = VPANEL_HEIGHT;
	int row_bytes = w * 3;
	int pad = (4 - (row_bytes % 4)) % 4;
	int data_size = (row_bytes + pad) * h;
	int file_size = 54 + data_size;

	uint8_t hdr[54] = {0};
	hdr[0] = 'B'; hdr[1] = 'M';
	*(uint32_t *)(hdr + 2)  = (uint32_t)file_size;
	*(uint32_t *)(hdr + 10) = 54;			/* pixel data offset */
	*(uint32_t *)(hdr + 14) = 40;			/* DIB header size    */
	*(int32_t  *)(hdr + 18) = w;
	*(int32_t  *)(hdr + 22) = h;			/* positive = bottom-up */
	*(uint16_t *)(hdr + 26) = 1;			/* planes */
	*(uint16_t *)(hdr + 28) = 24;			/* bpp */
	*(uint32_t *)(hdr + 34) = (uint32_t)data_size;
	fwrite(hdr, 1, 54, f);

	uint8_t pad_bytes[3] = {0, 0, 0};
	for (int y = h - 1; y >= 0; y--) {		/* bottom-up row order */
		for (int x = 0; x < w; x++) {
			uint8_t idx = p->fb[y][x];
			uint8_t bgr[3] = { p->palette[idx][2], p->palette[idx][1], p->palette[idx][0] };
			fwrite(bgr, 1, 3, f);
		}
		if (pad)
			fwrite(pad_bytes, 1, pad, f);
	}
	fclose(f);
	return 0;
}
