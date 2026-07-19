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
 *  cpsoc_state_clear - NEWLY RECONSTRUCTED this pass (was `FUN_c0012724`,
 *  entirely uncatalogued in any prior pass). Found by sweeping the address
 *  gap immediately before the confirmed `cpsoc_read_shadow` anchor
 *  (`0xc0012740`) with the query tool - a real, previously-missed function
 *  sitting right at the boundary of this compilation unit. Trivial body:
 *
 *      void FUN_c0012724(int param_1)
 *      {
 *          int i = 0;
 *          do { *(uint8_t *)(param_1 + i) = 0; i++; } while (i < 0x49);
 *      }
 *
 *  A plain zero-fill of a 0x49 (73)-byte buffer - the EXACT SAME entry
 *  count as this file's own confirmed 73-entry (0..0x48) switch/LED table
 *  (`cpsoc_read_switch_row`/`_led_row`/`_switch_row_clear`'s shared `> 0x48`
 *  bounds check, below). RESOLVES a genuinely open cross-file question:
 *  `eva_board_main.c`'s own `eva_board_final_setup` reconstruction lists
 *  this exact function (its own `FUN_c0012724`, called unconditionally at
 *  `0xc0007500` from `eva_board_final_setup`/`FUN_c00074bc`) as one of
 *  three callees with "suggestive-but-not-confirmed evidence" of being
 *  "cpsoc.cpp's own state-table clear" - confirmed here as a real cpsoc.c
 *  function by direct decompile: this is that state-table clear, zeroing
 *  the 73-byte switch/LED row-state shadow array at board bring-up, before
 *  any host command can read it. Sole caller confirmed via xrefs_to.
 *  @0xc0012724. */
void cpsoc_state_clear(void *row_state)	/* FUN_c0012724, real caller: eva_board_final_setup (eva_board_main.c) */
{
	uint8_t *p = (uint8_t *)row_state;
	int i;

	for (i = 0; i < 0x49; i++)
		p[i] = 0;
}

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
 *
 *  CORRECTION (2026-07-18 pass): the callee this dispatches to
 *  (`cpsoc_i2c_dispatch`, FUN_c0007120) does NOT do a live I2C/hardware
 *  access at all, despite its name and its `handle` parameter - fully
 *  re-decompiled this pass, see its own definition below. The `handle`
 *  parameter here is dead (the real callee ignores its own r0 and loads a
 *  fixed pool-base global instead) - kept for call-site-shape fidelity, not
 *  because it does anything. Left `cpsoc_i2c_handle`/DAT_c0012790 in place
 *  as documentation of what register the real disassembly loads there, even
 *  though it's provably unused by the callee.
 * ------------------------------------------------------------------------- */
extern uint8_t cpsoc_i2c_dispatch(void *handle, uint8_t reg, uint32_t out_value,
				  uint8_t raw_bit);			/* FUN_c0007120 */
extern void *cpsoc_i2c_handle;					/* DAT_c0012790 */

void cpsoc_read_switch_or_led(void *param1, uint32_t out_value, int index)	/* FUN_c0012750 */
{
	uint8_t raw = cpsoc_read_shadow(index);
	uint8_t reg = (index < 0x21) ? 0x7a : 0x79;

	cpsoc_i2c_dispatch(cpsoc_i2c_handle, reg, out_value, raw);
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_i2c_dispatch - RESOLVED this pass (was previously undecompiled,
 *  named purely from the caller's apparent intent). Real body:
 *
 *      char FUN_c0007120(void)
 *      {
 *          char cVar1;
 *          cVar1 = FUN_c0010cd0(DAT_c000714c);
 *          if (cVar1 != '\0') { FUN_c001d22c(1,0x1000); }
 *          return cVar1;
 *      }
 *
 *  Ghidra bounds this as a ZERO-argument function - another instance of
 *  this project's recurring "phantom forwarded parameter" pattern (already
 *  seen in cdix4192.c's register wrappers and eva_board_main.c's watchdog
 *  wrapper): it overwrites its own r0 with the fixed pool-base global
 *  `DAT_c000714c` before calling FUN_c0010cd0, but never touches r1/r2/r3,
 *  so the caller's `reg`/`out_value`/`raw_bit` (this function's real,
 *  un-shown params 2-4) ride through untouched into FUN_c0010cd0's own
 *  r1-r3. FUN_c0010cd0 is `cpsoc_queue_push_validated` (below) -
 *  confirmed by address and by DAT_c000714c matching the pool-base this
 *  whole section already uses. So this function is NOT a live I2C access at
 *  all: it's a thin wrapper that (a) calls cpsoc_queue_push_validated(fixed
 *  pool_base, reg, (uint8_t)out_value, raw_bit) - reg is always 0x79 or
 *  0x7a here, both inside cpsoc_queue_push_validated's valid [0x78,0x7b]
 *  range - and (b) on a successful push, posts event-flag 0x1000 via
 *  FUN_c001d22c(1, 0x1000) (an RTOS-style event-flag/semaphore "set bits"
 *  primitive, not itself reconstructed - out of this file's scope). THIS
 *  CLOSES THE LOOP end-to-end: bit 0x1000 is the exact same bit
 *  `cpsoc_dispatch_tick` (below) is gated on inside the master dispatcher
 *  `FUN_c0008b64` - i.e. a host-facing switch/LED-row read (opcode
 *  0x50/0x51/0x52) doesn't touch the PSoC chip synchronously at all: it
 *  enqueues a {out_value, raw_bit} record into the SAME 4-instance ring
 *  buffer the third-SPI-device LED-bargraph chain uses (instance = reg -
 *  0x78, so 1 or 2), then posts the flag that wakes the master dispatcher's
 *  own drain call on its next pass. Return value is the push's success/fail
 *  bool (previously undocumented - the extern above is now `uint8_t`
 *  return, not `void`). @0xc0007120.
 * ------------------------------------------------------------------------- */

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
 *                                  then calls `panel_gpio_reset_pulse` - RESOLVED
 *                                  this pass, see that function's own definition
 *                                  below: a real GPIO assert/60000-tick-hold/
 *                                  deassert pulse, also independently reachable
 *                                  from the master dispatcher's own opcode-9
 *                                  wire command)
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
extern void panel_gpio_reset_pulse(void);	/* FUN_c0000ba0, see its own definition below */

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
		/* RESOLVED this pass: FUN_c0000ba0 is `panel_gpio_reset_pulse`,
		 * see its own definition below. This same (draw-message, then
		 * reset-pulse) sequence is ALSO the master wire-protocol
		 * dispatcher's (FUN_c0007d1c) own opcode-9 handler - i.e. the
		 * diagnostic menu's key-8 action is functionally the same
		 * "reset" command a real host packet can also trigger, not a
		 * diagnostic-only action. */
		panel_gpio_reset_pulse();
	}
}

/* ------------------------------------------------------------------------- *
 *  panel_gpio_reset_pulse - RESOLVED this pass and NOW GIVEN A REAL BODY
 *  (was `FUN_c0000ba0`, previously documented only as a pseudo-code
 *  comment with no compilable definition - closed this pass, since Ghidra
 *  now bounds a real 64-byte function object at this address, superseding
 *  the earlier "no Ghidra function boundary, read from raw disassembly"
 *  note). Real decompile:
 *
 *      void FUN_c0000ba0(void)
 *      {
 *          uVar2 = DAT_c0000be0;               // = 0xc00e0068, the SAME
 *                                               //   shared context handle
 *                                               //   cad_delay_ticks/
 *                                               //   cpsoc_get_*_handle use
 *                                               //   everywhere else in this
 *                                               //   file
 *          uVar1 = FUN_c0001990(DAT_c0000be0); // get a generic device
 *                                               //   handle (66 xrefs
 *                                               //   firmware-wide - a
 *                                               //   shared "current board
 *                                               //   object" singleton
 *                                               //   getter, not cpsoc-
 *                                               //   specific; canonical
 *                                               //   name/reconstruction is
 *                                               //   i2c_by_gpio.c's own
 *                                               //   gpio_bank_get_base)
 *          FUN_c0002620(uVar1,1);              // assert  GPIO bank 3, bit 8
 *          FUN_c0001aa0(uVar2,DAT_c0000be4);   // cad_delay_ticks(.., 0xea60)
 *                                               //   = 60000 ticks - a real,
 *                                               //   long hold time, not a
 *                                               //   microsecond glitch pulse
 *          uVar2 = FUN_c0001990(uVar2);
 *          FUN_c0002620(uVar2,0);              // deassert GPIO bank 3, bit 8
 *      }
 *
 *  Two confirmed real callers (re-confirmed this pass via xrefs_to):
 *  `cpsoc_diag_menu_input`'s own key-8 diagnostic action, above, and -
 *  independently - the master wire-protocol dispatcher `wire_dispatch_
 *  command`'s (`FUN_c0007d1c`, `wire_dispatch.c`) own top-level command
 *  byte `== 9` handler, which first formats+draws a message at the SAME
 *  y=0x230 screen coordinate as this file's own key-8 action before
 *  calling this function - concrete evidence they're the same logical
 *  action reached two ways (diagnostic-menu key and real host wire
 *  command), not a diagnostic-only stub. Not itself part of cpsoc.cpp's
 *  own dispatch table (`wire_dispatch_command` lives outside this file's
 *  address range). @0xc0000ba0.
 * ------------------------------------------------------------------------- */
extern void *gpio_bank_get_base(void);				/* FUN_c0001990 - canonical reconstruction in i2c_by_gpio.c, 66 xrefs firmware-wide, not cpsoc-exclusive */
extern void  panel_gpio_level_set(void *bank, int level);	/* FUN_c0002620, see below */
extern void  irq_delay(void *unused, int units);		/* FUN_c0001aa0 - forward decl, full citation later in this file near cpsoc_analog_poll_channel */

void panel_gpio_reset_pulse(void)	/* FUN_c0000ba0 */
{
	void *ctx = (void *)0xc00e0068;	/* DAT_c0000be0, shared context handle */
	void *bank;

	bank = gpio_bank_get_base();		/* real arg DAT_c0000be0 ignored - see i2c_by_gpio.c's own confirmed zero-real-args finding for this function */
	panel_gpio_level_set(bank, 1);		/* assert GPIO bank 3, bit 8 */
	irq_delay(ctx, 0xea60);		/* DAT_c0000be4 = 60000 ticks - long hold, not a glitch pulse */
	bank = gpio_bank_get_base();
	panel_gpio_level_set(bank, 0);		/* deassert GPIO bank 3, bit 8 */
}

/* panel_gpio_level_set - NEWLY RECONSTRUCTED this pass (was bare `FUN_c0002620`,
 * only described inline in panel_gpio_reset_pulse's own comment, never
 * given its own definition or confirmed scope). Real body:
 *
 *     void FUN_c0002620(int param_1, int param_2)
 *     {
 *         if (param_2 == 1) { FUN_c00022d0(param_1,3,8); return; }
 *         FUN_c00022e0(param_1,3,8);
 *     }
 *
 * CONFIRMED cpsoc-PRIVATE (not a generic shared GPIO utility, unlike
 * `FUN_c00022d0`/`FUN_c00022e0` themselves - see below): `get_xrefs_to`
 * shows exactly 2 callers, BOTH inside `panel_gpio_reset_pulse` above. This
 * is the one real GPIO level-select wrapper this file owns outright, sitting
 * on top of the genuinely firmware-wide bit-set/bit-clear primitives.
 * @0xc0002620. */
extern void gpio_reg_set_bit(void *bank, int group, uint32_t mask);	/* FUN_c00022d0 - 13 callers firmware-wide, generic GPIO bit-set primitive, NOT cpsoc-exclusive, not reconstructed here */
extern void gpio_reg_clear_bit(void *bank, int group, uint32_t mask);	/* FUN_c00022e0 - sibling clear primitive, same scope note */

void panel_gpio_level_set(void *bank, int level)	/* FUN_c0002620 */
{
	if (level == 1)
		gpio_reg_set_bit(bank, 3, 8);
	else
		gpio_reg_clear_bit(bank, 3, 8);
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
 *
 *  CORRECTION (2026-07-18 pass): the italicized claim directly above is now
 *  itself half-wrong and left visible rather than silently deleted, same
 *  house style: `cpsoc_i2c_dispatch` (see its full re-decompile above,
 *  under cpsoc_read_switch_or_led) turns out to ALSO be cpsoc_queue_push_validated's
 *  caller, via FUN_c0010cd0 - so this ring buffer genuinely IS shared
 *  between the third-SPI-device chain AND the host-facing switch/LED-row
 *  read path (opcode 0x50/0x51/0x52) after all. Both original guesses were
 *  each half right: it's cpsoc-owned (not a firmware-wide generic utility),
 *  AND it's the switch/LED path's real queue, AND the analog-polling
 *  chain's queue - the same 4 instances serve both producers.
 *
 *  Also resolved: `DAT_c0010ccc` (the fault-call's `file` argument) is the
 *  literal string **`"../cpsoc.cpp"`** (`0xc0023190`, string length 12,
 *  confirmed via a direct string-table lookup this pass) - the SAME literal
 *  used at every other fault call site in this whole "third SPI device"
 *  section (`cpsoc_queue_push_validated`, `cpsoc_queue_command_with_retry`,
 *  `cpsoc_analog_poll_channel`, and the newly-reconstructed
 *  `cpsoc_dispatch_tick` family below - see each function's own citation).
 *  This directly resolves this project's own previously-stated open
 *  question ("whether this section's lack of a `__FILE__` anchor means...
 *  cpsoc.cpp's own translation unit... or a separate one") - it does NOT
 *  lack an anchor; every fault call in it cites `"../cpsoc.cpp"` by name.
 *  The earlier "no anchor found" conclusion was from a string-*xref* search
 *  (Ghidra's auto-analysis apparently doesn't track these particular
 *  constant-propagated DAT_ literals as real xrefs to the string); a direct
 *  string-table address lookup on each resolved DAT_ finds it immediately.
 * ------------------------------------------------------------------------- */
int cpsoc_event_queue_push(void *pool_base, int instance, const uint32_t *value)	/* FUN_c0010c54 */
{
	uint8_t *inst = (uint8_t *)pool_base + instance * 0x208;
	uint16_t count = *(uint16_t *)(inst + 0x204);

	if (count > 0x7f) {
		crypto_at88_fault(0, "../cpsoc.cpp" /* DAT_c0010ccc = 0xc0023190 */, 0x9a);	/* shared hard-halt handler */
		return 0;
	}
	uint16_t widx = *(uint16_t *)(inst + 0x200);
	*(uint32_t *)(inst + widx * 4) = *value;
	*(uint16_t *)(inst + 0x204) = count + 1;
	*(uint16_t *)(inst + 0x200) = (widx + 1) & 0x7f;
	return 1;
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_event_queue_pop - the real, previously-unreconstructed pop
 *  counterpart to cpsoc_event_queue_push (found this pass while tracing
 *  cpsoc_queue_drain_writes below). RESOLVES the instance struct's read-index
 *  field: offset `+0x202` (`DAT_c0010c50 == 0x202`), sitting exactly between
 *  the write-index at `+0x200` (cpsoc_event_queue_push, above) and the count
 *  at `+0x204` - the full instance layout is now: `128 x uint32_t slots
 *  (0x000-0x1FF) | uint16_t widx (0x200) | uint16_t ridx (0x202) |
 *  uint16_t count (0x204) | 2 pad bytes | ... 0x208 total stride`.
 *  Returns 0 (empty) if count==0, else copies the slot at `ridx`, advances
 *  `ridx` modulo 128, and decrements count under the SAME
 *  irq_save_and_disable/irq_restore critical-section pair
 *  cpsoc_queue_push_validated uses around the push side (confirmed:
 *  `FUN_c0005500`/`FUN_c0005510` by address). @0xc0010bf0. */
extern int irq_save_and_disable(void);	/* FUN_c0005500, see crypto_at88.c */
extern void irq_restore(int flags);	/* FUN_c0005510, counterpart to crypto_at88.c's irq_save_and_disable */

int cpsoc_event_queue_pop(void *pool_base, int instance, uint32_t *out_value)	/* FUN_c0010bf0 */
{
	uint8_t *inst = (uint8_t *)pool_base + instance * 0x208;
	uint16_t count = *(uint16_t *)(inst + 0x204);
	uint16_t ridx;

	if (count == 0)
		return 0;
	ridx = *(uint16_t *)(inst + 0x202);
	*out_value = *(uint32_t *)(inst + ridx * 4);
	*(uint16_t *)(inst + 0x202) = (ridx + 1) & 0x7f;
	irq_save_and_disable();
	*(uint16_t *)(inst + 0x204) = count - 1;
	irq_restore(0 /* real saved-flags value not captured by this decompile */);
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
 *  cpsoc_read_switch_or_led.
 *
 *  CORRECTION (2026-07-18 pass): the claim directly above - "no `__FILE__`
 *  string anchors this address range directly... the one exception to this
 *  project's usual anchoring standard" - is now WRONG and superseded. Once
 *  the section's `DAT_` fault-call file arguments were actually resolved
 *  (they'd previously been left as unresolved "0, comment DAT_xxx" placeholders
 *  throughout this file), every one of them - `cpsoc_event_queue_push`,
 *  `cpsoc_queue_push_validated`, `cpsoc_queue_command_with_retry`,
 *  `cpsoc_analog_poll_channel`, and the newly-reconstructed
 *  `cpsoc_dispatch_tick`/`cpsoc_read_event_pair` below - resolves to the
 *  literal string `"../cpsoc.cpp"` at `0xc0023190`. This section is NOT an
 *  anchoring exception after all: it cites its own compilation unit's
 *  `__FILE__` string directly, the same as every other subsystem in this
 *  project. (The earlier "full-image string search found no xref" claim
 *  was apparently a false negative of Ghidra's own xref tracking for these
 *  particular constant-propagated literals, not a real absence of the
 *  string reference - a direct string-table address lookup on each
 *  resolved `DAT_` finds it immediately, no search needed.)
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

/* ------------------------------------------------------------------------- *
 *  cpsoc_spi_submit_write - RESOLVED this pass (was `FUN_c00032f8`, "the
 *  underlying SPI submit primitive... not traced further"). Real body,
 *  fully re-decompiled:
 *
 *      bool FUN_c00032f8(int handle, reg, byte *data, int len)
 *      {
 *          cad_delay_ticks(.., 200);                    // initial settle
 *          for (i = 0; i < 1000; i++) {                 // busy-wait: device
 *              if ((*(handle+8) & 0x1000) == 0) break;  //   "busy" bit
 *              cad_delay_ticks(.., 1);                  //   (bit 0x1000)
 *              if (i > 999) return false;                //   must CLEAR
 *          }
 *          *(handle+0x14) = len;                        // program length
 *          *(handle+0x1c) = reg;                         // program reg-select
 *          *(handle+0x24) = <last live status snapshot>;  // control-reg base
 *          cad_delay_ticks(.., 10);                       // setup settle
 *          for (i = 0; i < len; i++) {
 *              *(handle+0x20) = data[i];                  // stage TX byte
 *              for (j = 0; j < 1000; j++) {                // per-byte
 *                  if ((*(handle+8) & 0x10) != 0) break;    //  ready-wait:
 *                  cad_delay_ticks(.., <undetermined>);      //  bit 0x10
 *              }                                            //  must SET
 *              if (byte timed out) break;
 *          }
 *          cad_delay_ticks(.., 200);                      // final settle
 *          *(handle+0x24) |= 0x800;                        // latch "done"
 *          return <last per-byte wait succeeded>;
 *      }
 *
 *  A real, bounded SPI byte-stream write: pre-checks a "device busy" status
 *  bit (0x1000 at handle+8) clearing within 1000x1-tick polls, programs
 *  length/reg-select/a control-word snapshot into the register block, then
 *  writes each byte of `data` into a TX-data register (handle+0x20),
 *  polling a SEPARATE status bit (0x10, opposite polarity - must SET, not
 *  clear) per byte with the same 1000-iteration bound. Sets a "transaction
 *  done" flag bit (0x800) unconditionally at the end. Returns false if the
 *  initial busy-wait times out OR the last byte's ready-wait times out.
 *  The inner per-byte poll's own delay call (`cad_delay_ticks(DAT_c00033e8)`
 *  with no explicit second argument in the real disassembly) is another
 *  instance of this project's "phantom forwarded parameter" pattern - the
 *  tick-count argument rides through from an earlier register value rather
 *  than being freshly loaded; its real value wasn't isolated this pass.
 *  Sole real caller: cpsoc_queue_command_with_retry, below. @0xc00032f8. */
extern void cad_delay_ticks(void *unused_param, int32_t target);	/* FUN_c00085a8, see omap_l108.c */

int cpsoc_spi_submit_write(void *handle, uint8_t reg, const uint8_t *data, int len)	/* FUN_c00032f8 */
{
	volatile uint32_t *status = (volatile uint32_t *)((uint8_t *)handle + 8);
	int i, j;
	int last_byte_ok = 1;

	cad_delay_ticks(0, 200);
	for (i = 0; ; i++) {
		if ((*status & 0x1000) == 0)
			break;
		cad_delay_ticks(0, 1);
		if (i > 999)
			return 0;
	}
	*(int *)((uint8_t *)handle + 0x14) = len;
	*(int *)((uint8_t *)handle + 0x1c) = reg;
	/* *(handle+0x24) is seeded from a live status-register snapshot taken
	 * during the busy-wait loop above - not a fixed constant, not modeled
	 * as an assignment here. */
	cad_delay_ticks(0, 10);
	for (i = 0; i < len; i++) {
		*((uint8_t *)handle + 0x20) = data[i];
		last_byte_ok = 0;
		for (j = 0; j < 1000; j++) {
			if ((*status & 0x10) != 0) {
				last_byte_ok = 1;
				break;
			}
			cad_delay_ticks(0, 0 /* real arg not isolated - see comment above */);
		}
		if (!last_byte_ok)
			break;
	}
	cad_delay_ticks(0, 200);
	*(uint32_t *)((uint8_t *)handle + 0x24) |= 0x800;
	return last_byte_ok;
}

/* cpsoc_spi_submit_read - the SIBLING read primitive (`FUN_c00033f0`),
 * found and reconstructed this pass while resolving `cpsoc_dispatch_tick`
 * below. Structurally near-identical to cpsoc_spi_submit_write above -
 * SAME initial busy-wait on status bit 0x1000, SAME register-block
 * programming, SAME per-byte 0x10-bit poll - but the per-byte loop READS
 * INTO `data[i]` from `*(handle+0x18)` instead of writing OUT from it, and
 * there's a real extra wrinkle on the LAST byte: when `i == len-1`, it
 * ALSO ORs `0x8000` into `*(handle+0x24)` before that final byte's poll
 * (a "this is the last byte, latch differently" flag - real meaning not
 * determined, structurally cited not guessed at). @0xc00033f0. */
int cpsoc_spi_submit_read(void *handle, uint8_t reg, uint8_t *data, int len)	/* FUN_c00033f0 */
{
	volatile uint32_t *status = (volatile uint32_t *)((uint8_t *)handle + 8);
	int i, j;
	int last_byte_ok = 1;

	cad_delay_ticks(0, 100);
	for (i = 0; ; i++) {
		if ((*status & 0x1000) == 0)
			break;
		cad_delay_ticks(0, 1);
		if (i > 999)
			return 0;
	}
	*(int *)((uint8_t *)handle + 0x14) = len;
	*(int *)((uint8_t *)handle + 0x1c) = reg;
	cad_delay_ticks(0, 10);
	for (i = 0; i < len; i++) {
		if (i == len - 1)
			*(uint32_t *)((uint8_t *)handle + 0x24) |= 0x8000;	/* "last byte" flag */
		last_byte_ok = 0;
		for (j = 0; j < 1000; j++) {
			if ((*status & 8) != 0) {	/* NOTE: bit 0x8, not 0x10 - a
							 * different ready bit than the
							 * write side's per-byte poll */
				last_byte_ok = 1;
				break;
			}
			cad_delay_ticks(0, 0 /* real arg not isolated */);
		}
		data[i] = (uint8_t)*(uint32_t *)((uint8_t *)handle + 0x18);	/* RX-data register */
		if (!last_byte_ok)
			break;
	}
	cad_delay_ticks(0, 200);
	*(uint32_t *)((uint8_t *)handle + 0x24) |= 0x800;
	return last_byte_ok;
}

/* cpsoc_queue_command_with_retry - real command-submit-with-ack primitive:
 * retries the underlying submit (cpsoc_spi_submit_write, above) up to 4
 * times; on repeated failure, formats+draws the error message
 * `"Fail to send data to psoc : %d"` (`0xc00231a0`, confirmed string) at
 * screen position (100,100), and if a separate flag byte is clear, calls
 * the shared hard-halt handler with file `"../cpsoc.cpp"` (RESOLVED this
 * pass - was an unresolved `DAT_c0010dfc`). RESOLVED this pass: the
 * conditional-fault flag byte lives at fixed address `0xc00e0058`
 * (`DAT_c0010df8`) - 0x10 bytes before the shared delay/handle-context
 * constant `0xc00e0068` this whole file uses everywhere
 * (cad_delay_ticks's own dead first argument, `cpsoc_get_handle`'s
 * `DAT_c001160c`, etc.) - almost certainly one field inside the same
 * fixed device-context struct, though its own semantics (e.g. "diagnostic
 * mode: suppress hard fault on SPI failure") aren't independently
 * confirmed; its runtime value isn't captured by this static ELF-wrapper
 * import (BSS, zero at load time). Handle for the underlying submit is
 * fetched fresh via `FUN_c0001a00(DAT_c0010dec, 0)` (the same "select one
 * of two fixed handles by flag" idiom `cpsoc_get_handle`/FUN_c0001a1c uses
 * for the analog-polling chain, but a DIFFERENT handle pair - this is the
 * PSoC scan/LED chip's own bus handle, not the analog chip's).
 * @0xc0010d44. */
extern void *cpsoc_get_scan_handle(void *ctx, int which);	/* FUN_c0001a00, distinct handle-pair from cpsoc_get_handle/FUN_c0001a1c below */
extern void crypto_at88_format_fault_text(char *dst, const char *fmt,
					  const void *arg1, const void *arg3);	/* FUN_c00168fc, see crypto_at88.c */
extern void draw_text(int x, int y, const char *str, int unused);	/* FUN_c0015650, see crypto_at88.c/clcdc.c */
extern uint8_t cpsoc_command_suppress_fault_flag;	/* *0xc00e0058 (DAT_c0010df8), meaning not independently confirmed */

int cpsoc_queue_command_with_retry(uint8_t reg, void *data, int len)	/* FUN_c0010d44 */
{
	void *handle = cpsoc_get_scan_handle(0 /* DAT_c0010dec */, 0);
	int attempt;

	for (attempt = 0; attempt < 4; attempt++) {
		if (cpsoc_spi_submit_write(handle, reg, (const uint8_t *)data, len))
			return 1;
	}
	crypto_at88_format_fault_text(0 /* DAT_c0010df4 */, "Fail to send data to psoc : %d" /* DAT_c0010df0 = 0xc00231a0 */, (const void *)(intptr_t)reg, 0);
	draw_text(100, 100, 0 /* DAT_c0010df4 */, 0);
	if (!cpsoc_command_suppress_fault_flag)
		crypto_at88_fault(0, "../cpsoc.cpp" /* DAT_c0010dfc = 0xc0023190 */, 0 /* real line arg not isolated - phantom-forwarded register */);
	return 0;
}

/* cpsoc_queue_push_validated - the real, confirmed sole caller of
 * cpsoc_event_queue_push above: validates the opcode is in [0x78, 0x7b]
 * (hard-faults at line 0x84 otherwise, the SAME 4-opcode range
 * cpsoc_event_opcode_dispatch below routes), brackets the actual push in an
 * interrupt-disable/restore pair (irq_save_and_disable/irq_restore - see
 * crypto_at88.c's own corrected naming for the first half of this pair),
 * and maps opcode 0x78-0x7b to queue instance 0-3. @0xc0010cd0.
 * RESOLVED this pass: this function's own real, sole caller is
 * cpsoc_i2c_dispatch (FUN_c0007120, see its full re-decompile above under
 * cpsoc_read_switch_or_led) - confirming the host-facing switch/LED-row
 * read path shares this exact queue with the third-SPI-device chain. */

uint8_t cpsoc_queue_push_validated(void *pool_base, int opcode, uint8_t byte1, uint8_t byte2)	/* FUN_c0010cd0 */
{
	uint8_t entry[4];	/* real stack layout beyond the 2 explicitly-set bytes not confirmed */
	uint8_t ok;
	int flags;

	if ((unsigned int)(opcode - 0x78) >= 4) {
		crypto_at88_fault(0, "../cpsoc.cpp" /* DAT_c0010d40 = 0xc0023190 */, 0x84);
		return 0;
	}
	entry[0] = byte1;
	entry[1] = byte2;
	flags = irq_save_and_disable();
	ok = (uint8_t)cpsoc_event_queue_push(pool_base, opcode - 0x78, (uint32_t *)entry);
	irq_restore(flags);
	return ok;
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_read_event_pair - RESOLVED/RENAMED this pass. Previously modeled
 *  as `cpsoc_log_opcode` ("unconditionally logs the opcode into a 2-byte
 *  history field") - that was WRONG, found while fully re-decompiling
 *  `FUN_c0010f60`:
 *
 *      void FUN_c0010f60(handle_unused, reg, byte *dest, int len)
 *      {
 *          handle = FUN_c0001a00(DAT_c0010fac, 0);       // fresh handle,
 *                                                          //  same selector
 *                                                          //  idiom as
 *                                                          //  cpsoc_get_scan_handle
 *          ok = cpsoc_spi_submit_read(handle, reg, dest, len);
 *          if (!ok)
 *              crypto_at88_fault(0, "../cpsoc.cpp", 0x11d); // line 285,
 *                                                             // RESOLVED
 *                                                             // this pass
 *      }
 *
 *  This is a READ, not a log: it fetches `len` fresh bytes FROM the PSoC
 *  chip (reg = the opcode) INTO `dest`, hard-faulting on failure - the
 *  opposite direction from what the old name implied. In
 *  cpsoc_event_opcode_dispatch below, this means the dispatcher reads a
 *  fresh 2-byte record from hardware into cpsoc+0x820/+0x821 BEFORE
 *  routing on it, not that it's recording something already known.
 *  @0xc0010f60. */
extern uint8_t cpsoc_read_event_pair(void *cpsoc_unused, int opcode, void *dest, int len);	/* FUN_c0010f60 */

/* cpsoc_event_opcode_dispatch - the opcode router: reads a fresh 2-byte
 * record from the chip via reg=opcode into cpsoc+0x820/+0x821
 * (cpsoc_read_event_pair, above - cpsoc_led_set/_clear's own scratch field,
 * same address, different purpose per call), hard-faulting on read failure,
 * then dispatches exactly opcodes 0x78/0x79/0x7a/0x7b to 4 sub-handlers
 * (0x79 is a real no-op). Default case is a genuine infinite-loop hang
 * trap, not an assert - still the only non-assert error path found
 * anywhere in this project (see omap_l108_spi.c's own note on this same
 * function, previously documented before this attribution was resolved).
 * @0xc0011430. */
extern uint8_t cpsoc_tag_router_a(void *cpsoc);	/* FUN_c00113d0, tag byte @+0x820: 0x30/0x40/0x90 */
extern uint8_t cpsoc_tag_router_b(void *cpsoc);	/* FUN_c00111e0, tag byte @+0x820: 'P' (0x50) */
extern uint8_t cpsoc_tag_router_c(void *cpsoc);	/* FUN_c0011374, tag byte @+0x820: 0x30/0x40/0x60 */

uint8_t cpsoc_event_opcode_dispatch(void *cpsoc, int opcode)	/* FUN_c0011430 */
{
	cpsoc_read_event_pair(cpsoc, opcode, (uint8_t *)cpsoc + 0x820, 2);

	switch (opcode) {
	case 0x78: return cpsoc_tag_router_a(cpsoc);
	case 0x79: return 0;
	case 0x7a: return cpsoc_tag_router_b(cpsoc);
	case 0x7b: return cpsoc_tag_router_c(cpsoc);
	default:   for (;;) { }	/* confirmed: genuine hang, not an assert */
	}
}

/* cpsoc_tag_router_a/_b/_c - three tag-byte routers, each re-reading the
 * SAME history byte cpsoc_read_event_pair just fetched (cpsoc+0x820) and
 * routing to one of several LED-bargraph handlers below by its value.
 * Tag 0x30 in both _a and _c routes to clcdc_test_pattern's own dispatcher
 * (FUN_c001123c, already reconstructed in clcdc.c) - directly tying this
 * analog-polling chain into the boot/factory-test menu's pattern selector.
 * Tag 'P' (0x50) in _b routes to cpsoc_led_ramp below.
 *
 * FULLY TRANSCRIBED this pass (previously "structurally cited, not
 * transcribed" - now fully re-decompiled, see below each). Every sub-call
 * (`clcdc_test_pattern_dispatch`/`cpsoc_led_cycle`/`cpsoc_led_toggle`/
 * `cpsoc_led_quantize`/`cpsoc_led_ramp`) is invoked with ZERO explicit
 * arguments in the real disassembly - the same recurring "phantom forwarded
 * parameter" pattern flagged elsewhere in this project (cdix4192.c,
 * eva_board_main.c): `cpsoc` rides through untouched in r0 rather than
 * being reloaded, so each callee still receives it correctly at the ABI
 * level even though Ghidra's decompile shows a no-arg call.
 * @0xc00113d0 (_a), 0xc00111e0 (_b), 0xc0011374 (_c). */
extern void clcdc_test_pattern_dispatch(void *cpsoc);	/* FUN_c001123c, see clcdc.c - phantom-forwarded cpsoc */
extern void cpsoc_led_cycle(void *cpsoc);	/* FUN_c001106c, tag 0x40 */
extern void cpsoc_led_toggle(void *cpsoc);	/* FUN_c0011094, tag 0x90 */
extern void cpsoc_led_ramp(void *cpsoc);	/* FUN_c0011170, tag 'P'/0x50 */
extern void cpsoc_led_quantize(void *cpsoc);	/* FUN_c0011018, tag 0x60 */

uint8_t cpsoc_tag_router_a(void *cpsoc)	/* FUN_c00113d0 */
{
	uint8_t tag = *((uint8_t *)cpsoc + 0x820);

	if (tag == 0x40) {
		cpsoc_led_cycle(cpsoc);
	} else if (tag == 0x30) {
		clcdc_test_pattern_dispatch(cpsoc);
		return 1;
	} else if (tag == 0x90) {
		cpsoc_led_toggle(cpsoc);
		return 1;
	} else {
		return 0;
	}
	return 1;
}

uint8_t cpsoc_tag_router_b(void *cpsoc)	/* FUN_c00111e0 */
{
	uint8_t tag = *((uint8_t *)cpsoc + 0x820);

	if (tag == 'P')	/* 0x50 */
		cpsoc_led_ramp(cpsoc);
	return (tag == 'P');
}

uint8_t cpsoc_tag_router_c(void *cpsoc)	/* FUN_c0011374 */
{
	uint8_t tag = *((uint8_t *)cpsoc + 0x820);

	if (tag == 0x30) {
		clcdc_test_pattern_dispatch(cpsoc);
		return 1;
	}
	if (tag > 0x2f) {
		if (tag == 0x40) {
			cpsoc_led_cycle(cpsoc);
			return 1;
		}
		if (tag == 0x60) {
			cpsoc_led_quantize(cpsoc);
			return 1;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  RESOLVED this pass: cpsoc_led_cycle/_toggle/_ramp/_quantize's four
 *  "offset, real value not resolved" DAT_ constants (`DAT_c0011090`,
 *  `DAT_c00110b4`, `DAT_c00111dc`, `DAT_c0011064`) all resolve to the SAME
 *  literal immediate value: **0x821**. This isn't 4 separate globals - it's
 *  ONE shared field, `cpsoc+0x821`, the SECOND byte of the very scratch
 *  pair `cpsoc_read_event_pair` (above) just fetched from hardware into
 *  `cpsoc+0x820/+0x821` (cpsoc+0x820 is the tag byte the 3 routers switch
 *  on; cpsoc+0x821 is this payload byte). So all four LED handlers read
 *  their input (prev-index / flag / delta / ADC-source-byte) from the SAME
 *  just-fetched payload byte, not from four independent per-handler struct
 *  fields as the old "offset" framing implied - a real, concrete structural
 *  simplification, not a guess (each DAT_ address independently resolves to
 *  the literal `0x821`, confirmed via direct data lookup on all four).
 * ------------------------------------------------------------------------- */
#define CPSOC_EVENT_PAYLOAD_OFFSET 0x821	/* cpsoc+0x821, shared by all 4 handlers below */

/* cpsoc_led_cycle - cycles an LED index through a fixed 16-wide window
 * ([0x21,0x30]) via cpsoc_led_clear, `((x-1) % 0x10) + 0x21`. @0xc001106c. */
void cpsoc_led_cycle(void *cpsoc)
{
	uint8_t *state = (uint8_t *)cpsoc;
	int prev = state[CPSOC_EVENT_PAYLOAD_OFFSET];

	cpsoc_led_clear(cpsoc, (prev - 1) % 0x10 + 0x21);
}

/* cpsoc_led_toggle - toggles LED index 0x48 between clear/set depending on
 * the shared payload byte (cpsoc+0x821). @0xc0011094. */
void cpsoc_led_toggle(void *cpsoc)
{
	uint8_t *state = (uint8_t *)cpsoc;

	if (state[CPSOC_EVENT_PAYLOAD_OFFSET])
		cpsoc_led_set(cpsoc, 0x48);
	else
		cpsoc_led_clear(cpsoc, 0x48);
}

/* cpsoc_led_ramp - accumulates a signed per-call delta (the shared payload
 * byte, cpsoc+0x821) into a cached, currently-lit LED index (NOT
 * per-cpsoc-instance - ground truth stores this accumulator at one fixed
 * global address, `0xc0098f9d` [RESOLVED this pass, was `*DAT_c00111d8`,
 * "real address not resolved"], shared across calls, matching a genuine
 * single-instance bargraph position), clamped to [0x21,0x28] (8 LEDs):
 * clears the OLD index first, THEN accumulates and clamps, THEN sets the
 * new index - a real bargraph/level-meter ramp, not a one-shot set.
 * @0xc0011170. */
extern uint8_t cpsoc_led_ramp_position;	/* *0xc0098f9d (DAT_c00111d8) - the cached, currently-lit LED index; adjacent in memory to cpsoc_analog_led_cache (0xc0098f9c) and cpsoc_led_quantize_cache (0xc0098f9e) below - a 3-byte contiguous cache block for the 3 independent LED-bargraph channels */
extern void cpsoc_led_ramp_redraw(void *cpsoc);	/* FUN_c0011148, see its own reconstruction below */

void cpsoc_led_ramp(void *cpsoc)
{
	uint8_t *state = (uint8_t *)cpsoc;
	int8_t delta = (int8_t)state[CPSOC_EVENT_PAYLOAD_OFFSET];

	cpsoc_led_clear(cpsoc, cpsoc_led_ramp_position);
	cpsoc_led_ramp_position += delta;
	if (cpsoc_led_ramp_position < 0x21)
		cpsoc_led_ramp_position = 0x21;
	else if (cpsoc_led_ramp_position > 0x28)
		cpsoc_led_ramp_position = 0x28;
	cpsoc_led_set(cpsoc, cpsoc_led_ramp_position);
	cpsoc_led_ramp_redraw(cpsoc);
}

/* cpsoc_led_ramp_redraw - RESOLVED this pass (was "FUN_c0011148, not
 * traced"). Real body:
 *
 *     void FUN_c0011148(int param_1)
 *     {
 *         *(param_1 + 0x820) = 0x80;
 *         *(param_1 + 0x821) = 0;
 *         FUN_c0010d44(0x7a, param_1 + 0x820, 2);   // cpsoc_queue_command_with_retry
 *     }
 *
 * Not actually a "redraw" - it stages a fixed 2-byte command {0x80, 0}
 * into the shared scratch field and submits it via reg 0x7a (the SAME
 * register cpsoc_led_set/_clear use for low-index LEDs). Real meaning of
 * command byte 0x80 not independently confirmed (plausibly "commit/latch
 * bargraph display", consistent with being the last step after the
 * clear/accumulate/set sequence above, but not proven). Also has a SECOND
 * confirmed caller besides cpsoc_led_ramp: `FUN_c00114a4` (see the
 * never-returning background-loop cluster documented near
 * cpsoc_analog_poll_task, below), which calls it once before entering its
 * own infinite dispatch loop. @0xc0011148. */
void cpsoc_led_ramp_redraw(void *cpsoc)	/* FUN_c0011148 */
{
	uint8_t *scratch = (uint8_t *)cpsoc + 0x820;

	scratch[0] = 0x80;
	scratch[1] = 0;
	cpsoc_queue_command_with_retry(0x7a, scratch, 2);
}

/* cpsoc_led_quantize - the SAME coarse-quantize-and-notify-on-change shape
 * as cpsoc_analog_poll_channel below (`(raw >> 5) + 0x29`), driving a
 * SEPARATE cached LED index than cpsoc_analog_poll_channel's own - two
 * independent quantized readings feeding two independent LED positions,
 * not the same value read twice. Input is the shared payload byte
 * (cpsoc+0x821), cache is `0xc0098f9e` [RESOLVED this pass, was
 * `*DAT_c0011068`, "real address not resolved" - the third byte of the
 * same 3-byte cache block noted above]. @0xc0011018. */
extern uint8_t cpsoc_led_quantize_cache;	/* *0xc0098f9e (DAT_c0011068) */

void cpsoc_led_quantize(void *cpsoc)
{
	uint8_t *state = (uint8_t *)cpsoc;
	uint8_t next = (uint8_t)((state[CPSOC_EVENT_PAYLOAD_OFFSET] >> 5) + 0x29);

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
/* cpsoc_hw_reset_toggle/cpsoc_hw_init_cmd - RESOLVED (parameter width) this
 * pass. Both stage a fixed tag byte (0xe0) plus a masked payload byte into
 * the shared cpsoc+0x820/+0x821 scratch field and submit via
 * cpsoc_queue_command_with_retry, but with DIFFERENT registers AND
 * different mask widths than previously modeled:
 *
 *     FUN_c00114e0(cpsoc, val): scratch[0]=0xe0; scratch[1]=(val&7)<<2;
 *                               cpsoc_queue_command_with_retry(0x78, scratch, 2);
 *     FUN_c001150c(cpsoc, val): scratch[0]=0xe0; scratch[1]=val&3;
 *                               cpsoc_queue_command_with_retry(0x79, scratch, 2);
 *
 * i.e. `cpsoc_hw_reset_toggle`'s second argument is NOT a plain assert/
 * deassert boolean - it's a 3-bit field (masked `&7`, shifted `<<2` into
 * the payload byte's bits [4:2]), submitted via reg 0x78. Real callers
 * pass values *DAT_c0011608 (poll_channel), 1, 2, 6 (analog_poll_task /
 * FUN_c00116d8, below) - i.e. genuinely different values across call
 * sites, not just 0/1. `cpsoc_hw_init_cmd`'s argument is a 2-bit field
 * (`&3`), submitted via reg 0x79 - also NOT the fixed literal 0 previously
 * assumed at some call sites; real callers dereference a fixed global
 * (`*DAT_c0011610`) whose value isn't captured by this static import.
 * @0xc00114e0 (reset_toggle), 0xc001150c (init_cmd). */
void cpsoc_hw_reset_toggle(void *cpsoc, int val)	/* FUN_c00114e0 */
{
	uint8_t *scratch = (uint8_t *)cpsoc + 0x820;

	scratch[0] = 0xe0;
	scratch[1] = (uint8_t)((val & 7) << 2);
	cpsoc_queue_command_with_retry(0x78, scratch, 2);
}

void cpsoc_hw_init_cmd(void *cpsoc, int cmd)	/* FUN_c001150c */
{
	uint8_t *scratch = (uint8_t *)cpsoc + 0x820;

	scratch[0] = 0xe0;
	scratch[1] = (uint8_t)(cmd & 3);
	cpsoc_queue_command_with_retry(0x79, scratch, 2);
}

extern void irq_delay(void *unused, int units);			/* FUN_c0001aa0, shared delay primitive - actually cad_delay_ticks by another name; kept as a distinct extern since cad.c/omap_l108.c own the canonical `cad_delay_ticks` name and this file's own call sites weren't individually re-verified against it this pass */
extern void *cpsoc_get_handle(void *bus, int unused);			/* FUN_c0001a1c - the ANALOG chip's own handle-selector, a DIFFERENT fixed handle-pair than cpsoc_get_scan_handle (FUN_c0001a00) used by cpsoc_queue_command_with_retry */
extern void omap_spi_write(void *spi, uint16_t value);			/* see omap_l108_spi.c */
extern int cpsoc_analog_lookup_table[];				/* base 0xc001fc6c (DAT_c0011618) - RESOLVED address this pass, contents not resolved (BSS/zero in this static import) */
extern int *cpsoc_analog_table_index_ptr;				/* 0xc01cd4fc (DAT_c0011614) - RESOLVED this pass: this is a POINTER TO the index variable, not the index itself - real call sites dereference it TWICE (`lookup_table[*index_ptr]`), not once as an earlier draft of this file assumed */
extern int cpsoc_read_16(void *handle, uint16_t *out);			/* FUN_c000366c */
extern uint8_t cpsoc_analog_led_cache;	/* *0xc0098f9c (DAT_c001161c) - RESOLVED this pass, first byte of the same 3-byte cache block as cpsoc_led_ramp_position (0xc0098f9d)/cpsoc_led_quantize_cache (0xc0098f9e) above */
extern int cpsoc_analog_reset_arg;	/* *DAT_c0011608 (pointer itself at 0xc01cd4f8, RESOLVED this pass) - dereferenced global, real pointed-to VALUE not captured by this static import (BSS/zero at load time), NOT the literal 1 an earlier draft of this file assumed */
extern int cpsoc_analog_init_arg;	/* *DAT_c0011610 (pointer itself at 0xc01cd500, RESOLVED this pass) - dereferenced global, real pointed-to VALUE not captured, NOT the literal 0 an earlier draft assumed */

void cpsoc_analog_poll_channel(void *cpsoc)	/* FUN_c0011534 */
{
	uint16_t raw;
	uint8_t next;
	void *handle;

	cpsoc_hw_reset_toggle(cpsoc, cpsoc_analog_reset_arg);
	irq_delay(0, 1000);
	cpsoc_hw_init_cmd(cpsoc, cpsoc_analog_init_arg);
	irq_delay(0, 200);
	handle = cpsoc_get_handle(cpsoc, 0);
	omap_spi_write(handle, (uint16_t)cpsoc_analog_lookup_table[*cpsoc_analog_table_index_ptr]);

	handle = cpsoc_get_handle(cpsoc, 0);
	if (!cpsoc_read_16(handle, &raw))
		crypto_at88_fault(0, "../cpsoc.cpp" /* DAT_c0011620 = 0xc0023190 */, 0x29c);

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
 *  are. @0xc0011624 (no Ghidra function boundary; read directly from raw
 *  disassembly, same as eva_board_main.c's own init-table/main-loop code).
 *
 *  Item 3 (how/where this task gets started) is STILL genuinely open as a
 *  static call graph question - zero xrefs/data-pointer references to
 *  0xc0011624 found anywhere in this pass's dumps, same as
 *  eva_board_start_task's own "spawns a background task" call in
 *  eva_board_main.c. BUT: this pass found TWO MORE never-returning
 *  functions with the exact same signature (zero static callers, infinite
 *  loop body) immediately adjacent in this address range - see the note
 *  block right after this function. That's real, new circumstantial
 *  evidence for how tasks like this one get started (some kind of
 *  literal-address task-spawn primitive, matching `eva_board_start_task`'s
 *  own `(1, 4)` call whose args were never resolved to a target address
 *  either) even though the exact mechanism is still not nailed down.
 * ------------------------------------------------------------------------- */
void cpsoc_analog_poll_task(void *cpsoc)	/* FUN_c0011624 */
{
	cpsoc_hw_reset_toggle(cpsoc, 1);
	cpsoc_hw_reset_toggle(cpsoc, 0);
	omap_spi_write(cpsoc_get_handle(cpsoc, 0), 0x9000);
	cpsoc_hw_reset_toggle(cpsoc, 0);
	omap_spi_write(cpsoc_get_handle(cpsoc, 0), (uint16_t)cpsoc_analog_lookup_table[*cpsoc_analog_table_index_ptr]);

	for (;;) {
		cpsoc_analog_poll_channel(cpsoc);
		cpsoc_event_opcode_dispatch(cpsoc, 0x78);
		cpsoc_event_opcode_dispatch(cpsoc, 0x7b);
	}
}

/* ------------------------------------------------------------------------- *
 *  TWO SIBLING never-returning loops, found in a prior pass while sweeping
 *  this file's full address range - NEITHER has any static caller either,
 *  same "zero xrefs anywhere in the image" signature as cpsoc_analog_poll_task
 *  above. FULLY TRANSCRIBED this pass (previously left as structural
 *  description only, "out of scope creep beyond this pass's assignment") -
 *  both decompile cleanly with no dense/ambiguous arithmetic, so there is
 *  no remaining reason not to give them real bodies. Real callers: still
 *  none found (zero xrefs_to for both, re-confirmed this pass) - same open
 *  invocation-mechanism question as cpsoc_analog_poll_task itself.
 *
 *  Hypothesis, NOT confirmed: given `cpsoc_dispatch_tick` (below) is now
 *  PROVEN to be the real, master-dispatcher-invoked consumer for this
 *  whole queue/read mechanism, it's plausible these two functions (and
 *  possibly cpsoc_analog_poll_task itself) are either dead code, an
 *  alternate/earlier build's polling strategy superseded by
 *  cpsoc_dispatch_tick's synchronous approach, or genuinely-unreachable
 *  task bodies never wired up in this firmware image - not asserted as
 *  fact, since no direct evidence (a disabled call site, a build flag,
 *  etc.) was found either way.
 * ------------------------------------------------------------------------- */

/* cpsoc_bargraph_task_variant_a - `FUN_c00114a4` (@0xc00114a4, 60 bytes).
 * Calls cpsoc_led_ramp_redraw once (phantom-forwarded cpsoc, no explicit
 * arg in the real disassembly - same pattern as every other call site into
 * this function), then loops forever dispatching 3 of the 4 event opcodes
 * (0x78, 0x7b, 0x7a - NOT 0x79, which is always a no-op per
 * cpsoc_event_opcode_dispatch above). The SAME dispatch-loop shape as
 * cpsoc_analog_poll_task's own tail loop, but WITHOUT the analog-channel
 * poll step, and dispatching 0x7a as a THIRD opcode cpsoc_analog_poll_task's
 * own loop never touches. Zero static callers. @0xc00114a4. */
void cpsoc_bargraph_task_variant_a(void *cpsoc)	/* FUN_c00114a4 */
{
	cpsoc_led_ramp_redraw(cpsoc);
	for (;;) {
		cpsoc_event_opcode_dispatch(cpsoc, 0x78);
		cpsoc_event_opcode_dispatch(cpsoc, 0x7b);
		cpsoc_event_opcode_dispatch(cpsoc, 0x7a);
	}
}

/* cpsoc_bargraph_task_variant_b - `FUN_c00116d8` (@0xc00116d8, 312 bytes).
 * A `do{}while(true)` of 3 near-identical blocks, each: cpsoc_hw_reset_toggle
 * (cpsoc, N) (N = 1, 2, 6 across the 3 blocks) -> cpsoc_hw_init_cmd(cpsoc,0)
 * -> cpsoc_led_set(cpsoc,0) (block 1 only) or cpsoc_led_clear(cpsoc,0)
 * (blocks 2-3) -> THREE calls to cpsoc_read_event_pair(cpsoc, reg,
 * cpsoc+0x820, 2) for reg = 0x78, 0x7b, 0x7a in that order, calling
 * cpsoc_read_event_pair directly rather than through cpsoc_event_opcode_
 * dispatch's own tag-router chain - i.e. this variant reads the raw
 * hardware pair but never routes/acts on the tag byte the way the other
 * two polling paths (cpsoc_analog_poll_task, cpsoc_bargraph_task_variant_a)
 * do. Zero static callers. @0xc00116d8. */
void cpsoc_bargraph_task_variant_b(void *cpsoc)	/* FUN_c00116d8 */
{
	uint8_t *pair = (uint8_t *)cpsoc + 0x820;

	for (;;) {
		cpsoc_hw_reset_toggle(cpsoc, 1);
		cpsoc_hw_init_cmd(cpsoc, 0);
		cpsoc_led_set(cpsoc, 0);
		cpsoc_read_event_pair(cpsoc, 0x78, pair, 2);
		cpsoc_read_event_pair(cpsoc, 0x7b, pair, 2);
		cpsoc_read_event_pair(cpsoc, 0x7a, pair, 2);

		cpsoc_hw_reset_toggle(cpsoc, 2);
		cpsoc_hw_init_cmd(cpsoc, 0);
		cpsoc_led_clear(cpsoc, 0);
		cpsoc_read_event_pair(cpsoc, 0x78, pair, 2);
		cpsoc_read_event_pair(cpsoc, 0x7b, pair, 2);
		cpsoc_read_event_pair(cpsoc, 0x7a, pair, 2);

		cpsoc_hw_reset_toggle(cpsoc, 6);
		cpsoc_hw_init_cmd(cpsoc, 0);
		cpsoc_led_clear(cpsoc, 0);
		cpsoc_read_event_pair(cpsoc, 0x78, pair, 2);
		cpsoc_read_event_pair(cpsoc, 0x7b, pair, 2);
		cpsoc_read_event_pair(cpsoc, 0x7a, pair, 2);
	}
}

/* ========================================================================= *
 *  cpsoc_dispatch_tick - THE REAL, CONFIRMED consumer for both the
 *  host-facing switch/LED-row queue (cpsoc_i2c_dispatch's own enqueues) and
 *  the third-SPI-device LED-bargraph queue: called SYNCHRONOUSLY, directly,
 *  from the master wire-protocol dispatcher `FUN_c0008b64`, gated on status
 *  bit **0x1000** - the EXACT SAME bit `cpsoc_i2c_dispatch` posts via
 *  `FUN_c001d22c(1, 0x1000)` on a successful enqueue. This closes the loop
 *  completely: a host-facing switch/LED-row read (opcode 0x50/0x51/0x52)
 *  enqueues a record and posts flag 0x1000 -> the master dispatcher's next
 *  pass sees bit 0x1000 set in its status word -> calls
 *  `cpsoc_dispatch_tick(DAT_c0009068)` -> which drains/reads every queue
 *  instance synchronously, no separate background task involved. Real body:
 *
 *      void FUN_c0010f08(cpsoc)
 *      {
 *          FUN_c0010b58(cpsoc, 0x78);   // cpsoc_poll_reg_reads: live READ loop
 *          FUN_c0010e48(cpsoc, 0x78);   // cpsoc_queue_drain_writes: instance 0
 *          FUN_c0010e48(cpsoc, 0x79);   // instance 1
 *          FUN_c0010b58(cpsoc, 0x7b);   // live READ loop
 *          FUN_c0010b58(cpsoc, 0x7a);   // live READ loop
 *          FUN_c0010e48(cpsoc, 0x7a);   // instance 2
 *      }
 *
 *  Note the real asymmetry (not simplified away): reg 0x79 is only ever
 *  DRAINED (write-queue side), never READ live; reg 0x7b is only ever READ
 *  live, never drained as a write-queue instance. Real reason not
 *  determined - documented as-is rather than "corrected" into a
 *  symmetrical pattern that isn't what the disassembly shows.
 *  @0xc0010f08, called from FUN_c0008b64 at 0xc0008cd4 (`if ((status &
 *  0x1000) != 0) { FUN_c0010f08(DAT_c0009068); FUN_c001422c(DAT_c0009040);
 *  }` - the trailing FUN_c001422c call, immediately after, is almost
 *  certainly the matching event-flag CLEAR/ack for the 0x1000 bit just
 *  posted by cpsoc_i2c_dispatch, not itself reconstructed here).
 * ========================================================================= */
extern uint8_t cpsoc_queue_drain_writes(void *cpsoc, int reg);	/* FUN_c0010e48 */
extern void cpsoc_poll_reg_reads(void *cpsoc, int reg);		/* FUN_c0010b58 */

void cpsoc_dispatch_tick(void *cpsoc)	/* FUN_c0010f08 */
{
	cpsoc_poll_reg_reads(cpsoc, 0x78);
	cpsoc_queue_drain_writes(cpsoc, 0x78);
	cpsoc_queue_drain_writes(cpsoc, 0x79);
	cpsoc_poll_reg_reads(cpsoc, 0x7b);
	cpsoc_poll_reg_reads(cpsoc, 0x7a);
	cpsoc_queue_drain_writes(cpsoc, 0x7a);
}

/* cpsoc_queue_drain_writes - drains the software write-queue for one
 * instance (reg-0x78) while entries remain, submitting each popped 2-byte
 * record to hardware via cpsoc_queue_command_with_retry(reg, entry, 2), and
 * relaying each result onward via a CROSS-SUBSYSTEM call into cad.c's own
 * address range (`FUN_c00073e8`, confirmed by its `DAT_c0013504`/
 * `DAT_c001362c`-family references landing inside cad.c's own
 * 0xc001335c-0xc0013f5c span - NOT reconstructed here, out of this file's
 * scope; symmetric with cad.c's own cross-subsystem call INTO cpsoc via
 * cad_pedal_send_release/cpsoc_i2c_dispatch). On a submit failure, waits
 * ~1000 ticks (2x500) and returns false without continuing to drain
 * further entries in that instance this call. Real body:
 *
 *     char FUN_c0010e48(pool_base, reg)
 *     {
 *         cpsoc_get_scan_handle(DAT_c0010f00, 0);   // fetched but its
 *                                                    //   result is UNUSED -
 *                                                    //   real, confirmed
 *                                                    //   dead call, not a
 *                                                    //   transcription gap
 *         ok = true;
 *         if (queue_instance(reg-0x78).count != 0) {
 *             do {
 *                 if (!cpsoc_event_queue_pop(pool_base, reg-0x78, &entry))
 *                     return ok;
 *                 ok = cpsoc_queue_command_with_retry(reg, &entry, 2);
 *                 FUN_c00073e8(DAT_c0010f04, reg, entry[0], entry[1]);  // cad.c
 *             } while (ok);
 *             cad_delay_ticks(.., 500); cad_delay_ticks(.., 500);
 *             ok = false;
 *         }
 *         return ok;
 *     }
 *
 * @0xc0010e48. Also confirmed to have a SECOND real caller besides
 * cpsoc_dispatch_tick: `cpsoc_drain_queue_wrapper` (`FUN_c0007150`,
 * @0xc0007150, immediately after cpsoc_i2c_dispatch's own address) - NOW
 * FULLY RECONSTRUCTED this pass, see its own definition right after this
 * function's closing brace below (previously left as "not itself
 * reconstructed"). */
extern void cad_pedal_queue_notify(void *ctx, int reg, uint8_t byte1, uint8_t byte2);	/* FUN_c00073e8, real cad.c function - out of scope here */

uint8_t cpsoc_queue_drain_writes(void *cpsoc, int reg)	/* FUN_c0010e48 */
{
	uint8_t entry[4];	/* cpsoc_event_queue_pop writes a full 4-byte slot; only entry[0]/[1] are meaningfully consumed below */
	uint8_t ok = 1;

	(void)cpsoc_get_scan_handle(0 /* DAT_c0010f00 = 0xc00e0068, same shared context handle as everywhere else */, 0);	/* real dead call, confirmed - result unused */
	/* NOTE: the real disassembly has an explicit outer `if (count != 0)`
	 * guard around this whole loop+tail; expressed here instead as
	 * cpsoc_event_queue_pop's own internal empty-check on the FIRST
	 * iteration, which is behaviorally identical - an empty queue makes
	 * pop() fail immediately, hitting `return ok` (still 1) below before
	 * the delay/ok=0 tail, exactly matching the real "skip the whole
	 * if-block, return true" empty-queue behavior. */
	do {
		if (!cpsoc_event_queue_pop(cpsoc, reg - 0x78, (uint32_t *)entry))
			return ok;
		ok = (uint8_t)cpsoc_queue_command_with_retry((uint8_t)reg, entry, 2);
		cad_pedal_queue_notify(0 /* DAT_c0010f04 = 0xc01cac00, same shared delay-handle constant as cpsoc_spi_submit_write/_read */, reg, entry[0], entry[1]);
	} while (ok);
	cad_delay_ticks(0 /* DAT_c0010f04 */, 500);
	cad_delay_ticks(0 /* DAT_c0010f04 */, 500);
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  cpsoc_drain_queue_wrapper - NEWLY RECONSTRUCTED this pass (was bare
 *  `FUN_c0007150`, address 0xc0007150, "not itself reconstructed" per this
 *  file's own earlier note). Real decompile:
 *
 *      undefined1 FUN_c0007150(void)
 *      {
 *          return FUN_c0010e48(DAT_c000716c);
 *      }
 *
 *  Ghidra bounds this as a ZERO-argument function - the SAME "phantom
 *  forwarded parameter" shape already documented for cpsoc_i2c_dispatch
 *  (FUN_c0007120, immediately before this function's own address) above:
 *  it loads a FIXED pool-base global (`DAT_c000716c` = `0xc01cb8d0`) into
 *  r0 before tail-calling cpsoc_queue_drain_writes (FUN_c0010e48), but
 *  never touches r1 - so this function's real, un-shown parameter (the
 *  caller's second argument) rides through untouched as cpsoc_queue_
 *  drain_writes' own `reg` parameter.
 *
 *  CROSS-FILE CORRECTION (flagged here, deliberately NOT edited there -
 *  cad.c is out of this file's own scope): cad.c's own `cad_init`
 *  (`FUN_c00136c0`) calls this exact function twice at the very end of cad
 *  subsystem bring-up - `FUN_c0007150(DAT_c0013818, 0x78);
 *  FUN_c0007150(DAT_c0013818, 0x79);` (confirmed via this function's own
 *  xrefs_to) - and cad.c currently names/declares it `cad_register_handler
 *  (void *dispatcher, int opcode)`, describing it as registering cad's own
 *  opcode handlers with a wire-protocol dispatcher. That description is
 *  wrong: `DAT_c0013818` (cad.c's own "dispatcher handle" argument) is
 *  dead - ignored exactly like every other phantom-forwarded first
 *  argument documented throughout this file - and the real effect is
 *  draining THIS file's own cpsoc write-queue instances 0 and 1 (reg
 *  0x78, 0x79), not registering anything with anyone. Real, concrete
 *  finding this resolves: at the end of cad subsystem bring-up, any
 *  switch/LED-row write cpsoc queued before cad finished initializing gets
 *  flushed out over cpsoc's own bus. @0xc0007150. */
uint8_t cpsoc_drain_queue_wrapper(void *unused_dispatcher, int reg)	/* FUN_c0007150 */
{
	(void)unused_dispatcher;
	return cpsoc_queue_drain_writes((void *)0 /* DAT_c000716c = 0xc01cb8d0, fixed pool_base */, reg);
}

/* cpsoc_poll_reg_reads - live hardware READ loop for one register: reads a
 * 2-byte response via cpsoc_spi_submit_read (the SAME primitive
 * cpsoc_read_event_pair uses, but through a DIFFERENT handle-selector -
 * `FUN_c0001a00(DAT_c0010be8,0)`, the cpsoc_get_scan_handle idiom), gates
 * each successful read through `cpsoc_read_tag_valid` (a real tag-byte
 * whitelist, below) and, only if valid, forwards it into ANOTHER
 * cross-subsystem call - `FUN_c0007220`, cobjectmgr.c's own "large
 * secondary protocol dispatcher, possibly a debug console" (per that
 * file's own README status, not reconstructed here). Loops while reads
 * keep succeeding AND validating AND (when valid) the forward call reports
 * "keep going" - real per-iteration exit conditions structurally cited
 * from `FUN_c0010b58`'s real disassembly, not fully transcribed since two
 * of its three callees (`cpsoc_read_tag_valid`'s exact use of its own
 * return value in the loop condition, and cobjectmgr's own dispatcher) sit
 * partly outside this file's scope. @0xc0010b58. */
extern uint8_t cpsoc_read_tag_valid(uint8_t tag);	/* FUN_c0010b08, see its own reconstruction below */
extern void cobjectmgr_secondary_dispatch(void *ctx, int reg, uint8_t byte1, uint8_t byte2);	/* FUN_c0007220, see cobjectmgr.c's own README status - not reconstructed here */

void cpsoc_poll_reg_reads(void *cpsoc, int reg)	/* FUN_c0010b58 */
{
	void *handle = cpsoc_get_scan_handle(0 /* DAT_c0010be8 = 0xc00e0068 */, 0);
	uint8_t entry[2];
	int keep_going;

	do {
		keep_going = 0;
		if (cpsoc_spi_submit_read(handle, (uint8_t)reg, entry, 2)) {
			if (cpsoc_read_tag_valid(entry[0])) {
				cobjectmgr_secondary_dispatch(0 /* DAT_c0010bec = 0xc01cac00 */, reg, entry[0], entry[1]);
				keep_going = 1;
			}
		}
	} while (keep_going);
}

/* cpsoc_read_tag_valid - a real, confirmed whitelist of "recognized event
 * tag byte" values, gating cpsoc_poll_reg_reads' forwarding above. Exact
 * ranges from the real disassembly: `{0x30,0x31,0x32}`, `{0x40-0x4f}`,
 * `0x50`, `{0x60-0x6f}`, `{0x80-0x87}`, `0x90`, `0x70`. Several of these
 * ranges (0x31/0x32, 0x41-0x4f, 0x61-0x6f, 0x80-0x87, 0x70) are NOT among
 * the specific tag values cpsoc_tag_router_a/_b/_c actually branch on
 * above - i.e. this whitelist is broader than what the 3 routers
 * recognize, consistent with it being a generic "is this plausibly a real
 * event byte, not read-noise/uninitialized-bus garbage" gate rather than a
 * tag-specific dispatch table. @0xc0010b08. */
uint8_t cpsoc_read_tag_valid(uint8_t tag)	/* FUN_c0010b08 */
{
	if ((uint8_t)(tag - 0x30) < 3)
		return 1;
	if ((tag & 0xf0) == 0x40)
		return 1;
	if (tag == 0x50)
		return 1;
	if ((tag & 0xf0) == 0x60)
		return 1;
	if ((tag & 0xf8) == 0x80)
		return 1;
	if (tag == 0x90)
		return 1;
	return (tag == 0x70);
}

/* ------------------------------------------------------------------------- *
 *  Address-range sweep note - EXTENDED this pass (closure sweep, continuing
 *  the 2026-07-18 pass). Confirmed every function in this file's cited
 *  ranges (0xc0007108-0xc0007220, 0xc0008618, 0xc0010c54-0xc0011938,
 *  0xc0012724-0xc0012840) now has a real, decompiled, COMPILABLE body
 *  except cpsoc_analog_poll_task itself (still genuinely no Ghidra function
 *  boundary at 0xc0011624, as already documented above - the one true gap
 *  left in this file, honestly). This pass additionally resolved 4 items
 *  previously left as bare `FUN_`/pseudo-code-comment placeholders and
 *  found 4 more out-of-scope items by sweeping the immediate address
 *  neighborhood with the query tool:
 *
 *  NEWLY GIVEN REAL BODIES this pass:
 *   - `cpsoc_state_clear` (`FUN_c0012724`) - previously entirely
 *     uncatalogued; resolves eva_board_main.c's own open "plausibly
 *     cpsoc.cpp's own state-table clear" cross-file question.
 *   - `cpsoc_drain_queue_wrapper` (`FUN_c0007150`) - previously "not
 *     itself reconstructed"; also corrects a real cad.c mis-attribution
 *     (see that function's own citation above cpsoc_queue_drain_writes).
 *   - `cpsoc_bargraph_task_variant_a`/`_b` (`FUN_c00114a4`/`FUN_c00116d8`)
 *     - previously structurally described only; both decompile cleanly
 *       with no ambiguous arithmetic, so both are now fully transcribed.
 *   - `panel_gpio_reset_pulse`/`panel_gpio_level_set` (`FUN_c0000ba0`/
 *     `FUN_c0002620`) - previously documented ONLY as a pseudo-code
 *     comment with no compilable `void panel_gpio_reset_pulse(void){...}`
 *     definition anywhere in this file; both now have real bodies.
 *     `panel_gpio_level_set` is confirmed cpsoc-PRIVATE (exactly 2
 *     callers, both inside `panel_gpio_reset_pulse`) - a genuinely new
 *     attribution, not previously stated either way.
 *
 *  CONFIRMED OUT OF SCOPE this pass (checked, not silently skipped):
 *   - `FUN_c0011814` (@0xc0011814, 4 bytes): a bare `mov pc,lr` no-op stub -
 *     this is eva_board_main.c's OWN already-documented
 *     `eva_board_init_table_entry_0` sub-call, physically placed inside
 *     this file's address range but not this file's code.
 *   - `FUN_c0011820` onward (@0xc0011820-~0xc0012198, ~16+ functions): a
 *     generic GPIO pulse-toggle utility cluster (`FUN_c0002450`/
 *     `FUN_c000248c`/`FUN_c0002338`-style pin set/clear/config primitives)
 *     with 16+ callers spread across a much wider address range than this
 *     file - NONE of them reference the cpsoc+0x820/+0x821 scratch
 *     convention, the reg 0x78-0x7b register-bank pattern, or the
 *     `"../cpsoc.cpp"` fault string every genuine cpsoc.cpp function in
 *     this file cites. Confirmed NOT cpsoc's own code; left unattributed.
 *   - `FUN_c0007108`/`FUN_c0007114` (@0xc0007108/0xc0007114, physically
 *     immediately BEFORE cpsoc_i2c_dispatch's own address 0xc0007120):
 *     both call `FUN_c0009194` (`omap_usbdc_reloc` per wire_dispatch.c's
 *     own citation) and are called exclusively from functions in the
 *     0xc0000400-0xc0005400 range (USB endpoint bring-up, per caller
 *     addresses/shapes) - confirmed NOT cpsoc's own code despite the
 *     address proximity, same "false neighbor" pattern as the GPIO cluster
 *     above.
 *   - `FUN_c001120c`/`FUN_c001121c` (@0xc001120c/0xc001121c, sitting
 *     between cpsoc_tag_router_b's own end and clcdc_test_pattern_
 *     dispatch's own start): both are simple one-line global setters whose
 *     ONLY callers (1 each, confirmed via xrefs_to) are inside
 *     `clcdc_test_pattern_dispatch` (`FUN_c001123c`) itself - private
 *     helpers of that clcdc.c-owned function, not cpsoc's own code, despite
 *     physically sitting inside this file's own address range (the same
 *     kind of cross-file interleaving already established for
 *     `clcdc_test_pattern_dispatch` itself, cited as extern above).
 *   - `FUN_c0000be8` (@0xc0000be8, immediately after panel_gpio_reset_
 *     pulse's own address): also calls `FUN_c0009194`/`omap_usbdc_reloc`
 *     and has its one confirmed caller (`FUN_c000bc1c`) well outside this
 *     file's own address neighborhood - confirmed NOT cpsoc's own code,
 *     same USB-subsystem false-neighbor pattern as `FUN_c0007108`/
 *     `FUN_c0007114` above.
 * ------------------------------------------------------------------------- */

/* ========================================================================= *
 *  MEMORY-DUMP / BOOT-IMAGE READBACK INVESTIGATION (2026-07-18, closure pass)
 * ========================================================================= *
 *
 *  Specific question asked: does ANY wire-protocol command reachable from
 *  the host let you read raw firmware memory back - e.g. to pull the boot
 *  splash bitmap (KRONOS_V06R06.VSB.md's own "Boot splash resource"
 *  section - payload offset `0x25235`/runtime address `0xC0025235` per that
 *  doc's own 2026-07-19 offset correction; an EARLIER doc revision had
 *  cited `0x32800`/`0xC0032800`, which that same correction now
 *  identifies as a horizontally-rolled misread of the same image, not a
 *  second copy) off a real, already-flashed unit after the fact?
 *
 *  ANSWER: NO SUCH CAPABILITY WAS FOUND ANYWHERE IN THE TRACED COMMAND
 *  SURFACE. Every host-facing read path this project has reconstructed
 *  reads a FIXED, SMALL, HARDWARE-DEFINED location - never a host-supplied
 *  address, never an arbitrary length, never routed to a generic "copy N
 *  bytes starting at address X out over USB" primitive. Specifically:
 *
 *   1. cpsoc.c's OWN three host-facing read entry points
 *      (cpsoc_read_switch_row/_led_row/_switch_row_clear, reg 0x50/0x51/
 *      0x52) take only a fixed `index` (0..0x48, bounds-checked) selecting
 *      one of 73 known switch/LED table slots - never a raw address, and
 *      (per cpsoc_i2c_dispatch's own full re-decompile above) don't even
 *      touch hardware synchronously; they enqueue into a 4-instance ring
 *      buffer cpsoc_dispatch_tick later drains. No path from a host-
 *      supplied address to a memory dereference exists anywhere in this
 *      chain.
 *   2. `wire_dispatch_command`'s (`wire_dispatch.c`, owned by other work,
 *      read-only here) FULL opcode table was already exhaustively
 *      reconstructed by that file's own pass and is reproduced in
 *      KRONOS_V06R06.VSB.md's "wire-protocol command dispatcher" section.
 *      The three opcodes that doc's own table flags as "not yet
 *      attributed" - `0xd0`/`0xd1`/`0x80` (op-byte-0 family) - were
 *      checked specifically for this investigation: all three are
 *      CALIBRATION-SLOT commands (`cad_calibration_init_slot`, a second
 *      unattributed-but-clearly-calibration-shaped context call, and
 *      `cad_pedal_object_set_mode`), each taking a small `slot`/`mode`
 *      argument in `cmd[1]`, never an address, never returning a payload
 *      larger than the command's own fixed reply shape. No generic
 *      read-N-bytes-from-address handler exists among them.
 *   3. `crypto_at88.c`'s `crypto_at88_self_test` (read-only here) is the
 *      only AT88-adjacent function that both writes AND reads back a
 *      buffer - but it's a fixed 16-byte round-trip self-test against the
 *      AT88 crypto chip's own zone-0 storage (not firmware RAM/flash at
 *      all), takes no host-supplied address, and - per that file's own
 *      confirmed finding - has ZERO static callers anywhere in the image,
 *      i.e. it isn't even reachable from the wire protocol in this build.
 *   4. `panelbus_dispatch.c`'s (read-only here) opcode `0x80`-`0x87`
 *      handler - the one other opcode range flagged as unusual in that
 *      file's own README (a genuine used-return-value anomaly on its
 *      fault-handler call) - is a device-ID NEGOTIATION handshake: it
 *      caches a single small ID byte (`ctx->field_40b`) per-context and
 *      compares it against subsequent calls' argument byte. No address
 *      parameter, no memory dereference beyond that one cached byte.
 *
 *  CONCLUSION: boot-image (or any other arbitrary firmware-memory)
 *  extraction from an already-flashed unit is NOT possible via any USB
 *  wire-protocol command surface traced across this project so far
 *  (cpsoc.c, wire_dispatch.c, crypto_at88.c, panelbus_dispatch.c). The
 *  splash bitmap's own bytes only ever leave the board rendered as
 *  pixels on the physical LCD via clcdc.c, never as a raw byte stream
 *  the host can request. Getting those bytes off a real unit would need
 *  either (a) extracting them from the VSB firmware image file itself
 *  (already done - see KRONOS_V06R06.VSB.md's own "Boot splash resource"
 *  section for the exact, corrected offset) rather than from a running
 *  board, or (b) a genuinely new capability (e.g. a JTAG/serial-console
 *  path, or a host-side firmware-update/verify command not yet
 *  reconstructed) outside every command surface this pass could find -
 *  NOT a gap in this pass's coverage of the KNOWN wire protocol, which is
 *  now exhaustively swept.
 * ========================================================================= */
