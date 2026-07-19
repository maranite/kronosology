/* SPDX-License-Identifier: GPL-2.0 */
/*
 * crypto_at88.c - the NKS4 panel firmware's own direct path to the AT88SC/NV2AC
 * security chip, independent of the host's OA.ko/OmapNKS4Module.ko/GetPubIdMod.ko.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS2S_V01R10.VSB (Kronos 2,
 * "KRONOS II" product tag, same 917760-byte container/TI OMAP-L1x/ARM926EJ-S
 * target as K1, loaded at 0xC0000000), read via the pre-fetched static dump
 * (all_decompiled_k2.json/all_data_k2.json), not the live Ghidra bridge. This
 * is a migration pass from the already-done K1 reconstruction at
 * kronosology/reconstructed/K1_V06R06/crypto_at88.c - every function below
 * was independently re-matched against K2's own decompile by code shape
 * (K2's address layout for this whole cluster is shifted/reordered relative
 * to K1, NOT a constant offset - e.g. K1's queue-init lived at 0xc0000ec8,
 * K2's equivalent is at 0xc0000c48, while K2's OWN 0xc0000ec8 is a
 * completely different function, i2c_gpio_set_scl) and cross-checked, not
 * assumed carried over unchanged.
 *
 * ANCHOR: "../CryptoAt88.cpp" lives at 0xc002a70c (per this task's own
 * confirmed anchor list). Independently reconfirmed here:
 * crypto_at88_self_test's fault call reads DAT_c0000ec4, which resolves to
 * literal value 0xc002a70c - exact match.
 *
 * FILE-SPLIT NOTE (per this migration pass's explicit instructions - do not
 * double-port the same function into both output files): K1's crypto_at88.c
 * kept static, address-identical duplicate copies of most of the shared I2C
 * bit-bang layer (start/stop/write_byte/read_byte/ack/frame_command/
 * at88_i2c_write/at88_i2c_read/at88_lock/at88_unlock) "for continuity with
 * earlier passes' citations", even though the anchor-string evidence in both
 * K1 and K2 shows that entire layer (including the lock/unlock busy-guard
 * pair) genuinely belongs to I2cByGpio.cpp's own translation unit, not
 * CryptoAt88.cpp's. This K2 port does NOT reproduce that duplication: the
 * bit-bang layer is defined exactly once, in i2c_by_gpio.c, and this file
 * calls its two exported entry points (i2c_gpio_write_block/read_block) via
 * `extern`. This file itself is the canonical, single home for everything
 * that genuinely is CryptoAt88.cpp-specific: the queue relay, the factory
 * self-test, and the fault/assert handler (which anchors to THIS file's own
 * string, confirmed above and independently for K1's crypto_at88_self_test).
 *
 * Every K1 finding this file depends on (retry bound, byte-order/reversal
 * formula in the read-relay wire record, the dead `chip` handle, the queue's
 * 2-deep/32-byte-stride ring layout, the write path's fire-and-forget
 * behavior) was independently re-derived from K2's own decompile text below,
 * not copied from K1's citations - see each function's own comment.
 */

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------------- *
 *  Thin wrappers over i2c_by_gpio.c's exported block-transfer primitives.
 *  K1: FUN_c0000ef4/FUN_c0000f30 -> K2: FUN_c0000c74/FUN_c0000cb0.
 *
 *  CONFIRMED (independently, this K2 pass): the real FUN_c0000c74/
 *  FUN_c0000cb0 do NOT forward their own `chip` parameter to
 *  i2c_gpio_write_block/i2c_gpio_read_block at all - they pass the address
 *  of an uninitialized 4-byte on-stack buffer instead
 *  (`undefined1 auStack_14[4]; FUN_c0001468(auStack_14, param_2, ...)` in
 *  the real K2 decompile) - the exact same "phantom forwarded parameter"
 *  pattern K1 found at this same pair of call sites. Provably harmless here
 *  (the entire GPIO layer ignores `chip` regardless of what's passed, see
 *  i2c_by_gpio.c), so `chip` is still forwarded normally below for
 *  clarity/recompilability.
 * ------------------------------------------------------------------------- */
extern int i2c_gpio_write_block(void *chip, uint8_t cmd, uint8_t arg1, uint8_t arg2,
				int len, const uint8_t *data);	/* FUN_c0001468, i2c_by_gpio.c */
extern int i2c_gpio_read_block(void *chip, uint8_t cmd, uint8_t arg1, uint8_t arg2,
			       int len, uint8_t *dest);	/* FUN_c00013b8, i2c_by_gpio.c */

static inline void crypto_at88_write(void *chip, uint8_t cmd, uint8_t arg1,
				     uint8_t arg2, uint32_t len, const void *data)
{								/* FUN_c0000c74, @0xc0000c74 (K1: FUN_c0000ef4) */
	i2c_gpio_write_block(chip, cmd, arg1, arg2, (int)len, data);
}
static inline void crypto_at88_read(void *chip, uint8_t cmd, uint8_t arg1,
				    uint8_t arg2, uint32_t len, void *dest)
{								/* FUN_c0000cb0, @0xc0000cb0 (K1: FUN_c0000f30) */
	i2c_gpio_read_block(chip, cmd, arg1, arg2, (int)len, dest);
}

/* screen text-draw primitive: (x, y, string, ?) - FUN_c0012578, @0xc0012578
 * (K1: FUN_c0015650). Confirmed identical call shape at the two sites below
 * (`(10,10,...,0)` / `(10,0x14,...,0)`). 4th param's real meaning still not
 * resolved on either board. */
extern void draw_text(int x, int y, const char *str, int unknown_arg4);
/* irq_save_and_disable - FUN_c0004f40, @0xc0004f40 (K1: FUN_c0005500).
 * Called with no visible arguments at its one call site here, same as K1. */
extern int  irq_save_and_disable(void);
extern void crypto_at88_format_fault_text(char *dst, const char *fmt,
					  const void *arg1, const void *arg3);	/* FUN_c0013824, @0xc0013824 (K1: FUN_c00168fc) */

/* ------------------------------------------------------------------------- *
 *  crypto_at88_fault - the hard-halt assert handler. @0xc000a730
 *  (K1: @0xc000919c). 63 callers firmware-wide (K1 had 51 - both boards
 *  clearly reuse this as the generic fault primitive well beyond this file).
 *  Confirmed identical body shape: irq_save_and_disable(); format the fault
 *  text; draw_text(10,10,...); draw_text(10,20,...); infinite loop, never
 *  returns.
 * ------------------------------------------------------------------------- */
void crypto_at88_fault(const void *unused_arg1, const char *file, int line)
{
	irq_save_and_disable();	/* one-way: never restored, this function never returns */
	crypto_at88_format_fault_text((char *)0 /* real fixed message-buffer global, not extracted this pass */,
				      (const char *)0 /* real fixed template global, not extracted this pass */,
				      unused_arg1, (const void *)(intptr_t)line);
	draw_text(10, 10, (const char *)0, 0);
	draw_text(10, 20, file, 0);
	for (;;) { }		/* confirmed: do {} while (true), never returns */
}

/* ------------------------------------------------------------------------- *
 *  crypto_at88_self_test - factory zone-0 write/read-back round-trip test.
 *  @0xc0000da8 (K1: @0xc0001028).
 *
 *  RE-CONFIRMED (independently, this K2 pass): genuinely ZERO callers in the
 *  full K2 xref data, exactly like K1 - same "either dead factory-test code,
 *  or reached only through a mechanism static analysis can't see" open
 *  question, still moot on this board too.
 *
 *  ONE GENUINE DIFFERENCE FROM K1: the fault call inside the final compare
 *  loop cites source line 0xbb (187) in this K2 build, vs K1's 0xbd (189) -
 *  a real difference in the cited line number (this file's real source
 *  presumably has two fewer lines above the assert in the K2 revision, e.g.
 *  from an unrelated edit elsewhere in the original CryptoAt88.cpp), not a
 *  transcription error - independently re-read off K2's own decompile
 *  (`FUN_c000a730(0,DAT_c0000ec4,0xbb)`) rather than assumed equal to K1's.
 *  DAT_c0000ec4 itself resolves to 0xc002a70c, i.e. still cites
 *  "../CryptoAt88.cpp" (this file), not I2cByGpio.cpp - same as K1.
 * ------------------------------------------------------------------------- */
void crypto_at88_self_test(void *chip)
{
	uint8_t buf[16];
	int i;

	/* $B4 zone-select -> zone 0: {0xb4, 0x03, 0x00, 0x00} */
	crypto_at88_write(chip, 0xb4, 3, 0, 0, buf);

	/* known write pattern: 0,1,2,...,15 */
	for (i = 0; i < 16; i++)
		buf[i] = (uint8_t)i;
	/* $B0 write of that pattern to zone-0 address 0 */
	crypto_at88_write(chip, 0xb0, 0, 0, 16, buf);

	/* poison the buffer before reading back - confirmed identical 0xa5
	 * poison constant and descending-index loop shape to K1. */
	for (i = 15; i >= 0; i--)
		buf[i] = 0xa5;
	/* $B2 read of 16 bytes back from zone-0 address 0 */
	crypto_at88_read(chip, 0xb2, 0, 0, 16, buf);

	/* compare against the ORIGINAL write pattern - any mismatch is a hard,
	 * unrecoverable fault. Line number 0xbb here, see note above. */
	for (i = 0; i < 16; i++) {
		if (buf[i] != (uint8_t)i)
			crypto_at88_fault(0, "../CryptoAt88.cpp", 0xbb);
	}
}

/* ------------------------------------------------------------------------- *
 *  Queued command relay - a 2-deep ring buffer (index masked & 1) of pending
 *  AT88 write/read requests, drained by crypto_at88_process_queue.
 *
 *  RE-CONFIRMED (independently, this K2 pass, exact struct-offset match to
 *  K1): dequeue primitive @0xc0000d48 (K1: @0xc0000fc8), 32-byte entries,
 *  count field at handle+0x42, read-index byte at handle+0x41; push
 *  primitive @0xc0000cec (K1: @0xc0000f6c), same 32-byte stride,
 *  write-index byte at handle+0x40, gated on count<2. Both re-derived
 *  straight from K2's own pointer arithmetic (`param_1 + (uint)*(byte
 *  *)(param_1+0x41) * 0x20` etc.), not assumed identical to K1.
 *
 *  QUEUE OBJECT: single fixed global, re-confirmed shared between
 *  crypto_at88_process_queue's own pop/write/read calls and the producer's
 *  push call - both resolve to the same literal address 0xc0007a98 in this
 *  K2 build (K1's equivalent DAT_c0005f58/DAT_c00084c8 pairing).
 *
 *  PRODUCER: FUN_c0007a9c in this K2 build (K1: FUN_c0007d1c), the firmware's
 *  USB host-command dispatcher - confirmed calling the push primitive above
 *  with the same `< 0x21` (33-byte) length gate K1 documented, and the same
 *  4-byte-group wire-reversal helper (FUN_c001372c here, K1's FUN_c0016804)
 *  used on both the host->device (write-enqueue) and device->host
 *  (relay-read-result, below) directions. Out of scope for this file (lives
 *  in wire_dispatch.c territory, reconstructed by a different agent) -
 *  cited by address only, per this project's existing convention.
 * ------------------------------------------------------------------------- */
struct at88_queue_entry {
	uint8_t  cmd;		/* LSB is a read(1)/write(0) selector, same convention as K1 */
	uint8_t  arg1, arg2, len;
	uint8_t  data[28];	/* rest of the 32-byte entry */
};

extern int  at88_queue_pop(void *queue_handle, struct at88_queue_entry *out);	/* FUN_c0000d48 */
extern int  at88_queue_push(void *queue_handle, const struct at88_queue_entry *in); /* FUN_c0000cec - producer lives in wire_dispatch.c, out of scope here */
extern void at88_relay_read_result(void *unused_param,
				   struct at88_queue_entry *entry);		/* FUN_c00078e0 */

/* the shared queue object both crypto_at88_process_queue and the
 * out-of-scope producer operate on - 0xc0007a98 in this K2 build, confirmed
 * the same runtime address at both use sites (see block comment above). */
extern void *g_at88_queue;

/*
 * crypto_at88_process_queue - FUN_c00079dc, @0xc00079dc (K1: FUN_c0005e9c).
 * RE-CONFIRMED single-parameter signature, matching K1's own correction:
 * neither the queue object (fixed global, see above) nor the eventual USB
 * destination channel (also a fixed global, see at88_relay_read_result
 * below) is derived from this function's own parameter - it is simply
 * forwarded one level down into at88_relay_read_result, which likewise never
 * uses it. Same "phantom forwarded parameter" pattern as K1, independently
 * re-observed here.
 *
 * Caller: FUN_c000a58c in this K2 build (K1: master_dispatch_tick,
 * FUN_c0008b64) - an event-flag-gated dispatch tick, same role as K1's
 * caller, not itself reconstructed here (out of scope).
 *
 * WRITE PATH FIRE-AND-FORGET: re-confirmed at this level (no call to
 * at88_relay_read_result on the write branch) exactly like K1 - a successful
 * $B0/$B4/$B8 write produces no host-visible completion event from this
 * function.
 */
void crypto_at88_process_queue(void *unused_param)		/* FUN_c00079dc */
{
	struct at88_queue_entry e;

	while (at88_queue_pop(g_at88_queue, &e)) {
		if ((e.cmd & 1) == 0) {
			/* write path - fire and forget, no result relayed */
			crypto_at88_write(g_at88_queue, e.cmd, e.arg1, e.arg2,
					  e.len, e.data);
		} else {
			e.cmd &= 0xfe;
			memset(e.data, 0, sizeof(e.data));
			crypto_at88_read(g_at88_queue, e.cmd, e.arg1, e.arg2,
					 e.len, e.data);
			/* relay the read result to the host as an AtmelRead
			 * (0xe1) event - see at88_relay_read_result below. */
			at88_relay_read_result(unused_param, &e);
		}
	}
}

/*
 * at88_relay_read_result - FUN_c00078e0, @0xc00078e0 (K1: FUN_c0005da0).
 * Builds the wire-format AtmelRead (0xe1) event record from a completed
 * read's queue entry and submits it to the host-bound USB transmit queue.
 *
 * WIRE-FORMAT BYTE ORDER - RE-DERIVED INDEPENDENTLY FROM K2'S OWN DECOMPILE,
 * not copied from K1's (already twice-corrected) citation, and found to be
 * IDENTICAL to K1's final, corrected formula:
 *  - word 0 = [entry.arg2, entry.arg1, entry.cmd, 0xe1] (opcode byte high).
 *    Confirmed directly from K2's stack-local assignment order (`local_6e =
 *    *param_2; local_6f = param_2[1]; local_70 = param_2[2]; local_6d =
 *    0xe1;` followed by a 4-byte block copy from &local_70, i.e. ascending
 *    memory order [local_70, local_6f, local_6e, local_6d] = [arg2, arg1,
 *    cmd, 0xe1]).
 *  - word 1 = [entry.data[2], entry.data[1], entry.data[0], entry.len] -
 *    same "reverse the first 3 content bytes, keep the 4th field (len) in
 *    place" pattern as word 0, confirmed the identical way.
 *  - every trailing 4-byte payload group is a FULL 4-byte reversal: source
 *    bytes [i, i+1, i+2, i+3] (i starting at 3, entry.data[i+0..i+3]) land in
 *    wire order [data[i+3], data[i+2], data[i+1], data[i+0]].
 *  - the trailing loop's continuation condition is confirmed `i < len`
 *    (K2's own `SBORROW4(iVar2,(uint)(byte)param_2[3])`-based condition
 *    reduces to the same plain signed comparison K1's 2026-07-18 correction
 *    already established) - NOT `i + 4 <= len`. This is the SAME corrected
 *    formula as K1's final version, independently re-derived here rather
 *    than assumed to still apply on this board.
 *
 * NOTE ON BOUNDS (same genuine firmware quirk as K1, not fixed away): the
 * out-of-scope producer's own length gate is `< 0x21` (33), larger than this
 * struct's real 28-byte data capacity, so for `len` in the high-20s both the
 * real producer's copy-in and this function's copy-out walk a few bytes past
 * the nominal `data[28]` array. Transcribed faithfully, matching K1's own
 * fidelity-over-safety convention.
 *
 * `unused_param` (K2's own `param_1`) is never referenced in the real
 * function body - confirmed here exactly as K1 found. The real USB
 * destination channel is a separate fixed global (g_at88_usb_channel,
 * 0xc00079d8 in this K2 build), distinct from the queue object above.
 */
extern void at88_usb_tx_submit(void *dest_channel, const void *buf, int len);	/* FUN_c000bff0, @0xc000bff0 (K1: FUN_c000acec) */
extern void *g_at88_usb_channel;	/* 0xc00079d8 in this K2 build */

void at88_relay_read_result(void *unused_param, struct at88_queue_entry *entry)
{
	uint8_t wire[64];
	int wire_len = 8;
	int i;

	(void)unused_param;	/* real function never references its own param_1 */

	/* word 0: [arg2, arg1, cmd, 0xe1] - opcode 0xe1 (AtmelRead) high byte */
	wire[0] = entry->arg2;
	wire[1] = entry->arg1;
	wire[2] = entry->cmd;
	wire[3] = 0xe1;
	/* word 1: [data[2], data[1], data[0], len] */
	wire[4] = entry->data[2];
	wire[5] = entry->data[1];
	wire[6] = entry->data[0];
	wire[7] = entry->len;
	/* remaining payload bytes, 4 at a time, each group fully reversed */
	for (i = 3; i < (int)entry->len; i += 4, wire_len += 4) {
		wire[wire_len + 0] = entry->data[i + 3];
		wire[wire_len + 1] = entry->data[i + 2];
		wire[wire_len + 2] = entry->data[i + 1];
		wire[wire_len + 3] = entry->data[i + 0];
	}

	at88_usb_tx_submit(g_at88_usb_channel, wire, wire_len);
}
