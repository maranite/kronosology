/* SPDX-License-Identifier: GPL-2.0 */
/*
 * panelbus_record_queue.c - two-function gap-fill: FUN_c0012d88 (pop,
 * @0xc0012d88, 80 bytes) and FUN_c0012de0 (push, @0xc0012de0, 112 bytes),
 * the 256-slot record ring buffer underneath panelbus_dispatch.c's own
 * `panelbus_submit_record` (FUN_c0012e58, already fully reconstructed
 * there).
 *
 * Assignment context: assigned gap cluster 0xc0012d88-0xc0012e80.
 * panelbus_dispatch.c already defines FUN_c0012e58 (panelbus_submit_record)
 * with a real body, but only `extern`-declares its callee:
 *
 *     extern uint8_t panelbus_record_submit_raw(void *target, const uint8_t *packed_pair);
 *         -- FUN_c0012de0, not traced further (that file's own words)
 *
 * and FUN_c0012d88 (the pop counterpart) isn't mentioned there at all - its
 * one real caller, FUN_c00087c4, is itself only `extern`-declared in
 * wire_dispatch.c as `eva_tick_unk_1` ("FUN_c00087c4, bit 0x1, not traced"),
 * called from master_dispatch_tick on status bit 0x1 every firmware tick.
 * Per this project's collision-avoidance rule, panelbus_dispatch.c is NOT
 * edited here - this file supplies the two missing bodies instead, exactly
 * the "cpsoc_read_event_pair.c" precedent for an already-analyzed-but-
 * never-defined callee.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled.json
 * / all_data.json), queried via query_dump.py, 2026-07-19 pass. No live
 * Ghidra MCP calls this pass.
 *
 * QUEUE LAYOUT - derived directly from both functions' own offset
 * arithmetic, not guessed:
 *   +0x000..+0x3ff : ring[256], 4-byte records (256*4 = 0x400)
 *   +0x400         : write_index (uint16) - only ever touched by the PUSH
 *                    side (FUN_c0012de0), confirmed by the fixed literal
 *                    0x400 used there (not a DAT_-resolved constant, unlike
 *                    every other field in this struct - transcribed as a
 *                    literal to match)
 *   +0x402         : read_index (uint16) - DAT_c0012ddc (pop) /
 *                    (push never touches it)
 *   +0x404         : count (uint16) - DAT_c0012dd8 (pop) / DAT_c0012e54 (push)
 *   +0x406         : mode_flags (uint16) - DAT_c0012e50 (push only; tested
 *                    against bit 0, see push's own gate below)
 * Total struct size 0x408 (1032 bytes). This is a DIFFERENT ring than
 * panelbus_dispatch.c's own `struct panelbus_tx_channel` (128 x 4-byte
 * slots, read_index/count/unknown_206 at +0x202/+0x204/+0x206, no push
 * side found there either) - same general shape (per-channel 4-byte-record
 * ring feeding the same wire-protocol family), but a distinct instance with
 * a distinct (256-slot, 0x408-byte) layout, confirmed by the different
 * struct size and by neither function here ever touching offset 0x202/0x206.
 *
 * FUN_c0012d88 (pop) real decompile:
 *
 *     bool FUN_c0012d88(int param_1,undefined4 *param_2)
 *     {
 *       iVar3 = DAT_c0012ddc;  // 0x402
 *       iVar2 = DAT_c0012dd8;  // 0x404
 *       bVar4 = *(short *)(param_1 + DAT_c0012dd8) != 0;
 *       if (bVar4) {
 *         *param_2 = *(undefined4 *)(param_1 + (uint)*(ushort *)(param_1 + DAT_c0012ddc) * 4);
 *         sVar1 = *(short *)(param_1 + iVar3);
 *         *(short *)(param_1 + iVar2) = *(short *)(param_1 + iVar2) + -1;
 *         *(ushort *)(param_1 + iVar3) = sVar1 + 1U & 0xff;
 *       }
 *       return bVar4;
 *     }
 *
 * i.e.: if count != 0, read ring[read_index] into *out, count--,
 * read_index = (read_index+1) & 0xff (an 8-bit wraparound over a 256-slot
 * ring - consistent, matches push's own `< 0x100` capacity check below).
 * Return whether a record was popped.
 *
 * FUN_c0012de0 (push) real decompile:
 *
 *     bool FUN_c0012de0(int param_1,undefined4 *param_2)
 *     {
 *       iVar2 = DAT_c0012e54;  // 0x404, count
 *       if ((((*(ushort *)(param_1 + DAT_c0012e50) & 1) == 0) ||           // mode_flags & 1 == 0
 *           (bVar3 = false, *(char *)((int)param_2 + 1) != '\x03')) &&    // OR tag != 3
 *          (bVar3 = *(ushort *)(param_1 + DAT_c0012e54) < 0x100, bVar3)) { // AND count < 256
 *         *(undefined4 *)(param_1 + (uint)*(ushort *)(param_1 + 0x400) * 4) = *param_2;
 *         sVar1 = *(short *)(param_1 + 0x400);
 *         *(ushort *)(param_1 + iVar2) = *(short *)(param_1 + iVar2) + (ushort)bVar3;
 *         *(ushort *)(param_1 + 0x400) = sVar1 + (ushort)bVar3 & 0xff;
 *       }
 *       return bVar3;
 *     }
 *
 * Worked through by hand (Ghidra's own `bVar3`-reuse-as-flag idiom, the
 * same pattern already documented at length in cpsoc_issp.c's
 * issp_wait_ready and this project's other "phantom bVar" notes): the
 * `(A || B) && C` outer shape only SKIPS the push body when A is false AND
 * B is false, i.e. when `(mode_flags & 1) != 0 AND tag == 3` - every other
 * combination falls through to the `C` (`count < 0x100`) check, which is
 * the real gate that also becomes the return value and the +1 increment
 * amount. Net effect, transcribed as a straight-line equivalent below:
 *   - if (mode_flags & 1) && (packed_pair[1] == 3): return false, no push
 *     (a specific record TAG is filtered out only when this queue's own
 *     mode flag bit 0 is set - real meaning of both the flag and the
 *     filtered tag value not independently confirmed, transcribed as-is).
 *   - else if count >= 256 (queue full): return false, no push.
 *   - else: ring[write_index] = *record; count++; write_index =
 *     (write_index+1) & 0xff; return true.
 *
 * `param_2` is declared `undefined4 *` (a 4-byte read in both functions),
 * but panelbus_submit_record's own real call site only guarantees a
 * 2-byte `packed[2]` local array - the same "read 4 bytes from a
 * caller-guaranteed-2-byte buffer" firmware quirk already flagged
 * elsewhere in this project (crypto_at88.c's own queue-producer note);
 * transcribed faithfully (`const uint8_t *`, 2 bytes read/written) rather
 * than silently widening panelbus_submit_record's own contract.
 *
 * STILL OPEN: `mode_flags`'s (+0x406) own real meaning and who sets it
 * (no writer found in either function here or in panelbus_dispatch.c);
 * the real reason the "tag == 3" record is filtered when that bit is set.
 */

#include <stdint.h>

#define PANELBUS_RECORD_QUEUE_RING_SLOTS 256

/* Byte offsets into the caller-supplied `target` blob, matching the
 * decompile's own DAT_-resolved constants exactly (see header). Kept as
 * plain offsets, not a struct, since the ring array itself is variable-
 * length-shaped (256 x 4 bytes) ahead of the index fields - consistent
 * with this project's own established convention for this situation (see
 * panelbus_dispatch.c's `struct panelbus_tx_channel` for the analogous,
 * differently-sized sibling ring). */
#define PANELBUS_RQ_OFF_WRITE_INDEX 0x400	/* uint16, push-side only, literal (not DAT_-resolved) in the real decompile */
#define PANELBUS_RQ_OFF_READ_INDEX  0x402	/* uint16, DAT_c0012ddc */
#define PANELBUS_RQ_OFF_COUNT       0x404	/* uint16, DAT_c0012dd8 (pop) / DAT_c0012e54 (push) */
#define PANELBUS_RQ_OFF_MODE_FLAGS  0x406	/* uint16, DAT_c0012e50, push-side gate only */

static inline uint16_t rq_get16(void *base, int off)
{
	return *(uint16_t *)((uint8_t *)base + off);
}
static inline void rq_set16(void *base, int off, uint16_t v)
{
	*(uint16_t *)((uint8_t *)base + off) = v;
}

/*
 * panelbus_record_queue_pop - FUN_c0012d88. Pops one 4-byte record from
 * `queue` into `*out` if non-empty. Real (only) caller: FUN_c00087c4
 * (extern-declared `eva_tick_unk_1` in wire_dispatch.c, itself called from
 * master_dispatch_tick on status bit 0x1 - not reconstructed here, out of
 * this file's own assigned gap). @0xc0012d88.
 */
uint8_t panelbus_record_queue_pop(void *queue, uint32_t *out)	/* FUN_c0012d88 */
{
	uint16_t count = rq_get16(queue, PANELBUS_RQ_OFF_COUNT);

	if (count != 0) {
		uint16_t read_index = rq_get16(queue, PANELBUS_RQ_OFF_READ_INDEX);

		*out = ((uint32_t *)queue)[read_index];
		rq_set16(queue, PANELBUS_RQ_OFF_COUNT, count - 1);
		rq_set16(queue, PANELBUS_RQ_OFF_READ_INDEX, (uint16_t)((read_index + 1) & 0xff));
		return 1;
	}
	return 0;
}

/*
 * panelbus_record_submit_raw - FUN_c0012de0. Pushes one 2-byte
 * [value, tag] packed pair (widened to a 4-byte ring slot, matching the
 * real decompile's own 4-byte read off `packed_pair` - see header's note
 * on this being a genuine narrow-buffer-read quirk, not widened here)
 * into `queue`, unless gated out or full. Same name/signature
 * panelbus_dispatch.c's own `panelbus_submit_record` already
 * extern-declares this as. Sole caller: panelbus_submit_record
 * (FUN_c0012e58, panelbus_dispatch.c). @0xc0012de0.
 */
uint8_t panelbus_record_submit_raw(void *queue, const uint8_t *packed_pair)	/* FUN_c0012de0 */
{
	uint16_t mode_flags = rq_get16(queue, PANELBUS_RQ_OFF_MODE_FLAGS);
	uint16_t count;

	if ((mode_flags & 1) != 0 && packed_pair[1] == 3)
		return 0;	/* filtered: mode bit 0 set AND tag == 3 - see header note */

	count = rq_get16(queue, PANELBUS_RQ_OFF_COUNT);
	if (count >= PANELBUS_RECORD_QUEUE_RING_SLOTS)
		return 0;	/* full */

	{
		uint16_t write_index = rq_get16(queue, PANELBUS_RQ_OFF_WRITE_INDEX);

		((uint32_t *)queue)[write_index] = *(const uint32_t *)packed_pair;
		rq_set16(queue, PANELBUS_RQ_OFF_COUNT, count + 1);
		rq_set16(queue, PANELBUS_RQ_OFF_WRITE_INDEX, (uint16_t)((write_index + 1) & 0xff));
	}
	return 1;
}
