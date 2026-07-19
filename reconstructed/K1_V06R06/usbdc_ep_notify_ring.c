/* SPDX-License-Identifier: GPL-2.0 */
/*
 * usbdc_ep_notify_ring.c - reconstructs two assigned address ranges:
 *
 *   Section A: 0xc000618c-0xc0006578 (9 functions)
 *   Section B: 0xc0006790-0xc0006b74 (9 functions)
 *
 * 18 functions total. The gap between the two ranges (0xc0006578-
 * 0xc0006790, containing FUN_c0006578 itself, 484 bytes, plus ~52 bytes of
 * trailing unaccounted space) is NOT part of either assigned range and is
 * NOT reconstructed here.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json, 2026-07-18 pass) plus this pass's own DAT_ resolution via
 * query_dump.py dat. No live Ghidra MCP calls made this pass (bridge
 * concurrency caveat per project README; also unnecessary here - the
 * static dump fully resolved every literal below).
 *
 * ANCHOR: NONE. Re-swept all 14 real "../<name>.cpp" __FILE__ strings via
 * `query_dump.py strings cpp` - all 14 remain claimed by other subsystems
 * (crypto_at88.c, i2c_by_gpio.c, clcdc.c, cpsoc.c, ctouchpanel.c, cad.c,
 * mcasp.c, cdix4192.c, eva_board_main.c, cobjectmgr.c, omap_l108.c,
 * omap_l108_spi.c, omap_l137_usbdc.c, McAspHandler.cpp). None of this
 * range's 18 functions calls any fault/assert-style primitive with a file-
 * string argument either (every function here is a plain leaf/near-leaf
 * struct accessor - no error path exists to carry a __FILE__ literal).
 * Same "no anchor" situation already documented by usbdc_midi_status_glue.c/
 * panelbus_dispatch.c/wire_dispatch.c/heap_alloc.c/chan_link_hw.c for their
 * own neighboring ranges - file named descriptively per that established
 * fallback.
 *
 * WHY THIS ISN'T MERGED INTO AN EXISTING FILE: overwhelming code-shape and
 * cross-file address evidence (below) ties this cluster to the SAME
 * hardware/software layer omap_l137_usbdc.c/_ep0.c/_ext.c already own
 * (their real anchor, "../MCU/Component/OmapL137Usbdc.cpp") plus
 * midi_chan_status_queues.c's chan_status_obj and midi_engine.c's ring
 * singleton. Per this pass's instructions, functions that genuinely belong
 * in an existing file are cited as cross-file follow-up, NOT used to
 * justify editing that file - hence a new file, not an edit to
 * omap_l137_usbdc_ext.c or midi_chan_status_queues.c.
 *
 * DECISIVE CROSS-FILE EVIDENCE:
 *   1. FUN_c00064ac and FUN_c0006a04 (both reconstructed below) are
 *      ALREADY named and declared (as bare externs, never given a body)
 *      by omap_l137_usbdc_ext.c: `usbdc_ep0_notify_helper2` and
 *      `usbdc_ep0_notify_helper` respectively. Names reused verbatim
 *      below. See "ARGUMENT-COUNT CORRECTION" further down - both of that
 *      file's own extern signatures over-count arguments by one.
 *   2. FUN_c0006a78 (reconstructed below as usb_midi_packet_decode) is
 *      ALREADY extensively analyzed - but never given a body - by
 *      midi_chan_status_queues.c's own header ("OUT OF RANGE" section),
 *      which independently derived its exact USB-MIDI CIN dispatch table
 *      from that function's own decompile. This file's reconstruction
 *      matches that analysis exactly (see the function's own note) -
 *      cross-confirmed, not re-derived from scratch.
 *   3. DAT_c0006b4c (FUN_c0006a78's own handle argument to the midi_txq_*
 *      family) = 0xC01CC12C, the exact literal midi_chan_status_queues.c
 *      names `chan_status_obj`.
 *   4. DAT_c0006b70 (FUN_c0006b50's own argument) = 0xC01CAD44, the exact
 *      literal midi_engine.c names as its context singleton (DAT_c0007654).
 *      FUN_c0006b50 is a thin wrapper around midi_engine.c's own
 *      midi_ring1_has_space (FUN_c000d5d0, already fully reconstructed
 *      there) - reused by name, not re-declared as unresolved.
 *   5. DAT_c0006558/DAT_c0006a68 (FUN_c00064ac/FUN_c0006a04) = 0xC01CACC0,
 *      the exact literal omap_l137_usbdc_ep0.c names `usbdc_ep0_ctx_handle`.
 *   6. DAT_c0006974/DAT_c000655c (FUN_c00068fc/FUN_c00064ac) = 0xC01CC750,
 *      a NEW handle this file introduces (not previously named anywhere) -
 *      confirmed to be a real handle-to-handle (pointer-to-pointer) object
 *      because FUN_c00068fc passes it directly to midi_engine.c's own
 *      midi_hw_channel_ready(uint32_t *handle, int channel) (FUN_c000cc94)
 *      and chan_slot_dispatch.c's own chan_link_tx(uint32_t target, ...)
 *      (FUN_c000c1f0) - both already-reconstructed, reused by name below.
 *      Named `chan_link_ctx_handle` here.
 *
 * ARGUMENT-COUNT CORRECTION (not editing the other file, flagged here per
 * this pass's instructions): omap_l137_usbdc_ext.c declares
 *   extern uint32_t usbdc_ep0_notify_helper(void *handle, void *ctx, uint32_t len);
 *   extern void usbdc_ep0_notify_helper2(void *handle, void *ctx, uint32_t len, uint32_t param3);
 * i.e. 3 and 4 formal parameters respectively. The REAL Ghidra-recovered
 * signatures of FUN_c0006a04 and FUN_c00064ac (this file, below) are
 *   void FUN_c0006a04(undefined4 param_1, undefined4 param_2)          -- 2 params
 *   void FUN_c00064ac(undefined4 param_1, undefined4 param_2, undefined4 param_3) -- 3 params
 * Both real callees ignore param_1 entirely (dead handle - the actual
 * object is the fixed global literal each function reads directly, same
 * "dead handle, real object is a fixed global" pattern midi_engine.c's own
 * header already names). The caller (usbdc_endpoint_event_dispatch,
 * FUN_c000aae0) supplies one MORE argument at each call site than either
 * real callee has formal parameters for - the trailing extra argument is
 * simply dropped by the callee, exactly the "phantom-forwarded-argument"
 * pattern this project has repeatedly documented (cdix4192.c,
 * eva_board_main.c, cpsoc.c, panelbus_dispatch.c, and now here). Not fixed
 * in omap_l137_usbdc_ext.c (out of this file's own scope this pass).
 *
 * NAMING CORRECTION for `usbdc_ep_state_notify` (omap_l137_usbdc_ext.c's
 * own extern for FUN_c0006578/_858/_69ac/_988, "out of range"): having now
 * reconstructed 3 of those 4 real bodies (FUN_c0006578 itself remains out
 * of either of my own ranges), they are NOT interchangeable copies of one
 * "notify" operation as that file's own naming implies - they are three
 * functions with genuinely different bodies (FUN_c0006858 does a real
 * record-select-and-push through usbdc_notify_code_select2 +
 * usbdc_ep0_ctx_notify's own FUN_c0009890 call; FUN_c00069ac is a bare
 * 1-line forwarder to a completely unrelated link-status poll
 * (midi_link_status_poll, FUN_c00062f0); FUN_c0006988 is a small
 * threshold-gated up-counter with no notify-style side effect at all).
 * They merely share a call-site SHAPE (`usbdc_ep_state_notify(0, code)`,
 * same phantom-handle pattern as above) from the caller's point of view -
 * not a shared implementation. Flagged here, not corrected there.
 *
 * NEW GLOBALS THIS FILE RESOLVES (none previously named anywhere in the
 * project - all independently cross-confirmed by at least 2 of this
 * file's own functions using the identical literal):
 *   - usbdc_notify_ring[4]      @ 0xC01CCA48  (DAT_c00063fc/_c0006574/_c0006a70)
 *     4-entry uint32_t ring; producer usbdc_ep0_notify_helper2 (below),
 *     consumer usbdc_ep0_notify_helper (below). Init-filled with the
 *     literal 0x6c (108) by usbdc_ep_state_init.
 *   - usbdc_notify_ring_widx    @ 0xC01CCA94  (DAT_c00063d4/_c000656c)
 *     producer-side write index, mod 4.
 *   - usbdc_notify_ring_ridx    @ 0xC01CCA44  (DAT_c00063f8/_c0006570/_c0006a6c)
 *     consumer-side read index, mod 4 - ALSO zeroed by the producer's own
 *     flag-clear branch (see usbdc_ep0_notify_helper2's own note on the
 *     asymmetric 0-vs-1 reset values, transcribed faithfully, not
 *     "corrected").
 *   - usbdc_notify_pending_flag @ 0xC0098F64  (DAT_c00063f4/_c0006568/_c0006a64)
 *     byte(?) flag gating both producer and consumer paths - see WIDTH
 *     NOTE below.
 *   - usbdc_notify_scaled_accum @ 0xC01CCA88  (DAT_c0006408/_c0006564)
 *     running sum of omap_tick_scale(len, 0x12) results.
 *   - chan_link_ctx_handle      @ 0xC01CC750  (DAT_c000655c/_c0006974) - see
 *     item 6 above.
 *
 * WIDTH NOTE: usbdc_notify_pending_flag is WRITTEN as a 4-byte store in
 * usbdc_ep_state_init (`*DAT_c00063f4 = 1`, Ghidra's undefined4 default for
 * an unrecognized-width store) but READ as a single byte (`*(char *)DAT_...`)
 * in both usbdc_ep0_notify_helper2 and usbdc_ep0_notify_helper. Transcribed
 * faithfully (word store / byte loads) rather than forced to one width -
 * genuinely open whether the wider store is intentionally clearing
 * adjacent fields too or is simply how the compiler emitted a small
 * constant store here.
 *
 * STILL OPEN:
 *   - The object at 0xC01CC0F4 (DAT_c000630c/_c0006560/_c0006840/_c00069dc/
 *     _c0006a74), passed to 4 different out-of-range helpers (FUN_c000f230/
 *     _f59c/_ec0c/_f69c) from functions in this file, is NOT independently
 *     named/resolved anywhere else in the project. Its accessed field
 *     offsets (+0x10/+0x14, +0x1a, +0x1c, +0x20, +0x24, +0x26, +0x28,
 *     +0x2c, per those 4 callees' own decompiles) resemble a MIDI/USB
 *     framing-and-rate object, but this is NOT independently confirmed -
 *     left as a resolved-address, unnamed handle (usbdc_link_state_obj)
 *     rather than guessed. NEEDS LIVE QUERY/cross-file follow-up: whoever
 *     eventually reconstructs FUN_c000f230/_f59c/_ec0c/_f69c/_f584 (all 5
 *     are out of this file's own two ranges) should resolve this object's
 *     real role and, if warranted, rename usbdc_link_state_obj throughout.
 *   - FUN_c0009890 (called by usbdc_ep0_ctx_notify below) is not
 *     attributed to any file yet - out of range, cited as a bare extern.
 *   - usbdc_notify_code_select / usbdc_notify_code_select2 (FUN_c0006288/
 *     FUN_c0006298) are byte-for-byte IDENTICAL function bodies at two
 *     different addresses - most likely two compiler-emitted instances of
 *     one inline/static C++ member function (one per translation unit/call
 *     site), matching this project's own established "duplicated static
 *     inline" observations elsewhere. Not merged into one symbol here -
 *     each retains its own real address and name, matching house style of
 *     never inventing a shared symbol Ghidra didn't actually emit.
 *   - Every "phantom-forwarded-argument" call site below (usbdc_notify_
 *     code_select/_2, both called with 2 extra trailing arguments the real
 *     callee's formal parameter list doesn't have) is transcribed with the
 *     real (shorter) formal signature; the discarded extra arguments are
 *     noted in each call site's own comment, not silently dropped.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---------------------------------------------------------------------- *
 *  Shared primitives, already reconstructed elsewhere in this project -
 *  reused here by (matching) extern declaration only, per project
 *  convention.
 * ---------------------------------------------------------------------- */
extern int32_t omap_tick_scale(int32_t ticks, int32_t divisor);	/* FUN_c001e3f8, omap_l108.c/clcdc.c/soc_periph.c/ctouchpanel.c/panelbus_dispatch.c */
extern int      midi_hw_channel_ready(uint32_t *handle, int channel);	/* FUN_c000cc94, midi_engine.c */
extern uint32_t chan_link_tx(uint32_t target, uint8_t subcode, const void *buf, uint32_t len);	/* FUN_c000c1f0, chan_slot_dispatch.c */
extern int       midi_ring1_has_space(uint32_t *ctx);	/* FUN_c000d5d0, midi_engine.c - real name despite being reused here on the ring-2/context singleton, see file header point 4 */
extern void midi_txq_set_realtime_flag(void *obj, uint8_t mask);		/* FUN_c0010078, midi_chan_status_queues.c */
extern void midi_txq_push1(void *obj, uint8_t byte);				/* FUN_c00100ac, midi_chan_status_queues.c */
extern void midi_txq_push2(void *obj, uint8_t byte0, uint8_t byte1);		/* FUN_c0010120, midi_chan_status_queues.c */
extern void midi_txq_push3(void *obj, uint8_t byte0, uint8_t byte1, uint8_t byte2);	/* FUN_c00101b8, midi_chan_status_queues.c */

/* ---------------------------------------------------------------------- *
 *  Out-of-range helpers this file's own functions call into, but which are
 *  not attributed to any file yet - bare externs, real names deferred.
 * ---------------------------------------------------------------------- */
extern void     FUN_c000f230(void *link_state_obj);				/* out of range, see STILL OPEN */
extern bool     FUN_c000f59c(void *link_state_obj);				/* out of range */
extern uint8_t  FUN_c000ec0c(void *link_state_obj, int selector);		/* out of range */
extern int      FUN_c000f584(void *link_state_obj);				/* out of range */
extern int      FUN_c000f69c(void *link_state_obj, uint32_t param2, uint32_t code);	/* out of range */
extern uint8_t  FUN_c000a13c(void *ep0_ctx_handle, int flag);			/* out of range */
extern void     FUN_c0009890(void *dev, const void *buf, uint32_t param3);	/* out of range, not attributed - param2 is a buffer pointer per its own decompile, not a plain scalar */

/* usbdc_link_state_obj - 0xC01CC0F4, see file header STILL OPEN. */
#define USBDC_LINK_STATE_OBJ	((void *)0xC01CC0F4)

/* usbdc_ep0_ctx_handle - 0xC01CACC0, per omap_l137_usbdc_ep0.c. */
#define USBDC_EP0_CTX_HANDLE	((void *)0xC01CACC0)

/* chan_link_ctx_handle - 0xC01CC750, new this file, see header point 6. */
#define CHAN_LINK_CTX_HANDLE	((uint32_t *)0xC01CC750)

/* chan_status_obj - 0xC01CC12C, per midi_chan_status_queues.c. */
#define CHAN_STATUS_OBJ		((void *)0xC01CC12C)

/* midi_ring1/2 context singleton - 0xC01CAD44, per midi_engine.c. */
#define MIDI_CTX_SINGLETON	((uint32_t *)0xC01CAD44)

/* the shared 4-entry notify ring and its two indices/flag/accumulator. */
static uint32_t usbdc_notify_ring[4]     __attribute__((section(".data"))) = {0, 0, 0, 0};	/* 0xC01CCA48 */
#define USBDC_NOTIFY_RING_WIDX	(*(uint32_t *)0xC01CCA94)
#define USBDC_NOTIFY_RING_RIDX	(*(uint32_t *)0xC01CCA44)
#define USBDC_NOTIFY_PENDING	(*(uint32_t *)0xC0098F64)	/* WIDTH NOTE, see file header */
#define USBDC_NOTIFY_ACCUM	(*(uint32_t *)0xC01CCA88)

/* ============================================================================
 *  SECTION A - 0xc000618c-0xc0006578 (9 functions)
 * ============================================================================ */

/* ----------------------------------------------------------------------
 *  notify_record_pack3 - FUN_c000618c, @0xc000618c (16 bytes)
 * ----------------------------------------------------------------------
 *  Writes a fixed 3-byte record {b0,b1,b2} into the caller-supplied
 *  buffer. Sole caller: usbdc_notify_code_select (below), 5 call sites,
 *  one per switch case.
 * ------------------------------------------------------------------- */
void notify_record_pack3(uint8_t *buf, uint8_t b0, uint8_t b1, uint8_t b2)	/* FUN_c000618c */
{
	buf[2] = b2;
	buf[0] = b0;
	buf[1] = b1;
}

/* ----------------------------------------------------------------------
 *  usbdc_notify_code_select - FUN_c0006288, @0xc0006288 (228 bytes)
 * ----------------------------------------------------------------------
 *  Reads a fixed 5-entry table (usbdc_notify_code_table, DAT_c0006284 =
 *  0xC001F280, a literal-pool array in the low code/rodata region - not
 *  independently named/typed further here) and, based on `selector`,
 *  both writes a fixed 3-byte record via notify_record_pack3 into `buf`
 *  AND returns one of the 5 table entries. Selector values seen at the
 *  real call site (usbdc_notify_code_select's own caller,
 *  mcasp_ep_ctx_notify below): only param_3=8 is ever passed there
 *  (falls into `default`), so the other cases (1, 2, -2, -1) are
 *  transcribed from the switch itself but have no confirmed live caller
 *  in this project's own xref data.
 *
 *  Sole caller: mcasp_ep_ctx_notify (FUN_c00068fc, below), call site
 *  0xc0006918... actually 0xc000... (see xrefs) - called with 5 arguments
 *  (extra trailing `8,1` beyond this function's own 3 formal parameters,
 *  the "phantom-forwarded-argument" pattern, see file header).
 *
 *  NOTE: byte-for-byte identical body to usbdc_notify_code_select2 below -
 *  see file header STILL OPEN.
 * ------------------------------------------------------------------- */
extern const uint32_t usbdc_notify_code_table[5];	/* DAT_c0006284 = 0xC001F280 */

uint32_t usbdc_notify_code_select(void *unused, uint8_t *buf, int selector)	/* FUN_c0006288 */
{
	uint32_t t0 = usbdc_notify_code_table[0];
	uint32_t t1 = usbdc_notify_code_table[1];
	uint32_t t2 = usbdc_notify_code_table[2];
	uint32_t t3 = usbdc_notify_code_table[3];
	uint32_t t4 = usbdc_notify_code_table[4];

	switch (selector) {
	default:
		notify_record_pack3(buf, 0, 0, 0xc);
		return t2;
	case 1:
		notify_record_pack3(buf, 0, 8, 0xc);
		return t1;
	case 2:
		notify_record_pack3(buf, 0, 0x10, 0xc);
		return t0;
	case -2:
		notify_record_pack3(buf, 0, 0xf0, 0xb);
		return t4;	/* unmodified default value, per Ghidra (no reassignment on this case) */
	case -1:
		notify_record_pack3(buf, 0, 0xf8, 0xb);
		return t3;
	}
}

/* ----------------------------------------------------------------------
 *  usbdc_notify_code_select2 - FUN_c0006298, @0xc0006298 (16 bytes)
 * ----------------------------------------------------------------------
 *  Identical body to usbdc_notify_code_select above (see its own note and
 *  file header STILL OPEN). Sole caller: usbdc_ep0_ctx_notify (FUN_
 *  c0006858, below), called with 4 arguments (one phantom-forwarded
 *  trailing `8` beyond this function's 3 formal parameters).
 * ------------------------------------------------------------------- */
uint32_t usbdc_notify_code_select2(void *unused, uint8_t *buf, int selector)	/* FUN_c0006298 */
{
	uint32_t t0 = usbdc_notify_code_table[0];
	uint32_t t1 = usbdc_notify_code_table[1];
	uint32_t t2 = usbdc_notify_code_table[2];
	uint32_t t3 = usbdc_notify_code_table[3];
	uint32_t t4 = usbdc_notify_code_table[4];

	switch (selector) {
	default:
		notify_record_pack3(buf, 0, 0, 0xc);
		return t2;
	case 1:
		notify_record_pack3(buf, 0, 8, 0xc);
		return t1;
	case 2:
		notify_record_pack3(buf, 0, 0x10, 0xc);
		return t0;
	case -2:
		notify_record_pack3(buf, 0, 0xf0, 0xb);
		return t4;
	case -1:
		notify_record_pack3(buf, 0, 0xf8, 0xb);
		return t3;
	}
}

/* ----------------------------------------------------------------------
 *  usbdc_ep_counter_clear - FUN_c00062a8, @0xc00062a8 (28 bytes)
 * ----------------------------------------------------------------------
 *  Clears one of two fixed byte(?) counters depending on `sel`:
 *  sel==0 clears 0xC01CCAB4 (DAT_c00062c8); sel!=0 clears 0xC01CCA5C
 *  (DAT_c00062c4) - the SAME address usbdc_ep_notify_debounce_bump
 *  (FUN_c0006988, Section B) increments/resets, a real cross-file field
 *  confirmation.
 *
 *  Callers (2, both FUN_c000a45c - out of range, the same function
 *  omap_l137_usbdc_ep0.c documents as its own upper exclusive boundary):
 *  0xc000a578, 0xc000a620.
 * ------------------------------------------------------------------- */
void usbdc_ep_counter_clear(void *unused, uint8_t sel)	/* FUN_c00062a8 */
{
	if (sel == 0)
		*(uint32_t *)0xC01CCAB4 = 0;	/* DAT_c00062c8 */
	else
		*(uint32_t *)0xC01CCA5C = 0;	/* DAT_c00062c4 */
}

/* ----------------------------------------------------------------------
 *  mcasp_ep_counter_clear - FUN_c00062cc, @0xc00062cc (28 bytes)
 * ----------------------------------------------------------------------
 *  Same shape as usbdc_ep_counter_clear above, but on a DIFFERENT address
 *  pair (0xC01CCAB0/0xC01CCA58 vs 0xC01CCAB4/0xC01CCA5C) - 0xC01CCA58 is
 *  also read by usbdc_ep_rate_update (FUN_c0006790, Section B) as a
 *  threshold, another real cross-file field confirmation.
 *
 *  Callers (2, both FUN_c000e498 - out of range, cited by chan_link_hw.c/
 *  chan_slot_dispatch.c/chan_param_ctrl.c/usbdc_midi_status_glue.c, i.e.
 *  the audio/channel-link side rather than the USB EP0 side
 *  usbdc_ep_counter_clear serves): 0xc000e5ec, 0xc000e6c8.
 * ------------------------------------------------------------------- */
void mcasp_ep_counter_clear(void *unused, uint8_t sel)	/* FUN_c00062cc */
{
	if (sel == 0)
		*(uint32_t *)0xC01CCAB0 = 0;	/* DAT_c00062ec */
	else
		*(uint32_t *)0xC01CCA58 = 0;	/* DAT_c00062e8 */
}

/* ----------------------------------------------------------------------
 *  midi_link_status_poll - FUN_c00062f0, @0xc00062f0 (28 bytes)
 * ----------------------------------------------------------------------
 *  Thin wrapper: forwards to the out-of-range FUN_c000f59c on the fixed
 *  usbdc_link_state_obj handle (0xC01CC0F4). Sole caller:
 *  midi_link_status_poll_wrap (FUN_c00069ac, Section B), 0xc00069b8.
 * ------------------------------------------------------------------- */
bool midi_link_status_poll(void)	/* FUN_c00062f0 */
{
	return FUN_c000f59c(USBDC_LINK_STATE_OBJ);	/* DAT_c000630c = 0xC01CC0F4 */
}

/* ----------------------------------------------------------------------
 *  usbdc_ep_state_init - FUN_c0006310, @0xc0006310 (188 bytes)
 * ----------------------------------------------------------------------
 *  Sets 0xC01CC74C=1 unconditionally, then branches on `full_init`:
 *
 *  full_init != 0: resets a cluster of fields including
 *  usbdc_notify_ring_widx (set to 1, NOT 0 - transcribed as-is, see file
 *  header WIDTH/asymmetry note), usbdc_notify_pending_flag (set to 1),
 *  usbdc_notify_ring_ridx (set to 0), and fills the entire 4-entry
 *  usbdc_notify_ring with the literal 0x6c (108) - the exact same array
 *  usbdc_ep0_notify_helper2 (below) writes real notify codes into later,
 *  and usbdc_ep0_notify_helper (below) reads from - confirms this is the
 *  ring's own init path.
 *
 *  full_init == 0: resets a smaller, disjoint field cluster including
 *  usbdc_notify_scaled_accum (0xC01CCA88, zeroed here).
 *
 *  Callers (2, both FUN_c000a45c - out of range, USB EP0 side, same
 *  caller as usbdc_ep_counter_clear above): 0xc000a58c, 0xc000a634.
 * ------------------------------------------------------------------- */
void usbdc_ep_state_init(void *unused, bool full_init)	/* FUN_c0006310 */
{
	int i;

	*(uint32_t *)0xC01CC74C = 1;	/* DAT_c00063cc */

	if (full_init) {
		*(uint32_t *)0xC0098F70 = 0xC0000;	/* DAT_c00063d0 */
		USBDC_NOTIFY_RING_WIDX = 1;		/* DAT_c00063d4 = 0xC01CCA94 */
		*(uint32_t *)0xC01CCAAC = 0;		/* DAT_c00063d8 */
		*(uint32_t *)0xC01CCA90 = 0;		/* DAT_c00063dc */
		*(uint32_t *)0xC01CCA8C = 0;		/* DAT_c00063e0 */
		*(uint32_t *)0xC01CCA6C = 0;		/* DAT_c00063e4 */
		*(uint32_t *)0xC0098F78 = 0xC0000;	/* DAT_c00063e8 */
		*(uint32_t *)0xC0098F74 = 0xC0000;	/* DAT_c00063ec */
		*(uint32_t *)0xC01CCA68 = 0;		/* DAT_c00063f0 */
		USBDC_NOTIFY_PENDING = 1;		/* DAT_c00063f4 = 0xC0098F64 */
		USBDC_NOTIFY_RING_RIDX = 0;		/* DAT_c00063f8 = 0xC01CCA44 */
		for (i = 0; i < 4; i++)
			usbdc_notify_ring[i] = 0x6c;	/* DAT_c00063fc = 0xC01CCA48 */
		return;
	}

	*(uint32_t *)0xC0098F88 = 0xC0000;	/* DAT_c0006400 */
	*(uint32_t *)0xC01CCAA4 = 0;		/* DAT_c0006404 */
	USBDC_NOTIFY_ACCUM = 0;		/* DAT_c0006408 = 0xC01CCA88 */
	*(uint32_t *)0xC01CCAA8 = 0;		/* DAT_c000640c */
}

/* ----------------------------------------------------------------------
 *  mcasp_ep_state_init - FUN_c0006410, @0xc0006410 (112 bytes)
 * ----------------------------------------------------------------------
 *  Same 2-branch reset shape as usbdc_ep_state_init above, but touches a
 *  DIFFERENT, smaller field cluster (0xC01CCA60/0xC0098F68/0xC01CCAA0/
 *  0xC01CCA78/0xC01CCA74/0xC0098F6C on the true branch; 0xC0098F80/
 *  0xC01CCA98/0xC01CC7C8/0xC01CCA9C/0xC0098F84 on the false branch) and
 *  does NOT touch the notify ring at all.
 *
 *  Callers (2, both FUN_c000e498 - out of range, audio/channel-link side,
 *  same caller as mcasp_ep_counter_clear above): 0xc000e600, 0xc000e6dc.
 * ------------------------------------------------------------------- */
void mcasp_ep_state_init(void *unused, bool full_init)	/* FUN_c0006410 */
{
	if (full_init) {
		*(uint32_t *)0xC01CCA60 = 0;		/* DAT_c0006480 */
		*(uint32_t *)0xC0098F68 = 0xC0000;	/* DAT_c0006484 */
		*(uint32_t *)0xC01CCAA0 = 0;		/* DAT_c0006488 */
		*(uint32_t *)0xC01CCA78 = 0;		/* DAT_c000648c */
		*(uint32_t *)0xC01CCA74 = 0;		/* DAT_c0006490 */
		*(uint32_t *)0xC0098F6C = 0xC0000;	/* DAT_c0006494 */
		return;
	}

	*(uint32_t *)0xC0098F80 = 0xC0000;	/* DAT_c0006498 */
	*(uint32_t *)0xC01CCA98 = 0;		/* DAT_c000649c */
	*(uint32_t *)0xC01CC7C8 = 0;		/* DAT_c00064a0 */
	*(uint32_t *)0xC01CCA9C = 0;		/* DAT_c00064a4 */
	*(uint32_t *)0xC0098F84 = 0xC0000;	/* DAT_c00064a8 */
}

/* ----------------------------------------------------------------------
 *  usbdc_ep0_notify_helper2 - FUN_c00064ac, @0xc00064ac (172 bytes)
 * ----------------------------------------------------------------------
 *  ALREADY NAMED (bare extern only, never given a body) by
 *  omap_l137_usbdc_ext.c - name reused verbatim. See file header
 *  "ARGUMENT-COUNT CORRECTION": the real formal signature has 3
 *  parameters, not 4 - `param2`/ctx is dead here (never referenced).
 *
 *  Reads two flag bytes at usbdc_ep0_ctx_handle+0x70 (0xC01CACC0+0x70) and
 *  chan_link_ctx_handle+0x70 (0xC01CC750+0x70), calls the out-of-range
 *  FUN_c000f230 on usbdc_link_state_obj (0xC01CC0F4), clears whichever of
 *  the two flag bytes was set, then:
 *   - scales `len` via omap_tick_scale(len, 0x12) and accumulates the
 *     result into usbdc_notify_scaled_accum;
 *   - if usbdc_notify_pending_flag is set, clears it AND resets
 *     usbdc_notify_ring_ridx to 0 and usbdc_notify_ring_widx to 1 (the
 *     asymmetric reset already flagged in the file header);
 *   - unconditionally stores `len` into usbdc_notify_ring at the current
 *     write index and advances that index mod 4 - this is the ring's
 *     real PRODUCER path (usbdc_ep0_notify_helper below is its consumer).
 *
 *  Sole caller: FUN_c0004984 (usbdc_ep0_notify_rx_complete,
 *  omap_l137_usbdc_ext.c), 0xc00049c4.
 * ------------------------------------------------------------------- */
void usbdc_ep0_notify_helper2(void *handle, uint32_t len, uint32_t param3)	/* FUN_c00064ac - real 3-param signature, see header correction */
{
	uint8_t *ep0_flag  = (uint8_t *)USBDC_EP0_CTX_HANDLE + 0x70;	/* DAT_c0006558/_655c pair */
	uint8_t *link_flag = (uint8_t *)CHAN_LINK_CTX_HANDLE + 0x70;
	bool ep0_set  = *ep0_flag  != 0;
	bool link_set = *link_flag != 0;
	int32_t scaled;
	uint32_t widx;

	FUN_c000f230(USBDC_LINK_STATE_OBJ);	/* DAT_c0006560 = 0xC01CC0F4 */

	if (ep0_set)
		*ep0_flag = 0;
	if (link_set)
		*link_flag = 0;

	scaled = omap_tick_scale((int32_t)len, 0x12);
	USBDC_NOTIFY_ACCUM += scaled;			/* DAT_c0006564 = 0xC01CCA88 */

	if (USBDC_NOTIFY_PENDING) {			/* DAT_c0006568 = 0xC0098F64 */
		USBDC_NOTIFY_PENDING = 0;
		USBDC_NOTIFY_RING_RIDX = 0;		/* DAT_c0006570 = 0xC01CCA44 */
		USBDC_NOTIFY_RING_WIDX = 1;		/* DAT_c000656c = 0xC01CCA94 */
	}

	widx = USBDC_NOTIFY_RING_WIDX;
	usbdc_notify_ring[widx] = len;			/* DAT_c0006574 = 0xC01CCA48 */
	USBDC_NOTIFY_RING_WIDX = (widx + 1) & 3;
	(void)param3;	/* dead, per Ghidra's own 3-param signature */
}

/* ============================================================================
 *  SECTION B - 0xc0006790-0xc0006b74 (9 functions)
 * ============================================================================ */

/* ----------------------------------------------------------------------
 *  usbdc_ep_rate_update - FUN_c0006790, @0xc0006790 (176 bytes)
 * ----------------------------------------------------------------------
 *  Calls the out-of-range FUN_c000ec0c(usbdc_link_state_obj, 1); if it
 *  returns 0 (false), returns immediately. Otherwise, if the "armed" latch
 *  at 0xC01CCA78 was 0, initializes three fields (0xC01CC7C8=0,
 *  0xC01CCA78=1, 0xC01CCA74=0 - the first of these, 0xC01CC7C8, is the
 *  SAME address mcasp_ep_state_init's own false-branch resets). Then, if
 *  the latch is >0 and `len` <= the threshold at 0xC01CCA58 (the SAME
 *  address mcasp_ep_counter_clear touches), computes a signed delta
 *  between a fresh tick snapshot (the out-of-range FUN_c000f584) and the
 *  previous one stored at 0xC01CCA60, clamps it to {-1,0,1}, and OVER-
 *  WRITES 0xC01CCA60 with that clamped value (so that slot alternates
 *  between "previous raw tick" and "clamped trend", read faithfully as
 *  observed) - plus resets 0xC01CC7C8 to 0 again.
 *
 *  Sole caller: FUN_c000c39c (out of range - chan_link_hw.c/
 *  chan_slot_dispatch.c/chan_param_ctrl.c/usbdc_midi_status_glue.c all
 *  cite this exact function), 0xc000c6e8.
 * ------------------------------------------------------------------- */
void usbdc_ep_rate_update(void *unused, int len)	/* FUN_c0006790 */
{
	uint32_t *ready    = (uint32_t *)0xC01CCA78;	/* DAT_c0006844 - "armed" latch; overwritten with the clamped trend value below via `trend`, NOT this slot - see note */
	uint32_t *flag     = (uint32_t *)0xC01CC7C8;	/* DAT_c0006848 - shared address with mcasp_ep_state_init's own reset field */
	uint32_t *cur_snap = (uint32_t *)0xC01CCA74;	/* DAT_c000684c */
	uint32_t *thresh   = (uint32_t *)0xC01CCA58;	/* DAT_c0006850 - shared address with mcasp_ep_counter_clear's own flag */
	uint32_t *trend    = (uint32_t *)0xC01CCA60;	/* DAT_c0006854 - doubles as "prev tick" input and, after this call, "clamped delta" output */
	int32_t cur, prev, delta;

	if (!FUN_c000ec0c(USBDC_LINK_STATE_OBJ, 1))	/* DAT_c0006840 = 0xC01CC0F4 */
		return;

	if (*ready == 0) {
		*flag = 0;
		*ready = 1;
		*cur_snap = 0;
	}

	if ((int32_t)*ready <= 0)
		return;
	if (len > (int)*thresh)
		return;

	cur  = FUN_c000f584(USBDC_LINK_STATE_OBJ);	/* returns 0x90 - link_state_obj+0x28 */
	prev = (int32_t)*trend;
	*cur_snap = (uint32_t)cur;

	delta = cur - prev;
	if (delta > 0)
		delta = 1;
	else if (delta < 0)
		delta = -1;
	/* delta==0 stays 0 - matches the original's SCARRY4-based overflow-safe clamp exactly */
	*trend = (uint32_t)delta;
	*flag = 0;
}

/* ----------------------------------------------------------------------
 *  usbdc_ep0_ctx_notify - FUN_c0006858, @0xc0006858 (136 bytes)
 * ----------------------------------------------------------------------
 *  Calls the out-of-range FUN_c000a13c(usbdc_ep0_ctx_handle, 1); if it
 *  returns nonzero, returns immediately (no-op). Otherwise: stashes the
 *  current value of a status word (0xC0098F74) into a second slot
 *  (0xC0098F78), calls usbdc_notify_code_select2 on a fixed scratch
 *  buffer (0xC01CCA70) with a selector read from 0xC01CCA6C (1 phantom
 *  trailing argument `8` beyond the real 3-param signature - see file
 *  header), writes the returned code back into the FIRST status word
 *  (0xC0098F74) while the SECOND slot (0xC0098F70) receives the OLD value
 *  of that status word (a swap-like update, not a plain copy), clears a
 *  flag at 0xC01CCA68, then calls the out-of-range FUN_c0009890(handle,
 *  buf, 3) with a 4th raw argument (`1`) the real 3-param callee drops.
 *
 *  This is the function omap_l137_usbdc_ext.c's own extern comment labels
 *  "usbdc_ep_state_notify (FUN_c0006858)" - see file header NAMING
 *  CORRECTION: it does real record-select-and-dispatch work, not a
 *  generic notify.
 *
 *  Sole caller: FUN_c000aae0 (usbdc_endpoint_event_dispatch,
 *  omap_l137_usbdc_ext.c), 0xc000ac0c - real 2nd argument at that call
 *  site is `code` (0x40 or 8, per that function's own switch), passed as
 *  a phantom/dead handle here (never referenced in this body).
 * ------------------------------------------------------------------- */
void usbdc_ep0_ctx_notify(void *unused)	/* FUN_c0006858 */
{
	uint8_t   ok;
	uint32_t *buf      = (uint32_t *)0xC01CCA70;	/* DAT_c00068ec - fixed scratch buffer, same shape as notify_record_pack3's own target */
	uint32_t *selector = (uint32_t *)0xC01CCA6C;	/* DAT_c00068f0 */
	uint32_t *dstA     = (uint32_t *)0xC0098F78;	/* DAT_c00068e8 */
	uint32_t *srcA     = (uint32_t *)0xC0098F74;	/* DAT_c00068e4 */
	uint32_t *dstB     = (uint32_t *)0xC0098F70;	/* DAT_c00068f4 */
	uint32_t code, old_srcA;

	ok = FUN_c000a13c(USBDC_EP0_CTX_HANDLE, 1);	/* DAT_c00068e0 = 0xC01CACC0 */
	if (ok != 0)
		return;

	*dstA = *srcA;					/* stash old srcA into dstA */
	code = usbdc_notify_code_select2(NULL, (uint8_t *)buf, (int)*selector);	/* real signature: (void*, uint8_t*, int); phantom trailing `8` dropped */
	old_srcA = *dstA;				/* re-read the stashed value */
	*srcA = code;					/* srcA now holds the new code */
	*dstB = old_srcA;				/* dstB gets the OLD srcA value */
	*(uint32_t *)0xC01CCA68 = 0;	/* DAT_c00068f8 */

	FUN_c0009890(USBDC_EP0_CTX_HANDLE, buf, 3);	/* 4th raw arg `1` dropped - real formal signature has 3 params */
}

/* ----------------------------------------------------------------------
 *  mcasp_ep_ctx_notify - FUN_c00068fc, @0xc00068fc (120 bytes)
 * ----------------------------------------------------------------------
 *  Mirrors usbdc_ep0_ctx_notify above, but on chan_link_ctx_handle
 *  (0xC01CC750) instead of usbdc_ep0_ctx_handle: calls the ALREADY-
 *  RECONSTRUCTED midi_hw_channel_ready(chan_link_ctx_handle, 2)
 *  (midi_engine.c); if ready, returns (no-op, NOTE: inverted vs
 *  usbdc_ep0_ctx_notify's own "0 == proceed" convention - this one
 *  proceeds when the result IS 0 too, i.e. `!= 0` gates the return here
 *  as well - transcribed exactly as decompiled). Copies a field, calls
 *  usbdc_notify_code_select (real 3-param signature; 2 phantom trailing
 *  args `8, 1` at the raw call site), stores the result, then calls the
 *  ALREADY-RECONSTRUCTED chan_link_tx(chan_link_ctx_handle, 2, val, 3)
 *  (chan_slot_dispatch.c).
 *
 *  Sole caller: FUN_c000c39c (CONDITIONAL_CALL, out of range - same
 *  caller as usbdc_ep_rate_update above), 0xc000c708.
 * ------------------------------------------------------------------- */
void mcasp_ep_ctx_notify(void *unused)	/* FUN_c00068fc */
{
	uint32_t *selector = (uint32_t *)0xC01CCA60;	/* DAT_c0006984 */
	uint8_t  *buf      = (uint8_t  *)0xC01CCA64;	/* DAT_c000697c */
	uint32_t *dst      = (uint32_t *)0xC0098F6C;	/* DAT_c0006980 */
	uint32_t *src      = (uint32_t *)0xC0098F68;	/* DAT_c0006978 */
	uint32_t code;

	if (midi_hw_channel_ready(CHAN_LINK_CTX_HANDLE, 2))	/* DAT_c0006974 = 0xC01CC750 */
		return;

	*dst = *src;
	code = usbdc_notify_code_select(NULL, buf, (int)*selector);	/* real signature: (void*, uint8_t*, int); phantom trailing `8,1` dropped */
	*src = code;

	chan_link_tx((uint32_t)CHAN_LINK_CTX_HANDLE, 2, buf, 3);
}

/* ----------------------------------------------------------------------
 *  usbdc_ep_notify_debounce_bump - FUN_c0006988, @0xc0006988 (32 bytes)
 * ----------------------------------------------------------------------
 *  Threshold-gated counter at 0xC01CCA5C (the SAME address
 *  usbdc_ep_counter_clear conditionally clears): increments while below
 *  `code`, else resets to 1.
 *
 *  Sole caller: FUN_c000aae0 (usbdc_endpoint_event_dispatch), 0xc000acac
 *  - real 2nd argument `code` (0x40 or 8, see usbdc_ep0_ctx_notify's own
 *  note).
 * ------------------------------------------------------------------- */
void usbdc_ep_notify_debounce_bump(void *unused, int code)	/* FUN_c0006988 */
{
	uint32_t *counter = (uint32_t *)0xC01CCA5C;	/* DAT_c00069a8 */

	if ((int)*counter < code)
		(*counter)++;
	else
		*counter = 1;
}

/* ----------------------------------------------------------------------
 *  midi_link_status_poll_wrap - FUN_c00069ac, @0xc00069ac (20 bytes)
 * ----------------------------------------------------------------------
 *  Bare 1-line forwarder to midi_link_status_poll (FUN_c00062f0,
 *  Section A) - its return value is discarded. This is the function
 *  omap_l137_usbdc_ext.c's own extern comment labels "usbdc_ep_state_
 *  notify (FUN_c00069ac)" - see file header NAMING CORRECTION: it has no
 *  notify-style behavior at all.
 *
 *  Sole caller: FUN_c000aae0, 0xc000ac24 (phantom handle argument again,
 *  never used here - this function takes no parameters).
 * ------------------------------------------------------------------- */
void midi_link_status_poll_wrap(void)	/* FUN_c00069ac */
{
	(void)midi_link_status_poll();
}

/* ----------------------------------------------------------------------
 *  mcasp_ep_rate_check - FUN_c00069c0, @0xc00069c0 (28 bytes)
 * ----------------------------------------------------------------------
 *  Bare forwarder to the out-of-range FUN_c000ec0c(usbdc_link_state_obj,
 *  0) - the dir=0 sibling of usbdc_ep_rate_update's own dir=1 call.
 *  Return value discarded.
 *
 *  Sole caller: FUN_c000c39c, 0xc000c8e4.
 * ------------------------------------------------------------------- */
void mcasp_ep_rate_check(void)	/* FUN_c00069c0 */
{
	(void)FUN_c000ec0c(USBDC_LINK_STATE_OBJ, 0);	/* DAT_c00069dc = 0xC01CC0F4 */
}

/* ----------------------------------------------------------------------
 *  usbdc_ep0_notify_helper - FUN_c0006a04, @0xc0006a04 (96 bytes)
 * ----------------------------------------------------------------------
 *  ALREADY NAMED (bare extern only, never given a body) by
 *  omap_l137_usbdc_ext.c - name reused verbatim. See file header
 *  "ARGUMENT-COUNT CORRECTION": real formal signature has 2 parameters,
 *  not 3 - the caller's `ctx` argument is simply not received.
 *
 *  This is the notify ring's real CONSUMER (mirror of usbdc_ep0_notify_
 *  helper2's producer role, Section A): reads usbdc_ep0_ctx_handle+0x6f
 *  flag; if usbdc_notify_pending_flag is 0, pops one entry off
 *  usbdc_notify_ring at the current read index (usbdc_notify_ring_ridx,
 *  advanced mod 4) - else uses the fixed default 0x6c. Forwards the
 *  selected code to the out-of-range FUN_c000f69c(usbdc_link_state_obj,
 *  len, code). If the +0x6f flag was set, clears it.
 *
 *  Sole caller: FUN_c00048f8 (usbdc_ep0_notify_tx_complete,
 *  omap_l137_usbdc_ext.c), 0xc0004934.
 * ------------------------------------------------------------------- */
uint32_t usbdc_ep0_notify_helper(void *handle, uint32_t len)	/* FUN_c0006a04 - real 2-param signature, see header correction */
{
	uint8_t *ep0_flag = (uint8_t *)USBDC_EP0_CTX_HANDLE + 0x6f;	/* DAT_c0006a68 = 0xC01CACC0 */
	bool ep0_set = *ep0_flag != 0;
	uint32_t code = 0x6c;

	if (!USBDC_NOTIFY_PENDING) {			/* DAT_c0006a64 = 0xC0098F64 */
		uint32_t ridx = USBDC_NOTIFY_RING_RIDX;	/* DAT_c0006a6c = 0xC01CCA44 */
		USBDC_NOTIFY_RING_RIDX = (ridx + 1) & 3;
		code = usbdc_notify_ring[ridx];		/* DAT_c0006a70 = 0xC01CCA48 */
	}

	(void)FUN_c000f69c(USBDC_LINK_STATE_OBJ, len, code);	/* DAT_c0006a74 = 0xC01CC0F4 */

	if (ep0_set)
		*ep0_flag = 0;

	return code;
}

/* ----------------------------------------------------------------------
 *  usb_midi_packet_decode - FUN_c0006a78, @0xc0006a78 (212 bytes)
 * ----------------------------------------------------------------------
 *  A USB-MIDI-Class 4-byte event-packet decoder, walking `buf` in 4-byte
 *  strides while >= 4 bytes remain. This function's own dispatch was
 *  ALREADY fully analyzed (from its own decompile) by midi_chan_status_
 *  queues.c's header, as the confirmed sole caller of its own midi_txq_*
 *  functions - this reconstruction matches that analysis exactly:
 *  low nibble of byte0 is the USB-MIDI CIN; CIN 0x2/0x6/0xC/0xD (2-byte
 *  messages) -> midi_txq_push2; CIN 0x3/0x4/0x7/0x8-0xB/0xE (3-byte
 *  messages) -> midi_txq_push3; CIN 0x5 -> midi_txq_push1(byte1); CIN 0xF
 *  with byte1 < 0xF8 -> midi_txq_push1(byte1), else (0xF8-0xFF) ->
 *  midi_txq_set_realtime_flag with a bit computed from byte1; CIN 0x0/0x1
 *  -> no-op. Every push/flag call passes chan_status_obj (0xC01CC12C,
 *  DAT_c0006b4c) as the fixed handle.
 *
 *  Sole caller: FUN_c000a9f4 (usbdc_ep_state9_handler, per midi_engine.c's
 *  own naming - out of range), CONDITIONAL_CALL, 0xc0006d0c.
 * ------------------------------------------------------------------- */
void usb_midi_packet_decode(void *unused, const uint8_t *buf, uint32_t len)	/* FUN_c0006a78 */
{
	uint8_t cin;

	if (len < 4)
		return;

	do {
		cin = buf[0] & 0xf;

		if (cin == 0xf) {
			uint8_t b1 = buf[1];
			if (b1 < 0xf8) {
				midi_txq_push1(CHAN_STATUS_OBJ, b1);
			} else {
				midi_txq_set_realtime_flag(CHAN_STATUS_OBJ,
					(uint8_t)(1u << ((uint8_t)(b1 + 8))));
			}
		} else if (cin == 0xe) {
			midi_txq_push3(CHAN_STATUS_OBJ, buf[1], buf[2], buf[3]);
		} else if (cin < 0xc) {
			if (cin > 6) {
				midi_txq_push3(CHAN_STATUS_OBJ, buf[1], buf[2], buf[3]);
			} else if (cin == 6) {
				midi_txq_push2(CHAN_STATUS_OBJ, buf[1], buf[2]);
			} else if (cin == 5) {
				midi_txq_push1(CHAN_STATUS_OBJ, buf[1]);
			} else if (cin > 2) {
				midi_txq_push3(CHAN_STATUS_OBJ, buf[1], buf[2], buf[3]);
			} else if (cin == 2) {
				midi_txq_push2(CHAN_STATUS_OBJ, buf[1], buf[2]);
			}
			/* cin 0/1: no-op */
		} else {
			midi_txq_push2(CHAN_STATUS_OBJ, buf[1], buf[2]);
		}

		len -= 4;
		buf += 4;
	} while (len >= 4);
}

/* ----------------------------------------------------------------------
 *  midi_tx_ready_query - FUN_c0006b50, @0xc0006b50 (32 bytes)
 * ----------------------------------------------------------------------
 *  Thin bool-normalizing wrapper around midi_engine.c's own
 *  midi_ring1_has_space (FUN_c000d5d0, already fully reconstructed
 *  there), called on the fixed context singleton 0xC01CAD44 - see file
 *  header point 4.
 *
 *  Callers (2): FUN_c000a9f4 (usbdc_ep_state9_handler, 0xc0006d38 - the
 *  SAME caller as usb_midi_packet_decode above) and FUN_c000fe20
 *  (chan_status_dispatch, per midi_chan_status_queues.c - 0xc0010014).
 * ------------------------------------------------------------------- */
int midi_tx_ready_query(void)	/* FUN_c0006b50 */
{
	int has_space = midi_ring1_has_space(MIDI_CTX_SINGLETON);	/* DAT_c0006b70 = 0xC01CAD44 */

	return has_space != 0;
}
