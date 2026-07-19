/* SPDX-License-Identifier: GPL-2.0 */
/*
 * usbdc_midi_status_glue.c - reconstructs the assigned address range
 * 0xc0006d78-0xc00073e8 (16 real Ghidra function objects at the addresses
 * listed below; 3 of them - FUN_c0007120/FUN_c0007150/FUN_c0007220 - are
 * NOT reconstructed here, see "ALREADY-COVERED ADDRESSES" below). 13
 * functions are genuinely new this file:
 *
 *   Part A (0xc0006d78-0xc00070fc, 11 functions) - a small cluster reading/
 *   writing a shared "channel/USB status" object pair and relaying data out
 *   of midi_engine.c's own ring-2 singleton.
 *   Part B (0xc0007108-0xc0007114, 2 functions) - a pair of USB endpoint
 *   register-block pointer accessors built on the same reloc-base global
 *   omap_l137_usbdc.c/midi_engine.c already document.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json), 2026-07-18 pass. No live Ghidra MCP calls this pass (the
 * bridge is flagged concurrency-unsafe under this project's own parallel
 * work).
 *
 * ANCHOR: NONE. A fresh sweep of all 14 real "../<name>.cpp" __FILE__
 * strings in this image (`query_dump.py strings .cpp`) confirms all 14 are
 * already claimed by other subsystems (crypto_at88.c, i2c_by_gpio.c,
 * clcdc.c, cpsoc.c, ctouchpanel.c, cad.c, mcasp.c, cdix4192.c,
 * eva_board_main.c, cobjectmgr.c, omap_l108.c, omap_l108_spi.c,
 * omap_l137_usbdc.c, McAspHandler.cpp) - none near this range. No
 * fault-call `file` argument anywhere in this range's own functions
 * resolves to any string either (every error/skip path here is a plain
 * early `return`, matching the same structural signature chan_param_ctrl.c/
 * chan_link_hw.c/chan_slot_dispatch.c already documented for their own
 * neighboring, also-unanchored ranges). File named descriptively per this
 * project's established fallback (compare panelbus_dispatch.c/
 * wire_dispatch.c/heap_alloc.c/chan_link_hw.c).
 *
 * ALREADY-COVERED ADDRESSES (present in the assigned sweep, NOT
 * reconstructed here - each is a real Ghidra function starting inside
 * 0xc0006d78-0xc00073e8, already given a body in another file this same
 * pass):
 *   - FUN_c0007120 (cpsoc_i2c_dispatch) - cpsoc.c.
 *   - FUN_c0007150 (cpsoc_drain_queue_wrapper) - cpsoc.c.
 *   - FUN_c0007220 (panelbus_cmd_dispatch, per panelbus_dispatch.c; ALSO
 *     cited under a different name/role, cobjectmgr_secondary_dispatch, as
 *     an extern in cpsoc.c - a real, already-flagged cross-file naming
 *     discrepancy between those two files, not this file's to resolve).
 *     This function's own body (948 bytes) extends past this range's own
 *     upper bound (0xc00073e8) into cad.c's own cad_trigger_calibration
 *     (FUN_c00073e8) - i.e. the two functions are address-adjacent with no
 *     gap, both already owned elsewhere.
 *
 * DECISIVE CROSS-FILE EVIDENCE FOR ATTRIBUTION - three fixed global
 * objects this file's functions touch are independently, exactly
 * identified by sibling files already committed this same pass:
 *
 *   1. DAT_c0009af8 (FUN_c0009a98's own base pointer, see Part A below) =
 *      0xC01CCE50 - the EXACT SAME literal omap_l137_usbdc_ep0.c's own
 *      header names `usbdc_ep0_dev_handle` ("DAT_c000975c and 18 other
 *      aliases, == 0xc01cce50"). This file's Part A functions are real
 *      consumers of the USB0/MUSB device-controller object.
 *   2. DAT_c0006da8/DAT_c0006f34 = 0xC01CACC0 - the EXACT SAME literal
 *      omap_l137_usbdc_ep0.c's own header names `usbdc_ep0_ctx_handle`
 *      (its EP0 SETUP-request context object, DAT_c0009098, "confirmed to
 *      resolve to 0xc01cacc0"). Both this file's own uses of the constant
 *      are DEAD arguments to callees that ignore whatever they're handed
 *      (see each function's own note below) - so this is evidence of
 *      *association* with the EP0 request path, not a live field access.
 *   3. DAT_c0006f30 = 0xC01CAD44 - the EXACT SAME literal midi_engine.c's
 *      own header names as its ring-2/context singleton ("DAT_c0007654,
 *      resolved literal 0xC01CAD44 - a real, always-populated pointer
 *      literal"). This file's FUN_c0006e90 (Part A) is a direct,
 *      cross-file caller of midi_engine.c's own midi_ring2_pop_copy/
 *      midi_ring2_is_empty on that exact singleton.
 *   4. DAT_c000ebdc/DAT_c000ec08 (Part B) = 0xC01CCB10 - the EXACT SAME
 *      literal omap_l137_usbdc.c's own header names `usbdc_reloc_base`
 *      (DAT_c0003b50) and midi_engine.c independently re-derives as
 *      DAT_c000ce44 ("both literals resolving to 0xC01CCB10").
 *
 * Taken together: this file sits physically between cobjectmgr.c/
 * panelbus_dispatch.c's own low-address functions and midi_engine.c's own
 * range, and its code is a real, concrete bridge between the USB0 device
 * controller object, its EP0 request-context object, and midi_engine.c's
 * ring-2 singleton - a "USB<->MIDI status glue" cluster, not a guess.
 *
 * PHANTOM-FORWARDED-ARGUMENT PATTERN (this project's own well-established
 * finding, e.g. cdix4192.c/eva_board_main.c/cpsoc.c/panelbus_dispatch.c):
 * every dead first argument below is transcribed as a literal 0, with a
 * trailing comment citing the real DAT_ symbol and resolved address,
 * rather than invented as a real, used parameter, matching house style.
 *
 * STILL OPEN / NEEDS LIVE QUERY:
 *   - FUN_c0006d78's own caller list (per all_decompiled.json's raw
 *     xrefs_to) includes a call site at 0xc0006e80 attributed to
 *     "FUN_c000a980" - but 0xc0006e80 falls in the address gap between
 *     THIS file's own FUN_c0006d78 (ends 0xc0006da8) and FUN_c0006e90
 *     (starts 0xc0006e90), with no Ghidra function boundary covering it in
 *     the static dump, and FUN_c000a980 (chan_param_ctrl.c's own cited
 *     usbdc_ep_state7_handler) is a much higher address (0xc000a980) that
 *     cannot itself contain a call site at 0xc0006e80. This looks like
 *     either a stale/incorrect xref-to-function attribution in the static
 *     dump, or a genuine unboxed code fragment Ghidra never gave its own
 *     function object - NOT resolved here, no live Ghidra access this
 *     pass. Whatever real code sits at 0xc0006e80 is NOT reconstructed in
 *     this file (out of the assigned range's own function list either
 *     way).
 *   - FUN_c000701c/FUN_c0007028/FUN_c0007078's own single call sites
 *     (0xc0000760/0xc0000748/0xc0000754 respectively, all `from_func:
 *     null` in the raw dump, 12 bytes apart, in a gap with no Ghidra
 *     function object covering them either) - looks like a small 3-entry
 *     init/table region just above FUN_c0000784, out of this file's own
 *     assigned range and not investigated further here.
 *   - The exact IRQ/status bit semantics FUN_c0009a98/FUN_c0009afc encode
 *     (base bit 0x200 or 2, shifted by index-1) are mechanically
 *     transcribed in this file's own comments but not matched against any
 *     public MUSB register name - plausible candidates (per-endpoint
 *     TX/RX interrupt enable) are noted but not asserted as fact.
 */

#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------
 * Cross-file externs - declared here, defined elsewhere; none of the
 * following files are edited by this one.
 * --------------------------------------------------------------------- */

/* omap_l137_usbdc_ep0.c - USB0/EP0 device object. FUN_c0009a98/_afc's own
 * base pointer (DAT_c0009af8/DAT_c0009b64 respectively) resolves to this
 * SAME literal (0xC01CCE50) - see header point 1. Out of this file's own
 * range (0xc0009a98/0xc0009afc), not reconstructed here. */
extern void usbdc_ep_irqmask_set(void *unused_handle, uint8_t line, uint8_t which);	/* FUN_c0009a98 */
extern void usbdc_ep_irqmask_clear(void *unused_handle, uint8_t line, uint8_t which);	/* FUN_c0009afc */

/* A small "read one status bit" helper: resolves its own base through
 * FUN_c0001a80 (always returns the fixed literal 0x1e00000 regardless of
 * argument - the same "phantom forwarded parameter" pattern used
 * throughout this project, see i2c_by_gpio.c/cdix4192.c/eva_board_main.c/
 * midi_engine.c's own header for prior instances of this exact shape),
 * then tests bit 4 of a byte at that fixed base plus DAT_c0009b94. Out of
 * this file's own range (0xc0009b68), not reconstructed here. Genuinely
 * takes NO real argument in Ghidra's own recovered signature - the calls
 * below that pass one are themselves phantom-argument call sites. */
extern uint8_t usbdc_status_bit4(void);	/* FUN_c0009b68 */

/* midi_engine.c - ring-2 singleton consumers. Declared locally here
 * (same convention this project's other files already use for shared
 * cross-file helpers, e.g. cpsoc.c/panelbus_dispatch.c re-declaring
 * irq_save_and_disable/irq_restore locally rather than via a shared
 * header) - defined in midi_engine.c, NOT edited by this file. */
extern uint32_t midi_ring2_pop_copy(uint32_t *ctx, char *out);	/* FUN_c000d0fc, midi_engine.c */
extern int       midi_ring2_is_empty(uint32_t *ctx);			/* FUN_c000d600, midi_engine.c */

/* A raw MIDI-byte-stream scanner/relay - real signature recovered
 * directly from its own decompile (char *param_1, byte *param_2), tests
 * param_1's first byte for a null terminator and scans param_2 for the
 * SysEx EOX byte (0xf7). Address 0xc00057ec is far below this file's own
 * assigned range and outside every other subsystem file's own documented
 * range too - cited here only, not reconstructed. */
extern uint32_t chan_byte_stream_relay(char *param_1, uint8_t *param_2);	/* FUN_c00057ec */

/* omap_l137_usbdc.c - the shared USB reloc-base global. Re-derived here
 * under a locally-scoped extern, same convention as point 4 above. */
extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c0009194, omap_l137_usbdc.c */

/* ===========================================================================
 * PART A - channel/USB status glue (0xc0006d78-0xc00070fc)
 * ===========================================================================
 *
 * Two fixed global objects dominate this section, both resolved via
 * query_dump.py's `dat` lookup on every DAT_ constant this section's real
 * decompile references:
 *
 *   - 0xC01CC12C ("chan_status_obj" below) - read/written by FUN_c0006e90,
 *     FUN_c00070a4, FUN_c0007028 (forwarded to the out-of-range
 *     FUN_c00103e4), and FUN_c0007078 (forwarded to the out-of-range
 *     FUN_c000fe20). A single shared status/flag object touched by nearly
 *     every function in this section.
 *   - 0xC01CC0F4 ("chan_ring_obj" below) - the ring/counter object
 *     FUN_c0006fa8 reads and decrements at its own +0x10 field, and the
 *     SAME object the 4 trivial "clear a ushort" stubs
 *     (FUN_c00070d8/_e4/_f0/_fc) target at offsets +4/+0x10/+0x1c/+0x28 -
 *     a confirmed, exact structural match at +0x10 (FUN_c00070e4 clears
 *     the identical field FUN_c0006fa8 decrements), strong evidence these
 *     4 stubs and FUN_c0006fa8 share one real object with (at least) 4
 *     entries at stride 0xc starting at offset 4.
 * ========================================================================= */

extern uint32_t chan_status_obj;	/* DAT_c0006f3c/DAT_c00070c4/DAT_c0007070/DAT_c0007080, all == 0xC01CC12C */
extern uint32_t chan_ring_obj;		/* DAT_c0006fb4/DAT_c00070e0/DAT_c00070ec/DAT_c00070f8/DAT_c0007104, all == 0xC01CC0F4 */

/* chan_irq_toggle - dispatches to usbdc_ep_irqmask_set/_clear based on
 * `enable`. `unused_ctx` is a real, confirmed-dead argument: the real
 * callees (FUN_c0009a98/FUN_c0009afc) never reference their own first
 * parameter anywhere in their bodies - only a FIXED global
 * (usbdc_ep0_dev_handle, see header point 1). Every caller of THIS
 * function that this pass could trace (FUN_c0007028 below, plus 3
 * out-of-range callers - FUN_c000fe20/FUN_c000d6fc/an unresolved gap
 * caller, see "STILL OPEN" above) passes DAT_c0006da8 = 0xC01CACC0
 * (usbdc_ep0_ctx_handle, header point 2) as this dead argument - i.e. two
 * independent call sites in this file (here and FUN_c0006e90's own dead
 * call to usbdc_status_bit4 below) both reuse the SAME EP0-context
 * constant as a throwaway argument, not resolved further than "probably a
 * leftover live-register value from a shared calling convention, not a
 * meaningful parameter." @0xc0006d78. */
void chan_irq_toggle(void *unused_ctx, uint8_t line, uint8_t which, bool enable)	/* FUN_c0006d78 */
{
	if (enable) {
		usbdc_ep_irqmask_set(0 /* DAT_c0006da8 = 0xC01CACC0, usbdc_ep0_ctx_handle - dead arg */, line, which);
		return;
	}
	usbdc_ep_irqmask_clear(0 /* DAT_c0006da8 = 0xC01CACC0, dead arg, see above */, line, which);
}

/* chan_ring2_relay_and_status - pops one record from midi_engine.c's own
 * ring-2 singleton (0xC01CAD44, header point 3) into the caller-supplied
 * `out` buffer, then relays additional raw bytes through
 * chan_byte_stream_relay for up to 16 more 4-byte groups (bounded by both
 * an iteration count and a byte-offset ceiling of 0x3f), and finally
 * reports readiness: 0 (busy) if ring-2 still has pending data, otherwise
 * the sign of `chan_status_obj+0x61c` collapsed to a plain boolean (see
 * derivation below).
 *
 * PHANTOM-FORWARDED-ARGUMENT: the real decompile calls
 * `FUN_c000d0fc(DAT_c0006f30)` with only ONE visible argument, but
 * midi_ring2_pop_copy's own real signature (midi_engine.c, FUN_c000d0fc)
 * takes TWO (`ctx`, `out`) - and the very next line reassigns this
 * function's own `out` parameter as `out = out + return_value`, i.e. the
 * caller-supplied `out` pointer is silently forwarded through an
 * untouched register as midi_ring2_pop_copy's own second argument. Same
 * "first arg dead / real second arg rides through unchanged" idiom this
 * project has repeatedly documented (cpsoc_drain_queue_wrapper,
 * cpsoc_i2c_dispatch, cdix4192.c, eva_board_main.c).
 *
 * `FUN_c0009b68(DAT_c0006f34)` (usbdc_status_bit4 above) is called with
 * its OWN return value entirely unused - a confirmed dead call, same
 * pattern cpsoc.c's own "fetched but its result is UNUSED" note
 * documents for cpsoc_get_scan_handle.
 *
 * FINAL READINESS CHECK - collapsed from the real decompile's raw form:
 *
 *     iVar3 = 1 - (uint)*(ushort *)(chan_status_obj + 0x61c);
 *     if (1 < *(ushort *)(chan_status_obj + 0x61c)) iVar3 = 0;
 *
 *   which is exactly `(field == 0) ? 1 : 0` for field in {0,1,2,...}: field
 *   0 -> 1; field 1 -> 1-1=0; field >=2 -> forced 0 by the second line.
 *   Verified by direct enumeration (field 0..2 covers every branch), not
 *   guessed. @0xc0006e90. */
int chan_ring2_relay_and_status(uint32_t handle, char *out, uint32_t *out_len)	/* FUN_c0006e90 */
{
	uint32_t n;
	int iterations;
	uint16_t *status_field;

	n = midi_ring2_pop_copy((uint32_t *)0 /* DAT_c0006f30 = 0xC01CAD44, midi_engine.c's ring-2 singleton */, out);
	(void)usbdc_status_bit4();	/* DAT_c0006f34 = 0xC01CACC0, dead call - result unused, real decompile confirms */
	out = out + n;

	if (n < 0x3d) {
		iterations = 0;
		for (;;) {
			uint32_t keep_going = chan_byte_stream_relay((char *)handle, (uint8_t *)out);
			iterations = iterations + 1;
			if (keep_going == 0)
				break;
			n = n + 4;
			out = out + 4;
			if (n > 0x3f || iterations > 0xf)
				break;
		}
	}

	*out_len = n;

	if (!midi_ring2_is_empty((uint32_t *)0 /* DAT_c0006f30, same singleton */))
		return 0;

	/* field == 0 ? 1 : 0, see derivation above */
	status_field = (uint16_t *)((uint8_t *)&chan_status_obj + 0x61c);
	return (*status_field == 0) ? 1 : 0;
}

/* chan_ring_drain_pack - fills `out` with up to 48 3-byte groups (6 bytes
 * per outer-loop iteration, matching the `iVar12 -= 6` stride) drained
 * from a table at `*chan_tx_slot_count_ptr + slot*8 + word*4` (2
 * consecutive 4-byte words per iteration), OR, when `chan_ring_obj`'s own
 * +0x14 flag byte is set (or its +0x10 counter has already reached 0),
 * writes zero-filled 3-byte groups instead - a real, transcribed-as-found
 * branch, not simplified into one path.
 *
 * The iteration-count expression `iVar12` collapses cleanly by direct
 * enumeration of the ushort domain (verified in Python, not guessed):
 * with `cnt = chan_ring_obj+0x10` treated as a small counter,
 * `sign(cnt - 0x36)*6 + 0x120` is EXACTLY what the raw SCARRY4/SBORROW4
 * expression computes for every cnt in [0, 0x300) - i.e. iterations start
 * at 48 (0x120/6) and shift +-1 depending on whether cnt is below/above
 * 0x36 (54 decimal). Likewise the ring-index wraparound at
 * `chan_ring_obj+0xe` collapses exactly to `(idx + 1) % 0x300` (0x300 =
 * 768 decimal, consistent with a 256-slot x 3-byte ring) - also verified
 * by direct enumeration of the ushort domain.
 *
 * Each non-zero 4-byte source word is unpacked as 3 output bytes (low,
 * bits 8-15, bits 16-23), discarding the top byte (bits 24-31) of every
 * source word - mechanically transcribed exactly as decompiled. Whether
 * the discarded top byte is a USB-MIDI Cable/CIN byte, and what the
 * source table's own producer/consumer contract actually is, is NOT
 * conclusively determined this pass - flagged, not guessed.
 *
 * SOLE CALLER (confirmed via xrefs_to): FUN_c000c39c
 * (chan_slot_dispatch.c's own documented "port interrupt dispatcher",
 * confirmed there independently) - calls this with
 * `chan_ring_drain_pack(DAT_c0000de8, *DAT_c0000de4, 8)`; this function's
 * own real signature only has TWO parameters (`ctx`, `out`) per Ghidra's
 * recovered prototype - the caller's third visible argument (8) is
 * another phantom-forwarded value with no matching parameter here, not
 * fabricated into a third parameter. @0xc0006fa8. */
uint32_t chan_ring_drain_pack(uint32_t ctx, uint8_t *out)	/* FUN_c0006fa8 */
{
	uint8_t *base = (uint8_t *)&chan_ring_obj;
	uint16_t *idx_field   = (uint16_t *)(base + 0xe);	/* ring index, wraps at 0x300 */
	int16_t  *cnt_field   = (int16_t  *)(base + 0x10);	/* small signed counter */
	int8_t   *flag_field  = (int8_t   *)(base + 0x14);	/* mode/flush flag */
	int iterations, ret;
	uint8_t *p = out;

	(void)ctx;	/* real callee never references its own first argument either */

	if (*flag_field == 0) {
		int sign = (*cnt_field > 0x36) - (*cnt_field < 0x36);	/* verified sign(cnt-0x36), see derivation above */
		ret = sign * 6 + 0x120;
	} else {
		ret = 0x120;
	}
	iterations = ret;

	for (; iterations > 5; iterations -= 6) {
		uint16_t slot = *idx_field;
		extern uint32_t *chan_tx_slot_table_ptr;	/* DAT_c000f574, resolved 0xC01CD4C8 - a further pointer, dereferenced at runtime */

		if (*cnt_field == 0 || *flag_field != 0) {
			int n = *(int *)chan_tx_slot_table_ptr;	/* real dynamic global, not a compile-time constant */
			do {
				n = n - 1;
				p[0] = 0;
				p[1] = 0;
				p[2] = 0;
				p += 3;
			} while (n >= 0);
		} else {
			int word;
			for (word = 0; word < 2; word++) {
				uint32_t w = *(uint32_t *)((uint8_t *)chan_tx_slot_table_ptr + slot * 8 + word * 4);
				p[0] = (uint8_t)(w);
				p[1] = (uint8_t)(w >> 8);
				p[2] = (uint8_t)(w >> 16);
				p += 3;
			}
			*idx_field = (uint16_t)((*idx_field + 1) % 0x300);	/* verified wraparound, see derivation above */
			(*cnt_field)--;
		}
	}
	return (uint32_t)ret;
}

/* chan_status_promote_on_flag - if a caller-selected object (indexed 0/1/
 * other via the out-of-range FUN_c0001a38 selector, itself already used
 * by 9 other call sites project-wide, e.g. FUN_c00103e4's own entry
 * sequence below) has bit 0x80 set at its own +0x14 field, forces its own
 * +8 field to 3. `handle` (the incoming `DAT_c00102d4` argument, ==
 * 0xC00E0068, the "shared context handle" this project's own README
 * documents repeatedly, e.g. cpsoc.c's cpsoc_get_scan_handle) is passed
 * straight through unmodified to the selector - a real, meaningful
 * forward this time, not a phantom (FUN_c0001a38 does use its own
 * `param_2` selector argument, just not `param_1`/handle, per its own
 * decompile: `if(param_2==0) return DAT_c0001a5c; ... `). Real caller list
 * (per xrefs_to) is a single call site at 0xc0000760 with no containing
 * function object in the static dump - see "STILL OPEN" above.
 * @0xc000701c. */
extern void *chan_selector_object(void *handle, int selector);	/* FUN_c0001a38, out of range */

void chan_status_promote_on_flag(void)	/* FUN_c000701c */
{
	uint8_t *obj = (uint8_t *)chan_selector_object(0 /* DAT_c00102d4 = 0xC00E0068, shared context handle */, 1);

	if (*(uint32_t *)(obj + 0x14) & 0x80)
		*(uint32_t *)(obj + 8) = 3;
}

/* chan_maybe_enable_irq4 - if the out-of-range FUN_c00103e4 (a 296-byte
 * dispatcher, itself starting with the exact same
 * `chan_selector_object(chan_status_obj, 1)` idiom as
 * chan_status_promote_on_flag above) reports non-zero AND
 * `*chan_status_obj_ptr2` is non-zero, calls chan_irq_toggle with the
 * fixed arguments (4, 1, enable=1) - the SAME literal channel index (4)
 * midi_engine.c's own header independently documents for
 * midi_hw_channel_active ("calls FUN_c000cc60(DAT_c0006d68, 4)") - a
 * plausible but NOT confirmed link between the two "channel 4" constants
 * (different callees, same numeric coincidence only). @0xc0007028. */
extern uint32_t chan_dispatch_probe(uint32_t handle);	/* FUN_c00103e4, out of range */

void chan_maybe_enable_irq4(uint32_t handle)	/* FUN_c0007028 */
{
	extern uint8_t chan_status_obj_ptr2;	/* DAT_c0007074 = 0xC01CC748 */

	if (chan_dispatch_probe(0 /* DAT_c0007070 = 0xC01CC12C, chan_status_obj */) == 0)
		return;
	if (chan_status_obj_ptr2 == 0)
		return;
	chan_irq_toggle((void *)0, 4, 1, true);
}

/* chan_status_notify - trivial single-call forwarder into the
 * out-of-range FUN_c000fe20 (568 bytes, also one of FUN_c0006d78's own
 * out-of-range callers per its own caller list). Sole call site: 0xc0000754,
 * same unresolved 3-entry gap as chan_status_promote_on_flag/
 * chan_maybe_enable_irq4's own siblings, see "STILL OPEN" above.
 * @0xc0007078. */
extern void chan_status_dispatch(void *obj);	/* FUN_c000fe20, out of range, 568 bytes */

void chan_status_notify(void)	/* FUN_c0007078 */
{
	chan_status_dispatch((void *)0 /* DAT_c0007080 = 0xC01CC12C, chan_status_obj */);
}

/* chan_status_byte_msb - reads a byte field at chan_status_obj+0x409 and
 * reports whether its top bit (0x80) is set. Collapsed from the real
 * SBORROW4/SCARRY4-laden decompile by direct enumeration of the full byte
 * domain (0-255) in Python - every value maps to exactly `(b & 0x80) !=
 * 0`, verified, not guessed. Given the 0x80-test shape and this section's
 * broader MIDI-adjacent evidence, plausibly a "is this a MIDI status
 * byte" check, but that specific interpretation is NOT confirmed - the
 * bit-test itself is. Sole caller (per xrefs_to): FUN_c000d6fc, out of
 * this file's own assigned range (0xc000d6fc sits in the real gap between
 * midi_engine.c's own range end and chan_param_ctrl.c's own range start -
 * neither file's own assigned sweep covers it either, per both files' own
 * header notes). @0xc00070a4. */
bool chan_status_byte_msb(void)	/* FUN_c00070a4 */
{
	uint8_t *field = (uint8_t *)&chan_status_obj + 0x409;
	return (*field & 0x80) != 0;
}

/* chan_ring_entry_clear_{0..3} - a confirmed quartet: each zeroes one
 * ushort field of chan_ring_obj at stride-0xc offsets 4/0x10/0x1c/0x28.
 * FUN_c00070e4's own target (+0x10) is the EXACT SAME field
 * chan_ring_drain_pack (FUN_c0006fa8 above) reads/decrements as its own
 * `cnt_field` - a confirmed, address-exact structural link, not a guess.
 * Each has exactly one caller: FUN_c000a45c (entries 0 and 2, offsets 4
 * and 0x1c) and FUN_c000e498 (entries 1 and 3, offsets 0x10 and 0x28) -
 * both out of this file's own assigned range, not investigated further
 * here (same "trivial zero-a-global stub cluster" pattern clcdc.c's own
 * README section already documents for 3 similar functions in its own
 * neighborhood). @0xc00070d8/_e4/_f0/_fc respectively. */
void chan_ring_entry_clear_0(void)	/* FUN_c00070d8 */
{
	*(uint16_t *)((uint8_t *)&chan_ring_obj + 4) = 0;
}

void chan_ring_entry_clear_1(void)	/* FUN_c00070e4 */
{
	*(uint16_t *)((uint8_t *)&chan_ring_obj + 0x10) = 0;
}

void chan_ring_entry_clear_2(void)	/* FUN_c00070f0 */
{
	*(uint16_t *)((uint8_t *)&chan_ring_obj + 0x1c) = 0;
}

void chan_ring_entry_clear_3(void)	/* FUN_c00070fc */
{
	*(uint16_t *)((uint8_t *)&chan_ring_obj + 0x28) = 0;
}

/* ===========================================================================
 * PART B - USB endpoint register-block pointer helpers (0xc0007108-0xc0007114)
 * ===========================================================================
 *
 * Both compute `omap_usbdc_reloc(usbdc_reloc_base) + index*stride + fixed`
 * - the SAME `usbdc_reloc_base` global (0xC01CCB10, header point 4)
 * omap_l137_usbdc.c's own omap_usbdc_init_ep0 and midi_engine.c's own
 * eva_board_usb_ctx_b_init both independently derive their own register-
 * block/ring-table pointers from. Confirmed via a real caller
 * (FUN_c0000484, decompiled directly this pass): `FUN_c0007108(base, 0)`
 * is used as a byte-address BASE for a fill-level computation
 * (`*puVar2 - FUN_c0007108(...)`, then divided by 0xc0 - the exact same
 * stride this function itself uses), i.e. these really do return memory
 * addresses of a per-index register/buffer block, not scalar values.
 * `unused` (the incoming first argument, forwarded from `param_1`/
 * `DAT_c0000568` etc at every real call site) is confirmed dead: both
 * DAT_c000ebdc and DAT_c000ec08 resolve to the identical fixed literal
 * 0xC01CCB10 regardless of what the caller passes. */

/* usbdc_ep_regblock_ptr_a - reloc_base + index*0xc0 + 0x400. 8 confirmed
 * callers (FUN_c0000484 x2, FUN_c0004d74 x3, FUN_c00050cc x3), all in the
 * 0xc0000400-0xc0005400 USB endpoint bring-up address neighborhood
 * cpsoc.c's own "confirmed out of scope" sweep note already cites for
 * this exact function - reproduced here as this function's real home.
 * @0xc0007108. */
uint32_t usbdc_ep_regblock_ptr_a(uint32_t unused, int index)	/* FUN_c0007108 */
{
	(void)unused;
	return omap_usbdc_reloc(0 /* DAT_c000ebdc = 0xC01CCB10, usbdc_reloc_base */) + (uint32_t)index * 0xc0 + 0x400;
}

/* usbdc_ep_regblock_ptr_b - reloc_base + index*0x60 + 0x1c00. Same
 * reloc-base global as usbdc_ep_regblock_ptr_a above (independently
 * re-derived via a separate DAT_ symbol, DAT_c000ec08, both == the same
 * 0xC01CCB10 literal). 6 confirmed callers, all FUN_c0004d74/FUN_c00050cc
 * - the same two functions that call usbdc_ep_regblock_ptr_a, consistent
 * with these being two related per-endpoint register-block accessors used
 * side by side. @0xc0007114. */
uint32_t usbdc_ep_regblock_ptr_b(uint32_t unused, int index)	/* FUN_c0007114 */
{
	(void)unused;
	return omap_usbdc_reloc(0 /* DAT_c000ec08 = 0xC01CCB10, usbdc_reloc_base */) + (uint32_t)index * 0x60 + 0x1c00;
}
