/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cpsoc_row_sync.c - single-function gap-fill: FUN_c0012814 (@0xc0012814,
 * 120 bytes), the real body behind wire_dispatch.c's own `eva_wire_reg_write`
 * extern (opcode 5 handler).
 *
 * Assignment context: this address was flagged "remaining" for the small
 * gap cluster 0xc0012814. wire_dispatch.c already `extern`-declares it
 * (`extern void eva_wire_reg_write(void *ctx, uint8_t reg, uint16_t value);
 * /- FUN_c0012814, opcode 5 -/`) and calls it from its own opcode-5 handler:
 *
 *     eva_wire_reg_write(wire_cpsoc_ctx, cmd[2], (uint16_t)cmd[1] + (uint16_t)cmd[0] * 0x100);
 *
 * - but never supplies a compilable definition, matching the exact
 * "extern-declared, analyzed in prose, no body" gap this project's own
 * cpsoc_read_event_pair.c already precedent-set a fix for. Per this
 * project's collision-avoidance rule, wire_dispatch.c is NOT edited here.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled.json
 * / all_data.json), queried via query_dump.py, 2026-07-19 pass. No live
 * Ghidra MCP calls this pass (2-agent-cap in effect; static dump sufficient).
 *
 * Real decompile (query_dump.py func c0012814):
 *
 *     void FUN_c0012814(int param_1,int param_2,uint param_3)
 *     {
 *       int iVar1;
 *       param_3 = param_3 & 0xffff;
 *       iVar1 = param_2 * 0x10;
 *       do {
 *         if (((param_3 & 1) == 0) || (*(char *)(param_1 + iVar1) != '\0')) {
 *           if (((param_3 & 1) == 0) && (*(char *)(param_1 + iVar1) != '\0')) {
 *             FUN_c00127ac(param_1,iVar1);
 *           }
 *         } else {
 *           FUN_c00127e0(param_1,iVar1);
 *         }
 *         iVar1 = iVar1 + 1;
 *         param_3 = param_3 >> 1;
 *       } while (iVar1 < param_2 * 0x10 + 0x10);
 *       return;
 *     }
 *
 * CORRECTION to wire_dispatch.c's own extern (flagged here, not fixed
 * there, per this project's established cross-file-discrepancy convention -
 * see README.md's "Known cross-file discrepancies" precedent): this is NOT
 * a single hardware "register write". `param_2` ("reg" at that file's call
 * site) is really a ROW index, multiplied by 0x10 (16) to get a byte
 * offset; `param_3` ("value") is a 16-BIT MASK, one bit per one of 16
 * consecutive switch/LED-row byte-offsets in the SAME `row_state` array
 * cpsoc.c's own cpsoc_read_switch_row/_read_switch_row_clear (FUN_c00127e0/
 * FUN_c00127ac, reg 0x50/0x52) already operate on (confirmed: `param_1` is
 * the same `wire_cpsoc_ctx` == `row_state` base pointer used at every
 * other opcode-0/opcode-1 reg-0x50/0x52 call site in wire_dispatch.c).
 *
 * REAL BEHAVIOR - an edge-triggered 16-wide diff/sync, not a raw write:
 * for each of the 16 byte offsets `row*16 .. row*16+15`, compare the
 * desired bit (from the mask, LSB first) against the row_state array's
 * CURRENT cached value at that offset (the same `row_state[i] = 0/1`
 * cache cpsoc_read_switch_row sets to 1 and cpsoc_read_switch_row_clear
 * sets to 0):
 *   - desired bit == 1 AND row_state[i] == 0 (not yet marked "on"):
 *     call cpsoc_read_switch_row(row_state, i) - which does the real
 *     hardware access AND marks row_state[i] = 1.
 *   - desired bit == 0 AND row_state[i] != 0 (currently marked "on"):
 *     call cpsoc_read_switch_row_clear(row_state, i) - real hardware
 *     access, marks row_state[i] = 0.
 *   - desired bit already matches the cached state: no call at all (no
 *     redundant hardware I/O).
 * This "only touch what changed" shape falls straight out of the raw
 * decompile's boolean logic once traced by hand (worked through explicitly
 * below rather than asserted) - it is NOT a simplification/guess, it is
 * the literal control flow.
 *
 * Bound check: cpsoc_read_switch_row/_clear's own real bodies (cpsoc.c)
 * both early-return for any offset > 0x48 - so `row` values above ~4
 * silently no-op past that point every iteration; not re-guarded here,
 * exactly matching the original (no bounds check exists in THIS function
 * either).
 *
 * Real caller: wire_dispatch_command (FUN_c0007d1c, wire_dispatch.c),
 * opcode 5, exactly one call site.
 */

#include <stdint.h>

/* --- cross-file externs, cpsoc.c (same row_state hardware primitives
 * wire_dispatch.c's own opcode-0/1 reg-0x50/0x52 branches already use) --- */
extern void cpsoc_read_switch_row(uint8_t *row_state, int index);		/* FUN_c00127e0, reg 0x50 */
extern void cpsoc_read_switch_row_clear(uint8_t *row_state, int index);	/* FUN_c00127ac, reg 0x52 */

/*
 * cpsoc_sync_switch_row_bits - real name/signature for FUN_c0012814.
 * `row_state` is wire_dispatch.c's own `wire_cpsoc_ctx`; `row` is the
 * 16-wide row group index; `mask16` is the desired on/off bit per offset
 * (bit 0 = row*16+0, bit 15 = row*16+15). See header comment above for the
 * full derivation of the "diff and only touch changed bits" behavior.
 */
void cpsoc_sync_switch_row_bits(uint8_t *row_state, int row, uint32_t mask16)	/* FUN_c0012814 */
{
	uint32_t bits = mask16 & 0xffff;
	int off = row * 0x10;
	int end = row * 0x10 + 0x10;

	do {
		int want_set = (bits & 1) != 0;
		int cur_set  = row_state[off] != 0;

		if (want_set && !cur_set)
			cpsoc_read_switch_row(row_state, off);
		else if (!want_set && cur_set)
			cpsoc_read_switch_row_clear(row_state, off);
		/* else: already matches cached state, no hardware access */

		off++;
		bits >>= 1;
	} while (off < end);
}
