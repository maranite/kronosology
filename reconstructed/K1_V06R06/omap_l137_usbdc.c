/* SPDX-License-Identifier: GPL-2.0 */
/*
 * omap_l137_usbdc.c - the OMAP-L137 USB device controller peripheral
 * driver: endpoint-0 bring-up and a transfer-completion state machine that
 * turns out to sit DIRECTLY downstream of the generic USB-submit primitive
 * (FUN_c000acec) this project already established as shared across
 * crypto_at88.c, cobjectmgr.c, and cad.c's own event pump - closing the
 * loop on where every subsystem's "send an event to the host" call
 * ultimately bottoms out in real hardware.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-17.
 * Anchor: "../MCU/Component/OmapL137Usbdc.cpp" has 3 xrefs - two inside
 * omap_usbdc_init_ep0 below, one inside omap_usbdc_poll_transfer.
 *
 * CLOSURE PASS, 2026-07-18 (static-dump re-query, no live Ghidra bridge):
 * re-queried every function/DAT_ constant this file's own "still open" list
 * had flagged. Real, concrete results:
 *  - omap_usbdc_init_ep0's own final field-setup block was WRONG in the
 *    prior draft: every field in that block is written on `dev` (param_1),
 *    NOT on the `ep0`/hardware-register-block parameter (param_2) as
 *    previously claimed - corrected below, see that function's own note.
 *  - Almost every `DAT_` constant in this file now has a REAL resolved
 *    value (not just a named-but-unknown placeholder) - retry bounds, byte
 *    offsets, a shared status/flags-byte offset used by BOTH functions in
 *    this file, and the two hard-fault line numbers/file pointers. See
 *    each site below.
 *  - FUN_c0009574 (the higher-level USB object bring-up caller) is now
 *    fully decompiled and reconstructed, including a genuinely new
 *    cross-file finding: its own caller is `FUN_c00074bc`, which
 *    `eva_board_main.c` already names `eva_board_final_setup` and lists as
 *    one of ITS OWN still-open items ("own real role... not traced"). Not
 *    edited into eva_board_main.c this pass (owned by a different agent
 *    concurrently) - flagged here for that file's own next pass.
 *  - The exact code path between FUN_c000acc8 and FUN_c000acec is now
 *    fully traced: FUN_c000acc8 is NOT a no-op-adding wrapper as previously
 *    claimed - it silently forwards a real second argument (`len`) that
 *    Ghidra's own per-function signature analysis failed to recognize as a
 *    parameter of FUN_c000acc8 itself (both of its real call sites pass 2
 *    arguments; FUN_c000acc8's own decompiled signature shows 0). See its
 *    own section below.
 */

#include <stdint.h>
#include <stdbool.h>

extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_init_ep0 - one of the two confirmed anchors. Configures the
 *  control endpoint (endpoint 0). Takes TWO distinct objects, confirmed
 *  (this pass) to be genuinely different things, not two names for the same
 *  pointer as an earlier draft implied:
 *   - `dev` (param_1) - a software-side endpoint-descriptor struct: holds
 *     the init-busy/ready flag this function polls (+4), and - corrected
 *     this pass - EVERY ONE of the final max-packet-size/transfer-type
 *     fields this function sets up (see the tail of the function below).
 *   - `regs` (param_2) - the actual MMIO register block: the mode field
 *     (+0x144) and the reset/control register (+0x184) this function
 *     busy-waits on both live here, nowhere else.
 *
 *  Sequence:
 *   - derives three register-block pointers via a shared relocation helper
 *     (omap_usbdc_reloc/FUN_c0009194, called three times with the SAME
 *     input `usbdc_reloc_base` - offsets +0x2800/+0x2a00/+0x2c00 added to
 *     each result, stored into three separate global pointer slots). Two
 *     more plain global-to-global copies happen in between (newly found
 *     this pass, see below) - not part of the register-block derivation,
 *     real purpose not traced.
 *   - writes two raw (non-pointer - see the DAT_ resolution note below)
 *     32-bit constants into `regs`+0x38/+0x3c.
 *   - sets the mode field (`regs`+0x144, bits 0x0f0, value 0x110).
 *   - runs a bounded busy-wait on `dev`'s ready flag (+4, bit 0), retrying
 *     up to the resolved bound below before hard-faulting (line 0xae).
 *   - the hardware reset sequence proper, entirely on `regs`+0x184: sets
 *     bit 0x8000, holds through a fixed 50 (0x32) iteration empty delay
 *     loop, clears it, then a masked-and-OR'd reconfiguration
 *     (`& 0xffff89f0 | 0x4972`), then waits (same bound) for bit 0x20000 to
 *     SET, hard-faulting on real timeout (line 0xda) - the second of the
 *     two "../MCU/Component/OmapL137Usbdc.cpp" assert call sites.
 *   - CORRECTED tail: endpoint-0 max-packet-size/transfer-type fields, all
 *     on `dev` (param_1), not `regs`:
 *       dev[0x401] |= 0x21          (a status/flags byte - see cross-
 *                                     function note below, this SAME byte
 *                                     offset is also touched by
 *                                     omap_usbdc_poll_transfer)
 *       dev+0x28  = dev+0x20        (copy one 32-bit field to another)
 *       dev's ready flag &= ~8      (clears bit 3 of the same flag word
 *                                     the busy-wait above polled bit 0 of)
 *       dev+0x406 (u16) = 0x1f      (31 - plausibly EP0 max-packet-size)
 *       dev+0x408 (u16) = 0x1e      (30 - plausibly a second max-packet
 *                                     value, e.g. the OUT direction)
 *       dev+0x40b (u8)  = 0 then 8  (a genuine redundant/dead store in the
 *                                     real compiled code - writes 0
 *                                     immediately overwritten by 8, not a
 *                                     transcription artifact)
 *       dev+0x30 = 0x01ff1e1f       (raw constant, real value now known,
 *                                     meaning not decoded)
 *       dev+0x34 = 0x00080010       (raw constant, real value now known,
 *                                     meaning not decoded)
 *  @0xc0003984. Confirmed real caller: FUN_c0009574 (omap_usbdc_object_init
 *  below) - allocates the same three register-block offsets into a handle
 *  struct before calling this, then zeroes several more state-struct
 *  fields and calls FUN_c0009550, both now fully traced below.
 * ------------------------------------------------------------------------- */
extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c0009194 */

void omap_usbdc_init_ep0(void *dev, void *regs)	/* FUN_c0003984 */
{
	uint8_t *d = (uint8_t *)dev;
	uint8_t *r = (uint8_t *)regs;
	uint32_t *ready = (uint32_t *)(d + 4);
	uint32_t *ctrl = (uint32_t *)(r + 0x184);
	uint32_t attempt;

	extern uint32_t usbdc_regblock_a, usbdc_regblock_b, usbdc_regblock_c;	/* DAT_c0003b54/b58/b68 */
	extern uint32_t usbdc_reloc_base;	/* DAT_c0003b50 */

	usbdc_regblock_a = omap_usbdc_reloc(usbdc_reloc_base) + 0x2800;
	usbdc_regblock_b = omap_usbdc_reloc(usbdc_reloc_base) + 0x2a00;
	usbdc_regblock_c = omap_usbdc_reloc(usbdc_reloc_base) + 0x2c00;

	/* RESOLVED this pass: DAT_c0003b5c/b60 are NOT pointers (their raw
	 * values don't fall in this image's ~0xc0000000-0xc0300000 code/data
	 * range) - they're literal hardware-register configuration words. */
	*(uint32_t *)(r + 0x38) = 0x83e70b13;	/* DAT_c0003b5c, resolved raw value */
	*(uint32_t *)(r + 0x3c) = 0x95a4f1e0;	/* DAT_c0003b60, resolved raw value */

	/* NEWLY FOUND this pass: two plain global-to-global copies, not
	 * present in the earlier draft of this function at all. Both source/
	 * dest pairs are themselves large raw values (0xc423c020/0xc423c420 -
	 * again not valid pointers into this image, and suspiciously exactly
	 * 0x400 apart), consistent with copying some kind of encoded
	 * default/reset constant pair into a "current" shadow slot - real
	 * purpose not traced this pass. */
	extern uint32_t usbdc_dat_b64, usbdc_dat_b6c;	/* DAT_c0003b64 -> DAT_c0003b6c */
	extern uint32_t usbdc_dat_b70, usbdc_dat_b74;	/* DAT_c0003b70 -> DAT_c0003b74 */
	usbdc_dat_b6c = usbdc_dat_b64;
	usbdc_dat_b74 = usbdc_dat_b70;

	*(uint32_t *)(r + 0x144) = (*(uint32_t *)(r + 0x144) & 0xfffff00f) | 0x110;

	/* RESOLVED this pass: the retry bound (DAT_c0003b78) is a real value,
	 * 0xf423f = 999999 - the SAME constant omap_usbdc_poll_transfer's own
	 * retry bound (DAT_c0004c2c) uses, confirmed by direct comparison, not
	 * just structural resemblance. Likewise DAT_c0003b7c (the fault call's
	 * "file" argument) resolves to 0xc0022d68 - literally the address of
	 * the "../MCU/Component/OmapL137Usbdc.cpp" string itself, confirming
	 * this genuinely is one of the anchor's 3 real xrefs. */
	attempt = 0;
	while ((*ready & 1) != 0) {
		if (attempt > 0xf423f) {
			crypto_at88_fault(0, (const char *)0xc0022d68, 0xae);
			break;
		}
		attempt++;
	}

	*ctrl |= 0x8000;
	for (volatile int i = 0; i < 0x32; i++)
		;	/* fixed-iteration hardware settling delay, real cycle count not derived */
	*ctrl &= 0xffff7fff;
	*ctrl = (*ctrl & 0xffff89f0) | 0x4972;

	attempt = 0;
	while ((*ctrl & 0x20000) == 0) {
		if (attempt > 0xf423f) {
			crypto_at88_fault(0, (const char *)0xc0022d68, 0xda);
			break;
		}
		attempt++;
	}

	/* CORRECTION (this pass): every field below is written on `dev`
	 * (param_1), NOT on `regs` (param_2) as the earlier draft of this
	 * function claimed - re-verified directly against a fresh decompile.
	 * All four byte-offset DAT_ constants below are now resolved to real
	 * values (0x401/0x40b/0x406), not left as unresolved placeholders. */
	d[0x401] |= 0x21;			/* DAT_c0003b80, resolved: offset 0x401 - see cross-function note below */
	*(uint32_t *)(d + 0x28) = *(uint32_t *)(d + 0x20);
	*ready &= 0xfffffff7;
	*(uint16_t *)(d + 0x406) = 0x1f;	/* DAT_c0003b88, resolved: offset 0x406 */
	*(uint16_t *)(d + 0x408) = 0x1e;	/* (0x406 + 2) */
	d[0x40b] = 0;				/* DAT_c0003b84, resolved: offset 0x40b - real redundant store, see comment above */
	d[0x40b] = 8;
	*(uint32_t *)(d + 0x30) = 0x01ff1e1f;	/* DAT_c0003b8c, resolved raw value */
	*(uint32_t *)(d + 0x34) = 0x00080010;	/* DAT_c0003b90, resolved raw value */
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_poll_transfer - the other confirmed anchor, and the real
 *  bottom of a call chain traced across this entire project. A 3-state
 *  (idle -> in-flight -> complete) transfer-completion state machine
 *  (state stored in a fixed global, DAT_c0004c24):
 *
 *   state 0 (idle): if the requested transfer size (`len`) is below a fixed
 *     LITERAL threshold - 0x1f41 = 8001 bytes, confirmed this pass to be a
 *     hardcoded immediate in the instruction stream, NOT sourced from any
 *     DAT_ global - does nothing; otherwise sets bit 0x40 in a status byte
 *     at offset 0x401 (DAT_c0004c38, resolved this pass - see cross-
 *     function note below) and advances to state 1.
 *   state 1 (in-flight): checks a hardware status register (offset+0x460,
 *     bit 0) - if the transfer is still busy (bit clear), retries up to
 *     0xf423f (999999, the SAME resolved bound as omap_usbdc_init_ep0's
 *     own retries) before hard-faulting on real timeout (line 0x6aa = 1706,
 *     resolved this pass - the earlier draft left this as an unknown "0"
 *     placeholder); once the bit sets, advances to state 2.
 *   state 2 (complete): terminal state - the function returns true exactly
 *     when this state is reached (this call or a prior one).
 *
 *  CROSS-FUNCTION FINDING (this pass): DAT_c0004c38 (this function's own
 *  status-byte offset) and DAT_c0003b80 (omap_usbdc_init_ep0's own
 *  max-packet flags-byte offset, above) BOTH resolve to the exact same
 *  value, 0x401. The two functions share one flags byte in the `dev`
 *  struct: omap_usbdc_init_ep0 sets bits 0x21 in it at bring-up, and this
 *  function OR's in bit 0x40 ("large transfer in progress") on top - a
 *  real, concrete confirmation that `ep` here is the SAME `dev` struct
 *  omap_usbdc_init_ep0 populates, not merely a same-shaped but distinct
 *  object.
 *
 *  Reached via a thin-looking wrapper (FUN_c000acc8 - see its own section
 *  below, its real behavior is more than "adds nothing") which sits at
 *  0xc000acc8, only 0x24 bytes before FUN_c000acec - the SAME generic
 *  USB-submit primitive already established as shared by crypto_at88.c's
 *  AtmelRead event, cobjectmgr.c's host-notify event, and cad.c's own
 *  calibration-progress event pump. @0xc0004b88.
 * ------------------------------------------------------------------------- */
bool omap_usbdc_poll_transfer(void *ep, int32_t len)	/* FUN_c0004b88 */
{
	extern int32_t usbdc_transfer_state;		/* DAT_c0004c24 */
	uint8_t *e = (uint8_t *)ep;

	if (usbdc_transfer_state == 1) {
		if ((*(uint16_t *)(e + 0x460) & 1) == 0) {
			extern int32_t usbdc_poll_attempts, usbdc_poll_bound;	/* DAT_c0004c28, DAT_c0004c2c (== 0xf423f) */
			if (usbdc_poll_bound < usbdc_poll_attempts)
				crypto_at88_fault(0, (const char *)0xc0022d68, 0x6aa);
			else
				usbdc_poll_attempts++;
			goto out;
		}
		usbdc_transfer_state = 2;
	} else if (usbdc_transfer_state == 0 && len >= 0x1f41) {
		e[0x401] |= 0x40;	/* DAT_c0004c38, resolved - SAME byte offset as omap_usbdc_init_ep0's 0x21, see note above */
		usbdc_transfer_state = 1;
	}

out:
	return usbdc_transfer_state == 2;
}

/* ------------------------------------------------------------------------- *
 *  FUN_c000acc8 - RESOLVED this pass to be more than a no-op-adding
 *  wrapper. Ghidra's own decompile shows it taking ZERO parameters
 *  ("void FUN_c000acc8(void)") and calling FUN_c0004b88(*DAT_c000ace8) with
 *  a single visible argument - but BOTH of its real call sites disagree:
 *
 *    FUN_c000acec (0xc000ad08): FUN_c000acc8(param_1, 0)
 *    FUN_c0008b64 (0xc0009030): FUN_c000acc8(DAT_c0009098,
 *                    *(int*)(param_1+0x28) + *(int*)(param_1+0x24) * 2000)
 *
 *  Both call sites pass 2 arguments. The real explanation: FUN_c000acc8
 *  never explicitly reads its own first argument (r0) before overwriting
 *  it with the dereferenced global default handle, so Ghidra's per-
 *  function signature analysis dropped it from the visible signature - but
 *  its SECOND argument (r1, `len`) is left untouched all the way through to
 *  the call into FUN_c0004b88, i.e. genuinely forwarded even though
 *  Ghidra's own decompile of FUN_c000acc8 doesn't show it being read. This
 *  is a real, confirmed instance of the same class of issue as this
 *  project's already-documented "phantom forwarded parameter" pattern
 *  (`cdix4192.c`, `eva_board_main.c`) - except inverted: those were
 *  parameters that LOOKED forwarded but weren't; this is a parameter that
 *  IS forwarded but Ghidra's per-function analysis didn't count as one.
 *
 *  Net real behavior: whatever handle argument is passed in is IGNORED -
 *  the function always operates on the single global default USB
 *  endpoint-0 handle (DAT_c000ace8) instead - and `len` is passed straight
 *  through to omap_usbdc_poll_transfer.
 *
 *  This resolves the exact code path this file's own "still open" list
 *  asked about:
 *   - FUN_c000acec (the shared USB-submit primitive) calls this wrapper
 *     FIRST, at entry, with len=0, purely as a READINESS gate: since
 *     len=0 is always below the 8001-byte threshold, this call can only
 *     ever observe (never itself arm) the state machine - it returns true
 *     only if a PRIOR transfer already reached state 2 (complete/idle-
 *     ready), and FUN_c000acec bails out immediately (returns without
 *     submitting) if not.
 *   - FUN_c0008b64 (the master wire-protocol dispatch loop) calls this
 *     SAME wrapper unconditionally, once per tick, at the very end of its
 *     own body, with a REAL, non-zero computed length
 *     (`dev+0x28 + dev+0x24 * 2000`) - this is the actual call that ARMS a
 *     large transfer (drives state 0 -> 1) and advances an in-flight one
 *     (state 1 -> 2) over successive ticks. The length formula itself
 *     (a per-tick multiplying counter times a fixed 2000) suggests a
 *     chunked/streaming transfer whose total size grows by a fixed unit
 *     each tick, but the real meaning of `dev+0x24`/`dev+0x28` themselves
 *     is not traced this pass.
 *  @0xc000acc8.
 * ------------------------------------------------------------------------- */
extern uint32_t usbdc_ep0_default_handle;	/* DAT_c000ace8, the single global default endpoint-0 handle */

bool omap_usbdc_poll_default_ep(void *handle_unused, int32_t len)	/* FUN_c000acc8 */
{
	(void)handle_unused;	/* real argument, but never read - see note above */
	return omap_usbdc_poll_transfer((void *)usbdc_ep0_default_handle, len);
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_object_init - the higher-level USB object bring-up function
 *  this file's own "still open" list flagged as "only partially traced".
 *  Fully decompiled and reconstructed this pass:
 *
 *   - calls FUN_c0001a80 (a function that ALWAYS returns the fixed constant
 *     0x1e00000 regardless of its own input - the SAME "phantom parameter"
 *     shape already documented elsewhere in this project) and stores that
 *     fixed value into the target of a global handle-pointer slot
 *     (`*DAT_c000967c = 0x1e00000`) - seeding a GLOBAL default `dev`
 *     handle with a fixed value BEFORE using it, not something computed
 *     per-call.
 *   - derives 5 more register-block-style pointers via the SAME
 *     omap_usbdc_reloc helper omap_usbdc_init_ep0 uses (offsets +0x200,
 *     +0xe240, +0 (raw), +0x2c00, +0x2a00), stored into 5 separate global
 *     slots - a second, apparently independent set of register-block
 *     derivations from the ones inside omap_usbdc_init_ep0 itself.
 *   - initializes `param_1` (the caller-supplied USB object - a DIFFERENT
 *     object from the global `dev` handle above): stores the incoming
 *     `param_2` into its first field, zeroes 4 more scattered byte fields.
 *   - calls FUN_c0001948 (returns the fixed constant DAT_c0001950 =
 *     0x1c14000 regardless of its own input - ANOTHER phantom-parameter
 *     instance, resolved this pass) to obtain the fixed hardware
 *     register-block base address, then calls
 *     omap_usbdc_init_ep0(*global_dev_handle, 0x1c14000) - confirming
 *     `regs` in omap_usbdc_init_ep0 really is a fixed MMIO base, not a
 *     per-object-computed address.
 *     NOTE: the real call site passes a THIRD argument (`param_2`, this
 *     function's own second parameter) - `FUN_c0003984(*puVar2,uVar6,
 *     param_2)` - but omap_usbdc_init_ep0's own decompiled body only reads
 *     2 parameters. This third argument is genuinely dead at the callee -
 *     confirmed by omap_usbdc_init_ep0's own body never referencing a
 *     third parameter anywhere.
 *   - calls FUN_c0009550 (omap_usbdc_clear_pending_state below) to finish
 *     initializing `param_1`, then zeroes/sets 7 more scattered fields on
 *     it, and finally updates a global status word (`*DAT_c0009694 =
 *     (*DAT_c0009694 & 0xffffffb0) | 0x20`).
 *
 *  CROSS-FILE FINDING (this pass): this function's own sole caller is
 *  `FUN_c00074bc` (0xc0007550) - which `eva_board_main.c` already names
 *  `eva_board_final_setup` and lists as one of ITS OWN still-open items
 *  ("own real role... not traced"). This resolves that: `eva_board_main.c`'s
 *  post-init-table setup call IS the USB device controller bring-up.
 *  @0xc0009574.
 * ------------------------------------------------------------------------- */
extern uint32_t omap_usbdc_phantom_const_a(uint32_t unused);	/* FUN_c0001a80, always returns 0x1e00000 */
extern uint32_t omap_usbdc_ep0_regs_base(uint32_t unused);	/* FUN_c0001948, always returns 0x1c14000 (DAT_c0001950) */

void omap_usbdc_object_init(uint32_t *obj, uint32_t init_arg)	/* FUN_c0009574 */
{
	extern uint32_t *usbdc_global_dev_handle_slot;	/* DAT_c000967c: holds a pointer to the global dev handle */
	extern uint32_t usbdc_reloc_base_b;		/* DAT_c0009678 */
	extern uint32_t usbdc_regblock_d, usbdc_regblock_e, usbdc_regblock_f,	/* DAT_c0009680/84/88 */
			usbdc_regblock_g, usbdc_regblock_h;			/* DAT_c000968c/90 */

	*usbdc_global_dev_handle_slot = omap_usbdc_phantom_const_a(0);	/* always 0x1e00000 */

	usbdc_regblock_d = omap_usbdc_reloc(usbdc_reloc_base_b) + 0x200;
	usbdc_regblock_e = omap_usbdc_reloc(usbdc_reloc_base_b) + 0xe240;
	usbdc_regblock_f = omap_usbdc_reloc(usbdc_reloc_base_b);
	usbdc_regblock_g = omap_usbdc_reloc(usbdc_reloc_base_b) + 0x2c00;
	usbdc_regblock_h = omap_usbdc_reloc(usbdc_reloc_base_b) + 0x2a00;

	obj[0] = init_arg;
	((uint8_t *)obj)[4] = 0;
	((uint8_t *)obj)[6] = 0;
	((uint8_t *)obj)[7] = 0;
	((uint8_t *)obj)[0x58] = 0;

	uint32_t regs_base = omap_usbdc_ep0_regs_base(0);	/* always 0x1c14000 */
	omap_usbdc_init_ep0((void *)(uintptr_t)*usbdc_global_dev_handle_slot, (void *)(uintptr_t)regs_base);
	/* real call also passes `init_arg` as a 3rd argument - confirmed dead,
	 * see comment above */

	extern void omap_usbdc_clear_pending_state(uint32_t *obj);	/* FUN_c0009550 */
	omap_usbdc_clear_pending_state(obj);

	extern uint32_t usbdc_status_word;	/* DAT_c0009694 */
	*(uint16_t *)((uint8_t *)obj + 0x80) = 0;	/* real index (undefined4*)obj+0x20 = +0x80 */
	((uint8_t *)obj)[0x6e] = 1;
	((uint8_t *)obj)[0x7c] = 0;		/* real index (int)obj+0x1f*4 = +0x7c */
	*(uint16_t *)((uint8_t *)obj + 0x7e) = 0;
	((uint8_t *)obj)[0x6f] = 0;
	((uint8_t *)obj)[0x70] = 0;		/* real index obj+0x1c = +0x70 */
	usbdc_status_word = (usbdc_status_word & 0xffffffb0) | 0x20;
	((uint8_t *)obj)[5] = 0;
}

/* ------------------------------------------------------------------------- *
 *  omap_usbdc_clear_pending_state - zeroes a 5-byte run (obj+0x71..0x75)
 *  and sets a 4-byte sentinel field (obj+0x78 = 0xffffffff, plausibly a
 *  "no active transfer"/invalid-handle marker). Called once, from
 *  omap_usbdc_object_init above, right after omap_usbdc_init_ep0 returns.
 *  @0xc0009550.
 * ------------------------------------------------------------------------- */
void omap_usbdc_clear_pending_state(uint32_t *obj)	/* FUN_c0009550 */
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
 * Still genuinely open (narrowed considerably this pass):
 *  - The 5 register-block pointers omap_usbdc_object_init derives
 *    (usbdc_regblock_d..h) - a SECOND, apparently independent set from the
 *    3 omap_usbdc_init_ep0 itself derives; whether these overlap the same
 *    physical peripheral windows isn't traced.
 *  - The two raw (non-pointer) 32-bit constants written into `regs`+0x38/
 *    +0x3c inside omap_usbdc_init_ep0, and the two global-to-global copies
 *    (DAT_c0003b64->b6c, DAT_c0003b70->b74) - real numeric values now
 *    known, but their functional meaning is not decoded.
 *  - `dev+0x24`/`dev+0x28`, the two fields FUN_c0008b64's own per-tick call
 *    into omap_usbdc_poll_default_ep computes `len` from - plausibly a
 *    chunk counter and a base/remaining length for a streamed large
 *    transfer, not confirmed.
 *  - Whether the 8001-byte (0x1f41) threshold is a real hardware DMA/FIFO
 *    limit or a firmware policy choice - now confirmed to be a hardcoded
 *    literal in the instruction stream either way (not DAT_-sourced), but
 *    that doesn't resolve which of the two it is.
 *  - `usbdc_reloc_base` (DAT_c0003b50) vs `usbdc_reloc_base_b`
 *    (DAT_c0009678) - two separately-named relocation-base globals used by
 *    the two different register-block-derivation blocks in this file;
 *    not confirmed whether they hold the same runtime value.
 * ------------------------------------------------------------------------- */
