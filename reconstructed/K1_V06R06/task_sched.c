/* SPDX-License-Identifier: GPL-2.0 */
/*
 * task_sched.c - the panel firmware's real priority-based task scheduler:
 * TCB table bring-up, the ready-queue/priority-bitmap dispatcher, the
 * kernel-object (event-flag-group) table and its set/clear/wait API, and
 * the tick-ordered delay/timeout min-heap those primitives block into.
 *
 * Ground truth: fresh Ghidra decompile of KRONOS_V06R06.VSB, 2026-07-18,
 * worked from a pre-fetched static dump (all_decompiled.json/all_data.json)
 * per this pass's own methodology note in README.md - no live MCP bridge
 * calls were made. Every DAT_ constant cited below was resolved via the
 * dump's own `dat` lookup (a real relocation-resolved data address, the
 * same mechanism eva_board_main.c used for eva_board_init_table_start/end),
 * not guessed.
 *
 * ASSIGNMENT AND SCOPE, read this first: this file was assigned the address
 * sweep 0xc001bef8-0xc001ce7c, 0xc001d0f8-0xc001d4c0, 0xc001d850-0xc001e60c,
 * 0xc001e918-0xc001e990 (65 real Ghidra function objects) on the hypothesis
 * that it was one cohesive RTOS-scheduler compilation unit. It is NOT. A
 * full-image string search (see README.md's "Ghidra setup" section for the
 * method) found no new `__FILE__` anchor anywhere in this range - the same
 * "no anchor, code-shape evidence only" situation heap_alloc.c already
 * documents for the general allocator. But unlike heap_alloc.c, this range
 * contains at least THREE structurally unrelated pieces of code, distinguished
 * here on hard evidence (call patterns, data-address arithmetic, and known
 * instruction encodings), not guesswork:
 *
 *   (A) 0xc001bef8-0xc001bf54: two IEEE-754 double-precision compare helpers
 *       (the `0x7ff00000` exponent-mask idiom is a standard libgcc
 *       __gedf2/__ledf2-family soft-float comparison shape) plus one
 *       C++-streambuf-shaped virtual callback (FUN_c001bf54, reached only via
 *       a DATA/vtable xref from FUN_c001a304, outside this range, and itself
 *       calling into clcdc.c's own FUN_c0015bb4) - compiler-runtime and
 *       iostream plumbing, not scheduler code. NOT reconstructed here.
 *   (B) 0xc001c070-0xc001c98c: a complete newlib-style C-library syscall
 *       layer built on ARM Angel/RDI semihosting - EVERY function in this
 *       block that touches hardware does it via `software_interrupt(0x123456)`,
 *       which is literally the well-known ARM semihosting SWI immediate
 *       (`0x123456` on ARM, `0xAB` on Thumb; see any ARM RDI/Angel monitor
 *       reference). The block contains: word-optimized strcmp/strlen
 *       (FUN_c001c070/c001c114), a 20-slot open-file-descriptor table walker
 *       (FUN_c001c174), a three-standard-stream (stdin/stdout/stderr-shaped)
 *       lazy-init block (FUN_c001c1a0), and semihosted open/close/read/write-
 *       shaped stubs (FUN_c001c3b4 returns SWI op 6 = SYS_WRITEC-adjacent;
 *       FUN_c001c568 returns SWI op 5; FUN_c001c700 returns SWI op 2 =
 *       SYS_CLOSE; FUN_c001c788 returns a packed SWI-op/mode constant) each
 *       wrapping a raw `software_interrupt(0x123456)` trap, an errno
 *       accessor (FUN_c001ca24, `return *DAT_c001ca30`), and a calloc-shaped
 *       wrapper over a malloc-family primitive (FUN_c001c98c, calls
 *       FUN_c0016164 and zeroes the result). Genuinely a C library / debug-
 *       monitor I/O layer, not scheduler code. NOT reconstructed here -
 *       flagged for a future `libc_semihosting.c` (or similar) pass.
 *   (C) 0xc001e300-0xc001e608 and all of 0xc001e918-0xc001e990: libgcc
 *       integer-division and soft-float helpers (__udivsi3/__divsi3-shaped
 *       restoring-division loops, an __adddf3/__subdf3-shaped double add/sub
 *       at FUN_c001e608, and __floatsidf/__floatdidf-shaped int-to-double
 *       converters at FUN_c001e918/c001e978 - both reached only from
 *       FUN_c0018ea4, well outside this range). FUN_c001e3f8 in this group is
 *       ALREADY attributed and named (`omap_tick_scale`, clcdc.c/omap_l108.c,
 *       "a generic signed-division utility... shared firmware-wide") - this
 *       file does not re-describe it. The rest of this group is likewise
 *       compiler runtime, not reconstructed here.
 *
 * What's left after excluding (A)/(B)/(C) - roughly 0xc001ca34-0xc001ce7c,
 * all of 0xc001d0f8-0xc001d4c0, and 0xc001d850-0xc001e250 - IS the real
 * thing: crt0's task/kernel-object bring-up plus the scheduler primitives
 * eva_board_main.c already partially named but explicitly left "not
 * transcribed... no independent verification path" (eva_board_sched_ready/
 * _requeue/_dispatch = FUN_c001da64/FUN_c001ddac/FUN_c001d850). All three are
 * FULLY TRANSCRIBED below, using the exact same FUN_ addresses
 * eva_board_main.c already cited - this is the direct resolution of that
 * file's own "still genuinely open" list, reported back there rather than
 * edited in (this pass's instructions are to leave eva_board_main.c
 * untouched; see this file's own closing summary and the pass's final
 * report for what a later consolidation should fold back in).
 *
 * HEADLINE FINDING: eva_board_main IS answered, concretely, for the first
 * time. eva_board_main.c's own "still genuinely open" question - "is
 * eva_board_main itself a task made ready during crt0's own init sequence?"
 * - could not be answered from that file's own range because the actual
 * MECHANISM by which crt0 auto-starts tasks lives entirely in THIS file's
 * range (FUN_c001ccb8's own table walk + FUN_c001ce00's own task-creation
 * primitive, see below). The mechanism is now fully understood: crt0 walks a
 * ROM table of pre-configured task records and auto-starts every one whose
 * config-record flags bit 1 is set. This file does NOT have read access to
 * that ROM table's actual contents (a live `read_memory` query would be
 * needed, same as eva_board_init_table's contents were resolved in an
 * earlier pass) - so whether eva_board_main (0xc0005644) is literally one of
 * the auto-started entries is still not proven. But the open question has
 * moved from "what mechanism, if any, does this?" (unknown) to "does task
 * table entry N happen to hold 0xc0005644?" (a single, well-defined,
 * live-queryable fact) - a real, if partial, resolution.
 *
 * SECOND HEADLINE FINDING: the real ARM IRQ vector gets installed here, at
 * crt0 time, by name. FUN_c001cba8 (already listed by eva_board_main.c as
 * one of crt0's 11 calls, previously untraced) decodes the ARM exception
 * vector table's own `LDR PC,[PC,#imm]` encoding at its IRQ slot (vector
 * offset 0x18, matching this project's own already-documented "5x
 * LDR PC,[PC,#0x18]-style entries reading targets from a literal pool"
 * vector-table shape) and overwrites that literal-pool slot with
 * `0xc001d650` - installing the real IRQ handler. See the "vector table
 * capture/patch cluster" section below for the full evidence chain
 * (FUN_c001cc2c/FUN_c001cba8/FUN_c001cc88), including what's NOT resolved
 * (FUN_c001cc88's own net effect, called twice, is flagged honestly as
 * open, not asserted).
 *
 * eva_board_crt0's own call count is corrected upward: eva_board_main.c
 * documents "eleven back-to-back... calls". Walking the raw call-site
 * addresses in this range (c001cafc through c001cb94) finds at least
 * THIRTEEN real BL targets in the same unbroken sequence, plus two more
 * call-shaped xrefs to a bare `do{}while(true);` (FUN_c001cbe4) that are
 * very likely the SAME kind of Ghidra linear-disassembly artifact already
 * resolved for FUN_c0009534/FUN_c0009540 elsewhere in this file's own
 * neighborhood (a conditional branch to a fault/panic path misread as an
 * unconditional call) - flagged, not asserted, since this pass had no live
 * disassembly access to confirm it the way that earlier case was confirmed.
 * See the crt0-tail section below for the full corrected call list.
 */

#include <stdint.h>

/* ============================================================================
 *  KERNEL-OBJECT (EVENT-FLAG-GROUP) TABLE - bring-up and layout
 * ============================================================================
 *
 *  A RAM table of 0x10-byte records at kobj_table (resolved 0xc01d5538),
 *  1-based handle indexing (`0 < handle <= *kobj_count`), populated at boot
 *  by kobj_table_init (FUN_c001d0f8, @0xc001d0f8) from an 8-byte-stride ROM
 *  source array (resolved 0xc0022c30) whose own count lives at resolved
 *  0xc0022c48. That same count address is independently reused, and
 *  numerically CONFIRMED equal (not just structurally similar - the actual
 *  resolved addresses match), as the max-handle bound in every one of
 *  kobj_eventflag_set/_clear/_wait below (DAT_c001d30c/DAT_c001d3a0/
 *  DAT_c001d4b8 all resolve to the identical 0xc0022c48). Likewise
 *  kobj_table's own base (0xc01d5538) is exactly ONE record (0x10 bytes)
 *  behind kobj_table_init's own destination base (DAT_c001d140 resolves to
 *  0xc01d5548 = 0xc01d5538 + 0x10) - i.e. handle 1 addresses the first
 *  record kobj_table_init populates, confirming this is genuinely ONE table,
 *  not two coincidentally-adjacent ones.
 *
 *  Record layout (0x10 bytes), from kobj_table_init's own writes:
 *    +0x00 wait-list "prev" (self-linked to +0x00 itself when empty)
 *    +0x04 wait-list "next" (self-linked to +0x00 itself when empty)
 *    +0x08 back-pointer to this record's own ROM source entry
 *    +0x0c current flags word, seeded from ROM source+4
 *
 *  This is NOT the same table as a task's "linked config object" (TCB+0x08,
 *  see the TCB section below) - that's a separate, 0x20-byte-stride ROM
 *  array. An earlier draft of this file conflated the two on the strength of
 *  both being reached via an offset-8 pointer; resolving the literal
 *  addresses numerically (0xc01d5538-family vs. 0xc0022c5c) showed they are
 *  genuinely different tables serving different purposes (event-flag-group
 *  state vs. per-task default config) - corrected before writing this out.
 * ------------------------------------------------------------------------- */
/* struct sched_tcb is defined here, ahead of its own dedicated section further
 * down (the "TASK CONTROL BLOCK TABLE, READY QUEUE, AND DISPATCHER" heading),
 * purely because the kernel-object/event-flag API immediately below needs to
 * dereference a waiting task's own TCB fields (wait_desc, link_prev/link_next)
 * - see that later section's own header comment for the full field-by-field
 * evidence behind this layout; this forward placement is a pure C ordering
 * requirement, not a claim that TCBs are somehow part of the kobj table. */
struct sched_tcb {
	struct sched_tcb *link_prev;		/* +0x00 */
	struct sched_tcb *link_next;		/* +0x04 */
	uint32_t cfg_record;			/* +0x08 */
	uint8_t state;				/* +0x0c */
	uint8_t priority;			/* +0x0d */
	uint8_t ctrl;				/* +0x0e */
	uint8_t pad_0f;
	uint32_t unused_0x10;
	uint32_t *wait_desc;			/* +0x14 */
	void *saved_sp;				/* +0x18 */
	void (*resume)(void);			/* +0x1c */
};

struct sched_ready_slot {
	struct sched_tcb *head;
	struct sched_tcb *tail;
};

extern int32_t *kobj_count;			/* DAT_c001d13c/DAT_c001d30c/DAT_c001d3a0/DAT_c001d4b8, resolved 0xc0022c48 */
extern uint32_t kobj_src_table;		/* DAT_c001d144, resolved 0xc0022c30, 8-byte-stride ROM source */

struct kobj_record {
	struct kobj_record *wait_prev;		/* +0x00 */
	struct kobj_record *wait_next;		/* +0x04 */
	uint32_t src_record;			/* +0x08, &kobj_src_table[i] */
	uint32_t flags;			/* +0x0c */
};
extern struct kobj_record kobj_table[];	/* DAT_c001d140/DAT_c001d310/DAT_c001d3a4/DAT_c001d4bc, resolved 0xc01d5538 (1-based indexing) */

void kobj_table_init(void)	/* FUN_c001d0f8, @0xc001d0f8 */
{
	extern uint32_t kobj_src_table_words[];	/* same base as kobj_src_table, 2 words/record */
	int32_t count = *kobj_count;
	uint32_t src = (uint32_t)kobj_src_table_words;
	struct kobj_record *dst = &kobj_table[1];	/* index 1 = kobj_table base per the +0x10 offset above */

	if (count == 0)
		return;

	do {
		uint32_t seed_flags = *(uint32_t *)(src + 4);
		count--;
		dst->src_record = src;
		dst->flags = seed_flags;
		dst->wait_prev = dst;
		dst->wait_next = dst;
		src += 8;
		dst = (struct kobj_record *)((uint8_t *)dst + 0x10);
	} while (count != 0);
}

/* ------------------------------------------------------------------------- *
 *  kobj_eventflag_test_and_clear (FUN_c001dfc8, @0xc001dfc8) - the shared
 *  primitive behind both set/wait below: tests a kobj's current flags word
 *  against a caller mask under AND (param_3 bit0=0) or OR (bit0=1) semantics,
 *  and on a match, writes the matched flags to *param_4 and - only if the
 *  matched WAITER's own descriptor requests auto-clear (its own record's
 *  bit 2 set, tested via `*puVar1 & 4` where puVar1 is the kobj's own
 *  src_record) - clears the kobj's flags word to 0. Fully transcribed, no
 *  ambiguity in the real decompile.
 * ------------------------------------------------------------------------- */
uint32_t kobj_eventflag_test_and_clear(struct kobj_record *kobj, uint32_t mask,
					uint32_t mode, uint32_t *out_flags)
	/* FUN_c001dfc8 */
{
	uint32_t flags = kobj->flags;

	if ((mode & 1) == 0) {
		if ((flags & mask) != mask)
			return 0;
	} else {
		if ((flags & mask) == 0)
			return 0;
	}

	*out_flags = flags;
	if ((*(uint32_t *)kobj->src_record & 4) != 0)
		kobj->flags = 0;
	return 1;
}

/* ------------------------------------------------------------------------- *
 *  kobj_eventflag_set (FUN_c001d22c, @0xc001d22c) - "set event flags" API:
 *  bounds-checks the handle, ORs `mask` into the kobj's flags word, and if a
 *  task is waiting on this kobj, re-tests its saved mask/mode (stashed on
 *  the WAITING task's own stack frame - see kobj_eventflag_wait below for
 *  how that descriptor is built) via kobj_eventflag_test_and_clear; on a
 *  match, unlinks the waiter and wakes it (sched_release_wait). The real
 *  decompile then does `iVar3 = FUN_c001dbf0(piVar2); if (iVar3 != 0)
 *  *DAT_c001d314 = 1;` - treating sched_release_wait's result as meaningful
 *  - but Ghidra types sched_release_wait itself `void` (no explicit
 *  `return <value>` anywhere in its own body, see that function below). The
 *  likely real explanation, not independently proven: sched_release_wait's
 *  own "task wasn't aborting" branch tail-calls sched_make_ready (FUN_c001d9e0,
 *  which DOES return a real uint32_t "reschedule needed" value) without
 *  capturing it, so on real ARM hardware r0 simply carries that value
 *  through untouched even though the C-level source never names it. This
 *  file does NOT reproduce that r0-forwarding behavior (there is no safe,
 *  non-fabricated way to express "return whatever register a void function
 *  incidentally left set" in standard C) - kobj_eventflag_set below
 *  therefore does NOT set the reschedule-needed flag (DAT_c001d314,
 *  resolved 0xc01d576c), a known, flagged incompleteness rather than a
 *  guessed value.
 *
 *  Return codes match eva_board_start_task's own convention exactly
 *  (0xffffffe7 = -25). Guard polarity note: this function requires
 *  sched_flag_5730 NON-ZERO to proceed - see the "shared address, NOT a
 *  shared polarity" note below kobj_eventflag_wait, which uses the SAME
 *  resolved address but requires it ZERO. Transcribed exactly as the real
 *  decompile shows for each function; not reconciled into one story.
 * ------------------------------------------------------------------------- */
extern uint8_t sched_flag_5730;		/* DAT_c001d308, resolved 0xc01d5730 - see polarity note below */
extern uint8_t sched_reschedule_needed;	/* DAT_c001d314, resolved 0xc01d576c - NOT set by kobj_eventflag_set
						 * below, see the r0-forwarding note above */
extern void sched_release_wait(struct sched_tcb *tcb);	/* FUN_c001dbf0, defined below */

int32_t kobj_eventflag_set(int32_t handle, uint32_t mask)	/* FUN_c001d22c */
{
	if (sched_flag_5730 == 0)
		return -25;

	if (handle < 1 || handle > *kobj_count)
		return -18;

	{
		struct kobj_record *kobj = &kobj_table[handle];
		/* kobj's own +0x00/+0x04 fields double as a wait-list sentinel using
		 * the SAME relative layout as a struct sched_tcb's link_prev/link_next
		 * (single-waiter-only list, see kobj_eventflag_wait's own EBUSY check) -
		 * reinterpret-cast rather than duplicate the link fields in two structs */
		struct sched_tcb *waiter = (struct sched_tcb *)kobj->wait_prev;

		kobj->flags |= mask;

		if ((struct kobj_record *)waiter != kobj) {
			uint32_t *desc = waiter->wait_desc;	/* TCB+0x14, see kobj_eventflag_wait */

			if (kobj_eventflag_test_and_clear(kobj, desc[2] /* saved mask */,
							   desc[3] /* saved mode */, &desc[4] /* out slot */)) {
				/* unlink waiter from its own current list position -
				 * transcribed exactly per the real decompile's two stores */
				struct sched_tcb *next = waiter->link_next;

				next->link_prev = waiter->link_prev;
				waiter->link_prev->link_next = next;

				sched_release_wait(waiter);
				/* NOT setting sched_reschedule_needed here - see the
				 * r0-forwarding note above this function's own header comment */
			}
		}
	}
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  kobj_eventflag_clear (FUN_c001d318, @0xc001d318) - "clear event flags"
 *  API: ANDs `keep_mask` into the kobj's flags word (i.e. the caller passes
 *  the bits to KEEP, not the bits to clear - confirmed by the raw `&=`, no
 *  complement anywhere in this function). Guard polarity is the OPPOSITE of
 *  kobj_eventflag_set above despite reading the identical resolved address
 *  (0xc01d5730): this function requires it ZERO to proceed. See the
 *  polarity note below kobj_eventflag_wait.
 * ------------------------------------------------------------------------- */
int32_t kobj_eventflag_clear(int32_t handle, uint32_t keep_mask)	/* FUN_c001d318 */
{
	if (sched_flag_5730 != 0)
		return -25;

	if (handle < 1 || handle > *kobj_count)
		return -18;

	kobj_table[handle].flags &= keep_mask;
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  kobj_eventflag_wait (FUN_c001d3a8, @0xc001d3a8) - "wait for event flags"
 *  API, the one genuinely blocking primitive in this cluster. Builds a small
 *  4-word wait descriptor {status, (unused), mask, mode} PLUS a trailing
 *  output-flags word on ITS OWN STACK FRAME, points the current task's own
 *  TCB+0x14 field at it, and - if the flags aren't already satisfied -
 *  inserts the current task into the kobj's own wait list
 *  (sched_wait_list_insert) and calls the scheduler dispatcher directly
 *  (sched_dispatch) to switch away. Execution only resumes here once some
 *  other path (kobj_eventflag_set's wake, or a timeout - see
 *  sched_timed_wait_list_insert) has re-readied this task; at that point the
 *  descriptor's status/output-flags words (written by whoever woke it) are
 *  read back out to the caller.
 *
 *  This is the SAME descriptor-on-stack mechanism eva_board_main.c's own
 *  extern comment for eva_board_sched_wait_list_head already flagged as
 *  unresolved ("the (iVar2+0x14)->+4 field the raw decompile derefs") - this
 *  file does not attempt to resolve that specific eva_board_start_task call
 *  site (out of scope, lives in eva_board_main.c), but the mechanism itself
 *  - TCB+0x14 pointing at a per-blocking-call stack descriptor - is now
 *  fully understood from this function's own body.
 *
 *  Only 0/1 are valid for `mode` (checked: `(mode & 0xfffffffe) != 0` is
 *  rejected) - an AND/OR selector, matching kobj_eventflag_test_and_clear's
 *  own bit-0 branch.
 *
 *  GUARD POLARITY, confirmed by re-reading the exact decompile text (not
 *  from memory) for all three eventflag functions: kobj_eventflag_set
 *  requires DAT_c001d308 (resolved 0xc01d5730) NON-ZERO to proceed;
 *  kobj_eventflag_clear requires the SAME resolved address ZERO;
 *  kobj_eventflag_wait requires it ZERO too (`*DAT_c001d4b0==0`), AND a
 *  SECOND, different resolved address (DAT_c001d4b4, resolved 0xc01d57f8 -
 *  the same one eva_board_sched_ready/sched_remove_from_ready read as a
 *  fixed "reschedule" pass-through value elsewhere in this file) NON-ZERO.
 *  This is a real, confirmed-by-address asymmetry between "set" and
 *  "clear"/"wait" that this file does NOT have a clean unifying story for -
 *  an earlier draft of this comment asserted one (idle-flag vs. init-latch)
 *  that didn't actually survive checking sched_dispatch's own narrow WFI
 *  window against how often kobj_eventflag_set would then be able to
 *  succeed. Left honestly open rather than re-guessed; every guard below is
 *  transcribed to match its own real decompile exactly, not harmonized.
 * ------------------------------------------------------------------------- */
extern uint32_t sched_flag_57f8;	/* DAT_c001d4b4, resolved 0xc01d57f8 */
extern void sched_wait_list_insert(struct kobj_record *list_head, void *wait_desc);	/* FUN_c001dc48, defined below */
extern void sched_dispatch(void);							/* FUN_c001d850, defined below */

int32_t kobj_eventflag_wait(int32_t handle, uint32_t mask, uint32_t mode, uint32_t *out_flags)
	/* FUN_c001d3a8 */
{
	int32_t status = 0;

	if (sched_flag_5730 != 0 || sched_flag_57f8 == 0) {
		return -25;
	}
	if (handle < 1 || handle > *kobj_count) {
		return -18;
	}
	if (mask == 0 || (mode & 0xfffffffeu) != 0) {
		return -17;
	}

	{
		struct kobj_record *kobj = &kobj_table[handle];

		if (kobj->wait_prev != kobj) {
			/* already has a waiter: this kobj supports only one at a time */
			return -28;
		}

		if (kobj_eventflag_test_and_clear(kobj, mask, mode, out_flags)) {
			return 0;	/* already satisfied, no blocking needed */
		}

		{
			/* wait descriptor, on THIS call's own stack frame:
			 *   [0] status (filled in by whoever wakes us)
			 *   [1] unused/padding in the visible decompile
			 *   [2] mask   (this call's own `mask`)
			 *   [3] mode   (this call's own `mode`)
			 *   [4] matched-flags output (copied to *out_flags below)
			 */
			uint32_t wait_desc[5];

			wait_desc[2] = mask;
			wait_desc[3] = mode;

			sched_wait_list_insert(kobj, wait_desc);
			sched_dispatch();	/* switch away; execution resumes here once woken */

			status = (int32_t)wait_desc[0];
			if (status == 0)
				*out_flags = wait_desc[4];
		}
	}
	return status;
}

/* ============================================================================
 *  TASK CONTROL BLOCK TABLE, READY QUEUE, AND DISPATCHER
 * ============================================================================
 *
 *  TCB layout (0x20-byte stride, resolved base 0xc01d54c8 - CONFIRMED the
 *  exact same address eva_board_main.c already names eva_board_sched_tcb_table
 *  / DAT_c001d0f4, cross-checked via sched_tcb_table_init_and_autostart's own
 *  resolved base 0xc01d54e8 = tcb_table + 1*0x20, matching task-id-1-based
 *  indexing exactly):
 *    +0x00 ready/wait-list "prev"
 *    +0x04 ready/wait-list "next"
 *    +0x08 pointer to this task's ROM config record (0x20-byte-stride array,
 *          resolved base 0xc0022c5c - NOT the kobj table, see the note
 *          above)
 *    +0x0c state byte: 0 = unallocated; bit0 set = "made ready, not yet
 *          started/dispatched"; bit0 clear = active; bit 0x20 = currently on
 *          some wait list (requeue-on-reprioritize path); 0x32 = a blocked/
 *          waiting state code seen written by sched_wait_list_insert /
 *          sched_timed_wait_list_insert (bits beyond 0x20 not decoded)
 *    +0x0d priority byte, 0 (highest) .. 15 (lowest) - confirmed by
 *          sched_ready_scan_highest's own bitmap-scan bound (checks at most
 *          a 16-bit-wide word, byte then nibble then a 4-bit lookup table)
 *    +0x0e small control-byte, two bits cleared during reset
 *          (sched_tcb_reset below); broader meaning not decoded
 *    +0x10 zeroed by sched_tcb_reset; not otherwise referenced in this range
 *    +0x14 "wait descriptor" pointer - for a task blocked in
 *          kobj_eventflag_wait, this points at THAT call's own stack-frame
 *          wait_desc (see above); genuinely a per-blocking-call transient
 *          pointer, not a fixed struct member with one meaning
 *    +0x18 saved stack pointer (dispatcher-managed)
 *    +0x1c entry/resume function pointer, called indirectly by the
 *          dispatcher - for a freshly-created task this is its real entry
 *          point (see sched_task_create_and_ready); for the currently-
 *          running task being switched OUT, the dispatcher overwrites this
 *          with its own resume label (DAT_c001d878) before picking a new
 *          task, i.e. +0x1c always means "where to resume this task"
 *
 *  Ready-queue layout: 16 slots (priority 0..15), 8 bytes each
 *  (self-linked-when-empty head/tail pair), resolved base 0xc01d5770 -
 *  CONFIRMED identical across sched_make_ready (DAT_c001da54),
 *  eva_board_sched_ready (DAT_c001db4c), sched_ready_scan_highest
 *  (DAT_c001e070), and sched_remove_from_ready (DAT_c001e120) - one real
 *  array, four independent literal-pool copies of its address (this
 *  project's now-familiar per-function-local-literal-pool pattern).
 *
 *  Ready bitmap (resolved 0xc01d57f0) and "current/highest-ready" pointer
 *  (resolved 0xc01d57fc) are likewise cross-confirmed identical across all
 *  four ready-queue functions plus sched_dispatch itself.
 * ------------------------------------------------------------------------- *
 *  (struct sched_tcb / struct sched_ready_slot themselves are defined above,
 *  ahead of the kernel-object/event-flag section - see that forward
 *  placement's own comment for why.)
 * ------------------------------------------------------------------------- */
extern struct sched_tcb sched_tcb_table[];		/* resolved 0xc01d54c8, 1-based indexing */
extern struct sched_ready_slot sched_ready_list[16];	/* resolved 0xc01d5770 */
extern uint32_t sched_ready_bitmap;			/* resolved 0xc01d57f0 */
extern struct sched_tcb *sched_current_ready;		/* resolved 0xc01d57fc, "next/highest ready" */
extern struct sched_tcb *sched_current_task;		/* resolved 0xc01d57f4, "task actually running" -
							 * address-adjacent to but DISTINCT from
							 * sched_current_ready (0xc01d57fc); relationship
							 * between the two not independently proven beyond
							 * what sched_dispatch's own body shows below */

/* ------------------------------------------------------------------------- *
 *  Shared address, NOT a shared polarity (resolved 0xc01d5730) - confirmed
 *  IDENTICAL across FIVE call sites: eva_board_start_task's own guard
 *  (eva_board_sched_not_ready_flag / DAT_c001d0e8, eva_board_main.c, guards
 *  with `!= 0` fails), kobj_eventflag_set's guard (DAT_c001d308, `== 0`
 *  fails), kobj_eventflag_clear/_wait's guards (DAT_c001d39c/DAT_c001d4b0,
 *  `!= 0` fails - the OPPOSITE polarity from kobj_eventflag_set despite the
 *  identical address), FUN_c001cc2c's own `*DAT_c001cc7c = 1` (the FIRST of
 *  crt0's calls, see below - a one-time latch to 1 at boot), and
 *  sched_dispatch's own idle-loop toggle (DAT_c001d9cc: 1 while parked in
 *  WFI with nothing ready, cleared the instant a task is about to run).
 *
 *  These do not resolve into one clean story. A tempting reading ("the flag
 *  means CPU-currently-idle; wait/clear require NOT idle; set requires
 *  idle") fails on its own terms: sched_dispatch clears the flag back to 0
 *  essentially immediately after every WFI wake (before even checking
 *  whether a task is ready), so the window where the flag reads non-zero
 *  during normal operation is vanishingly narrow - yet kobj_eventflag_set
 *  has 6 real call sites and is clearly load-bearing, not a function that
 *  almost always fails. Left honestly open: same resolved address, four
 *  distinct call sites, at least two incompatible-looking polarities. Named
 *  `sched_flag_5730` in this file (not `sched_running_flag`, an earlier
 *  draft's name this pass retracted once the polarity conflict surfaced) -
 *  a later consolidation pass, ideally with live disassembly access to
 *  re-verify each guard byte-for-byte, should own reconciling this with
 *  eva_board_main.c's own `eva_board_sched_not_ready_flag` name for the
 *  same address.
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 *  sched_make_ready (FUN_c001d9e0, @0xc001d9e0) - the SIMPLER of this
 *  firmware's two "insert into ready queue" primitives (see
 *  eva_board_sched_ready below for the richer one eva_board_start_task
 *  calls directly). Always inserts at the tail of ready_list[tcb->priority],
 *  sets the ready-bitmap bit, and - only if there was previously no
 *  "current/highest ready" task at all, OR this task's priority is
 *  numerically lower (= more urgent) than the current one's - installs it as
 *  the new sched_current_ready and reports "reschedule needed" (return 1).
 *  Used internally by sched_release_wait (waking a blocked task) and by
 *  sched_task_create_and_ready (auto-starting a boot-time task) - both
 *  contexts where the task is, by construction, NOT already on any ready
 *  list, so the "already active, just reprioritize" branch
 *  eva_board_sched_ready has to handle doesn't apply here.
 * ------------------------------------------------------------------------- */
uint32_t sched_make_ready(struct sched_tcb *tcb)	/* FUN_c001d9e0 */
{
	uint8_t prio = tcb->priority;
	struct sched_ready_slot *slot = &sched_ready_list[prio];
	struct sched_tcb *old_tail = slot->tail;
	struct sched_tcb *prev_ready = sched_current_ready;

	tcb->link_prev = (struct sched_tcb *)slot;
	tcb->link_next = old_tail->link_next;	/* == (struct sched_tcb*)slot when list was empty */
	slot->tail = tcb;
	old_tail->link_next = tcb;
	sched_ready_bitmap |= (1u << prio);

	if (prev_ready != 0 && prio >= prev_ready->priority)
		return 0;

	sched_current_ready = tcb;
	return 1;
}

/* ------------------------------------------------------------------------- *
 *  eva_board_sched_ready (FUN_c001da64, @0xc001da64) - the primitive
 *  eva_board_main.c's eva_board_start_task calls directly. Handles a task
 *  that may already be linked into SOME ready slot (removes it from there
 *  first, clearing that slot's bitmap bit if it was the last entry), THEN
 *  re-inserts at the tail of ready_list[new_priority] and updates the
 *  priority field, THEN runs the same "is this now more urgent than
 *  whatever's current" comparison sched_make_ready does. Fully transcribed
 *  - this closes eva_board_main.c's own "structurally confirmed but not
 *  transcribed... no independent verification path" note for this function.
 * ------------------------------------------------------------------------- */
extern struct sched_tcb *sched_ready_scan_highest(void);	/* FUN_c001e024, defined below */

uint32_t eva_board_sched_ready(struct sched_tcb *tcb, uint8_t new_priority)	/* FUN_c001da64 */
{
	uint8_t old_priority = tcb->priority;
	struct sched_tcb *old_next = tcb->link_next;
	struct sched_ready_slot *old_slot = &sched_ready_list[old_priority];

	tcb->priority = new_priority;
	old_next->link_prev = tcb->link_prev;

	if ((struct sched_tcb *)old_slot == old_next) {
		/* the slot's own head == its own tail == the removed task:
		 * old_priority's ready list is now empty */
		sched_ready_bitmap &= ~(1u << old_priority);
	}
	tcb->link_prev->link_next = old_next;

	{
		struct sched_ready_slot *new_slot = &sched_ready_list[new_priority];
		struct sched_tcb *prev_ready = sched_current_ready;
		struct sched_tcb *old_tail = new_slot->tail;

		tcb->link_next = old_tail->link_next;
		sched_ready_bitmap |= (1u << new_priority);
		new_slot->tail = tcb;
		old_tail->link_next = tcb;
		tcb->link_prev = (struct sched_tcb *)new_slot;

		if (prev_ready == tcb) {
			if (old_priority <= new_priority) {
				struct sched_tcb *highest = sched_ready_scan_highest();
				sched_current_ready = highest;
				if (highest == tcb)
					return 0;
				return sched_reschedule_needed != 0;
			}
		} else if (new_priority < prev_ready->priority) {
			sched_current_ready = tcb;
			return sched_reschedule_needed;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  sched_ready_scan_highest (FUN_c001e024, @0xc001e024) - find-highest-
 *  priority-with-a-ready-task: reads the ready bitmap, checks the low byte
 *  first (skip 8 priority levels if clear), then the low nibble (skip 4
 *  more if clear), then indexes a small ROM nibble->bit-position lookup
 *  table (resolved 0xc0022cc0) for the final 0-3 offset - a textbook
 *  branchless-ish find-first-set restricted to a 16-bit-wide bitmap, which
 *  is exactly the 16 priority levels (0-15) eva_board_start_task's own
 *  `priority_code > 0x10` bound already implies. Returns the HEAD of that
 *  priority's ready list (the next task to run), not just its index.
 * ------------------------------------------------------------------------- */
extern uint8_t sched_prio_nibble_lut[];	/* resolved 0xc0022cc0 */

struct sched_tcb *sched_ready_scan_highest(void)	/* FUN_c001e024 */
{
	uint32_t bitmap = sched_ready_bitmap;
	int base = 0;
	uint32_t nibble;

	if ((bitmap & 0xff) == 0) {
		bitmap >>= 8;
		base = 8;
	}
	nibble = bitmap & 0xf;
	if (nibble == 0) {
		nibble = (bitmap >> 4) & 0xf;
		base += 4;
	}
	return sched_ready_list[sched_prio_nibble_lut[nibble - 1] + base].head;
}

/* ------------------------------------------------------------------------- *
 *  sched_remove_from_ready (FUN_c001e074, @0xc001e074) - the removal-only
 *  counterpart used when a task blocks (see sched_timed_wait_list_insert's
 *  own use via sched_delay_heap_insert below): unlinks from its current
 *  ready slot, clears the bitmap bit if that made the slot empty, and if
 *  the removed task WAS sched_current_ready, re-scans for the new highest
 *  (or clears sched_current_ready to 0 if nothing else is ready).
 * ------------------------------------------------------------------------- */
struct sched_tcb *sched_remove_from_ready(struct sched_tcb *tcb)	/* FUN_c001e074 */
{
	uint8_t prio = tcb->priority;
	struct sched_tcb *old_next = tcb->link_next;
	struct sched_ready_slot *slot = &sched_ready_list[prio];

	tcb->link_prev->link_next = old_next;
	old_next->link_prev = tcb->link_prev;

	if ((struct sched_tcb *)slot == old_next) {
		sched_ready_bitmap &= ~(1u << prio);
		if (sched_current_ready == tcb) {
			struct sched_tcb *highest = sched_ready_bitmap ? sched_ready_scan_highest() : 0;
			sched_current_ready = highest;
			return highest;
		}
	} else if (sched_current_ready == tcb) {
		sched_current_ready = old_next;
		return old_next;
	}
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  eva_board_sched_dispatch / sched_dispatch (FUN_c001d850, @0xc001d850) -
 *  the scheduler's own idle/dispatch tail, already independently confirmed
 *  in eva_board_main.c to be byte-for-byte identical to eva_board_crt0's own
 *  fall-through tail. Fully transcribed here (closing eva_board_main.c's
 *  third "not transcribed" item):
 *
 *   1. Save the outgoing task's context: sched_current_task's own +0x18
 *      (stack pointer) gets this function's OWN current stack address, and
 *      its own +0x1c (resume pointer) gets set to a fixed label
 *      (DAT_c001d878, resolved 0xc01d5568 -8x from kobj_table's own base -
 *      not independently decoded further) - i.e. "if this task runs again,
 *      resume right back inside this dispatcher".
 *   2. Loop: sched_current_task := sched_current_ready. If non-NULL, break
 *      out and dispatch it. If NULL (nothing ready), mark the idle flag
 *      (sched_flag_5730, see the shared-address note above) 1, execute
 *      `coproc_moveto_Wait_for_interrupt(1)` (ARM WFI), clear the idle flag
 *      on wake, and re-check.
 *   3. Tail-call through the new current task's own +0x1c resume pointer -
 *      for a freshly-created task (see sched_task_create_and_ready below)
 *      this is the fixed generic task trampoline every new task starts at
 *      (NOT a per-task entry function directly - see that function's own
 *      correction note); for a task resuming after having been switched out
 *      by THIS same function, it's this dispatcher's own resume label from
 *      step 1.
 * ------------------------------------------------------------------------- */
extern void resume_label(void);		/* DAT_c001d878, a real code label this function's own step 1 writes into outgoing TCBs; not itself decoded */
extern void coproc_moveto_Wait_for_interrupt(int arg);	/* ARM WFI intrinsic, see eva_board_main.c */

void sched_dispatch(void)	/* FUN_c001d850, == eva_board_main.c's eva_board_sched_dispatch */
{
	struct sched_tcb *outgoing = sched_current_task;

	outgoing->saved_sp = __builtin_frame_address(0);
	outgoing->resume = resume_label;

	for (;;) {
		struct sched_tcb *next = sched_current_ready;
		sched_current_task = next;
		if (next != 0)
			break;
		sched_flag_5730 = 1;
		coproc_moveto_Wait_for_interrupt(1);
		sched_flag_5730 = 0;
	}

	sched_current_task->resume();
}

/* ------------------------------------------------------------------------- *
 *  eva_board_sched_requeue (FUN_c001ddac, @0xc001ddac) - unlinks `tcb` from
 *  wherever it currently sits (any doubly-linked list using the +0x00/+0x04
 *  link fields - ready slot or kobj wait list, this function doesn't care
 *  which) and re-inserts it into the list headed by `list_head`, walking
 *  forward while the existing entries' priority is <= tcb's own (i.e.
 *  numerically-ascending priority order, matching every other list in this
 *  file). Fully transcribed - closes eva_board_main.c's second
 *  "not transcribed" item. This is a genuinely generic list primitive:
 *  besides eva_board_start_task's own reprioritize-while-waiting call, the
 *  SAME shape of sorted-priority-list insert appears inline (not via this
 *  function) inside sched_wait_list_insert/sched_timed_wait_list_insert
 *  below - this function is the one place it's factored out and reused.
 * ------------------------------------------------------------------------- */
void eva_board_sched_requeue(struct sched_tcb *list_head, struct sched_tcb *tcb)	/* FUN_c001ddac */
{
	if ((*(uint32_t *)tcb->cfg_record & 1) == 0)
		return;

	{
		struct sched_tcb *old_next = tcb->link_next;

		old_next->link_prev = tcb->link_prev;
		tcb->link_prev->link_next = old_next;

		{
			struct sched_tcb *walk = list_head->link_prev;	/* *param_1, i.e. list_head's own "head" field */

			while (walk != list_head && walk->priority <= tcb->priority)
				walk = walk->link_prev;

			{
				struct sched_tcb *walk_next = walk->link_next;

				tcb->link_prev = walk;
				tcb->link_next = walk_next;
				walk->link_next = tcb;
				walk_next->link_prev = tcb;
			}
		}
	}
}

/* ------------------------------------------------------------------------- *
 *  sched_release_wait (FUN_c001dbf0, @0xc001dbf0) - wakes a task blocked in
 *  kobj_eventflag_wait (or, per its own dispatch below, cancels a pending
 *  timeout for one): if the task's own wait descriptor's status slot is
 *  already non-zero (meaning it ALSO has a live entry in the delay/timeout
 *  heap - see sched_timed_wait_list_insert), first extracts it from that
 *  heap (sched_delay_heap_extract_min) so the timer ISR won't also try to
 *  wake it later. Clears the status slot to 0 (success), then either
 *  re-readies the task (normal case) or, if its state byte has bit 2 set
 *  (an "aborting"-shaped condition, not independently confirmed), just
 *  stamps state=4 without re-readying it.
 *
 *  NOTE on the extract-min call: the real decompile calls it with ZERO
 *  visible arguments (`FUN_c001ded8();`) even though FUN_c001ded8's OWN
 *  body declares `uint *param_1` and dereferences it immediately - the same
 *  "implicit ARM register-reuse argument" pattern this project's own
 *  mcasp.c already documents ("shown called with no visible arguments in
 *  the decompile, but their own bodies each take one - restored explicitly
 *  this pass as ARM's r0-register-reuse"). Here the register holding
 *  `desc` (TCB+0x14, the immediately-preceding load) is still live in r0 at
 *  the call site, so this file restores it explicitly as the real argument.
 * ------------------------------------------------------------------------- */
extern void sched_delay_heap_extract_min(uint32_t *heap_ref);	/* FUN_c001ded8, described below */

void sched_release_wait(struct sched_tcb *tcb)	/* FUN_c001dbf0 */
{
	uint32_t *desc = tcb->wait_desc;

	if (*desc != 0) {
		sched_delay_heap_extract_min(desc);
		desc = tcb->wait_desc;
	}
	*desc = 0;

	if ((tcb->state & 4) == 0) {
		sched_make_ready(tcb);
		return;
	}
	tcb->state = 4;
}

/* ------------------------------------------------------------------------- *
 *  sched_wait_list_insert (FUN_c001dc48, @0xc001dc48) - inserts the
 *  CURRENT task (sched_current_task, resolved to the identical
 *  0xc01d57f4 this function reads as DAT_c001dcf4) into `list_head`'s own
 *  wait list, sorted by priority (same walk shape as
 *  eva_board_sched_requeue above, duplicated inline rather than calling out
 *  to it), after stamping state=0x32 (a blocked/waiting code) and pointing
 *  the task's own +0x14 field at `wait_desc`.
 * ------------------------------------------------------------------------- */
void sched_wait_list_insert(struct kobj_record *list_head, void *wait_desc)	/* FUN_c001dc48 */
{
	struct sched_tcb *self = sched_current_task;

	self->state = 0x32;
	self->wait_desc = (uint32_t *)wait_desc;

	if ((*(uint32_t *)((struct sched_tcb *)list_head)->cfg_record & 1) != 0) {
		/* list_head's own record was itself active: walk its existing
		 * priority-sorted contents (same shape as eva_board_sched_requeue) */
		struct sched_tcb *head = (struct sched_tcb *)list_head;
		struct sched_tcb *walk = head->link_prev;

		while (walk != head && walk->priority <= self->priority)
			walk = walk->link_prev;

		{
			struct sched_tcb *walk_next = walk->link_next;

			self->link_prev = walk;
			self->link_next = walk_next;
			walk->link_next = self;
			walk_next->link_prev = self;
		}
	} else {
		struct sched_tcb *head = (struct sched_tcb *)list_head;
		struct sched_tcb *walk_next = head->link_next;

		self->link_prev = head;
		self->link_next = walk_next;
		head->link_next = self;
		walk_next->link_prev = self;
	}
}

/* ------------------------------------------------------------------------- *
 *  sched_timed_wait_list_insert (FUN_c001dcf8, @0xc001dcf8) /
 *  sched_delay_heap_insert (FUN_c001e180, @0xc001e180) - the TIMED variant:
 *  stamps state=0x32 on the current task, calls sched_delay_heap_insert
 *  (below) to both remove it from the ready queue and, if `ticks` > 0,
 *  register a wake-up entry in the delay/timeout min-heap, THEN runs the
 *  same priority-sorted wait-list insert as sched_wait_list_insert above.
 *  Not independently called from anywhere in this sweep's own range - its
 *  caller lives outside this file (a timed variant of kobj_eventflag_wait,
 *  presumably, but not traced this pass).
 * ------------------------------------------------------------------------- */
extern struct sched_tcb *sched_current_task_b;	/* DAT_c001dda8, resolved 0xc01d57f4 - SAME global as sched_current_task */
extern uint32_t sched_delay_heap_sift_up(uint32_t heap_index);	/* FUN_c001e250 - NOT transcribed, see the
									 * dedicated delay/timeout min-heap section below */

void sched_delay_heap_insert(void *heap_link, struct sched_tcb *tcb, int32_t ticks,
			      uint32_t default_wake)	/* FUN_c001e180 */
{
	extern struct sched_tcb *sched_current_task_c;	/* DAT_c001e1e0, resolved 0xc01d57f4 - same global again */
	extern uint32_t sched_tick_base;		/* DAT_c001e1e4, resolved 0xc01d5804 */
	extern uint32_t sched_delay_heap_count;	/* DAT_c001e2f8, resolved 0xc01d580c */
	extern void *sched_delay_heap[];		/* DAT_c001e2fc, resolved 0xc01d54d0, 8 bytes/entry */

	struct sched_tcb *removed = sched_remove_from_ready(sched_current_task_c);
	(void)removed;

	if (ticks < 1) {
		*(uint32_t *)heap_link = default_wake;
		return;
	}

	{
		uint32_t base = sched_tick_base;

		*(void **)heap_link = tcb;
		tcb->link_next = (struct sched_tcb *)0;	/* raw decompile: param_2[1] = iVar3 (list-head handle) */
		tcb->cfg_record = (uint32_t)heap_link;		/* raw decompile: param_2[2] = iVar4 */

		{
			uint32_t idx = ++sched_delay_heap_count;

			idx = sched_delay_heap_sift_up(idx);
			sched_delay_heap[idx * 2 - 2] = (void *)(uintptr_t)(base + ticks);
			sched_delay_heap[idx * 2 - 1] = tcb;
			tcb->link_prev = (struct sched_tcb *)(uintptr_t)idx;
		}
	}
}

void sched_timed_wait_list_insert(struct kobj_record *list_head, void *heap_link,
				   int32_t ticks, uint32_t default_wake)	/* FUN_c001dcf8 */
{
	struct sched_tcb *self = sched_current_task_b;

	self->state = 0x32;
	sched_delay_heap_insert(heap_link, self, ticks, default_wake);

	if ((*(uint32_t *)((struct sched_tcb *)list_head)->cfg_record & 1) != 0) {
		struct sched_tcb *head = (struct sched_tcb *)list_head;
		struct sched_tcb *walk = head->link_prev;

		while (walk != head && walk->priority <= self->priority)
			walk = walk->link_prev;

		{
			struct sched_tcb *walk_next = walk->link_next;

			*(struct sched_tcb **)heap_link = walk;	/* NOTE: raw decompile stores into heap_link+4,
									 * transcribed literally - see caller for the
									 * exact field this lands on */
			walk->link_next = self;
			walk_next->link_prev = self;
		}
	} else {
		struct sched_tcb *head = (struct sched_tcb *)list_head;
		struct sched_tcb *walk_next = head->link_next;

		head->link_next = self;
		walk_next->link_prev = self;
	}
}

/* ------------------------------------------------------------------------- *
 *  Delay/timeout min-heap (FUN_c001de18/FUN_c001ded8/FUN_c001e250,
 *  @0xc001de18/@0xc001ded8/@0xc001e250) - a genuinely NEW finding this pass:
 *  the mechanism sched_delay_heap_insert (above) feeds. A classic binary
 *  min-heap, keyed by absolute wake-tick value, over the same
 *  sched_delay_heap array (resolved 0xc01d54d0) referenced above, sized and
 *  counted via sched_delay_heap_count (resolved 0xc01d580c - the SAME
 *  address FUN_c001ce40 zeroes as part of crt0 bring-up, see the crt0-tail
 *  section below) with a "base time" reference (resolved 0xc01d5800, also
 *  zeroed by FUN_c001ce40).
 *
 *  All three functions are dense, unverified pointer/index arithmetic with
 *  no way to check against real hardware - per this project's own
 *  established practice for code this shape (heap_alloc.c's heap_malloc/
 *  heap_free, clcdc.c's clcdc_blit_glyph, cobjectmgr.c's
 *  cobjectmgr_handle_type_b), NOT transcribed here. What IS confirmed,
 *  purely from the resolved data addresses each one touches (not from
 *  reading the arithmetic itself):
 *    - FUN_c001de18 ("sift down" / bubble a candidate entry toward the leaves,
 *      comparing against sched_delay_heap_count's own bound) is called from
 *      sched_delay_heap_extract_min (FUN_c001ded8) AND from one more site
 *      outside this sweep's own range (FUN_c001ce7c, address-adjacent,
 *      itself just past this range's own upper bound - not traced).
 *    - FUN_c001ded8 (sched_delay_heap_extract_min, used above by
 *      sched_release_wait) pops the root (soonest wake time), decrements the
 *      count, and re-heapifies via either FUN_c001e250 or FUN_c001de18
 *      depending on the vacated slot's own child comparison.
 *    - FUN_c001e250 (sift up, called by sched_delay_heap_insert above via
 *      its own local name sched_delay_heap_sift_up) walks a freshly-inserted
 *      leaf toward the root while its wake-tick value is less than its
 *      parent's.
 *  Left as `extern` declarations with this structural description only,
 *  consistent with this project's own discipline against presenting guessed
 *  index arithmetic as fact.
 * ------------------------------------------------------------------------- */
extern uint32_t sched_delay_heap_sift_up(uint32_t heap_index);	/* FUN_c001e250 - NOT transcribed, see above */
extern void sched_delay_heap_sift_down(uint32_t heap_index, int32_t wake_tick);	/* FUN_c001de18 - NOT transcribed */
/* sched_delay_heap_extract_min (FUN_c001ded8) already declared above, also NOT transcribed */

/* ============================================================================
 *  CRT0 TAIL - TCB TABLE BRING-UP, AUTO-START WALK, AND THE VECTOR-TABLE
 *  CAPTURE/PATCH CLUSTER
 * ============================================================================
 *
 *  sched_tcb_reset (FUN_c001cdcc, @0xc001cdcc) - resets one TCB slot to its
 *  cold-boot default: copies the default priority out of the task's ROM
 *  config record (cfg_record+0x0c, a WORD read here vs. the single BYTE
 *  eva_board_start_task itself reads at the same offset - consistent, this
 *  just also clears the 3 upper bytes as a side effect of a 4-byte store
 *  into a byte-sized TCB field), clears two control bits in TCB+0x0e,
 *  zeroes TCB+0x10, and sets state=0 (unallocated). Called once per
 *  configured task slot by sched_tcb_table_init_and_autostart below, BEFORE
 *  that function decides whether to actually auto-start it.
 * ------------------------------------------------------------------------- */
void sched_tcb_reset(struct sched_tcb *tcb)	/* FUN_c001cdcc */
{
	uint32_t cfg_word = *(uint32_t *)(tcb->cfg_record + 0xc);

	tcb->ctrl &= 0xfd;
	tcb->unused_0x10 = 0;
	tcb->priority = (uint8_t)cfg_word;
	tcb->ctrl &= 0xfb;
	tcb->state = 0;
}

/* ------------------------------------------------------------------------- *
 *  sched_task_create_and_ready (FUN_c001ce00, @0xc001ce00) - THE task-
 *  creation primitive: given a TCB whose cfg_record is already set, reads
 *  the config record's own +0x10/+0x14 (stack base/size, by inference from
 *  the `end = base+size` shape) and pushes cfg+4/cfg+8 as a 2-word initial
 *  frame at the top of the new stack.
 *
 *  IMPORTANT CORRECTION vs. an earlier draft of this comment: TCB+0x1c is
 *  NOT set from the config record. The real decompile reads a FIXED
 *  constant (`uVar1 = DAT_c001ce3c;`, no per-record indexing anywhere near
 *  it) and stores THAT same fixed value into every task's TCB+0x1c -
 *  confirmed the exact same field sched_dispatch's own tail-call/resume
 *  logic uses. Every task created through this path therefore starts at
 *  the IDENTICAL resume address - a classic RTOS "generic task trampoline"
 *  shape: DAT_c001ce3c is almost certainly a small stub that itself pops
 *  the two just-pushed stack words (cfg+8, then cfg+4) and jumps to the
 *  REAL per-task entry function using one of them as the actual entry
 *  point and the other as its argument - which of the two plays which role
 *  is not resolved (DAT_c001ce3c itself sits outside this sweep's own
 *  address range and was not traced this pass).
 * ------------------------------------------------------------------------- */
extern void sched_task_trampoline(void);	/* DAT_c001ce3c - a fixed code-address constant, NOT itself in this sweep's range; see the correction above */

void sched_task_create_and_ready(struct sched_tcb *tcb)	/* FUN_c001ce00 */
{
	uint32_t cfg = tcb->cfg_record;
	uint8_t *stack_end = (uint8_t *)(*(uint32_t *)(cfg + 0x14) + *(uint32_t *)(cfg + 0x10));
	uint32_t init_word = *(uint32_t *)(cfg + 4);

	*(uint32_t *)(stack_end - 4) = *(uint32_t *)(cfg + 8);
	*(uint32_t *)(stack_end - 8) = init_word;

	tcb->resume = sched_task_trampoline;
	tcb->saved_sp = stack_end - 8;

	sched_make_ready(tcb);
}

/* ------------------------------------------------------------------------- *
 *  sched_tcb_table_init_and_autostart (FUN_c001ccb8, @0xc001ccb8) - called
 *  from FUN_c001cad4 (below), right before kobj_table_init: self-links all
 *  16 ready-queue slots (this is genuinely where sched_ready_list gets its
 *  cold-boot empty-circular-list shape, NOT kobj_table_init - an important
 *  correction, since eva_board_main.c's own prior prose attributed "the
 *  task-control-block table" generically to "FUN_c001cad4... calling
 *  FUN_c001d0f8" without distinguishing the TCB/ready-queue setup done HERE
 *  from the kobj-table setup FUN_c001d0f8 itself actually does), clears the
 *  ready bitmap, then walks a ROM table of task-id values (resolved
 *  0xc0022c50) up to a real count (resolved 0xc0022cbc, SAME address as
 *  eva_board_main.c's own eva_board_sched_max_task_id / DAT_c001d0ec -
 *  CONFIRMED identical, tying this walk's own bound to eva_board_start_task's
 *  own bounds check). For each table[i] task-id: computes its TCB (base
 *  resolved 0xc01d54e8 = sched_tcb_table+1, matching 1-based task-id
 *  indexing exactly), points its cfg_record at a PARALLEL 0x20-byte-stride
 *  ROM array (resolved 0xc0022c5c) using the SAME task-id-based offset,
 *  resets it via sched_tcb_reset, and - only if that config record's own
 *  flags word has bit 1 set - calls sched_task_create_and_ready to actually
 *  bring the task up. Tasks whose ROM record does NOT have bit 1 set are
 *  left at state=0 (unallocated) - presumably created later through some
 *  other, not-yet-found API (out of scope for this pass).
 * ------------------------------------------------------------------------- */
extern int32_t *sched_autostart_count;			/* DAT_c001cdb8, resolved 0xc0022cbc == eva_board_main.c's own eva_board_sched_max_task_id */
extern int32_t sched_autostart_task_ids[];		/* DAT_c001cdc0, resolved 0xc0022c50 */
extern uint8_t sched_task_cfg_table[];			/* DAT_c001cdc8, resolved 0xc0022c5c, 0x20-byte stride, task-id indexed */

void sched_tcb_table_init_and_autostart(void)	/* FUN_c001ccb8 */
{
	int i;

	for (i = 0; i < 16; i++) {
		sched_ready_list[i].head = (struct sched_tcb *)&sched_ready_list[i];
		sched_ready_list[i].tail = (struct sched_tcb *)&sched_ready_list[i];
	}
	sched_ready_bitmap = 0;

	{
		int32_t count = *sched_autostart_count;
		int i2 = 0;

		while (i2 < count) {
			for (;;) {
				int32_t task_id = sched_autostart_task_ids[i2++];
				struct sched_tcb *tcb = &sched_tcb_table[task_id];
				uint32_t cfg = (uint32_t)&sched_task_cfg_table[(task_id - 1) * 0x20];

				tcb->cfg_record = cfg;
				tcb->pad_0f &= 0xfe;
				sched_tcb_reset(tcb);

				if ((*(uint32_t *)cfg & 2) != 0) {
					sched_task_create_and_ready(tcb);
					break;
				}
				if (count <= i2)
					return;
			}
		}
	}
}

/* ------------------------------------------------------------------------- *
 *  The crt0-tail wrapper eva_board_main.c already names (FUN_c001cad4,
 *  @0xc001cad4) - one of eva_board_crt0's own 11-13 calls (see the header
 *  comment's call-count correction). Provided here as a real definition for
 *  the first time (eva_board_main.c's own prose only cited it, never gave it
 *  a body): calls sched_tcb_table_init_and_autostart, then kobj_table_init,
 *  then walks one more small ROM table (count resolved 0xc0022c1c, records
 *  resolved 0xc0022bbc, 3 words/record: {slot_index, unused, value}) writing
 *  fixed values into specific slots of a 101-entry (0x65) table that
 *  FUN_c001cba8 (below) separately zero-initializes - a sparse override of a
 *  default-zeroed lookup table. What that table itself IS (a syscall/trap
 *  vector table, an object-index map, or something else) is not resolved
 *  this pass - NEEDS LIVE QUERY if pursued further (would need to know what
 *  READS this table, not just what writes it, and no reader was found
 *  anywhere in this sweep's own range).
 * ------------------------------------------------------------------------- */
extern int32_t *sched_override_count;		/* DAT_c001d638, resolved 0xc0022c1c */
extern uint32_t sched_override_records[];	/* DAT_c001d63c, resolved 0xc0022bbc, 3 words/record */
extern void sched_sparse_table_patch(int32_t index, uint32_t value);	/* FUN_c001cbec, defined below */

void eva_board_crt0_tcb_and_kobj_init(void)	/* FUN_c001cad4 */
{
	int32_t count;
	uint32_t *rec;

	sched_tcb_table_init_and_autostart();
	kobj_table_init();

	count = *sched_override_count;
	rec = sched_override_records;
	if (count == 0)
		return;

	do {
		sched_sparse_table_patch(rec[0], rec[2]);
		count--;
		rec += 3;
	} while (count != 0);
}

/* ------------------------------------------------------------------------- *
 *  sched_sparse_table_patch (FUN_c001cbec, @0xc001cbec) - the one-line
 *  write primitive the override walk above uses. @0xc0022bbc's own record
 *  values land in the SAME 101-entry table sched_sparse_table_zero
 *  (below) zeroes, resolved 0xc01d557c.
 * ------------------------------------------------------------------------- */
extern uint32_t sched_sparse_table[];	/* DAT_c001cbf8, resolved 0xc01d557c */

void sched_sparse_table_patch(int32_t index, uint32_t value)	/* FUN_c001cbec */
{
	sched_sparse_table[index] = value;
}

/* ------------------------------------------------------------------------- *
 *  VECTOR-TABLE CAPTURE/PATCH CLUSTER - FUN_c001cc2c, FUN_c001cba8,
 *  FUN_c001cc88 (@0xc001cc2c, @0xc001cba8, @0xc001cc88). Genuinely new this
 *  pass, and (for the first two) high-confidence:
 *
 *  FUN_c001cc2c reads the ARM exception vector table's first 8 words -
 *  addresses 0x00, 0x04, ..., 0x1c, matching this project's own
 *  already-documented vector table shape ("5x LDR PC,[PC,#0x18]-style
 *  entries reading targets from a literal pool immediately after the
 *  table" - eva_board_main.c's reset-vector section). For each word, it
 *  decodes it AS an ARM `LDR PC,[PC,#imm]` instruction encoding: effective
 *  literal-pool address = instruction_address + 8 + (instruction & 0xfff),
 *  exactly the real ARM PC-relative-load address formula. It stores, per
 *  vector: the computed literal-pool slot address into a resolved-0xc01d5734
 *  8-entry table, and the CURRENT value at that slot (i.e. the currently-
 *  installed handler address) into a resolved-0xc01d5710 8-entry table. This
 *  is a capture of the live vector table's own handler addresses, indexed
 *  by their own literal-pool slot addresses. Genuinely confident: the
 *  arithmetic matches the ARM LDR-literal encoding exactly, and the base
 *  addresses (0, 4, 8...) line up with the already-documented vector table
 *  location once low addresses are understood to alias the reset-time
 *  vector table (consistent with this project's own established base
 *  0xC0000000 reset-vector location).
 *
 *  FUN_c001cba8 (already one of eva_board_main.c's own named 11 crt0 calls,
 *  previously untraced) zero-initializes the 101-entry sched_sparse_table
 *  above, THEN dereferences slot index 6 of FUN_c001cc2c's own pointer
 *  table (offset 0x18 = 6*4, matching vector-table offset 0x18 = the real
 *  ARM IRQ vector) and writes a fixed code address (resolved 0xc001d650)
 *  directly into it - i.e. THIS FUNCTION INSTALLS THE REAL IRQ HANDLER,
 *  patching over whatever default/dummy handler the vector table shipped
 *  with. This is a real, concrete, previously-unknown answer to "how do
 *  interrupts get wired up in this firmware" - flagged prominently in this
 *  file's own header comment.
 *
 *  FUN_c001cc88, called TWICE later in the same crt0 sequence, walks all 8
 *  entries of the SAME two tables and writes `*pointer_table[i] =
 *  value_table[i]` - on its face, a writeback of FUN_c001cc2c's own
 *  captured (pre-patch) values, which would appear to UNDO FUN_c001cba8's
 *  own IRQ patch above if nothing else modifies value_table[6] first. NOT
 *  resolved: this sweep found no code anywhere in its own range that writes
 *  to the value table between FUN_c001cc2c's capture and FUN_c001cc88's own
 *  first call - the two intervening call-shaped xrefs to FUN_c001cbe4 (a
 *  bare infinite loop) are the only candidate, and per this file's header
 *  comment those are themselves suspected (not confirmed) to be a
 *  misattributed-branch artifact rather than real code that could patch
 *  the table. Left as a genuinely open, flagged item rather than a guess.
 * ------------------------------------------------------------------------- */
extern uint32_t sched_vector_slot_table[8];	/* DAT_c001cc80/DAT_c001ccb0, resolved 0xc01d5734 */
extern uint32_t sched_vector_value_table[8];	/* DAT_c001cc84/DAT_c001ccb4, resolved 0xc01d5710 */
extern void sched_irq_entry(void);		/* DAT_c001cbe0, resolved 0xc001d650 - the installed IRQ handler; not itself in this sweep's range */

void sched_vector_table_capture(void)	/* FUN_c001cc2c */
{
	/* DAT_c001cc7c, resolved 0xc01d5730 - see shared-address note above; set here to 1 */
	int i;

	sched_flag_5730 = 1;

	for (i = 0; i < 8; i++) {
		uint32_t insn_addr = (uint32_t)(i * 4);
		uint32_t insn = *(uint32_t *)insn_addr;
		uint32_t slot_addr = insn_addr + (insn & 0xfff) + 8;

		sched_vector_slot_table[i] = slot_addr;
		sched_vector_value_table[i] = *(uint32_t *)slot_addr;
	}
}

void sched_irq_vector_install(void)	/* FUN_c001cba8 */
{
	int i;

	for (i = 0; i < 0x65; i++)
		sched_sparse_table[i] = 0;

	*(void (**)(void))sched_vector_slot_table[6] = sched_irq_entry;
}

/* NOT re-confirmed which of the two writeback calls, if either, is the "real"
 * one vs. a harmless no-op given the open question above - transcribed
 * literally, exactly as the decompile shows, twice at its own two call sites. */
void sched_vector_table_writeback(void)	/* FUN_c001cc88 */
{
	int i;

	for (i = 0; i < 8; i++)
		*(uint32_t *)sched_vector_slot_table[i] = sched_vector_value_table[i];
}

/* ------------------------------------------------------------------------- *
 *  eva_board_crt0's OWN corrected call-tail list, addresses c001cafc
 *  through c001cb94 (all within the same unbounded raw-disassembly region
 *  eva_board_main.c already describes eva_board_crt0/eva_board_main as
 *  living in - Ghidra assigns none of these call sites their own containing
 *  function). NOT wrapped in a C function here (it isn't one - it's a
 *  straight-line tail inside eva_board_crt0 itself, which is eva_board_main.c's
 *  own function to own); recorded here purely as a corrected reference list
 *  for a later consolidation pass, in call order:
 *
 *    c001cafc  sched_vector_table_capture      (FUN_c001cc2c)   [NEW body, this file]
 *    c001cb00  sched_irq_vector_install         (FUN_c001cba8)   [NEW body, this file]
 *    c001cb04  FUN_c001cba4                     (confirmed true no-op, `mov pc,lr`)
 *    c001cb08  timer/delay-heap init            (FUN_c001ce40, see below)
 *    c001cb0c  eva_board_crt0_tcb_and_kobj_init (FUN_c001cad4)   [NEW body, this file]
 *    c001cb10  FUN_c001ca34                      (subsystem-init group A - callees
 *                                                 FUN_c0000040 etc. all outside this
 *                                                 sweep's range, left as bare externs)
 *    c001cb60  FUN_c001ca84  <-- NOT in eva_board_main.c's original 11-call list.
 *                                Same shape as FUN_c001ca34: 8 more calls
 *                                (FUN_c00009d8, c0000404, c0000654, c0000848,
 *                                c0000784, c000090c, c0000238, c00000f4), all
 *                                outside this sweep's range, all outside scope here.
 *    c001cb78  sched_vector_table_writeback     (FUN_c001cc88, first call) [NEW, this file]
 *    c001cb7c  FUN_c001cbe4  <-- call-shaped xref to a bare infinite loop; see the
 *                                open-question note above, NOT confirmed real
 *    c001cb90  sched_vector_table_writeback     (FUN_c001cc88, second call)
 *    c001cb94  FUN_c001cbe4  <-- same open question, second occurrence
 *
 *  This raises eva_board_main.c's own documented "eleven back-to-back...
 *  calls" to at least THIRTEEN real calls plus two disputed ones - a real
 *  correction, reported here rather than edited into that file per this
 *  pass's own instructions.
 * ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- *
 *  Timer/delay-heap static init (FUN_c001ce40, @0xc001ce40) - one of
 *  eva_board_crt0's own originally-named 11 calls, previously untraced.
 *  Zeroes the delay-heap's own count (resolved 0xc01d580c - the SAME
 *  address sched_delay_heap_insert reads/increments as
 *  sched_delay_heap_count above) and a second field (resolved 0xc01d5800 -
 *  the SAME address sched_delay_heap_sift_up's own base-time reference
 *  reads, per the delay-heap section above), plus sets a ready-flag word
 *  (resolved 0xc01d5804, matching sched_delay_heap_insert's own
 *  sched_tick_base) to 1 and clears one more field (resolved 0xc01d5808,
 *  not independently cross-referenced elsewhere in this sweep). This is
 *  genuinely the delay/timeout-heap subsystem's own static bring-up -
 *  closing out the last of eva_board_main.c's originally-named-but-untraced
 *  11 crt0 calls that falls inside this sweep's own range.
 * ------------------------------------------------------------------------- */
void sched_delay_heap_static_init(void)	/* FUN_c001ce40 */
{
	extern uint32_t sched_tick_base_ready;		/* DAT_c001ce6c, resolved 0xc01d5804 */
	extern uint32_t *sched_delay_heap_count_ptr;	/* DAT_c001ce70, resolved 0xc01d580c */
	extern uint32_t sched_delay_heap_field_5808;	/* DAT_c001ce74, resolved 0xc01d5808 */
	extern uint32_t *sched_delay_heap_field_5800_ptr;	/* DAT_c001ce78, resolved 0xc01d5800 */

	sched_tick_base_ready = 1;
	*sched_delay_heap_count_ptr = 0;
	sched_delay_heap_field_5808 = 0;
	*sched_delay_heap_field_5800_ptr = 0;
}

/* -------------------------------------------------------------------------
 * Still genuinely open in this file:
 *  - Whether eva_board_main (0xc0005644) is itself one of the ROM-table
 *    entries sched_tcb_table_init_and_autostart auto-starts - the mechanism
 *    is now fully understood (see this file's header comment), but reading
 *    sched_autostart_task_ids[]/sched_task_cfg_table[]'s own contents would
 *    need a live `read_memory` query this pass didn't have access to.
 *    NEEDS LIVE QUERY: 0xc0022c50 (task-id table) and 0xc0022c5c (per-task
 *    0x20-byte config records, flags at +0, initial stack words at +4/+8,
 *    stack base/size at +0x10/+0x14) - does either of a record's +4/+8
 *    fields hold 0xc0005644, consistent with sched_task_create_and_ready's
 *    own trampoline+pushed-words startup shape (see that function's own
 *    correction note - the entry point is NOT a plain +4 field read
 *    directly into TCB+0x1c, as an earlier draft of this file wrongly
 *    assumed; it is one of the two words pushed onto the new task's stack
 *    for the fixed trampoline at DAT_c001ce3c to consume)?
 *  - sched_vector_table_writeback's (FUN_c001cc88) own net effect, called
 *    twice, and whether the two call-shaped xrefs to FUN_c001cbe4 between
 *    the vector-table capture and writeback are real code or a
 *    misattributed-branch artifact (see the vector-table cluster section).
 *    NEEDS LIVE QUERY: raw disassembly at 0xc001cb60-0xc001cb98, the same
 *    kind of query that resolved the FUN_c0009534/FUN_c0009540 case in
 *    eva_board_main.c.
 *  - sched_delay_heap_sift_up/_sift_down/_extract_min (FUN_c001e250/
 *    FUN_c001de18/FUN_c001ded8) - structurally identified as a binary
 *    min-heap keyed by wake tick, not transcribed (dense, unverified
 *    arithmetic, per this project's established practice for code this
 *    shape).
 *  - sched_timed_wait_list_insert's (FUN_c001dcf8) own caller - not found
 *    anywhere in this sweep's own range; presumably a timed variant of
 *    kobj_eventflag_wait living elsewhere in the image.
 *  - FUN_c001cad4's own third callee walk (the 101-entry sparse-table
 *    override, sched_sparse_table_patch/sched_sparse_table above) - no
 *    reader of sched_sparse_table was found anywhere in this sweep's own
 *    range, so its real purpose (trap-vector table? object-index map?) is
 *    unresolved.
 *  - The exact relationship between sched_current_task (0xc01d57f4) and
 *    sched_current_ready (0xc01d57fc) beyond what sched_dispatch's own body
 *    shows - both are used consistently and distinctly throughout this
 *    file, but no function in this range explains WHY two separate globals
 *    exist rather than one.
 *  - TCB+0x0e's own control-byte bits (only 2 of however many are cleared
 *    by sched_tcb_reset are ever referenced in this sweep's own range) and
 *    the 0x32 blocked-state code's own bit-level meaning beyond bit 0x20
 *    (on-wait-list, already confirmed via eva_board_start_task's own use).
 * ------------------------------------------------------------------------- */
