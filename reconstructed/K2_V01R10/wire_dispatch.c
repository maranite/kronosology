/* SPDX-License-Identifier: GPL-2.0 */
/*
 * wire_dispatch.c - K2 (KRONOS2S_V01R10.VSB) counterpart of K1_V06R06's file
 * of the same name: the panel firmware's central dispatch functions.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS2S_V01R10.VSB, migration
 * pass from K1_V06R06, 2026-07-18 (static dumps only, query_dump_k2.py - no
 * live Ghidra bridge, per project policy on concurrent access to the two
 * open Ghidra projects).
 *
 * HEADLINE FINDING: K2 restructured this file's two functions far more than
 * a simple address/constant port could capture, so BOTH are reconstructed
 * fresh from K2's own real decompile below rather than ported from K1's
 * text. Most importantly, master_dispatch_tick's own ROLE changed: K1's
 * version was itself the hardware-status-bit fan-out dispatcher, called
 * every main-loop tick. K2 moved that status-bit fan-out logic to a
 * ONE-TIME call from eva_board_main (FUN_c000a58c, described fully in this
 * file's own section below, called once per eva_board_main.c's own header
 * comment - NOT looped), and repurposed the actual every-tick call slot
 * (CONFIRMED via eva_board_main.c's own xref trace: `do { FUN_c00092b4
 * (handle); } while (true);`) for a much smaller function whose only job is
 * resolving a previously-deferred wire_dispatch_command continuation. This
 * is asserted as fact, not inferred by name similarity - see each function's
 * own section for the confirming evidence (caller shape, xref counts,
 * matching field offsets against wire_dispatch_command's own continuation-
 * save code).
 *
 * wire_dispatch_command itself (FUN_c0009b54) is CONFIRMED - callers,
 * overall control-flow shape (reentrancy guard, byte-3 opcode switch, same
 * opcode-family split by sign bit) all match K1's version closely enough to
 * be unambiguous - but a large fraction of individual opcodes have real,
 * confirmed behavioral differences (several now unconditionally fault where
 * K1 had working handlers; the calibration opcodes were simplified; a new
 * opcode was added; several context globals moved to handle-local fields).
 * Each difference is called out at its own opcode below, not summarized
 * away - this is exactly the kind of easy-to-miss per-opcode divergence
 * K1's own crypto_at88.c re-verification pass warned this project to watch
 * for.
 */

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 *  wire_dispatch_command - FUN_c0009b54, @0xC0009B54 (1192 bytes) - CONFIRMED
 * ============================================================================
 *
 *  CONFIRMED via callers (xrefs_to): "c0003d80 in None (UNCONDITIONAL_CALL)"
 *  and "c000bc70 in FUN_c000bc1c (UNCONDITIONAL_CALL)" - the same two-caller
 *  USB-receive-path shape K1's own version has (an un-bounded ISR/poll
 *  region plus a small named shim). Neither caller is reconstructed here,
 *  same out-of-scope treatment K1 gave FUN_c0003e24/FUN_c000a918/
 *  FUN_c000aae0 (that address neighborhood belongs to omap_l137_usbdc.c's
 *  own scope, owned by other work this pass).
 *
 *  Signature: (int handle, byte *cmd, uint len) -> byte, matching K1's own
 *  (void *handle, uint8_t *cmd, unsigned len) -> void* exactly in shape.
 *
 *  CONFIRMED STRUCTURAL DIFFERENCES from K1 (all read directly off K2's own
 *  full decompile, not guessed):
 *
 *   - The reentrancy guard moved from a bare global (K1's DAT_c000848c) to
 *     a HANDLE-LOCAL field: `*(char *)(handle + 0x3c)`. The +0x34 reentry
 *     counter increment is UNCHANGED (same offset as K1).
 *   - The deferred-continuation save (K1's opcodes 0xc6/0xc4 -> handle+0x10/
 *     +8) moved to different offsets: handle+0x18 (cmd pointer) and
 *     handle+0x10 (remaining len) - confirmed consistent with what
 *     master_dispatch_tick's own K2 counterpart (below) reads back out.
 *   - Opcode 0xC0 (K1: clcdc register write/set-bits/clear-bits dispatch)
 *     is GONE - falls into the shared fault path (crypto_at88_fault-
 *     equivalent) instead of doing any clcdc register write. Direct LCD
 *     register writes via this opcode are no longer supported in K2's wire
 *     protocol, or moved to a different opcode not identified this pass.
 *   - Opcode 0 (op-byte 0), sub-register 0x50/0x51/0x52 (K1: cpsoc_read_
 *     switch_row/_led_row/_switch_row_clear) now FAULT instead - confirmed
 *     by the `if (0x4f < reg) { if (reg < 0x53) goto fault; ... }` guard.
 *     Sub-register 0xd0/0xd1 (K1: calibration slot init dispatch) ALSO now
 *     falls straight into the fault path - the entire messy shared-tail/
 *     carried-variable 0xd0 handler K1's own file flagged as "not faithfully
 *     transcribed, easy to mistranscribe" is simply GONE in K2, replaced by
 *     an unconditional fault.
 *   - Opcode 0xF0 (GetVersion) no longer checks the handle+0x40 == -1
 *     sentinel before reporting - that pre-fault gate is absent in K2's
 *     version; it unconditionally reports.
 *   - Opcode 1 (op-byte 1), sub-register 0x51 (K1: cpsoc_read_led_row) now
 *     FAULTs (with report code 0x4e1) instead of reading. Sub-registers
 *     0x50/0x52 still work as before, transcribed 1:1.
 *   - Opcode 1, bank 0xa0/0xc0 (K1: cad_calibration_lookup + mark_changed/
 *     init_slot, via a shared lookup table) are SIMPLIFIED: the lookup
 *     table indirection is GONE, replaced by a direct bounds check
 *     (`if (cmd[1] > 0x1e) fault;`) followed by one direct call using cmd[1]
 *     (bank 0xa0) or cmd[1] plus cmd[0]'s two nibbles (bank 0xc0) - no
 *     lookup, no carried-variable shared tail. A real simplification, not a
 *     transcription gap.
 *   - Opcode 1, bank 0xb0 (K1: a 0x26-iteration loop resetting every
 *     calibration slot that isn't already raw) is COMPLETELY DIFFERENT in
 *     K2: a single call using cmd[0]'s two nibbles as arguments, no loop at
 *     all. The "reset all slots" behavior K1 had is gone.
 *   - Opcode 1, bank 0xf0 (K1: cpsoc_i2c_dispatch(handle, 0x78, 0x70, 0))
 *     is now a report-code call (eva_wire_report_code(1, 0x40)) instead -
 *     no cpsoc I2C dispatch happens here anymore.
 *   - Opcode 4 (K1: a fixed-delay call, eva_wire_delay(ctx, 0x1e)) is now
 *     just a HANDLE FLAG SET: `*(uint8_t *)(handle_ctx + 0x118) = 1;` - no
 *     delay call at all.
 *   - Opcode 6 (K1: eva_wire_flag_a_set(ctx) + a separate global flag byte
 *     set) now writes a HANDLE-LOCAL byte (handle+0x3d = 1) instead of the
 *     separate global K1 used.
 *   - Opcode 7 (K1: eva_wire_flag_b_set(ctx, (cmd[2]>>1)&1) - bit-extracted)
 *     now passes `handle` directly (not a DAT_ context) and `cmd[2]` RAW,
 *     with no visible bit-mask at this call site - if any bit extraction
 *     still happens, it now happens inside the callee.
 *   - Opcode 9 matches K1 closely: diag-fill + draw_text(100, 0x230, ...) +
 *     halt-step, same three-call shape, same coordinates.
 *   - Opcode 10 is ENTIRELY NEW, no K1 counterpart: sets a handle-local
 *     boolean field (`handle[0x50] = (cmd[2] != 0) ? 1 : 0;`).
 *   - The AT88 relay handlers (opcodes 0xE0/0xE1) are now factored into
 *     their OWN subroutines (FUN_c0007a9c and FUN_c0007c34 respectively,
 *     taking (handle, &cmd, &len) so they can advance the caller's own
 *     cursor/length) rather than being inlined byte-shuffle code as in K1.
 *     Their own internals are NOT transcribed here, same caution K1 applied
 *     to its own inline version (real byte-order bugs were previously found
 *     in adjacent AT88 relay code by this project's re-verification pass).
 *   - Opcode 0xC5 (clcdc_dispatch_set_palette_hook) DOES independently
 *     confirm K1's own previously-uncertain argument order - K2's decompile
 *     spells out `FUN_c0011f3c(ctx, cmd[2], cmd[1], cmd[0], cmd[7])`
 *     explicitly, i.e. (ctx, reg=cmd[2], hi=cmd[1], lo=cmd[0], extra=cmd[7])
 *     - exactly the order K1's own file cited as "not fully re-derived".
 *     This is real corroborating evidence for K1's own citation, not proof
 *     K1's code is byte-identical (K2 could have reordered its own args
 *     independently), but is offered as a useful cross-check for whoever
 *     revisits K1's own opcode 0xC5 handler.
 * ------------------------------------------------------------------------- */

/* --- confirmed/likely cross-file targets (address-cited, not independently
 * traced this pass - each belongs to another file's own scope) --- */
extern void clcdc_cursor_set_stride(void *cursor, uint32_t stride);			/* FUN_c0011f34, opcode 0x81 */
extern void clcdc_dispatch_set_palette_hook(void *ctx, uint8_t reg, uint8_t hi, uint8_t lo, uint8_t extra);	/* FUN_c0011f3c, opcode 0xc5 - full arg order CONFIRMED, see note above */
extern void clcdc_draw_text(uint16_t x, uint16_t y, const char *str, uint32_t font_or_mode);	/* FUN_c0012578, opcode 9 and elsewhere */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 - CONFIRMED K2 fault/halt handler, 63 project-wide callers */
extern void eva_lcd_set_brightness(void *handle, uint8_t value);		/* FUN_c0009354, opcode 199 */
extern void eva_wire_report_code(int subsystem_id, uint32_t code);		/* FUN_c0019f30 - CONFIRMED (5 callers, all pass literal 1 as subsystem_id, matching K1's own eva_wire_report_code note exactly) */
extern void eva_wire_at88_relay_write(void *handle, uint8_t **cmd, int *len);	/* FUN_c0007a9c, opcode 0xe0 - factored subroutine, not transcribed (see note above) */
extern void eva_wire_at88_relay_read(void *handle, uint8_t **cmd, int *len);	/* FUN_c0007c34, opcode 0xe1 - factored subroutine, not transcribed */
extern void eva_wire_pedal_mode_set(void *ctx, uint8_t value);		/* FUN_c0006458, opcode 0 reg 0x80 */
extern void eva_wire_pedal_send(void *ctx, uint8_t index, int set);		/* FUN_c00058b8, opcode-0/1 reg 0x50/0x52 shared tail - LIVE-QUERY RESOLVED 2026-07-19 (decompile_function on both this function and wire_dispatch_command itself): real 3-parameter signature, not 2. `index` (0-0x48) looks up a (word_slot, bit_pos) pair from a packed table at DAT_c0005910 (word_slot==0xff is an invalid-index sentinel, silently ignored, no fault); word_slot>=8 clamps to a fixed overflow slot (ctx+100); `set` (CONFIRMED real 3rd argument at both call sites below, not a guess) selects OR (1, "set") vs AND-NOT (0, "clear") of the looked-up bit into ctx+0x54+word_slot*2 - the exact same `(word_slot, bit_pos)` table-lookup idiom already established elsewhere in this project for panel_manager_set_led_bit */
extern void eva_wire_pedal_release(void *ctx);					/* FUN_c0006468, opcode 1 reg 0x83 - K1's cad_pedal_send_release lost its 2nd (value) arg here, or it moved inside the callee, not independently confirmed */
extern void eva_wire_calib_mark_changed(void *ctx, uint8_t slot);		/* FUN_c0006280, opcode 1 bank 0xa0 - simplified, no lookup table (see note above) */
extern void eva_wire_calib_init_slot(void *ctx, uint8_t nib_hi, uint8_t nib_lo);	/* FUN_c0005ed4, opcode 1 bank 0xb0 - COMPLETELY restructured vs K1 (see note above) */
extern void eva_wire_calib_dispatch(void *ctx, uint8_t slot, uint8_t nib_hi, uint8_t nib_lo);	/* FUN_c0005e9c, opcode 1 bank 0xc0 - simplified, no lookup table */
extern void eva_wire_reg_low_write(void *ctx, uint8_t value);			/* inlined at opcode 0's reg<0x10 case - no longer a real function call, see body below */
extern void eva_wire_reg_write(void *ctx, uint8_t reg, uint16_t value);	/* FUN_c0005914, opcode 5 */
extern void eva_wire_flag_a_notify(void *ctx);					/* FUN_c0010298, opcode 6 - the global-flag part of K1's eva_wire_flag_a_set is now this call plus a handle-local byte set, see body below */
extern void eva_wire_flag_b_set(void *handle, uint8_t raw_value);		/* FUN_c0009b1c, opcode 7 - takes handle + raw cmd[2], no visible bit-mask at the call site (see note above) */
extern void eva_wire_diag_fill(void *dst, const void *src, int len);		/* FUN_c001372c, opcode 9 */
extern void eva_wire_diag_halt_step(void);					/* FUN_c0000904, opcode 9 */

uint8_t wire_dispatch_command(void *handle, uint8_t *cmd, unsigned len)	/* FUN_c0009b54, @0xc0009b54 - NOTE: K2's own decompile return type is `undefined1` (a byte), NOT `void *` as in K1 - a real, confirmed signature difference, not a transcription choice */
{
	extern void *wire_pedal_ctx;			/* DAT_c0009ffc, shared across opcode 0/1 pedal + calibration handlers */
	extern void *wire_lcd_cursor_ctx;		/* DAT_c000a020, opcodes 0x81/0xc5 */
	extern uint32_t *wire_pixel_table_base;	/* DAT_c000a024, opcode 0xc2 */
	extern const char *wire_fault_file;		/* DAT_c000a000, shared fault-report file constant across several opcodes */
	extern uint32_t wire_fault_line_c0;		/* DAT_c000a01c, opcode 0xc0's fault line */
	extern uint32_t wire_fault_line_reg;		/* DAT_c000a004, opcode-0 reg-range fault line */
	extern uint32_t wire_fault_line_bank_a0;	/* DAT_c000a008, opcode-1 bank 0xa0 bounds-fault line */
	extern uint32_t wire_fault_line_bank_c0;	/* DAT_c000a00c, opcode-1 bank 0xc0 bounds-fault line */
	extern void *wire_flag_a_ctx;			/* DAT_c000a010, opcode 6 */
	extern char *wire_diag_text_dst;		/* DAT_c000a018, opcode 9 fill destination */
	extern const char *wire_diag_text_src;		/* DAT_c000a014, opcode 9 fill source */

	*(int *)((uint8_t *)handle + 0x34) += 1;

	if (*(char *)((uint8_t *)handle + 0x3c) != 0)
		return 1;	/* real return-on-guard path, confirmed distinct from K1's own separate early-return - see note above on the guard moving to handle+0x3c */

	while (len > 2 || len == 3) {
		uint8_t opcode = cmd[3];

		if ((int8_t)opcode < 0) {
			/* extended-opcode family (0x80-0xff) */
			if (opcode == 0xc6 || opcode == 0xc4) {
				*(uint8_t **)((uint8_t *)handle + 0x18) = cmd;
				*(uint32_t *)((uint8_t *)handle + 0x10) = len;
				return 0;
			} else if (opcode == 0xc2) {
				uint32_t region_len = (cmd[9] << 8) | (cmd[8] << 0x10) | cmd[10];
				uint32_t base = (uint32_t)wire_pixel_table_base +
						 (((cmd[5] << 8) | (cmd[4] << 0x10) | cmd[6]) * 2);
				*(uint32_t *)((uint8_t *)handle + 0x14) = (cmd[1] << 8) | (cmd[0] << 0x10) | cmd[2];
				len -= 8;
				*(uint32_t *)((uint8_t *)handle + 0x28) = region_len;
				*(uint32_t *)((uint8_t *)handle + 0x20) = base + region_len * 2 - 2;
				*(uint32_t *)((uint8_t *)handle + 0x1c) = base;
				cmd += 8;
			} else if (opcode == 0x83) {
				*(uint32_t *)((uint8_t *)handle + 0x14) = 0;
			} else if (opcode == 0x81) {
				clcdc_cursor_set_stride(wire_lcd_cursor_ctx, (cmd[1] << 8) | (cmd[0] << 0x10) | cmd[2]);
			} else if (opcode == 199) {
				eva_lcd_set_brightness(handle, cmd[2]);
			} else if (opcode == 0xc5) {
				clcdc_dispatch_set_palette_hook(wire_lcd_cursor_ctx, cmd[2], cmd[1], cmd[0], cmd[7]);
				cmd += 4;
				len -= 4;
			} else if (opcode == 0xe0) {
				eva_wire_at88_relay_write(handle, &cmd, (int *)&len);
			} else if (opcode == 0xe1) {
				eva_wire_at88_relay_read(handle, &cmd, (int *)&len);
			} else if (opcode == 0xc0) {
				/* GONE in K2: unconditionally faults - see note above */
				crypto_at88_fault(0, wire_fault_file, wire_fault_line_c0);
			}
			/* opcode values other than the above (0x84-0xbf minus 0xc5,
			 * 0xc7=199 handled above by value; any not matched here):
			 * no call at all, falls through to the loop tail - matches
			 * K1's own "no invented fault" correction for its equivalent
			 * fallthrough case */
		} else if (opcode == 0) {
			uint8_t reg = cmd[2];
			if ((reg & 0xf0) == 0) {
				*(uint8_t *)((uint8_t *)handle + 0xc) = cmd[1];	/* inlined in K2 - was a function call (eva_wire_reg_low_dispatch) in K1 */
			} else if (reg < 0xd2) {
				if (reg < 0xd0) {
					if (reg > 0x4f) {
						if (reg < 0x53)
							crypto_at88_fault(0, wire_fault_file, wire_fault_line_reg);	/* reg 0x50/0x51/0x52: GONE in K2, was cpsoc row reads in K1 */
						else if (reg == 0x80)
							eva_wire_pedal_mode_set(wire_pedal_ctx, cmd[1]);
					}
				} else {
					crypto_at88_fault(0, wire_fault_file, wire_fault_line_reg);	/* reg 0xd0/0xd1: GONE in K2, was calibration dispatch in K1 */
				}
			} else if (reg == 0xee) {
				eva_wire_report_code(1, 0x10);	/* CommunicationCheck */
			} else if (reg == 0xf0) {
				eva_wire_report_code(1, 0x20);	/* GetVersion - no -1 sentinel pre-check in K2, see note above */
			}
		} else if (opcode == 1) {
			uint8_t reg = cmd[2];
			uint8_t bank = reg & 0xf0;
			if (reg == 0x50) {
				eva_wire_pedal_send(wire_pedal_ctx, cmd[1], 1);	/* LIVE-QUERY RESOLVED 2026-07-19: real 3rd arg is 1 ("set"), see extern decl note above */
			} else if (reg == 0x51) {
				crypto_at88_fault(0, wire_fault_file, 0x4e1);	/* GONE in K2, was cpsoc_read_led_row in K1 */
			} else if (reg == 0x52) {
				eva_wire_pedal_send(wire_pedal_ctx, cmd[1], 0);	/* LIVE-QUERY RESOLVED 2026-07-19: real 3rd arg is 0 ("clear") - CONFIRMED via decompile_function on wire_dispatch_command itself, resolving this file's own prior "not independently attributed" note */
			} else if (reg == 0x83) {
				eva_wire_pedal_release(wire_pedal_ctx);
			} else if (bank == 0xa0) {
				if (cmd[1] > 0x1e)
					crypto_at88_fault(0, wire_fault_file, wire_fault_line_bank_a0);
				eva_wire_calib_mark_changed(wire_pedal_ctx, cmd[1]);
			} else if (bank == 0xb0) {
				eva_wire_calib_init_slot(wire_pedal_ctx, (uint8_t)(cmd[0] >> 4), (uint8_t)(cmd[0] & 0xf));
			} else if (bank == 0xc0) {
				if (cmd[1] > 0x1e)
					crypto_at88_fault(0, wire_fault_file, wire_fault_line_bank_c0);
				eva_wire_calib_dispatch(wire_pedal_ctx, cmd[1], (uint8_t)(cmd[0] >> 4), (uint8_t)(cmd[0] & 0xf));
			} else if (bank == 0xf0) {
				eva_wire_report_code(1, 0x40);	/* changed in K2: was cpsoc_i2c_dispatch(handle,0x78,0x70,0) in K1 */
			}
		} else if (opcode == 4) {
			*(uint8_t *)((uint8_t *)wire_pedal_ctx + 0x118) = 1;	/* changed in K2: was a fixed-delay call in K1 */
		} else if (opcode == 5) {
			eva_wire_reg_write(wire_pedal_ctx, cmd[2], (uint16_t)cmd[1] + (uint16_t)cmd[0] * 0x100);
		} else if (opcode == 6) {
			eva_wire_flag_a_notify(wire_flag_a_ctx);
			*(uint8_t *)((uint8_t *)handle + 0x3d) = 1;	/* changed in K2: was a separate global flag in K1, now handle-local */
		} else if (opcode == 7) {
			eva_wire_flag_b_set(handle, cmd[2]);	/* changed in K2: takes handle (not a DAT_ ctx) and raw cmd[2] (no visible bit-mask), see note above */
		} else if (opcode == 9) {
			eva_wire_diag_fill(wire_diag_text_dst, wire_diag_text_src, 0x20);
			clcdc_draw_text(100, 0x230, wire_diag_text_dst, 0);
			eva_wire_diag_halt_step();
		} else if (opcode == 10) {
			/* NEW in K2, no K1 counterpart */
			*(uint8_t *)((uint8_t *)handle + 0x50) = (cmd[2] != 0) ? 1 : 0;
		}

		len -= 4;
		cmd += 4;
	}

	return 1;
}

/* ============================================================================
 *  eva_board_boot_status_dispatch - FUN_c000a58c, @0xC000A58C (316 bytes)
 * ============================================================================
 *
 *  CONFIRMED called exactly ONCE, from eva_board_main.c's own confirmed call
 *  site @0xC0007300, immediately after eva_board_final_setup and BEFORE the
 *  real forever-loop begins (see eva_board_main.c's own header). NOT looped.
 *
 *  Included here, not in eva_board_main.c, because its own internal shape -
 *  read a hardware status register via a paired read/ack call, then fan out
 *  by bit to a cluster of subsystem handlers - is STRUCTURALLY the closest
 *  thing in the whole K2 image to K1's own master_dispatch_tick. It is
 *  deliberately NOT named or declared as master_dispatch_tick here, because
 *  that name is defined by ROLE (the function actually called every main-
 *  loop iteration, confirmed to be FUN_c00092b4 below) - conflating the two
 *  would misrepresent a real, confirmed K2 architecture change: whatever
 *  logic K1 ran on every tick, K2 appears to run (at least the portion
 *  captured here) exactly once at boot instead.
 *
 *  status-bit fan-out (CONFIRMED from K2's own decompile):
 *   bit 0x0001  a cluster of subsystem pumps (FUN_c0009954, FUN_c00099ac,
 *               FUN_c000a370, FUN_c000a4bc, FUN_c000a0e8, FUN_c000a1d0,
 *               FUN_c000a270, FUN_c000bfcc, FUN_c000a308), gated additionally
 *               on a handle+8/handle+4 mismatch for the first sub-call
 *               (FUN_c00067f4) - none individually attributed this pass.
 *               No K1 bit lines up 1:1 with this - K1 spread equivalent-
 *               looking work across bits 0x1/0x2/0x4/0x8.
 *   bit 0x0010  FUN_c000735c(handle) - plausibly K1's bit-0x10 eva_tick_unk_4
 *               counterpart by bit value alone, not independently confirmed.
 *   bit 0x0020  FUN_c0007840(handle), then handle+0x38 <- handle+0x34,
 *               handle+0x30 <- 0, handle+0x2c <- 1 - CONFIRMED close match
 *               to K1's own bit-0x20 handler (K1: h+0x38<-h+0x34, h+0x30<-0,
 *               h+0x2d<-1) - same accounting-reset shape, one field offset
 *               different (0x2c here vs K1's 0x2d).
 *   bit 0x0040  FUN_c0007890(handle) - plausibly K1's bit-0x40 counterpart
 *               by bit value, not independently confirmed.
 *   bit 0x2000  FUN_c00079dc(handle) - CONFIRMED position match to K1's own
 *               bit-0x2000 crypto_at88_process_queue call (same bit value,
 *               same "drain a queue" role by position in this project's own
 *               prior crypto_at88.c findings), though FUN_c00079dc itself is
 *               not independently re-derived this pass.
 *   bit 0x0080  FUN_c000a0a0(handle), then returns early - NO K1 bit-0x80
 *               handler exists in eva_board_main.c's own K1 version at all;
 *               this is either new or a relocated K1 bit this pass didn't
 *               match.
 *
 *  NOT present here, confirmed absent by full-text inspection of this
 *  function's own decompile: K1's bits 0x100 (link status), 0x4 (cad
 *  calibration progress pump), 0x8 (ctouchpanel sample/update), 0x1000
 *  (ctouchpanel timeout), 0x8000 (chunk counter), and the unconditional
 *  diagnostic-overlay draw block. Also absent: the trailing
 *  omap_usbdc_poll_transfer_submit call K1's own master_dispatch_tick ends
 *  with - this function has no such tail at all.
 * ------------------------------------------------------------------------- */
extern void wire_status_read(int bank, void *reg, int mode, uint32_t *out);	/* FUN_c001a0ac */
extern void wire_status_ack(int bank, void *reg);				/* FUN_c001a01c - CONFIRMED (also independently ruled out as eva_board_start_task, see eva_board_main.c's "still open" section) */
extern void *wire_boot_status_reg;						/* DAT_c000a6c8 */
extern void eva_boot_status_unk_pump_a(void *handle);		/* FUN_c00067f4, gated sub-call inside bit 1 */
extern void eva_boot_status_unk_1(void *handle);		/* FUN_c0009954 */
extern void eva_boot_status_unk_2(void *handle);		/* FUN_c00099ac */
extern void eva_boot_status_unk_3(void *handle);		/* FUN_c000a370 */
extern void eva_boot_status_unk_4(void *handle);		/* FUN_c000a4bc */
extern void eva_boot_status_unk_5(void *handle);		/* FUN_c000a0e8 */
extern void eva_boot_status_unk_6(void *handle);		/* FUN_c000a1d0 */
extern void eva_boot_status_unk_7(void *handle);		/* FUN_c000a270 */
extern void eva_boot_status_unk_8(void *ctx, uint32_t val);	/* FUN_c000bfcc */
extern void eva_boot_status_unk_9(void *handle);		/* FUN_c000a308 */
extern int eva_boot_status_unk_10(void *ctx);			/* FUN_c000edec */
extern void eva_boot_status_unk_11(void *ctx);			/* FUN_c000c9c8 */
extern void eva_boot_status_unk_12(void *handle);		/* FUN_c000735c, bit 0x10 */
extern void eva_boot_status_unk_13(void *handle);		/* FUN_c0007840, bit 0x20 */
extern void eva_boot_status_unk_14(void *handle);		/* FUN_c0007890, bit 0x40 */
extern void crypto_at88_process_queue(void *handle);		/* FUN_c00079dc, bit 0x2000 - position-matched to K1's own crypto_at88_process_queue, not independently re-derived */
extern void eva_boot_status_unk_15(void *handle);		/* FUN_c000a0a0, bit 0x80 */

void eva_board_boot_status_dispatch(void *handle)	/* FUN_c000a58c, @0xc000a58c - called ONCE, see note above */
{
	uint32_t status;
	uint8_t *h = (uint8_t *)handle;

	wire_status_read(1, wire_boot_status_reg, 1, &status);
	wire_status_ack(1, wire_boot_status_reg);

	if (status & 1) {
		if (*(int *)(h + 8) != *(int *)(h + 4)) {
			eva_boot_status_unk_pump_a(0 /* DAT_c000a6cc */);
			*(uint32_t *)(h + 8) = *(uint32_t *)(h + 4);
		}
		eva_boot_status_unk_1(handle);
		eva_boot_status_unk_2(handle);
		eva_boot_status_unk_3(handle);
		eva_boot_status_unk_4(handle);
		eva_boot_status_unk_5(handle);
		eva_boot_status_unk_6(handle);
		eva_boot_status_unk_7(handle);
		eva_boot_status_unk_8(0 /* DAT_c000a6d0 */, *(uint32_t *)(h + 4));
		eva_boot_status_unk_9(handle);
		h[0x2d] = (eva_boot_status_unk_10(0 /* DAT_c000a6d4 */) != 0) ? 1 : 0;
		eva_boot_status_unk_11(0 /* DAT_c000a6d8 */);
	}

	if (status & 0x10)
		eva_boot_status_unk_12(handle);

	if (status & 0x20) {
		eva_boot_status_unk_13(handle);
		*(uint32_t *)(h + 0x38) = *(uint32_t *)(h + 0x34);
		*(uint32_t *)(h + 0x30) = 0;
		h[0x2c] = 1;
	}

	if (status & 0x40)
		eva_boot_status_unk_14(handle);

	if (status & 0x2000)
		crypto_at88_process_queue(handle);

	if (status & 0x80)
		eva_boot_status_unk_15(handle);
}

/* ============================================================================
 *  master_dispatch_tick - FUN_c00092b4, @0xC00092B4 (144 bytes) - CONFIRMED
 * ============================================================================
 *
 *  CONFIRMED to be the function actually called every main-loop iteration:
 *  its sole caller (FUN_c000a6dc, @0xC000A6DC, itself CONFIRMED called once
 *  from eva_board_main @0xC0007324) has the body
 *  `do { FUN_c00092b4(param_1); } while (true);` - an explicit, real,
 *  un-exited loop, the K2 counterpart of K1's own
 *  `for (;;) master_dispatch_tick(eva_board_handle);` tail.
 *
 *  RADICALLY SMALLER and DIFFERENT ROLE than K1's own 1240-byte version -
 *  this is the single most important structural finding in this file for
 *  this migration pass. K2's every-tick function does NOT read any hardware
 *  status register and does NOT fan out by status bit at all. Instead:
 *
 *   1. Checks whether wire_dispatch_command left a deferred continuation
 *      (handle+0x18 != 0 - the SAME field wire_dispatch_command's own
 *      opcode 0xc4/0xc6 handler above saves into). If one is pending:
 *      - reads the saved command's own opcode byte (offset +3 off the saved
 *        pointer) and dispatches by value: -0x3c (i.e. 0xc4 as a signed
 *        byte) resolves to FUN_c0009158 below; -0x3a (0xc6) resolves to
 *        FUN_c0008d24; anything else is a genuine protocol-consistency
 *        fault (crypto_at88_fault-equivalent) - this IS the confirmed real
 *        implementation of what K1's own file only described as "this
 *        function's own equivalent of the host's ContinueProcessingEvent,
 *        NOT consumed here" for opcodes 0xc6/0xc4.
 *      - clears the saved pointer (handle+0x18 = 0), then calls two small
 *        cleanup functions (FUN_c000183c, FUN_c0003824) - not attributed.
 *   2. Unconditionally (whether or not a continuation was pending): checks
 *      bit 0x4 of a queue-header word (`*(*DAT_c0009350 + 8) & 4`) and, if
 *      set, calls FUN_c0003434() and increments a counter field
 *      (`(*DAT_c0009350)[4] += 1`) - a small queue-depth accounting step,
 *      not attributed further.
 *
 *  Does NOT drive USB transfer submission at all - K1's own trailing
 *  `omap_usbdc_poll_transfer_submit(...)` call, and the entire ~80-line
 *  USB-adjacent state-machine cluster K1's file left as "genuinely open",
 *  have NO counterpart anywhere in this function. Whether that logic moved
 *  to an interrupt handler, to eva_board_boot_status_dispatch above, or was
 *  genuinely removed in K2's redesign is NOT determined this pass - flagged
 *  as open, not guessed at.
 *
 *  The two continuation resolvers are cited, not transcribed in full:
 *   - FUN_c0009158 (opcode 0xc4 continuation, 324 bytes) - a real, non-
 *     trivial pixel/glyph write loop (reads 11 bytes of saved command
 *     state, then a bounded loop writing 16-bit pixel values into a
 *     framebuffer-shaped table indexed by two 16-bit cursor fields with
 *     wraparound). Structurally this is the K2 continuation of K1's own
 *     opcode-0xc2/pixel-region-transfer machinery, not independently
 *     re-derived past this structural description - same caution K1 applied
 *     to clcdc_blit_glyph's own dense bit-shift math.
 *   - FUN_c0008d24 (opcode 0xc6 continuation, 1064 bytes) - NOT inspected in
 *     detail this pass beyond confirming its role as the sibling
 *     continuation resolver; far larger than this file's own scope budget
 *     allows for a full re-derivation. NEEDS LIVE QUERY if a future pass
 *     needs its full behavior.
 * ------------------------------------------------------------------------- */
extern void wire_continuation_pixel_resume(void *handle);	/* FUN_c0009158, opcode 0xc4 continuation - cited structurally, see note above. LIVE-QUERY RE-CONFIRMED 2026-07-19 (decompile_function): consumes 11 saved-command bytes into a clcdc cursor/region-setup call (FUN_c0011f80, opcode 0x81/0xc5's own clcdc_cursor_set_stride/_dispatch_set_palette_hook neighborhood), THEN loops writing 16-bit values (a palette lookup keyed by the 11th saved byte, into a DAT_c00092b0-based CLUT) to an 800-wide framebuffer-shaped array (x+y*800 addressing, bounds-checked against DAT_c00092a8), advancing two 16-bit x/y-style cursor fields with wraparound at a fixed line-width boundary (fields at the shared ctx+8/+0xa/+0xc/+0xe) - a genuine "palette-indexed rectangle fill" primitive, not just a raw pixel-value writer as previously summarized. */
extern void wire_continuation_other_resume(void *handle);	/* FUN_c0008d24, opcode 0xc6 continuation - LIVE-QUERY CHARACTERIZED 2026-07-19 (get_function_info + decompile_function: leaf, no callees, sole caller master_dispatch_tick, 1064 bytes). Genuinely different mechanism from FUN_c0009158 above (CONFIRMED not the same ring/cursor object - no +8/+0xa/+0xc/+0xe fields here at all): unpacks a raw byte stream (handle+0x18, 4 bytes/word) into a circular ring buffer of 0x321 (801) 16-bit slots bounded by handle+0x1c (write ptr)/handle+0x20 (end ptr)/handle+0x28 (wrap-margin, read from handle+0x28), one output ushort per input BYTE (four per source word, extracted at bit positions 24/[16:9 via a pre-doubled mask 0x1fe]/[8:1 via the same mask]/[7:0]*2 - CONFIRMED via read_memory that the shared mask constant DAT_c0009154 == 0x1fe, i.e. an 8-bit field pre-scaled by 2, so every extraction is really "one input byte, doubled, used as a ushort-table index" despite the differing shift/mask spellings), each byte looked up in a fixed 256-entry ushort table at DAT_c0009150 == 0xC001B814 (CONFIRMED via read_memory) - i.e. this is an 8-bit-indexed-pixel-to-16-bit LUT expansion feeding the ring buffer, structurally similar in ROLE to this project's own previously-confirmed RGB565 palette-loader finding (omap_l137_addr_gap_misc.c's cluster 5) but a distinct table/object. A length-vs-limit comparison against a second sentinel constant (DAT_c000914c == 0x1ff, also read_memory-confirmed) selects between two near-duplicate unpack loops (one for an initial short/boundary chunk, one for the steady-state 4-bytes-at-a-time case) - NOT transcribed statement-for-statement here (dense, ~140 real pointer-arithmetic statements with heavy duplicated wraparound-check inlining across both loop shapes; matches this project's own established practice of describing rather than force-transcribing code this shape, e.g. mcasp_reinit_reduced, sched_tcb_table_init_and_autostart's own phase 2). NEEDS LIVE QUERY only if a future pass specifically needs the byte-exact unpack order. */
extern void wire_continuation_cleanup_a(void *ctx);		/* FUN_c000183c */
extern void wire_continuation_cleanup_b(void);			/* FUN_c0003824 */
extern void *wire_queue_header;				/* DAT_c0009350 */
extern void wire_queue_depth_pump(void);			/* FUN_c0003434 */

void master_dispatch_tick(void *handle)	/* FUN_c00092b4, @0xc00092b4 */
{
	uint8_t *h = (uint8_t *)handle;
	uint32_t *saved_cmd_slot = (uint32_t *)(h + 0x18);
	uint32_t **queue_hdr;

	if (*saved_cmd_slot != 0) {
		int8_t saved_opcode = *(int8_t *)(*saved_cmd_slot + 3);

		if (saved_opcode == -0x3c)		/* 0xc4 */
			wire_continuation_pixel_resume(handle);
		else if (saved_opcode == -0x3a)	/* 0xc6 */
			wire_continuation_other_resume(handle);
		else
			crypto_at88_fault(0, 0 /* DAT_c0009344 */, 0 /* DAT_c0009348 */);

		*saved_cmd_slot = 0;
		wire_continuation_cleanup_a(0 /* DAT_c000934c */);
		wire_continuation_cleanup_b();
	}

	queue_hdr = (uint32_t **)wire_queue_header;
	if ((*(uint32_t *)(*queue_hdr + 2) & 4) == 0)	/* +8 bytes = index 2 of a uint32_t[] */
		return;

	wire_queue_depth_pump();
	(*queue_hdr)[4] += 1;
}

/* -------------------------------------------------------------------------
 * Still genuinely open in this file, K1->K2 migration pass, 2026-07-18,
 * updated after a 2026-07-19 live Ghidra MCP pass (dedicated single-session
 * bridge, get_disassembly/decompile_function/get_function_info/get_xrefs_to,
 * "2-agent cap, no further fan-out" authorization, see CLAUDE.md; zero
 * Agent-tool subagent calls made):
 *
 *  - RESOLVED 2026-07-19: eva_wire_pedal_send's real signature is 3-parameter
 *    (ctx, index, set), not 2 - and its distinguishing 3rd argument between
 *    opcode 0/1 reg 0x50 (1, "set") and reg 0x52 (0, "clear") is now
 *    confirmed and transcribed at both call sites above (decompile_function
 *    on both the callee and wire_dispatch_command itself).
 *  - IMPROVED 2026-07-19: FUN_c0008d24 (opcode 0xc6's continuation resolver)
 *    is no longer just "role confirmed" - it's now structurally
 *    characterized in detail (byte-per-LUT-entry pixel unpacker, 256-entry
 *    table at 0xC001B814, 0x321-halfword ring buffer) via decompile_function
 *    + read_memory, see its own extern-decl comment above. Still not
 *    transcribed statement-for-statement (dense, ~140 real statements with
 *    heavy duplicated wraparound-check inlining) - NEEDS LIVE QUERY only if
 *    a future pass specifically needs the byte-exact unpack order.
 *  - The USB transfer-submission tail K1's own master_dispatch_tick ends
 *    with, and its entire ~80-line USB-adjacent state-machine cluster, still
 *    have no located K2 counterpart in either function reconstructed here -
 *    not re-investigated this pass.
 *  - Most of eva_board_boot_status_dispatch's own subsystem-pump callees
 *    (FUN_c0009954, FUN_c00099ac, FUN_c000a370, FUN_c000a4bc, FUN_c000a0e8,
 *    FUN_c000a1d0, FUN_c000a270, FUN_c000bfcc, FUN_c000a308, FUN_c000edec,
 *    FUN_c000c9c8, FUN_c000735c, FUN_c0007840, FUN_c0007890, FUN_c000a0a0) -
 *    still no cross-file attribution, left as bare externs; one of them
 *    (FUN_c000a1d0) was incidentally decompiled during this pass's omap_gpio.c
 *    DIR/OUT_DATA sweep (it reads gpio_bank_get_base()+0x20 bit 22, the SAME
 *    bit gpio_field_0x20_bit22_status exposes, but calls gpio_bank_get_base
 *    directly rather than through that wrapper) - not otherwise attributed.
 *  - wire_dispatch_command's own two USB-receive-path callers (FUN_c0003d80
 *    and FUN_c000bc1c) were not reconstructed here, matching K1's own
 *    out-of-scope treatment of the equivalent K1 functions.
 *  - FUN_c0007a9c/FUN_c0007c34 (AT88 relay write/read, opcodes 0xe0/0xe1)
 *    internals not transcribed, same caution K1 applied to its own inline
 *    version of this logic. Not re-investigated this pass.
 * ------------------------------------------------------------------------- */
