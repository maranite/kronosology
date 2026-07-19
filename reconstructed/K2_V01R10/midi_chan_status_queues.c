/* SPDX-License-Identifier: GPL-2.0 */
/*
 * midi_chan_status_queues.c - K2 port of K1_V06R06/midi_chan_status_queues.c
 * (0xc0010078-0xc00103e4, 9 functions there: TX byte-ring pushers plus a
 * 128-entry RX record ring, both living on the shared chan_status_obj).
 * Migrated as part of the MIDI-subsystem cluster pass, 2026-07-19.
 *
 * Ground truth: static Ghidra dump of KRONOS2S_V01R10.VSB
 * (all_decompiled_k2.json/all_data_k2.json), queried via query_dump_k2.py.
 * No live Ghidra MCP calls this pass.
 *
 * LOCATION METHOD: found as the direct continuation of
 * uart1_midi_queue.c's own K2 cluster (0xc0011010-0xc0011a80) - see that
 * file's own header for how the whole run was located via soc_periph.c's
 * uart1_base_get. K1 kept the UART1 driver and this TX/RX-queue cluster in
 * two separate files (0xc000fc48-0xc0010078 vs 0xc0010078-0xc00103e4,
 * address-adjacent but distinct files per that pass's own task split); K2
 * migrated both into ONE contiguous run with no gap - a real, confirmed
 * architectural consolidation, not a coverage artifact (independently
 * re-verified: FUN_c0011188, the last function of uart1_midi_queue.c's own
 * K2 range, ends at 0xc0011200; FUN_c0011210, chan_status_dispatch's own K2
 * counterpart, decompiled directly here for cross-reference, starts
 * immediately after; this file's own 9 functions start at 0xc0011468).
 *
 * HEADLINE FINDING: identical to every other file in this pass - this
 * cluster is essentially UNCHANGED between K1 and K2. All 9 of K1's own
 * functions here have a confirmed K2 counterpart, 6 of them at an exact
 * Ghidra-reported byte size, and EVERY struct-offset constant
 * independently re-resolved from K2's own DAT_ pool (0x400/0x404/0x406/
 * 0x40a/0x414/0x417/0x418/0x618/0x61a/0x61c) is bit-for-bit identical to
 * K1's own values - the strongest single confirmation in this whole pass
 * that chan_status_obj's layout carried over completely unchanged.
 *
 * K1 vs K2 function map:
 *   midi_txq_set_realtime_flag  K1 FUN_c0010078 (48B)  -> K2 FUN_c0011468 (48B)
 *   midi_txq_push1               K1 FUN_c00100ac (112B) -> K2 FUN_c001149c (112B)
 *   midi_txq_push2                K1 FUN_c0010120 (148B) -> K2 FUN_c0011510 (148B)
 *   midi_txq_push3                K1 FUN_c00101b8 (172B) -> K2 FUN_c00115a8 (172B)
 *   uart1_rx_irq_enable            K1 FUN_c001026c (48B)  -> K2 FUN_c001165c (44B)
 *   uart1_rx_irq_disable           K1 FUN_c0010290 (48B)  -> K2 FUN_c001167c (44B)
 *   midi_rxq_push_record           K1 FUN_c00102d8 (164B) -> K2 FUN_c00116bc (164B)
 *   midi_dispatch_reset_2byte      K1 FUN_c0010394 (32B)  -> K2 FUN_c0011778 (32B)
 *   midi_dispatch_reset_3byte      K1 FUN_c00103bc (32B)  -> K2 FUN_c00117a0 (32B)
 * 9/9 K1 functions matched, 6 at exact byte size.
 *
 * OUT OF RANGE (cited for context only, NOT reconstructed here, matching
 * K1's own scope boundary):
 *  - FUN_c0011210 (568 bytes) - the K2 counterpart of K1's own out-of-range
 *    chan_status_dispatch/FUN_c000fe20 (uart1_midi_queue.c/
 *    usbdc_midi_status_glue.c both already cite this by K1 address). Unlike
 *    K1, this function IS genuinely decompilable in this K2 static dump
 *    (open item for a future pass, not reconstructed here - out of this
 *    file's own K1-derived scope) - confirmed the real drain-wait target
 *    all 3 midi_txq_push* functions call when the TX ring is too full.
 *  - FUN_c00117c8 (292 bytes) - the K2 counterpart of K1's own
 *    chan_dispatch_probe/FUN_c00103e4, the exclusive upper bound of this
 *    range in K1 too. Confirmed the real caller of
 *    midi_dispatch_reset_2byte/_3byte and midi_rxq_push_record (direct call
 *    sites at 0xc0011868/0xc0011878/0xc001180c) - reproduces K1's own
 *    cross-file finding exactly. Not reconstructed here either.
 *  - FUN_c0011dbc - the K2 counterpart of K1's own midi_pop_raw_record/
 *    FUN_c00109dc, confirmed the real caller of uart1_rx_irq_enable/
 *    _disable (3 call sites each) - the RX ring's own consumer, same role
 *    K1 documented.
 *
 * STILL OPEN (same items K1 left open, not independently re-resolved this
 * pass): the real targets of the three 8-byte pointer-to-member-function
 * descriptors midi_rxq_push_record/midi_dispatch_reset_2byte/_3byte reload
 * (this file's own DAT_c001176c/DAT_c001179c/DAT_c00117c4, K2 addresses not
 * individually resolved this pass); chan_status_obj's own fields between
 * 0x408-0x417 and 0x41f-0x617 beyond what this file's own 9 functions
 * directly touch.
 */

#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------- *
 *  Shared primitives - uart1_midi_queue.c's own K2 port.
 * ---------------------------------------------------------------------- */
extern void uart1_ier_thre_enable(void);	/* FUN_c0011124, uart1_midi_queue.c (K1: FUN_c000fd50) */
extern void uart1_ier_thre_disable(void);	/* FUN_c0011144, uart1_midi_queue.c (K1: FUN_c000fd74) */
extern int  irq_save_and_disable(void);	/* FUN_c0004f40, crypto_at88.c (K1: FUN_c0005500) */
extern void irq_restore(void);			/* FUN_c0004f50 (K1: FUN_c0005510) */
extern uint32_t uart1_base_get(void *chip);	/* FUN_c000180c, soc_periph.c (K1: uart_base_select(chip,1)) */
extern void chan_status_dispatch(void *obj);	/* FUN_c0011210, out of range - K2 counterpart of K1's FUN_c000fe20, 568 bytes, decompilable here but not reconstructed */

/* ============================================================================
 *  midi_txq_set_realtime_flag - FUN_c0011468, @0xc0011468 (48 bytes, K1:
 *  FUN_c0010078, 48 bytes - EXACT match). Identical: ORs a bit into the
 *  "pending realtime" flags byte at chan_status_obj+0x406 (DAT_c0011498 ==
 *  0x406, EXACT match to K1), guarded by the TX THRE IRQ gate.
 * ---------------------------------------------------------------------- */
void midi_txq_set_realtime_flag(void *obj, uint8_t mask)	/* FUN_c0011468 */
{
	uint8_t *p = (uint8_t *)obj + 0x406;

	uart1_ier_thre_disable();
	*p |= mask;
	uart1_ier_thre_enable();
}

/* ============================================================================
 *  midi_txq_push1 - FUN_c001149c, @0xc001149c (112 bytes, K1: FUN_c00100ac,
 *  112 bytes - EXACT match). Identical 1024-byte TX ring push with
 *  busy-wait-and-drain at capacity 0x3ff (DAT_c001150c == 0x404, EXACT
 *  match), write index +0x400.
 * ---------------------------------------------------------------------- */
void midi_txq_push1(void *obj, uint8_t byte)	/* FUN_c001149c */
{
	uint8_t *base = (uint8_t *)obj;
	uint16_t *widx = (uint16_t *)(base + 0x400);
	uint16_t *count = (uint16_t *)(base + 0x404);
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
 *  midi_txq_push2 - FUN_c0011510, @0xc0011510 (148 bytes, K1: FUN_c0010120,
 *  148 bytes - EXACT match). Identical 2-byte atomic push, drain-wait
 *  threshold 0x3fe (DAT_c00115a4 == 0x404, EXACT match, same -6U/-2
 *  derivation as K1).
 * ---------------------------------------------------------------------- */
void midi_txq_push2(void *obj, uint8_t byte0, uint8_t byte1)	/* FUN_c0011510 */
{
	uint8_t *base = (uint8_t *)obj;
	uint16_t *widx = (uint16_t *)(base + 0x400);
	uint16_t *count = (uint16_t *)(base + 0x404);
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
 *  midi_txq_push3 - FUN_c00115a8, @0xc00115a8 (172 bytes, K1: FUN_c00101b8,
 *  172 bytes - EXACT match). Identical 3-byte atomic push, drain-wait
 *  threshold 0x3fd (DAT_c0011658 == 0x3fd, EXACT match, DAT_c0011654 ==
 *  0x404).
 * ---------------------------------------------------------------------- */
void midi_txq_push3(void *obj, uint8_t byte0, uint8_t byte1, uint8_t byte2)	/* FUN_c00115a8 */
{
	uint8_t *base = (uint8_t *)obj;
	uint16_t *widx = (uint16_t *)(base + 0x400);
	uint16_t *count = (uint16_t *)(base + 0x404);
	uint16_t idx;

	while (*count >= 0x3fd) {
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
 *  FUN_c001165c (@0xc001165c, 44 bytes, K1: FUN_c001026c, 48 bytes) /
 *  FUN_c001167c (@0xc001167c, 44 bytes, K1: FUN_c0010290, 48 bytes).
 *  Identical IER bit-0x1 (RHR) set/clear on the fixed UART1 base. 3
 *  confirmed K2 callers each, all in FUN_c0011dbc (the K2 counterpart of
 *  K1's own midi_pop_raw_record/FUN_c00109dc, the RX ring's own consumer -
 *  matches K1's own caller role exactly, though K1 counted more call sites
 *  there).
 * ---------------------------------------------------------------------- */
void uart1_rx_irq_enable(void)		/* FUN_c001165c */
{
	uint32_t base = uart1_base_get((void *)0 /* dead arg, shared context handle */);

	*(uint32_t *)(base + 4) |= 1;
}

void uart1_rx_irq_disable(void)	/* FUN_c001167c */
{
	uint32_t base = uart1_base_get((void *)0 /* dead arg */);

	*(uint32_t *)(base + 4) &= ~1u;
}

/* ============================================================================
 *  midi_rxq_push_record - FUN_c00116bc, @0xc00116bc (164 bytes, K1:
 *  FUN_c00102d8, 164 bytes - EXACT match). Identical: pushes one 4-byte raw
 *  record into the 128-entry RX ring at chan_status_obj+0x418 if count
 *  (+0x61c) < 0x80, IRQ-guarded (irq_save_and_disable/irq_restore, NOT the
 *  UART1 IER gate - matches K1's own choice exactly); on overflow, resets
 *  the entire running-status parser state (count, saved-status +0x414,
 *  flag byte +0x417, reloads the dispatch-callback pair +0x40c/+0x410 from
 *  a fixed descriptor, RX read-index +0x61a and write-index +0x618 both
 *  zeroed, RX realtime-pending flags +0x40a zeroed) and returns 0 - net
 *  effect a full RX-parser reset rather than a partial drop, same as K1.
 *  Sole confirmed local caller: FUN_c00117c8 (chan_dispatch_probe's own K2
 *  counterpart, out of this file's own scope) - plus 7 further call sites
 *  with no containing function object in this static dump (same "unboxed
 *  call site" artifact this project repeatedly documents; K1's own version
 *  had exactly one confirmed caller).
 * ---------------------------------------------------------------------- */
void midi_rxq_push_record(void *obj, const uint32_t *rec)	/* FUN_c00116bc */
{
	uint8_t *base = (uint8_t *)obj;
	uint16_t *widx = (uint16_t *)(base + 0x618);
	uint16_t *count = (uint16_t *)(base + 0x61c);
	uint32_t *ring = (uint32_t *)(base + 0x418);
	int flags;

	if (*count < 0x80) {
		ring[*widx] = *rec;
		*widx = (uint16_t)((*widx + 1) & 0x7f);

		flags = irq_save_and_disable();
		(*count)++;
		irq_restore();
		(void)flags;
		return;
	}

	{
		extern const uint32_t midi_dispatch_reset_table0[2];	/* DAT_c001176c, K2 address not individually resolved this pass */

		*count = 0;
		base[0x417] = 0;
		*(uint32_t *)(base + 0x40c) = midi_dispatch_reset_table0[0];
		*(uint32_t *)(base + 0x410) = midi_dispatch_reset_table0[1];
	}

	*(uint16_t *)(base + 0x61a) = 0;	/* RX read-index */
	*widx = 0;				/* RX write-index */
	base[0x40a] = 0;			/* RX realtime-pending flags */
}

/* ============================================================================
 *  midi_dispatch_reset_2byte / midi_dispatch_reset_3byte -
 *  FUN_c0011778 (@0xc0011778, 32 bytes, K1: FUN_c0010394, 32 bytes - EXACT
 *  match) / FUN_c00117a0 (@0xc00117a0, 32 bytes, K1: FUN_c00103bc, 32 bytes
 *  - EXACT match). Identical: store the incoming status byte at
 *  chan_status_obj+0x414, reload the dispatch-callback pair (+0x40c/+0x410)
 *  from a fixed 8-byte descriptor DIFFERENT for each function
 *  (DAT_c001179c for the 2-byte variant, DAT_c00117c4 for the 3-byte
 *  variant - K2 addresses not individually resolved this pass, same open
 *  item K1 left for the descriptor table's own contents). Sole callers:
 *  FUN_c00117c8 (chan_dispatch_probe's own K2 counterpart, out of this
 *  file's own scope).
 * ---------------------------------------------------------------------- */
void midi_dispatch_reset_2byte(void *obj, uint8_t status)	/* FUN_c0011778 */
{
	uint8_t *base = (uint8_t *)obj;
	extern const uint32_t midi_dispatch_reset_table1[2];	/* DAT_c001179c */

	base[0x414] = status;
	*(uint32_t *)(base + 0x40c) = midi_dispatch_reset_table1[0];
	*(uint32_t *)(base + 0x410) = midi_dispatch_reset_table1[1];
}

void midi_dispatch_reset_3byte(void *obj, uint8_t status)	/* FUN_c00117a0 */
{
	uint8_t *base = (uint8_t *)obj;
	extern const uint32_t midi_dispatch_reset_table2[2];	/* DAT_c00117c4 */

	base[0x414] = status;
	*(uint32_t *)(base + 0x40c) = midi_dispatch_reset_table2[0];
	*(uint32_t *)(base + 0x410) = midi_dispatch_reset_table2[1];
}
