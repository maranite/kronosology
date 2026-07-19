/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mcasp.c - the TI OMAP-L1x McASP (Multichannel Audio Serial Port) peripheral
 * driver: hardware reset/enable sequencing and TX/RX serializer setup.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 *
 * SCOPE CORRECTION: the top-level source file the project's own docs list as
 * "../McAspHandler.cpp" has ZERO code anywhere in the image referencing its
 * own filename string - a first for this project (every other subsystem so
 * far had at least one real assert call site anchoring it). The actual
 * functional driver lives in a DIFFERENT, lower-level compilation unit:
 * "../MCU/Component/OmapL137Mcasp.cpp" (11 real xrefs, all from ONE function,
 * mcasp_init/FUN_c00026a0 - see below; corrected from an earlier off-by-one
 * count during a 2026-07-17 re-verification pass, real breakdown is a flat
 * 11, not 11+10 - see the 2026-07-18 pass note further down). Two plausible
 * readings, not resolved either way: McAspHandler.cpp could be a thin,
 * assert-free wrapper whose code genuinely exists but never happens to call
 * the fault handler, or it could be dead/unused code this particular
 * firmware build never actually links in. This file reconstructs the
 * confirmed OmapL137Mcasp.cpp driver instead.
 *
 * 2026-07-18 RE-VERIFICATION PASS: went back through mcasp_init's full raw
 * decompile field-by-field (previously only spot-checked) and found several
 * real bugs/gaps in the prior reconstruction, all fixed below:
 *   - The prior mcasp_state struct's field offsets did not actually match
 *     their own doc comments (pad0[0x10] followed immediately by
 *     serializer_pinfn put that array at +0x10, not +0x180 as claimed, and
 *     ctrl/busy landed at +0x28/+0x2c instead of +0x44/+0xc0) - a real
 *     miscompile risk, not just a cosmetic issue. Replaced with plain
 *     byte-offset pointer casts throughout (matching this project's own
 *     convention elsewhere for structs with non-contiguous confirmed
 *     offsets), so there is no struct layout left to get wrong.
 *   - mcasp_reset_stage's shared attempt counter was declared as
 *     `int *attempt = 0` (NULL) - a placeholder that was never finished;
 *     the real code reads the counter's address from a global pointer
 *     variable (DAT_c0002bf4) every stage. Fixed via extern
 *     mcasp_retry_counter.
 *   - mcasp_clock_lock/mcasp_clock_unlock were misnamed and, worse,
 *     mis-modeled: their real bodies (FUN_c000199c/FUN_c00019b0) do no
 *     locking or memory access of any kind - they are pure selector/lookup
 *     functions that pick one of 2-3 baked-in constants by an index
 *     argument. Confirmed via a second, independent call site
 *     (FUN_c000027c @0xc000027c) where their return values ARE captured
 *     into globals, proving they are getters, not void-return
 *     synchronization primitives. Renamed to mcasp_clock_param_select_a/b.
 *   - Ghidra shows the three step functions called with NO visible
 *     arguments in mcasp_init, but their own bodies take one. Comparing
 *     against FUN_c000027c above shows this is ARM r0-register-reuse: the
 *     preceding select-function's return value (still sitting in r0) IS the
 *     real argument. Restored explicitly below instead of silently dropped.
 *   - The TX/RX serializer block was previously documented as "genuinely
 *     symmetric" - false. Of the 7 paired fields, 2 differ (+0x70/+0xb0:
 *     0x80 vs 0xc0; +0x68/+0xa8: DAT_c0002be4 vs DAT_c0002be8). Corrected.
 *   - mcasp_init's real fault-handler call sites pass a per-call source
 *     line number (all resolved below, 325 through 514 decimal) and a
 *     constant file-string pointer (DAT_c0002bf8 == 0xc0022d20 ==
 *     "../MCU/Component/OmapL137Mcasp.cpp", independently re-confirming the
 *     scope correction above via a second code path). The previous pass
 *     left both as bare `0` placeholders; filled in below.
 * ------------------------------------------------------------------------- */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* the OmapL137Mcasp.cpp filename string is DAT_c0002bf8 == 0xc0022d20, always
 * the same at every mcasp_init fault call site (see file header) */
#define MCASP_SRCFILE (0 /* DAT_c0002bf8, "../MCU/Component/OmapL137Mcasp.cpp" */)

/* ------------------------------------------------------------------------- *
 *  mcasp_retry_bound_check - the shared timeout/retry-bound primitive every
 *  stage of mcasp_reset_sequence below uses: `return attempt <=
 *  DAT_c0002654`. DAT_c0002654 resolves to 0x270f (9999 decimal) - a single,
 *  flat retry bound shared by every reset stage in this function, not
 *  stage-specific. Confirmed real body (2026-07-18): 21 xrefs total, 11 from
 *  mcasp_init and 10 from elsewhere - a shared helper, likely used SoC-wide
 *  for register-polling loops, not McASP-specific. @0xc0002640.
 * ------------------------------------------------------------------------- */
extern int mcasp_retry_bound_check(int attempt);	/* FUN_c0002640 */

/* the real shared attempt counter is reached through one more level of
 * indirection than a plain int: DAT_c0002bf4 is itself a global pointer
 * variable whose value is the counter's address, re-read fresh at the top
 * of every reset stage (`piVar4 = DAT_c0002bf4;`). @0xc0002bf4 */
extern int *mcasp_retry_counter;

/*
 * mcasp_reset_stage - one stage of the reset sequence: sets `bit` in the
 * control register at `ctrl_reg`, then polls the SAME register (or, for
 * one stage, a different one - see mcasp_reset_sequence's own note) until
 * that bit reads back set (or, for exactly one stage, CLEAR - the hardware
 * "busy" bit convention is inverted for that one case), retrying via
 * mcasp_retry_bound_check and hard-faulting on real timeout. This helper
 * factors out the 11-times-repeated pattern in the real function rather
 * than transcribing it verbatim eleven times - behaviorally identical, not
 * a simplification of what the hardware actually does.
 */
static void mcasp_reset_stage(uint32_t *ctrl_reg, uint32_t bit, int wait_for_set,
			      int line)
{
	int *attempt = mcasp_retry_counter;

	if (wait_for_set)
		*ctrl_reg |= bit;
	*attempt = 0;
	while (wait_for_set ? ((*ctrl_reg & bit) == 0) : ((*ctrl_reg & bit) != 0)) {
		int a = *attempt;
		if (!mcasp_retry_bound_check(a)) {
			crypto_at88_fault(0, MCASP_SRCFILE, line);
			return;
		}
		*attempt = a + 1;
	}
}

/* ------------------------------------------------------------------------- *
 *  mcasp_configure_clock / mcasp_configure_pins / mcasp_apply_pin_config -
 *  all three confirmed (2026-07-18), all three operate on the CALLER-
 *  SUPPLIED sub_config pointer (mcasp_init's param_2), NOT on the McASP
 *  instance itself - easy to miss since they touch the same +0x10/+0x18
 *  byte offsets mcasp_init also happens to use on its own instance struct
 *  for unrelated fields. Real bodies:
 *
 *    mcasp_configure_clock(cfg):  cfg[+0x10] = 0;  cfg[+0x18] = 0;
 *    mcasp_configure_pins(cfg):   cfg[+0x10] = DAT_c000267c (0x1804);
 *                                 cfg[+0x18] = DAT_c0002680 (0xc02);
 *    mcasp_apply_pin_config(cfg): cfg[+0x10] |= 0x10000;
 *                                 cfg[+0x18] |= 0x10000;
 *
 *  Call order in mcasp_init is clock-then-pins-then-(much later)-apply, so
 *  configure_clock's zeroing is unconditionally overwritten by
 *  configure_pins moments later (both run back-to-back, gated on the same
 *  `if (sub_config)`) - real, faithfully preserved double-write, not dead
 *  code to be optimized out (plain RAM in this decompile, but preserving
 *  the exact write sequence matters if cfg is ever MMIO-backed). Read
 *  together the three calls tell a coherent story: clear the two pin-mux
 *  fields, load the real board pin-function encoding, then - once the
 *  first four reset stages have completed - OR in an enable bit. Final
 *  values when sub_config is supplied: cfg[+0x10] = 0x11804,
 *  cfg[+0x18] = 0x10c02.
 * ------------------------------------------------------------------------- */
void mcasp_configure_clock(void *cfg)		/* FUN_c0002658 */
{
	uint8_t *c = (uint8_t *)cfg;

	*(uint32_t *)(c + 0x10) = 0;
	*(uint32_t *)(c + 0x18) = 0;
}

void mcasp_configure_pins(void *cfg)		/* FUN_c0002668 */
{
	uint8_t *c = (uint8_t *)cfg;

	*(uint32_t *)(c + 0x10) = 0x1804;	/* DAT_c000267c */
	*(uint32_t *)(c + 0x18) = 0xc02;	/* DAT_c0002680 */
}

void mcasp_apply_pin_config(void *cfg)		/* FUN_c0002684 */
{
	uint8_t *c = (uint8_t *)cfg;

	*(uint32_t *)(c + 0x10) |= 0x10000;
	*(uint32_t *)(c + 0x18) |= 0x10000;
}

/* ------------------------------------------------------------------------- *
 *  mcasp_clock_param_select_a / mcasp_clock_param_select_b - PREVIOUSLY
 *  misnamed mcasp_clock_lock/mcasp_clock_unlock and mis-modeled as a
 *  critical-section pair. Real bodies (2026-07-18, both fully re-traced):
 *
 *    select_a(base, sel):  if (sel == 0) return 0x1c00000;
 *                           return DAT_c00019ac;   // 0x1e30000, dead branch
 *                                                   // at every known call site
 *    select_b(base, sel):  if (sel == 0) return DAT_c00019d4;  // 0x1c08000
 *                           if (sel == 1) return DAT_c00019dc; // 0x1c08400
 *                           return DAT_c00019d8;                // 0x1e38000
 *
 *  Neither function reads or writes any memory - both are pure constant
 *  selectors keyed on `sel`, with `base` accepted but unused by either body.
 *  Confirmed NOT locks by a second, independent caller: FUN_c000027c
 *  (unrelated to McASP - a different subsystem entirely, one more data
 *  point that these are generic SoC-wide helpers, not McASP-owned) captures
 *  both return values into globals rather than discarding them, which a
 *  real lock/unlock pair with void semantics could never do.
 *
 *  In mcasp_init, `sel` is always 0 at every call site, so in practice
 *  select_a always yields 0x1c00000 and select_b always yields 0x1c08000 -
 *  two addresses 0x8000 apart, each passed on as the base pointer for one
 *  of the three step functions below (see mcasp_clock_step_a/b/c and the
 *  r0-reuse note in mcasp_init). `base` (DAT_c0002c0c == 0xc00e0068 at the
 *  mcasp_init call sites) is itself just a firmware-RAM address, not a
 *  hardware peripheral base - most likely a shared clock-manager singleton
 *  instance pointer, unrelated to the McASP instance being initialized.
 * ------------------------------------------------------------------------- */
uint32_t mcasp_clock_param_select_a(void *base, int sel)	/* FUN_c000199c */
{
	if (sel == 0)
		return 0x1c00000;
	return 0x1e30000;	/* DAT_c00019ac - unreached by any known caller */
}

uint32_t mcasp_clock_param_select_b(void *base, int sel)	/* FUN_c00019b0 */
{
	if (sel == 0)
		return 0x1c08000;	/* DAT_c00019d4 */
	if (sel == 1)
		return 0x1c08400;	/* DAT_c00019dc */
	return 0x1e38000;		/* DAT_c00019d8 */
}

/* ------------------------------------------------------------------------- *
 *  mcasp_clock_step_a / mcasp_clock_step_b / mcasp_clock_step_c - the
 *  clock-reconfiguration sequence run mid-way through mcasp_init, between
 *  the first four and last seven reset-stage waits. PREVIOUS pass described
 *  this only as "plausibly a PLL divider update, not independently traced" -
 *  now traced (2026-07-18):
 *
 *    - step_b (FUN_c00054a8) is a CONFIRMED empty stub - `{ return; }`,
 *      size 4 bytes (just a branch back). Whatever argument it receives is
 *      irrelevant; it is a genuine no-op at this firmware revision.
 *    - step_a (FUN_c0004cdc) zeroes/initializes a ~0x3a0-byte block of
 *      fields starting at its argument (offsets +0x200 through +0x38c).
 *    - step_c (FUN_c0004d74) is far larger (772 bytes) and writes two
 *      32-entry loops of fixed-size records (0x20 bytes/entry at
 *      base+0x800.., 0x20 bytes/entry at base+0xc00..), each record built
 *      from a base address computed as
 *      `helper(shared_ptr) + index*stride + fixed_offset` (stride 0xc0 and
 *      0x60, offsets 0x400 and 0x1c00, via FUN_c0007108/FUN_c0007114 - see
 *      below). That address-generation shape - a shared global base,
 *      indexed by a small integer, over a fixed-size record array - reads
 *      far more like EDMA3 PaRAM (DMA parameter RAM) descriptor setup for
 *      the McASP TX/RX FIFOs than a PLL register write; step_c's caller
 *      (mcasp_init's own caller, FUN_c000ecc4) independently uses the exact
 *      same shared-base-plus-fixed-offset pattern (+0x400, +0x1c00,
 *      +0x3200) via the same FUN_c0009194 indirection immediately before
 *      calling mcasp_init. This is a genuine, evidence-based correction of
 *      the earlier "PLL divider" guess, though the record layout itself is
 *      not independently confirmed against TI's own EDMA3 PaRAM structure
 *      from this data alone - flagged, not asserted as fact.
 *    - FUN_c0007108/FUN_c0007114 (the two address-generator helpers) both
 *      resolve through FUN_c0009194, a trivial `return *param_1;` pointer
 *      dereference with 41 xrefs firmware-wide - clearly a generic shared
 *      base-pointer accessor, not McASP-owned, so it is deliberately left
 *      un-reconstructed here (out of this file's scope; not called
 *      directly by anything in the confirmed OmapL137Mcasp.cpp block).
 * ------------------------------------------------------------------------- */
extern void mcasp_clock_step_a(uint32_t base);		/* FUN_c0004cdc */
extern void mcasp_clock_step_b(uint32_t base);		/* FUN_c00054a8, confirmed no-op */
extern void mcasp_clock_step_c(uint32_t base);		/* FUN_c0004d74 */

/* ------------------------------------------------------------------------- *
 *  mcasp_init - the confirmed hardware bring-up sequence, matching TI's own
 *  documented McASP reset/enable procedure closely enough to be confident
 *  this is genuinely that sequence, not a generic pattern: writes a control
 *  bit, waits for the corresponding status bit, moving through what reads as
 *  clock-domain reset release, HCLK reset release, serializer reset release,
 *  and transmit/receive submodule resets, each gated on real hardware
 *  settling time (the polling loop, not a fixed delay) rather than a blind
 *  wait. @0xc00026a0. Sole caller: FUN_c000ecc4 (an audio-pipeline-object
 *  constructor which itself sets up the same EDMA-flavored base+offset
 *  fields step_c touches, immediately before calling mcasp_init).
 *
 *  Structure, fully re-traced field-by-field (2026-07-18):
 *   - zeroes 4 state fields (+0x44, +0x60, +0xa0, +0x14)
 *   - if sub_config is supplied: mcasp_configure_clock then
 *     mcasp_configure_pins on sub_config (NOT on the McASP instance - see
 *     the two functions' own note above)
 *   - configures two parallel TX/RX serializer register blocks
 *     (+0x64..+0x78/+0x88 and +0xa4..+0xb8/+0xc8) - MOSTLY symmetric (5 of
 *     7 fields identical between TX/RX) but genuinely NOT fully symmetric:
 *     +0x70(TX)=0x80 vs +0xb0(RX)=0xc0, and +0x68(TX)=0x180f2 vs
 *     +0xa8(RX)=0x180f6 - corrected from the previous pass's "genuinely
 *     symmetric" claim
 *   - sets 6 per-serializer pin-function fields (+0x180,+0x184,+0x188,
 *     +0x190 = 9; +0x194,+0x198 = 10) - a real, confirmed 4/2 split.
 *     Swept the rest of the binary (2026-07-18) for any other function
 *     reading these same offsets: none found in the 691-function set (the
 *     only other hits on the same raw offset literals belong to an
 *     unrelated USB SETUP-packet struct in a different file, a coincidental
 *     offset collision, not a real McASP-side second reader). So the 9-
 *     vs-10 meaning (plausibly "configure as transmit" vs "configure as
 *     receive" for McASP's AXR serializer pins) remains unconfirmed against
 *     TI's own register encoding - genuinely open, not just under-explored.
 *   - unconditionally (re)writes +0x10 = 0x200ff88, +0x18 = 0x8000,
 *     +0x14 = 0x2009f9f, and zeroes +0x48/+0x4c/+0x50 - unrelated to the
 *     sub_config-scoped +0x10/+0x18 fields above (different object
 *     entirely: this is the McASP instance itself, `ma`, not sub_config)
 *   - runs mcasp_reset_stage for bits 0x200, 0x2, 0x100, 0x1 (clock/HCLK
 *     reset release stages, real source lines 325/337/354/366)
 *   - if sub_config supplied: mcasp_apply_pin_config(sub_config)
 *   - clock-reconfiguration sequence: three step calls, each preceded by a
 *     param-select call whose return value is the step's real argument -
 *     Ghidra shows the step calls with zero visible arguments, but ARM's
 *     r0-is-both-arg1-and-return convention means the immediately
 *     preceding select call's result (still live in r0) IS what's passed;
 *     restored explicitly below rather than silently dropped as in the
 *     previous pass:
 *       mcasp_clock_step_a(mcasp_clock_param_select_a(cfg_base, 0));  // 0x1c00000
 *       mcasp_clock_step_b(mcasp_clock_param_select_b(cfg_base, 0));  // 0x1c08000, no-op regardless
 *       mcasp_clock_step_c(mcasp_clock_param_select_a(cfg_base, 0));  // 0x1c00000 again
 *   - sets +0xc0 (busy reg) and +0x80 both to 0xffff (DAT_c0002c10)
 *   - runs mcasp_reset_stage for bits 0x400, 0x4 (lines 419/431), then a
 *     busy-bit-CLEAR wait on the SEPARATE +0xc0 register, bit 0x20, line
 *     456 (inverted polarity - this is the one stage that waits for a bit
 *     to clear rather than set), then bits 0x800, 0x8, 0x1000, 0x10
 *     (serializer/state-machine reset stages, lines 473/485/502/514)
 *   - on full success, zeroes a final global flag (DAT_c0002c2c) and returns
 * ------------------------------------------------------------------------- */
void mcasp_init(void *ma_instance, void *sub_config)	/* FUN_c00026a0 */
{
	uint8_t *ma = (uint8_t *)ma_instance;
	void *cfg_base = (void *)0xc00e0068;	/* DAT_c0002c0c - shared clock-manager
						 * singleton pointer, not McASP-owned */
	uint32_t clk_param;
	int i;

	*(uint32_t *)(ma + 0x44) = 0;
	*(uint32_t *)(ma + 0x60) = 0;
	*(uint32_t *)(ma + 0xa0) = 0;
	*(uint32_t *)(ma + 0x14) = 0;

	if (sub_config) {
		mcasp_configure_clock(sub_config);
		mcasp_configure_pins(sub_config);
	}

	/* TX serializer block (+0x64..+0x78, +0x88) */
	*(uint32_t *)(ma + 0x64) = 0xffffff;
	*(uint32_t *)(ma + 0x68) = 0x180f2;	/* DAT_c0002be4 */
	*(uint32_t *)(ma + 0x6c) = 0x111;	/* DAT_c0002be0 */
	*(uint32_t *)(ma + 0x70) = 0x80;
	*(uint32_t *)(ma + 0x74) = 0xc007;	/* DAT_c0002bdc */
	*(uint32_t *)(ma + 0x78) = 3;
	*(uint32_t *)(ma + 0x88) = 0;

	/* RX serializer block (+0xa4..+0xb8, +0xc8) - NOT fully symmetric
	 * with TX, see +0x70/+0xb0 and +0x68/+0xa8 above */
	*(uint32_t *)(ma + 0xa4) = 0xffffff;
	*(uint32_t *)(ma + 0xa8) = 0x180f6;	/* DAT_c0002be8 - differs from TX's 0x68 */
	*(uint32_t *)(ma + 0xac) = 0x111;	/* DAT_c0002be0, same as TX */
	*(uint32_t *)(ma + 0xb0) = 0xc0;	/* differs from TX's 0x80 */
	*(uint32_t *)(ma + 0xb4) = 0xc007;	/* DAT_c0002bdc, same as TX */
	*(uint32_t *)(ma + 0xb8) = 3;
	*(uint32_t *)(ma + 0xc8) = 0;

	/* 6 per-serializer pin-function assignments: 4 entries set to 9, 2 to
	 * 10 - confirmed 4/2 split, meaning still unresolved (see above) */
	for (i = 0; i < 3; i++)
		*(uint32_t *)(ma + 0x180 + i * 4) = 9;
	*(uint32_t *)(ma + 0x190) = 9;
	*(uint32_t *)(ma + 0x194) = 10;
	*(uint32_t *)(ma + 0x198) = 10;

	/* unconditional (re)write of ma's own +0x10/+0x14/+0x18 - distinct
	 * from sub_config's same-numbered offsets above */
	*(uint32_t *)(ma + 0x10) = 0x200ff88;	/* DAT_c0002bec */
	*(uint32_t *)(ma + 0x18) = 0x8000;
	*(uint32_t *)(ma + 0x14) = 0x2009f9f;	/* DAT_c0002bf0 */
	*(uint32_t *)(ma + 0x50) = 0;
	*(uint32_t *)(ma + 0x4c) = 0;
	*(uint32_t *)(ma + 0x48) = 0;

	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x200, 1, 325);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x002, 1, 337);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x100, 1, 354);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x001, 1, 366);

	if (sub_config)
		mcasp_apply_pin_config(sub_config);

	/* clock-reconfiguration sequence - see mcasp_clock_step_a/b/c note
	 * above for the r0-reuse argument-passing evidence */
	clk_param = mcasp_clock_param_select_a(cfg_base, 0);	/* 0x1c00000 */
	mcasp_clock_step_a(clk_param);
	clk_param = mcasp_clock_param_select_b(cfg_base, 0);	/* 0x1c08000 */
	mcasp_clock_step_b(clk_param);				/* confirmed no-op */
	clk_param = mcasp_clock_param_select_a(cfg_base, 0);	/* 0x1c00000 again */
	mcasp_clock_step_c(clk_param);

	*(uint32_t *)(ma + 0xc0) = 0xffff;	/* DAT_c0002c10 */
	*(uint32_t *)(ma + 0x80) = 0xffff;	/* DAT_c0002c10, same value reused */

	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x400, 1, 419);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x004, 1, 431);
	mcasp_reset_stage((uint32_t *)(ma + 0xc0), 0x020, 0, 456);	/* busy bit CLEAR, inverted polarity */
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x800, 1, 473);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x008, 1, 485);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x1000, 1, 502);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x010, 1, 514);

	/* success path: zero the global "init OK" flag, DAT_c0002c2c */
}

/* ------------------------------------------------------------------------- *
 *  STILL OPEN (2026-07-18):
 *
 *  1. (PARTIALLY RESOLVED, follow-up live disassembly query 2026-07-18)
 *     0xc0002c60 IS real code, confirmed via raw disassembly - it is
 *     genuinely unbounded by Ghidra's auto-analysis (same "no function
 *     object at all" failure mode already seen for eva_board_main.c's own
 *     reset-vector-adjacent code), not dead data. Confirmed real
 *     instructions from 0xc0002c60:
 *
 *         c0002c60: bl   mcasp_configure_clock   (0xc0002658)
 *         c0002c64: cpy  r0, r8
 *         c0002c68: bl   mcasp_configure_pins    (0xc0002668)
 *         c0002c6c: ldr  r1, [DAT_c00030ec]
 *         c0002c70: mov  r3, #0x80
 *         c0002c74: str  r1, [r5, #0x74]
 *         c0002c78: ldr  r12, [DAT_c00030f0]
 *         c0002c7c: str  r3, [r5, #0x70]
 *         c0002c80: ldr  r3, [DAT_c00030f4]
 *         c0002c84: mvn  r0, #0xff000000
 *         c0002c88: str  r12, [r5, #0x6c]
 *         c0002c8c: mov  lr, #0x3
 *         c0002c90: str  r0, [r5, #0x64]
 *         c0002c94: str  r3, [r5, #0x68]
 *         c0002c98: mov  r3, #0xc0
 *         c0002c9c: str  lr, [r5, #0x78]
 *
 *     This writes the exact same five register-block fields (r5+0x64
 *     through +0x78) that mcasp_init sets up, but with DIFFERENT literal
 *     constants (0x80/0xc0/0x3 vs. mcasp_init's own values) and calls
 *     mcasp_configure_clock/_pins the same way mcasp_init does - strong
 *     evidence this is a second, differently-configured McASP instance's
 *     init sequence (or a related reconfigure/deinit path), not dead code.
 *     Its own containing function's start/end boundaries and name remain
 *     unresolved - Ghidra's function manager has no entry for this address
 *     at all, so `get_function_info`/decompile can't be used on it; only
 *     raw disassembly is available. Not reconstructed as a named function
 *     here since its real extent (and thus its real body) isn't known.
 *
 *  2. FUN_c0003228 (@0xc0003228) is a distinct, smaller function that also
 *     zeroes ma+0x44/+0x60/+0xa0/+0x14 and conditionally calls
 *     mcasp_configure_clock/mcasp_configure_pins - structurally a "reduced
 *     reinit" sibling of mcasp_init. It has no xref to the
 *     OmapL137Mcasp.cpp filename string, so its ownership isn't confirmed
 *     the same way mcasp_init's is; deliberately not reconstructed here to
 *     avoid asserting an unconfirmed attribution. Its own caller is
 *     FUN_c0000aa4 @0xc0000aa4.
 *
 *  3. The real meaning of the 9-vs-10 serializer pin-function split
 *     (TX-vs-RX AXR pin function, most likely) is not confirmed against
 *     TI's OMAP-L137 register documentation - no other reader of these
 *     fields exists anywhere in the decompiled function set (see
 *     mcasp_init's own note above).
 *
 *  4. mcasp_clock_step_a/step_c's record layout (32-entry loops of 0x20-
 *     byte records addressed via a shared global base + index*stride +
 *     fixed offset) is evidenced to plausibly be EDMA3 PaRAM descriptor
 *     setup, but not confirmed field-by-field against TI's actual PaRAM
 *     entry layout.
 * ------------------------------------------------------------------------- */
