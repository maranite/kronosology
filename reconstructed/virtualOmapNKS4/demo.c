/* SPDX-License-Identifier: GPL-2.0 */
/*
 * demo.c - exercises every vpanel draw op to prove the virtual panel actually
 * renders a real, viewable screen: a palette set (full RGB cube-ish ramp), a
 * SMPTE-style colour-bar test pattern drawn via SendPixelDataRegion (the same
 * op the real pixel-streaming pipeline uses), a solid-fill footer bar, and a
 * progress-bar readout, then dumps the result to BMP.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vpanel.h"

static void set_test_palette(struct vpanel *p)
{
	/* index 0..15: a simple 16-colour ramp used by the colour bars below;
	 * the rest keep vpanel_init's grayscale default. */
	static const uint8_t bars[16][3] = {
		{255,255,255}, {255,255,0}, {0,255,255}, {0,255,0},
		{255,0,255},   {255,0,0},   {0,0,255},   {0,0,0},
		{0,0,128},     {32,32,32},  {128,0,128}, {0,64,64},
		{64,64,0},     {200,200,200}, {90,90,90}, {20,20,20},
	};
	for (int i = 0; i < 16; i++)
		vpanel_update_color_pal(p, i, bars[i][0], bars[i][1], bars[i][2]);
}

int main(void)
{
	struct vpanel panel;
	vpanel_init(&panel);
	set_test_palette(&panel);

	/* colour bars across the top 400 rows, via SendPixelDataRegion - the
	 * exact same op real pixel-streaming draws through. */
	int bar_w = VPANEL_WIDTH / 16;
	uint8_t *row = malloc(VPANEL_WIDTH);
	for (int x = 0; x < VPANEL_WIDTH; x++)
		row[x] = (uint8_t)(x / bar_w);
	uint8_t *region = malloc((size_t)VPANEL_WIDTH * 400);
	for (int y = 0; y < 400; y++)
		memcpy(region + (size_t)y * VPANEL_WIDTH, row, VPANEL_WIDTH);
	vpanel_send_pixel_region(&panel, 0, VPANEL_WIDTH, 400, region);
	free(row);
	free(region);

	/* footer: solid fill via SendFillData */
	vpanel_send_fill(&panel, 9 /* dark purple */, VPANEL_WIDTH, 400 * VPANEL_WIDTH, 100);

	/* a simple "progress bar" readout drawn as a filled rectangle whose
	 * width tracks the progress percent - exercising SetProgressBarPercent's
	 * effect the same way the real panel would render one. */
	vpanel_set_progress_percent(&panel, 72);
	int pbar_y = 520, pbar_h = 30, pbar_w = VPANEL_WIDTH - 40;
	vpanel_send_fill(&panel, 13 /* light gray track */, pbar_w, pbar_y * VPANEL_WIDTH + 20, pbar_h);
	int filled_w = pbar_w * panel.progress_percent / 100;
	vpanel_send_fill(&panel, 3 /* green */, filled_w, pbar_y * VPANEL_WIDTH + 20, pbar_h);

	vpanel_dump_bmp(&panel, "/tmp/vpanel_demo.bmp");
	printf("vpanel demo: %u draw ops rendered, wrote /tmp/vpanel_demo.bmp\n",
	       panel.frames_rendered);
	return 0;
}
