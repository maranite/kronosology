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
 */

#include <stdint.h>

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
 *  the init table finishes, does one more setup call (eva_board_final_setup,
 *  role not traced), starts something with argument pair (1, 4) - plausibly
 *  spawning a background task (the watchdog task,
 *  cobjectmgr.c-adjacent FUN_c00090b8, is the leading candidate given the
 *  immediately-following fault-wrapper function below expects it might
 *  "return" - not confirmed which task this actually starts), then enters
 *  the REAL MAIN LOOP: fetch a handle, call the master wire-protocol
 *  dispatcher (already reconstructed as the trigger context for
 *  crypto_at88.c's queue relay, clcdc.c's progress bar, and cad.c/cpsoc.c's
 *  own opcode handlers), forever. This is, as far as this project has
 *  traced, the actual `for(;;)` loop the entire rest of the firmware runs
 *  inside of. @0xc0005644 (same un-bounded raw-disassembly region as
 *  eva_board_init_table above).
 * ------------------------------------------------------------------------- */
extern void eva_board_final_setup(void *handle);		/* FUN_c00074bc */
extern void eva_board_start_task(int priority, int arg);	/* FUN_c001cfd8 - args (1, 4), real meaning not traced */
extern void master_dispatch_tick(void *handle);		/* FUN_c0008b64, already reconstructed via its call sites in crypto_at88.c/clcdc.c/cobjectmgr.c */

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
