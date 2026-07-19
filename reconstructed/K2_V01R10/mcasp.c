/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mcasp.c - the TI OMAP-L1x McASP (Multichannel Audio Serial Port) peripheral
 * driver: hardware reset/enable sequencing and TX/RX serializer setup.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS2S_V01R10.VSB, 2026-07-18,
 * ported from K1_V06R06/mcasp.c (KRONOS_V06R06.VSB) by direct function-by-
 * function comparison against the K2 static dump.
 *
 * SCOPE: same as K1. "../McAspHandler.cpp" is CONFIRMED ABSENT from K2's
 * string table entirely (checked directly, not assumed) - it was never a
 * real compilation unit in either firmware build. The real driver lives in
 * "../MCU/Component/OmapL137Mcasp.cpp", confirmed present in K2 at
 * 0xc002a76c (K1: 0xc0022d20) via the same DAT_-pointer xref chain K1 used
 * (mcasp_init's own MCASP_SRCFILE global, see below - independently
 * resolves to 0xc002a76c, i.e. the string's own address, which is the
 * strongest possible confirmation this is the same function).
 *
 * PORTING METHOD / CONFIDENCE: every function in this file was matched by
 * decompiling the K2 static dump at the address found and diffing its raw
 * Ghidra decompile TEXT against K1's raw decompile text with all address-
 * literal DAT_/FUN_ tokens masked out. Every function below came back
 * BYTE-FOR-BYTE STRUCTURALLY IDENTICAL to its K1 counterpart - not just
 * similar shape, but literally the same operations, same field offsets,
 * same branch structure, in the same order - modulo relinked addresses.
 * Every resolved constant (register values, retry bound, clock-select
 * constants, pin-config values) is numerically IDENTICAL between K1 and
 * K2. Even mcasp_init's own embedded fault-call source LINE NUMBERS are
 * unchanged (325/337/354/366/419/431/456/473/485/502/514 decimal, cross-
 * checked against K2's own DAT_ constants - see mcasp_init below) - this is
 * about as strong a signal as static analysis can produce that
 * OmapL137Mcasp.cpp itself did NOT change one line between the K1 and K2
 * firmware builds; only the surrounding link layout moved. This is
 * genuinely interesting given the task brief's expectation that K2's audio
 * pipeline "could plausibly have been restructured for the hardware
 * generation change" - it was NOT, at least not at the McASP driver layer.
 * No new serializer, no different clock config, no differently-named
 * subsystem call - this file is a straight recompile/relink, not a
 * hardware-generation port.
 * ------------------------------------------------------------------------- */

#include <stdint.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);
	/* FUN_c000a730 (K1: FUN_c000919c) - shared firmware-wide hard-halt
	 * assert. K2 body confirmed structurally identical to K1's: same
	 * three-call escalation (FUN_c0004f40 stop-audio-ish call,
	 * FUN_c0013824 param stash, two FUN_c0012578 display calls) then an
	 * infinite do{}while(true) hang loop. */

/* the OmapL137Mcasp.cpp filename string is DAT_c00026d0 == 0xc002a76c,
 * always the same at every mcasp_init fault call site (see file header).
 * Confirmed by direct resolution: DAT_c00026d0's own data_value in the K2
 * dump IS 0xc002a76c, the exact address of the "../MCU/Component/
 * OmapL137Mcasp.cpp" string in K2's string table - independently
 * reconfirming both the file-scope attribution and that FUN_c0002178
 * really is mcasp_init. */
#define MCASP_SRCFILE (0 /* DAT_c00026d0, "../MCU/Component/OmapL137Mcasp.cpp" */)

/* ------------------------------------------------------------------------- *
 *  mcasp_retry_bound_check - ported from K1 FUN_c0002640 -> K2 FUN_c0002118.
 *  Body confirmed identical: `return attempt <= DAT_c000212c`.
 *  DAT_c000212c = 0x270f (9999 decimal) - numerically identical to K1's
 *  DAT_c0002654, same flat retry bound. Caller-count re-verification (the
 *  same class of check that caught K1's own off-by-one) not repeated here
 *  since K1's finding was about a DIFFERENT, unrelated shared helper's
 *  xref count; this function's identity is established structurally via
 *  its call sites inside mcasp_init/mcasp_reset_stage below, not via a raw
 *  xref count claim.
 * ------------------------------------------------------------------------- */
extern int mcasp_retry_bound_check(int attempt);	/* FUN_c0002118 */

/* the real shared attempt counter is reached through one more level of
 * indirection than a plain int: DAT_c00026cc is itself a global pointer
 * variable whose value is the counter's address (0xc00e037c on this K2
 * build; K1's DAT_c0002bf4 held a different but analogous RAM address),
 * re-read fresh at the top of every reset stage. @0xc00026cc */
extern int *mcasp_retry_counter;

/*
 * mcasp_reset_stage - one stage of the reset sequence: sets `bit` in the
 * control register at `ctrl_reg`, then polls the SAME register (or, for
 * one stage, a different one - see mcasp_init's own note) until that bit
 * reads back set (or, for exactly one stage, CLEAR - the hardware "busy"
 * bit convention is inverted for that one case), retrying via
 * mcasp_retry_bound_check and hard-faulting on real timeout. Factored out
 * of the 11-times-repeated pattern in mcasp_init, exactly as in K1 - the
 * K2 decompile shows the same eleven inline copies of this pattern,
 * confirmed identical to K1's copies field-by-field.
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
 *  ported from K1 FUN_c0002658/FUN_c0002668/FUN_c0002684 -> K2
 *  FUN_c0002130/FUN_c0002140/FUN_c000215c. All three confirmed structurally
 *  identical to K1, operating on the CALLER-SUPPLIED sub_config pointer
 *  (mcasp_init's param_2), not the McASP instance itself - same +0x10/+0x18
 *  byte-offset overlap-with-unrelated-fields caveat as K1. Real K2 bodies:
 *
 *    mcasp_configure_clock(cfg):  cfg[+0x10] = 0;  cfg[+0x18] = 0;
 *    mcasp_configure_pins(cfg):   cfg[+0x10] = DAT_c0002154 (0x1804);
 *                                 cfg[+0x18] = DAT_c0002158 (0xc02);
 *    mcasp_apply_pin_config(cfg): cfg[+0x10] |= 0x10000;
 *                                 cfg[+0x18] |= 0x10000;
 *
 *  Both pin-mux constants (0x1804, 0xc02) are numerically identical to K1's
 *  DAT_c000267c/DAT_c0002680. Same double-write behavior (configure_clock's
 *  zeroing unconditionally overwritten by configure_pins moments later,
 *  both gated on the same `if (sub_config)`) faithfully preserved.
 * ------------------------------------------------------------------------- */
void mcasp_configure_clock(void *cfg)		/* FUN_c0002130 */
{
	uint8_t *c = (uint8_t *)cfg;

	*(uint32_t *)(c + 0x10) = 0;
	*(uint32_t *)(c + 0x18) = 0;
}

void mcasp_configure_pins(void *cfg)		/* FUN_c0002140 */
{
	uint8_t *c = (uint8_t *)cfg;

	*(uint32_t *)(c + 0x10) = 0x1804;	/* DAT_c0002154 */
	*(uint32_t *)(c + 0x18) = 0xc02;	/* DAT_c0002158 */
}

void mcasp_apply_pin_config(void *cfg)		/* FUN_c000215c */
{
	uint8_t *c = (uint8_t *)cfg;

	*(uint32_t *)(c + 0x10) |= 0x10000;
	*(uint32_t *)(c + 0x18) |= 0x10000;
}

/* ------------------------------------------------------------------------- *
 *  mcasp_clock_param_select_a / mcasp_clock_param_select_b - ported from K1
 *  FUN_c000199c/FUN_c00019b0 -> K2 FUN_c000171c/FUN_c0001730. Confirmed
 *  identical pure constant-selector bodies, all four resolved constants
 *  numerically identical to K1:
 *
 *    select_a(base, sel):  if (sel == 0) return 0x1c00000;
 *                           return DAT_c000172c;   // 0x1e30000, dead branch
 *    select_b(base, sel):  if (sel == 0) return DAT_c0001754;  // 0x1c08000
 *                           if (sel == 1) return DAT_c000175c; // 0x1c08400
 *                           return DAT_c0001758;                // 0x1e38000
 *
 *  As in K1, `sel` is always 0 at every mcasp_init call site, so select_a
 *  always yields 0x1c00000 and select_b always yields 0x1c08000. `base`
 *  (DAT_c00026e4 on this K2 build, resolves to 0xc00e004c - a different RAM
 *  address than K1's 0xc00e0068, but the same "shared clock-manager
 *  singleton pointer, not McASP-owned" role) is accepted but unused by
 *  either body, same as K1.
 * ------------------------------------------------------------------------- */
uint32_t mcasp_clock_param_select_a(void *base, int sel)	/* FUN_c000171c */
{
	if (sel == 0)
		return 0x1c00000;
	return 0x1e30000;	/* DAT_c000172c - unreached by any known caller */
}

uint32_t mcasp_clock_param_select_b(void *base, int sel)	/* FUN_c0001730 */
{
	if (sel == 0)
		return 0x1c08000;	/* DAT_c0001754 */
	if (sel == 1)
		return 0x1c08400;	/* DAT_c000175c */
	return 0x1e38000;		/* DAT_c0001758 */
}

/* ------------------------------------------------------------------------- *
 *  mcasp_clock_step_a / mcasp_clock_step_b / mcasp_clock_step_c - ported
 *  from K1 FUN_c0004cdc/FUN_c00054a8/FUN_c0004d74 -> K2 FUN_c0004710/
 *  FUN_c0004edc/FUN_c00047a8. All three re-decompiled directly from the K2
 *  dump and diffed against K1 with addresses masked - CONFIRMED byte-for-
 *  byte structurally identical, including variable-numbering artifacts in
 *  Ghidra's own output (a strong signal of an unmodified recompile, not
 *  just equivalent logic):
 *
 *    - step_b (FUN_c0004edc) is STILL a confirmed empty stub - `{ return; }`,
 *      size 4 bytes, identical to K1.
 *    - step_a (FUN_c0004710), size 148 bytes (K1: 148), zeroes/initializes
 *      the same field layout at the same offsets (+0x200 through +0x38c)
 *      with the same literal constants (+0x284=0x10, +0x308=0xffffffff,
 *      +0x340=3, etc.) - identical to K1's FUN_c0004cdc field-for-field.
 *    - step_c (FUN_c00047a8), size 772 bytes (K1: 772), same two 32-entry
 *      loops of fixed-size records (0x20 bytes/entry at base+0x800..,
 *      0x20 bytes/entry at base+0xc00..), same stride/offset constants
 *      (0xc0/0x60 stride via +0x400/+0x1c00 base offsets), same EDMA3
 *      PaRAM-descriptor-shaped address generation - the only difference
 *      from K1's FUN_c0004d74 is the two address-generator helper
 *      addresses themselves (K1 FUN_c0007108/FUN_c0007114 -> K2
 *      FUN_c0008c6c/FUN_c0008c78), which resolve through the same generic
 *      shared-base-pointer accessor pattern K1 already identified as out
 *      of this file's scope. mcasp_init's own K2 caller (FUN_c00100ac,
 *      see mcasp_init below) independently uses the identical
 *      +0x400/+0x1c00/+0x3200 shared-base-plus-fixed-offset pattern
 *      immediately before calling mcasp_init - the same cross-confirmation
 *      K1 found at its own caller (FUN_c000ecc4).
 * ------------------------------------------------------------------------- */
extern void mcasp_clock_step_a(uint32_t base);		/* FUN_c0004710 */
extern void mcasp_clock_step_b(uint32_t base);		/* FUN_c0004edc, confirmed no-op */
extern void mcasp_clock_step_c(uint32_t base);		/* FUN_c00047a8 */

/* ------------------------------------------------------------------------- *
 *  mcasp_init - ported from K1 FUN_c00026a0 -> K2 FUN_c0002178. Size 1340
 *  bytes in BOTH builds (exact match). @0xc0002178. Sole caller: FUN_c00100ac
 *  (K1: FUN_c000ecc4) - the K2 audio-pipeline-object constructor, confirmed
 *  via direct decompile to set up the same +0x400/+0x1c00/+0x3200
 *  EDMA-flavored base+offset fields immediately before calling this
 *  function (`FUN_c0002178(*puVar2, uVar6)` is literally the last statement
 *  in FUN_c00100ac).
 *
 *  Every field write, every branch, every constant reconstructed here was
 *  cross-checked against the K2 dump directly (not assumed-ported from K1
 *  blind) and found IDENTICAL:
 *   - same 4 zeroed state fields (+0x44, +0x60, +0xa0, +0x14)
 *   - same sub_config-gated configure_clock/configure_pins pair
 *   - same TX/RX serializer block values, including the same NOT-fully-
 *     symmetric pair: +0x70(TX)=0x80 vs +0xb0(RX)=0xc0, and
 *     +0x68(TX)=0x180f2 (DAT_c00026bc) vs +0xa8(RX)=0x180f6 (DAT_c00026c0) -
 *     numerically identical constants to K1's DAT_c0002be4/DAT_c0002be8
 *   - same 6 per-serializer pin-function fields, same confirmed 4/2 split
 *     (9,9,9,9,10,10) - meaning still open, same as K1 (see below)
 *   - same unconditional (re)write of ma's own +0x10/+0x14/+0x18:
 *     0x200ff88 (DAT_c00026c4, == K1's DAT_c0002bec), 0x8000,
 *     0x2009f9f (DAT_c00026c8, == K1's DAT_c0002bf0)
 *   - same reset-stage sequence for bits 0x200/0x2/0x100/0x1, and CONFIRMED
 *     IDENTICAL source line numbers via K2's own DAT_ constants:
 *       DAT_c00026d4=0x145=325, DAT_c00026d8=0x151=337,
 *       DAT_c00026dc=0x162=354, DAT_c00026e0=0x16e=366
 *     - all four decimal values match K1's 325/337/354/366 exactly. This
 *     is strong, independent evidence OmapL137Mcasp.cpp's SOURCE did not
 *     change between the K1 and K2 firmware builds - only relinked.
 *   - same sub_config-gated apply_pin_config call
 *   - same three-call clock-reconfiguration sequence, same r0-reuse
 *     argument-passing shape (step call's real argument is the immediately
 *     preceding select call's still-live return value):
 *       mcasp_clock_step_a(mcasp_clock_param_select_a(cfg_base, 0));  // 0x1c00000
 *       mcasp_clock_step_b(mcasp_clock_param_select_b(cfg_base, 0));  // 0x1c08000, no-op regardless
 *       mcasp_clock_step_c(mcasp_clock_param_select_a(cfg_base, 0));  // 0x1c00000 again
 *   - same +0xc0/+0x80 = 0xffff (DAT_c00026e8, == K1's DAT_c0002c10)
 *   - same reset-stage sequence for bits 0x400/0x4, CONFIRMED identical
 *     lines 419/431 (DAT_c00026ec=0x1a3=419, DAT_c00026f0=0x1af=431)
 *   - same busy-bit-CLEAR wait on the separate +0xc0 register, bit 0x20,
 *     line 456 (0x1c8) - inline literal in BOTH builds, identical
 *   - same final reset-stage sequence for bits 0x800/0x8/0x1000/0x10,
 *     CONFIRMED identical lines 473/485/502/514 (DAT_c00026f4=0x1d9=473,
 *     DAT_c00026f8=0x1e5=485, DAT_c00026fc=0x1f6=502, DAT_c0002700=0x202=514)
 *   - same success path: zero the final "init OK" flag (DAT_c0002704,
 *     resolves to 0xc00a09a8 on this K2 build) and return
 * ------------------------------------------------------------------------- */
void mcasp_init(void *ma_instance, void *sub_config)	/* FUN_c0002178 */
{
	uint8_t *ma = (uint8_t *)ma_instance;
	void *cfg_base = (void *)0xc00e004c;	/* DAT_c00026e4 - shared clock-manager
						 * singleton pointer, not McASP-owned;
						 * K1's analogous DAT_c0002c0c held
						 * 0xc00e0068, a different RAM address
						 * on that build */
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
	*(uint32_t *)(ma + 0x68) = 0x180f2;	/* DAT_c00026bc */
	*(uint32_t *)(ma + 0x6c) = 0x111;	/* DAT_c00026b8 */
	*(uint32_t *)(ma + 0x70) = 0x80;
	*(uint32_t *)(ma + 0x74) = 0xc007;	/* DAT_c00026b4 */
	*(uint32_t *)(ma + 0x78) = 3;
	*(uint32_t *)(ma + 0x88) = 0;

	/* RX serializer block (+0xa4..+0xb8, +0xc8) - NOT fully symmetric
	 * with TX, see +0x70/+0xb0 and +0x68/+0xa8 above */
	*(uint32_t *)(ma + 0xa4) = 0xffffff;
	*(uint32_t *)(ma + 0xa8) = 0x180f6;	/* DAT_c00026c0 - differs from TX's 0x68 */
	*(uint32_t *)(ma + 0xac) = 0x111;	/* DAT_c00026b8, same as TX */
	*(uint32_t *)(ma + 0xb0) = 0xc0;	/* differs from TX's 0x80 */
	*(uint32_t *)(ma + 0xb4) = 0xc007;	/* DAT_c00026b4, same as TX */
	*(uint32_t *)(ma + 0xb8) = 3;
	*(uint32_t *)(ma + 0xc8) = 0;

	/* 6 per-serializer pin-function assignments: 4 entries set to 9, 2 to
	 * 10 - confirmed 4/2 split, meaning still unresolved (see K1 note,
	 * carries over unchanged to K2) */
	for (i = 0; i < 3; i++)
		*(uint32_t *)(ma + 0x180 + i * 4) = 9;
	*(uint32_t *)(ma + 0x190) = 9;
	*(uint32_t *)(ma + 0x194) = 10;
	*(uint32_t *)(ma + 0x198) = 10;

	/* unconditional (re)write of ma's own +0x10/+0x14/+0x18 - distinct
	 * from sub_config's same-numbered offsets above */
	*(uint32_t *)(ma + 0x10) = 0x200ff88;	/* DAT_c00026c4 */
	*(uint32_t *)(ma + 0x18) = 0x8000;
	*(uint32_t *)(ma + 0x14) = 0x2009f9f;	/* DAT_c00026c8 */
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

	*(uint32_t *)(ma + 0xc0) = 0xffff;	/* DAT_c00026e8 */
	*(uint32_t *)(ma + 0x80) = 0xffff;	/* DAT_c00026e8, same value reused */

	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x400, 1, 419);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x004, 1, 431);
	mcasp_reset_stage((uint32_t *)(ma + 0xc0), 0x020, 0, 456);	/* busy bit CLEAR, inverted polarity */
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x800, 1, 473);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x008, 1, 485);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x1000, 1, 502);
	mcasp_reset_stage((uint32_t *)(ma + 0x44), 0x010, 1, 514);

	/* success path: zero the global "init OK" flag, DAT_c0002704 */
}

/* ------------------------------------------------------------------------- *
 *  mcasp_reinit_reduced - K2 counterpart of K1's FUN_c0003228, ported to
 *  K2 FUN_c0002d00. Same "still open" ownership caveat as K1: no xref to
 *  the OmapL137Mcasp.cpp filename string, so attribution to this file is
 *  by structural resemblance only, not proven the way mcasp_init is.
 *  Confirmed structurally identical to K1's FUN_c0003228 - same size (84
 *  bytes in both builds), same two leading calls with a hardcoded second
 *  argument of 1, same 4-field zero, same sub_config-gated
 *  configure_clock/configure_pins pair (note: K2's decompile, like K1's,
 *  shows configure_clock called with the visible argument dropped - the
 *  same r0-reuse pattern documented for mcasp_init's clock-step calls
 *  applies here too, though not independently re-derived for this
 *  function). Its own caller is FUN_c0000864 (K1: FUN_c0000aa4), call site
 *  0xc000088c (K1: 0xc0000acc). Deliberately NOT reconstructed as a callable
 *  function body here, matching K1's own choice, to avoid asserting an
 *  unconfirmed file attribution - the two constituent calls
 *  (FUN_c0002c6c/FUN_c0002c88 in K2) are themselves unidentified and were
 *  not traced further, exactly as K1 left FUN_c0003194/FUN_c00031b0
 *  untraced.
 *
 *  Not defined here - see the STILL OPEN section below for why.
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 *  STILL OPEN (ported from K1, plus new K2-specific findings):
 *
 *  1. Same Ghidra-unbounded second code region K1 found at 0xc0002c60,
 *     confirmed to have a K2 counterpart at the PREDICTED address
 *     0xc0002738 (mcasp_init's own start address 0xc0002178 plus the exact
 *     same +0x5c0 byte offset K1's 0xc0002c60 sat at relative to
 *     0xc00026a0). Evidence: the K2 dump's own xref data shows two call
 *     sites with from_func == None (i.e. no bounded function object,
 *     exactly K1's "no function object at all" failure mode) at 0xc0002738
 *     and 0xc0002740, calling mcasp_configure_clock (FUN_c0002130) and
 *     mcasp_configure_pins (FUN_c0002140) respectively, IN THE SAME ORDER
 *     K1's raw disassembly showed at 0xc0002c60. This is strong indirect
 *     confirmation the region exists and does the same thing, but the
 *     static dump used for this port has no raw-disassembly command
 *     (unlike the live-query follow-up K1's own README describes) capable
 *     of reading actual instructions there.
 *     2026-07-19 LIVE QUERY RESOLVED: live get_disassembly at 0xc0002738
 *     CONFIRMS real code, not just xref-inferred existence - `bl 0xc0002130`
 *     (mcasp_configure_clock) immediately followed by `cpy r0,r8` then
 *     `bl 0xc0002140` (mcasp_configure_pins), exactly the order the K2 xref
 *     data predicted. The sequence continues past both calls (not just a
 *     2-call stub): loads three more literal-pool constants and stores them
 *     into offsets +0x74/+0x70 off r5 (mov r3,#0x80 into +0x70, a literal
 *     dword into +0x74), then continues loading further literals - a real,
 *     substantive continuation, consistent with a genuine second-instance
 *     init/reconfigure sequence rather than a bare 2-call trampoline. Same
 *     "no bounded Function object" status as omap_l108_syscfg.c's own
 *     orphan cluster (get_disassembly returns real instructions here on
 *     direct request; no Function/xref-caller data exists for it as a
 *     unit). Real extent beyond the ~10 instructions read this pass not
 *     independently established (would need to keep walking); genuinely
 *     confirms the region exists and does real, non-trivial work, per the
 *     hypothesis.
 *
 *  2. mcasp_reinit_reduced (K2 FUN_c0002d00, see note above) - K2
 *     counterpart of K1's FUN_c0003228, structurally identical, same
 *     unconfirmed-ownership caveat, deliberately not reconstructed as a
 *     named function body for the same reason K1 left it out.
 *
 *  3. The real meaning of the 9-vs-10 serializer pin-function split is
 *     UNCHANGED and still not confirmed against TI's OMAP-L137 register
 *     documentation - same open question as K1, not independently
 *     re-investigated for K2 since the values and structure are identical.
 *
 *  4. mcasp_clock_step_a/step_c's record layout (EDMA3 PaRAM descriptor
 *     setup hypothesis) carries over unchanged from K1 - still not
 *     confirmed field-by-field against TI's actual PaRAM entry layout.
 *
 *  CONCLUSION FOR THIS FILE: no evidence of audio-pipeline restructuring
 *  between K1 and K2 at the McASP driver layer. Every function, every
 *  offset, every constant, and even the embedded source line numbers are
 *  identical between the two builds; only absolute addresses differ. The
 *  Kronos 2 hardware generation change did not touch this subsystem's
 *  logic.
 * ------------------------------------------------------------------------- */
