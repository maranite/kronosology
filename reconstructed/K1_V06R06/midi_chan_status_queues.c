/* SPDX-License-Identifier: GPL-2.0 */
/*
 * midi_chan_status_queues.c - assigned address range 0xc0010078-0xc00103e4
 * sweep, 9 functions:
 *
 *   0xc0010078  FUN_c0010078  48B   midi_txq_set_realtime_flag
 *   0xc00100ac  FUN_c00100ac  112B  midi_txq_push1
 *   0xc0010120  FUN_c0010120  148B  midi_txq_push2
 *   0xc00101b8  FUN_c00101b8  172B  midi_txq_push3
 *   0xc001026c  FUN_c001026c  48B   uart1_rx_irq_enable
 *   0xc0010290  FUN_c0010290  48B   uart1_rx_irq_disable
 *   0xc00102d8  FUN_c00102d8  164B  midi_rxq_push_record
 *   0xc0010394  FUN_c0010394  32B   midi_dispatch_reset_2byte
 *   0xc00103bc  FUN_c00103bc  32B   midi_dispatch_reset_3byte
 *
 * (0xc00103e4 itself, chan_dispatch_probe, is the exclusive end of the
 * assigned range and is NOT reconstructed here - see "OUT OF RANGE"
 * below.)
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json), 2026-07-18 pass. No live Ghidra MCP calls this pass (the
 * bridge is flagged concurrency-unsafe under this project's own parallel
 * work - see README).
 *
 * ANCHOR: NONE. Full `strings` search of the image's 14 real `__FILE__`
 * anchors (queried via query_dump.py strings "cpp") turned up nothing in
 * or near this range - same "no anchor" situation as wire_dispatch.c,
 * panelbus_dispatch.c, usbdc_midi_status_glue.c, i2c_by_gpio.c. This file
 * is adjacent to uart1_midi_queue.c's own range (0xc000fa64-0xc000fe20,
 * ending just below 0xc0010078) and to cpsoc.c/cpsoc_issp.c territory
 * further out - skimmed both per instructions, neither's own address
 * range or struct offsets (cpsoc's 0xc00e0068 shared-context-handle
 * pattern IS reused here, see below, but that's a cross-cutting global,
 * not evidence of cpsoc.c ownership) overlaps this range.
 *
 * =========================================================================
 *  CORRECTED CROSS-FILE FINDING: this cluster operates on chan_status_obj
 *  (0xC01CC12C), NOT "a different struct entirely"
 * =========================================================================
 *  uart1_midi_queue.c's own header (see its lines ~50-61) documents this
 *  exact 0xc0010078-0xc00101b8 cluster as out-of-range and states it
 *  "operates on param_1+0x400/a distinct DAT_c001011c index field, NOT
 *  chan_status_obj". That claim is INCORRECT: the fixed handle constant
 *  every real caller passes in is `DAT_c0006b4c` (FUN_c0006a78's own call
 *  sites at 0xc0006ac8/0xc0006afc/0xc0006b1c/0xc0006b34, all four of my
 *  push/flag functions), which resolves to `-0x3fe33ed4` as a signed
 *  32-bit literal - i.e. exactly `0x100000000 - 0x3FE33ED4 = 0xC01CC12C`,
 *  the SAME address usbdc_midi_status_glue.c's own header already names
 *  `chan_status_obj` (its documented fields sit at +4/+8/+0x14/+0x61c -
 *  note +0x61c is itself one of THIS file's own confirmed offsets below,
 *  independent corroboration). This is a real, address-confirmed
 *  correction, not a guess - left as a cross-file follow-up (this pass's
 *  instructions forbid editing uart1_midi_queue.c to fix it directly).
 *
 *  Net effect: chan_status_obj is a single large struct with (at least)
 *  TWO independent circular queues living in it side by side:
 *
 *   - a 1024-byte raw TX BYTE ring at chan_status_obj+0x000..0x3FF, with
 *     write-index @+0x400 (ushort, wraps mod 0x400), count @+0x404
 *     (ushort, capacity 0x400, push functions block-and-drain once count
 *     would exceed 0x400-N for an N-byte push), and a "pending realtime
 *     byte" bitmask @+0x406 (byte) - see midi_txq_* below. Drained by the
 *     out-of-range chan_status_dispatch (FUN_c000fe20, already cited as
 *     extern by both uart1_midi_queue.c and usbdc_midi_status_glue.c),
 *     which calls uart1_tx_byte (FUN_c000fd24) to actually shift bytes
 *     out over UART1 - i.e. this is a USB-MIDI-IN -> UART1-OUT bridge
 *     queue, fed by the out-of-range USB packet decoder FUN_c0006a78
 *     (see "CALLER" note on midi_txq_push1 below).
 *
 *   - a 128-entry, 4-byte-record RX queue at chan_status_obj+0x418..
 *     0x617 (128*4 = 0x200 bytes), with write-index @+0x618 (ushort,
 *     wraps mod 0x80), count @+0x61c (ushort, capacity 0x80), and a
 *     SEPARATE read-index @+0x61a used only by the out-of-range consumer
 *     midi_pop_raw_record (FUN_c00109dc, already cited as extern by
 *     eva_crt0_tick_glue.c with exactly this offset triple: its own
 *     DAT_c0010acc/DAT_c0010ac8/DAT_c0010ad0 resolve to 0x61c/0x40a/
 *     0x61a - matching this file's own 0x61c count and, for the flags
 *     byte, +0x40a rather than +0x406, i.e. the RX side's OWN realtime-
 *     pending flags byte, distinct from the TX side's +0x406). Populated
 *     by the out-of-range chan_dispatch_probe (FUN_c00103e4, the
 *     exclusive end of this range) as UART1 RX bytes arrive one at a
 *     time - see midi_rxq_push_record below. Drained elsewhere into
 *     midi_engine.c's running-status classifier per eva_crt0_tick_glue.c's
 *     own citation.
 *
 *  A third, smaller piece of state - a smaller running-status parser
 *  living at +0x40c/+0x410 (a GCC Itanium-ABI pointer-to-member-function
 *  pair: code* at +0x40c, and a tagged ptrdiff_t at +0x410 whose LSB
 *  selects direct-call vs vtable-indexed-call, per chan_dispatch_probe's
 *  own decompile) plus a saved status byte at +0x414 and an extra byte at
 *  +0x417 - is RESET by two of this file's own functions
 *  (midi_dispatch_reset_2byte/_3byte) and by midi_rxq_push_record's own
 *  overflow path. The exact code these callback slots point AT is not
 *  resolved (would require reading the 8-byte descriptor tables at
 *  0xC001FB84/0xC001FB8C/0xC001FB94, which are referenced only as
 *  constant pointer VALUES in the decompile, never dereferenced within
 *  any function in this range - see "STILL OPEN" below).
 *
 * =========================================================================
 *  OUT OF RANGE (cited for context only, NOT reconstructed here)
 * =========================================================================
 *  - FUN_c0006a78 (0xc0006a78, 212B) - the sole caller of all four
 *    midi_txq_* functions. A USB-MIDI-Class 4-byte event-packet decoder:
 *    walks `buf` in 4-byte strides, takes the low nibble of byte 0 as the
 *    USB-MIDI Code Index Number (CIN), and dispatches by the standard
 *    USB-MIDI CIN table (USB Device Class Definition for MIDI Devices,
 *    Table 4-1): CIN 0x2/0x6/0xC/0xD (2-byte messages: System Common,
 *    SysEx-ends-2, Program Change, Channel Pressure) -> midi_txq_push2;
 *    CIN 0x3/0x4/0x7/0x8..0xB/0xE (3-byte messages: System Common, SysEx
 *    start/continue, SysEx-ends-3, Note Off/On/PolyPressure/CC, Pitch
 *    Bend) -> midi_txq_push3; CIN 0x5 (single-byte System Common /
 *    SysEx-ends-1) -> midi_txq_push1(byte1); CIN 0xF (Single Byte) with
 *    byte1 < 0xF8 -> midi_txq_push1(byte1), else (byte1 in 0xF8-0xFF,
 *    real-time) -> midi_txq_set_realtime_flag with a bit computed from
 *    byte1; CIN 0x0/0x1 (Misc/Cable) -> no-op. This mapping is what
 *    confirms all four midi_txq_* signatures below - re-derived
 *    independently from FUN_c0006a78's own decompile, not assumed.
 *  - FUN_c000fe20 (chan_status_dispatch, 568B) - drains the TX byte ring,
 *    already cited as extern (no body) by uart1_midi_queue.c and
 *    usbdc_midi_status_glue.c. Called directly by all three midi_txq_
 *    push* functions when the ring is too full; not reconstructed here
 *    either (it starts below 0xc0010078).
 *  - FUN_c00103e4 (chan_dispatch_probe, 296B) - the exclusive upper bound
 *    of this range, already cited as extern (no body) by
 *    usbdc_midi_status_glue.c. Own decompile shows it's the real caller
 *    of midi_dispatch_reset_2byte/_3byte and midi_rxq_push_record (see
 *    each function's own note below for the exact call-site evidence
 *    extracted from its decompile).
 *  - FUN_c00109dc (midi_pop_raw_record, 236B) - the RX queue's consumer,
 *    already cited as extern (no body) by eva_crt0_tick_glue.c. Its own
 *    decompile independently confirms the 0x418/0x618/0x61a/0x61c RX-ring
 *    offsets and is also where uart1_rx_irq_enable/_disable's OTHER call
 *    sites live (used there as the RX ring's own critical-section guard
 *    around index/count mutation, exactly mirroring how uart1_ier_thre_
 *    enable/_disable guard the TX ring in uart1_midi_queue.c and in this
 *    file's own midi_txq_* functions).
 *
 * =========================================================================
 *  STILL OPEN
 * =========================================================================
 *  - The real targets of the three 8-byte pointer-to-member-function
 *    descriptors at 0xC001FB84 (midi_rxq_push_record's overflow-reset
 *    default), 0xC001FB8C (midi_dispatch_reset_2byte's target) and
 *    0xC001FB94 (midi_dispatch_reset_3byte's target) are not resolved -
 *    all_data.json only records these as pointer VALUES referenced by
 *    the decompile, not as dereferenced data (nothing in this range reads
 *    *0xC001FB84 etc. directly). NEEDS LIVE QUERY: 0xC001FB84 (24 bytes,
 *    3x 8-byte {code*, ptrdiff_t} pairs) - what do the three callback
 *    pointers actually point to, and does that confirm the "2-byte vs
 *    3-byte running-status parser continuation" role inferred here from
 *    chan_dispatch_probe's own call-site arithmetic?
 *  - chan_status_obj's own fields between 0x408-0x417 and 0x41f-0x617
 *    (i.e. everything in the two queues' header region not directly
 *    touched by this range's 9 functions) remain unmapped.
 */

#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------- *
 *  Shared primitives, already defined elsewhere in this project - reused
 *  here by (matching) extern declaration only, per project convention.
 * ---------------------------------------------------------------------- */
extern void uart1_ier_thre_enable(void);	/* FUN_c000fd50, uart1_midi_queue.c - TX THRE IRQ gate */
extern void uart1_ier_thre_disable(void);	/* FUN_c000fd74, uart1_midi_queue.c - TX THRE IRQ gate */
extern int  irq_save_and_disable(void);	/* FUN_c0005500, crypto_at88.c */
extern void irq_restore(int flags);		/* FUN_c0005510, eva_crt0_tick_glue.c */
extern uint32_t uart_base_select(void *chip, int idx);	/* FUN_c0001a38, soc_periph.c - idx==1 -> UART1, 0x01D0C000 */
extern void chan_status_dispatch(void *obj);	/* FUN_c000fe20, out of range, 568 bytes - TX ring drain/service */

/* ============================================================================
 *  midi_txq_set_realtime_flag - FUN_c0010078, @0xc0010078 (48 bytes)
 * ============================================================================
 *  Sole caller: FUN_c0006a78 (USB-MIDI packet decoder, out of range), call
 *  site 0xc0006ac8, for CIN==0xF ("Single Byte") events whose payload byte
 *  is a MIDI realtime status (0xF8-0xFF). Real-time bytes bypass the TX
 *  byte ring entirely and are instead OR'd as a single bit into a
 *  "pending realtime" flags byte at chan_status_obj+0x406, guarded by the
 *  same TX THRE IRQ gate the byte-ring pushers use below - consistent
 *  with these being serviced with a shorter/urgent path by the out-of-
 *  range chan_status_dispatch rather than queued in-order with regular
 *  data bytes.
 *
 *  The caller computes `mask = 1 << ((byte1 + 8) & 0xff)`, which for
 *  byte1 in [0xF8,0xFF] evaluates to `1 << (byte1 - 0xF8)`, i.e. bit 0 =
 *  0xF8 (Timing Clock) ... bit 7 = 0xFF (System Reset) - independently
 *  confirmed by the out-of-range midi_pop_raw_record (FUN_c00109dc),
 *  whose own RX-side mirror of this flags byte reconstructs the original
 *  status as `bit_index - 8` (i.e. `bit_index + 0xf8` as an unsigned
 *  byte), the exact inverse mapping.
 * ---------------------------------------------------------------------- */
void midi_txq_set_realtime_flag(void *obj, uint8_t mask)	/* FUN_c0010078 */
{
	uint8_t *p = (uint8_t *)obj + 0x406;	/* DAT_c00100a8 = 0x406 */

	uart1_ier_thre_disable();
	*p |= mask;
	uart1_ier_thre_enable();
}

/* ============================================================================
 *  midi_txq_push1 - FUN_c00100ac, @0xc00100ac (112 bytes)
 * ============================================================================
 *  Pushes a single byte into the 1024-byte TX ring at chan_status_obj+0.
 *  If the ring's count field (+0x404) is already above capacity (0x3ff,
 *  i.e. >=1024), repeatedly disables the TX THRE IRQ and calls the
 *  out-of-range chan_status_dispatch to force a drain/service pass before
 *  re-checking - a busy-wait-and-drain, not a true blocking wait. Once
 *  room exists: writes `byte` at the current write-index (+0x400, wrapped
 *  mod 0x400 AFTER this write - matches Ghidra's post-increment-then-mask
 *  ordering exactly), then bumps count by 1 under the TX THRE IRQ gate.
 *
 *  Sole caller: FUN_c0006a78, call site 0xc0006afc (CIN 0x5 and the
 *  byte1<0xF8 half of CIN 0xF).
 * ---------------------------------------------------------------------- */
void midi_txq_push1(void *obj, uint8_t byte)	/* FUN_c00100ac */
{
	uint8_t *base = (uint8_t *)obj;
	uint16_t *widx = (uint16_t *)(base + 0x400);
	uint16_t *count = (uint16_t *)(base + 0x404);	/* DAT_c001011c = 0x404 */
	uint16_t idx;

	while (*count > 0x3ff) {
		uart1_ier_thre_disable();
		chan_status_dispatch(obj);
	}

	idx = *widx;
	*widx = (idx + 1) & 0x3ff;
	base[idx] = byte;

	uart1_ier_thre_disable();
	(*count)++;
	uart1_ier_thre_enable();
}

/* ============================================================================
 *  midi_txq_push2 - FUN_c0010120, @0xc0010120 (148 bytes)
 * ============================================================================
 *  Same TX ring as midi_txq_push1, but pushes 2 bytes atomically and waits
 *  for headroom of 2 (drain-wait threshold 0x3fe = 0x400-2, matching the
 *  real decompile's `DAT_c00101b4 - 6U` arithmetic once DAT_c00101b4's
 *  resolved value of 0x404 is substituted: 0x404-6 = 0x3fe). Count is
 *  bumped by 2 in one TX THRE IRQ-guarded step at the end, not per-byte.
 *
 *  Sole caller: FUN_c0006a78, call site 0xc0006b34 (CIN 0x2/0x6/0xC/0xD).
 * ---------------------------------------------------------------------- */
void midi_txq_push2(void *obj, uint8_t byte0, uint8_t byte1)	/* FUN_c0010120 */
{
	uint8_t *base = (uint8_t *)obj;
	uint16_t *widx = (uint16_t *)(base + 0x400);
	uint16_t *count = (uint16_t *)(base + 0x404);	/* DAT_c00101b4 = 0x404 */
	uint16_t idx;

	while (*count >= 0x3fe) {
		uart1_ier_thre_disable();
		chan_status_dispatch(obj);
	}

	idx = *widx;
	*widx = (idx + 1) & 0x3ff;
	base[idx] = byte0;
	idx = *widx;
	*widx = (idx + 1) & 0x3ff;
	base[idx] = byte1;

	uart1_ier_thre_disable();
	*count = (uint16_t)(*count + 2);
	uart1_ier_thre_enable();
}

/* ============================================================================
 *  midi_txq_push3 - FUN_c00101b8, @0xc00101b8 (172 bytes)
 * ============================================================================
 *  Same TX ring, 3-byte atomic push, drain-wait threshold 0x3fd
 *  (= 0x400-3, matching the real decompile's `DAT_c0010264 - 7U` once
 *  DAT_c0010264 = 0x404 is substituted... note the decompile's OWN loop
 *  condition literally uses `DAT_c0010268` == 0x3fd directly for the
 *  first (pre-loop) comparison and recomputes `DAT_c0010264 - 7U` (also
 *  0x3fd) inside the loop body for the repeat checks - same value, two
 *  different constant-folded expressions in the original binary, both
 *  transcribed faithfully here as the single threshold they both equal).
 *  Count is bumped by 3 in one TX THRE IRQ-guarded step at the end.
 *
 *  Sole caller: FUN_c0006a78, call site 0xc0006b1c (CIN 0x3/0x4/0x7/
 *  0x8-0xB/0xE).
 * ---------------------------------------------------------------------- */
void midi_txq_push3(void *obj, uint8_t byte0, uint8_t byte1, uint8_t byte2)	/* FUN_c00101b8 */
{
	uint8_t *base = (uint8_t *)obj;
	uint16_t *widx = (uint16_t *)(base + 0x400);
	uint16_t *count = (uint16_t *)(base + 0x404);	/* DAT_c0010264 = 0x404 */
	uint16_t idx;

	while (*count >= 0x3fd) {	/* DAT_c0010268 = 0x3fd */
		uart1_ier_thre_disable();
		chan_status_dispatch(obj);
	}

	idx = *widx;
	*widx = (idx + 1) & 0x3ff;
	base[idx] = byte0;
	idx = *widx;
	*widx = (idx + 1) & 0x3ff;
	base[idx] = byte1;
	idx = *widx;
	*widx = (idx + 1) & 0x3ff;
	base[idx] = byte2;

	uart1_ier_thre_disable();
	*count = (uint16_t)(*count + 3);
	uart1_ier_thre_enable();
}

/* ============================================================================
 *  uart1_rx_irq_enable / uart1_rx_irq_disable -
 *  FUN_c001026c (@0xc001026c, 48 bytes) / FUN_c0010290 (@0xc0010290, 48
 *  bytes)
 * ============================================================================
 *  Set/clear IER bit 0x1 (RHR - Receiver Data Available interrupt enable)
 *  on the same fixed UART1 base as uart1_midi_queue.c's own uart1_ier_
 *  thre_enable/_disable (which toggle bit 0x2, THRE, the TX-side sibling
 *  of this RX-side pair). Both take no arguments and touch no per-object
 *  state - confirmed shared, hardware-global primitives.
 *
 *  DAT_c001028c/DAT_c00102b0 both resolve to `-0x3ff1ff98` == 0xC00E0068,
 *  the project-wide "shared context handle" constant passed as the dead
 *  first argument to uart_base_select everywhere else in this project
 *  (cpsoc.c, mcasp.c, i2c_by_gpio.c, uart1_midi_queue.c, soc_irq_gate.c
 *  all independently document the exact same literal).
 *
 *  Callers (per xrefs_to, all three out of this range): all in
 *  FUN_c00109dc (midi_pop_raw_record, the RX ring's own consumer) -
 *  used there exactly as expected, as a critical-section guard bracketing
 *  RX-ring index/count/flags mutation, the RX-side mirror of how this
 *  file's own midi_txq_* functions use the TX THRE gate.
 * ---------------------------------------------------------------------- */
void uart1_rx_irq_enable(void)		/* FUN_c001026c */
{
	uint32_t base = uart_base_select((void *)0 /* DAT_c001028c = 0xC00E0068, dead arg */, 1);

	*(uint32_t *)(base + 4) |= 1;
}

void uart1_rx_irq_disable(void)	/* FUN_c0010290 */
{
	uint32_t base = uart_base_select((void *)0 /* DAT_c00102b0 = 0xC00E0068, dead arg */, 1);

	*(uint32_t *)(base + 4) &= ~1u;
}

/* ============================================================================
 *  midi_rxq_push_record - FUN_c00102d8, @0xc00102d8 (164 bytes)
 * ============================================================================
 *  Pushes one 4-byte raw record (`*rec`, treated as an opaque undefined4 -
 *  the real per-byte layout {status,d1,d2,rs_mode} is established by the
 *  out-of-range consumer midi_pop_raw_record/FUN_c00109dc, per eva_crt0_
 *  tick_glue.c's own documentation, not independently re-derived here)
 *  into the 128-entry RX record ring at chan_status_obj+0x418, IF the
 *  ring's count field (+0x61c) is below capacity (0x80 = 128 entries):
 *  stores the record at `+0x418 + widx*4`, advances widx (+0x618, wrapped
 *  mod 0x80), and bumps count by 1 under irq_save_and_disable/irq_restore
 *  (NOT the UART1 IER gate the TX side and uart1_rx_irq_* use - a plain
 *  global IRQ mask/restore instead, matching crypto_at88.c's own
 *  established pair). Returns 1 on success.
 *
 *  On overflow (count already >= 0x80): does NOT drop the incoming record
 *  silently - instead resets the entire running-status parser state:
 *  count (+0x61c) and the saved-status byte (+0x414) both zeroed, an
 *  extra flag byte (+0x417) zeroed, the "current dispatch callback"
 *  pointer-to-member pair (+0x40c/+0x410) reloaded from a fixed 8-byte
 *  descriptor at 0xC001FB84 (DAT_c0010388, see file header - target not
 *  independently resolved), the RX ring's read-index (+0x61a) AND
 *  write-index (+0x618) both zeroed, and the RX-side realtime-pending
 *  flags byte (+0x40a) zeroed. Returns 0. Net effect: a full RX-parser
 *  reset rather than a partial queue drop, consistent with "the consumer
 *  fell behind badly enough that resuming mid-stream isn't safe, so
 *  re-synchronize on the next status byte instead."
 *
 *  Sole caller: the out-of-range chan_dispatch_probe (FUN_c00103e4), its
 *  own `else` branch (`uVar6 < 0xf8` false, i.e. the incoming UART1 RX
 *  byte IS a realtime status 0xF8-0xFF) - builds a local 4-byte record
 *  `{status, 0, 0, 0}` on its own stack and passes its address here.
 * ---------------------------------------------------------------------- */
void midi_rxq_push_record(void *obj, const uint32_t *rec)	/* FUN_c00102d8 */
{
	uint8_t *base = (uint8_t *)obj;
	uint16_t *widx = (uint16_t *)(base + 0x618);	/* DAT_c0010380 = 0x618 */
	uint16_t *count = (uint16_t *)(base + 0x61c);	/* DAT_c001037c = 0x61c */
	uint32_t *ring = (uint32_t *)(base + 0x418);	/* DAT_c0010384 = 0x417, entry base 0x418 */
	int flags;

	if (*count < 0x80) {
		ring[*widx] = *rec;
		*widx = (uint16_t)((*widx + 1) & 0x7f);

		flags = irq_save_and_disable();
		(*count)++;
		irq_restore(flags);
		return;
	}

	*count = 0;
	base[0x417] = 0;	/* DAT_c0010384 = 0x417 */

	/* reload default dispatch-callback pair from the fixed descriptor
	 * table entry 0 (0xC001FB84, DAT_c0010388) - target not resolved,
	 * see file header STILL OPEN */
	{
		extern const uint32_t midi_dispatch_reset_table0[2];	/* DAT_c0010388 = 0xC001FB84 */

		*(uint32_t *)(base + 0x40c) = midi_dispatch_reset_table0[0];
		*(uint32_t *)(base + 0x410) = midi_dispatch_reset_table0[1];
	}

	*(uint16_t *)(base + 0x61a) = 0;	/* DAT_c001038c = 0x61a, RX read-index */
	*widx = 0;				/* DAT_c001038c - 2 = 0x618, RX write-index */
	base[0x40a] = 0;			/* DAT_c0010390 = 0x40a, RX realtime-pending flags */
}

/* ============================================================================
 *  midi_dispatch_reset_2byte / midi_dispatch_reset_3byte -
 *  FUN_c0010394 (@0xc0010394, 32 bytes) / FUN_c00103bc (@0xc00103bc, 32
 *  bytes)
 * ============================================================================
 *  Both store the incoming status byte at chan_status_obj+0x414 (the
 *  running-status parser's "current status" slot - same field
 *  midi_rxq_push_record's own overflow path zeroes) and then reload the
 *  dispatch-callback pair (+0x40c/+0x410) from a FIXED 8-byte descriptor,
 *  DIFFERENT for each function: 0xC001FB8C (DAT_c00103b8) for the 2-byte
 *  variant, 0xC001FB94 (DAT_c00103e0) for the 3-byte variant - 8 bytes
 *  apart from each other and from midi_rxq_push_record's own default
 *  entry (0xC001FB84), strongly suggesting a single 3-entry table of
 *  {code*, ptrdiff_t} descriptors indexed 0/1/2 (see file header).
 *
 *  Sole callers: the out-of-range chan_dispatch_probe (FUN_c00103e4),
 *  from its `uVar6 < 0xf0` branch (i.e. a channel-voice status byte,
 *  0x80-0xEF, arriving as the next UART1 RX byte while nothing else
 *  claimed it first) - selects between the two by
 *  `(status + 0x40) & 0xff < 0x20`, which is true for status in
 *  0xC0-0xDF (Program Change / Channel Pressure - the two 2-data-byte-
 *  total MIDI channel messages) and false for status in 0x80-0xBF /
 *  0xE0-0xEF (Note Off/On/PolyPressure/CC/PitchBend - the 3-data-byte-
 *  total channel messages). That split is exactly what the names below
 *  encode: "how many more bytes does this status expect before a full
 *  record gets pushed via midi_rxq_push_record" - inferred from
 *  chan_dispatch_probe's own call-site arithmetic, not independently
 *  confirmed by reading the callback targets themselves (STILL OPEN,
 *  see file header).
 * ---------------------------------------------------------------------- */
void midi_dispatch_reset_2byte(void *obj, uint8_t status)	/* FUN_c0010394 */
{
	uint8_t *base = (uint8_t *)obj;
	extern const uint32_t midi_dispatch_reset_table1[2];	/* DAT_c00103b8 = 0xC001FB8C */

	base[0x414] = status;	/* DAT_c00103b4 = 0x414 */
	*(uint32_t *)(base + 0x40c) = midi_dispatch_reset_table1[0];
	*(uint32_t *)(base + 0x410) = midi_dispatch_reset_table1[1];
}

void midi_dispatch_reset_3byte(void *obj, uint8_t status)	/* FUN_c00103bc */
{
	uint8_t *base = (uint8_t *)obj;
	extern const uint32_t midi_dispatch_reset_table2[2];	/* DAT_c00103e0 = 0xC001FB94 */

	base[0x414] = status;	/* DAT_c00103dc = 0x414 */
	*(uint32_t *)(base + 0x40c) = midi_dispatch_reset_table2[0];
	*(uint32_t *)(base + 0x410) = midi_dispatch_reset_table2[1];
}
