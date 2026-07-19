/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cad_pedal_encode_step.c - single-function gap-fill: FUN_c00133f8
 * (@0xc00133f8, 132 bytes), cad.c's own pedal-position encoder.
 *
 * Assignment context: assigned gap cluster 0xc00133ac-0xc001349c. Three of
 * the four addresses in that range (FUN_c00133ac/cad_pedal_send_release,
 * FUN_c00133ec/cad_pedal_object_set_mode, FUN_c0013480/cad_trim_adjust)
 * already have real, compilable bodies in cad.c. The fourth,
 * FUN_c00133f8, is only ever `extern`-declared there:
 *
 *     int cad_pedal_encode_step(int16_t *obj, uint8_t out_pair[2]);
 *         /- FUN_c00133f8, structure only, not transcribed - see comment above -/
 *
 * with a full prose analysis of its behavior but no definition - the same
 * "analyzed, never given a body" gap this project's cpsoc_read_event_pair.c
 * already precedent-set a fix for. eva_crt0_tick_glue.c independently
 * cites the SAME address/signature (also extern-only, "prototype only, no
 * body there") for its own Section 6 (eva_tick_unk_2, FUN_c000594c) - the
 * confirmed real caller/drain loop for this function. Per this project's
 * collision-avoidance rule, neither file is edited here.
 *
 * Ground truth: static Ghidra dump of KRONOS_V06R06.VSB (all_decompiled.json
 * / all_data.json), queried via query_dump.py, 2026-07-19 pass. No live
 * Ghidra MCP calls this pass.
 *
 * Real decompile (query_dump.py func c00133f8):
 *
 *     undefined4 FUN_c00133f8(short *param_1,undefined1 *param_2)
 *     {
 *       cVar2 = (char)param_1[1];
 *       while (true) {
 *         if (cVar2 != '\0') {
 *           *(undefined1 *)(param_1 + 1) = 0;
 *           return 0;
 *         }
 *         sVar4 = *param_1;
 *         iVar1 = (int)sVar4;
 *         *param_1 = 0;
 *         if (0x7f < iVar1) {
 *           sVar4 = 0x7f;
 *         }
 *         uVar3 = (undefined1)sVar4;
 *         if ((iVar1 < 0x80) && (iVar1 + 0x7f < 0 != SCARRY4(iVar1,0x7f))) {
 *           uVar3 = (undefined1)DAT_c001347c;
 *         }
 *         param_2[1] = uVar3;
 *         *param_2 = (char)param_1[1];
 *         *(char *)(param_1 + 1) = (char)param_1[1] + '\x01';
 *         if (param_2[1] != '\0') break;
 *         cVar2 = (char)param_1[1];
 *       }
 *       return 1;
 *     }
 *
 * FIELDS: `obj[0]` (param_1[0]) is the accumulated 16-bit signed pedal
 * position delta cad.c's own header already documents this function
 * consuming; `obj[1]`'s LOW BYTE (param_1[1] cast through `(char)`/
 * `undefined1*`) is a combined running index / one-shot "already produced
 * a pair this cycle" latch, per the analysis below.
 *
 * SCARRY4 simplification (documented, not silently dropped): `SCARRY4(a,b)`
 * is Ghidra's 32-bit signed-add-overflow-flag intrinsic. Here `a` (=iVar1)
 * is always a sign-extension of a 16-bit accumulator (range
 * [-32768,32767]) and `b` is the constant 0x7f (127) - their sum can never
 * overflow a 32-bit int, so `SCARRY4(iVar1, 0x7f)` is PROVABLY always 0 in
 * every reachable case. The guard therefore reduces exactly to
 * `(iVar1 < 0x80) && (iVar1 + 0x7f < 0)`, i.e. `iVar1 <= -0x80` (since
 * `iVar1 < 0x80` is implied whenever `iVar1 <= -0x80`) - transcribed as
 * that reduced form below, not as a literal SCARRY4 call, since introducing
 * a real overflow-intrinsic dependency for a condition that is
 * mathematically unreachable in this domain would be a worse fidelity
 * trade than the algebraic reduction.
 *
 * BEHAVIOR, traced by hand rather than simplified further (per this
 * project's "transcribe literally, including odd control flow" precedent -
 * see cpsoc_issp.c's issp_wait_ready): on entry, if the index/latch byte
 * (obj[1] low byte) is already nonzero, this call is a one-shot
 * "cool-down": reset the latch to 0 and report "nothing produced" (0).
 * Otherwise: clamp the accumulator to +127 (large positive) or substitute
 * a fixed sentinel byte (DAT_c001347c, real value 0x81) for any
 * accumulator <= -128 (a large negative delta - the naive 8-bit truncation
 * of e.g. -200 would alias with an unrelated small positive byte, so a
 * distinct out-of-band sentinel is used instead; this substitution never
 * triggers for accumulator values in [-127,127]); zero the accumulator;
 * emit `out_pair = {old_latch_byte, magnitude}`; increment the latch byte.
 * If the emitted magnitude is nonzero, stop here and report success (1).
 * If it's zero (accumulator was exactly 0), loop back to the top with the
 * just-incremented (now nonzero) latch byte - which immediately hits the
 * "already nonzero" branch above, resetting the latch back to 0 and
 * reporting "nothing produced" (0). Net observable effect across repeated
 * calls: a nonzero accumulated delta yields exactly ONE emitted pair
 * (index byte always 0, since the latch is reset every time it would
 * otherwise advance past 1), immediately followed by one "cooldown" call
 * that reports nothing and re-arms the latch for the next accumulation
 * cycle; a zero accumulator yields nothing every call. cad.c's own header
 * comment description ("loops internally until it produces a nonzero
 * magnitude or the index byte wraps to 0") is consistent with this trace.
 *
 * STILL OPEN: DAT_c001347c's own real-world meaning as a sentinel
 * (confirmed value 0x81/-127, not independently confirmed as anything
 * beyond "a fixed out-of-band byte"); whether the "cooldown" one-shot
 * pattern above is intentional rate-limiting or an artifact of a
 * multi-index design that never fires past index 0 in practice.
 */

#include <stdint.h>

extern const uint32_t cad_pedal_encode_sentinel;	/* DAT_c001347c, real value 0xff81 - low byte (0x81) is the one actually used, as the out-of-range magnitude sentinel */

int cad_pedal_encode_step(int16_t *obj, uint8_t out_pair[2])	/* FUN_c00133f8 */
{
	/* obj[1]'s low byte only - matches the real decompile's own
	 * `(char)param_1[1]` reads and `*(undefined1 *)(param_1+1) = ...`
	 * byte-sized writes (little-endian: byte offset param_1+1 halfword's
	 * low byte). obj[1]'s HIGH byte is never touched by this function. */
	uint8_t *latch_byte = (uint8_t *)&obj[1];

	for (;;) {
		if (*latch_byte != 0) {
			*latch_byte = 0;
			return 0;
		}

		{
			int32_t accum = obj[0];
			uint8_t magnitude;
			uint8_t index_out;

			obj[0] = 0;

			if (accum > 0x7f)
				accum = 0x7f;
			magnitude = (uint8_t)accum;

			if (accum <= -0x80)
				magnitude = (uint8_t)cad_pedal_encode_sentinel;

			index_out = *latch_byte;	/* always 0 in practice - see header trace */
			*latch_byte = (uint8_t)(index_out + 1);

			out_pair[0] = index_out;
			out_pair[1] = magnitude;

			if (magnitude != 0)
				return 1;
			/* magnitude == 0: loop back - *latch_byte is now nonzero (just
			 * incremented), so the next iteration immediately hits the
			 * reset-and-return-0 branch above. */
		}
	}
}
