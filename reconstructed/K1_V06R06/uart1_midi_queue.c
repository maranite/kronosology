/* SPDX-License-Identifier: GPL-2.0 */
/*
 * uart1_midi_queue.c - assigned address range 0xc000fa64-0xc000fe20 sweep,
 * second cluster: 0xc000fc48-0xc000fd98, five functions. A small UART1
 * hardware driver (16550-shaped register layout, matched against
 * soc_periph.c's own confirmed OMAP-L1x/DA850 UART1 base 0x01D0C000) plus a
 * 128-entry MIDI-realtime-byte ring buffer feeding it, both operating on
 * fields of the shared "chan_status_obj" global usbdc_midi_status_glue.c
 * already extensively documents (0xC01CC12C) - real, address-confirmed
 * evidence this struct is bigger and more cross-subsystem than that file's
 * own already-covered offsets (+4/+8/+0x14/+0x61c) suggested.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled/
 * all_data.json), 2026-07-18 pass. No live Ghidra MCP calls this pass (the
 * bridge is flagged concurrency-unsafe under this project's own parallel
 * work - see README).
 *
 * ANCHOR: NONE. No "../<name>.cpp" string sits anywhere near this range
 * (same conclusion this pass's sibling agents reached for their own
 * neighboring no-anchor ranges - wire_dispatch.c, panelbus_dispatch.c,
 * usbdc_midi_status_glue.c). Attribution here rests on: (1) the UART1
 * register-offset evidence below, cross-checked against soc_periph.c's own
 * independently-derived, TRM-matched uart_base_select; (2) the exact
 * address match between this cluster's own struct-base constants and
 * usbdc_midi_status_glue.c's already-documented chan_status_obj/ring-2
 * globals.
 *
 * =========================================================================
 *  HARDWARE EVIDENCE - UART1, base 0x01D0C000
 * =========================================================================
 *  uart1_channel_init (below) writes a small register file at offsets
 *  +0x34/+0xc/+0x24/+0x20/+8/+0x30/+4 off a base obtained via
 *  soc_periph.c's own `uart_base_select(handle, 1)` (idx==1 -> UART1,
 *  0x01D0C000, an exact TRM match per that file's own header). The write
 *  SEQUENCE at offset +0xc is a textbook 16550-family baud/format dance:
 *  0x80 (DLAB set - divisor-latch access enabled) immediately followed by
 *  3 (DLAB cleared, 8 data bits/no parity/1 stop bit) - exactly the
 *  standard "enter divisor-latch mode, (elsewhere) write the divisor,
 *  leave divisor-latch mode with the real line format" sequence, strong
 *  independent confirmation that +0xc is LCR. Offset +8's own sequence (1
 *  then 7) matches 16550 FCR just as cleanly: 1 = FIFO enable, 7 = FIFO
 *  enable + clear-RX-FIFO + clear-TX-FIFO, the standard "enable then reset
 *  both FIFOs" idiom. Offset +4 (IER) is written 5 (bits 0+2 = Receiver
 *  Data Available + Receiver Line Status interrupt enable) - consistent
 *  with a UART used for a real duplex link, not TX-only. Offsets
 *  +0x34/+0x20/+0x24/+0x30 are written raw constants (0/0x2c/1/0x6001)
 *  with no confirmed match against the public DA850 UART register
 *  extension map - left as opaque MMIO writes, not guessed at.
 *
 *  uart1_ier_thre_enable/_disable (below) toggle IER bit 0x2 (Transmit
 *  Holding Register Empty interrupt enable) - the standard 16550
 *  TX-interrupt gate. Confirmed generic/shared, NOT specific to this
 *  file's own ring buffer: 5 and 9 real callers respectively (per
 *  xrefs_to), spanning at least one more, unrelated ring-buffer
 *  implementation at 0xc0010078-0xc00101b8 (out of this file's range, a
 *  DIFFERENT struct entirely - operates on `param_1+0x400`/a distinct
 *  `DAT_c001011c` index field, not chan_status_obj). Both are called
 *  bracketing a critical section here (see uart1_queue_push_realtime_byte
 *  below), consistent with "disable the TX-empty interrupt as an
 *  interrupt-safe guard around a shared counter update," not a
 *  literal "start/stop transmitting" pair.
 *
 * =========================================================================
 *  STRUCT EVIDENCE - chan_status_obj, 0xC01CC12C
 * =========================================================================
 *  uart1_channel_init's own struct-base argument resolves (via
 *  DAT_c0007628, the constant its sole real caller passes) to EXACTLY
 *  0xC01CC12C - the same address usbdc_midi_status_glue.c's own header
 *  names `chan_status_obj` and documents fields for at +4/+8/+0x14/+0x61c.
 *  This file's own fields (+0x400 through +0x630, see below) are a
 *  DIFFERENT, non-overlapping offset range of that SAME object - real,
 *  address-confirmed evidence chan_status_obj is a large composite struct
 *  (at least 0x630+ bytes) shared across USB endpoint-0 state
 *  (omap_l137_usbdc.c's own "dev" struct, offsets +0x30/+0x34/+0x401/
 *  +0x406/+0x408/+0x40b - suspiciously close to this file's own
 *  +0x400-range fields, NOT confirmed to be the literal same struct base,
 *  flagged as a real open cross-file lead), USB/MIDI status
 *  (usbdc_midi_status_glue.c), and this file's own UART1 TX queue -
 *  consistent with this project's own README already calling
 *  chan_status_obj "touched by nearly every function in [that] section."
 *  Re-declared here as a plain `extern uint32_t chan_status_obj;`, same
 *  name/type usbdc_midi_status_glue.c already uses, per this project's own
 *  per-file extern-redeclaration convention (not a shared header).
 */

#include <stdint.h>
#include <stdbool.h>

extern uint32_t chan_status_obj;			/* 0xC01CC12C, usbdc_midi_status_glue.c's own name/type reused */
extern uint32_t uart_base_select(void *chip, int idx);	/* FUN_c0001a38, soc_periph.c (idx==1 -> UART1, 0x01D0C000) */
extern uint32_t omap_usbdc_reloc(uint32_t offset);		/* FUN_c0009194, omap_l137_usbdc.c */

/* DAT_c000fd10 (written by uart1_channel_init) and DAT_c000fe18 (read by
 * uart1_queue_push_realtime_byte) are the SAME global slot, 0xc01cd4f0 -
 * one extern declared once here rather than two locally-scoped copies. */
extern uint8_t *uart_queue_buf_ptr;	/* DAT_c000fd10 / DAT_c000fe18 = 0xc01cd4f0 */
extern uint8_t  uart_queue_drop_flag;	/* DAT_c000fe10 = 0xc01cd4f4, immediately adjacent slot */

/* ---------------------------------------------------------------------- *
 *  uart1_channel_init - FUN_c000fc48, @0xc000fc48 (264 bytes)
 *
 *  Zeroes a cluster of ring-buffer/status fields on `obj` (chan_status_obj
 *  at every real call site this pass traced), sets up a TX buffer pointer,
 *  copies an 8-byte ROM constant pair into the struct, THEN configures the
 *  UART1 hardware register file. Real field layout, transcribed exactly
 *  from the decompile's own offset arithmetic (all offsets relative to
 *  `obj`):
 *
 *    +0x400 (u16) = 0        +0x618 (u16) = 0   \_ two apparently-mirrored
 *    +0x402 (u16) = 0        +0x61a (u16) = 0    | field groups, second
 *    +0x404 (u16) = 0        +0x61c (u16) = 0    | group offset exactly
 *    +0x406 (u8)  = 0                            / +0x218 above the first -
 *                                                   same mirrored-region
 *                                                   shape clcdc.c's own
 *                                                   progress-bar rows
 *                                                   documented (real
 *                                                   precedent for this
 *                                                   firmware's own coding
 *                                                   style, not claimed as
 *                                                   the SAME mechanism).
 *    +0x40a (u8)  = 0                            - a flag/status byte
 *    +0x40c (u32) = word 0 of an 8-byte ROM       \_ copied verbatim from
 *                    constant pair (0xc001fb7c)    | 0xc001fb7c
 *    +0x410 (u32) = word 1 of the same pair       /  (DAT_c000fd18[0..1])
 *    +0x417 (u8)  = 0                            - another flag/status byte
 *    +0x407 (u8)  = 0  \
 *    +0x408 (u8)  = 0   > the ring buffer's own write-index/read-index/
 *    +0x409 (u8)  = 0  /  count control triple (confirmed by
 *                         uart1_queue_push_realtime_byte below, which reads
 *                         +0x407 as its write index and increments +0x409
 *                         as its count)
 *
 *  Also derives the ring buffer's real backing storage pointer:
 *  `uart_queue_buf_ptr = omap_usbdc_reloc(uart_queue_reloc_base) + 0xe280`
 *  - same reloc-indirection idiom omap_l137_usbdc.c/midi_engine.c both
 *  already document for USB descriptor memory, applied here with a THIRD,
 *  distinct reloc-base constant (0xc01ccb10) neither of those two files'
 *  own two reloc-base globals (0xc0003b50/0xc0009678) matches.
 *
 *  Finally configures UART1 (base via uart_base_select(handle, 1)) - see
 *  the file header's own hardware-evidence section for the register-offset
 *  reasoning.
 *
 *  NEEDS LIVE QUERY: the 8-byte ROM constant at 0xc001fb7c (copied into
 *  +0x40c/+0x410) - no raw byte content in the static dump, only the
 *  address itself.
 *
 *  Sole caller (per xrefs_to): FUN_c00074bc at call site 0xc0007544,
 *  passing DAT_c0007628 (= 0xC01CC12C = chan_status_obj) as `obj`.
 *  FUN_c00074bc (0xc00074bc, 320 bytes, outside this file's range) is a
 *  larger bring-up routine that ALSO calls this file's sibling
 *  cdix_autoswitch.c indirectly (via FUN_c000ecc4, see that file's own
 *  "THE OUTER STATE MACHINE" note) and cpsoc.c's cpsoc_i2c_dispatch
 *  (FUN_c0007120) four times with the confirmed LED-bargraph register set
 *  (0x78/0x79/0x7a/0x7b) - i.e. this whole area is one shared "channel
 *  object" bring-up sequence spanning USB, CDIX, cpsoc, and this file's
 *  own UART1, not four independent subsystems that happen to run near
 *  each other. Not reconstructed here (out of range) - cited only.
 * ---------------------------------------------------------------------- */
void uart1_channel_init(void *obj)	/* FUN_c000fc48 */
{
	extern uint32_t uart_queue_reloc_base;		/* DAT_c000fd0c = 0xc01ccb10 */
	extern uint32_t uart_queue_rom_pair[2];	/* DAT_c000fd18 = 0xc001fb7c, 8-byte ROM constant, content NOT resolved this pass */
	extern uint32_t uart1_mdr_const;		/* DAT_c0003730 = 0x6001, shared firmware-wide constant, cited elsewhere too */
	uint8_t *p = (uint8_t *)obj;
	uint32_t uartbase;

	*(uint16_t *)(p + 0x404) = 0;
	*(uint16_t *)(p + 0x402) = 0;
	*(uint8_t  *)(p + 0x406) = 0;
	*(uint16_t *)(p + 0x400) = 0;
	*(uint16_t *)(p + 0x61c) = 0;
	*(uint16_t *)(p + 0x61a) = 0;
	*(uint16_t *)(p + 0x618) = 0;
	*(uint8_t  *)(p + 0x40a) = 0;

	uart_queue_buf_ptr = (uint8_t *)(omap_usbdc_reloc(uart_queue_reloc_base) + 0xe280);

	*(uint8_t  *)(p + 0x417) = 0;
	*(uint32_t *)(p + 0x40c) = uart_queue_rom_pair[0];
	*(uint32_t *)(p + 0x410) = uart_queue_rom_pair[1];
	*(uint8_t  *)(p + 0x407) = 0;	/* ring write index */
	*(uint8_t  *)(p + 0x408) = 0;	/* ring read index (not read by any function reconstructed this pass) */
	*(uint8_t  *)(p + 0x409) = 0;	/* ring count */

	uartbase = uart_base_select(0 /* DAT_c000fd20 = 0xC00E0068, shared context handle, dead arg */, 1);
	*(uint32_t *)(uartbase + 0x34) = 0;
	*(uint32_t *)(uartbase + 0xc)  = 0x80;		/* LCR: DLAB set */
	*(uint32_t *)(uartbase + 0x24) = 1;
	*(uint32_t *)(uartbase + 0x20) = 0x2c;
	*(uint32_t *)(uartbase + 0xc)  = 3;		/* LCR: 8N1, DLAB cleared */
	*(uint32_t *)(uartbase + 8)    = 1;		/* FCR: FIFO enable */
	*(uint32_t *)(uartbase + 8)    = 7;		/* FCR: + clear RX/TX FIFOs */
	*(uint32_t *)(uartbase + 0x30) = uart1_mdr_const;	/* 0x6001 */
	*(uint32_t *)(uartbase + 4)    = 5;		/* IER: RDA + RLS */
}

/* ---------------------------------------------------------------------- *
 *  uart1_tx_byte - FUN_c000fd24, @0xc000fd24 (52 bytes)
 *
 *  Direct, unbuffered write to UART1's THR (offset 0, base via
 *  uart_base_select(handle, 1)) - real signature `FUN_c000fd24(uint
 *  param_1)`: `param_1` IS the byte to transmit (masked to 8 bits), NOT an
 *  object pointer - confirmed by the body doing nothing but
 *  `*(uart1_base) = param_1 & 0xff`.
 *
 *  4 callers (per xrefs_to), ALL from the single out-of-range function
 *  FUN_c000fe20 (usbdc_midi_status_glue.c's own `chan_status_dispatch`,
 *  568 bytes, immediately past this file's own assigned upper bound
 *  0xc000fe20 - genuinely the very next function in the image, not
 *  reconstructed by this pass at all). Consistent with chan_status_dispatch
 *  being the real MIDI-realtime-byte transmit path: it likely reads bytes
 *  back out of this file's own ring buffer (see
 *  uart1_queue_push_realtime_byte below) and writes each one straight to
 *  the wire via this function.
 * ---------------------------------------------------------------------- */
void uart1_tx_byte(uint8_t byte)	/* FUN_c000fd24 */
{
	uint32_t *thr = (uint32_t *)uart_base_select(0 /* DAT_c000fd4c = 0xC00E0068, dead arg */, 1);

	*thr = byte;
}

/* ---------------------------------------------------------------------- *
 *  uart1_ier_thre_enable / uart1_ier_thre_disable -
 *  FUN_c000fd50 (@0xc000fd50, 44 bytes) / FUN_c000fd74 (@0xc000fd74, 48
 *  bytes)
 *
 *  Set/clear IER bit 0x2 (THRE - Transmit Holding Register Empty
 *  interrupt enable) on the SAME fixed UART1 base every other function in
 *  this file uses. Both take NO arguments and touch no per-object state -
 *  confirmed shared, hardware-global primitives, not tied to any one
 *  ring/queue implementation:
 *
 *   - uart1_ier_thre_enable:  5 real callers (this file's own
 *     uart1_queue_push_realtime_byte below, plus 4 in the out-of-range
 *     0xc0010078-0xc00101b8 cluster, a DIFFERENT ring buffer entirely -
 *     see that cluster's own note in uart1_queue_push_realtime_byte
 *     below).
 *   - uart1_ier_thre_disable: 9 real callers (this file's own
 *     uart1_queue_push_realtime_byte, that same 0xc0010078-range cluster
 *     x8, and ONE conditional call from FUN_c000fe20/chan_status_dispatch
 *     at 0xc001000c).
 * ---------------------------------------------------------------------- */
void uart1_ier_thre_enable(void)	/* FUN_c000fd50 */
{
	uint32_t base = uart_base_select(0 /* DAT_c000fd70 = 0xC00E0068, dead arg */, 1);

	*(uint32_t *)(base + 4) |= 2;
}

void uart1_ier_thre_disable(void)	/* FUN_c000fd74 */
{
	uint32_t base = uart_base_select(0 /* DAT_c000fd94 = 0xC00E0068, dead arg */, 1);

	*(uint32_t *)(base + 4) &= ~2u;
}

/* ---------------------------------------------------------------------- *
 *  uart1_queue_push_realtime_byte - FUN_c000fd98, @0xc000fd98 (120 bytes)
 *
 *  Pushes ONE byte into `obj`'s 128-entry ring buffer (the same +0x407
 *  write-index/+0x409 count fields uart1_channel_init zeroes above),
 *  subject to a drop filter, then returns whether the push actually
 *  happened. Real signature: `bool FUN_c000fd98(int param_1, char
 *  param_2)`.
 *
 *  Drop-filter logic, transcribed exactly (this reads as "drop redundant
 *  MIDI Active Sensing (0xFE) bytes when the drop-flag is set, otherwise
 *  always push" - `param_2 != -2` is `byte != 0xFE`):
 *
 *      bool push = true;
 *      if (uart_queue_drop_flag != 0)
 *              push = (byte != 0xFE);
 *      if (push) { ...do the push, see below... }
 *      return push;
 *
 *  The push itself:
 *   1. `uart_queue_buf_ptr[obj->widx] = byte` - writes into the backing
 *      buffer uart1_channel_init derived (reloc-base + 0xe280), at the
 *      CURRENT (pre-increment) write index.
 *   2. `obj->widx = (obj->widx + 1) & 0x7f` - 128-entry wraparound.
 *   3. `uart1_ier_thre_disable()`, `obj->count++`, `uart1_ier_thre_enable()`
 *      - the count-field increment is bracketed by the shared IER-bit
 *      disable/enable pair (this file's own uart1_ier_thre_disable/
 *      _enable above), read as an interrupt-safe critical section around
 *      the field a consumer/ISR context presumably also touches (that
 *      consumer is not identified this pass - plausibly inside the
 *      out-of-range FUN_c000fe20/chan_status_dispatch, given its own 4
 *      direct calls to uart1_tx_byte above). NOT confirmed against an
 *      actual ISR - flagged as the most consistent reading of the
 *      disable/increment/enable bracket, not a certainty.
 *
 *  `uart_queue_drop_flag` (DAT_c000fe10 = 0xc01cd4f4) and
 *  `uart_queue_buf_ptr` (DAT_c000fe18 = 0xc01cd4f0) are BOTH re-resolved
 *  here to the exact same two addresses uart1_channel_init above already
 *  uses under the names `uart_queue_buf_ptr`/(undeclared drop-flag) -
 *  confirmed the same 2 adjacent global slots (buffer-pointer variable at
 *  0xc01cd4f0, a flag byte immediately after it at 0xc01cd4f4), not a
 *  coincidence of address arithmetic.
 *
 *  Sole caller (per xrefs_to): FUN_c000a980 at call site 0xc0006e00 - a
 *  316-byte raw incoming-byte-pair scanner (real signature
 *  `FUN_c000a980(undefined4 param_1)`) that reads 4-byte grouped
 *  [tag,data,..,..] records and, for records tagged 0x0f with a data byte
 *  >= 0xf8 (i.e. a MIDI System Realtime status byte carried in a
 *  USB-MIDI-Event-Packet-shaped 4-byte group), calls THIS function with
 *  `obj = DAT_c0006e84` (== 0xC01CC12C == chan_status_obj, confirmed
 *  address match) and `byte = the data byte`. A SIBLING branch (tag 0x1f,
 *  same data-byte range) instead calls the out-of-range FUN_c000dbac with
 *  `DAT_c0006e88` (== 0xC01CAD44 == usbdc_midi_status_glue.c's own
 *  documented "midi_engine.c ring-2 singleton") - i.e. FUN_c000a980 is a
 *  real fan-out point routing incoming realtime MIDI bytes to one of TWO
 *  different queues depending on a tag nibble, one of which is this
 *  file's own ring, the other ring-2. Neither FUN_c000a980 nor
 *  FUN_c000dbac are reconstructed here (both outside this file's own
 *  assigned range) - cited only to complete the trace.
 * ---------------------------------------------------------------------- */
bool uart1_queue_push_realtime_byte(void *obj, uint8_t byte)	/* FUN_c000fd98 */
{
	uint8_t *p = (uint8_t *)obj;
	bool push = true;

	if (uart_queue_drop_flag != 0)
		push = (byte != 0xFE);

	if (push) {
		uint8_t widx = p[0x407];

		uart_queue_buf_ptr[widx] = byte;
		p[0x407] = (widx + 1) & 0x7f;

		uart1_ier_thre_disable();
		p[0x409]++;
		uart1_ier_thre_enable();
	}

	return push;
}

/* Still open:
 *  - The 8-byte ROM constant at 0xc001fb7c (uart_queue_rom_pair) - content
 *    not readable from the static dump.
 *  - Offsets +0x34/+0x20/+0x24/+0x30 of the UART1 register file (values
 *    0/0x2c/1/0x6001) not matched against the public DA850 UART register
 *    extension map - transcribed as raw MMIO writes only.
 *  - +0x408 (the ring's own read-index field, zeroed by uart1_channel_init
 *    but never read by any function reconstructed this pass) - its
 *    consumer is presumably inside the out-of-range FUN_c000fe20
 *    (chan_status_dispatch), not traced this pass.
 *  - Whether the +0x400-range fields this file documents on
 *    chan_status_obj are the SAME struct region omap_l137_usbdc.c's own
 *    "dev" (EP0 software state) uses at its similarly-shaped +0x30/+0x34/
 *    +0x401/+0x406/+0x408/+0x40b offsets, or a coincidental offset
 *    overlap on a struct big enough to hold both independently - flagged,
 *    not resolved (out of both files' own scope to reconcile this pass).
 *  - The real consumer of this ring buffer (almost certainly
 *    FUN_c000fe20/chan_status_dispatch, given its 4 direct uart1_tx_byte
 *    calls, but not independently confirmed to read from THIS ring rather
 *    than some other source) - out of this file's assigned range.
 */
