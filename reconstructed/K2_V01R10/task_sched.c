/* SPDX-License-Identifier: GPL-2.0 */
/*
 * task_sched.c - KRONOS2S_V01R10.VSB (Kronos 2) port of K1_V06R06/task_sched.c.
 * Same subsystem, and the headline result of this port is unambiguous:
 * K2 KEPT THE EXACT SAME RTOS-SHAPED TASK SCHEDULER ARCHITECTURE AS K1.
 * TCB table + priority ready-queue/bitmap dispatcher, the kernel-object
 * (event-flag-group) table and its set/clear/wait API, and the tick-ordered
 * delay/timeout min-heap those primitives block into are all present, at
 * new addresses, with (where checked field-by-field) IDENTICAL branch
 * shapes, error codes, and struct-offset conventions to K1.
 *
 * Ground truth: static Ghidra decompile dump of KRONOS2S_V01R10.VSB,
 * 2026-07-18 (query_dump_k2.py, not the live Ghidra MCP bridge).
 *
 * METHOD: unlike K1's own task_sched.c (which had to separate three
 * unrelated compiler-runtime/libc clusters out of one assigned address
 * sweep), this port started from a KNOWN GOOD anchor: K2's own
 * eva_board_main.c already independently confirmed (2026-07-18, concurrent
 * pass, NOT edited here) that eva_board_crt0's own tail (FUN_c0007268,
 * @0xc0007268) falls into "the byte-for-byte-identical scheduler idle/
 * dispatch tail... poll a next-ready-task global, WFI when none ready,
 * indirect call through the ready task's own +0x1c function pointer" but
 * left it "not transcribed further... consistent with K1's own treatment of
 * this dense scheduler-primitive code" and explicitly flagged
 * eva_board_start_task's own K2 fate as UNRESOLVED ("no confirmed K2
 * counterpart at all"). This file resolves both of those open items for
 * real: the WFI/dispatch tail's own globals (sched_current_task,
 * sched_current_ready, the shared idle flag) were extracted directly from
 * FUN_c0007268's own decompiled body, then a full-image sweep for every
 * OTHER K2 function referencing those same three resolved data addresses
 * (not just name-matching DAT_ tokens, which are per-function-local in this
 * dump - resolving each one's real numeric address first) found the entire
 * rest of the cluster in one pass, address-adjacent, at 0xc0019b34-0xc001ab90.
 *
 * HEADLINE FINDING #2, closes eva_board_main.c's own open question: the
 * real, separately-callable sched_dispatch (K1: FUN_c001d850) IS present in
 * K2, at FUN_c001a420 (@0xc001a420) - a DIFFERENT function from crt0's own
 * inlined tail (FUN_c0007268), confirmed by direct comparison: FUN_c001a420
 * has an extra "save outgoing task's context" header (stash the current
 * stack pointer and a resume label into the outgoing TCB) that crt0's own
 * copy does NOT have - a genuine, confirmed difference (not an artifact):
 * on cold boot there is no real "outgoing" task yet, so crt0's own inlined
 * tail skips straight to the ready-scan/WFI loop, while the general-purpose
 * sched_dispatch (called from kobj_eventflag_wait below, mid-execution,
 * where there ALWAYS is a real outgoing task) does the full save-then-
 * switch sequence. K1's own task_sched.c asserted these two tails were
 * "byte-for-byte identical" without this specific distinction - this K2 port
 * is more precise about it, and the same distinction likely also holds for
 * K1 (not re-verified against K1's own dump this pass).
 *
 * PORTING METHOD / CONFIDENCE: every function below was matched to its K1
 * counterpart by decompiled-text structural comparison (address literals
 * masked) PLUS independent numeric resolution of its own key DAT_ globals
 * (not assumed to carry over addresses from K1). Every function came back
 * either byte-for-byte identical in branch shape, or with a specific,
 * flagged, real difference (see kobj_eventflag_wait/sched_wait_list_insert's
 * own notes below) - never smoothed over.
 *
 * ADDRESS MAP (K1 -> K2):
 *   kobj_table_init                    FUN_c001d0f8 -> FUN_c0019ee0
 *   kobj_eventflag_test_and_clear      FUN_c001dfc8 -> FUN_c001a980
 *   kobj_eventflag_set                 FUN_c001d22c -> FUN_c0019f30
 *   kobj_eventflag_clear               FUN_c001d318 -> FUN_c001a01c
 *   kobj_eventflag_wait                FUN_c001d3a8 -> FUN_c001a0ac
 *   sched_make_ready                   FUN_c001d9e0 -> FUN_c001a5b0
 *   sched_remove_from_ready            FUN_c001e074 -> FUN_c001a9dc
 *   sched_ready_scan_highest           FUN_c001e024 -> FUN_c001ab4c
 *   sched_dispatch                     FUN_c001d850 -> FUN_c001a420
 *   sched_release_wait                 FUN_c001dbf0 -> FUN_c001a6c8
 *   sched_wait_list_insert             FUN_c001dc48 -> FUN_c001a720, REAL DIFFERENCE, see below
 *   sched_delay_heap_extract_min       FUN_c001ded8 -> FUN_c001a890 (not transcribed, same as K1)
 *   sched_delay_heap_sift_down         FUN_c001de18 -> FUN_c001aae8 (address only, live pass, not transcribed)
 *   sched_delay_heap_sift_up           FUN_c001e250 -> FUN_c001a7d0 (address only, live pass, not transcribed)
 *   sched_tcb_reset                    (not named in K1) -> FUN_c0019cd4
 *   sched_task_create_and_ready        FUN_c001ce00 (K1's own citation) -> FUN_c0019d08
 *   sched_tcb_table_init_and_autostart (K1's own citation, unnamed FUN_)   -> FUN_c0019bc0
 *   eva_board_crt0's inlined dispatch tail (K1: FUN_c00055b8's own tail)   -> FUN_c0007268 (eva_board_main.c)
 *   (interrupt-vector-table capture, K1's FUN_c001cc2c)                   -> FUN_c0019b34, see below
 *
 * NOT ported/re-resolved this pass (matching K1's own explicit exclusions,
 * still true in K2): eva_board_sched_ready (K1's own "already active,
 * reprioritize" richer ready-insert, FUN_c001da64) has NO independently
 * confirmed K2 counterpart in this cluster - consistent with, and likely the
 * direct explanation for, eva_board_main.c's own "eva_board_start_task has
 * no confirmed K2 counterpart" finding: if K2 dropped the priority-
 * reprioritize entry point, `eva_board_start_task`-shaped calls may simply
 * not exist in K2's own crt0 sequence at all. eva_board_sched_requeue (K1's
 * own generic list-requeue helper, FUN_c001ddac) likewise has no confirmed
 * K2 mapping found this pass. The delay-heap sift-up/sift-down internals
 * (K1's FUN_c001de18/FUN_c001e250) were not individually searched for in K2
 * - only sched_delay_heap_extract_min's own address was confirmed, via
 * kobj_eventflag_wait's/sched_release_wait's own call graph. Compiler-
 * runtime clusters (A)/(B)/(C) that K1's own file had to explicitly exclude
 * from ITS OWN address sweep were never encountered here at all, since this
 * port started from confirmed call-graph evidence rather than a raw address
 * range - nothing to exclude.
 *
 * ============================================================================
 *  2026-07-19 LIVE GHIDRA FOLLOW-UP (read-only MCP bridge against
 *  kronos2s_v01r10_panel.elf - get_disassembly/decompile_function/
 *  get_xrefs_to/read_memory, no load_binary, no mutating calls, zero
 *  Agent-tool subagent calls, per this task's own 2-agent-cap authorization)
 * ============================================================================
 *
 * HEADLINE FINDING, RESOLVES THIS FILE'S OWN TOP PRIORITY OPEN ITEM: whether
 * eva_board_main is itself an auto-started ROM-table entry.
 *
 * read_memory on sched_tcb_table_init_and_autostart's own resolved literals
 * (count @0xC002A6F8, task-id array @0xC002A68C, cfg-record array @0xC002A698,
 * 0x20-byte stride) dumped the ROM table byte-exact. It holds EXACTLY 3 tasks
 * (ids 1/2/3, all with cfg+0 flags bit1 set - all three genuinely autostart),
 * with priorities (cfg+0xc) 0/2/4 respectively (lower = more urgent, per
 * sched_make_ready above). decompile_function on sched_task_create_and_ready
 * (FUN_c0019d08) FIRST corrected a real transcription bug in this port's own
 * prior draft (see the function's own fix note below), definitively settling
 * which of a task's two ROM-pushed stack words is the real jump target: it is
 * cfg+8 (loaded last, popped into PC by the generic trampoline), NOT cfg+4
 * (popped first, into R0/the argument register) - the exact ambiguity K1's
 * own file left open ("which of the two plays which role is not resolved").
 * Reading each task's own cfg+8 field and manually disassembling
 * (get_disassembly kept returning "no output" for this exact address range,
 * matching every one of this project's prior "unbounded region" tool-flake
 * reports - fell back to raw read_memory + hand ARM decode, cross-checked
 * against every already-confirmed literal/call target in eva_board_main.c's
 * own header):
 *
 *   - Task id=1, priority 0 (most urgent), entry 0xC00072C0: this IS
 *     eva_board_init_table's own ROM-table walk-and-dispatch loop - confirmed
 *     by decoding its own tail (`mov lr,pc; mov pc,r3`, a computed dispatch
 *     through r3) at 0xC00072EC, the EXACT address eva_board_main.c's own
 *     header already cites as "a COMPUTED_CALL... this is eva_board_init_
 *     table's own dispatch instruction, CONFIRMED by xref type and target."
 *     Falling out of that loop (r4>=r5 exit), this SAME task then loads
 *     eva_board_handle (literal 0xC01CB2EC) and calls eva_board_final_setup
 *     (BL @0xC00072F8 -> 0xC0009838, exactly eva_board_main.c's own second
 *     confirmed call site) - and THEN, rather than returning or falling
 *     through to a third call, hand-decodes to a genuine 2-instruction
 *     INFINITE LOOP: reload handle, `bl 0xC000A58C` (eva_board_boot_status_
 *     dispatch, eva_board_main.c's own third confirmed call site, @0xC0007300),
 *     `b 0xC00072FC` (branches back to the reload, not forward). This task
 *     never returns from this point on. It is the task that actually blocks:
 *     kobj_eventflag_wait's own note above already independently found
 *     FUN_c000a58c calling into this file's real scheduler primitive - THAT
 *     block is what yields the CPU to task id=2 below.
 *   - Task id=2, priority 2, entry 0xC0007314: a short, separately-prologued
 *     stub (`mov r12,sp; stmdb sp!,{fp,ip,lr,pc}; sub fp,ip,#4`) that loads
 *     eva_board_handle fresh from the SAME 0xC01CB2EC literal (not from the
 *     R0 the trampoline would have restored) and `bl`s straight into
 *     eva_board_main_loop (0xC000A6DC) - eva_board_main.c's own FOURTH
 *     confirmed call site (@0xC0007324), already independently decompiled
 *     there as `do { master_dispatch_tick(handle); } while(true);`.
 *   - Task id=3, priority 4 (least urgent), entry 0xC0007330: `mov r0,#0;
 *     ldr r1,[pc,#8]->0xC002B218; mov r2,#0x70; b 0xC000A730
 *     (crypto_at88_fault)` - an IMMEDIATE, unconditional fault call, no wait
 *     loop of any kind precedes it. read_memory on the r1 literal
 *     (0xC002B218) independently RESOLVES eva_board_main.c's own long-
 *     standing open item: that address is exactly the one eva_board_main.c's
 *     header already names as "../EvaBoardMain.cpp"'s string address, whose
 *     "only K1 xref sits inside eva_board_watchdog_fault_wrapper, which in K2
 *     could not be independently located." FOUND (cross-file finding,
 *     reported here rather than edited into eva_board_main.c, per that
 *     file's own instructions and this project's established convention):
 *     it is this ROM-table task, ID 3, priority 4. Because it is the LOWEST-
 *     priority of the 3 autostarted tasks and task id=2's own main-loop body
 *     has no wait/block call anywhere in it, task 3 is structurally
 *     UNREACHABLE under normal operation (sched_make_ready never installs a
 *     lower-urgency task over an already-ready higher one, and the highest-
 *     priority ready task is only ever replaced by dispatch, not preemption,
 *     in this cooperative model) - consistent with a "should never actually
 *     run" defensive/reserved slot whose sole purpose is to hard-fault loudly
 *     if the scheduler's own invariants are ever violated.
 *
 * NET CONCLUSION: "eva_board_main" as K1 knows it does NOT exist as one
 * function in K2 at all. eva_board_main.c's own reconstructed single-body
 * "eva_board_main" (calling init_table, final_setup, boot_status_dispatch,
 * then main_loop in one straight-line sequence) is a real, CONFIRMED
 * mis-assembly of independent evidence: those 4 call-site addresses were
 * derived purely from get_xrefs_to on each callee (never from one linear
 * disassembly), and this port's own manual decode of the actual bytes between
 * them proves boot_status_dispatch is called in an infinite 2-instruction
 * loop that never falls through to the main_loop call - the two live in
 * SEPARATE, independently ROM-table-autostarted tasks (id=1 and id=2 above),
 * dispatched by exactly the priority-ready-queue/WFI mechanism this file
 * already documents, not by sequential fall-through. Not edited into
 * eva_board_main.c per this pass's own file-scope boundaries - reported here
 * for a future consolidation pass, per this file's own existing practice.
 *
 * Task cfg+4 ("arg", values 0/1/1 for ids 1/2/3) does not appear to be
 * consumed by any of the 3 entry stubs above - task 2 reloads its handle from
 * a literal instead of using the R0 the trampoline would have restored, and
 * task 3 immediately overwrites R0 with 0. Flagged honestly as apparently
 * vestigial for this specific cohort of tasks, not forced into an
 * explanation.
 *
 * SECOND RESOLVED ITEM: the delay-heap sift internals. decompile_function on
 * sched_delay_heap_extract_min (FUN_c001a890) shows it calling FUN_c001aae8
 * with two arguments (half-index, wake-tick) on the "child is more urgent"
 * path and FUN_c001a7d0 with one argument (index) otherwise - matching K1's
 * own sift-down (2-arg, FUN_c001de18) / sift-up (1-arg, FUN_c001e250)
 * signatures exactly. FUN_c001aae8 = sched_delay_heap_sift_down (K1:
 * FUN_c001de18), FUN_c001a7d0 = sched_delay_heap_sift_up (K1: FUN_c001e250).
 * Neither was independently re-transcribed as C this pass (dense index
 * arithmetic, same treatment this project gives code this shape) but their
 * real K2 addresses are now confirmed via direct call-graph evidence, not
 * guessed.
 *
 * THIRD ITEM, PARTIALLY RESOLVED: get_xrefs_to on sched_remove_from_ready
 * (FUN_c001a9dc) live returns exactly 2 callers - sched_wait_list_insert
 * (already known) and a second, real call at 0xC001AAAC inside an unbounded
 * region (get_function_info/decompile_function both fail there, same
 * artifact as eva_board_main.c's own crt0 tail - manually disassembled
 * instead). That function (starts ~0xC001AA98, standard `mov r12,sp; stmdb
 * sp!,{r4,fp,ip,lr,pc}` prologue) calls `sched_remove_from_ready(*sched_
 * current_task)` at 0xC001AAAC, THEN `sched_tcb_reset(*sched_current_task)`
 * at 0xC001AAB4 - this SECOND call is the exact address K1's own file cited
 * as sched_tcb_reset's "one more site near sched_remove_from_ready's own
 * neighborhood (0xc001aab4, not traced)" - RESOLVED: it removes the
 * currently-running task from the ready queue and immediately resets its TCB
 * (state/priority/control bits), then branches on the task's own ctrl bit 0
 * (not decoded further - out of this pass's time budget). This does NOT
 * match either eva_board_sched_ready's or eva_board_sched_requeue's own K1
 * shape (neither resets a TCB's control bits) - looks more like a
 * self-terminate/exit primitive. NOT claimed as either named K1 function;
 * eva_board_sched_ready/eva_board_sched_requeue remain genuinely
 * unlocated in K2 - see updated "still open" section below.
 *
 * FOURTH ITEM: cobjectmgr.c's own cobjectmgr_hardware_fault_watchdog search
 * is independently re-confirmed from this file's own side. The kobj_
 * eventflag_clear "ack" primitive (this file's own FUN_c001a01c) was
 * re-queried live via get_xrefs_to: still exactly ONE caller (FUN_c000a58c,
 * the same master-dispatcher analogue the static-dump pass already found) -
 * matching the static dump's own result exactly, not just similar. Separately,
 * the full 3-entry ROM autostart table dumped for the finding above
 * accounts for ALL of K2's autostarted tasks (kobj_count/sched_autostart_
 * count both independently read as exactly 3) and none of the three matches
 * cobjectmgr_hardware_fault_watchdog's K1 shape (all three are now positively
 * identified as something else, see above) - a real, data-backed NEGATIVE
 * result ruling out "it's a 4th ROM-table autostart entry this project
 * hadn't looked at yet," not just an unswept gap.
 */

#include <stdint.h>

/* struct sched_tcb / struct kobj_record - IDENTICAL field layout to K1's own
 * task_sched.c (link_prev/link_next at +0x00/+0x04, cfg_record at +0x08,
 * state/priority/ctrl bytes at +0x0c/+0x0d/+0x0e, wait_desc at +0x14,
 * saved_sp/resume at +0x18/+0x1c for the TCB; wait_prev/wait_next/
 * src_record/flags at +0x00/+0x04/+0x08/+0x0c for the kobj record) -
 * re-confirmed field-by-field against K2's own decompiled offsets in every
 * function below (sched_make_ready's `param_1+3` == state at +0xc byte
 * offset, `*(byte*)(param_1+0xd)` == priority, etc.), not assumed to carry
 * over from K1 unchanged.
 */
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

struct kobj_record {
	struct kobj_record *wait_prev;		/* +0x00 */
	struct kobj_record *wait_next;		/* +0x04 */
	uint32_t src_record;			/* +0x08 */
	uint32_t flags;			/* +0x0c */
};

/* ============================================================================
 *  KERNEL-OBJECT (EVENT-FLAG-GROUP) TABLE
 * ============================================================================
 *
 *  Resolved addresses (independently re-derived from K2's own dump, NOT
 *  copied from K1): kobj_count @ 0xC002A684, kobj_table @ 0xC01D4F1C (kobj
 *  ROM source table @ 0xC002A66C). Same "kobj_table's own base is exactly
 *  ONE record (0x10 bytes) behind kobj_table_init's own destination base"
 *  relationship K1 confirmed: kobj_table_init writes starting at
 *  0xC01D4F2C == 0xC01D4F1C + 0x10 - independently re-verified here via
 *  K2's own resolved literal-pool values, not assumed.
 * ------------------------------------------------------------------------- */
extern int32_t *kobj_count;			/* 0xC002A684 */
extern uint32_t kobj_src_table;		/* 0xC002A66C, 8-byte-stride ROM source */
extern struct kobj_record kobj_table[];	/* 0xC01D4F1C, 1-based indexing */

/* ------------------------------------------------------------------------- *
 *  kobj_table_init - K2 @0xc0019ee0 (K1 @0xc001d0f8). CONFIRMED structurally
 *  identical to K1: count-driven loop over kobj_count, populating each
 *  0x10-byte record's src_record/flags from an 8-byte-stride ROM source and
 *  self-linking wait_prev/wait_next. Real K2 caller: FUN_c00199dc, one of
 *  eva_board_crt0's own subsystem-bring-up calls (see eva_board_main.c's own
 *  citation of FUN_c00199dc as one of the eleven, previously untraced).
 * ------------------------------------------------------------------------- */
void kobj_table_init(void)	/* FUN_c0019ee0 */
{
	extern uint32_t kobj_src_table_words[];
	int32_t count = *kobj_count;
	uint32_t src = (uint32_t)kobj_src_table_words;
	struct kobj_record *dst = &kobj_table[1];

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
 *  kobj_eventflag_test_and_clear - K2 @0xc001a980 (K1 @0xc001dfc8).
 *  CONFIRMED structurally identical: AND (mode bit0==0) or OR (bit0==1)
 *  match test, matched flags written to *out_flags, kobj flags cleared to 0
 *  only if the matched waiter's own src_record has bit 2 (auto-clear) set.
 * ------------------------------------------------------------------------- */
uint32_t kobj_eventflag_test_and_clear(struct kobj_record *kobj, uint32_t mask,
					uint32_t mode, uint32_t *out_flags)
	/* FUN_c001a980 */
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
 *  kobj_eventflag_set - K2 @0xc0019f30 (K1 @0xc001d22c). CONFIRMED
 *  structurally identical to K1, including the EXACT SAME return codes
 *  (0xffffffe7 = -25 guard failure, 0xffffffee = -18 handle-bound failure) -
 *  re-verified numerically from K2's own decompile, not assumed. Guard
 *  requires sched_flag_a (see below) NON-ZERO.
 * ------------------------------------------------------------------------- */
extern uint8_t sched_flag_a;		/* 0xC01D5114 - shared with sched_dispatch's own idle toggle, see below */
extern void sched_release_wait(struct sched_tcb *tcb);	/* FUN_c001a6c8, defined below */

int32_t kobj_eventflag_set(int32_t handle, uint32_t mask)	/* FUN_c0019f30 */
{
	if (sched_flag_a == 0)
		return -25;

	if (handle < 1 || handle > *kobj_count)
		return -18;

	{
		struct kobj_record *kobj = &kobj_table[handle];
		struct sched_tcb *waiter = (struct sched_tcb *)kobj->wait_prev;

		kobj->flags |= mask;

		if ((struct kobj_record *)waiter != kobj) {
			uint32_t *desc = waiter->wait_desc;

			if (kobj_eventflag_test_and_clear(kobj, desc[2], desc[3], &desc[4])) {
				struct sched_tcb *next = waiter->link_next;

				next->link_prev = waiter->link_prev;
				waiter->link_prev->link_next = next;

				sched_release_wait(waiter);
			}
		}
	}
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  kobj_eventflag_clear - K2 @0xc001a01c (K1 @0xc001d318). CONFIRMED
 *  structurally identical, INCLUDING THE OPPOSITE GUARD POLARITY from
 *  kobj_eventflag_set (requires sched_flag_a ZERO) despite sharing the
 *  identical resolved address - same real, unreconciled asymmetry K1's own
 *  file documented and deliberately left open. ANDs `keep_mask` into the
 *  kobj's flags word (caller passes bits to KEEP, confirmed by the raw `&=`,
 *  no complement).
 * ------------------------------------------------------------------------- */
int32_t kobj_eventflag_clear(int32_t handle, uint32_t keep_mask)	/* FUN_c001a01c */
{
	if (sched_flag_a != 0)
		return -25;

	if (handle < 1 || handle > *kobj_count)
		return -18;

	kobj_table[handle].flags &= keep_mask;
	return 0;
}

/* ------------------------------------------------------------------------- *
 *  kobj_eventflag_wait - K2 @0xc001a0ac (K1 @0xc001d3a8). CONFIRMED
 *  structurally identical to K1's own function: same 4-word-plus-output
 *  wait descriptor on the caller's own stack frame ([0]=status, [1]=pad,
 *  [2]=mask, [3]=mode, trailing out_flags), same single-waiter-only kobj
 *  list check (returns -0x1c/-28 if already occupied), same immediate-
 *  satisfaction fast path, same blocking path: sched_wait_list_insert then
 *  sched_dispatch. Guard requires sched_flag_a ZERO (matching
 *  kobj_eventflag_clear's own polarity) AND sched_flag_b (below) NON-ZERO -
 *  identical dual-guard shape to K1's own kobj_eventflag_wait.
 *
 *  Real caller: FUN_c000a58c - independently already documented by
 *  eva_board_main.c's own K2 header as "eva_board_boot_status_dispatch...
 *  structurally resembles master_dispatch_tick" (a hardware-status-bit
 *  fan-out). This is a genuine, useful cross-file finding this port reports
 *  back rather than edits in: that function is NOT purely a
 *  master_dispatch_tick lookalike - it also directly calls into this real
 *  scheduler primitive at least once. NOT edited into eva_board_main.c,
 *  per this pass's own file-scope boundaries.
 * ------------------------------------------------------------------------- */
extern uint32_t sched_flag_b;		/* 0xC01D51DC */
extern void sched_wait_list_insert(struct kobj_record *list_head, void *wait_desc);	/* FUN_c001a720, defined below */
extern void sched_dispatch(void);							/* FUN_c001a420, defined below */

int32_t kobj_eventflag_wait(int32_t handle, uint32_t mask, uint32_t mode, uint32_t *out_flags)
	/* FUN_c001a0ac */
{
	int32_t status = 0;

	if (sched_flag_a != 0 || sched_flag_b == 0)
		return -25;
	if (handle < 1 || handle > *kobj_count)
		return -18;
	if (mask == 0 || (mode & 0xfffffffeu) != 0)
		return -17;

	{
		struct kobj_record *kobj = &kobj_table[handle];

		if (kobj->wait_prev != kobj)
			return -28;

		if (kobj_eventflag_test_and_clear(kobj, mask, mode, out_flags))
			return 0;

		{
			uint32_t wait_desc[5];

			wait_desc[2] = mask;
			wait_desc[3] = mode;

			sched_wait_list_insert(kobj, wait_desc);
			sched_dispatch();

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
 *  Resolved addresses: sched_current_task @ 0xC01D51D8, sched_current_ready
 *  @ 0xC01D51E0, ready_list (16 slots, 8 bytes each) @ 0xC01D5154, ready
 *  bitmap @ 0xC01D51D4, sched_flag_a @ 0xC01D5114 - CONFIRMED, via direct
 *  literal-pool resolution, to be the SAME address kobj_eventflag_set's own
 *  guard and kobj_eventflag_wait's own idle-toggle both reference (same
 *  cross-function sharing K1 documented for its own sched_flag_5730),
 *  independently re-derived here rather than assumed to carry over.
 * ------------------------------------------------------------------------- */
extern struct sched_tcb *sched_current_task;		/* 0xC01D51D8 */
extern struct sched_tcb *sched_current_ready;		/* 0xC01D51E0 */
extern struct sched_ready_slot sched_ready_list[16];	/* 0xC01D5154 */
extern uint32_t sched_ready_bitmap;			/* 0xC01D51D4 */

/* ------------------------------------------------------------------------- *
 *  sched_make_ready - K2 @0xc001a5b0 (K1 @0xc001d9e0). CONFIRMED
 *  structurally identical: tail-insert into ready_list[tcb->priority], set
 *  the bitmap bit, stamp state bit0 = 1 ("made ready"), and install as the
 *  new sched_current_ready + return 1 only if there was no current-ready
 *  task at all, or this one is more urgent (numerically lower priority).
 *  2 confirmed callers: sched_release_wait (below) and
 *  sched_task_create_and_ready (below) - same two logical roles K1 documents
 *  (waking a blocked task; auto-starting a boot-time task).
 * ------------------------------------------------------------------------- */
uint32_t sched_make_ready(struct sched_tcb *tcb)	/* FUN_c001a5b0 */
{
	uint8_t prio = tcb->priority;
	struct sched_ready_slot *slot = &sched_ready_list[prio];
	struct sched_tcb *old_tail = slot->tail;
	struct sched_tcb *prev_ready = sched_current_ready;

	tcb->state = 1;
	tcb->link_prev = (struct sched_tcb *)slot;
	tcb->link_next = old_tail->link_next;
	slot->tail = tcb;
	old_tail->link_next = tcb;
	sched_ready_bitmap |= (1u << prio);

	if (prev_ready != 0 && prio >= prev_ready->priority)
		return 0;

	sched_current_ready = tcb;
	return 1;
}

/* ------------------------------------------------------------------------- *
 *  sched_ready_scan_highest - K2 @0xc001ab4c (K1 @0xc001e024). CONFIRMED
 *  structurally identical: byte-then-nibble bitmap scan plus a ROM
 *  nibble->bit-position lookup table, returning the HEAD of that priority's
 *  ready list.
 * ------------------------------------------------------------------------- */
extern uint8_t sched_prio_nibble_lut[];	/* resolved via DAT_c001ab94 */

struct sched_tcb *sched_ready_scan_highest(void)	/* FUN_c001ab4c */
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
 *  sched_remove_from_ready - K2 @0xc001a9dc (K1 @0xc001e074). CONFIRMED
 *  structurally identical: unlinks from the current ready slot, clears the
 *  bitmap bit if that emptied the slot, rescans for the new highest (or
 *  clears sched_current_ready to 0) if the removed task WAS
 *  sched_current_ready.
 * ------------------------------------------------------------------------- */
struct sched_tcb *sched_remove_from_ready(struct sched_tcb *tcb)	/* FUN_c001a9dc */
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
 *  sched_dispatch - K2 @0xc001a420 (K1 @0xc001d850). See this file's own
 *  HEADLINE FINDING #2 above: CONFIRMED to be a real, separately-callable
 *  function distinct from eva_board_crt0's own inlined copy of the same
 *  ready-scan/WFI tail (FUN_c0007268, eva_board_main.c) - this version has
 *  the additional "save outgoing task's context" header crt0's own copy
 *  omits. Sole caller: kobj_eventflag_wait above.
 * ------------------------------------------------------------------------- */
extern void coproc_moveto_Wait_for_interrupt(int arg);	/* ARM WFI intrinsic, see eva_board_main.c */

void sched_dispatch(void)	/* FUN_c001a420 */
{
	struct sched_tcb *outgoing = sched_current_task;

	outgoing->saved_sp = __builtin_frame_address(0);
	outgoing->resume = (void (*)(void))0xc001a448;	/* resume_label - a real code label this function's own header writes into outgoing TCBs; not itself decoded, same treatment as K1's own resume_label */

	for (;;) {
		struct sched_tcb *next = sched_current_ready;
		sched_current_task = next;
		if (next != 0)
			break;
		sched_flag_a = 1;
		coproc_moveto_Wait_for_interrupt(1);
		sched_flag_a = 0;
	}

	sched_current_task->resume();
}

/* ------------------------------------------------------------------------- *
 *  sched_release_wait - K2 @0xc001a6c8 (K1 @0xc001dbf0). CONFIRMED
 *  structurally identical to K1: if the task's wait descriptor's status slot
 *  is already non-zero (meaning it also has a live delay-heap entry),
 *  extracts it first (sched_delay_heap_extract_min), clears the status slot
 *  to 0, then either re-readies the task (sched_make_ready) or, if state
 *  bit 2 is set (an "aborting"-shaped condition), just stamps state=4.
 * ------------------------------------------------------------------------- */
extern void sched_delay_heap_extract_min(uint32_t *heap_ref);	/* FUN_c001a890, structure only - not transcribed, same as K1's own treatment of FUN_c001ded8 */
extern uint32_t sched_delay_heap_sift_down(uint32_t half_index, int32_t wake_tick);	/* FUN_c001aae8 (K1: FUN_c001de18) - address CONFIRMED live via extract_min's own 2-arg call site, 2026-07-19; not itself transcribed */
extern uint32_t sched_delay_heap_sift_up(uint32_t index);				/* FUN_c001a7d0 (K1: FUN_c001e250) - address CONFIRMED live via extract_min's own 1-arg call site, 2026-07-19; not itself transcribed */

void sched_release_wait(struct sched_tcb *tcb)	/* FUN_c001a6c8 */
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
 *  sched_wait_list_insert - K2 @0xc001a720 (K1 @0xc001dc48). REAL, CONFIRMED
 *  DIFFERENCE FROM K1, transcribed faithfully rather than smoothed over: K1's
 *  own sched_wait_list_insert does NOT call sched_remove_from_ready - only
 *  the separate TIMED variant (sched_timed_wait_list_insert, via
 *  sched_delay_heap_insert) did that, since K1's model assumed the blocking
 *  task is, by construction, not currently on any ready list. K2's version
 *  DOES call sched_remove_from_ready(sched_current_task) unconditionally,
 *  right after stamping state=0x32, BEFORE the priority-sorted list insert -
 *  independently re-confirmed by direct decompile inspection, not a
 *  misreading. Two explanations, neither confirmed this pass: (a) K2
 *  genuinely merged the timed/untimed insert paths and always removes from
 *  ready defensively (a no-op if the task wasn't there), or (b) this K2
 *  function is actually the TIMED variant and the untimed one has a
 *  different, not-yet-found address. Left open rather than forced into
 *  either story - transcribed exactly as K2's own decompile shows.
 *
 *  Otherwise identical to K1: stamps state=0x32 (blocked/waiting), points
 *  TCB+0x14 at the caller's wait_desc, then priority-sorted-inserts into
 *  list_head's own wait list (walking while existing entries' priority <=
 *  self's), branching on whether list_head's own cfg_record bit0 is active.
 * ------------------------------------------------------------------------- */
void sched_wait_list_insert(struct kobj_record *list_head, void *wait_desc)	/* FUN_c001a720 */
{
	struct sched_tcb *self = sched_current_task;

	self->state = 0x32;
	sched_remove_from_ready(self);		/* REAL DIFFERENCE FROM K1 - see note above */
	self->wait_desc = (uint32_t *)wait_desc;

	if ((*(uint32_t *)((struct sched_tcb *)list_head)->cfg_record & 1) != 0) {
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

/* ============================================================================
 *  BOOT-TIME BRING-UP: TCB/READY-QUEUE INIT + AUTO-START, AND THE
 *  INTERRUPT-VECTOR CAPTURE - real, confirmed to exist, but NOT fully
 *  structurally resolved this pass (dense pointer/index arithmetic, no
 *  independent verification path - same treatment this project already
 *  gives heap_malloc/heap_free, clcdc_blit_glyph, etc.)
 * ============================================================================
 */

/* ------------------------------------------------------------------------- *
 *  sched_tcb_reset - K2 @0xc0019cd4. Clears/seeds a handful of TCB control
 *  bits (state=0, two control-byte bits cleared, priority seeded from the
 *  TCB's own cfg_record+0xc) - matches K1's own header description of
 *  "sched_tcb_reset" (cited there by name but not independently given its
 *  own address). Called both from sched_tcb_table_init_and_autostart below
 *  AND from one more site near sched_remove_from_ready's own neighborhood
 *  (0xc001aab4, not traced) - real, not re-derived further.
 * ------------------------------------------------------------------------- */
void sched_tcb_reset(struct sched_tcb *tcb)	/* FUN_c0019cd4 */
{
	uint32_t cfg_flags = *(uint32_t *)(tcb->cfg_record + 0xc);

	tcb->ctrl &= ~2u;
	tcb->unused_0x10 = 0;
	tcb->priority = (uint8_t)cfg_flags;
	tcb->ctrl &= ~4u;
	tcb->state = 0;
}

/* ------------------------------------------------------------------------- *
 *  sched_task_create_and_ready - K2 @0xc0019d08 (K1's own citation:
 *  FUN_c001ce00). CONFIRMED to be the real "auto-start a ROM-configured
 *  task" primitive: builds a fresh stack frame from the task's own config
 *  record (stack base + size), stashes the entry point as the first value
 *  on that new stack, sets TCB+0x1c to a fixed generic trampoline
 *  (resolved 0xC001A4E8, "the fixed generic task trampoline every new task
 *  starts at" - identical role to K1's own finding), TCB+0x18 to the new
 *  stack pointer, then calls sched_make_ready. Called from
 *  sched_tcb_table_init_and_autostart below, once per ROM-table entry whose
 *  own gating condition (bit 2 of a status field, per that function's own
 *  loop) is satisfied - this IS the concrete mechanism eva_board_main.c's
 *  own K2 header left as "no confirmed counterpart" for eva_board_start_task
 *  - CLOSES that open item: K2 auto-starts tasks via a ROM table walk at
 *  crt0 time, exactly like K1, NOT via an eva_board_start_task-shaped
 *  explicit call from eva_board_main's own body.
 *
 *  BUG FIX (2026-07-19 live pass): this function's own prior draft here
 *  wrote `stack_top = cfg + *(cfg+0x10) + *(cfg+0x14)`, erroneously adding
 *  the cfg pointer itself into the sum. decompile_function on FUN_c0019d08
 *  live shows NO such addition - `iVar2 = *(int*)(iVar4+0x14) + *(int*)
 *  (iVar4+0x10)`, i.e. stack_top is simply cfg's own +0x10/+0x14 words ADDED
 *  TO EACH OTHER (a base address + a size, both stored as plain words, no
 *  pointer arithmetic against cfg), EXACTLY matching K1's own
 *  `(uint32_t *)(*(cfg+0x14) + *(cfg+0x10))` with no cfg addition either.
 *  Also confirmed, from the live ROM table's own real data (see this file's
 *  header, 2026-07-19 live pass): cfg+8 (not cfg+4) holds the real jump
 *  target - all 3 observed cfg+8 values are real code addresses
 *  (0xC00072C0/0xC0007314/0xC0007330), while cfg+4 holds small integers
 *  (0/1/1). Since the generic trampoline pops [stack_top-8] first (into the
 *  argument register) then [stack_top-4] second (into PC), and cfg+4 is
 *  stored at stack_top-8 while cfg+8 is stored at stack_top-4, this
 *  DEFINITIVELY resolves K1's own open ambiguity ("which of the two plays
 *  which role is not resolved"): cfg+4 is the task's argument, cfg+8 is its
 *  real entry point.
 * ------------------------------------------------------------------------- */
void sched_task_create_and_ready(struct sched_tcb *tcb)	/* FUN_c0019d08 */
{
	uint8_t *cfg = (uint8_t *)tcb->cfg_record;
	uint8_t *stack_top = (uint8_t *)(*(uint32_t *)(cfg + 0x14) + *(uint32_t *)(cfg + 0x10));
	uint32_t task_arg   = *(uint32_t *)(cfg + 4);	/* -> stack_top-8, popped into the arg register by the trampoline */
	uint32_t task_entry = *(uint32_t *)(cfg + 8);	/* -> stack_top-4, popped into PC by the trampoline - the REAL jump target, see fix note above */

	*(uint32_t *)(stack_top - 4) = task_entry;
	*(uint32_t *)(stack_top - 8) = task_arg;

	tcb->resume = (void (*)(void))0xc001a4e8;	/* generic task trampoline, resolved DAT_c0019d44 */
	tcb->saved_sp = (void *)(stack_top - 8);

	sched_make_ready(tcb);
}

/* ------------------------------------------------------------------------- *
 *  sched_tcb_table_init_and_autostart - K2 @0xc0019bc0. NOT individually
 *  named/addressed in K1's own file (K1's own header describes the
 *  mechanism narratively via FUN_c001ccb8's table walk without transcribing
 *  it as C). Two real phases, confirmed via its own body:
 *   1. Zero two list-head sentinels, then loop 16 times building
 *      self-linked-when-empty ready_list[0..15] entries at 8-byte stride -
 *      this IS the ready-queue's own boot-time init, at the SAME resolved
 *      base (0xC01D5154) sched_make_ready/sched_remove_from_ready/
 *      sched_ready_scan_highest all independently reference.
 *   2. Walk a ROM task-config table (count from resolved 0xC002A6F8),
 *      resetting each entry via sched_tcb_reset above, and calling
 *      sched_task_create_and_ready on any entry whose own status field has
 *      bit 2 set - directly matching K1's own headline finding "crt0 walks
 *      a ROM table of pre-configured task records and auto-starts every one
 *      whose config-record flags bit 1 is set" (bit-number-off-by-one
 *      caveat: transcribed exactly as K2's own decompile masks the bit,
 *      `& 2` here vs K1's own "bit 1" - both index the SAME bit position
 *      counting from bit 0, no real discrepancy, just a description-style
 *      difference).
 *  Called from FUN_c00199dc, the SAME crt0 subsystem-call site as
 *  kobj_table_init above - both boot-time table inits happen back-to-back
 *  during eva_board_crt0, matching K1's own architecture.
 *
 *  NOT independently re-derived further this pass (dense pointer
 *  arithmetic, matches this project's own established practice for code
 *  this shape) - documented structurally rather than transcribed
 *  statement-for-statement for phase 2's own inner loop.
 * ------------------------------------------------------------------------- */
void sched_tcb_table_init_and_autostart(void);	/* FUN_c0019bc0, structure only - not transcribed, see above */

/* ------------------------------------------------------------------------- *
 *  Interrupt-vector-table capture - K2 @0xc0019b34. Matches K1's own
 *  "SECOND HEADLINE FINDING" (K1's FUN_c001cba8/FUN_c001cc2c cluster): loops
 *  8 times over the ARM exception vector table's own literal-pool region
 *  (addresses 0, 4, 8, ..., 28 - i.e. the vector table itself), decoding
 *  each `LDR PC,[PC,#imm]`-style entry's own target literal-pool slot and
 *  copying it into two parallel software arrays (an address-slot array and
 *  a value array). Sets a flag (resolved SAME address as sched_flag_a,
 *  0xC01D5114) to 1 first - i.e. this function ALSO touches the shared
 *  scheduler idle-flag global, a genuine, confirmed cross-reference not
 *  independently explained this pass (possibly this flag has a dual
 *  "system not yet fully up" / "idle" meaning, matching K1's own
 *  unreconciled-polarity note for the identical address). Real caller:
 *  FUN_c0007268 (eva_board_crt0), one of its eleven subsystem-bring-up
 *  calls.
 *
 *  NOT independently re-derived to C - dense pointer-relative literal-pool
 *  arithmetic, matches this project's own precedent for this shape of code.
 *  Documented here purely as a confirmed, real cross-reference to this
 *  file's own sched_flag_a, not claimed as part of the scheduler's own
 *  functional core.
 * ------------------------------------------------------------------------- */
void interrupt_vector_capture(void);	/* FUN_c0019b34, structure only - not transcribed, see above */

/* -------------------------------------------------------------------------
 * Still genuinely open (updated 2026-07-19 live pass):
 *  - eva_board_sched_ready (K1's own "already-active, reprioritize" richer
 *    ready-insert) and eva_board_sched_requeue (K1's generic sorted-list
 *    requeue helper): STILL no confirmed K2 counterpart. The live pass DID
 *    turn up one new, real, previously-uncharacterized function (~0xC001AA98,
 *    unbounded region, calls sched_remove_from_ready then sched_tcb_reset on
 *    *sched_current_task - see the 2026-07-19 header section above) but its
 *    shape (remove + full TCB reset, not a reprioritize-in-place or a
 *    generic list requeue) does NOT match either K1 description - checked
 *    and explicitly NOT claimed as either, rather than forced. Both K1 names
 *    remain genuinely unlocated in K2.
 *  - sched_wait_list_insert's own unconditional sched_remove_from_ready
 *    call - genuinely unreconciled against K1's own timed/untimed split,
 *    see that function's own note above.
 *  - The delay-heap sift-up/sift-down internals: addresses now CONFIRMED
 *    (sift_down=FUN_c001aae8, sift_up=FUN_c001a7d0, see the 2026-07-19
 *    header section and their own extern declarations above), but neither
 *    was transcribed to C this pass (dense index arithmetic, same treatment
 *    this project gives code this shape elsewhere).
 *  - sched_tcb_table_init_and_autostart's own ROM table CONTENTS - RESOLVED
 *    this pass via live read_memory (see the file header's own "HEADLINE
 *    FINDING" section): exactly 3 autostart tasks exist, and eva_board_main
 *    does NOT exist as a single entity at all in K2 - it is split across (at
 *    least) 2 of those 3 tasks. Full detail, addresses, and priorities in
 *    the header above.
 *  - The unbounded 0xC001AA98 function's own tail (branch on TCB ctrl bit 0,
 *    past the sched_tcb_reset call) - not decoded this pass, out of time
 *    budget.
 *  - interrupt_vector_capture's own shared-flag cross-reference to
 *    sched_flag_a - real, confirmed, but not explained.
 *  - This project's own convention (matching K1's task_sched.c) of NOT
 *    editing eva_board_main.c even though this file resolves several of its
 *    own "still open" items (including, now, its OWN previously-unlocated
 *    "../EvaBoardMain.cpp" xref - see the header's task id=3 finding) - a
 *    future consolidation pass should fold the HEADLINE FINDING #2 /
 *    auto-start-mechanism / eva_board_main-decomposition results back into
 *    that file's own header, per this file's own instructions.
 * ------------------------------------------------------------------------- */
