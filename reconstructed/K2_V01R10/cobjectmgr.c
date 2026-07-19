/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cobjectmgr.c - KRONOS2S_V01R10.VSB (Kronos 2 / "KRONOS II") port of the K1
 * reconstruction at K1_V06R06/cobjectmgr.c. Same subsystem: a single
 * current-object slot, polled once per dispatch-loop tick, dispatching on a
 * type tag and releasing the object when done.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB,
 * 2026-07-18 (query_dump_k2.py against all_decompiled_k2.json /
 * all_data_k2.json - the live Ghidra MCP bridge was NOT used, per task
 * instructions, to avoid colliding with concurrent sessions on the same
 * Ghidra projects).
 *
 * Anchor: "../cobjectmgr.cpp" string at K2 0xc002b2d4 (K1: 0xc0022dcc).
 * Located via its literal-pool DAT_ holders (K1's own established technique)
 * rather than a live xref query: the string's signed data_value
 * (-0x3ffd4d2c) was matched against every DAT_ constant in the K2 dump,
 * turning up 5 holder addresses (0xc0009344, 0xc000994c, 0xc000929c,
 * 0xc000a094, 0xc000a000) referenced from 5 distinct K2 functions - fewer
 * than K1's own 9-xref sweep found, most likely because this static dump's
 * xref set is narrower than a live Ghidra xref search, not because K2 itself
 * has fewer real references. All 5 are accounted for below (4 reconstructed,
 * 1 - the wire-protocol dispatcher - cited only for the same one confirmed
 * connection K1's own file already documents, not reconstructed here).
 *
 * STRUCTURAL FINDING vs K1: cobjectmgr_state gained 8 bytes of leading
 * padding in K2. Every field K1 found at (+8/+0xc/+0x10/+0x14/+0x18/+0x20)
 * is confirmed present in K2 at exactly (+0x10/+0x14/+0x18/+0x1c/+0x20/+0x28)
 * - a uniform +8 shift, confirmed independently in cobjectmgr_tick,
 * cobjectmgr_handle_type_a AND cobjectmgr_handle_type_b (all three K2
 * functions agree on the same shifted offsets). Not resolved what occupies
 * the new leading 8 bytes.
 *
 * NOT resolved this pass (see note above cobjectmgr_hardware_fault_watchdog
 * below): despite an exhaustive structural sweep (every infinite-loop
 * function in the whole K2 decompile set was enumerated and checked by size
 * and call-count), no K2 function matching cobjectmgr_hardware_fault_
 * watchdog's shape (a bare `do { wait(2,MASK,...); ack(2,MASK); fault(...); }
 * while(true);`, 3 calls, ~76 bytes) was found. Flagged as NEEDS LIVE QUERY
 * rather than guessed at.
 */

#include <stdint.h>

/* ------------------------------------------------------------------------- *
 *  Shared LCD drawing primitives, duplicated here from clcdc.c per this
 *  project's own per-file convention - see K1's cobjectmgr.c for the
 *  rationale. K2 addresses are this file's own literal-pool copies
 *  (confirmed via cobjectmgr_handle_type_a's/_type_b's own decompile),
 *  their real owning definitions live in clcdc.c (sibling file, not edited
 *  here).
 * ------------------------------------------------------------------------- */
struct clcdc_cursor {
	uint8_t  pad0[4];
	uint32_t stride;		/* +4 */
	uint8_t  pad1[2];
	int16_t  x, y;			/* +8, +0xa */
	int16_t  left_margin;		/* +0xc */
	int16_t  right_edge;		/* +0xe */
};
extern void clcdc_cursor_init_from_offset(struct clcdc_cursor *c, uint32_t offset, int width);	/* FUN_c0011f80 (K1: FUN_c001505c) */
extern uint16_t *clcdc_framebuffer;		/* *DAT_c00092ac here (K1: DAT_c0007c24) */
extern uint16_t *clcdc_palette;		/* *DAT_c00092b0 here (K1: DAT_c0007c28) */
extern uint32_t  clcdc_fb_pixel_count_limit;	/* DAT_c00092a8 here (K1: DAT_c0007c20) */

extern struct clcdc_cursor *cobjectmgr_draw_cursor;	/* DAT_c00092a4 (K1: DAT_c0007c1c) */
extern void crypto_at88_fault(const void *unused_arg1, const char *file, int line);	/* FUN_c000a730 (K1: FUN_c000919c) - confirmed identical body/signature in K2's static dump */

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_state - see the file header's "STRUCTURAL FINDING vs K1" note:
 *  same fields as K1, uniformly shifted +8 bytes.
 * ------------------------------------------------------------------------- */
struct cobjectmgr_state {
	uint8_t  pad0[0x10];
	int32_t  stream_remaining;	/* +0x10 (K1: +8) */
	int32_t  scratch_count;	/* +0x14 (K1: +0xc) */
	uint8_t *current_object;	/* +0x18 (K1: +0x10) */
};

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_handle_type_a - tag 0xc4 (-0x3c) handler. K2 @0xc0009158
 *  (K1 @0xc0007ad0). Confirmed structurally IDENTICAL to K1's reconstruction
 *  - same 11-byte record decode, same solid-colour pixel-run draw with the
 *  same right_edge wrap rule - only the struct offsets (+8 shift, see above)
 *  and literal-pool addresses differ. Ported directly from K1; see K1's own
 *  file for the full behavioural writeup (including the still-open "trailing
 *  3-byte width field not accounted for in stream bookkeeping" question,
 *  which reproduces identically here: K2's decompile still only advances
 *  stream_remaining/current_object by 8, never by 11).
 * ------------------------------------------------------------------------- */
void cobjectmgr_handle_type_a(struct cobjectmgr_state *mgr)	/* FUN_c0009158 */
{
	uint8_t *p = mgr->current_object;
	int32_t orig_remaining = mgr->stream_remaining;
	uint32_t offset, width;
	uint8_t color;

	if (p[3] != 0xc4)
		crypto_at88_fault(0, 0 /* DAT_c000929c, "../cobjectmgr.cpp" */, 0 /* DAT_c00092a0 */);

	mgr->stream_remaining = orig_remaining - 4;
	mgr->scratch_count = ((uint32_t)p[1] << 8) | ((uint32_t)p[0] << 16) | (uint32_t)p[2];
	mgr->current_object = p + 4;

	mgr->stream_remaining = orig_remaining - 8;
	mgr->current_object = p + 8;

	offset = ((uint32_t)p[5] << 8) | ((uint32_t)p[4] << 16) | (uint32_t)p[6];
	color  = p[7];
	width  = ((uint32_t)p[9] << 8) | ((uint32_t)p[8] << 16) | (uint32_t)p[10];	/* still-open trailing-bytes question, see header note */

	clcdc_cursor_init_from_offset(cobjectmgr_draw_cursor, offset, (int)width);

	if (mgr->scratch_count > 0) {
		do {
			uint32_t idx = (uint32_t)(uint16_t)cobjectmgr_draw_cursor->x
				     + (uint32_t)(uint16_t)cobjectmgr_draw_cursor->y * 800u;
			if (idx <= clcdc_fb_pixel_count_limit)
				clcdc_framebuffer[idx] = clcdc_palette[color];

			uint16_t right_edge = (uint16_t)cobjectmgr_draw_cursor->right_edge;
			cobjectmgr_draw_cursor->x++;
			if (right_edge < (uint16_t)cobjectmgr_draw_cursor->x) {
				cobjectmgr_draw_cursor->x = cobjectmgr_draw_cursor->left_margin;
				cobjectmgr_draw_cursor->y++;
			}
			mgr->scratch_count--;
		} while (mgr->scratch_count > 0);
	}
}

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_handle_type_b - tag 0xc6 (-0x3a) handler. K2 @0xc0008d24
 *  (K1 @0xc000769c). NOT transcribed, same treatment as K1's own file (too
 *  dense to transcribe with confidence). Confirmed structurally the SAME
 *  dense four-way wraparound pointer walk as K1 - same 0x321 wrap distance
 *  constant, same shift amounts (0x18, 0xf, 7, plain low byte) extracting 4
 *  packed sub-fields per source dword, same two-code-path split. Field
 *  offsets confirmed shifted +8 exactly like cobjectmgr_state above: source
 *  payload now at +0x18 (K1 +0x10), write cursor +0x1c (K1 +0x14), wrap
 *  sentinel +0x20 (K1 +0x18), width +0x28 (K1 +0x20) - independent
 *  confirmation of the same struct-padding finding cobjectmgr_tick and
 *  cobjectmgr_handle_type_a both already show.
 * ------------------------------------------------------------------------- */
void cobjectmgr_handle_type_b(struct cobjectmgr_state *mgr);	/* FUN_c0008d24, structure only - not transcribed, see above */

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_tick - the confirmed core. K2 @0xc00092b4 (K1 @0xc0007c2c).
 *  Structurally IDENTICAL to K1: occupied-slot check, tag dispatch to
 *  type_a/type_b, hard fault on unrecognized tag, slot clear, then the same
 *  two "release"/"cleanup" calls (see their own notes below - same "neither
 *  one is really object-specific" finding as K1), then the same unconditional
 *  per-tick hardware-ready poll.
 *
 *  GENUINELY NEW STRUCTURAL FINDING vs K1: this function's only static
 *  caller in the K2 dump is FUN_c000a6dc, an 28-byte function whose ENTIRE
 *  body is `do { cobjectmgr_tick(param_1); } while(true);` - a bare
 *  dedicated infinite loop, not K1's master-dispatcher-driven "once per
 *  tick" call site. This is consistent with (but does not by itself prove)
 *  K2 running cobjectmgr_tick as its own free-running task rather than a
 *  once-per-dispatch poll; not fully traced (FUN_c000a6dc's own caller shows
 *  as "None" in this dump, so what spawns that loop as a task is unresolved).
 *  Flagged here since it may matter to any sibling file that also touches
 *  the master-dispatcher / task-table code.
 * ------------------------------------------------------------------------- */
extern void cobjectmgr_release_object(void *slot);	/* FUN_c000183c (K1: FUN_c0001a80) - NOT actually object-specific, see below */
extern void cobjectmgr_object_cleanup(uint32_t implicit_state);	/* FUN_c0003824 (K1: FUN_c0003e04) */
extern void cobjectmgr_wait_ready(void);		/* FUN_c0003434 (K1: FUN_c000395c), shared with clcdc.c */

void cobjectmgr_tick(struct cobjectmgr_state *mgr)	/* FUN_c00092b4 */
{
	if (mgr->current_object) {
		int8_t tag = *((int8_t *)mgr->current_object + 3);

		if (tag == -0x3c)
			cobjectmgr_handle_type_a(mgr);
		else if (tag == -0x3a)
			cobjectmgr_handle_type_b(mgr);
		else
			crypto_at88_fault(0, 0 /* DAT_c0009344, "../cobjectmgr.cpp" */, 0 /* DAT_c0009348 */);

		mgr->current_object = 0;
		cobjectmgr_release_object(0 /* DAT_c000934c */);	/* real call site passes DAT_c000934c; callee ignores its argument regardless - see note below */
		cobjectmgr_object_cleanup(0x1e00000);	/* K1-established finding reproduces here: r0 at this point is release_object's own discarded fixed return value */
	}

	/* unconditional per-tick hardware-ready poll, same shape as K1 */
	extern int32_t *cobjectmgr_tick_state;		/* DAT_c0009350 (K1: DAT_c0007cc8) */
	if ((*(uint32_t *)(*cobjectmgr_tick_state + 8) & 4) != 0) {
		cobjectmgr_wait_ready();
		cobjectmgr_tick_state[4]++;
	}
}

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_release_object (FUN_c000183c, K1: FUN_c0001a80) - confirmed
 *  BYTE-FOR-BYTE identical body to K1:
 *
 *      undefined4 FUN_c000183c(void) { return 0x1e00000; }
 *
 *  Same "shared generic constant accessor, not cobjectmgr-owned, ignores any
 *  argument" finding as K1 - not re-derived here, just re-confirmed against
 *  K2's own decompile. K1's own file cites 8 unrelated call sites for this
 *  same behaviour; not re-swept for K2 (out of this file's scope).
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_object_cleanup (FUN_c0003824, K1: FUN_c0003e04) - confirmed
 *  identical one-line wrapper shape:
 *
 *      void FUN_c0003824(undefined4 param_1) { FUN_c000379c(param_1,1,1); }
 *
 *  FUN_c000379c (K1: FUN_c0003d7c) is NOT cobjectmgr-owned, same as K1 -
 *  cited only for this one confirmed fact, not independently re-verified
 *  against K2's own callers this pass.
 * ------------------------------------------------------------------------- */
extern void FUN_c000379c(uint32_t param_1, int slot_index, int sub_index);	/* not cobjectmgr-owned, see K1's own note */

void cobjectmgr_object_cleanup(uint32_t implicit_state)	/* FUN_c0003824 */
{
	FUN_c000379c(implicit_state, 1, 1);
}

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_notify_host (FUN_c000a028, K1: FUN_c0005c50). K2 @0xc000a028.
 *  Confirmed structurally IDENTICAL to K1 - same mode/extra_flag validation,
 *  same fixed 4-byte wire event (byte0=0, byte1=0, byte2=flags, byte3=7),
 *  same dead first parameter. Found via the same anchor-string DAT_ sweep
 *  that located this file's other functions (DAT_c000a094 holder).
 *
 *  Callers differ from K1 in COUNT and SHAPE: K1 had exactly 4 callers, all
 *  from one master dispatcher (FUN_c0008b64). K2's static dump shows 3
 *  callers, from 3 DIFFERENT functions (FUN_c000a0a0, FUN_c000a184,
 *  FUN_c000a1d0) - not traced further, out of this file's scope, but a real
 *  difference worth flagging rather than silently porting the "family of
 *  4, all from one dispatcher" description unchanged.
 * ------------------------------------------------------------------------- */
extern void at88_usb_tx_submit(void *dest_channel, const void *buf, int len);	/* FUN_c000bff0 (K1: FUN_c000acec) */
extern void *cobjectmgr_notify_channel;	/* DAT_c000a09c (K1: DAT_c0005cc0) */

void cobjectmgr_notify_host(void *unused_param1, int mode, char extra_flag)	/* FUN_c000a028 */
{
	uint8_t wire[4];
	uint8_t flags = 0;

	(void)unused_param1;	/* dead parameter, confirmed same as K1 */

	if (mode != 0) {
		if (mode == 1)
			flags = 2;
		else
			crypto_at88_fault(0, 0 /* DAT_c000a094, "../cobjectmgr.cpp" */, 0 /* DAT_c000a098 */);
	}
	if (extra_flag != 0)
		flags |= 1;

	wire[0] = 0;
	wire[1] = 0;
	wire[2] = flags;
	wire[3] = 7;

	at88_usb_tx_submit(cobjectmgr_notify_channel, wire, 4);
}

/* ------------------------------------------------------------------------- *
 *  cobjectmgr_hardware_fault_watchdog (K1: FUN_c00090b8, @0xc00090b8) -
 *  NEEDS LIVE QUERY. K1's version is a tiny (76-byte), unmistakably-shaped
 *  function: `do { wait_blocking(2,0x4000,1,&scratch,0xffffffff);
 *  ack(2,0x4000); crypto_at88_fault(...); } while(true);` - exactly 3 calls,
 *  no other logic.
 *
 *  This pass could NOT find a K2 match despite an exhaustive structural
 *  sweep of the static dump:
 *   - Every one of the 35 K2 functions containing an infinite loop
 *     (`while( true )` / `for(;;)`) was enumerated by address/size; none is
 *     close to 76 bytes with exactly 3 calls in the loop body (the nearest
 *     candidates - FUN_c000a6dc/28B, FUN_c0008c84/56B, FUN_c000e3c4/60B -
 *     were individually checked and are unrelated: FUN_c000a6dc turned out
 *     to be cobjectmgr_tick's own dedicated poll loop, see the note above
 *     cobjectmgr_tick).
 *   - K1's underlying wait/ack primitive pair was traced into K2: the ack
 *     half (K1 FUN_c001d318, "clear event-group bits") is confirmed present
 *     at K2 FUN_c001a01c (identical body/error-constant shape), but it has
 *     only ONE static caller in K2 (FUN_c000a58c, itself the master-
 *     dispatcher analogue of K1's FUN_c0008b64 - confirmed via its own
 *     opening `poll(1,...); ack(1,...);` pair matching K1's dispatcher
 *     preamble exactly). K1's ack primitive had TWO callers (dispatcher +
 *     watchdog); K2's has only one. The blocking wait-with-timeout half (K1
 *     FUN_c001d4c0, 5 params including a timeout) was searched for by
 *     signature and by its own callee FUN_c001a980 (K2 name for K1's
 *     FUN_c001dfc8) - FUN_c001a980 has exactly 2 callers in K2, both already
 *     identified as the non-blocking poll form, not a blocking form.
 *
 *  Working hypothesis, NOT confirmed: the watchdog task may have been
 *  restructured or its wait primitive replaced with a genuine OS-level
 *  blocking call not visible as a plain function call in this decompile
 *  (e.g. an inlined syscall/trap), or it is reached only via an indirect/
 *  task-table dispatch the same way cobjectmgr_object_destroy is (see K1's
 *  own note on that function) and therefore carries zero static xrefs
 *  in Ghidra's own analysis, not just this dump's narrower sweep.
 *
 *  Real name/behaviour intentionally NOT guessed at. Whoever needs this
 *  address (eva_board_main.c's watchdog wrapper) should treat it as
 *  unresolved rather than substitute a fabricated K2 address.
 *
 *  2026-07-19 LIVE GHIDRA FOLLOW-UP (read-only MCP bridge against
 *  kronos2s_v01r10_panel.elf, per this task's own 2-agent-cap
 *  authorization; zero Agent-tool subagent calls): re-checked from TWO
 *  independent angles, both NEGATIVE (genuinely ruling this out further,
 *  not just re-stating the same unswept gap):
 *
 *   1. get_xrefs_to live on the ack primitive (this project's
 *      kobj_eventflag_clear, K2 FUN_c001a01c, task_sched.c) still returns
 *      EXACTLY ONE caller (FUN_c000a58c) - identical to the static dump's
 *      own result. Ghidra's live xref database agrees with the narrower
 *      static dump here, not just the code-shape sweep - the watchdog's
 *      call to this primitive (if it exists at all) really does carry zero
 *      xrefs anywhere in this binary's own analysis, live or static.
 *   2. A companion pass through task_sched.c live-read the ROM autostart
 *      table sched_tcb_table_init_and_autostart walks (0xC002A68C ids /
 *      0xC002A698 cfg records) byte-exact: it holds EXACTLY 3 tasks, and all
 *      three are now positively identified as something else entirely (a
 *      former-eva_board_init_table+final_setup+boot_status_dispatch task,
 *      a main-loop task, and - genuinely surprising - eva_board_main.c's own
 *      previously-unlocated "../EvaBoardMain.cpp" fault xref, see
 *      task_sched.c's own 2026-07-19 header section for the full byte-level
 *      trace). None of the 3 is cobjectmgr-shaped (none references this
 *      file's own struct offsets or crypto_at88_fault call convention). This
 *      is a real, data-backed NEGATIVE result: whatever runs
 *      cobjectmgr_hardware_fault_watchdog, it is NOT one of K2's ROM-table
 *      autostart entries - ruling out that specific mechanism concretely,
 *      rather than leaving it as an unswept possibility.
 *
 *  Net effect: this function's real K2 address remains genuinely
 *  unresolved, but two more plausible discovery mechanisms have now been
 *  tried and concretely ruled out rather than merely left unswept.
 * ------------------------------------------------------------------------- */

/*
 * cobjectmgr_free_list_recursive (K1: FUN_c0015bc8, @0xc0015bc8). K2
 * @0xc0012af0. Confirmed BYTE-FOR-BYTE identical shape to K1 (same
 * self-recursive singly-linked-list walk-and-free, same register-continuity
 * treatment for the argument-less recursive call).
 */
extern void heap_free(void *unused_handle, void *ptr);	/* FUN_c0012e58 (K1: FUN_c0015f30), see heap_alloc.c */

void cobjectmgr_free_list_recursive(void *unused_handle, void **node)	/* FUN_c0012af0 */
{
	if (*node != 0)
		cobjectmgr_free_list_recursive(unused_handle, (void **)*node);
	heap_free(unused_handle, node);
}

/*
 * cobjectmgr_object_destroy - real C++ virtual destructor. K1: FUN_c0015bf8
 * @0xc0015bf8. K2 @0xc0012b20. Confirmed BYTE-FOR-BYTE identical body/struct
 * layout to K1 - same self-reference sentinel guard, same 15-bucket hash
 * table walk at +0x4c, same circular list at +0x148/+0x14c, same single
 * extra pointer at +0x54, same vtable-slot virtual call at +0x38/+0x3c, same
 * sibling-destroy recursion at +0x25c. This struct's offsets are UNCHANGED
 * from K1 (unlike cobjectmgr_state above) - it is a different struct
 * (the widget/object being destroyed, not the dispatch state), so the +8
 * padding finding above does not apply here. Zero static callers in K2 too,
 * consistent with K1's own "reached only via vtable dispatch" conclusion.
 */
extern void *cobjectmgr_object_destroy_self_ref;	/* DAT_c0012c00 (K1: DAT_c0015cd8) */

struct cobjectmgr_widget {
	uint8_t  pad0[0x38];
	void    *vtable_thunk_holder;			/* +0x38 */
	void   (*vtable_slot)(void *self);		/* +0x3c */
	uint8_t  pad1[0x4c - 0x40];
	void   **hash_buckets;				/* +0x4c */
	uint8_t  pad2[0x54 - 0x50];
	void    *extra_ptr;				/* +0x54 */
	uint8_t  pad3[0x148 - 0x58];
	void    *list_head;				/* +0x148 */
	void    *list_sentinel;			/* +0x14c */
	uint8_t  pad4[0x25c - 0x150];
	void    *sibling_destroy_list;			/* +0x25c */
};

void cobjectmgr_object_destroy(struct cobjectmgr_widget *obj)	/* FUN_c0012b20 */
{
	int i;
	void **link;

	if ((void *)obj == cobjectmgr_object_destroy_self_ref)
		return;

	if (obj->hash_buckets != 0) {
		for (i = 0; i < 0xf; i++) {
			link = (void **)obj->hash_buckets[i];
			while (link != 0) {
				void *next = *link;
				heap_free(obj, link);
				link = (void **)next;
			}
		}
		heap_free(obj, obj->hash_buckets);
	}

	if (obj->list_head != 0) {
		link = (void **)obj->list_head;
		while ((void *)link != obj->list_sentinel) {
			void *next = *link;
			heap_free(obj, link);
			link = (void **)next;
		}
	}

	if (obj->extra_ptr != 0)
		heap_free(obj, obj->extra_ptr);

	if (obj->vtable_thunk_holder != 0) {
		obj->vtable_slot(obj);
		if (obj->sibling_destroy_list != 0) {
			cobjectmgr_free_list_recursive(obj, (void **)obj->sibling_destroy_list);
			return;
		}
		return;
	}
}

/* ------------------------------------------------------------------------- *
 * NOT FOUND IN K2 / not reconstructed here (see file header):
 *  - cobjectmgr_hardware_fault_watchdog - NEEDS LIVE QUERY, see its own note
 *    above.
 *  - The wire-protocol command dispatcher (K1: FUN_c0007d1c) that populates
 *    this struct's current_object/stream_remaining fields - its K2
 *    counterpart is FUN_c0009b54 (confirmed via the same anchor-string DAT_
 *    holder sweep, DAT_c000a000 - opcode switch has the same 0xc4/0xc6
 *    cases doing `*(byte**)(state+0x18)=payload; *(uint*)(state+0x10)=len;
 *    return 0;`, i.e. the same +8-shifted offsets as everything else in this
 *    file). Not reconstructed here, same as K1's own file - cited only for
 *    this one confirmed connection, and to record its K2 address in case a
 *    future wire_dispatch.c-equivalent pass needs it.
 * ------------------------------------------------------------------------- */
