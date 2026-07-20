/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wire_dispatch.c - the panel firmware's two central dispatch loops:
 *
 *   - wire_dispatch_command (FUN_c0007d1c) - the single entry point for
 *     every incoming 32-bit command word from the host, decoding an opcode
 *     byte and routing to essentially every subsystem this project has
 *     reconstructed (clcdc, cpsoc, cad, crypto_at88, ...). The panel-side
 *     counterpart of the host's own COmapNKS4Command wire protocol
 *     (kronosology/reconstructed/OmapNKS4Module/command.cpp).
 *   - master_dispatch_tick (FUN_c0008b64) - the status-bit dispatcher called
 *     unconditionally, forever, from eva_board_main.c's own main loop: reads
 *     one hardware status register per call and fans out to ~10 subsystem
 *     handlers by bit, including triggering wire_dispatch_command's own
 *     upstream callers' work indirectly (crypto_at88's queue relay, cad's
 *     calibration pump, ctouchpanel's sample/timeout pair) and, at the very
 *     end, directly driving the USB transfer-completion state machine
 *     (omap_l137_usbdc.c's own omap_usbdc_poll_transfer, via its thin
 *     wrapper) for whatever payload the tick's own handle carries.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-18 pass
 * (static dump only, no live Ghidra this pass - see project README).
 *
 * File placement: neither function has its own __FILE__ string anchor (a
 * full image string search near both addresses, 0xc0007000-0xc0009200,
 * found nothing usable - the "../EvaBoardMain.cpp" string itself is the
 * closest textual anchor in the image, but it belongs to
 * eva_board_watchdog_fault_wrapper in eva_board_main.c, not to either
 * function here). Given their size (1904 and 1240 bytes - both far larger
 * than a single-purpose helper) and their role as shared, cross-subsystem
 * dispatch points rather than board-bring-up code, they're kept in their
 * own file instead of folded into eva_board_main.c - matching this
 * project's existing precedent of giving a large, structurally distinct,
 * unanchored function cluster (e.g. cpsoc.c's own third-SPI-device
 * cluster) its own section once its role is independently confirmed, here
 * taken one step further into its own file since BOTH functions qualify
 * and neither belongs to eva_board_main.c's own "board bring-up" scope.
 */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 *  wire_dispatch_command - FUN_c0007d1c, @0xc0007d1c (1904 bytes)
 * ============================================================================
 *
 *  Two real callers (both confirmed via xrefs_to, not guessed):
 *   - FUN_c0003e24, call site 0xc0004360 - a large (1812-byte) USB
 *     status-register-driven state machine in the same low address range as
 *     omap_l137_usbdc.c's own confirmed functions; that file's own USB
 *     interrupt/poll handler.
 *   - FUN_c000a918, call site 0xc000a96c - a thin shim: reads a length via
 *     FUN_c0004b5c, copies the payload via FUN_c0004858, then calls
 *     straight into wire_dispatch_command. FUN_c000a918 itself is one
 *     `case` (value 5) of a larger switch in FUN_c000aae0 (cases 0/1/2/3/
 *     5/7/9 - case 0 also calls FUN_c0004b5c/FUN_c0004858 directly, cases
 *     1/2 call other FUN_c0009xxx-range functions), all clearly USB
 *     receive-path plumbing.
 *
 *   RESOLVED 2026-07-19: all three (FUN_c0003e24 = usbdc_core_isr,
 *   FUN_c000a918 = usbdc_ep_recv_bulk, FUN_c000aae0 =
 *   usbdc_endpoint_event_dispatch) are now fully reconstructed - in
 *   omap_l137_usbdc_ext.c, NOT this file (they sit in omap_l137_usbdc.c's
 *   own address neighborhood/scope, out of this file's own scope). A
 *   SECOND, previously-undocumented wire_dispatch_command call site was
 *   found in the process, inside usbdc_core_isr's own EP0 SETUP-packet
 *   handling path (distinct from usbdc_ep_recv_bulk's bulk-OUT call site
 *   above) - see omap_l137_usbdc_ext.c's own header and per-function
 *   comments for the full trace, live-Ghidra-verified DAT_ constant values,
 *   and two real bugs the earlier static-dump-only draft of that file had
 *   (fixed there, not here).
 *
 *  This confirms the wiring this project's own KRONOS_V06R06.VSB.md doc
 *  left as "not yet traced": eva_board_main's main loop does NOT call this
 *  dispatcher directly with USB data - the USB receive path (both a large
 *  ISR/poll state machine and a smaller opcode-switch shim) calls it
 *  independently, on its own schedule, whenever a full command arrives.
 *  master_dispatch_tick (below) is the OTHER, unconditional-tick caller
 *  context that everything else in this project already tied to this
 *  dispatcher's own downstream effects (crypto_at88's queue relay, clcdc's
 *  progress bar) - the two are siblings, not caller/callee of each other.
 *
 *  Opcode table (byte 3 of each 4-byte command word, high bit set = the
 *  "0xc0-0xff" extended-opcode family, high bit clear = the low literal
 *  family below) - cross-checked against KRONOS_V06R06.VSB.md's own
 *  "wire-protocol command dispatcher" table, which was derived from this
 *  same function independently:
 *
 *   0xc0        clcdc register write/set-bits/clear-bits (clcdc_reg_write/
 *               _set_bits/_clear_bits, selected by a sub-byte of the word)
 *   0x81        clcdc_cursor_set_stride
 *   0xc2        sets up a pixel-region transfer context on the handle
 *               (offsets +0xc/+0x14/+0x18/+0x20)
 *   0x83        clears that same pixel-region transfer context
 *   0xc5        clcdc_dispatch_set_palette_hook (palette update)
 *   0xc6, 0xc4  early return - deferred to a streaming continuation
 *               (this function's own equivalent of the host's
 *               ContinueProcessingEvent), NOT consumed here
 *   199 (0xc7)  LCD brightness (eva_lcd_set_brightness, not independently
 *               traced elsewhere in this project)
 *   0xe0        AT88 relay write path - reassembles a variable-length
 *               payload into the shared AT88 relay queue state
 *               (DAT_c00084c4, the same state byte crypto_at88.c's own
 *               queue relay reads)
 *   0xe1        AT88 relay read path (AtmelRead) - same shared queue state
 *   0x50/0x51/0x52 (op-byte 0, one-byte value) or (op-byte 1, two-byte
 *               value) - cpsoc.c's three register-bank read wrappers
 *               (cpsoc_read_switch_row/_read_led_row/_read_switch_row_clear)
 *   0x80        cad pedal object mode set (op-byte 0)
 *   0x83        cad pedal "send release" (op-byte 1, gated on a flag byte)
 *   0xd0/0xd1   calibration-slot init/mark-changed style calls into BOTH
 *               a cad.c context (cad_calibration_init_slot, confirmed by
 *               shared handle address, see below) and a second,
 *               unattributed context (FUN_c00148fc, address range shared
 *               with ctouchpanel.c) - not yet fully attributed
 *   0xa0-0xaf, 0xb0-0xbf, 0xc0-0xcf (op-byte 1 sub-ranges) - cad
 *               calibration slot lookups (FUN_c0014488, cad_calibration_
 *               mark_changed, cad_calibration_slot_is_raw,
 *               cad_calibration_init_slot)
 *   0xf0-0xff (op-byte 1) - cpsoc_i2c_dispatch(handle, 0x78, 0x70, 0)
 *   0xee        comm-check (CommunicationCheck)
 *   0xf0 (op-byte 0)  version query - hard-faults via the shared
 *               crypto_at88_fault/clcdc_assert handler if a state byte at
 *               handle+0x40 reads -1 (the same sentinel eva_board_final_
 *               setup writes there during bring-up)
 *   1, 4, 5, 6, 7, 9 (bare op-bytes, no sub-register byte) - misc: a
 *               fixed-delay call, a small register write, two flag-set
 *               calls, and a diagnostic text-draw-and-halt-briefly sequence
 *               (op-byte 9 - draws DAT_c00084b8 at (100, 0x230) then calls
 *               an unidentified FUN_c0000ba0, the exact call cpsoc.c's own
 *               notes independently flagged as "not yet identified" from
 *               the cpsoc.diag_menu side - same call site, now attributed
 *               to this dispatcher's op-byte 9)
 *   0xd0/0xd1/0x80 (op-byte 0 family) - not yet attributed to a known
 *               host-side command name (per the .md doc's own table)
 *
 *  CONFIRMED cross-file handle sharing (address-level, not guessed): the
 *  context pointer this function reads for its cpsoc register-bank calls
 *  (DAT_c0008490) resolves to the exact same address as eva_board_final_
 *  setup's own DAT_c0007604 - i.e. the context FUN_c0012724 (eva_board_
 *  final_setup's still-unattributed callee that zeroes 0x49/73 bytes) zeroes
 *  at boot IS the cpsoc register-bank context this dispatcher reads from on
 *  every 0x50/0x51/0x52 command. Likewise DAT_c0008494 (cad pedal calls)
 *  resolves to the exact same address as eva_board_final_setup's own
 *  cad_pedal_handle (DAT_c000760c), and DAT_c000849c (cad_calibration_
 *  init_slot's own first argument here) resolves to the exact same address
 *  as eva_board_final_setup's own cad_handle (DAT_c0007610). All three
 *  confirmed by direct value comparison of the resolved DAT_ constants in
 *  this pass's data dump, not by proximity or guesswork.
 *
 *  The dense per-opcode payload-reassembly arithmetic for 0xe0 (a real,
 *  non-trivial byte-shuffling loop reversing a variable-length AT88 payload
 *  4 bytes at a time) is preserved faithfully below rather than simplified,
 *  since crypto_at88.c's own re-verification pass already found real
 *  byte-order bugs in nearby AT88 relay code - this is exactly the kind of
 *  dense, easy-to-mistranscribe logic that pass was written to catch, so it
 *  is transcribed as literally as the decompile allows instead of
 *  "cleaned up".
 * ------------------------------------------------------------------------- */

/* --- clcdc.c (CONFIRMED matches, cited not redefined) --- */
extern void clcdc_reg_write(void *ctl, uint32_t reg_offset, uint32_t value);		/* FUN_c0015094 */
extern void clcdc_reg_set_bits(void *ctl, uint32_t reg_offset, uint32_t mask);		/* FUN_c00150a4 */
extern void clcdc_reg_clear_bits(void *ctl, uint32_t reg_offset, uint32_t mask);	/* FUN_c00150bc */
extern void clcdc_cursor_set_stride(void *cursor, uint32_t stride);			/* FUN_c0015010 */
extern void clcdc_dispatch_set_palette_hook(void *param);				/* FUN_c0015018 */
extern void clcdc_draw_text(uint16_t x, uint16_t y, const char *str, uint32_t font_or_mode); /* FUN_c0015650 */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000919c, shared assert/halt handler */

/* --- cpsoc.c (CONFIRMED matches) --- */
extern void cpsoc_read_switch_row(uint8_t *row_state, int index);		/* FUN_c00127e0, reg 0x50 */
extern void cpsoc_read_led_row(void *dest, int index);			/* FUN_c0012794, reg 0x51 */
extern void cpsoc_read_switch_row_clear(uint8_t *row_state, int index);	/* FUN_c00127ac, reg 0x52 */
extern void cpsoc_i2c_dispatch(void *handle, uint8_t reg, uint32_t out_value, uint8_t raw_bit);	/* FUN_c0007120 */

/* --- cad.c (CONFIRMED matches) --- */
extern void cad_pedal_object_set_mode(void *obj, uint8_t mode);		/* FUN_c00133ec, reg 0x80 (op-byte 0) */
extern void cad_pedal_send_release(void *obj, int value);			/* FUN_c00133ac, reg 0x83 (op-byte 1) */
extern void cad_calibration_mark_changed(void *cad, int slot);		/* FUN_c001385c */
extern void cad_calibration_init_slot(void *cad, int slot, uint8_t threshold, uint8_t cap);	/* FUN_c001381c */
extern int  cad_calibration_slot_is_raw(void *cad, int slot);			/* FUN_c0013910 */
extern int cad_calibration_lookup(int key);					/* FUN_c0014488 - shared lookup table, also referenced from ctouchpanel.c/cobjectmgr.c per that file's own note */

/* --- own/unattributed callees - generic names, FUN_ address is the real anchor --- */
extern void eva_wire_report_code(int subsystem_id, uint32_t code);		/* FUN_c001d22c - a notify/report primitive, NOT a hard fault (distinct from crypto_at88_fault). Every real call site passes the LITERAL 1 as subsystem_id, never `handle` */
extern void eva_lcd_set_brightness(void *handle, uint8_t value);		/* FUN_c0007ccc, opcode 0xc7 - not independently traced elsewhere */
extern void eva_wire_ctx_c0008498_calib(void *ctx, unsigned arg);		/* FUN_c00148fc, address range shared with ctouchpanel.c, not attributed - real call sites use 0, 1, or 2 args (see 0xd0's own citation comment below); only the 2-arg (0xd1) call site is reproduced as a real call here */
extern void eva_wire_delay(void *ctx, uint16_t code);				/* FUN_c0014f4c, opcode 4 - ctx confirmed == eva_board_final_setup's DAT_c0007618 by resolved-address match */
extern void eva_wire_reg_write(void *ctx, uint8_t reg, uint16_t value);	/* FUN_c0012814, opcode 5 */
extern void eva_wire_flag_a_set(void *ctx);					/* FUN_c000ef60, opcode 6 */
extern void eva_wire_flag_b_set(void *ctx, uint8_t bit);			/* FUN_c000eee4, opcode 7 */
extern void eva_wire_diag_fill(void *dst, const void *src, int len);		/* FUN_c0016804, opcode 9 - dst (DAT_c00084b8) is filled from src (DAT_c00084b4), then drawn as text below, so dst must be mutable */
extern void eva_wire_diag_halt_step(void);					/* FUN_c0000ba0, opcode 9 - same call cpsoc.c's own diag-menu notes flag as "not yet identified" */
extern void eva_wire_reg_low_dispatch(void *handle, uint8_t value);		/* FUN_c0005738, reg<0x10 case (op-byte 0) */
extern bool eva_wire_at88_ready(void *ctx);					/* FUN_c0000f6c, gates the 0x2000 fault-report call on 0xe0/0xe1 */

void *wire_dispatch_command(void *handle, uint8_t *cmd, unsigned len)	/* FUN_c0007d1c */
{
	extern uint8_t wire_reentry_guard;		/* DAT_c000848c */
	extern uint8_t *wire_at88_relay_state;		/* DAT_c00084c4 - shared with crypto_at88.c's queue relay */
	extern void *wire_cpsoc_ctx;			/* DAT_c0008490 - CONFIRMED == eva_board_final_setup's DAT_c0007604 */
	extern void *wire_cad_pedal_ctx;		/* DAT_c0008494 - CONFIRMED == eva_board_final_setup's cad_pedal_handle */
	extern void *wire_cad_calib_ctx_a;		/* DAT_c0008498 */
	extern void *wire_cad_ctx;			/* DAT_c000849c - CONFIRMED == eva_board_final_setup's cad_handle */
	extern const char *wire_version_fault_file;	/* DAT_c00084a0 - passed to crypto_at88_fault as `file` */
	extern int wire_version_fault_line;		/* DAT_c00084a4, resolved literal value 0x777 - a real line-number-shaped constant, not a pointer */
	extern void *wire_delay_ctx;			/* DAT_c00084a8 */
	extern void *wire_flag_a_ctx;			/* DAT_c00084ac */
	extern uint8_t *wire_flag_b_flag;		/* DAT_c00084b0 */
	extern const char *wire_diag_text;		/* DAT_c00084b4 - fill source */
	extern char *wire_diag_text_alt;		/* DAT_c00084b8 - fill destination, then drawn as text; must be mutable */
	extern void *wire_i2c_ctx;			/* DAT_c00084bc */
	extern uint16_t *wire_pixel_table_base;	/* DAT_c00084c0 */
	extern void *wire_at88_ready_ctx;		/* DAT_c00084c8 - gates the 0x2000 fault-report call on 0xe0/0xe1, distinct from wire_flag_a_ctx (DAT_c00084ac) */

	if (wire_reentry_guard != 0)
		return (void *)1;

	*(int *)((uint8_t *)handle + 0x34) += 1;

	while (len > 2 || len == 3) {
		uint8_t *at88 = wire_at88_relay_state;
		uint8_t opcode = cmd[3];

		if ((int8_t)opcode < 0) {
			/* extended-opcode family (0x80-0xff) */
			if (opcode == 0xc0) {
				uint8_t sub = cmd[1];
				uint32_t val = cmd[0] | (cmd[6] << 0x10) | (cmd[5] << 0x18) | (cmd[7] << 8);
				uint8_t *next = cmd + 4;
				len -= 4;
				if (cmd[2] == 1)
					clcdc_reg_set_bits(wire_i2c_ctx, sub, val);
				else if (cmd[2] == 2)
					clcdc_reg_clear_bits(wire_i2c_ctx, sub, val);
				else
					clcdc_reg_write(wire_i2c_ctx, sub, val);
				cmd = next;
			} else if (opcode == 0x81) {
				clcdc_cursor_set_stride(wire_i2c_ctx, (cmd[1] << 8) | (cmd[0] << 0x10) | cmd[2]);
			} else if (opcode == 0xc2) {
				uint8_t *h = (uint8_t *)handle;
				uint32_t region_len = (cmd[9] << 8) | (cmd[8] << 0x10) | cmd[10];
				uint32_t base = (uint32_t)wire_pixel_table_base +
						 (((cmd[5] << 8) | (cmd[4] << 0x10) | cmd[6]) * 2);
				*(uint32_t *)(h + 0xc) = (cmd[1] << 8) | (cmd[0] << 0x10) | cmd[2];
				*(uint32_t *)(h + 0x18) = base + region_len * 2 - 2;
				*(uint32_t *)(h + 0x20) = region_len;
				*(uint32_t *)(h + 0x14) = base;
				cmd += 8;
				len -= 8;
			} else if (opcode == 0x83) {
				*(uint32_t *)((uint8_t *)handle + 0xc) = 0;
			} else if (opcode == 0xc5) {
				clcdc_dispatch_set_palette_hook(wire_i2c_ctx);
				/* real args: (ctx, reg=cmd[2], hi=cmd[1], lo=cmd[0], extra=cmd[7]) per raw decompile - not fully re-derived here */
				cmd += 4;
				len -= 4;
			} else if (opcode == 0xc6 || opcode == 0xc4) {
				/* deferred to a streaming continuation - not consumed here */
				*(uint8_t **)((uint8_t *)handle + 0x10) = cmd;
				*(uint32_t *)((uint8_t *)handle + 8) = len;
				return (void *)0;
			} else if (opcode == 199) {
				eva_lcd_set_brightness(handle, cmd[2]);
			} else if (opcode == 0xe0) {
				/* AT88 relay write path - variable-length payload reassembly,
				 * preserved literally (dense byte-shuffle, see file note above) */
				uint32_t rem = len - 4;
				wire_at88_relay_state[0] = cmd[2];
				at88[1] = cmd[1];
				uint8_t *src = cmd + 4;
				at88[2] = cmd[0];
				at88[3] = cmd[7];
				unsigned n = at88[3];
				at88[4] = cmd[6];
				at88[5] = cmd[5];
				at88[6] = *src;
				if (n > 3) {
					/* variable-length tail copy, 4 bytes/iteration, byte-reversed
					 * per dword - not independently re-derived past what the raw
					 * decompile shows; treat with the same caution
					 * crypto_at88.c's own re-verification pass applied to nearby
					 * AT88 relay byte-order code (real bugs were found there). */
					src = cmd + 8;
					rem = len - 8;
					/* ... dense reassembly loop, see FUN_c0007d1c raw decompile
					 * for the exact index arithmetic; not transcribed further to
					 * avoid presenting an unverified byte-order guess as fact. */
				}
				cmd = src;
				len = rem;
				if (at88[3] < 0x21 && eva_wire_at88_ready(wire_at88_ready_ctx)) {
					eva_wire_report_code(1, 0x2000);
				}
			} else if (opcode == 0xe1) {
				wire_at88_relay_state[0] = cmd[2] | 1;
				len -= 4;
				at88[1] = cmd[1];
				uint8_t *next = cmd + 4;
				at88[2] = cmd[0];
				at88[3] = cmd[7];
				cmd = next;
				if (at88[3] < 0x21 && eva_wire_at88_ready(wire_at88_ready_ctx)) {
					eva_wire_report_code(1, 0x2000);
				}
			}
		} else if (opcode == 0) {
			uint8_t reg = cmd[2];
			if ((reg & 0xf0) == 0)
				eva_wire_reg_low_dispatch(handle, cmd[1]);
			else if (reg == 0x50)
				cpsoc_read_switch_row(wire_cpsoc_ctx, cmd[1]);
			else if (reg == 0x51)
				cpsoc_read_led_row(wire_cpsoc_ctx, cmd[1]);
			else if (reg == 0x52)
				cpsoc_read_switch_row_clear(wire_cpsoc_ctx, cmd[1]);
			else if (reg == 0x80)
				cad_pedal_object_set_mode(wire_cad_pedal_ctx, cmd[1]);
			else if (reg == 0xd0) {
				/* NOT faithfully transcribed past the branch skeleton: the raw
				 * decompile has this whole case fall through into a SECOND,
				 * shared call to cad_calibration_init_slot (label LAB_c0008034)
				 * using a value (uVar4) that's also reached via a `goto` from
				 * the op-byte-1 bank==0xc0 case below with a DIFFERENT,
				 * stale value of that same variable - a genuine shared-tail/
				 * carried-variable pattern that's easy to mistranscribe into a
				 * real bug (see crypto_at88.c's own re-verification pass for
				 * precedent). Cited structurally rather than guessed at:
				 *   slot < 6:      FUN_c00148fc(ctx_a); cad_calibration_init_slot(cad, 0x20, slot+4, slot+8); then falls through to the shared tail with uVar4=0x21, bVar5=slot+4, bVar7=slot+8
				 *   slot == 6/7:   FUN_c00148fc(ctx_a, 1 or 2); cad_calibration_init_slot(cad, 0x20, 4, 8); then the same shared tail with uVar4=0x21, bVar5=4, bVar7=8
				 *   slot >  7:     goto LAB_c0008474 (skips straight to the loop tail, no calibration call at all)
				 * The shared tail itself: cad_calibration_init_slot(cad, uVar4, bVar5, bVar7).
				 */
				uint8_t slot = cmd[1];
				(void)slot;
			} else if (reg == 0xd1) {
				eva_wire_ctx_c0008498_calib(wire_cad_calib_ctx_a, cmd[1] * 10u);
			} else if (reg == 0xee) {
				eva_wire_report_code(1, 0x10);	/* CommunicationCheck */
			} else if (reg == 0xf0) {
				if (*((int8_t *)handle + 0x40) == -1)
					crypto_at88_fault(0, wire_version_fault_file, wire_version_fault_line);
				eva_wire_report_code(1, 0x20);	/* GetVersion */
			}
			/* any other reg value under op-byte 0: no call at all (the raw
			 * decompile just falls through to the loop tail, LAB_c0008474 -
			 * confirmed NOT a fault path, corrected from an earlier draft of
			 * this file that wrongly invented a report call here) */
		} else if (opcode == 1) {
			uint8_t reg = cmd[2];
			uint8_t bank = reg & 0xf0;
			uint16_t val16 = (uint16_t)((cmd[0] << 8) | cmd[1]);
			if (reg == 0x50)
				cpsoc_read_switch_row(wire_cpsoc_ctx, val16);
			else if (reg == 0x51)
				cpsoc_read_led_row(wire_cpsoc_ctx, val16);
			else if (reg == 0x52)
				cpsoc_read_switch_row_clear(wire_cpsoc_ctx, val16);
			else if (reg == 0x83) {
				if (*((int8_t *)wire_cad_pedal_ctx + 3) == 0)
					cad_pedal_send_release(wire_cad_pedal_ctx, cmd[1] & 1);
			} else if (bank == 0xa0) {
				int entry = cad_calibration_lookup(cmd[1]);
				cad_calibration_mark_changed(wire_cad_ctx, entry);
			} else if (bank == 0xb0) {
				int slot;
				for (slot = 0; slot < 0x26; slot++) {
					if (!cad_calibration_slot_is_raw(wire_cad_ctx, slot))
						cad_calibration_init_slot(wire_cad_ctx, slot, 0, 0);
				}
			} else if (bank == 0xc0) {
				int entry = cad_calibration_lookup(cmd[1]);
				if (!cad_calibration_slot_is_raw(wire_cad_ctx, entry)) {
					/* NOT faithfully transcribed: the raw decompile re-derives a
					 * register/bank pair from cmd[0]'s nibbles and jumps INTO the
					 * opcode-0xd0 handler's own shared tail above (LAB_c0008034),
					 * calling cad_calibration_init_slot with whatever value that
					 * handler's own uVar4 last held - a genuine cross-branch
					 * shared-tail dependency, not reproduced here to avoid
					 * guessing at carried-variable state. See the 0xd0 case above
					 * for the full citation of what LAB_c0008034 actually does. */
				}
			} else if (bank == 0xf0) {
				cpsoc_i2c_dispatch(handle, 0x78, 0x70, 0);
			}
		} else if (opcode == 4) {
			eva_wire_delay(wire_delay_ctx, 0x1e);
		} else if (opcode == 5) {
			eva_wire_reg_write(wire_cpsoc_ctx, cmd[2], (uint16_t)cmd[1] + (uint16_t)cmd[0] * 0x100);
		} else if (opcode == 6) {
			eva_wire_flag_a_set(wire_flag_a_ctx);
			*wire_flag_b_flag = 1;
		} else if (opcode == 7) {
			eva_wire_flag_b_set(wire_flag_a_ctx, (cmd[2] >> 1) & 1);
		} else if (opcode == 9) {
			eva_wire_diag_fill(wire_diag_text_alt, wire_diag_text, 0x20);
			clcdc_draw_text(100, 0x230, wire_diag_text_alt, 0);
			eva_wire_diag_halt_step();
		}

		len -= 4;
		cmd += 4;
	}

	return (void *)1;
}

/* ============================================================================
 *  master_dispatch_tick - FUN_c0008b64, @0xc0008b64 (1240 bytes)
 * ============================================================================
 *
 *  Called unconditionally, forever, from eva_board_main's own main loop (see
 *  eva_board_main.c). Reads one hardware status register (via a pair of
 *  calls, wire_status_read/_ack) into a local bitmask, then fans out by bit:
 *
 *   0x0100  eva_board_link_status_change (FUN_c0008a5c) - not traced
 *   0x0001  FUN_c00087c4 - not traced
 *   0x0002  FUN_c000594c - not traced
 *   0x0004  cad_calibration_progress_pump - CONFIRMED (omap_l108.c). Real
 *           call site passes `handle` as an argument despite that file's own
 *           signature taking none - the same "phantom forwarded parameter"
 *           pattern already found twice elsewhere in this project
 *           (cdix4192.c's register wrappers, eva_board_watchdog_fault_
 *           wrapper) - a real, recurring transcription risk, not fixed here.
 *   0x0008  ctouchpanel_sample_raw + ctouchpanel_update - CONFIRMED
 *           (ctouchpanel.c). Ties ctouchpanel's own per-tick sampling
 *           directly to this dispatcher, closing a loop that file's own
 *           "called every master-dispatcher tick" note for
 *           ctouchpanel_check_timeout already implied but didn't show for
 *           the sample/update pair itself.
 *   0x0010  clears a ready flag, calls FUN_c000590c - not traced
 *   (unconditional) if a "ready" flag is set and a free-running counter
 *           exceeds a threshold: draws three fixed text lines via
 *           clcdc_draw_text - a status/diagnostic overlay, not gated by any
 *           status bit itself.
 *   0x0020  FUN_c0005d34, then updates handle+0x38/+0x30/+0x2d - a
 *           transfer-accounting reset (+0x38 <- +0x34, +0x30 <- 0), same
 *           handle fields wire_dispatch_command's own reentry counter
 *           (+0x34) increments on every call.
 *   0x0040  FUN_c0005cc4 - not traced
 *   0x1000  FUN_c0010f08, then ctouchpanel_check_timeout - CONFIRMED
 *           (ctouchpanel.c's own documented call site).
 *   0x2000  crypto_at88_process_queue - CONFIRMED (crypto_at88.c's own
 *           documented trigger context - this IS the "FUN_c0008b64, the
 *           firmware's central interrupt-status dispatch loop" that file's
 *           own notes already named without reconstructing).
 *   0x8000  increments handle+0x24 (a chunk counter), calls FUN_c0015420 -
 *           not traced.
 *
 *  Below the bit fan-out: a dense state machine (~80 lines in the raw
 *  decompile) around a cluster of DAT_c00090xx globals, all in the same
 *  low-0xc0009xxx/0xc000axxx/0xc000bxxx/0xc000cxxx address neighborhood as
 *  omap_l137_usbdc.c's own confirmed functions (omap_usbdc_reloc @0xc0009194,
 *  the USB object bring-up caller @0xc0009574) - plausibly USB
 *  configure/endpoint-state bookkeeping (enable/disable, halt/clear-halt
 *  style toggles), NOT independently traced or attributed to that file this
 *  pass (out of this file's own scope - omap_l137_usbdc.c is owned by other
 *  concurrent work). Preserved as raw FUN_/DAT_ calls below rather than
 *  guessed at.
 *
 *  CONFIRMED, and the single most important finding of this pass: the very
 *  last call in the function,
 *
 *      FUN_c000acc8(DAT_c0009098, *(handle+0x28) + *(handle+0x24)*2000)
 *
 *  is a direct call into omap_l137_usbdc.c's own confirmed thin wrapper
 *  around omap_usbdc_poll_transfer (that file's own note: "FUN_c000acc8,
 *  0x24 bytes before FUN_c000acec"). This means master_dispatch_tick
 *  doesn't just trigger OTHER subsystems' USB sends indirectly - it
 *  directly drives the USB transfer-completion state machine itself, once
 *  per tick, with a length computed from the same handle+0x24/+0x28 fields
 *  the 0x20-bit handler above resets and the 0x8000-bit handler increments.
 *  Given omap_usbdc_poll_transfer's own documented 8001-byte "large
 *  transfer" threshold and 3-state (idle/in-flight/complete) machine, the
 *  `+0x24 * 2000` term reads as a real chunked-submission stride - i.e. this
 *  dispatcher is pumping a large transfer through in ~2000-byte-equivalent
 *  chunks, one attempt per tick, until omap_usbdc_poll_transfer's own state
 *  machine reports completion. Not independently confirmed against
 *  hardware, but a concrete, address-verified structural finding, not a
 *  guess.
 * ------------------------------------------------------------------------- */
extern void wire_status_read(int bank, void *reg, int mode, uint32_t *out);	/* FUN_c001d3a8 */
extern void wire_status_ack(int bank, void *reg);				/* FUN_c001d318 */
extern void *wire_status_reg;							/* DAT_c000903c */

extern void eva_link_status_change(void *handle);	/* FUN_c0008a5c, bit 0x100 */
extern void eva_tick_unk_1(void *handle);		/* FUN_c00087c4, bit 0x1 */
extern void eva_tick_unk_2(void *handle);		/* FUN_c000594c, bit 0x2 */
extern void cad_calibration_progress_pump(void);	/* FUN_c0005a1c, omap_l108.c - CONFIRMED, real call site passes `handle` despite this signature (phantom-param pattern) */
extern int  ctouchpanel_sample_raw(void *tp, uint8_t out[7]);		/* FUN_c0014010, ctouchpanel.c - CONFIRMED */
extern void ctouchpanel_update(void *tp, uint8_t new_sample[8]);	/* FUN_c0014d80, ctouchpanel.c - CONFIRMED */
extern void eva_tick_unk_3(void *handle);		/* FUN_c0005b14, bit 0x8 tail */
extern void eva_tick_unk_4(void *handle);		/* FUN_c000590c, bit 0x10 */
extern void clcdc_draw_text(uint16_t x, uint16_t y, const char *str, uint32_t font_or_mode); /* FUN_c0015650 */
extern void eva_tick_unk_5(void *handle);		/* FUN_c0005d34, bit 0x20 */
extern void eva_tick_unk_6(void *handle);		/* FUN_c0005cc4, bit 0x40 */
extern void eva_tick_unk_7(void *ctx);			/* FUN_c0010f08, bit 0x1000 - concurrently reconstructed as panelbus_dispatch.c's own panelbus_poll_channels(struct panelbus_tx_channel *) in this same pass; not renamed here to avoid a mid-edit dependency on a file owned by other concurrent work, see that file for the real signature/name */
extern void ctouchpanel_check_timeout(void *tp);	/* FUN_c001422c, ctouchpanel.c - CONFIRMED */
extern void crypto_at88_process_queue(void *unused_param);	/* FUN_c0005e9c, crypto_at88.c - CONFIRMED */
extern void eva_tick_unk_8(uint32_t ctx);		/* FUN_c0015420, bit 0x8000 */
extern bool omap_usbdc_poll_transfer_submit(void *ep, int32_t len);	/* FUN_c000acc8, omap_l137_usbdc.c's own thin wrapper - CONFIRMED */
extern void *wire_usb_ep_handle;			/* DAT_c0009098 */

/* the ~80-line USB-adjacent state-machine cluster (DAT_c000907x-c00090bx)
 * referenced above is NOT individually declared or transcribed here - it is
 * cited only in the structural description above, consistent with this
 * project's treatment of dense, unattributed logic elsewhere (e.g.
 * clcdc_blit_glyph's bit-shift math). Reconstructing it faithfully needs
 * omap_l137_usbdc.c's own address range fully mapped, out of this file's
 * scope this pass. */

void master_dispatch_tick(void *handle)	/* FUN_c0008b64 */
{
	uint32_t status;
	uint8_t *h = (uint8_t *)handle;

	wire_status_read(1, wire_status_reg, 1, &status);
	wire_status_ack(1, wire_status_reg);

	if (status & 0x0100) eva_link_status_change(handle);
	if (status & 0x0001) eva_tick_unk_1(handle);
	if (status & 0x0002) eva_tick_unk_2(handle);
	if (status & 0x0004) cad_calibration_progress_pump();	/* real call site: FUN_c0005a1c(handle) */

	if (status & 0x0008) {
		uint8_t sample[8];
		if (ctouchpanel_sample_raw(0 /* DAT_c0009040 */, sample)) {
			ctouchpanel_update(0 /* DAT_c0009044 */, sample);
			eva_tick_unk_3(handle);
		}
	}

	if (status & 0x0010) {
		/* *DAT_c0009048 = 0; */
		eva_tick_unk_4(handle);
	}

	/* unconditional diagnostic-overlay draw, gated on a ready flag + a
	 * free-running counter threshold rather than any status bit */
	{
		extern uint8_t *eva_tick_ready_flag;		/* DAT_c0009048 */
		extern uint32_t eva_tick_counter_threshold;	/* DAT_c000904c */
		extern uint32_t eva_tick_counter_read(void);	/* FUN_c0000140 */
		if (*eva_tick_ready_flag != 0 && eva_tick_counter_read() > eva_tick_counter_threshold) {
			clcdc_draw_text(0 /* DAT_c0009050 */, 0 /* DAT_c0009054 */, 0 /* DAT_c0009058 */, 0);
			clcdc_draw_text(0x110, 0x1e0, 0 /* DAT_c000905c */, 0);
			clcdc_draw_text(0x110, 0 /* DAT_c0009060 */, 0 /* DAT_c0009064 */, 0);
		}
	}

	if (status & 0x0020) {
		eva_tick_unk_5(handle);
		*(uint32_t *)(h + 0x38) = *(uint32_t *)(h + 0x34);
		*(uint32_t *)(h + 0x30) = 0;
		h[0x2d] = 1;
	}

	if (status & 0x0040) eva_tick_unk_6(handle);

	if (status & 0x1000) {
		eva_tick_unk_7(0 /* DAT_c0009068 */);
		ctouchpanel_check_timeout(0 /* DAT_c0009040 */);
	}

	if (status & 0x2000)
		crypto_at88_process_queue(handle);

	if (status & 0x8000) {
		*(uint32_t *)(h + 0x24) += 1;
		eva_tick_unk_8(0 /* DAT_c000906c */);
	}

	/* ---------------------------------------------------------------
	 * USB-adjacent state-machine cluster, ~80 lines in the raw
	 * decompile (DAT_c0009070..DAT_c00090ac, calling FUN_c000f0c8,
	 * FUN_c0005c50, FUN_c0002588, FUN_c000f0b8, FUN_c0009548,
	 * FUN_c0009a98/_afc/_930/_98e0, FUN_c000cc14, FUN_c000c038/_260,
	 * FUN_c000b850/_860, FUN_c000da0c, FUN_c000b6c4) - NOT transcribed,
	 * see structural note above. Genuinely open, not fabricated.
	 * --------------------------------------------------------------- */

	omap_usbdc_poll_transfer_submit(wire_usb_ep_handle,
		*(int32_t *)(h + 0x28) + *(int32_t *)(h + 0x24) * 2000);
}

/* -------------------------------------------------------------------------
 * Still genuinely open in this file:
 *  - wire_dispatch_command's 0xe0 (AT88 relay write) payload-reassembly
 *    loop - the dense byte-shuffle is cited, not transcribed past its
 *    fixed-width header fields, specifically to avoid repeating the kind of
 *    byte-order bug crypto_at88.c's own re-verification pass already found
 *    in adjacent AT88 relay code.
 *  - Most of the eva_wire_ and eva_tick_ generic externs above (bare
 *    FUN_/DAT_ addresses, no cross-file attribution found this pass).
 *  - master_dispatch_tick's own ~80-line USB-adjacent state-machine
 *    cluster (DAT_c0009070-DAT_c00090ac) - plausibly omap_l137_usbdc.c's
 *    own endpoint configure/halt bookkeeping, not attributed or
 *    transcribed - that file's own address range and scope, out of this
 *    file's own scope this pass.
 *  - RESOLVED 2026-07-19: FUN_c0003e24 and FUN_c000a918/FUN_c000aae0
 *    (wire_dispatch_command's two real USB-receive-path callers) are now
 *    fully reconstructed in omap_l137_usbdc_ext.c (they still belong to
 *    omap_l137_usbdc.c's own address neighborhood/scope, not this file's -
 *    that's why the reconstruction lives there, not here). See that file's
 *    own header for the live-Ghidra verification pass and two corrected
 *    bugs.
 *  - The exact meaning of DAT_c00084b4 vs DAT_c00084b8 (op-byte-9's two
 *    text buffers) - which is source and which is destination for
 *    eva_wire_diag_fill is inferred from argument position, not confirmed.
 * ------------------------------------------------------------------------- */
