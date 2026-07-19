/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_usbdc.c - K2 (KRONOS2S_V01R10.VSB) port of the OMAP-L137 USB
 * device controller peripheral driver: endpoint-0 bring-up and the
 * transfer-completion state machine sitting directly downstream of the
 * generic USB-submit primitive.
 *
 * Source: K1_V06R06/omap_l137_usbdc.c (READ-ONLY baseline, not edited).
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json, 2026-07-18), no live Ghidra
 * bridge access this pass (concurrency-unsafe, per this project's own
 * operating constraint - both firmware images are open as separate Ghidra
 * projects that could collide under concurrent access).
 *
 * Anchor: "../MCU/Component/OmapL137Usbdc.cpp" moved from K1 0xc0022d68 to
 * K2 0xc002a790 (confirmed via query_dump_k2.py strings). Every function
 * below was located NOT by proximity to that string but by following the
 * SAME distinctive raw constant K1's own file already used to cross-check
 * itself: the 0xf423f (999999) retry bound, independently present at K2
 * DAT_c0003598 (inside omap_usbdc_init_ep0's K2 counterpart) and
 * DAT_c0004660 (inside omap_usbdc_poll_transfer's K2 counterpart) - both
 * confirmed to also carry a fault-call file-pointer operand resolving to
 * the K2 anchor string address 0xc002a790 itself, closing the loop.
 *
 * GENUINE, CONFIRMED STRUCTURAL DIFFERENCES vs K1 found this pass (not
 * decompiler noise - each cross-checked by resolving every DAT_ operand
 * involved to a concrete value):
 *
 *  1. omap_usbdc_init_ep0's K2 counterpart (FUN_c000345c) NO LONGER writes
 *     the two raw hardware-register constants K1 wrote directly into
 *     `regs`+0x38/+0x3c (0x83e70b13 / 0x95a4f1e0 in K1), and NO LONGER
 *     performs the `regs`+0x144 mode-field masked update
 *     (`& 0xfffff00f | 0x110` in K1). Neither write exists anywhere in
 *     FUN_c000345c's own body NOR in the hardware-reset sequence it now
 *     calls out to (see #2) - `regs` (param_2) is passed through
 *     completely unused except as an argument to that call. Where (or
 *     whether) this configuration now happens is NOT traced this pass -
 *     flagged as open, not guessed.
 *
 *  2. K1's second-phase hardware reset sequence (ctrl|=0x8000, fixed
 *     0x32-iteration delay, clear 0x8000, masked reconfigure, busy-wait for
 *     bit 0x20000) was factored OUT of the init_ep0 body entirely in K2,
 *     into a separate function (FUN_c0001b58, named
 *     omap_usbdc_syscfg_hw_reset below). That function's own fault call
 *     resolves to a DIFFERENT source file than the USB anchor -
 *     "../MCU/Component/OmapL108Syscfg.cpp", line 0x51 (81) - confirmed via
 *     direct DAT_ resolution (DAT_c0001bf0 == 0xc002a748, independently
 *     confirmed to be that exact string's address in K2's string table,
 *     NOT 0xc002a790/OmapL137Usbdc.cpp). This is a real, confirmed
 *     reassignment of this hardware-reset primitive to a shared
 *     syscfg-owned helper, not a transcription error - plausibly a
 *     refactor where OMAP-L1x-family peripheral bring-up shares one
 *     reset-and-wait helper at the syscfg layer.
 *
 *     WITHIN that reused reset sequence, the final register write itself
 *     also changed shape: K1 did `*ctrl = (*ctrl & 0xffff89f0) | 0x4972;`
 *     (preserves bits outside the mask); K2 does a PLAIN, unconditional
 *     `*ctrl = 0x4972;` (DAT_c0001be8, resolved raw value 0x4972) - no
 *     mask-preserve of prior register content. Confirmed by direct
 *     inspection of the decompile (an unconditional store vs. a
 *     read-modify-write produce visibly different instruction shapes, not
 *     something a decompiler conflates) - real behavioral difference, not
 *     re-interpreted.
 *
 *  3. omap_usbdc_init_ep0's K2 counterpart explicitly ARMS the dev-ready
 *     flag (`dev`+4 = 1) immediately before entering the busy-wait poll
 *     loop that waits for bit 0 of that SAME word to clear. K1's version
 *     has no equivalent write - it only ever reads `dev`+4 inside this
 *     function. Confirmed real (not a phantom/dead store): the very next
 *     read (`uVar5 = *(param_1+4)`) that seeds the loop condition reads
 *     back the value just written. Not independently explained (whether
 *     K1 relies on a caller to arm this bit instead is not traced).
 *
 *  4. omap_usbdc_object_init's K2 counterpart (FUN_c000a8a0) zeroes a
 *     slightly different field set on its own `obj` than K1: K1 zeroes
 *     obj+4, obj+6, obj+7, obj+0x58; K2 zeroes obj+4, obj+5, obj+6, obj+0x58
 *     - i.e. obj+7 is NOT zeroed in K2 at all (confirmed absent anywhere
 *     in the function, including its own tail field-setup block), while
 *     obj+5 is zeroed in K2's INITIAL block instead of K1's TAIL block
 *     (where K1 zeroes obj+5 as its very last statement). Net: the same
 *     10 offsets end up touched in K2 as K1's 11 minus obj+7 - a genuine,
 *     confirmed one-field removal, not a reordering artifact.
 *
 * CONFIRMED IDENTICAL / cross-checks that CLOSE two of K1's own "still
 * open" questions (see K1 file's own tail comment):
 *  - omap_usbdc_poll_transfer's K2 counterpart (FUN_c00045bc) is
 *    byte-for-byte structurally identical to K1: same 3-state machine, same
 *    0x1f41 (8001) literal threshold, same 0x401 status-byte offset, same
 *    0xf423f retry bound.
 *  - The thin wrapper (FUN_c000acc8 in K1) is byte-for-byte structurally
 *    identical in K2 (FUN_c000bfcc): same phantom-forwarded-`len`-argument
 *    shape, same single-arg real call site.
 *  - omap_usbdc_clear_pending_state's K2 counterpart (FUN_c000a87c) is
 *    byte-for-byte identical to K1: same 5-byte zero run at obj+0x71..0x75,
 *    same obj+0x78=0xffffffff sentinel.
 *  - K1's own "still open" note asked whether `usbdc_reloc_base`
 *    (init_ep0's own relocation-base global) and `usbdc_reloc_base_b`
 *    (object_init's own relocation-base global) hold the same runtime
 *    value. In K2, BOTH resolve to the exact same literal, 0xc01cc50c
 *    (DAT_c0003578 inside init_ep0's counterpart, DAT_c000a9a0 inside
 *    object_init's counterpart) - confirmed identical in K2. Not
 *    independently re-verified against K1's own build, but a strong hint
 *    the answer there is also "yes".
 *  - Likewise confirmed in K2: the "default ep0 dev handle" the thin
 *    wrapper reads (DAT_c000bfec, K2) and the handle-slot target
 *    omap_usbdc_object_init's counterpart populates (DAT_c000a9a4, K2)
 *    resolve to the exact same literal, 0xc01cc84c - same cross-function
 *    sharing K1 already established between its own equivalents.
 *
 * USB descriptor / VID:PID note: this file (endpoint-0 hardware bring-up
 * and the transfer-completion state machine) contains NO descriptor tables
 * or ID literals of its own in either K1 or K2 - those live in
 * omap_l137_usbdc_ep0.c's usbdc_get_descriptor cluster. See that file's own
 * K2 port for the descriptor-table-pointer comparison.
 */

#include <stdint.h>
#include <stdbool.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_init_ep0 - K2 counterpart of K1's FUN_c0003984. Configures
 *  endpoint 0. See file header items #1-#3 for the three confirmed
 *  behavioral differences from K1 folded into this function's body below.
 *
 *  `dev` (param_1) - software-side endpoint-descriptor struct, same role as
 *    K1: holds the init-busy/ready flag (+4) and every max-packet-size/
 *    transfer-type field this function's tail sets up.
 *  `regs` (param_2) - the MMIO register block. UNLIKE K1, this function's
 *    own body no longer touches `regs` directly at all (see item #1) - it
 *    is only forwarded, unused by this function itself, into
 *    omap_usbdc_syscfg_hw_reset below (which uses regs+0x184, the SAME
 *    ctrl/reset register K1 used at this same offset).
 *
 *  Sequence (K2, FUN_c000345c):
 *   - derives three register-block pointers via the SAME
 *     omap_usbdc_reloc-shaped helper K1 used (offsets +0x2800/+0x2a00/
 *     +0x2c00 over the SAME input `usbdc_reloc_base`, unchanged from K1).
 *   - CONFIRMED NEW (item #3): arms `dev`+4 bit 0 (writes 1) immediately
 *     before the busy-wait poll loop that waits for that SAME bit to clear.
 *   - two plain global-to-global copies (DAT_c0003588->c000358c,
 *     DAT_c0003590->c0003594) - same shape as K1's own two copies, values
 *     not independently decoded (as in K1).
 *   - busy-waits on `dev`+4 bit 0 clearing, same 0xf423f (999999) retry
 *     bound as K1, hard-faults via crypto_at88_fault on real timeout
 *     (line 0xae, SAME line number as K1 - this fault call is still
 *     attributed to OmapL137Usbdc.cpp in K2, confirmed via DAT_c000359c ==
 *     0xc002a790).
 *   - calls omap_usbdc_syscfg_hw_reset(regs) - see that function below for
 *     the CONFIRMED-DIFFERENT-FROM-K1 hardware reset sequence (item #2).
 *     CONFIRMED ABSENT from this function (item #1): K1's own regs+0x38/
 *     +0x3c raw constant writes and regs+0x144 mode-field update - neither
 *     appears here nor inside omap_usbdc_syscfg_hw_reset.
 *   - tail: endpoint-0 max-packet-size/transfer-type fields on `dev`,
 *     IDENTICAL offsets and raw values to K1: dev[0x401]|=0x21,
 *     dev+0x28=dev+0x20, ready-flag&=~8, dev+0x406(u16)=0x1f,
 *     dev+0x408(u16)=0x1e, dev+0x40b: 0 then 8 (same real redundant store
 *     K1 documents), dev+0x30=0x01ff1e1f, dev+0x34=0x00080010 - every one
 *     of these confirmed via direct DAT_ resolution against K2's own dump.
 *  @0xc000345c (K2). K1 counterpart: @0xc0003984.
 * ------------------------------------------------------------------------- */
extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c000a728 (K2) / FUN_c0009194 (K1) - cross-file, out of scope */
extern void omap_usbdc_syscfg_hw_reset(void *regs);	/* FUN_c0001b58 (K2 only - see below) */

void omap_usbdc_init_ep0(void *dev, void *regs)	/* FUN_c000345c (K2) */
{
	uint8_t *d = (uint8_t *)dev;
	uint32_t *ready = (uint32_t *)(d + 4);
	uint32_t attempt;

	extern uint32_t usbdc_regblock_a, usbdc_regblock_b, usbdc_regblock_c;	/* DAT_c000357c/80/84 (K2) */
	extern uint32_t usbdc_reloc_base;	/* DAT_c0003578 (K2) - CONFIRMED same literal (0xc01cc50c)
						 * as omap_usbdc_object_init's own usbdc_reloc_base_b in K2,
						 * see file header */

	usbdc_regblock_a = omap_usbdc_reloc(usbdc_reloc_base) + 0x2800;
	usbdc_regblock_b = omap_usbdc_reloc(usbdc_reloc_base) + 0x2a00;

	/* CONFIRMED NEW vs K1 (item #3): arm the ready flag before polling it. */
	*ready = 1;

	extern uint32_t usbdc_dat_b64, usbdc_dat_b6c;	/* DAT_c0003588 -> DAT_c000358c (K2) */
	extern uint32_t usbdc_dat_b70, usbdc_dat_b74;	/* DAT_c0003590 -> DAT_c0003594 (K2) */
	usbdc_regblock_c = omap_usbdc_reloc(usbdc_reloc_base) + 0x2c00;
	usbdc_dat_b6c = usbdc_dat_b64;
	usbdc_dat_b74 = usbdc_dat_b70;

	/* CONFIRMED ABSENT vs K1 (item #1): no regs+0x38/+0x3c raw-constant
	 * writes, no regs+0x144 mode-field update anywhere in this function. */

	attempt = 0;
	while ((*ready & 1) != 0) {
		if (attempt > 0xf423f) {	/* DAT_c0003598, resolved: 0xf423f (999999), same bound as K1 */
			crypto_at88_fault(0, (const char *)0xc002a790, 0xae);	/* DAT_c000359c == 0xc002a790 (K2 anchor) */
			break;
		}
		attempt++;
	}

	/* CONFIRMED NEW vs K1 (item #2): hardware reset sequence factored out
	 * into a shared helper, now attributed to a DIFFERENT source file
	 * (OmapL108Syscfg.cpp) - see that function's own header comment. */
	omap_usbdc_syscfg_hw_reset(regs);

	d[0x401] |= 0x21;			/* DAT_c00035a0, resolved: 0x401 - identical to K1 */
	*(uint32_t *)(d + 0x28) = *(uint32_t *)(d + 0x20);
	*ready &= 0xfffffff7;
	*(uint16_t *)(d + 0x406) = 0x1f;	/* DAT_c00035a8, resolved: 0x406 - identical to K1 */
	*(uint16_t *)(d + 0x408) = 0x1e;	/* (0x406 + 2) */
	d[0x40b] = 0;				/* DAT_c00035a4, resolved: 0x40b - identical to K1, same
						 * real redundant store K1 already documents */
	d[0x40b] = 8;
	*(uint32_t *)(d + 0x30) = 0x01ff1e1f;	/* DAT_c00035ac, resolved: identical raw value to K1 */
	*(uint32_t *)(d + 0x34) = 0x00080010;	/* DAT_c00035b0, resolved: identical raw value to K1 */
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_syscfg_hw_reset - K2-ONLY function, not present as a separate
 *  symbol in K1 (K1 inlined this same sequence directly into
 *  omap_usbdc_init_ep0's own body). Semantically the second half of K1's
 *  hardware reset sequence: sets ctrl (regs+0x184) bit 0x8000, holds
 *  through the SAME fixed 0x32-iteration empty delay loop, clears bit
 *  0x8000, then - CONFIRMED DIFFERENT FROM K1 (see file header item #2) -
 *  performs a PLAIN unconditional store of 0x4972 (DAT_c0001be8) into ctrl
 *  rather than K1's masked-preserve `(*ctrl & 0xffff89f0) | 0x4972`, then
 *  busy-waits (same 0xf423f/999999 bound) for bit 0x20000 to SET,
 *  hard-faulting on real timeout.
 *
 *  CONFIRMED via direct DAT_ resolution: the fault call's file-pointer
 *  operand (DAT_c0001bf0) resolves to 0xc002a748, which K2's own string
 *  table independently confirms is "../MCU/Component/OmapL108Syscfg.cpp"
 *  (NOT the USB anchor "../MCU/Component/OmapL137Usbdc.cpp" at 0xc002a790)
 *  - line 0x51 (81). This function has genuinely been reattributed to (or
 *    shared with) the OMAP-L1x system-configuration module in K2 - not a
 *    misattribution on this pass's part.
 *
 *  Sole caller in K2: omap_usbdc_init_ep0 above, called with `regs`
 *  forwarded unchanged. @0xc0001b58 (K2). No direct K1 address - this is a
 *  K2-side factoring of code that in K1 has no standalone symbol.
 * ------------------------------------------------------------------------- */
void omap_usbdc_syscfg_hw_reset(void *regs)	/* FUN_c0001b58 (K2) */
{
	uint32_t *ctrl = (uint32_t *)((uint8_t *)regs + 0x184);
	uint32_t attempt;

	*ctrl |= 0x8000;
	for (volatile int i = 0; i < 0x32; i++)
		;	/* fixed-iteration hardware settling delay, same bound as K1 */
	*ctrl &= 0xffff7fff;

	/* CONFIRMED DIFFERENT vs K1: plain store, no mask-preserve of prior
	 * register content (K1: `*ctrl = (*ctrl & 0xffff89f0) | 0x4972;`). */
	*ctrl = 0x4972;	/* DAT_c0001be8, resolved raw value */

	attempt = 0;
	if ((*ctrl & 0x20000) == 0) {
		while (1) {
			if (attempt > 0xf423f) {	/* DAT_c0001bec, resolved: 0xf423f (999999), same bound as K1 */
				crypto_at88_fault(0, (const char *)0xc002a748, 0x51);	/* DAT_c0001bf0 ==
					0xc002a748 == "../MCU/Component/OmapL108Syscfg.cpp", NOT the USB
					anchor - see header comment */
				return;
			}
			attempt++;
			if ((*ctrl & 0x20000) != 0)
				return;
		}
	}
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_poll_transfer - K2 counterpart of K1's FUN_c0004b88.
 *  CONFIRMED structurally IDENTICAL to K1: same 3-state (idle/in-flight/
 *  complete) machine, same DAT_-resolved values throughout (state global,
 *  poll-attempt counter, 0xf423f/999999 retry bound, 0x401 shared status
 *  byte, and the SAME 0x1f41 (8001-byte) literal threshold - confirmed
 *  still a hardcoded immediate in K2's instruction stream too, not
 *  DAT_-sourced, same as K1). Only the fault call's own line-number operand
 *  differs (K2: 0x636 = 1590; K1: 0x6aa = 1706) - expected, since K2's
 *  OmapL137Usbdc.cpp source file itself differs in total content/length
 *  from K1's (this whole project's ~28%-of-payload-bytes-differ baseline).
 *  @0xc00045bc (K2). K1 counterpart: @0xc0004b88.
 * ------------------------------------------------------------------------- */
bool omap_usbdc_poll_transfer(void *ep, int32_t len)	/* FUN_c00045bc (K2) */
{
	extern int32_t usbdc_transfer_state;		/* DAT_c0004658 (K2) */
	uint8_t *e = (uint8_t *)ep;

	if (usbdc_transfer_state == 1) {
		if ((*(uint16_t *)(e + 0x460) & 1) == 0) {
			extern int32_t usbdc_poll_attempts, usbdc_poll_bound;	/* DAT_c000465c, DAT_c0004660 (== 0xf423f) */
			if (usbdc_poll_bound < usbdc_poll_attempts)
				crypto_at88_fault(0, (const char *)0xc002a790, 0x636);	/* DAT_c0004664 == 0xc002a790
					(K2 anchor); DAT_c0004668 == 0x636 (1590), K2's own line number */
			else
				usbdc_poll_attempts++;
			goto out;
		}
		usbdc_transfer_state = 2;
	} else if (usbdc_transfer_state == 0 && len >= 0x1f41) {
		e[0x401] |= 0x40;	/* DAT_c000466c, resolved: 0x401 - identical to K1 */
		usbdc_transfer_state = 1;
	}

out:
	return usbdc_transfer_state == 2;
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_poll_default_ep - K2 counterpart of K1's FUN_c000acc8.
 *  CONFIRMED structurally IDENTICAL to K1: same phantom-forwarded-argument
 *  shape (Ghidra's own decompile of FUN_c000bfcc shows a 0-parameter
 *  signature calling FUN_c00045bc(*DAT_c000bfec) with a single visible
 *  argument, but the real call site - FUN_c000bff0 below, out of this
 *  file's scope - passes 2 arguments; the second, `len`, is genuinely
 *  forwarded through even though Ghidra's per-function analysis of
 *  FUN_c000bfcc itself doesn't show it being read, same class of issue as
 *  K1's own FUN_c000acc8). Net real behavior identical to K1: the handle
 *  argument is ignored, the function always operates on the single global
 *  default USB endpoint-0 handle instead.
 *
 *  Callers (K2): FUN_c000bff0 (the generic USB-submit primitive, same
 *  cross-file role as K1's FUN_c000acec - shared/out of scope, not owned by
 *  this file in either firmware) and FUN_c000a58c (the K2 counterpart of
 *  K1's FUN_c0008b64/master_dispatch_tick - owned by wire_dispatch.c's own
 *  K2 port, not this file).
 *  @0xc000bfcc (K2). K1 counterpart: @0xc000acc8.
 * ------------------------------------------------------------------------- */
extern uint32_t usbdc_ep0_default_handle;	/* DAT_c000bfec (K2), the single global default USB endpoint-0 handle -
						 * CONFIRMED same literal (0xc01cc84c) as omap_usbdc_object_init's
						 * own handle-slot target in K2, see file header */

bool omap_usbdc_poll_default_ep(void *handle_unused, int32_t len)	/* FUN_c000bfcc (K2) */
{
	(void)handle_unused;	/* real argument, but never read - see note above */
	return omap_usbdc_poll_transfer((void *)usbdc_ep0_default_handle, len);
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_object_init - K2 counterpart of K1's FUN_c0009574. See file
 *  header item #4 for the one confirmed field-write difference (obj+7 no
 *  longer zeroed in K2).
 *
 *   - calls FUN_c000183c (K2 counterpart of K1's FUN_c0001a80 - CONFIRMED
 *     still always returns the fixed constant 0x1e00000 regardless of its
 *     own input) and stores that fixed value into the target of the global
 *     handle-pointer slot - identical to K1.
 *   - derives 5 more register-block-style pointers via the SAME
 *     omap_usbdc_reloc helper (offsets +0x200/+0xe240/+0(raw)/+0x2c00/
 *     +0x2a00, IDENTICAL order and offsets to K1).
 *   - initializes `obj` (param_1, the caller-supplied USB object): stores
 *     `init_arg` into its first field, then zeroes obj+4, obj+5, obj+6,
 *     obj+0x58 - CONFIRMED DIFFERENT from K1's obj+4/obj+6/obj+7/obj+0x58
 *     (obj+7 dropped; obj+5 zeroed here instead of in the tail, see below).
 *   - calls FUN_c00016c8 (K2 counterpart of K1's FUN_c0001948 - CONFIRMED
 *     still always returns the fixed constant 0x1c14000/DAT_c00016d0)
 *     then calls omap_usbdc_init_ep0(*global_dev_handle_slot, regs_base,
 *     init_arg) - the SAME 3-argument real call site K1 has (third argument
 *     still dead at the callee, confirmed - omap_usbdc_init_ep0's own body
 *     above never references a third parameter).
 *   - calls omap_usbdc_clear_pending_state(obj) to finish initializing
 *     `obj`, then zeroes/sets the tail fields: obj+0x70, obj+0x80(u16),
 *     obj+0x6e=1, obj+0x7c, obj+0x7e(u16), obj+0x6f - IDENTICAL offsets to
 *     K1's own tail MINUS K1's own trailing obj+5 write (moved earlier in
 *     K2, see above) and updates the same global status word
 *     (`& 0xffffffb0 | 0x20`).
 *
 *  @0xc000a8a0 (K2). K1 counterpart: @0xc0009574.
 * ------------------------------------------------------------------------- */
extern uint32_t omap_usbdc_phantom_const_a(uint32_t unused);	/* FUN_c000183c (K2) / FUN_c0001a80 (K1), always returns 0x1e00000 */
extern uint32_t omap_usbdc_ep0_regs_base(uint32_t unused);	/* FUN_c00016c8 (K2) / FUN_c0001948 (K1), always returns 0x1c14000 */

void omap_usbdc_object_init(uint32_t *obj, uint32_t init_arg)	/* FUN_c000a8a0 (K2) */
{
	extern uint32_t *usbdc_global_dev_handle_slot;	/* DAT_c000a9a4 (K2) - CONFIRMED same literal (0xc01cc84c)
							 * as usbdc_ep0_default_handle above, see file header */
	extern uint32_t usbdc_reloc_base_b;		/* DAT_c000a9a0 (K2) - CONFIRMED same literal (0xc01cc50c)
							 * as omap_usbdc_init_ep0's own usbdc_reloc_base, see header */
	extern uint32_t usbdc_regblock_d, usbdc_regblock_e, usbdc_regblock_f,	/* DAT_c000a9a8/ac/b0 (K2) */
			usbdc_regblock_g, usbdc_regblock_h;			/* DAT_c000a9b4/b8 (K2) */

	*usbdc_global_dev_handle_slot = omap_usbdc_phantom_const_a(0);	/* always 0x1e00000 */

	usbdc_regblock_d = omap_usbdc_reloc(usbdc_reloc_base_b) + 0x200;
	usbdc_regblock_e = omap_usbdc_reloc(usbdc_reloc_base_b) + 0xe240;
	usbdc_regblock_f = omap_usbdc_reloc(usbdc_reloc_base_b);
	usbdc_regblock_g = omap_usbdc_reloc(usbdc_reloc_base_b) + 0x2c00;
	usbdc_regblock_h = omap_usbdc_reloc(usbdc_reloc_base_b) + 0x2a00;

	obj[0] = init_arg;
	((uint8_t *)obj)[4] = 0;
	((uint8_t *)obj)[5] = 0;	/* CONFIRMED DIFFERENT position vs K1 (K1 zeroes obj+5 in its
					 * own tail, last statement) - net field set differs, see below */
	((uint8_t *)obj)[6] = 0;
	/* CONFIRMED ABSENT vs K1: obj+7 is NOT zeroed anywhere in this
	 * function (K1's own `((uint8_t *)obj)[7] = 0;` has no K2 counterpart -
	 * checked both this init block and the tail below). */
	((uint8_t *)obj)[0x58] = 0;

	uint32_t regs_base = omap_usbdc_ep0_regs_base(0);	/* always 0x1c14000 */
	omap_usbdc_init_ep0((void *)(uintptr_t)*usbdc_global_dev_handle_slot, (void *)(uintptr_t)regs_base);
	/* real call also passes `init_arg` as a 3rd argument - confirmed dead,
	 * same as K1 */

	extern void omap_usbdc_clear_pending_state(uint32_t *obj);	/* FUN_c000a87c (K2) */
	omap_usbdc_clear_pending_state(obj);

	extern uint32_t usbdc_status_word;	/* DAT_c000a9bc (K2) */
	((uint8_t *)obj)[0x70] = 0;
	*(uint16_t *)((uint8_t *)obj + 0x80) = 0;
	((uint8_t *)obj)[0x6e] = 1;
	((uint8_t *)obj)[0x7c] = 0;
	*(uint16_t *)((uint8_t *)obj + 0x7e) = 0;
	((uint8_t *)obj)[0x6f] = 0;
	usbdc_status_word = (usbdc_status_word & 0xffffffb0) | 0x20;
	/* NOTE: no trailing obj+5 write here - already done above, unlike K1
	 * which does it here as its own very last statement. Net offsets
	 * touched by this whole function (K2): {4,5,6,0x58,0x6e,0x6f,0x70,
	 * 0x7c,0x7e,0x80} - one fewer than K1's {4,5,6,7,0x58,0x6e,0x6f,0x70,
	 * 0x7c,0x7e,0x80} (missing obj+7). */
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_clear_pending_state - K2 counterpart of K1's FUN_c0009550.
 *  CONFIRMED byte-for-byte structurally IDENTICAL to K1: zeroes a 5-byte
 *  run (obj+0x71..0x75) and sets obj+0x78 = 0xffffffff.
 *  @0xc000a87c (K2). K1 counterpart: @0xc0009550.
 * ------------------------------------------------------------------------- */
void omap_usbdc_clear_pending_state(uint32_t *obj)	/* FUN_c000a87c (K2) */
{
	uint8_t *p = (uint8_t *)obj + 0x70;
	int i = 5;

	do {
		i--;
		p++;
		*p = 0;
	} while (i >= 0);
	*(uint32_t *)((uint8_t *)obj + 0x78) = 0xffffffff;
}

/* -------------------------------------------------------------------------
 * Still genuinely open (K2), not fabricated:
 *  - Where (or whether) K1's regs+0x38/+0x3c raw-constant writes and
 *    regs+0x144 mode-field update moved to in K2 (item #1) - confirmed
 *    absent from every function this pass reconstructed, not traced
 *    further; NEEDS LIVE QUERY: is there a K2-only syscfg-layer USB0
 *    mode-configuration function (plausibly near omap_usbdc_syscfg_hw_reset
 *    at 0xc0001b58, or inside whatever else attributes to
 *    OmapL108Syscfg.cpp) that now does this instead?
 *  - The two raw (non-pointer) 32-bit constants at K2 DAT_c0003588/
 *    DAT_c0003590 (the "two global-to-global copies" inside
 *    omap_usbdc_init_ep0) and their real functional meaning - same
 *    unresolved status as K1's own DAT_c0003b64/b70.
 *  - `omap_usbdc_syscfg_hw_reset`'s own real relationship to whatever else
 *    lives in "OmapL108Syscfg.cpp" - only this one call site (from
 *    omap_usbdc_init_ep0) was traced; whether it's genuinely shared with
 *    non-USB peripheral bring-up in K2 is not confirmed, only strongly
 *    suggested by the file-string attribution.
 *  - Every item K1's own file already left open (5 register-block pointer
 *    overlap between the two derivation sites, `dev+0x24`/`dev+0x28` real
 *    meaning, whether the 8001-byte threshold is a real hardware limit or
 *    firmware policy) remains equally open in K2 - nothing this pass found
 *    resolves them.
 * ------------------------------------------------------------------------- */
