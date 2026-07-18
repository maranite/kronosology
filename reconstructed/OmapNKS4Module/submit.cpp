// SPDX-License-Identifier: GPL-2.0
/*
 * submit.c  -  host->panel URB submission, the read-event handshake, the wake
 * signals, and aftertouch calibration.
 *
 *  Output: a free URB is pulled from the command/video free list, its transfer buffer
 *  is filled with byte-swapped 32-bit command words, and it is handed to
 *  stg_usb_submit_urb().  Each successful submit Pings the active-sense thread so its
 *  idle tick is deferred.
 *
 *  Input handshake: a query installs sWaitReadPtr; the interrupt path calls
 *  SendNKS4EventToLinuxReader() with the matching response, which stores it and wakes
 *  WaitForNKS4ReadEvent().  The Atmel read path uses WaitOnAtmelRead()/
 *  SignalAtmelReadComplete() the same way.
 */

#include "omapnks4_internal.h"

extern struct urb_node *sBulkFreeCommandURBList;	/* usb.c */
extern struct urb_node *sBulkFreeVideoURBList;
extern int sBulkCommandURBsInUse, sBulkVideoURBsInUse;
/* Ground truth (real Kbuild build attempt, 2026-07-15): this was a genuine
 * pre-existing duplicate-definition bug (also defined in usb.cpp, which is the
 * real owner - set from OmapNKS4Probe's endpoint discovery) never caught before
 * since the module had never actually been linked. */
extern unsigned int sMaxWritePacketSize;

static unsigned int *sWaitReadPtr;	/* where to deliver the next response word */
int sDoingWait4Write;
static int sShutdownDelay;

/* Wait-queue storage for this file's own four wake sites - same 12-byte
 * wait_queue_head_t-sized-storage convention established in usb.cpp (see
 * omapnks4_internal.h's own comment for the full derivation/precedent).
 * UPDATED 2026-07-18: sReadEventWaitQueue and sAtmelReadWaitQueue are now
 * genuinely load-bearing - WaitForNKS4ReadEvent/WaitForNKS4CommandWrite/
 * WaitOnAtmelRead all really block on them via real prepare_to_wait()/
 * schedule_timeout()/finish_wait() or sleep_on_timeout() (see each function's
 * own comment below for the disassembly evidence; this was previously a
 * stg_msleep() polling simplification, now corrected). sVideoMsgWaitQueue/
 * sShutdownSsdWaitQueue are UNCHANGED by this pass - ProcessMsgRoutine/
 * ShutdownSSDRoutine (main.cpp) still poll rather than block on them, a
 * separate, not-yet-revisited simplification (out of this pass's scope). */
/* NOT static: OmapNKS4Probe (usb.cpp) __init_waitqueue_head()s all four of these
 * alongside its own three - see that function's own comment. This file's own
 * prior comment above ("__wake_up()ing them is currently a harmless no-op, not
 * yet load-bearing") was wrong: a real kernel __wake_up() walks a
 * wait_queue_head_t's embedded list_head, which for a genuinely empty queue
 * must self-point (next==prev==&head), not sit zeroed - confirmed via a real
 * NULL-pointer oops in __wake_up_common on a live VM boot test, 2026-07-17,
 * the first test ever to actually exercise SendNKS4EventToLinuxReader()'s own
 * __wake_up(sReadEventWaitQueue, ...) call (every earlier test's
 * CommunicationCheck always timed out before reaching it). A real device's
 * first genuine interrupt-IN reply would have hit this identical crash on
 * real hardware - a real correctness bug, not a VM-only issue. */
unsigned char sReadEventWaitQueue[0xc];
unsigned char sAtmelReadWaitQueue[0xc];
unsigned char sVideoMsgWaitQueue[0xc];
unsigned char sShutdownSsdWaitQueue[0xc];

/* thread run/signal flags (main.c) */
extern int sProcessMsgThreadRunning, sVideoMsgSignalled;
extern int sShutdownSSDThreadRunning, sShutdownSSDSignaled;

/* ---- small helpers ----------------------------------------------------- */

unsigned int NumBytesToNumInts(int bytes)		{ return (unsigned)(bytes + 3) >> 2; }

/* Full 4-byte reversal of each of nInts words (panel is big-endian) - ground
 * truth: real ReverseMessage@0x12050 disassembly, 2026-07-16. The real code
 * is `ROR CX,8; ROR ECX,0x10; ROR CX,8` - NOT "swap bytes within each 16-bit
 * half, keep halves in place" as this previously implemented (confirmed
 * wrong the hard way: for 0xAABBCCDD the real sequence produces 0xDDCCBBAA,
 * a full byte-order reversal, but the old code produced 0xBBAADDCC - same
 * per-half byte swap, but the two 16-bit halves stay in their original
 * position instead of swapping too). This corrupted every command word ever
 * sent to the panel, causing CommunicationCheck to time out waiting for a
 * response the panel never sent to a request it never understood -
 * confirmed on real hardware, 2026-07-16 (no crash, unlike the earlier
 * bugs - just silent protocol-level garbage). */
void ReverseMessage(unsigned int *words, int nInts)
{
	for (; nInts; nInts--, words++) {
		unsigned int w = *words;
		*words = ((w & 0xffu) << 24) | ((w & 0xff00u) << 8) |
			 ((w >> 8) & 0xff00u) | ((w >> 24) & 0xffu);
	}
}

unsigned int OmapNKS4GetMaxWritePacketSize(void)	{ return sMaxWritePacketSize; }
void SetShutdownDelay(int delay)			{ sShutdownDelay = delay; }

bool OmapNKS4WriteQueueNotFull(int type)
{
	if (type == 1)
		return sBulkFreeVideoURBList != (struct urb_node *)&sBulkFreeVideoURBList;
	return sBulkFreeCommandURBList != (struct urb_node *)&sBulkFreeCommandURBList;
}

/* ---- input handshake / wake signals ------------------------------------ */

void SendNKS4EventToLinuxReader(unsigned int cmd)
{
	if (sWaitReadPtr) {
		*sWaitReadPtr = cmd;
		sWaitReadPtr = 0;
		__wake_up(sReadEventWaitQueue, 3, 1, 0);
	}
}

/* CORRECTION (disassembly re-check, 2026-07-18): WaitForNKS4ReadEvent and
 * WaitForNKS4CommandWrite (below) do NOT poll in the real binary - both real
 * functions build a stack-local `wait_queue_t` and run it through genuine
 * `prepare_to_wait()`/`schedule_timeout()`/`finish_wait()` blocking, exactly the
 * hand-expanded body of the kernel's own `wait_event_timeout()` macro. The
 * previous comment on this file's now-removed `schedule_timeout_wait()`/
 * `sleep_on_timeout()` shims - "called with elided arguments in the original
 * decompile" - was simply wrong: every argument (wait-queue address, TASK
 * state, jiffies budget) is directly visible in the raw disassembly once you
 * look past the decompiler's stripped-down `undefined4 WaitForNKS4CommandWrite
 * (void)` signature. Surprising real finding: BOTH functions block on the exact
 * SAME wait queue, `sReadEventWaitQueue` (real address 0x1b668 in both
 * WaitForNKS4ReadEvent@0x12a95/0x12ab1 and WaitForNKS4CommandWrite@0x12980/
 * 0x129a9), and SendNKS4EventToLinuxReader's own `__wake_up(0x1b668,...)` call
 * (@0x12b40) is the wake for both - confirmed via `get_xrefs_to` on 0x1b668
 * (8 references: OmapNKS4Probe's one `__init_waitqueue_head` call plus exactly
 * these three functions, no others). There is no separate "command write"
 * queue in the real binary - sDoingWait4Write's own completion (cleared by
 * WriteCallback, usb.cpp) is signaled on the same queue as a read event.
 *
 * `struct omap_wait_entry` mirrors the real Linux 2.6.32 `wait_queue_t` layout
 * exactly as both functions construct it on the stack (flags=0, private=current
 * task via the same %fs:per_cpu__current_task access as
 * stg_get_current_task_nks4(), func=<see below>, task_list self-pointing at
 * construction) - not guessed, read directly off the stack-store sequence at
 * each function's entry (e.g. WaitForNKS4ReadEvent@0x12a57-0x12a82). One field
 * is NOT independently symbol-confirmed: the `func` wake-callback constant
 * (0x20148, a data relocation Ghidra never resolved to a named import - it
 * isn't in this binary's own `get_imports` list, unlike the four call
 * primitives above). Modeled as `autoremove_wake_function`, the real kernel's
 * own default for exactly this construction pattern (`DEFINE_WAIT()` in
 * <linux/wait.h>) and the only plausible candidate - not a disassembly-
 * confirmed symbol name, flagged here as the one best-effort part of this fix. */
struct omap_wait_entry {
	unsigned int flags;
	void *priv;
	void *func;
	void *next, *prev;	/* task_list, self-pointing (empty) at construction */
};

extern "C" int autoremove_wake_function(void *wait, unsigned int mode, int sync, void *key);

static inline void *submit_get_current_task(void)
{
	void *current_task;
	asm volatile("mov %%fs:per_cpu__current_task, %0" : "=r"(current_task));
	return current_task;
}

static void init_wait_entry(struct omap_wait_entry *w)
{
	w->flags = 0;
	w->priv = submit_get_current_task();
	w->func = (void *)autoremove_wake_function;
	w->next = w->prev = &w->next;
}

int WaitForNKS4ReadEvent(unsigned int *resp)
{
	sWaitReadPtr = resp;
	if (!resp)
		return 0;
	/* VM-only: deliver a reply vm_usb_submit_urb (usb.cpp) queued while this
	 * call's own SubmitNKS4CommandWrite() was still running - it couldn't
	 * call SendNKS4EventToLinuxReader() directly back then because
	 * sWaitReadPtr (just set above) didn't exist yet at that point in the
	 * single synchronous call chain. See vm_usb_submit_urb's own comment. */
	if (sVmVirtualProbe && sVmPendingReplyValid) {
		sVmPendingReplyValid = 0;
		SendNKS4EventToLinuxReader(sVmPendingReply);
	}
	/* real wait_event_timeout(sReadEventWaitQueue, sWaitReadPtr==0, 1000) -
	 * 0x3e8=1000 is the literal MOV EBX,0x3e8 immediate at WaitForNKS4ReadEvent
	 * +0x35 (@0x12a75), TASK_UNINTERRUPTIBLE (state=2) is the real ECX
	 * immediate at each prepare_to_wait call. */
	struct omap_wait_entry wait;
	init_wait_entry(&wait);
	long remaining = 1000;
	for (;;) {
		prepare_to_wait(sReadEventWaitQueue, &wait, 2 /* TASK_UNINTERRUPTIBLE */);
		if (sWaitReadPtr == 0)
			break;
		remaining = schedule_timeout(remaining);
		if (remaining == 0)
			break;
	}
	finish_wait(sReadEventWaitQueue, &wait);
	if (sWaitReadPtr == 0)
		return 0;
	/* CORRECTION (fresh adversarial pass, 2026-07-18): ground truth's real
	 * timeout message (@0x12adc) is a %s/%d/%llu-parameterized
	 * "<6>OmapNKS4:%s: line %d: [%llu] WaitForNKS4ReadEvent() timed out\n\n"
	 * with %s="WaitForNKS4ReadEvent", %d=0x405=1029, and a live rdtsc()
	 * timestamp - and a DOUBLE trailing newline. Previous text
	 * ("<6>OmapNKS4: WaitForNKS4ReadEvent() timed out\n") dropped the
	 * func/line/timestamp entirely and only had one newline. */
	printk("<6>OmapNKS4:%s: line %d: [%llu] WaitForNKS4ReadEvent() timed out\n\n",
	       "WaitForNKS4ReadEvent", 1029, omapnks4_rdtsc());
	sWaitReadPtr = 0;
	return -1;
}

/* WaitOnAtmelRead: ground truth is a single tail-call to the real kernel
 * sleep_on_timeout() - no hand-rolled loop at all (disassembly@0x12070: MOV
 * EDX,0x7d0 ; MOV EAX,sAtmelReadWaitQueue ; CALL sleep_on_timeout ; RET). 0x7d0
 * = 2000 jiffies - CORRECTED from this file's previous guessed 1000; the
 * fake `sleep_on_timeout()` polling shim this file used to shadow the real
 * kernel function of the same name is gone, this now calls the genuine
 * import declared in omapnks4_internal.h. */
void WaitOnAtmelRead(void)            { sleep_on_timeout(sAtmelReadWaitQueue, 2000); }
void SignalAtmelReadComplete(void)    { __wake_up(sAtmelReadWaitQueue, 3, 1, 0); }

void SignalVideoMessageProcessor(void)
{
	if (sProcessMsgThreadRunning) { sVideoMsgSignalled = 1; __wake_up(sVideoMsgWaitQueue, 3, 1, 0); }
}

void SignalShutdownSSD(void)
{
	if (sShutdownSSDThreadRunning) { sShutdownSSDSignaled = 1; __wake_up(sShutdownSsdWaitQueue, 3, 1, 0); }
}

/* ---- URB submission ---------------------------------------------------- */

/* CORRECTION (re-verification pass, 2026-07-17): this pop was completely
 * unlocked. Ground truth wraps every free-list pop in
 * _spin_lock_irqsave()/_spin_unlock_irqrestore() - a real, previously
 * unaddressed race, not just a style gap: URBs are freed back onto this
 * same list from WriteCallback's completion-handler (interrupt) context
 * concurrently with process-context submits popping from it here.
 *
 * FURTHER CORRECTION (fresh adversarial pass, 2026-07-18): the lock fix above
 * was still incomplete against ground truth in three ways, all confirmed via
 * disassembly of every real call site (SubmitNKS4CommandMultipleWriteNONBLOCKING
 * @0x12500, SubmitOmapNKS4CmdBulkWrite @0x12650, SubmitOmapNKS4BulkWrite
 * @0x127b0 - all three inline the identical sequence):
 *
 *  1. `struct urb_node { next, pprev }` (omapnks4_internal.h) is a real Linux
 *     hlist_node - pprev is a POINTER TO A POINTER, not a plain prev pointer.
 *     Ground truth's pop is a genuine `hlist_del()`: `next=node->next;
 *     pprev=node->pprev; next->pprev=pprev; *pprev=next;` (confirmed
 *     unconditional, no NULL-check on next, e.g. @0x1254d-0x012556) - the
 *     previous `*list = n->next` here only advanced the head pointer and left
 *     the NEW head's own pprev stale, which would misdirect a future unlink
 *     the moment this list ever holds more than one node.
 *  2. Ground truth then poisons the just-removed node with the kernel's own
 *     LIST_POISON1/LIST_POISON2 (0x100100/0x200200 - real <linux/poison.h>
 *     constants) at node->next/node->pprev (@0x12558-0x1255f) - previously
 *     not reproduced at all.
 *  3. Ground truth uses TWO SEPARATE lock objects, one per free list, not one
 *     shared lock: the command-list pop locks address 0x1b6a0
 *     (SubmitNKS4CommandMultipleWriteNONBLOCKING@0x1252c,
 *     SubmitOmapNKS4CmdBulkWrite@0x1268e) while the video-list pop locks a
 *     DIFFERENT address 0x1b698 (SubmitOmapNKS4BulkWrite@0x127f2) - serializing
 *     both lists on one shared lock (as this file previously did) is safe but
 *     not what ground truth does, and unnecessarily forces the command and
 *     video submit paths to contend with each other.
 *  4. All three call sites also increment their pool's in-use counter
 *     (sBulkCommandURBsInUse/sBulkVideoURBsInUse) INSIDE this same critical
 *     section (e.g. `ADD [sBulkCommandURBsInUse],1` sits between the lock and
 *     unlock calls @0x12543/0x126a5/0x12809) - this file previously did the
 *     increment in each caller AFTER pop_free_urb() had already released the
 *     lock, a real (if narrow) lost-update race between two concurrent
 *     submitters on separate CPUs under this SMP-preempt kernel.
 *
 * Fixed by making pop_free_urb() take the specific lock and counter to use,
 * matching ground truth's per-list separation and counter-under-lock ordering
 * exactly, and by performing a real hlist_del() including the poison writes. */
static unsigned char sCommandUrbFreeListLock[4];
static unsigned char sVideoUrbFreeListLock[4];

/* CORRECTION (full-coverage re-audit, 2026-07-18): the emptiness check itself was
 * happening INSIDE the lock (check-under-lock, matching neither the timing nor the
 * unlocked-vs-locked-read distinction ground truth actually makes). Fresh disassembly
 * of all three real call sites (SubmitNKS4CommandMultipleWriteNONBLOCKING@0x12500/
 * clone.0@0x10c90, SubmitOmapNKS4CmdBulkWrite@0x12650, SubmitOmapNKS4BulkWrite@0x127b0)
 * shows ground truth checks `*list == list` (an UNLOCKED read) BEFORE ever calling
 * _spin_lock_irqsave() at all - the "no free urbs" printk+return sits in the same
 * unlocked branch as the check itself, and the locked branch below it pops
 * UNCONDITIONALLY (no second empty-check under the lock). Fixed to match: the
 * emptiness check now happens before the lock is taken, not after. */
/* pop a free URB node off 'list' (intrusive hlist, head == &list when empty) */
static struct urb_node *pop_free_urb(struct urb_node **list, unsigned char *lock,
				      int *in_use_counter)
{
	if (*list == (struct urb_node *)list)
		return 0;
	unsigned long flags = _spin_lock_irqsave(lock);
	struct urb_node *n = *list;
	struct urb_node *next = n->next;
	struct urb_node **pprev = n->pprev;
	next->pprev = pprev;
	*pprev = next;
	n->next = (struct urb_node *)0x100100;		/* LIST_POISON1 */
	n->pprev = (struct urb_node **)0x200200;	/* LIST_POISON2 */
	(*in_use_counter)++;
	_spin_unlock_irqrestore(lock, flags);
	return n;
}

/* fill the URB transfer buffer with the command words and submit.
 *
 * doReverse: ground truth (real disassembly, 2026-07-16 hardware session) -
 * the plain command-word path (SubmitNKS4CommandMultipleWriteNONBLOCKING,
 * real @0x2500, and the clone.0@0xc90 it calls) writes words RAW, no
 * reversal at all. Only the Atmel-prefixed path
 * (SubmitOmapNKS4CmdBulkWrite, real @0x2650 - which has its own direct
 * ReverseMessage call below, not routed through this function) and the
 * event-receive decode path reverse. Confirmed on real hardware: sending
 * CommunicationCheck (0x00ee0000) through this function with reversal
 * enabled put "00 ee 00 00" on the wire instead of stock's "00 00 ee 00" -
 * the panel silently never recognized the command and never replied
 * (bulk-OUT completed fine at the USB level; interrupt-IN never got real
 * traffic; comm-check timed out). The video/bulk write path
 * (SubmitOmapNKS4BulkWrite) also routes through this function but its own
 * real reversal behavior hasn't been ground-truthed yet, so it keeps the
 * previous (reversing) behavior via doReverse=true rather than being
 * silently changed alongside the confirmed command-word fix. */
static int submit_urb_words(struct urb_node *node, const unsigned int *words, unsigned int nInts,
			    unsigned int prefixLenBytes, bool doReverse)
{
	/* +0x40 = urb->buffer - confirmed both by usb.cpp's own real-offset table
	 * comment ("+0x40 buffer") and by alloc_command_urbs/configure_interrupt_urb
	 * actually writing the kmalloc'd buffer there. This read a completely
	 * different (free-list-node-adjacent) offset instead, so buf came back as
	 * garbage/NULL and the very first real command write NULL-derefed
	 * ("_Z22SubmitNKS4CommandWritej" oops, confirmed on real hardware,
	 * 2026-07-16) - a plain wrong-offset typo, not a deeper design issue. */
	void *urb = (char *)node - 0x14;
	unsigned int *buf = (unsigned int *)*(void **)((char *)urb + 0x40);

	/* +0x50 = urb->length - ground truth: real
	 * SubmitNKS4CommandMultipleWriteNONBLOCKING@0x12500 sets this via
	 * `*(uint*)(node + 0x3c) = ...`, i.e. relative to the URB_NODE pointer
	 * (node = urb + 0x14, confirmed by that same function poison-writing the
	 * node's own linkage fields at node+0/node+4 right after popping it), so
	 * node+0x3c == urb+0x50 - matching usb.cpp's own documented offset table
	 * ("+0x50 length"). This previously wrote urb+0x3c directly (this file's
	 * own `urb` is already the base URB pointer, not the node pointer, so no
	 * +0x14 correction was needed/wanted here) - urb+0x3c is transfer_flags
	 * (see usb.cpp's alloc_urb_pool/configure_interrupt_urb, which OR in
	 * 0x100 there), so every command write was clobbering the flags field
	 * with the byte count instead of setting the real length field,
	 * confirmed on real hardware, 2026-07-16 (comm-check timeout with no
	 * crash - the write "succeeded" but carried garbage flags/an unset
	 * length, so the panel never received a correctly-framed transfer). */
	*(unsigned int *)((char *)urb + 0x50) = nInts * 4 + prefixLenBytes;
	for (unsigned int i = 0; i < nInts; i++)
		buf[i] = words[i];
	if (doReverse)
		ReverseMessage(buf, NumBytesToNumInts(nInts * 4 + prefixLenBytes));

	/* CORRECTION (full-coverage re-audit, 2026-07-18): the two "TEMP diagnostic"
	 * printks previously here (a wire-bytes/len/flags/pipe dump before submit, an
	 * "rc=%d" dump after) do not exist anywhere in ground truth - fresh decompile of
	 * the real SubmitNKS4CommandMultipleWriteNONBLOCKING@0x12500 (this function is
	 * fully inlined into that one real caller, confirmed only one call site exists)
	 * shows exactly two printks total in the whole function: the "ran out of urbs"
	 * one (now in the caller below, matching ground truth's own branch structure)
	 * and the "fails %d" one immediately below - no debug dump of any kind. Removed
	 * as non-ground-truth debug scaffolding, same class of finding as main.cpp's own
	 * already-removed "TEMP diagnostic" additions.
	 *
	 * mem_flags=0x20 (GFP_ATOMIC) - ground truth: real
	 * SubmitNKS4CommandMultipleWriteNONBLOCKING@0x10d01 disassembly, this is
	 * a command-write submission which can run from atomic/locked contexts,
	 * unlike the interrupt URB's own initial process-context submit in
	 * OmapNKS4Init (which the real binary passes GFP_KERNEL=0xd0). */
	int rc = vm_usb_submit_urb(urb, 0x20);
	if (rc == 0) {
		CActiveSenseThread_Ping();	/* defer the idle active-sense tick */
		return 0;
	}
	/* CORRECTION (full-coverage re-audit, 2026-07-18): ground truth's real message
	 * (.rodata @0x19b88, search_strings-confirmed) is
	 * "<6>OmapNKS4:%s: line %d: ERROR: SubmitNKS4CommandMultipleWriteNONBLOCKING()
	 * fails %d\n" with %s=the function's own name and %d=0x32a=810 - previous text
	 * ("<6>OmapNKS4: usb_submit_urb failed\n") had no relation to the real string. */
	printk("<6>OmapNKS4:%s: line %d: ERROR: SubmitNKS4CommandMultipleWriteNONBLOCKING() fails %d\n",
	       "SubmitNKS4CommandMultipleWriteNONBLOCKING", 810, rc);
	return -1;
}

/* CORRECTION (full-coverage re-audit, 2026-07-18): ground truth's real message
 * (.rodata @0x19b28, search_strings-confirmed) is "<6>OmapNKS4:%s: line %d: ERROR:
 * SubmitNKS4CommandMultipleWriteNONBLOCKING() ran out of urbs\n" with %s=the
 * function's own name and %d=0x311=785 - previous text dropped the %s/%d/"ERROR:"
 * portion entirely. */
int SubmitNKS4CommandMultipleWriteNONBLOCKING(unsigned int *cmds, unsigned int nInts)
{
	struct urb_node *n = pop_free_urb(&sBulkFreeCommandURBList, sCommandUrbFreeListLock,
					  &sBulkCommandURBsInUse);
	if (!n) {
		printk("<6>OmapNKS4:%s: line %d: ERROR: SubmitNKS4CommandMultipleWriteNONBLOCKING() ran out of urbs\n",
		       "SubmitNKS4CommandMultipleWriteNONBLOCKING", 785);
		return -1;
	}
	return submit_urb_words(n, cmds, nInts, 0, false);
}

/* CORRECTION (fresh adversarial pass, 2026-07-18): this printk did not match
 * ground truth at all - real disassembly (@0x1262c) shows this error path
 * uses the SAME parameterized format string as WaitForNKS4CommandWrite's own
 * "no free urbs" message below (real .rodata @0x1a018:
 * "<6>OmapNKS4:%s: line %d: SubmitNKS4CommandWrite() fails - no free urbs\n"),
 * with %s="SubmitNKS4CommandWrite" (@0x19140) and %d=0x344=836 - both
 * confirmed via read_memory/search_strings. Previous text
 * ("<6>OmapNKS4: SubmitNKS4CommandWrite() fails - no free urbs\n") dropped the
 * "%s: line %d:" portion entirely. */
int SubmitNKS4CommandWrite(unsigned int cmd)
{
	WaitForFreeBulkWriteURB(0);
	if (sBulkFreeCommandURBList == (struct urb_node *)&sBulkFreeCommandURBList) {
		printk("<6>OmapNKS4:SubmitNKS4CommandWrite: line 836: SubmitNKS4CommandWrite() fails - no free urbs\n");
		return -1;
	}
	return SubmitNKS4CommandMultipleWriteNONBLOCKING(&cmd, 1);
}

/* Ground truth: fresh Ghidra decompile + raw disassembly of
 * WaitForNKS4CommandWrite@0x128f0, 2026-07-16 (existence/shape)/2026-07-18
 * (wait mechanism, this correction). Genuinely undefined previously (only
 * forward-declared, called throughout command.cpp) despite being the core
 * "send a command word and block until the panel finishes writing it"
 * primitive. Same submit shape as SubmitNKS4CommandWrite above (this file's
 * own real duplicate of that submit sequence, not a refactor to share code,
 * to stay faithful to the real binary's own two separate, near-identical
 * functions).
 *
 * CORRECTION (disassembly re-check, 2026-07-18): the wait for
 * sDoingWait4Write to clear is NOT a plain stg_msleep() poll in the real
 * binary - see WaitForNKS4ReadEvent's own comment above for the full
 * writeup (both functions share the identical prepare_to_wait()/
 * schedule_timeout()/finish_wait() shape against the very same
 * sReadEventWaitQueue). The one real difference here: each of the outer
 * 51 (0x33) retry passes gets its OWN fresh `wait_queue_t` and its OWN fresh
 * 10-jiffy (0xa) schedule_timeout budget (confirmed by the real EBX=0xa
 * reload sitting INSIDE the per-pass setup block, @0x1296d, not hoisted
 * above the outer retry loop) - i.e. up to ~510 jiffies of real blocking
 * before the first "waiting for..." log line, not the previous 51*20ms=1020ms
 * of msleep polling. The outer 51-iterations-then-log-and-retry-forever
 * shape itself was already correct and is unchanged. */
int WaitForNKS4CommandWrite(unsigned int cmd)
{
	sDoingWait4Write = 1;
	WaitForFreeBulkWriteURB(0);
	if (sBulkFreeCommandURBList == (struct urb_node *)&sBulkFreeCommandURBList) {
		/* CORRECTION (fresh adversarial pass, 2026-07-18): decompile of this
		 * exact call site (@0x128f0) resolves the format args literally as
		 * %s="SubmitNKS4CommandWrite" (not "WaitForNKS4CommandWrite" - a real
		 * Korg source-level copy-paste artifact, apparently reusing
		 * SubmitNKS4CommandWrite's own message verbatim) and %d=0x344=836
		 * (not 868) - previously both wrong. */
		printk("<6>OmapNKS4:SubmitNKS4CommandWrite: line 836: SubmitNKS4CommandWrite() fails - no free urbs\n");
	} else if (SubmitNKS4CommandMultipleWriteNONBLOCKING(&cmd, 1) == 0) {
		for (;;) {
			for (int i = 0; i < 0x33; i++) {
				if (sDoingWait4Write == 0) {
					sDoingWait4Write = 0;
					return 0;
				}
				struct omap_wait_entry wait;
				init_wait_entry(&wait);
				long remaining = 10;
				for (;;) {
					prepare_to_wait(sReadEventWaitQueue, &wait, 2 /* TASK_UNINTERRUPTIBLE */);
					if (sDoingWait4Write == 0)
						break;
					remaining = schedule_timeout(remaining);
					if (remaining == 0)
						break;
				}
				finish_wait(sReadEventWaitQueue, &wait);
				if (sDoingWait4Write == 0) {
					sDoingWait4Write = 0;
					return 0;
				}
			}
			/* CORRECTION (fresh adversarial pass, 2026-07-18): the real
			 * .rodata string here (@0x1a21c, search_strings-confirmed) is
			 * "WaitForNKS4CommandWrite: waiting for NKS4 Command write\n" -
			 * genuinely missing the "<6>OmapNKS4:" prefix every other
			 * message in this file carries (a bare literal, not the
			 * %s/%d-parameterized style of the other two messages here). */
			printk("WaitForNKS4CommandWrite: waiting for NKS4 Command write\n");
		}
	}
	printk("<6>OmapNKS4:WaitForNKS4CommandWrite: line 977: WaitForNKS4CommandWrite fails\n");
	sDoingWait4Write = 0;
	return -1;
}

/* command-prefixed bulk write (Atmel etc.): byte 0 = command, then data.
 *
 * CORRECTION (full-coverage re-audit, 2026-07-18): fresh decompile of the real
 * SubmitOmapNKS4CmdBulkWrite@0x12650 turned up three genuine printk-text bugs, all
 * confirmed against real .rodata via search_strings:
 *  - the "too long" text was missing its real "%s: line %d:"/"fails -"/"!" framing
 *    (real string @0x1a060: "<6>OmapNKS4:%s: line %d: SubmitOmapNKS4CmdBulkWrite()
 *    fails - message too long!\n", %s/%d = "SubmitOmapNKS4CmdBulkWrite"/0x35f=863);
 *  - the "no free urbs" case had NO printk at all (real string @0x1a0b4:
 *    "<6>OmapNKS4:%s: line %d: SubmitOmapNKS4CmdBulkWrite() fails - no free
 *    urbs\n", args ("SubmitOmapNKS4CmdBulkWrite", 0x369=873));
 *  - the submit-failure path had NO printk at all (real string @0x1a100 -
 *    SHARED with SubmitOmapNKS4BulkWrite below - a real Korg copy-paste: the
 *    literal text says "SubmitOmapNKS4VideoWrite() fails %d" even from inside
 *    THIS function, while the %s argument correctly says
 *    "SubmitOmapNKS4CmdBulkWrite"; args ("SubmitOmapNKS4CmdBulkWrite",
 *    0x37d=893, rc)). */
int SubmitOmapNKS4CmdBulkWrite(unsigned char command, unsigned char *data, unsigned int nBytes)
{
	if (nBytes >= sMaxWritePacketSize) {
		printk("<6>OmapNKS4:%s: line %d: SubmitOmapNKS4CmdBulkWrite() fails - message too long!\n",
		       "SubmitOmapNKS4CmdBulkWrite", 863);
		return -1;
	}
	WaitForFreeBulkWriteURB(0);
	struct urb_node *n = pop_free_urb(&sBulkFreeCommandURBList, sCommandUrbFreeListLock,
					  &sBulkCommandURBsInUse);
	if (!n) {
		printk("<6>OmapNKS4:%s: line %d: SubmitOmapNKS4CmdBulkWrite() fails - no free urbs\n",
		       "SubmitOmapNKS4CmdBulkWrite", 873);
		return -1;
	}
	/* +0x40 = urb->buffer, same fix/reasoning as submit_urb_words above. */
	void *urb = (char *)n - 0x14;
	unsigned char *buf = (unsigned char *)*(void **)((char *)urb + 0x40);
	buf[0] = command;
	for (unsigned int i = 0; i < nBytes; i++)
		buf[i + 1] = data[i];
	unsigned int words = (nBytes + 4) >> 2;
	/* +0x50 = urb->length, same fix/reasoning as submit_urb_words above. */
	*(unsigned int *)((char *)urb + 0x50) = words * 4;
	ReverseMessage((unsigned int *)buf, words);
	/* mem_flags=0x20 (GFP_ATOMIC), same reasoning as submit_urb_words above. */
	int rc = vm_usb_submit_urb(urb, 0x20);
	if (rc == 0) {
		CActiveSenseThread_Ping();
		return 0;
	}
	printk("<6>OmapNKS4:%s: line %d: ERROR: SubmitOmapNKS4VideoWrite() fails %d\n",
	       "SubmitOmapNKS4CmdBulkWrite", 893, rc);
	return -1;
}

/* doReverse=false: ground truth (fresh objdump of the stock .ko, 2026-07-15
 * follow-up session) - the real SubmitOmapNKS4BulkWrite@0x127b0 is fully
 * inlined at its call site and writes the video/bulk words RAW (`rep movs
 * DWORD PTR es:[edi],DWORD PTR ds:[esi]` then a trailing `rep movs BYTE`),
 * exactly like the plain command-word path - zero `ror` instructions
 * anywhere in the function, and no call to any reversal routine. The
 * previous doReverse=true here was carried over unverified from the
 * command-word bug fix above ("hasn't been ground-truthed yet") - now
 * checked directly and confirmed wrong the same way. Cross-checked against
 * every `ror cx,8`/`ror ecx,0x10` site in the whole binary (there are only
 * four: ReverseMessage itself with 0 callers, SubmitOmapNKS4CmdBulkWrite's
 * own direct call below, and two receive/decode-side sites in
 * ReceiveEventBuffer/SetSTGInDownload/GetSPDIFClockError) - this function is
 * not among them.
 *
 * CORRECTION (fresh adversarial pass, 2026-07-18): this used to route through
 * submit_urb_words() with a word count of `(nBytes+3)>>2`, which rounds
 * nBytes UP to the next multiple of 4 for both the urb->length field and the
 * copy width. Ground truth (@0x12831-0x1284b) sets urb->length to the exact
 * byte count `nBytes` (untouched, `MOV [EBX+0x50],EAX` where EAX==nBytes) and
 * copies exactly nBytes bytes (`SHR ECX,2` dwords via REP MOVSD, then
 * `nBytes&3` remaining bytes via REP MOVSB) - never rounded up. Every current
 * real caller (video.cpp) always passes a multiple of 4 so this was latent/
 * inert in practice, but for a non-4-aligned nBytes the old code would have
 * set a too-large length and read up to 3 bytes past the end of 'data'. Given
 * this real byte-exact behavior differs from submit_urb_words()'s word-
 * oriented contract, this function now fills the buffer directly instead of
 * sharing that helper. */
/* CORRECTION (full-coverage re-audit, 2026-07-18): same three-printk-bug class as
 * SubmitOmapNKS4CmdBulkWrite just above, confirmed via fresh decompile of the real
 * SubmitOmapNKS4BulkWrite@0x127b0 plus search_strings against real .rodata:
 *  - "too long" text missing its real "%s: line %d:"/"fails -"/"!" framing (real
 *    string @0x1a148: "<6>OmapNKS4:%s: line %d: SubmitOmapNKS4VideoWrite() fails -
 *    message too long!\n", args ("SubmitOmapNKS4BulkWrite", 0x391=913) - note the
 *    literal text itself says "SubmitOmapNKS4VideoWrite()", not "...BulkWrite()",
 *    even from inside this function - the same real Korg copy-paste pattern as the
 *    "fails %d" string below);
 *  - "no free urbs" case had NO printk at all (real string @0x1a198:
 *    "<6>OmapNKS4:%s: line %d: SubmitOmapNKS4VideoWrite() fails - no free urbs\n",
 *    args ("SubmitOmapNKS4BulkWrite", 0x39b=923));
 *  - submit-failure path had NO printk at all (real string @0x1a100 - the SAME
 *    string SubmitOmapNKS4CmdBulkWrite's own submit-failure path uses, args
 *    ("SubmitOmapNKS4BulkWrite", 0x3a9=937, rc)). */
int SubmitOmapNKS4BulkWrite(unsigned int *data, unsigned int nBytes)
{
	if (nBytes > sMaxWritePacketSize) {
		printk("<6>OmapNKS4:%s: line %d: SubmitOmapNKS4VideoWrite() fails - message too long!\n",
		       "SubmitOmapNKS4BulkWrite", 913);
		return -1;
	}
	WaitForFreeBulkWriteURB(1);
	struct urb_node *n = pop_free_urb(&sBulkFreeVideoURBList, sVideoUrbFreeListLock,
					  &sBulkVideoURBsInUse);
	if (!n) {
		printk("<6>OmapNKS4:%s: line %d: SubmitOmapNKS4VideoWrite() fails - no free urbs\n",
		       "SubmitOmapNKS4BulkWrite", 923);
		return -1;
	}

	/* +0x40 = urb->buffer, +0x50 = urb->length - same offsets as
	 * submit_urb_words()/SubmitOmapNKS4CmdBulkWrite above. */
	void *urb = (char *)n - 0x14;
	unsigned char *buf = (unsigned char *)*(void **)((char *)urb + 0x40);
	*(unsigned int *)((char *)urb + 0x50) = nBytes;

	unsigned int nWords = nBytes >> 2;
	const unsigned int *src32 = data;
	unsigned int *dst32 = (unsigned int *)buf;
	for (unsigned int i = 0; i < nWords; i++)
		dst32[i] = src32[i];
	const unsigned char *srcTail = (const unsigned char *)(src32 + nWords);
	unsigned char *dstTail = buf + nWords * 4;
	for (unsigned int i = 0; i < (nBytes & 3); i++)
		dstTail[i] = srcTail[i];

	/* mem_flags=0x20 (GFP_ATOMIC), same reasoning as submit_urb_words above. */
	int rc = vm_usb_submit_urb(urb, 0x20);
	if (rc == 0) {
		CActiveSenseThread_Ping();
		return 0;
	}
	printk("<6>OmapNKS4:%s: line %d: ERROR: SubmitOmapNKS4VideoWrite() fails %d\n",
	       "SubmitOmapNKS4BulkWrite", 937, rc);
	return -1;
}

int SubmitOmapNKS4VideoWrite(unsigned int *data, unsigned int nBytes)
{
	return SubmitOmapNKS4BulkWrite(data, nBytes);
}

/* ---- aftertouch calibration -------------------------------------------- */
/*
 * Per-channel calibration applied to raw analog aftertouch (opcode 3).  The driver
 * keeps a calibration table (sCalibrationData) and an optional FPU-using callback;
 * returns 0xffff for an out-of-range / disabled reading.  (Generic curve handling is
 * shared with the engine; this is the panel-side entry.)
 */
/* NOT extern: previously declared extern expecting an external definition
 * that never existed anywhere ("Unknown symbol sCalibrationData" at insmod,
 * confirmed on real hardware, 2026-07-16). Real behavior (ApplyNKS4Calibration
 * below): NULL disables per-channel calibration entirely (every reading
 * passed through raw) - stays NULL here, same as this driver's likely
 * power-on-default state before any "set calibration table" command is ever
 * received (no such command is reconstructed/known to exist yet - see
 * docs/gaps.md), so this is a real, faithful default, not a placeholder
 * standing in for missing data. */
void *sCalibrationData;
/* Ground truth (fresh Ghidra decompile, 2026-07-17, SetupNKS4Calibration@0x17a00/
 * CleanupNKS4Calibration@0x17a10): an optional callback set alongside
 * sCalibrationData.
 * CORRECTION (fresh adversarial pass, 2026-07-18): the claim below this
 * comment used to make ("never actually invoked anywhere ... its exact real
 * purpose is unconfirmed") was WRONG - see ApplyNKS4Calibration's own header
 * comment further down. It IS invoked, unconditionally, once per call,
 * gating entry to the whole per-channel dispatch alongside sCalibrationData,
 * and is called as `sCalibrationMsgCallbackFunc(chan, raw)` (regparm3
 * EAX=chan, EDX=raw, confirmed @0x17a73-0x17a7a). Still modeled as an opaque
 * `void *` here (cast to a proper function-pointer type at the one call
 * site) rather than typed at the declaration, to avoid disturbing
 * SetupNKS4Calibration/CleanupNKS4Calibration's existing `void *` parameter
 * shape. */
static void *sCalibrationMsgCallbackFunc;

/* Ground truth: ApplyGenericCalibration.clone.0@0x17960 real disassembly,
 * 2026-07-16 - genuinely a LOCAL function in the stock module (GCC function
 * cloning, not evidence of an external module), not "a real function from a
 * different Korg module" as previously guessed. Per-channel curve table
 * layout (9 shorts = 18 bytes per channel, indexed by chan below): table[0]/
 * table[1] = low-segment thresholds, table[2]/table[3] = high-segment
 * thresholds, table[4..5] (as one float) = low-segment slope, table[6..7]
 * (as one float) = high-segment slope, table[8] = last-returned-value cache
 * (dedup: returns -1 instead of an unchanged value). _DAT_0001af40 = 512.0f
 * (read directly from the real binary's .rodata.cst4, confirmed via
 * read_memory) - a fixed-point centering offset for the high segment. */
/* CORRECTION (fresh adversarial pass, 2026-07-18): both range comparisons were
 * off by one against ground truth's real disassembly (@0x17960). Real code:
 * `if (*param_1 < param_2) { if (param_1[1] < param_2) {high} else {low} }
 * else sVar1=0` - i.e. the OUTER gate is `table[0] < raw` (strict), not
 * `raw < table[0]`, and the INNER gate is `table[1] < raw` (strict) selecting
 * the high segment, not `raw < table[1]` selecting the low segment. Net
 * effect: at raw==table[0] ground truth stays 0 (previously this reconstruction
 * computed a ramp value here too - though that value happens to equal 0
 * anyway since (raw-table[0])==0, so this specific boundary was value-
 * identical despite the wrong branch); at raw==table[1] ground truth takes the
 * LOW-segment ramp formula, but this reconstruction took the HIGH-segment
 * 0x200 baseline instead - a real, observable single-sample value bug at that
 * exact boundary, now fixed by using <= on both outer and inner gates. */
static int ApplyGenericCalibration(short *table, short raw)
{
	short result = 0;
	if (raw <= table[0]) {
		/* below range: stays 0 */
	} else if (raw <= table[1]) {
		result = (short)((float)(raw - table[0]) * *(float *)(table + 4));
	} else {
		result = 0x200;
		if (table[2] < raw) {
			result = 0x3ff;
			if (raw <= table[3])
				result = (short)((float)(raw - table[2]) * *(float *)(table + 6) + 512.0f);
		}
	}
	if (table[8] != result) {
		table[8] = result;
		return result;
	}
	return -1;
}

extern "C" void SetupNKS4Calibration(void *table, void *msgCallback)
{
	sCalibrationData = table;
	sCalibrationMsgCallbackFunc = msgCallback;
}

extern "C" void CleanupNKS4Calibration(void)
{
	sCalibrationData = 0;
	sCalibrationMsgCallbackFunc = 0;
}

/*
 * CORRECTION (fresh adversarial pass, 2026-07-18): this whole function was wrong.
 * Ground truth (fresh decompile + disassembly, ApplyNKS4Calibration@0x17a30, correct
 * 89849-byte target) is NOT a generic "table = sCalibrationData + chan*9" lookup
 * applied uniformly to any channel - it's a hardcoded per-channel-NUMBER dispatch:
 *
 *  - Gate: proceeds only when BOTH sCalibrationData AND sCalibrationMsgCallbackFunc
 *    are non-NULL (`MOV EDX,[sCalibrationData]; TEST; JZ skip` then the same for
 *    sCalibrationMsgCallbackFunc @0x17a57-0x17a5e) - not sCalibrationData alone.
 *    Previous header comment's claim that the callback field is "never actually
 *    invoked anywhere" is WRONG, corrected below - the gate itself references it,
 *    and it IS called (see next point).
 *  - When gated in: real code does a manual FPU-enable dance (CLTS/FXSAVE/FNINIT
 *    @0x17a60-0x17a71, matching mirror FXRSTOR + conditional CR0.TS restore @0x17a85-
 *    0x17a9e) around the float math below - this function's real callers
 *    (ReceiveEventBuffer/COmapNKS4Driver_ReceiveEventBuffer) run from the USB
 *    interrupt-completion path, unlike this module's other two float use sites
 *    (CActiveSenseThread's ctor, SetProgressBarPercent) which run in normal
 *    thread/RT-task context and need no such wrapping under this file's
 *    -mhard-float override.
 *  - `sCalibrationMsgCallbackFunc` IS called, unconditionally, once, BEFORE the
 *    per-channel dispatch (`MOV EAX,ESI(chan); MOV EDX,EBX(raw); CALL
 *    [sCalibrationMsgCallbackFunc]` @0x17a73-0x17a7a) - real regparm3 (chan,raw).
 *  - Per-channel dispatch is a real jump table (@0x19600, 25 entries, index=chan-5,
 *    read via read_memory and cross-checked against every distinct code block it
 *    points at):
 *      chan==5  -> ApplyGenericCalibration(sCalibrationData+0x5c, raw)   (@0x17b38)
 *      chan==6  -> ApplyGenericCalibration(sCalibrationData+0x70, raw)   (@0x17b20)
 *      chan==0x1d -> ApplyGenericCalibration(sCalibrationData+0x84, raw) (@0x17ab8)
 *      chan in [0x10..0x18] (9 channels) -> direct scale/offset formula, no table:
 *          result = raw>0x3f5 ? 0x3ff : (short)((float)raw*SCALE + OFFSET)  (@0x17ae8)
 *      chan==0x1b -> sFootPedalMappingTable[(unsigned)raw >> 2]            (@0x17ad0)
 *      any other chan (incl. <5) -> falls straight through to the common tail,
 *          i.e. the function's default return value (raw, untouched) - every
 *          unlisted jump-table slot points directly at the tail block @0x17a85.
 *    The three ApplyGenericCalibration offsets (0x5c/0x70/0x84, each 20 bytes
 *    apart) prove this is NOT the previous "chan*9(=18 bytes)" formula - these
 *    are three independent fixed slots for channels 5, 6, and 0x1d specifically,
 *    not a uniformly-strided table indexed by channel number.
 *  - SCALE/OFFSET are the exact real float32 bit patterns at the real binary's
 *    .rodata (0x1af44/0x1af48, confirmed via read_memory): 0x3f81437a
 *    (~1.00987f) and 0x3f000000 (0.5f exactly) - the +0.5 is the classic
 *    truncate-toward-zero-becomes-round-to-nearest trick, matching the real
 *    FISTTP (truncating store) instruction used here.
 *  - sFootPedalMappingTable is a real 256-entry `short` lookup table baked into
 *    the binary's own .rodata at a fixed address (0x19680) - NOT part of the
 *    externally-supplied sCalibrationData blob. Transcribed verbatim below via
 *    read_memory (256 x 2 bytes = 512 bytes, confirmed exact table extent: the
 *    curve is 0 for a deadzone, ramps monotonically, and saturates at 0x3ff for
 *    the remaining high indices - a complete, sensible foot-pedal response
 *    curve, not a truncated read).
 *  - Every real per-case result is only ever the callee's LOW 16 BITS, sign-
 *    extended (`MOVSX EBX,AX` after each ApplyGenericCalibration call) - this
 *    also means ApplyGenericCalibration's own -1 "unchanged from cache" sentinel
 *    propagates out of ApplyNKS4Calibration as -1 too (0xFFFF sign-extends to
 *    -1 the same as any other short), not as the original raw value - captured
 *    below by assigning the (short) truncation directly, matching that MOVSX.
 */
typedef void (*nks4_calib_msg_fn)(unsigned int chan, short raw);

static inline float nks4_bits_to_float(unsigned int bits)
{
	union { unsigned int u; float f; } conv;
	conv.u = bits;
	return conv.f;
}

/* real .rodata @0x1af44/0x1af48 - see header comment above. */
#define NKS4_AFTERTOUCH_SCALE_BITS  0x3f81437au	/* ~1.00987f */
#define NKS4_AFTERTOUCH_OFFSET_BITS 0x3f000000u	/* 0.5f exactly */

/* real .rodata @0x19680, 256 entries x 2 bytes, read_memory-verified 2026-07-18 -
 * indexed by (unsigned)raw >> 2 (raw is a 10-bit-range ADC reading, 0..1023). */
static const short sFootPedalMappingTable[256] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0008, 0x0010, 0x0018, 0x0020, 0x0028, 0x0030,
	0x0038, 0x0040, 0x0048, 0x0048, 0x0050, 0x0058, 0x0058, 0x0060,
	0x0069, 0x0069, 0x0071, 0x0071, 0x0079, 0x0081, 0x0081, 0x0089,
	0x0089, 0x0091, 0x0099, 0x0099, 0x00a1, 0x00a1, 0x00a9, 0x00b1,
	0x00b1, 0x00b9, 0x00b9, 0x00c1, 0x00c9, 0x00c9, 0x00d1, 0x00d1,
	0x00d9, 0x00e1, 0x00e1, 0x00e9, 0x00e9, 0x00f1, 0x00f9, 0x00f9,
	0x0102, 0x0102, 0x010a, 0x0112, 0x0112, 0x011a, 0x011a, 0x0122,
	0x012a, 0x012a, 0x0132, 0x0132, 0x013a, 0x0142, 0x0142, 0x014a,
	0x014a, 0x0152, 0x015a, 0x015a, 0x0162, 0x0162, 0x016a, 0x0172,
	0x0172, 0x017a, 0x017a, 0x0182, 0x018a, 0x018a, 0x0192, 0x0192,
	0x019b, 0x01a3, 0x01a3, 0x01ab, 0x01ab, 0x01b3, 0x01bb, 0x01bb,
	0x01c3, 0x01c3, 0x01cb, 0x01d3, 0x01d3, 0x01db, 0x01db, 0x01e3,
	0x01eb, 0x01eb, 0x01f3, 0x01f3, 0x01fb, 0x0203, 0x0203, 0x020b,
	0x020b, 0x0213, 0x021b, 0x021b, 0x0223, 0x0223, 0x022b, 0x0233,
	0x0234, 0x023c, 0x023c, 0x0244, 0x024c, 0x024c, 0x0254, 0x0254,
	0x025c, 0x0264, 0x0264, 0x026c, 0x026c, 0x0274, 0x027c, 0x027c,
	0x0284, 0x0284, 0x028c, 0x0294, 0x0294, 0x029c, 0x029c, 0x02a4,
	0x02ac, 0x02ac, 0x02b4, 0x02b4, 0x02bc, 0x02c4, 0x02c4, 0x02cc,
	0x02cd, 0x02d5, 0x02dd, 0x02dd, 0x02e5, 0x02e5, 0x02ed, 0x02f5,
	0x02f5, 0x02fd, 0x02fd, 0x0305, 0x030d, 0x030d, 0x0315, 0x0315,
	0x031d, 0x0325, 0x0325, 0x032d, 0x032d, 0x0335, 0x033d, 0x033d,
	0x0345, 0x0345, 0x034d, 0x0355, 0x0355, 0x035d, 0x035d, 0x0365,
	0x036e, 0x036e, 0x0376, 0x0376, 0x037e, 0x0386, 0x0386, 0x038e,
	0x038e, 0x0396, 0x039e, 0x039e, 0x03a6, 0x03a6, 0x03ae, 0x03b6,
	0x03b6, 0x03be, 0x03be, 0x03c6, 0x03ce, 0x03ce, 0x03d6, 0x03d6,
	0x03de, 0x03e6, 0x03e6, 0x03ee, 0x03ee, 0x03f6, 0x03f6, 0x03ff,
	0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff,
	0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff,
	0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff,
	0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff,
};

/* real fixed FPU-enable dance around this function's float math (ground truth
 * @0x17a60-0x17a71 / mirrored @0x17a85-0x17a9e) - see header comment above.
 * Operates on a dedicated static save area rather than the real binary's own
 * fixed absolute link addresses (0x1f2c0/0x1f600), which are just this
 * particular build's own layout and not meaningful to reproduce literally. */
static unsigned char sNKS4CalibFpuSaveArea[512] __attribute__((aligned(16)));

static inline unsigned long nks4_calib_fpu_begin(void)
{
	unsigned long cr0;
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	asm volatile("clts");
	asm volatile("fxsave %0" : "=m"(sNKS4CalibFpuSaveArea));
	asm volatile("fninit");
	return cr0;
}

static inline void nks4_calib_fpu_end(unsigned long saved_cr0)
{
	asm volatile("fxrstor %0" : : "m"(sNKS4CalibFpuSaveArea));
	if (saved_cr0 & 0x8) {	/* CR0.TS was set before we CLTS'd - restore it */
		unsigned long cr0;
		asm volatile("mov %%cr0, %0" : "=r"(cr0));
		cr0 |= 0x8;
		asm volatile("mov %0, %%cr0" : : "r"(cr0));
	}
}

extern "C" int ApplyNKS4Calibration(unsigned int chan, short raw)
{
	int result = raw;	/* default: pass-through for any unhandled channel */

	if (!sCalibrationData || !sCalibrationMsgCallbackFunc)
		return result;

	unsigned long saved_cr0 = nks4_calib_fpu_begin();
	((nks4_calib_msg_fn)sCalibrationMsgCallbackFunc)(chan, raw);

	switch (chan) {
	case 5:
		result = (short)ApplyGenericCalibration(
			(short *)((char *)sCalibrationData + 0x5c), raw);
		break;
	case 6:
		result = (short)ApplyGenericCalibration(
			(short *)((char *)sCalibrationData + 0x70), raw);
		break;
	case 0x1d:
		result = (short)ApplyGenericCalibration(
			(short *)((char *)sCalibrationData + 0x84), raw);
		break;
	case 0x10: case 0x11: case 0x12: case 0x13: case 0x14:
	case 0x15: case 0x16: case 0x17: case 0x18:
		if (raw > 0x3f5) {
			result = 0x3ff;
		} else {
			float scale = nks4_bits_to_float(NKS4_AFTERTOUCH_SCALE_BITS);
			float offset = nks4_bits_to_float(NKS4_AFTERTOUCH_OFFSET_BITS);
			result = (short)((float)raw * scale + offset);
		}
		break;
	case 0x1b:
		/* ground truth: SAR DI,2 (16-bit arithmetic shift) then MOVSX EDI,DI -
		 * plain signed `raw >> 2` on the already-16-bit `short` reproduces
		 * this exactly (int promotion preserves sign/magnitude). */
		result = sFootPedalMappingTable[raw >> 2];
		break;
	default:
		/* every other jump-table slot (incl. chan<5) points straight at the
		 * common tail in ground truth - result stays the pass-through raw. */
		break;
	}

	nks4_calib_fpu_end(saved_cr0);
	return result;
}
