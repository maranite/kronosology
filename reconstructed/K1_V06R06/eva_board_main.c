/* SPDX-License-Identifier: GPL-2.0 */
/*
 * eva_board_main.c - the panel board's real entry point: an init-function-
 * pointer-table walker followed by the firmware's actual main loop.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../EvaBoardMain.cpp" has one xref, inside a small fault-wrapper
 * function (eva_board_watchdog_fault_wrapper below) that Ghidra never
 * assigned a containing function boundary to - the code was analyzed
 * directly from raw disassembly at 0xc0005610-0xc0005698, not through the
 * usual decompile_function path (which requires a recognized function).
 *
 * CORRECTION (SPI/USB cleanup + re-verification pass, 2026-07-17): the
 * original version of this file claimed the init table below explained
 * EVERY "zero static callers" function found across the project
 * (crypto_at88_self_test, cdix_configure_and_verify,
 * cobjectmgr_object_destroy). Having now actually read the table's real
 * contents via Ghidra's read_memory (not just its start/end pointers), this
 * is WRONG - the table has exactly ONE entry, and that entry's own code
 * (eva_board_init_table_entry_0 below) doesn't call any of those three
 * functions. The "ties the whole project together" framing overreached.
 * Corrected below; the real invocation mechanism for those three
 * zero-caller functions remains genuinely unresolved.
 *
 * INITIALIZATION-PATH PASS, 2026-07-18: closed out both items this file
 * previously left as "still genuinely open" - eva_board_final_setup
 * (FUN_c00074bc, plus its own compatibility-check helper, FUN_c00073fc) and
 * eva_board_start_task (FUN_c001cfd8, a real priority-scheduler primitive).
 * Also added a new top section tracing the ARM reset vector at 0xC0000000
 * down through the reset handler and crt0 - the actual path that leads here.
 * Worked from static Ghidra dumps only (no live MCP bridge this pass); see
 * each new section's own notes for exactly what's confirmed vs. inferred,
 * and the file-level "still open" list at the bottom for what remains.
 */

#include <stdint.h>

/* ============================================================================
 *  RESET VECTOR / CRT0 CHAIN - what actually runs before eva_board_main
 * ============================================================================
 *
 *  The ARM exception vector table lives at 0xC0000000 (5x `LDR PC,[PC,#0x18]`
 *  -style entries reading their real targets out of a literal pool
 *  immediately after the table). Ghidra decompiles the reset vector's own
 *  entry as an indirect call through its literal-pool slot:
 *
 *      void entry(void)                 // @0xC0000000
 *      {
 *          (*DAT_c0000020)();           // literal pool slot, confirmed
 *          return;                      // value 0xC0009534
 *      }
 *
 *  DAT_c0000020's resolved value is exactly 0xC0009534, matching this
 *  project's own starting assumption - entry's real reset-handler target.
 *
 *  eva_board_reset_handler (FUN_c0009534, @0xc0009534) - RESOLVED via a
 *  live disassembly follow-up query (0xc0009520-0xc0009570), settling the
 *  ARM/Thumb-interworking hypothesis this section previously carried as
 *  its leading theory: WRONG. The real explanation is much more mundane -
 *  a plain literal pool sitting inline in the code stream, misread as
 *  instructions by the linear disassembler because the function it feeds
 *  never returns (nothing marks the fall-through region as data):
 *
 *      c0009534: ldr  r3, [c0009544]     ; load literal @0xc0009544 into r3
 *      c0009538: cpy  sp, r3             ; sp = r3 (the stack-pointer literal)
 *      c000953c: bl   0xc00055b8         ; call eva_board_crt0 - NEVER RETURNS
 *      c0009540..c0009547:               ; 8 bytes of literal-pool DATA (the
 *                                        ; ldr's target word lives at c0009544;
 *                                        ; c0009540-3 is a second pool slot),
 *                                        ; unreachable as code but walked by
 *                                        ; linear disassembly anyway - this is
 *                                        ; exactly what produced the earlier
 *                                        ; "FUN_c0009540 calls itself" and the
 *                                        ; "pcode error at c0009544" artifacts
 *      c0009548: ldrb r0, [r0, #0x7]     ; -- real code resumes here, but this
 *      c000954c: mov  pc, lr             ;    is an unrelated 2-instruction
 *                                        ;    accessor, NOT a continuation of
 *                                        ;    the reset handler
 *
 *  So `eva_board_reset_handler` is genuinely just 3 ARM instructions (12
 *  bytes, 0xc0009534-0xc000953f): load the initial stack pointer from its
 *  own literal pool, install it, and tail-call into `eva_board_crt0`
 *  (confirmed below to never return - it falls straight into the
 *  scheduler's idle/dispatch loop). Ghidra's function boundary at
 *  "FUN_c0009540" is spurious - it doesn't correspond to any real function;
 *  the address is just where the unreachable literal pool happens to start.
 *  No Thumb code, no interworking boundary, nothing exotic - purely a
 *  standard "linear disassembly ran off the end of a noreturn function into
 *  its own literal pool" artifact, common in flat/symbol-less ARM binaries.
 *
 *  eva_board_crt0 (FUN_c00055b8, @0xc00055b8) - the ONE confirmed call target
 *  out of the corrupted span above, and cleanly decompiled in its own right
 *  (no warnings). A classic embedded crt0: zero a BSS-style region
 *  (DAT_c0005600..DAT_c0005604, a start/end pointer pair - same "table
 *  walked start-to-end" idiom as eva_board_init_table below), then eleven
 *  back-to-back calls into what are almost certainly per-subsystem/hardware
 *  bring-up and the task/scheduler-table constructors (FUN_c0005530,
 *  FUN_c0009188, FUN_c0000a20, FUN_c00018e4, FUN_c0001c84, FUN_c001cc2c,
 *  FUN_c001cba8, FUN_c001cba4, FUN_c001ce40, FUN_c001cad4, FUN_c001ca34 -
 *  none individually traced this pass, all address-adjacent to
 *  eva_board_start_task's own scheduler primitives below rather than to any
 *  already-reconstructed peripheral driver). One of them, FUN_c001cad4, is
 *  itself confirmed (via its own xref) to call FUN_c001d0f8, a small loop
 *  that populates a table of task-control-block entries (each entry copying
 *  an 8-byte source record into a 0x10-byte destination stride and
 *  self-initializing two list-link words) - i.e. this IS the task-table
 *  constructor eva_board_start_task's own TCB table depends on being
 *  populated by the time anything calls it.
 *
 *  After the eleven-call sequence, eva_board_crt0 sets a global flag
 *  (`*DAT_c001cb28 = 1`) and falls DIRECTLY into what is, byte-for-byte, the
 *  same scheduler idle/dispatch tail already independently confirmed inside
 *  eva_board_start_task's own callee FUN_c001d850 below (poll a "next ready
 *  task" global, `coproc_moveto_Wait_for_interrupt(1)` - the ARM `WFI`
 *  intrinsic - when none is ready, and an indirect call through the ready
 *  task's own offset+0x1c function-pointer field once one is). In other
 *  words: eva_board_crt0 is not just "runtime init", it's runtime init THEN
 *  the scheduler's own entry into its idle/dispatch loop, unconditionally,
 *  with no return.
 *
 *  STILL OPEN, not fabricated: whether eva_board_main itself is one of the
 *  tasks made ready somewhere inside that eleven-call init sequence (the
 *  natural hypothesis - this crt0 chain never returns, and eva_board_main's
 *  own for(;;) loop is clearly meant to be the "real" main loop - but no
 *  task-table entry pointing at 0xc0005644 was found in this pass; the task
 *  table's actual contents, as opposed to its construction code, weren't
 *  read via any tool this pass has access to). Left as the strongest
 *  remaining lead connecting this section to the rest of the file, not
 *  asserted as confirmed.
 * ============================================================================
 */

/* ------------------------------------------------------------------------- *
 *  eva_board_init_table - walks a table of function pointers from a start
 *  address to an end address (DAT_c0005664/DAT_c0005668), calling each
 *  non-NULL entry in order via a manual ARM call sequence (`mov lr,pc; cpy
 *  pc,r3`) and skipping NULL entries. @0xc0005610 (no Ghidra-recognized
 *  function boundary; traced directly from raw disassembly).
 *
 *  RESOLVED (SPI/USB cleanup pass, 2026-07-17): the table's actual contents
 *  were read directly via Ghidra's read_memory. `eva_board_init_table_start`
 *  (0xc0005664) holds `0xc0098f54`; `eva_board_init_table_end` (0xc0005668)
 *  holds `0xc0098f58` - a 4-byte difference, meaning the table has exactly
 *  **ONE entry**, not a multi-function driver-probe table as originally
 *  hypothesized. That one entry (at 0xc0098f54) holds `0xc0009168` -
 *  eva_board_init_table_entry_0 below. crypto_at88_self_test,
 *  cdix_configure_and_verify, and cobjectmgr_object_destroy are NOT called
 *  through this table - their real invocation mechanism is still genuinely
 *  unresolved. Left as a real, if smaller-than-hoped, structural finding:
 *  the "init table" walker is genuine ARM code doing genuine indirect
 *  dispatch, it's just dispatching to a single lazy-init singleton, not a
 *  driver-probe table.
 * ------------------------------------------------------------------------- */
extern void (*eva_board_init_table_start[])(void);	/* DAT_c0005664, real value 0xc0098f54 */
extern void (*eva_board_init_table_end[])(void);	/* DAT_c0005668, real value 0xc0098f58 */

void eva_board_init_table(void)
{
	void (**entry)(void) = eva_board_init_table_start;

	while (entry < eva_board_init_table_end) {
		void (*fn)(void) = *entry++;
		if (fn)
			fn();
	}
}

/* ------------------------------------------------------------------------- *
 *  eva_board_init_table_entry_0 - the table's one real entry (@0xc0009168).
 *  A lazy-init singleton: compares two globals (an init-done flag pattern),
 *  and on first call runs a 2-step setup - eva_board_init_table_entry_0_zero
 *  (@0xc0005720, just zeroes one byte at a fixed pointer) and
 *  eva_board_init_table_entry_0_stub (@0xc0011814, confirmed via raw
 *  disassembly to be a genuine 4-byte no-op: `mov pc,lr` and nothing else -
 *  not a decompile failure, the real function body IS empty) - then always
 *  returns a fixed handle (DAT_c0009164) whether or not the init branch ran.
 *  Has zero static callers of its own (only reached through the init table),
 *  consistent with being a real lazy singleton getter.
 *
 *  Address-neighborhood observation, NOT independently confirmed: this
 *  function and its return handle sit in the same low-0xc0009xxx address
 *  range as omap_l137_usbdc.c's omap_usbdc_reloc (0xc0009194) and the USB
 *  object bring-up function that calls omap_usbdc_init_ep0 (0xc0009574) -
 *  plausibly this is itself a "get or create the USB device singleton"
 *  accessor for that same subsystem, given the no-op second step is
 *  consistent with a hook left compiled-out for this particular firmware
 *  build. Not verified either way - flagged as a lead, not a finding.
 * ------------------------------------------------------------------------- */
extern void eva_board_init_table_entry_0_zero(uint8_t *flag);	/* FUN_c0005720 */
extern void eva_board_init_table_entry_0_stub(void);		/* FUN_c0011814, genuinely a no-op */

void *eva_board_init_table_entry_0(void)	/* FUN_c0009168 */
{
	extern int eva_board_singleton_flag_a, eva_board_singleton_flag_b;	/* DAT_c0009174, DAT_c000915c */
	extern uint8_t *eva_board_singleton_zero_target;			/* DAT_c0009160 */
	extern void *eva_board_singleton_handle;				/* DAT_c0009164 */

	if (eva_board_singleton_flag_a == eva_board_singleton_flag_b) {
		eva_board_init_table_entry_0_zero(eva_board_singleton_zero_target);
		eva_board_init_table_entry_0_stub();
	}
	return eva_board_singleton_handle;
}

/* ------------------------------------------------------------------------- *
 *  eva_board_main - real firmware entry point (the function containing
 *  eva_board_init_table's own code, immediately followed by this): after
 *  the init table finishes, does one more setup call - eva_board_final_setup,
 *  now fully reconstructed below (per-subsystem hardware bring-up plus a
 *  board-compatibility self-test gate) - then eva_board_start_task(1, 4),
 *  also now fully reconstructed below: a real priority-scheduler primitive
 *  that marks task/slot 1 ready at priority level 3 (the `4` argument minus
 *  one) and invokes the scheduler if that task wasn't already running. NOT
 *  confirmed which physical task this is (the watchdog task, cobjectmgr.c's
 *  FUN_c00090b8, remains the leading candidate - see that function's own
 *  "should never return" structure and this file's watchdog-fault-wrapper
 *  below - but eva_board_start_task's own table lookup only proves it acts
 *  on a pre-existing task-table slot, not which one). Then enters the REAL
 *  MAIN LOOP: fetch a handle, call the master wire-protocol dispatcher (now
 *  fully reconstructed in wire_dispatch.c, this pass - previously only
 *  known via its call sites in crypto_at88.c/clcdc.c/cobjectmgr.c), forever.
 *  This is, as far as this project has traced, the actual `for(;;)` loop the
 *  entire rest of the firmware runs inside of. @0xc0005644 (same
 *  un-bounded raw-disassembly region as eva_board_init_table above).
 * ------------------------------------------------------------------------- */
extern void eva_board_final_setup(void *handle);		/* FUN_c00074bc - full body reconstructed below */
extern uint32_t eva_board_start_task(int task_id, unsigned priority_code);	/* FUN_c001cfd8 - args (1, 4); full body reconstructed below */
extern void master_dispatch_tick(void *handle);		/* FUN_c0008b64 - reconstructed in wire_dispatch.c, this pass */

void eva_board_main(void)
{
	extern void *eva_board_handle;		/* DAT_c000566c, real fixed address */

	eva_board_init_table();

	eva_board_final_setup(eva_board_handle);
	eva_board_start_task(1, 4);

	for (;;)
		master_dispatch_tick(eva_board_handle);
}

/* ------------------------------------------------------------------------- *
 *  eva_board_watchdog_fault_wrapper - the confirmed anchor for
 *  "../EvaBoardMain.cpp" itself. Calls the hardware-fault watchdog task
 *  (cobjectmgr.c's own FUN_c00090b8 finding - an infinite loop that only
 *  exits by hard-faulting internally), and IF that call ever actually
 *  returns (which, per FUN_c00090b8's own confirmed structure, should never
 *  happen under normal operation - its only exit path is its own internal
 *  assert), unconditionally builds and raises a SEPARATE hard fault right
 *  here, with a fixed line number (0x6d = 109). This is defensive code: a
 *  belt-and-suspenders fault if the watchdog task itself somehow falls
 *  through its own supposedly-infinite loop. @0xc0005670 (no Ghidra function
 *  boundary; the one real xref to the "../EvaBoardMain.cpp" string sits
 *  inside this function, at the fault call site itself).
 * ------------------------------------------------------------------------- */
extern void cobjectmgr_hardware_fault_watchdog(void *handle);	/* FUN_c00090b8, see cobjectmgr.c's own note on this function */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* CORRECTION (re-verification pass, 2026-07-17): originally documented as
 * taking and forwarding a `handle` parameter. Re-verified against fresh
 * disassembly: this function takes NO effective parameter. r0 is loaded
 * from a fixed literal (DAT_c0005698) immediately before the call to
 * cobjectmgr_hardware_fault_watchdog, not from an incoming argument
 * register - the same "phantom forwarded parameter" pattern independently
 * found in cdix4192.c's cdix_reg_write/cdix_reg_read during this same
 * verification pass. DAT_c0005698 happens to hold the same runtime value
 * as eva_board_handle (below), which is why the earlier draft's assumption
 * read as plausible without being correct. */
void eva_board_watchdog_fault_wrapper(void)
{
	cobjectmgr_hardware_fault_watchdog(0 /* DAT_c0005698, a fixed literal - see correction note above */);
	/* per FUN_c00090b8's own structure, this line should be unreachable
	 * under normal operation - reaching it at all means the watchdog task
	 * fell through its own infinite loop, which is itself the actual
	 * fault being reported here. */
	crypto_at88_fault(0, 0 /* DAT_c000569c */, 0x6d);
}

/* ============================================================================
 *  eva_board_final_setup - FULLY RECONSTRUCTED, 2026-07-18 pass
 * ============================================================================
 *
 *  eva_board_compat_check (FUN_c00073fc, @0xc00073fc) - eva_board_final_setup's
 *  own first callee, called with the board handle directly (not a per-
 *  subsystem DAT_ constant like every other callee below), and its ONLY
 *  caller is eva_board_final_setup - genuinely local to this compilation
 *  unit, not a generic peripheral driver. Structurally, this is a hardware
 *  compatibility/self-test GATE with the exact same "draw an error, hang
 *  forever" shape already confirmed for crypto_at88.c's FUN_c000919c - a
 *  second, independent instance of this firmware's fail-fast halt idiom:
 *
 *   - eva_board_hw_id_check (FUN_c0009218) reads a byte at a fixed offset
 *     (+0x1ac00) off the same USB/peripheral relocation base used throughout
 *     omap_l137_usbdc.c (FUN_c0009194 - "omap_usbdc_reloc" in that file),
 *     compares it against two constants (0x17, 0x27) and returns a
 *     pass/fail-shaped result. Not confirmed what physical signal this is
 *     (board revision strap, silicon ID register, and similar are all
 *     plausible) - flagged as a lead, not asserted.
 *   - If that check reports "unrecognized", falls back to a probe loop over
 *     device/register bank numbers 0x78-0x7b (bus-transaction primitive
 *     FUN_c00033f0, which itself busy-waits on a status bit with up to 1000
 *     retries via omap_l108.c's own cad_delay_ticks - real evidence this is
 *     a genuine bus transaction, not a stub) - the SAME four bank numbers
 *     eva_board_final_setup itself initializes via cpsoc_i2c_dispatch below,
 *     and the same four cad.c/cpsoc.c already register as their own opcode
 *     handlers elsewhere in this project.
 *   - Success (either the id check passed, or all four probes answered)
 *     returns normally, letting eva_board_final_setup continue.
 *   - Failure falls into an error screen (two possible messages, chosen by
 *     eva_board_probe_summary, FUN_c001267c - itself re-probing the same
 *     four bank numbers 0x78/0x79/0x7b/0x7a in sequence to decide which
 *     message to show) followed by a genuine empty `do {} while(true);` -
 *     an unrecoverable hang, structurally identical in intent to
 *     crypto_at88.c's assert handler, just a separate call site.
 *
 *  The dense bus-transaction arithmetic inside FUN_c00033f0/FUN_c001267c/
 *  their own callee FUN_c0012560 is NOT transcribed here - cited and
 *  structurally described only, same treatment this project already gives
 *  clcdc_blit_glyph's bit-shift math, to avoid presenting guessed opcodes as
 *  fact in code this fiddly with no way to verify against real hardware.
 * ------------------------------------------------------------------------- */
extern int eva_board_hw_id_check(void *reloc_handle);			/* FUN_c0009218, DAT_c00074a4 */
extern void *eva_board_probe_bus_handle(void *handle, int unused_zero);	/* FUN_c0001a00, DAT_c00074a8 - real handle for the probe below */
extern int eva_board_bus_probe(void *bus, int bank, void *scratch, int len);	/* FUN_c00033f0 - I2C/SPI-style transaction: busy-waits on a status bit, up to 1000 retries via cad_delay_ticks(1) */
extern void draw_text(int x, int y, const char *str, int unknown_arg4);	/* FUN_c0015650, see crypto_at88.c */
extern int eva_board_probe_summary(void *handle);			/* FUN_c001267c - re-probes banks 0x78/0x79/0x7b/0x7a via FUN_c0012560, not itself transcribed */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

void eva_board_compat_check(void *handle)	/* FUN_c00073fc */
{
	extern void *eva_board_reloc_handle;		/* DAT_c00074a4 */
	extern void *eva_board_probe_bus_source;	/* DAT_c00074a8 */
	extern const char *eva_board_compat_title;	/* DAT_c00074ac */
	extern void *eva_board_probe_summary_arg;	/* DAT_c00074b0 */
	extern const char *eva_board_compat_msg_fail;	/* DAT_c00074b4 */
	extern const char *eva_board_compat_msg_partial; /* DAT_c00074b8 */
	int bank;

	if (eva_board_hw_id_check(eva_board_reloc_handle) == 0) {
		for (bank = 0x78; ; bank++) {
			void *probe_handle = eva_board_probe_bus_handle(eva_board_probe_bus_source, 0);
			uint8_t scratch[4];
			if (!eva_board_bus_probe(probe_handle, bank, scratch, 2))
				break;			/* probe failed -> fall into the error/halt path below */
			if (bank > 0x7b)
				return;			/* all four banks (0x78..0x7b) answered -> compatible, return normally */
		}
	}

	/* unrecognized board id, or a failed bus probe: unrecoverable error screen */
	draw_text(0x14, 0x46, eva_board_compat_title, 0);
	if (eva_board_probe_summary(eva_board_probe_summary_arg) == 0)
		draw_text(0x14, 0x50, eva_board_compat_msg_partial, 0);
	else
		draw_text(0x14, 0x50, eva_board_compat_msg_fail, 0);

	for (;;)
		;	/* confirmed genuine infinite hang, same idiom as crypto_at88.c's FUN_c000919c */
}

/* ------------------------------------------------------------------------- *
 *  eva_board_final_setup (FUN_c00074bc, @0xc00074bc) - the confirmed real
 *  body behind the previously-open "role not traced" note. A per-subsystem
 *  hardware bring-up sequencer: sets three fixed byte fields on the board
 *  handle (+1/+2/+3 = 0x18/8/0x10, purpose not decoded), runs the
 *  compatibility gate above, then makes TWELVE further calls, each passing
 *  a fixed per-subsystem context pointer read out of this function's own
 *  data segment (DAT_c00075fc..DAT_c0007630) - i.e. this is genuinely a
 *  "bring up every peripheral driver with its own handle" dispatcher, one
 *  level above each individual subsystem's own init entry point.
 *
 *  Three of the twelve are cross-file CONFIRMED matches by address:
 *   - FUN_c00136c0 = cad.c's own cad_init(cad_handle)
 *   - FUN_c0013394 = cad.c's own cad_pedal_object_init(pedal_handle)
 *   - FUN_c0009574 = omap_l137_usbdc.c's own higher-level USB object
 *     bring-up function (its second argument is a literal 0 here, not a
 *     DAT_ constant - the only one of the twelve called that way)
 *
 *  Three more have suggestive-but-NOT-independently-confirmed evidence
 *  (recorded as leads, not renamed - these addresses are outside this
 *  file's own scope to attribute, and each already-anchored subsystem file
 *  is owned by other work in this pass):
 *   - FUN_c0014f84 writes a literal 800 into its handle's own +4 field -
 *     the LCD's known 800-pixel width (KronosFB's own /dev/fb1 documentation
 *     agrees) - plausibly clcdc.cpp's own top-level constructor, sitting
 *     immediately below ctouchpanel.c's own confirmed 0xc0014010-0xc0014f84
 *     range and immediately above clcdc.c's own confirmed range starting
 *     near 0xc0015010.
 *   - FUN_c0012724 zeroes exactly 0x49 (73) bytes - the same 73-entry
 *     switch/LED table size cpsoc.c's own read functions already bounds-
 *     check against - plausibly cpsoc.cpp's own state-table clear.
 *   - FUN_c000ecc4/FUN_c000cdc8 both call FUN_c0009194 (omap_usbdc_reloc,
 *     per omap_l137_usbdc.c) with register-block offsets (+0x400, +0x6240,
 *     +0xa240) distinct from omap_usbdc_init_ep0's own (+0x2800/+0x2a00/
 *     +0x2c00) - plausibly a second, separate USB register-block setup
 *     this project hasn't traced, called from FUN_c000bc1c/FUN_c000cdc8's
 *     own small wrapper pair rather than from FUN_c0009574 directly.
 *
 *  The remaining seven (FUN_c0010ad4, FUN_c0012d28, FUN_c001485c,
 *  FUN_c0014ee8, FUN_c0000ec8, FUN_c000fc48, FUN_c000bc1c) are generic
 *  struct-clear/small-setup routines with no confirmed subsystem attribution
 *  - left as bare FUN_ externs rather than guessed at.
 *
 *  After the twelve calls, does board-specific finishing touches: sets a
 *  mode field (+0x3c = 2), a sentinel byte (+0x40 = 0xff), clears two 4-byte
 *  fields (+0x28, +0x24), registers four cpsoc register banks - 0x78, 0x79,
 *  0x7b, 0x7a, i.e. the SAME four banks eva_board_compat_check's own probe
 *  loop cycles through above - each to value 0xb0 via cpsoc_i2c_dispatch
 *  (cpsoc.c's own FUN_c0007120), reads one byte from a fixed hardware/MMIO
 *  address (resolved literal 0xC0E00058 - outside the SDRAM code/data
 *  range, consistent with a real peripheral register, physical meaning not
 *  decoded) into the handle's own +0x2c field, sets a "final setup done"
 *  flag at a fixed address sitting inside the SAME static-data block as
 *  eva_board_init_table_entry_0's own singleton state above (DAT_c0007638,
 *  resolved 0xC0098F8C - 0x38 bytes past that entry's own handle slot,
 *  0xC0098F54), and finally marks the handle itself initialized (+0x0 = 1).
 * ------------------------------------------------------------------------- */
extern void *eva_board_lcd_ctx;		/* DAT_c00075fc - plausibly clcdc's own context, see note above */
extern void *eva_board_ctx_c0007600;		/* DAT_c0007600 */
extern void *eva_board_cpsoc_state_ctx;	/* DAT_c0007604 - plausibly cpsoc's own 73-entry table, see note above (FUN_c0012724 zeroes 0x49 bytes) */
extern void *eva_board_ctx_c0007608;		/* DAT_c0007608 */
extern void *cad_pedal_handle;			/* DAT_c000760c - confirmed: passed to cad_pedal_object_init below */
extern void *cad_handle;			/* DAT_c0007610 - confirmed: passed to cad_init below */
extern void *eva_board_ctx_c0007614;		/* DAT_c0007614 */
extern void *eva_board_ctx_c0007618;		/* DAT_c0007618 */
extern void *eva_board_ctx_c000761c;		/* DAT_c000761c */
extern void *eva_board_usb_ctx_a;		/* DAT_c0007620 - passed to FUN_c000ecc4, see note above */
extern void *eva_board_usb_ctx_shared;		/* DAT_c0007624 - reused as the argument to both FUN_c000bc1c and FUN_c000cdc8 below */
extern void *eva_board_ctx_c0007628;		/* DAT_c0007628 */
extern void *eva_board_usb_dev_handle;		/* DAT_c000762c - passed to omap_l137_usbdc.c's own USB bring-up (FUN_c0009574) */
extern void *eva_board_usb_ctx_b;		/* DAT_c0007630 - passed to FUN_c000cdc8, see note above */
extern uint8_t *eva_board_mmio_strap_byte;	/* DAT_c0007634, resolved value 0xC0E00058 - a real peripheral/MMIO address, physical meaning not decoded */
extern uint8_t eva_board_final_setup_done_flag;	/* DAT_c0007638, resolved value 0xC0098F8C - same static-data block as eva_board_init_table_entry_0's own singleton state */

extern void eva_board_ctx_c00075fc_init(void *ctx);	/* FUN_c0014f84 */
extern void eva_board_compat_check(void *handle);	/* FUN_c00073fc, defined above */
extern void eva_board_ctx_c0007600_init(void *ctx);	/* FUN_c0010ad4 */
extern void eva_board_ctx_c0007604_init(void *ctx);	/* FUN_c0012724 - zeroes 0x49 (73) bytes, see note above */
extern void eva_board_ctx_c0007608_init(void *ctx);	/* FUN_c0012d28 */
extern void cad_pedal_object_init(void *obj);		/* FUN_c0013394, cad.c - CONFIRMED, arg is DAT_c000760c */
extern void cad_init(void *cad);			/* FUN_c00136c0, cad.c - CONFIRMED, arg is DAT_c0007610 */
extern void eva_board_ctx_c0007614_init(void *ctx);	/* FUN_c001485c */
extern void eva_board_ctx_c0007618_init(void *ctx);	/* FUN_c0014ee8 */
extern void eva_board_ctx_c000761c_init(void *ctx);	/* FUN_c0000ec8 */
extern void eva_board_usb_ctx_a_init(void *ctx);	/* FUN_c000ecc4, see note above, arg is DAT_c0007620 */
extern void eva_board_ctx_c0007628_init(void *ctx);	/* FUN_c000fc48 */
extern void omap_usbdc_object_init(void *dev, int unused_zero);	/* FUN_c0009574, omap_l137_usbdc.c - CONFIRMED */
extern void eva_board_usb_ctx_shared_setup(void *ctx, int flag);	/* FUN_c000bc1c */
extern void eva_board_usb_ctx_b_init(void *ctx, void *shared);	/* FUN_c000cdc8, see note above */
extern void cpsoc_i2c_dispatch(void *handle, uint8_t reg, uint32_t out_value, uint8_t raw_bit);	/* FUN_c0007120, cpsoc.c - CONFIRMED */

void eva_board_final_setup(void *handle)	/* FUN_c00074bc */
{
	uint8_t *h = (uint8_t *)handle;

	h[1] = 0x18;
	h[2] = 8;
	h[3] = 0x10;

	eva_board_ctx_c00075fc_init(eva_board_lcd_ctx);
	eva_board_compat_check(handle);
	eva_board_ctx_c0007600_init(eva_board_ctx_c0007600);
	eva_board_ctx_c0007604_init(eva_board_cpsoc_state_ctx);
	eva_board_ctx_c0007608_init(eva_board_ctx_c0007608);
	cad_pedal_object_init(cad_pedal_handle);
	cad_init(cad_handle);
	eva_board_ctx_c0007614_init(eva_board_ctx_c0007614);
	eva_board_ctx_c0007618_init(eva_board_ctx_c0007618);
	eva_board_ctx_c000761c_init(eva_board_ctx_c000761c);
	eva_board_usb_ctx_a_init(eva_board_usb_ctx_a);

	{
		void *shared = eva_board_usb_ctx_shared;	/* uVar2 in the raw decompile */
		eva_board_ctx_c0007628_init(eva_board_ctx_c0007628);
		omap_usbdc_object_init(eva_board_usb_dev_handle, 0);
		eva_board_usb_ctx_shared_setup(shared, 0);
		eva_board_usb_ctx_b_init(eva_board_usb_ctx_b, shared);
	}

	*(uint32_t *)(h + 0x3c) = 2;
	h[0x40] = 0xff;
	*(uint32_t *)(h + 0x28) = 0;
	*(uint32_t *)(h + 0x24) = 0;

	cpsoc_i2c_dispatch(handle, 0x78, 0xb0, 0);
	cpsoc_i2c_dispatch(handle, 0x79, 0xb0, 0);
	cpsoc_i2c_dispatch(handle, 0x7b, 0xb0, 0);
	cpsoc_i2c_dispatch(handle, 0x7a, 0xb0, 0);

	h[0x2c] = *eva_board_mmio_strap_byte;
	h[0x2d] = 0;
	eva_board_final_setup_done_flag = 1;
	*h = 1;
}

/* ============================================================================
 *  eva_board_start_task - FULLY RECONSTRUCTED, 2026-07-18 pass
 * ============================================================================
 *
 *  FUN_c001cfd8 (@0xc001cfd8) is a real primitive of this firmware's own
 *  small priority-based task scheduler, NOT a generic "spawn a task with a
 *  function pointer" call - it operates on a PRE-EXISTING task-control-block
 *  table (the same table eva_board_crt0's own FUN_c001cad4/FUN_c001d0f8
 *  populate at boot, see the reset-vector section at the top of this file),
 *  looking a task up by index and either marking it ready or updating its
 *  priority. Evidence for this reading, all from its own three callees:
 *
 *   - eva_board_sched_ready (FUN_c001da64) manipulates a priority-indexed
 *     ready bitmap (`DAT_c001db4c[priority*8]`) and a matching doubly-linked
 *     ready-queue, then compares the newly-readied task against the
 *     currently-running one to decide whether a reschedule is needed -
 *     textbook O(1) priority-bitmap scheduler "make ready" logic.
 *   - eva_board_sched_requeue (FUN_c001ddac) inserts a node into a list
 *     sorted by a one-byte priority field (offset+0xd - the SAME field
 *     eva_board_start_task itself writes below) - a wait-queue reinsertion
 *     on priority change.
 *   - eva_board_sched_dispatch (FUN_c001d850) is the scheduler's own
 *     idle/dispatch loop: poll a "next ready task" global, execute
 *     `coproc_moveto_Wait_for_interrupt(1)` (ARM `WFI`) when none is ready,
 *     otherwise an indirect call through the ready task's own offset+0x1c
 *     function pointer - BYTE-FOR-BYTE the same tail already found inside
 *     eva_board_crt0 above. Two independent call sites converging on the
 *     identical idle-loop shape is strong, not coincidental, evidence this
 *     is the one real scheduler dispatch primitive in the firmware.
 *
 *  None of these three are transcribed as C here (their own dense
 *  bitmap/pointer arithmetic isn't independently confirmed against any
 *  external scheduler reference and isn't this file's own scope) - cited
 *  and described structurally, consistent with this project's treatment of
 *  other dense, unverifiable arithmetic elsewhere (e.g. clcdc_blit_glyph).
 *
 *  eva_board_start_task's own logic, fully transcribed below:
 *   1. Guard: if the scheduler-not-ready flag is set, fail immediately
 *      (error -25 / 0xffffffe7).
 *   2. Bounds-check task_id against a max-index global; task_id 0 is always
 *      valid (a fixed "default" slot); priority_code must be <= 0x10 (16)
 *      in every case.
 *   3. Look up the task's control block: task_id 0 uses a fixed default
 *      TCB pointer; any other id indexes a table with a 0x20-byte (32-byte)
 *      stride - i.e. task_id really is a small integer task index, not a
 *      priority or opcode.
 *   4. Compute the target priority byte: priority_code 0 pulls a DEFAULT
 *      priority out of the TCB's own linked object; any other value is
 *      used as (priority_code - 1) - a 1-based-to-0-based priority-level
 *      conversion, so eva_board_main's own call, (1, 4), really does mean
 *      "task index 1, priority level 3".
 *   5. Read the TCB's control/state byte. Zero means the slot was never
 *      allocated (error -41 / 0xffffffd7). Bit 0 clear means the task is
 *      already active: just update its stored priority field, and if bit
 *      0x20 is also set (task is currently on some wait list), reinsert it
 *      into that list at the new priority via eva_board_sched_requeue. Bit
 *      0 set means the task hasn't been made ready yet: call
 *      eva_board_sched_ready to enqueue it at the computed priority, and if
 *      that call reports the ready task is now more urgent than whatever's
 *      currently running, invoke eva_board_sched_dispatch to act on it
 *      immediately rather than waiting for the next natural reschedule
 *      point.
 * ------------------------------------------------------------------------- */
extern uint8_t eva_board_sched_not_ready_flag;	/* DAT_c001d0e8 */
extern int eva_board_sched_max_task_id;	/* DAT_c001d0ec */
extern void *eva_board_sched_default_tcb;	/* DAT_c001d0f0 */
extern uint8_t *eva_board_sched_tcb_table;	/* DAT_c001d0f4, 0x20-byte stride per task */
extern void *eva_board_sched_wait_list_head;	/* the (iVar2+0x14)->+4 field the raw decompile derefs for eva_board_sched_requeue's list argument */
extern uint32_t eva_board_sched_ready(void *tcb, unsigned priority);		/* FUN_c001da64 - not transcribed, see note above */
extern void eva_board_sched_requeue(void *list_head, void *tcb);		/* FUN_c001ddac - not transcribed, see note above */
extern void eva_board_sched_dispatch(void);					/* FUN_c001d850 - not transcribed, see note above; real arg is ignored, see below */

uint32_t eva_board_start_task(int task_id, unsigned priority_code)	/* FUN_c001cfd8 */
{
	uint8_t *tcb;
	uint8_t new_priority;
	uint8_t state;

	if (eva_board_sched_not_ready_flag != 0)
		return 0xffffffe7;	/* -25: scheduler not initialized yet */

	if (task_id < 1 || task_id > eva_board_sched_max_task_id) {
		if (task_id != 0)
			return 0xffffffee;	/* -18: bad task id */
		if (priority_code > 0x10)
			return 0xffffffef;	/* -17: bad priority_code */
	} else if (priority_code > 0x10) {
		return 0xffffffef;
	}

	tcb = (task_id == 0)
		? (uint8_t *)eva_board_sched_default_tcb
		: eva_board_sched_tcb_table + task_id * 0x20;

	if (priority_code == 0)
		new_priority = *(uint8_t *)(*(uint32_t *)(tcb + 8) + 0xc);	/* default priority from the TCB's linked object */
	else
		new_priority = (uint8_t)(priority_code - 1);

	state = tcb[0xc];
	if (state == 0)
		return 0xffffffd7;	/* -41: task slot not allocated */

	if ((state & 1) == 0) {
		/* already active: update stored priority, requeue if waiting */
		tcb[0xd] = new_priority;
		if ((state & 0x20) != 0)
			eva_board_sched_requeue(eva_board_sched_wait_list_head, tcb);
	} else {
		/* not yet started: make ready, dispatch immediately if now-most-urgent */
		if (eva_board_sched_ready(tcb, new_priority) != 0)
			eva_board_sched_dispatch();
	}

	return state & 1;
}

/* -------------------------------------------------------------------------
 * Still genuinely open in this file, 2026-07-18 pass:
 *  - Whether eva_board_main is itself a task made ready during
 *    eva_board_crt0's own eleven-call init sequence, or reached some other
 *    way - the strongest remaining lead tying the reset-vector section at
 *    the top of this file to the rest of it, not confirmed.
 *  - (RESOLVED, follow-up live query 2026-07-18) The FUN_c0009534/
 *    FUN_c0009540 anomaly was NOT ARM/Thumb interworking - it's a plain
 *    inline literal pool after a noreturn tail-call, misread as code by
 *    linear disassembly. See the corrected writeup in that section above.
 *  - Seven of eva_board_final_setup's twelve subsystem-init callees have no
 *    confirmed cross-file attribution (FUN_c0010ad4, FUN_c0012d28,
 *    FUN_c001485c, FUN_c0014ee8, FUN_c0000ec8, FUN_c000fc48, FUN_c000bc1c) -
 *    left as bare externs, not guessed at.
 *  - eva_board_compat_check's own eva_board_hw_id_check (FUN_c0009218) -
 *    what the 0x17/0x27 constants it compares against actually represent
 *    (board revision? silicon id?) is not decoded.
 *  - eva_board_start_task's own eva_board_sched_ready/_requeue/_dispatch
 *    (FUN_c001da64/FUN_c001ddac/FUN_c001d850) are structurally confirmed
 *    but not transcribed - the firmware's task-control-block struct layout
 *    (0x20-byte stride, known fields at +0x8/+0xc/+0xd/+0x14 so far) is
 *    real but incomplete.
 * ------------------------------------------------------------------------- */
