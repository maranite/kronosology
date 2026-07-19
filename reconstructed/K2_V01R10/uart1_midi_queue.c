/* SPDX-License-Identifier: GPL-2.0 */
/*
 * uart1_midi_queue.c - K2 port of K1_V06R06/uart1_midi_queue.c
 * (0xc000fc48-0xc000fe20 second cluster, 5 functions there: UART1 16550-
 * shaped hardware driver + 128-entry MIDI-realtime-byte TX ring). Migrated
 * as part of the MIDI-subsystem cluster pass, 2026-07-19.
 *
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json), queried via query_dump_k2.py.
 * No live Ghidra MCP calls this pass.
 *
 * LOCATION METHOD: soc_periph.c's own K2 port already resolved
 * `uart1_base_get` (FUN_c000180c, K1's own 3-way uart_base_select's real K2
 * counterpart - see that file's own "UART SHRINKS" finding: K2 has only a
 * single, no-parameter UART1-only accessor, no confirmed UART0/UART2
 * siblings). This pass swept every caller of that function and found a
 * single, self-contained ~2000-byte cluster (0xc0011010-0xc0011a80) that
 * covers BOTH this file's own K1 territory AND K1's own
 * midi_chan_status_queues.c territory back to back with no gap - see that
 * file's own K2 port for the second half.
 *
 * HEADLINE FINDING: identical to every other file in this pass - this
 * cluster is essentially UNCHANGED between K1 and K2, migrated wholesale
 * into a single contiguous address run (unlike K1, where the UART1 driver
 * and the MIDI status-queue cluster sat in two separate files). All 5 of
 * K1's own functions here have a confirmed K2 counterpart, 3 of them at an
 * exact Ghidra-reported byte size.
 *
 * K1 vs K2 function map:
 *   uart1_channel_init          K1 FUN_c000fc48 (264B) -> K2 FUN_c0011010 (260B)
 *   uart1_tx_byte                K1 FUN_c000fd24 (52B)  -> K2 FUN_c00110e8 (52B)
 *   uart1_ier_thre_enable         K1 FUN_c000fd50 (44B)  -> K2 FUN_c0011124 (44B)
 *   uart1_ier_thre_disable        K1 FUN_c000fd74 (48B)  -> K2 FUN_c0011144 (44B)
 *   uart1_queue_push_realtime_byte K1 FUN_c000fd98 (120B) -> K2 FUN_c0011188 (120B)
 * 5/5 K1 functions matched.
 *
 * "UART SHRINKS" reconfirmed at the call site: uart1_base_get takes a dead
 * first argument (DAT_c00110e4/_c001111c/_c0011140/_c0011160, all resolve
 * to the SAME literal -0x3ff1ffb4 == 0xC00E004C, the project-wide "shared
 * context handle" dead-argument constant - a DIFFERENT literal than K1's
 * own 0xC00E0068, but the identical role) and always returns the fixed
 * UART1 base regardless, matching K1's own `uart_base_select(chip, 1)`
 * dead-handle pattern exactly.
 *
 * REAL, CONFIRMED DIFFERENCE FROM K1 - uart1_tx_byte: K1's own version is a
 * pure, side-effect-free "write byte to THR" primitive. K2's version
 * (FUN_c00110e8) does MORE: after writing the byte (via a one-level-deeper
 * callee, FUN_c0003230, rather than a direct MMIO write - not independently
 * traced this pass, presumed a thin THR-write wrapper given its call
 * shape), it ALSO sets a flag global (DAT_c0011120 -> 0xC01CCEED) to 1
 * whenever the byte being sent is NOT 0xFE (Active Sensing) - the exact
 * same "drop redundant 0xFE" condition K1's own uart1_queue_push_realtime_byte
 * tests on its OWN drop-flag global. Genuinely new coupling between the TX
 * primitive and the queue's drop-filter state that K1's version did not
 * have - transcribed faithfully, not smoothed away.
 *
 * Struct field offsets confirmed IDENTICAL to K1 throughout (+0x400/+0x402/
 * +0x404/+0x406/+0x40a/+0x40c/+0x410/+0x417/+0x407/+0x408/+0x409/+0x618/
 * +0x61a/+0x61c on the shared status object) - not re-derived field-by-field
 * in each function's own comment below except where noted.
 *
 * STILL OPEN (same items K1 left open, not independently re-resolved this
 * pass): the 8-byte ROM constant copied into +0x40c/+0x410 (K2's own
 * DAT_c00110dc -> 0xC0027F38); offsets +0x34/+0x20/+0x24/+0x30 of the UART1
 * register file, transcribed as raw MMIO writes only; FUN_c0003230's own
 * real body (the THR-write callee, one level deeper than K1's own direct
 * write - not traced this pass).
 */

#include <stdint.h>
#include <stdbool.h>

extern uint32_t chan_status_obj;			/* usbdc_midi_status_glue.c's own name/type, this file's own status object */
extern uint32_t uart1_base_get(void *chip);		/* FUN_c000180c, soc_periph.c (K1: uart_base_select(chip,1), FUN_c0001a38) */
extern uint32_t omap_usbdc_reloc(uint32_t offset);	/* FUN_c000a728, omap_l137_usbdc.c (K1: FUN_c0009194) */

/* DAT_c00110d0/DAT_c00110d4 - reloc-base and its own derived ring-buffer
 * pointer, same idiom as K1. */
extern uint8_t *uart_queue_buf_ptr;	/* DAT_c00110d4 / DAT_c0011208 = SAME slot, 0xC01CCEE8 */
extern uint8_t  uart_queue_drop_flag;	/* DAT_c0011200 -> 0xC01CCEEC */

/* ---------------------------------------------------------------------- *
 *  uart1_channel_init - FUN_c0011010, @0xc0011010 (260 bytes, K1:
 *  FUN_c000fc48, 264 bytes). Identical to K1: zeroes ring-buffer/status
 *  fields on `obj` (chan_status_obj at its own real call site), derives
 *  uart_queue_buf_ptr = omap_usbdc_reloc(reloc_base) + 0xe280 (SAME offset
 *  as K1), copies an 8-byte ROM constant pair into +0x40c/+0x410 (DAT_
 *  c00110dc -> 0xC0027F38, content not resolved this pass either), then
 *  configures UART1 (base via uart1_base_get - no idx parameter, "UART
 *  SHRINKS" reconfirmed, see file header) with the SAME register sequence
 *  as K1: +0x34=0, LCR(+0xc)=0x80 then 3, FCR(+8)=1 then 7, MDR(+0x30)=
 *  fixed constant (DAT_c0003208), IER(+4)=5.
 *
 *  Sole caller (per xrefs_to): FUN_c0009838 at call site 0xc0009880 - a
 *  larger bring-up routine (out of this file's own scope), the K2
 *  counterpart of K1's own FUN_c00074bc bring-up sequence.
 * ---------------------------------------------------------------------- */
void uart1_channel_init(void *obj)	/* FUN_c0011010 */
{
	extern uint32_t uart_queue_reloc_base;		/* DAT_c00110d0 -> 0xC01CCB10-shaped reloc base */
	extern uint32_t uart_queue_rom_pair[2];	/* DAT_c00110dc -> 0xC0027F38, content NOT resolved this pass */
	extern uint32_t uart1_mdr_const;		/* DAT_c0003208, shared firmware-wide constant */
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
	*(uint8_t  *)(p + 0x408) = 0;	/* ring read index */
	*(uint8_t  *)(p + 0x409) = 0;	/* ring count */

	uartbase = uart1_base_get((void *)0 /* dead arg, 0xC00E004C shared context handle */);
	*(uint32_t *)(uartbase + 0x34) = 0;
	*(uint32_t *)(uartbase + 0xc)  = 0x80;		/* LCR: DLAB set */
	*(uint32_t *)(uartbase + 0x24) = 1;
	*(uint32_t *)(uartbase + 0x20) = 0x2c;
	*(uint32_t *)(uartbase + 0xc)  = 3;		/* LCR: 8N1, DLAB cleared */
	*(uint32_t *)(uartbase + 8)    = 1;		/* FCR: FIFO enable */
	*(uint32_t *)(uartbase + 8)    = 7;		/* FCR: + clear RX/TX FIFOs */
	*(uint32_t *)(uartbase + 0x30) = uart1_mdr_const;
	*(uint32_t *)(uartbase + 4)    = 5;		/* IER: RDA + RLS */
}

/* ---------------------------------------------------------------------- *
 *  uart1_tx_byte - FUN_c00110e8, @0xc00110e8 (52 bytes, K1: FUN_c000fd24,
 *  52 bytes - EXACT match). REAL, CONFIRMED DIFFERENCE FROM K1: also sets
 *  the drop-flag-adjacent global DAT_c0011120 (-> 0xC01CCEED) to 1 whenever
 *  `byte != 0xFE` - see file header. The actual THR write itself is routed
 *  through FUN_c0003230 (one level deeper than K1's own direct MMIO write,
 *  not independently traced this pass).
 *
 *  4 confirmed K2 callers, all from FUN_c0011210 (the K2 counterpart of
 *  K1's own out-of-range chan_status_dispatch/FUN_c000fe20 - genuinely
 *  decompilable in this K2 dump, though still out of this file's own
 *  assigned scope, matching K1's own boundary).
 * ---------------------------------------------------------------------- */
extern void uart1_thr_write(uint32_t base, uint8_t byte);	/* FUN_c0003230, out of range - presumed THR-write wrapper */

void uart1_tx_byte(uint8_t byte)	/* FUN_c00110e8 */
{
	extern uint8_t uart1_tx_flag;	/* DAT_c0011120 -> 0xC01CCEED */
	uint32_t base = uart1_base_get((void *)0 /* dead arg */);

	uart1_thr_write(base, byte);
	if (byte != 0xFE)
		uart1_tx_flag = 1;
}

/* ---------------------------------------------------------------------- *
 *  uart1_ier_thre_enable / uart1_ier_thre_disable -
 *  FUN_c0011124 (@0xc0011124, 44 bytes, K1: FUN_c000fd50, 44 bytes - EXACT
 *  match) / FUN_c0011144 (@0xc0011144, 44 bytes, K1: FUN_c000fd74, 48
 *  bytes). Identical IER bit-0x2 (THRE) set/clear on the fixed UART1 base.
 *  5/9 confirmed K2 callers respectively, matching K1's own caller-count
 *  evidence closely (this file's own uart1_queue_push_realtime_byte plus
 *  the midi_txq_push1/2/3/midi_txq_set_realtime_flag cluster - see
 *  midi_chan_status_queues.c's own K2 port).
 * ---------------------------------------------------------------------- */
void uart1_ier_thre_enable(void)	/* FUN_c0011124 */
{
	uint32_t base = uart1_base_get((void *)0 /* dead arg */);

	*(uint32_t *)(base + 4) |= 2;
}

void uart1_ier_thre_disable(void)	/* FUN_c0011144 */
{
	uint32_t base = uart1_base_get((void *)0 /* dead arg */);

	*(uint32_t *)(base + 4) &= ~2u;
}

/* ---------------------------------------------------------------------- *
 *  uart1_queue_push_realtime_byte - FUN_c0011188, @0xc0011188 (120 bytes,
 *  K1: FUN_c000fd98, 120 bytes - EXACT match). Identical drop-filter logic
 *  (drop redundant 0xFE when uart_queue_drop_flag is set), identical
 *  128-entry ring push (write index +0x407, wrap &0x7f) and IER-guarded
 *  count increment (+0x409).
 *
 *  Confirmed K2 callers: FUN_c000a370 and FUN_c000bc84 - TWO callers,
 *  unlike K1's own single confirmed caller (FUN_c000a980) - a real,
 *  confirmed widening of this function's own reach in K2, not
 *  independently traced further this pass.
 * ---------------------------------------------------------------------- */
bool uart1_queue_push_realtime_byte(void *obj, uint8_t byte)	/* FUN_c0011188 */
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

/* Still open (same items K1 left open, not independently re-resolved this
 * pass): the 8-byte ROM constant at DAT_c00110dc (0xC0027F38); UART1
 * register offsets +0x34/+0x20/+0x24/+0x30 not matched against a TRM;
 * FUN_c0003230's own real body; the real consumer of this ring buffer
 * (presumably FUN_c0011210, the K2 counterpart of chan_status_dispatch,
 * given its own 4 direct uart1_tx_byte calls - not independently confirmed
 * this pass). */
