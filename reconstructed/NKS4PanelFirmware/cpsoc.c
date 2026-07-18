/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cpsoc.c - the PSoC ("Panel Scan System") button/LED scan-chip driver: the
 * three host-facing switch/LED register-bank readers (also the real wire-
 * protocol entry points for opcode-0 reg 0x50/0x51/0x52, per
 * FUN_c0007d1c - see KRONOS_V06R06.VSB.md's new "wire-protocol command
 * dispatcher" section), the hidden factory diagnostic menu built on top of
 * them, and a generic event-queue push primitive found while chasing this
 * file's own assert call sites (candidate for the switch/button event queue,
 * not independently confirmed to be cpsoc-exclusive - see its own comment).
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17, continuing
 * kronosology/docs/modules/KRONOS_V06R06.VSB.md's own earlier partial trace (which had
 * found the 73-entry switch/LED name table and the menu's existence, but not this
 * level of behavioral detail). See this project's README.md for status.
 */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* shared hard-halt handler, see crypto_at88.c */

/* ------------------------------------------------------------------------- *
 *  Raw register read - a cached shadow-state byte array (base DAT_c001274c),
 *  NOT a live hardware read on every call. @0xc0012740
 * ------------------------------------------------------------------------- */
extern uint8_t *cpsoc_shadow_state;	/* DAT_c001274c, real fixed address */

static inline uint8_t cpsoc_read_shadow(int index)	/* FUN_c0012740 */
{
	return cpsoc_shadow_state[index];
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_read_switch_or_led - dispatches to one of two register banks
 *  depending on index, confirming and completing the .md doc's earlier
 *  "< 0x21 vs >= 0x21" observation: index < 0x21 (33) -> register 0x7a,
 *  index >= 0x21 -> register 0x79. @0xc0012750
 * ------------------------------------------------------------------------- */
extern void cpsoc_i2c_dispatch(void *handle, uint8_t reg, uint32_t out_value,
			       uint8_t raw_bit);			/* FUN_c0007120 */
extern void *cpsoc_i2c_handle;					/* DAT_c0012790 */

void cpsoc_read_switch_or_led(void *param1, uint32_t out_value, int index)	/* FUN_c0012750 */
{
	uint8_t raw = cpsoc_read_shadow(index);
	uint8_t reg = (index < 0x21) ? 0x7a : 0x79;

	cpsoc_i2c_dispatch(cpsoc_i2c_handle, reg, out_value, raw);
}

/*
 * The three real callers of cpsoc_read_switch_or_led - confirmed 2026-07-17
 * via FUN_c0007d1c, the firmware's central wire-protocol command dispatcher
 * (see KRONOS_V06R06.VSB.md's own new section on it). Host commands with
 * opcode byte 0 and reg byte 0x50/0x51/0x52 route directly to these three
 * functions - i.e. these aren't just internal helpers, they're the actual
 * host-facing entry points for reading panel switch/LED state over the wire.
 * All three share the same 0x48 (72) bounds check, matching the real 73-entry
 * (0..0x48) switch/LED table this doc already extracted.
 *
 *   reg 0x50 (cpsoc_read_switch_row)  - also sets row_state[index]=1 (marks
 *                                        the row "dirty"/just-read)
 *   reg 0x51 (cpsoc_read_led_row)     - no dirty-flag side effect
 *   reg 0x52 (cpsoc_read_switch_row_clear) - clears row_state[index]=0
 *
 * The exact semantic split (switch-press vs switch-release vs LED state)
 * isn't independently confirmed beyond this structural pattern - modeled as
 * three named wrappers around the shared read primitive rather than guessed
 * at with confidence.
 */
void cpsoc_read_switch_row(uint8_t *row_state, int index)		/* FUN_c00127e0, reg 0x50 */
{
	if (index > 0x48)
		return;
	cpsoc_read_switch_or_led(row_state, 0x50, index);
	row_state[index] = 1;
}

void cpsoc_read_led_row(void *dest, int index)				/* FUN_c0012794, reg 0x51 */
{
	if (index > 0x48)
		return;
	cpsoc_read_switch_or_led(dest, 0x51, index);
}

void cpsoc_read_switch_row_clear(uint8_t *row_state, int index)	/* FUN_c00127ac, reg 0x52 */
{
	if (index > 0x48)
		return;
	cpsoc_read_switch_or_led(row_state, 0x52, index);
	row_state[index] = 0;
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_diag_menu_input - the hidden factory diagnostic menu's input handler.
 *  @0xc0008618. Confirms and extends the .md doc's earlier "up/down-scrollable
 *  list, 0..0x48 (73 entries)" finding with the actual navigation key codes and
 *  screen layout:
 *
 *    param_3 (key code) == 0x28 -> move DOWN the list (index++, capped at 0x48)
 *    param_3 (key code) == 0x27 -> move UP the list   (index--, floored at 0)
 *    param_3 == 0x17            -> latches a "menu active" flag on key-down
 *    param_3 == 8, while that flag is latched and this is a key-down event ->
 *                                  a distinct third action (draws at y=0x230,
 *                                  then calls a not-yet-identified FUN_c0000ba0 -
 *                                  plausibly "exit/reset the diagnostic menu",
 *                                  not confirmed)
 *
 *  Screen layout confirmed from the real draw-call y-coordinates: y=0x208 (520)
 *  is the idle/header line ("Switch : %15s" / "LED : %15s" per the .md doc's
 *  own format-string extraction), y=0x21c (540) is the live switch/LED readout
 *  line for the currently-selected index, y=0x230 (560) is the param_3==8
 *  special-action line.
 *
 *  param_2 is a key event-state selector: 1 = key-down/pressed, 0 = otherwise
 *  (released, or idle redraw) - inferred from the branch structure, not an
 *  independently-confirmed enum.
 * ------------------------------------------------------------------------- */
extern int cpsoc_menu_index;			/* *DAT_c00087a0, real fixed address */
extern uint8_t cpsoc_menu_active_flag;	/* *DAT_c00087bc, real fixed address */

void cpsoc_diag_menu_input(void *display_buf, int key_state, int key_code)	/* FUN_c0008618 */
{
	if (key_state == 1) {
		/* CORRECTION (re-verification pass, 2026-07-17): the real function
		 * reads the CURRENT row via cpsoc_read_switch_row_clear (reg 0x52)
		 * at the PRE-movement index BEFORE index++/-- runs, in addition to
		 * the already-documented cpsoc_read_switch_row (reg 0x50) call at
		 * the post-movement index below - an entire extra hardware read
		 * the original draft of this function omitted. */
		cpsoc_read_switch_row_clear((uint8_t *)0 /* DAT_c00087ac */, cpsoc_menu_index);

		/* redraw the header line for whatever key_code names, then the
		 * live switch/LED readout for the (possibly just-moved) index */
		if (key_code == 0x28) {
			if (cpsoc_menu_index < 0x48)
				cpsoc_menu_index++;
		} else if (key_code == 0x27) {
			if (cpsoc_menu_index > 0)
				cpsoc_menu_index--;
		}
		cpsoc_read_switch_row((uint8_t *)0 /* DAT_c00087ac */, cpsoc_menu_index);
		/* draw_text(...) calls omitted here - see FUN_c0008618's real
		 * disassembly for the exact format-string/coordinate sequence;
		 * behaviorally: redraws both header and readout lines. */
	} else if (key_state == 0) {
		/* idle path: (re)draw just the header line at y=0x208 */
	}

	if (key_code == 0x17) {
		cpsoc_menu_active_flag = (key_state == 1);
		return;
	}
	if (key_code != 8)
		return;
	if (!cpsoc_menu_active_flag)
		return;
	if (key_state == 1) {
		/* the param_3==8 special action - not yet identified beyond
		 * "draws at y=0x230, then calls FUN_c0000ba0()". */
	}
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_event_queue_push - a generic 128-entry, 4-byte-wide ring buffer push
 *  primitive (520-byte-stride multi-instance array; real firmware-wide, not
 *  proven cpsoc-specific yet - found while chasing cpsoc.cpp's own assert
 *  call sites). Real hard-halt on overflow (count > 0x7f), not a silent drop.
 *  @0xc0010c54.
 *
 *  RESOLVED (SPI-device closure pass, 2026-07-17): this queue's real
 *  instance-selection caller is now confirmed - see cpsoc_queue_push_validated
 *  below, its ONLY caller (confirmed via get_xrefs_to). It gates entry by
 *  opcode range 0x78-0x7b (4 values) - the exact same 4 opcodes
 *  cpsoc_event_opcode_dispatch (below) handles - closing the loop: this genuinely
 *  IS cpsoc-owned, not a shared generic utility, and it's specifically the
 *  event queue for cpsoc's own analog polling chain documented below, not
 *  the host-facing switch/button event queue that section's own earlier
 *  guess suggested. Kept the "not independently confirmed" framing struck
 *  through rather than deleted, since it was a real, reasonable hypothesis
 *  at the time it was written - just not the one that turned out to be true.
 * ------------------------------------------------------------------------- */
int cpsoc_event_queue_push(void *pool_base, int instance, const uint32_t *value)	/* FUN_c0010c54 */
{
	uint8_t *inst = (uint8_t *)pool_base + instance * 0x208;
	uint16_t count = *(uint16_t *)(inst + 0x204);

	if (count > 0x7f) {
		crypto_at88_fault(0, 0 /* DAT_c0010ccc */, 0x9a);	/* shared hard-halt handler */
		return 0;
	}
	uint16_t widx = *(uint16_t *)(inst + 0x200);
	*(uint32_t *)(inst + widx * 4) = *value;
	*(uint16_t *)(inst + 0x204) = count + 1;
	*(uint16_t *)(inst + 0x200) = (widx + 1) & 0x7f;
	return 1;
}

/* ========================================================================= *
 *  cpsoc's own third SPI-bus device - analog channel polling + LED bargraph
 *  drive, resolved 2026-07-17 while chasing what omap_l108_spi.c had flagged
 *  as an unattributed "third SPI device." Full chain, address range
 *  0xc0010f00-0xc00117ff, immediately adjacent to cpsoc_event_queue_push
 *  above (0xc0010c54) and cpsoc_diag_menu_input's own supporting code:
 *
 *    cpsoc_analog_poll_task (never returns)
 *      -> cpsoc_analog_poll_channel (ADC read, quantize, notify-on-change)
 *           -> cpsoc_led_clear/cpsoc_led_set  (old value out, new value in)
 *      -> cpsoc_event_opcode_dispatch(0x78) / (0x7b)  (polled every loop tick)
 *           -> [tag-byte router] -> one of 4 LED-bargraph handlers
 *
 *  cpsoc_led_clear/cpsoc_led_set's own register encoding is the smoking gun:
 *  both collapse to reg 0x79 (index >= 0x21) or reg 0x7a (index <= 0x20) -
 *  the EXACT SAME two-register-bank split already documented above for
 *  cpsoc_read_switch_or_led. No `__FILE__` string anchors this address
 *  range directly (confirmed via a fresh full-image string search - all 14
 *  real anchor strings in this image are already accounted for elsewhere),
 *  so this attribution rests on this shared register convention plus direct
 *  code proximity/adjacency to already-anchored cpsoc.cpp code, not a
 *  string-xref proof like every other subsystem in this project - the one
 *  exception to this project's usual anchoring standard, flagged as such.
 * ========================================================================= */

/* cpsoc_led_clear/cpsoc_led_set - "clear the old LED index, light the new
 * one" pair: both compute a register (0x79 for index>=0x21, 0x7a for
 * index<=0x20 - every value is one or the other, so the tag byte written
 * first (0x50/0x52) is always overwritten; harmless dead default, not a
 * bug) and forward to cpsoc_queue_command_with_retry via the shared +0x820
 * scratch/log field, mirroring cpsoc_diag_menu_input's own use of that same
 * offset. @0xc0010fb8 (set, tag 0x50), @0xc0010fe8 (clear, tag 0x52). */
extern int cpsoc_queue_command_with_retry(uint8_t reg, void *data, int len);	/* FUN_c0010d44 */

void cpsoc_led_set(void *cpsoc, int led_index)		/* FUN_c0010fb8 */
{
	uint8_t *scratch = (uint8_t *)cpsoc + 0x820;
	uint8_t reg = (led_index > 0x20) ? 0x79 : 0x7a;

	scratch[0] = 0x50;
	scratch[1] = (uint8_t)led_index;
	cpsoc_queue_command_with_retry(reg, scratch, 2);
}

void cpsoc_led_clear(void *cpsoc, int led_index)	/* FUN_c0010fe8 */
{
	uint8_t *scratch = (uint8_t *)cpsoc + 0x820;
	uint8_t reg = (led_index > 0x20) ? 0x79 : 0x7a;

	scratch[0] = 0x52;
	scratch[1] = (uint8_t)led_index;
	cpsoc_queue_command_with_retry(reg, scratch, 2);
}

/* cpsoc_queue_command_with_retry - real command-submit-with-ack primitive:
 * retries the underlying submit up to 4 times; on repeated failure, draws
 * an error message on screen and, if a separate flag byte is clear, calls
 * the shared hard-halt handler. The exact submit primitive underneath
 * (FUN_c00032f8) and the flag byte's own meaning weren't traced further
 * this pass. @0xc0010d44. */
int cpsoc_queue_command_with_retry(uint8_t reg, void *data, int len)	/* FUN_c0010d44 */
{
	(void)reg; (void)data; (void)len;
	/* real body: up to 4 submit attempts via FUN_c00032f8(handle, reg, data,
	 * len); on exhaustion, draws an error via crypto_at88_format_fault_text-
	 * style calls and conditionally calls crypto_at88_fault at line 0 -
	 * structurally cited, not transcribed, since the underlying submit
	 * primitive and the conditional-fault flag's meaning are both still
	 * open. */
	return 0;
}

/* cpsoc_queue_push_validated - the real, confirmed sole caller of
 * cpsoc_event_queue_push above: validates the opcode is in [0x78, 0x7b]
 * (hard-faults at line 0x84 otherwise, the SAME 4-opcode range
 * cpsoc_event_opcode_dispatch below routes), brackets the actual push in an
 * interrupt-disable/restore pair (irq_save_and_disable/irq_restore - see
 * crypto_at88.c's own corrected naming for the first half of this pair),
 * and maps opcode 0x78-0x7b to queue instance 0-3. @0xc0010cd0. */
extern void irq_restore(int flags);	/* FUN_c0005510, counterpart to crypto_at88.c's irq_save_and_disable */

uint8_t cpsoc_queue_push_validated(void *pool_base, int opcode, uint8_t byte1, uint8_t byte2)	/* FUN_c0010cd0 */
{
	uint8_t entry[4];	/* real stack layout beyond the 2 explicitly-set bytes not confirmed */
	uint8_t ok;
	int flags;

	if ((unsigned int)(opcode - 0x78) >= 4) {
		crypto_at88_fault(0, 0 /* DAT_c0010d40 */, 0x84);
		return 0;
	}
	entry[0] = byte1;
	entry[1] = byte2;
	flags = irq_save_and_disable();
	ok = (uint8_t)cpsoc_event_queue_push(pool_base, opcode - 0x78, (uint32_t *)entry);
	irq_restore(flags);
	return ok;
}

/* cpsoc_event_opcode_dispatch - the opcode router: unconditionally logs the
 * opcode into a 2-byte history field at cpsoc+0x820 (cpsoc_led_set/_clear's
 * own scratch field - same address, different purpose per call), then
 * dispatches exactly opcodes 0x78/0x79/0x7a/0x7b to 4 sub-handlers (0x79 is
 * a real no-op). Default case is a genuine infinite-loop hang trap, not an
 * assert - still the only non-assert error path found anywhere in this
 * project (see omap_l108_spi.c's own note on this same function, previously
 * documented before this attribution was resolved). @0xc0011430. */
extern void cpsoc_log_opcode(void *cpsoc, int opcode, void *dest, int len);	/* FUN_c0010f60 */
extern uint8_t cpsoc_tag_router_a(void *cpsoc);	/* FUN_c00113d0, tag byte @+0x820: 0x30/0x40/0x90 */
extern uint8_t cpsoc_tag_router_b(void *cpsoc);	/* FUN_c00111e0, tag byte @+0x820: 'P' (0x50) */
extern uint8_t cpsoc_tag_router_c(void *cpsoc);	/* FUN_c0011374, tag byte @+0x820: 0x30/0x40/0x60 */

uint8_t cpsoc_event_opcode_dispatch(void *cpsoc, int opcode)	/* FUN_c0011430 */
{
	cpsoc_log_opcode(cpsoc, opcode, (uint8_t *)cpsoc + 0x820, 2);

	switch (opcode) {
	case 0x78: return cpsoc_tag_router_a(cpsoc);
	case 0x79: return 0;
	case 0x7a: return cpsoc_tag_router_b(cpsoc);
	case 0x7b: return cpsoc_tag_router_c(cpsoc);
	default:   for (;;) { }	/* confirmed: genuine hang, not an assert */
	}
}

/* cpsoc_tag_router_a/_b/_c - three tag-byte routers, each re-reading the
 * SAME history byte cpsoc_event_opcode_dispatch just logged (cpsoc+0x820)
 * and routing to one of several LED-bargraph handlers below by its value.
 * Tag 0x30 in both _a and _c routes to clcdc_test_pattern's own dispatcher
 * (FUN_c001123c, already reconstructed in clcdc.c) - directly tying this
 * analog-polling chain into the boot/factory-test menu's pattern selector.
 * Tag 'P' (0x50) in _b routes to cpsoc_led_ramp below. Structurally cited,
 * not transcribed - each is a short if/else-if chain over 2-3 fixed tag
 * values falling through to "no match, no-op" otherwise. @0xc00113d0 (_a),
 * 0xc00111e0 (_b), 0xc0011374 (_c). */
extern void clcdc_test_pattern_dispatch(void);	/* FUN_c001123c, see clcdc.c */
extern void cpsoc_led_cycle(void *cpsoc);	/* FUN_c001106c, tag 0x40 */
extern void cpsoc_led_toggle(void *cpsoc);	/* FUN_c0011094, tag 0x90 */
extern void cpsoc_led_ramp(void *cpsoc);	/* FUN_c0011170, tag 'P'/0x50 */
extern void cpsoc_led_quantize(void *cpsoc);	/* FUN_c0011018, tag 0x60 */

/* cpsoc_led_cycle - cycles an LED index through a fixed 16-wide window
 * ([0x21,0x30]) via cpsoc_led_clear, `((x-1) % 0x10) + 0x21`. @0xc001106c. */
void cpsoc_led_cycle(void *cpsoc)
{
	extern int cpsoc_led_cycle_offset;	/* DAT_c0011090, real value not resolved */
	uint8_t *state = (uint8_t *)cpsoc;
	int prev = state[cpsoc_led_cycle_offset];

	cpsoc_led_clear(cpsoc, (prev - 1) % 0x10 + 0x21);
}

/* cpsoc_led_toggle - toggles LED index 0x48 between clear/set depending on
 * a flag byte. @0xc0011094. */
void cpsoc_led_toggle(void *cpsoc)
{
	extern int cpsoc_led_toggle_flag_offset;	/* DAT_c00110b4, real value not resolved */
	uint8_t *state = (uint8_t *)cpsoc;

	if (state[cpsoc_led_toggle_flag_offset])
		cpsoc_led_set(cpsoc, 0x48);
	else
		cpsoc_led_clear(cpsoc, 0x48);
}

/* cpsoc_led_ramp - accumulates a signed per-call delta (read from the cpsoc
 * struct) into a cached, currently-lit LED index (NOT per-cpsoc-instance -
 * ground truth stores this accumulator at one fixed global address, shared
 * across calls, matching a genuine single-instance bargraph position),
 * clamped to [0x21,0x28] (8 LEDs): clears the OLD index first, THEN
 * accumulates and clamps, THEN sets the new index - a real bargraph/
 * level-meter ramp, not a one-shot set. @0xc0011170. */
void cpsoc_led_ramp(void *cpsoc)
{
	extern uint8_t cpsoc_led_ramp_position;	/* *DAT_c00111d8, real address not resolved - the cached, currently-lit LED index */
	extern int cpsoc_led_ramp_delta_offset;	/* DAT_c00111dc, real value not resolved */
	uint8_t *state = (uint8_t *)cpsoc;
	int8_t delta = (int8_t)state[cpsoc_led_ramp_delta_offset];

	cpsoc_led_clear(cpsoc, cpsoc_led_ramp_position);
	cpsoc_led_ramp_position += delta;
	if (cpsoc_led_ramp_position < 0x21)
		cpsoc_led_ramp_position = 0x21;
	else if (cpsoc_led_ramp_position > 0x28)
		cpsoc_led_ramp_position = 0x28;
	cpsoc_led_set(cpsoc, cpsoc_led_ramp_position);
	extern void cpsoc_led_ramp_redraw(void *cpsoc);	/* FUN_c0011148, not traced */
	cpsoc_led_ramp_redraw(cpsoc);
}

/* cpsoc_led_quantize - the SAME coarse-quantize-and-notify-on-change shape
 * as cpsoc_analog_poll_channel below (`(raw >> 5) + 0x29`), driving a
 * SEPARATE cached LED index than cpsoc_analog_poll_channel's own - two
 * independent quantized readings feeding two independent LED positions,
 * not the same value read twice. @0xc0011018. */
void cpsoc_led_quantize(void *cpsoc)
{
	extern uint8_t cpsoc_led_quantize_cache;	/* *DAT_c0011068, real address not resolved */
	extern int cpsoc_led_quantize_src_offset;	/* DAT_c0011064, real value not resolved */
	uint8_t *state = (uint8_t *)cpsoc;
	uint8_t next = (uint8_t)((state[cpsoc_led_quantize_src_offset] >> 5) + 0x29);

	if (cpsoc_led_quantize_cache == next)
		return;
	cpsoc_led_clear(cpsoc, cpsoc_led_quantize_cache);
	cpsoc_led_quantize_cache = next;
	cpsoc_led_set(cpsoc, cpsoc_led_quantize_cache);
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_analog_poll_channel - edge-triggered ADC-read-and-notify: resets
 *  the device, delays, sends an SPI write (lookup-table value, same shape
 *  as cpsoc_analog_poll_task's own bring-up step), reads a 16-bit value
 *  back (hard-faults on failure), quantizes via `(raw >> 13) + 0x29`, and
 *  ONLY if the quantized value changed from a cached copy: clears the old
 *  LED index and sets the new one. Confirmed real caller:
 *  cpsoc_analog_poll_task below. @0xc0011534. Previously documented in
 *  omap_l108_spi.c before this attribution was resolved - see that file's
 *  own updated cross-reference.
 * ------------------------------------------------------------------------- */
extern void cpsoc_hw_reset_toggle(void *cpsoc, int assert);		/* FUN_c00114e0 */
extern void irq_delay(void *unused, int units);			/* FUN_c0001aa0, shared delay primitive */
extern void cpsoc_hw_init_cmd(void *cpsoc, int cmd);			/* FUN_c001150c */
extern void *cpsoc_get_handle(void *bus, int unused);			/* FUN_c0001a1c */
extern void omap_spi_write(void *spi, uint16_t value);			/* see omap_l108_spi.c */
extern int cpsoc_analog_lookup_table[];				/* real base address not resolved */
extern int cpsoc_analog_table_index;					/* DAT_c0011614-family, not resolved */
extern int cpsoc_read_16(void *handle, uint16_t *out);			/* FUN_c000366c */

void cpsoc_analog_poll_channel(void *cpsoc)	/* FUN_c0011534 */
{
	extern uint8_t cpsoc_analog_led_cache;	/* real address not resolved */
	uint16_t raw;
	uint8_t next;

	cpsoc_hw_reset_toggle(cpsoc, 1);
	irq_delay(0, 1000);
	cpsoc_hw_init_cmd(cpsoc, 0 /* real command value not resolved */);
	irq_delay(0, 200);
	omap_spi_write(cpsoc_get_handle(cpsoc, 0), (uint16_t)cpsoc_analog_lookup_table[cpsoc_analog_table_index]);

	if (!cpsoc_read_16(cpsoc_get_handle(cpsoc, 0), &raw))
		crypto_at88_fault(0, 0 /* DAT_c0011620 */, 0x29c);

	next = (uint8_t)((raw >> 13) + 0x29);
	if (next == cpsoc_analog_led_cache)
		return;
	cpsoc_led_clear(cpsoc, cpsoc_analog_led_cache);
	cpsoc_analog_led_cache = next;
	cpsoc_led_set(cpsoc, cpsoc_analog_led_cache);
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_analog_poll_task - NEVER RETURNS. Hardware bring-up (reset toggle
 *  via cpsoc_hw_reset_toggle, the SAME 0x9000 SPI config command cad_init
 *  sends to its own chip, a second reset toggle, a lookup-table-driven SPI
 *  write) followed by an unconditional infinite loop, polled every
 *  iteration:
 *   - cpsoc_analog_poll_channel (above)
 *   - cpsoc_event_opcode_dispatch(cpsoc, 0x78) and (cpsoc, 0x7b)
 *  A standalone background polling task, not registered with the shared
 *  wire-protocol dispatcher the way cad.c/cpsoc.c's own read-row functions
 *  are. How/where this task itself gets started was not traced this pass -
 *  it is NOT eva_board_main.c's own init-table entry, confirmed unrelated
 *  there. @0xc0011624 (no Ghidra function boundary; read directly from raw
 *  disassembly, same as eva_board_main.c's own init-table/main-loop code).
 * ------------------------------------------------------------------------- */
void cpsoc_analog_poll_task(void *cpsoc)	/* FUN_c0011624 */
{
	cpsoc_hw_reset_toggle(cpsoc, 1);
	cpsoc_hw_reset_toggle(cpsoc, 0);
	omap_spi_write(cpsoc_get_handle(cpsoc, 0), 0x9000);
	cpsoc_hw_reset_toggle(cpsoc, 0);
	omap_spi_write(cpsoc_get_handle(cpsoc, 0), (uint16_t)cpsoc_analog_lookup_table[cpsoc_analog_table_index]);

	for (;;) {
		cpsoc_analog_poll_channel(cpsoc);
		cpsoc_event_opcode_dispatch(cpsoc, 0x78);
		cpsoc_event_opcode_dispatch(cpsoc, 0x7b);
	}
}

/* -------------------------------------------------------------------------
 * Still genuinely open for this whole SPI-device section:
 *  - Most DAT_ constants (no data-segment symbols resolved in this
 *    ELF-wrapper import) - real lookup-table contents, cache-field
 *    addresses, and the flag/offset fields each LED handler reads.
 *  - The underlying SPI submit primitive inside cpsoc_queue_command_with_retry
 *    (FUN_c00032f8) and its conditional-fault flag's meaning.
 *  - How/where cpsoc_analog_poll_task itself gets started as a background
 *    task - the one remaining real "who calls this" gap in this section.
 *  - Whether this section's lack of a `__FILE__` anchor means it's
 *    genuinely part of cpsoc.cpp's own translation unit (most likely, given
 *    the shared register convention and code proximity) or a separate
 *    compilation unit inlined/placed adjacently by the linker - left as the
 *    one attribution in this project without a string-xref proof.
 * ------------------------------------------------------------------------- */
