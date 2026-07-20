/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wire_to_vpanel.c - translates a captured dmesg/console log containing this
 * project's "DIAG <opcode> wire decode: ..." lines (OmapNKS4Module/usb.cpp's
 * vm_usb_submit_urb(), see that file's own 2026-07-19 comment block) into
 * real calls against vpanel.c's actual COmapNKS4VideoAPI-equivalent draw API,
 * producing a real, viewable BMP of what a vm_virtual_probe=1 boot's video-
 * bulk traffic actually drew.
 *
 * This is glue, not reconstruction: every numeric field it reads was already
 * decoded by usb.cpp's own ground-truth-cited wire-format logic (see
 * OmapNKS4Module/video.cpp's "CORRECTION, 2026-07-18" comment above
 * ProcessEvents() for the byte-layout citations backing each field below).
 * This file just calls vpanel's real draw functions with the captured
 * values, in the order they were captured - it does not reinterpret,
 * re-decode, or invent anything.
 *
 * Known, deliberate limitation (see OmapNKS4Module/README.md's "Bulk-video/
 * URB-pool path confirmed working" section, "Still open, not exercised by
 * this test"): the DIAG SendPixelDataRegion line only captures the 0xc2
 * *header* (width/offset/rowBytes) - the actual pixel bytes ride over a
 * SEPARATE streaming path (ContinueProcessingEvent's 0xc6 chunks + 0x83 end
 * marker) that this diagnostic does not capture. This tool counts those
 * headers but never fabricates pixel content for them - only opcodes with a
 * captured, real payload are actually drawn.
 *
 * Usage: wire_to_vpanel <captured_log_file> <output.bmp>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vpanel.h"

int main(int argc, char **argv)
{
	int n_fill = 0, n_pixel_hdr = 0, n_pal = 0, n_lcd = 0, n_xaxis = 0;
	int n_lines = 0, n_unparsed_diag = 0;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <captured_log_file> <output.bmp>\n", argv[0]);
		return 2;
	}

	FILE *in = fopen(argv[1], "r");
	if (!in) {
		perror("fopen input");
		return 1;
	}

	struct vpanel panel;
	vpanel_init(&panel);

	char line[1024];
	while (fgets(line, sizeof(line), in)) {
		/* Only lines from vm_usb_submit_urb()'s video wire-decode diagnostic
		 * (usb.cpp) are candidates here - this module's log also carries other,
		 * unrelated "DIAG ..." lines (e.g. main.cpp's worker-thread/scheduler
		 * diagnostics) that this tool has nothing to do with and shouldn't flag
		 * as "unrecognized". */
		char *p = strstr(line, "vm_virtual_probe: DIAG ");
		if (!p)
			continue;
		p += strlen("vm_virtual_probe: ");

		unsigned int width, base, color, height, bytes, reg, val, data_lo,
			     offset, rowBytes, a, b, c, d, cmd, len;

		if (sscanf(p, "DIAG SendFillData wire decode: width=%u base=%u color=%u "
			      "height=%u (raw cmd=0x%x len=%u)",
			   &width, &base, &color, &height, &cmd, &len) == 6) {
			vpanel_send_fill(&panel, (uint8_t)color, (int)width, (int)base, (int)height);
			n_fill++;
			n_lines++;
			continue;
		}
		if (sscanf(p, "DIAG UpdateColorPal wire decode: a=%u b=%u c=%u d=%u "
			      "(raw cmd=0x%x len=%u)",
			   &a, &b, &c, &d, &cmd, &len) == 6) {
			vpanel_update_color_pal(&panel, (uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d);
			n_pal++;
			n_lines++;
			continue;
		}
		if (sscanf(p, "DIAG InitLCDRegs wire decode: reg=%u val=%u data_lo=%u "
			      "(raw cmd=0x%x len=%u)",
			   &reg, &val, &data_lo, &cmd, &len) == 5) {
			vpanel_init_lcd_regs(&panel, (char)reg, (char)val, (int)data_lo);
			n_lcd++;
			n_lines++;
			continue;
		}
		if (sscanf(p, "DIAG XAxisByteSize wire decode: bytes=%u (raw cmd=0x%x len=%u)",
			   &bytes, &cmd, &len) == 3) {
			vpanel_x_axis_bytesize(&panel, (int)bytes);
			n_xaxis++;
			n_lines++;
			continue;
		}
		if (sscanf(p, "DIAG SendPixelDataRegion wire decode: width=%u offset=%u "
			      "rowBytes=%u (raw cmd=0x%x len=%u)",
			   &width, &offset, &rowBytes, &cmd, &len) == 5) {
			n_pixel_hdr++;
			n_lines++;
			fprintf(stderr,
				"note: SendPixelDataRegion header seen (width=%u offset=%u "
				"rowBytes=%u) but no pixel payload was captured by this "
				"diagnostic - not drawn (see this file's own header comment)\n",
				width, offset, rowBytes);
			continue;
		}

		n_unparsed_diag++;
		fprintf(stderr, "warning: unrecognized video DIAG line, skipped: %s", p);
	}
	fclose(in);

	if (vpanel_dump_bmp(&panel, argv[2]) != 0) {
		fprintf(stderr, "failed to write %s\n", argv[2]);
		return 1;
	}

	printf("wire_to_vpanel: parsed %d draw-command line(s) from %s\n", n_lines, argv[1]);
	printf("  SendFillData:         %d (drawn)\n", n_fill);
	printf("  UpdateColorPal:       %d (drawn)\n", n_pal);
	printf("  InitLCDRegs:          %d (no-op on this panel model, see vpanel.c)\n", n_lcd);
	printf("  XAxisByteSize:        %d (no-op on this panel model, see vpanel.c)\n", n_xaxis);
	printf("  SendPixelDataRegion:  %d header(s) seen, 0 drawn (no pixel payload captured)\n",
	       n_pixel_hdr);
	if (n_unparsed_diag)
		printf("  WARNING: %d unrecognized DIAG line(s) skipped - see stderr\n",
		       n_unparsed_diag);
	printf("vpanel draw ops actually applied to the framebuffer: %u\n", panel.frames_rendered);
	printf("wrote %s\n", argv[2]);
	return 0;
}
