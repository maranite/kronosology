// SPDX-License-Identifier: GPL-2.0
/*
 * usb.c  -  USB driver glue for the NKS4 panel (vendor 0x0944, product 0x1005).
 *
 *   - OmapNKS4Probe       : bind the device, discover endpoints (1 interrupt-IN +
 *                           1 bulk-OUT), bring up the subsystems, build the URB pools
 *                           (1 interrupt URB, 16 command URBs, 256 video URBs).
 *   - OmapNKS4Disconnect  : tear the device + URB pools down.
 *   - WriteCallback       : bulk-OUT completion -> return URB to its free list.
 *   - InterruptCallback   : interrupt-IN completion -> ReceiveEventBuffer + re-submit.
 *   - WaitForFreeBulkWriteURB / CleanupOmapNKS4Driver.
 *
 * URB struct field offsets used (Linux 2.6.32 struct urb):
 *   +0x14 free-list node (next/prev)  +0x28 dev      +0x30 pipe   +0x38 status
 *   +0x3c transfer_flags              +0x40 buffer   +0x50 length +0x60 start_frame
 *   +0x68 interval                    +0x70 context-tag (0=command,1=video)
 */

#include "omapnks4_internal.h"

/* Forward declaration only, deliberately NOT <linux/usb.h> - see omapnks4.h's own
 * comment for why that header is unusable from a C++ translation unit on this
 * kernel. OmapNKS4Probe's own body (below) never dereferences a real field of
 * this type - it immediately reinterprets the pointer as raw ints (the struct's
 * real layout was recovered as offsets, not field names, from the binary) - and
 * this module never constructs a real `struct usb_driver`/`usb_device_id` itself
 * (that's handled by the STG framework's own `stg_usb_register_driver()`), so an
 * opaque pointer type is all this translation unit actually needs. */
struct usb_interface;

/* ---- URB pools (free lists are intrusive doubly-linked, head points to self) --- */
/* struct urb_node's definition moved to omapnks4_internal.h - submit.cpp needs it
 * too (found via a real Kbuild build attempt, 2026-07-15: "invalid use of
 * incomplete type" against the extern forward declarations submit.cpp already
 * had). */
/* NOT static: submit.cpp references all four of these as extern (see its own
 * "usb.c" comments on the matching declarations) - `static` here gave
 * them internal linkage invisible to submit.cpp's extern references, so they
 * silently became genuinely unresolved symbols at insmod time ("Unknown
 * symbol sBulkFreeCommandURBList" etc, confirmed on real hardware,
 * 2026-07-16) despite building without error (ld -r doesn't require externs
 * to resolve at the module's own internal link step). */
struct urb_node *sBulkFreeCommandURBList;
/* CORRECTION (full-coverage sweep, 2026-07-18): WriteCallback's free-list push
 * (below) performs a genuine, UNCONDITIONAL hlist_add_head() - fresh
 * disassembly (@0x10098/0x1014f) shows no NULL/self-pointer check at all
 * before executing `old_head->pprev = &new_node`. Since this project's own
 * "empty" sentinel is the head pointer's own address (see free_all_urbs()/
 * WaitForFreeBulkWriteURB below - already an established, correct
 * convention), pushing onto an EMPTY list writes 4 bytes past the head
 * pointer itself. Ground truth's real binary confirms a dedicated slot
 * really exists there, not just adjacent unrelated data:
 * sBulkFreeCommandURBList sits at 0x1b010, sBulkFreeVideoURBList at 0x1b018 -
 * an 8-byte gap, not 4 - and get_xrefs_to on 0x1b010 shows BOTH 0x1b010 and
 * 0x1b014 carry a real, compiled-in self-referencing pointer value of
 * 0x1b010 (confirmed identically for the video list: 0x1b018/0x1b01c both
 * self-reference 0x1b018). I.e. each free-list head is a real 8-byte
 * urb_node-shaped pair in ground truth (next=&self, pprev=&self.next), not a
 * bare pointer - this reconstruction's own bare-pointer declaration was
 * missing the second (pprev) slot entirely. Nothing anywhere reads this slot
 * back meaningfully (no READ xref targets 0x1b014/0x1b01c in the real
 * binary, only the one write-on-empty-push) - it is genuinely write-only
 * padding, so a plain reserved field (never read by this reconstruction's
 * own logic either) is a faithful, minimal fix: without it, the very first
 * WriteCallback push after a pool is fully drained (all 16 command URBs or
 * all 256 video URBs simultaneously in flight) would silently clobber
 * whichever global the compiler happens to place next - sBulkFreeVideoURBList
 * immediately follows sBulkFreeCommandURBList in this file's own declaration
 * order, so that corruption was a real, live risk before this fix, not a
 * theoretical one. */
static void *sBulkFreeCommandURBListPad;
struct urb_node *sBulkFreeVideoURBList;
static void *sBulkFreeVideoURBListPad;
int sBulkCommandURBsInUse, sBulkVideoURBsInUse;
/* CORRECTION (re-verification pass, 2026-07-17): this pool is 16 entries in
 * the real binary, not 15 - the real alloc loop's bound check
 * (`0xf < iVar7`) only fires once the counter reaches 16, and every
 * teardown/failure path (OmapNKS4Disconnect, CleanupOmapNKS4Driver, all
 * three OmapNKS4Probe failure exits) zeroes 16 consecutive dwords, not 15.
 * See alloc_command_urbs/free_all_urbs below for the matching count fixes. */
static void *sBulkCommandURBs[16];	/* command URB pool */
static void *sBulkVideoURBs[256];	/* video URB pool   */
/* NOT static: main.cpp's OmapNKS4Init reads this (stg_usb_submit_urb(sInterruptURB,
 * 0), to start the interrupt-IN transfer) right after probe succeeds - same
 * duplicate-static bug as sDriverState above, confirmed on real hardware,
 * 2026-07-16 (main.cpp's own separate copy stayed NULL forever since only
 * usb.cpp's probe success path ever set it). */
void *sInterruptURB;
static unsigned int sInterruptURBInterval;

/* Command/video wait queues - real wait_queue_head_t-sized storage (12 bytes,
 * matching OA's own already-boot-tested `wakeQueue[0xc]` convention, see
 * omapnks4_internal.h's own comment). WaitForFreeBulkWriteURB (below) currently
 * polls via stg_msleep(20) instead of actually blocking on these - a pre-existing,
 * separately-documented simplification (see that function's own comment) - so
 * __wake_up()ing them here is a currently-harmless no-op, not yet load-bearing.
 * Initialized via __init_waitqueue_head() in OmapNKS4Probe. */
static unsigned char sCommandWaitQueue[0xc];
static unsigned char sVideoWaitQueue[0xc];
static unsigned char sReadWaitQueue[0xc];
static const char sWaitQueueLockKeyDummy[1] = {0};

/* NOT static: main.cpp's OmapNKS4Init reads this after wait_for_completion_timeout
 * to check probe's actual result - both files previously had their OWN separate
 * `static int sDriverState`, so usb.cpp's probe success path was setting a
 * completely different variable than the one main.cpp checked, which stayed 0
 * forever ("driver state is 0" even after a real "probe success" printk,
 * confirmed on real hardware, 2026-07-16). */
int sDriverState;			/* 0 none, 1 up, 2 disconnected, 3 probing */
static int sDisconnect;
static int *sDeviceInstance;
unsigned int sMaxWritePacketSize;
/* NOT static: main.cpp's OmapNKS4Init sets this (rt_request_srq's return
 * value); usb.cpp's own cleanup path (rt_free_srq(sSTG2NKS4SrqNumber)) needs
 * the SAME value, not its own separate always-zero copy - same duplicate-
 * static bug as sDriverState/sInterruptURB above, confirmed on real
 * hardware, 2026-07-16. */
int sSTG2NKS4SrqNumber;
/* VM-only: set by vm_usb_submit_urb (below), consumed by WaitForNKS4ReadEvent
 * (submit.cpp) - see both functions' own comments for why the reply can't be
 * delivered synchronously at submit time. */
unsigned int sVmPendingReply;
int sVmPendingReplyValid;
char sCommandWatermarkWaiter, sVideoWatermarkWaiter;
extern int sDoingWait4Write;		/* submit.c */
extern unsigned char sReadEventWaitQueue[0xc];		/* submit.c */
extern unsigned char sAtmelReadWaitQueue[0xc];		/* submit.c */
extern unsigned char sVideoMsgWaitQueue[0xc];		/* submit.c */
extern unsigned char sShutdownSsdWaitQueue[0xc];	/* submit.c */

/* proc handles (procfs.c) */
extern void *gProc, *gProcProgress, *gProcHardwareVersion, *gProcOmapVersion;

/* event queue (procfs.c) */
extern unsigned int sEventQueue[256], sEventQueueReadIndex, sEventQueueWriteIndex,
		    sNumEventsInQueue, sReadIndexModLock;

/* ---- the URB completion handlers (the byte at urb+0x70 tags command vs video) -- */

void WriteCallback(struct urb *urb)
{
	int status = *(int *)((char *)urb + 0x38);
	/* REMOVED (Opus review, 2026-07-18): this "TEMP diagnostic" printk (dating
	 * to a 2026-07-16 debug session) does not exist in ground truth - fresh
	 * disassembly of WriteCallback@0x10040 proceeds straight from reading
	 * status to the status!=0 test, no printk call before it. Fires on every
	 * command/video URB completion in interrupt/completion context - a real
	 * fidelity gap on one of this module's most frequently-hit callbacks, not
	 * just cosmetic. Same class of finding as the "TEMP diagnostic"/"TEMP
	 * EXPERIMENT" printks already removed from main.cpp/submit.cpp in earlier
	 * sessions - this one was missed by those sweeps. */

	if (status != 0) {
		/* Ground truth (fresh disassembly, 2026-07-18, WriteCallback@0x10190):
		 * ALL THREE branches below converge on one more unconditional printk
		 * (format string @0x1a869, no trailing newline) before status is
		 * cleared - this second call was missing entirely from the previous
		 * version. Message text also re-verified exact against ground truth
		 * (the real strings carry the "%s: line %d:" prefix and a double
		 * space before "ERROR", unlike the previous shortened text). */
		if (status == -104)	printk("<6>OmapNKS4:%s: line %d:  ERROR: urb wc CONNRESET\n", "WriteCallback", 0x27f);
		else if (status == -2)	printk("<6>OmapNKS4:%s: line %d:  ERROR: urb wc NOENT\n", "WriteCallback", 0x27c);
		else			printk("<6>OmapNKS4:%s: line %d:  ERROR: urb wc status %02x\n", "WriteCallback", 0x282, status);
		printk(" ERROR: urb wc status %02x", status);
		*(int *)((char *)urb + 0x38) = 0;
		return;
	}

	struct urb_node *node = (struct urb_node *)((char *)urb + 0x14);
	if (*(int *)((char *)urb + 0x70) == 1) {		/* video URB */
		/* push onto sBulkFreeVideoURBList under irqsave */
		sBulkVideoURBsInUse--;
		node->next = sBulkFreeVideoURBList;
		node->pprev = &sBulkFreeVideoURBList;
		sBulkFreeVideoURBList = node;
		if (sVideoWatermarkWaiter)
			/* mode=3 (TASK_INTERRUPTIBLE|TASK_UNINTERRUPTIBLE), nr_exclusive=1,
			 * key=0 - matches OA's own already-boot-tested __wake_up call
			 * shape (daemon_lifecycle.cpp) for this exact "wake one waiter"
			 * pattern. */
			__wake_up(sVideoWaitQueue, 3, 1, 0);
	} else {						/* command URB */
		/* Ground truth (fresh disassembly, 2026-07-18, WriteCallback@0x10063):
		 * passes EAX=urb->transfer_buffer (+0x40), EDX=urb->actual_length>>2
		 * (+0x50, byte length converted to word count) - NOT a no-arg call as
		 * previously coded. */
		COmapNKS4Driver_NotifyTransmittedCommandComplete(
			*(NKS4Command **)((char *)urb + 0x40),
			*(unsigned int *)((char *)urb + 0x50) >> 2);
		sBulkCommandURBsInUse--;
		node->next = sBulkFreeCommandURBList;
		node->pprev = &sBulkFreeCommandURBList;
		sBulkFreeCommandURBList = node;
		if (sCommandWatermarkWaiter)
			__wake_up(sCommandWaitQueue, 3, 1, 0);
		if (sBulkCommandURBsInUse == 0 && sDoingWait4Write) {
			sDoingWait4Write = 0;
			/* CORRECTION (full-coverage sweep, 2026-07-18): ground truth
			 * (fresh disassembly, @0x100e2) uses queue address 0x1b668 for
			 * THIS specific wake_up, not the same 0x1b674 the
			 * sCommandWatermarkWaiter branch above it uses (confirmed
			 * distinct - two different immediates loaded into EAX at two
			 * different call sites in the same function). 0x1b668 is
			 * submit.cpp's own sReadEventWaitQueue - already independently
			 * confirmed there (get_xrefs_to on 0x1b668 now shows 9 real
			 * xrefs, one more than the "8 total, no others" that pass
			 * found, this WriteCallback site being the one it hadn't looked
			 * at) as the SAME queue WaitForNKS4CommandWrite blocks on
			 * (submit.cpp: "There is no separate command write done queue
			 * in the real binary"). Makes sense: sDoingWait4Write is
			 * cleared here specifically to unblock WaitForNKS4CommandWrite,
			 * which sleeps on sReadEventWaitQueue, not this file's own
			 * sCommandWaitQueue (that one is only for
			 * WaitForFreeBulkWriteURB's free-URB-available wait, address
			 * 0x1b674, per the watermark-waiter wake just above). Previous
			 * version woke the wrong queue here, meaning a real caller
			 * blocked in WaitForNKS4CommandWrite's final wait would never
			 * be promptly woken by the last in-flight command URB
			 * completing - only recovered via its own 10-jiffy poll retry,
			 * not immediately as ground truth intends. */
			__wake_up(sReadEventWaitQueue, 3, 1, 0);
		}
	}
}

void InterruptCallback(struct urb *urb)
{
	int status = *(int *)((char *)urb + 0x38);
	/* REMOVED (Opus review, 2026-07-18): the unconditional "fired" printk and
	 * the per-byte raw-bytes dump loop below (both "TEMP diagnostic" from a
	 * 2026-07-16 debug session) do not exist in ground truth - fresh
	 * disassembly of InterruptCallback@0x10220 proceeds straight from reading
	 * status to the status==-115||status==0 test, no printk call before it,
	 * and no byte-dump loop anywhere in the real function. This fires on
	 * every interrupt-IN completion; the per-byte dump loop in particular
	 * issued up to 64 separate printk() calls per real event packet from
	 * interrupt/completion context - a real fidelity gap (not just cosmetic)
	 * on this module's most latency-sensitive callback. Same class of finding
	 * as the "TEMP diagnostic"/"TEMP EXPERIMENT" printks already removed from
	 * main.cpp/submit.cpp/WriteCallback (above) in earlier sessions - this
	 * one was missed by those sweeps. */

	if (status == -115 || status == 0) {	/* -EINPROGRESS or OK */
		unsigned int len = *(unsigned int *)((char *)urb + 0x54);
		if (len != 0) {
			unsigned char *buf =
				(unsigned char *)*(void **)((char *)urb + 0x40);
			COmapNKS4Driver_ReceiveEventBuffer(buf, len / 4);
		}
		*(unsigned int *)((char *)urb + 0x68) = sInterruptURBInterval;
		/* mem_flags=0x20 (GFP_ATOMIC) - ground truth: real
		 * InterruptCallback@0x10244 disassembly - this resubmit runs from
		 * interrupt/completion context. */
		vm_usb_submit_urb(urb, 0x20);
	} else {
		/* CORRECTION (full-coverage sweep, 2026-07-18): ground truth
		 * (fresh disassembly, @0x1026e-0x10289) sets up FOUR printk args, not
		 * one - the real format string (confirmed via read_memory @0x19a18)
		 * carries the same "%s: line %d:" prefix used throughout this file's
		 * other diagnostics, not a bare one-line message. Name string
		 * "InterruptCallback" @0x190af, line=0x43e (1086). */
		printk("<6>OmapNKS4:%s: line %d: InterruptCallback() urb->status %d\n",
		       "InterruptCallback", 0x43e, status);
	}
}

/* Real i386 wait_queue_t, mirroring submit.cpp's own already-established
 * struct omap_wait_entry (flags/private/func/task_list{next,prev}, 20 bytes) -
 * duplicated locally here (distinctly named) rather than shared via the
 * header, since usb.cpp and submit.cpp are separate translation units and
 * this type is submit.cpp-local there too. Confirmed byte-for-byte against
 * this function's own disassembly (below), not just carried over by
 * analogy: field offsets +0x00/+0x04/+0x08/+0x0c/+0x10 exactly match the
 * [esp+0x2c]/[esp+0x30]/[esp+0x34]/[esp+0x38]/[esp+0x3c] stack-slot pattern
 * WaitForFreeBulkWriteURB itself uses (see below). */
struct nks4_usb_wait_entry {
	unsigned int flags;
	void *priv;
	void (*func)(void);
	void *next, *prev;
};

/* Same real kernel wake-callback DEFINE_WAIT() defaults to - see submit.cpp's
 * own comment on its identical construction for why this is the one
 * non-disassembly-confirmed part (the func constant is an unnamed data
 * relocation, @0x20148 here too, absent from get_imports - autoremove_wake_function
 * is the only plausible real kernel symbol for this shape). */
extern "C" void autoremove_wake_function(void);

/* Real per-CPU current-task read (%fs:per_cpu__current_task) - the real
 * shipped binary's own disassembly shows the literal `64 a1 68 01 02 00`
 * (MOV EAX,FS:[0x20168]) opcode bytes at both of this function's call sites,
 * the same idiom submit.cpp's own current-task accessor uses for this exact
 * wait_queue_t construction.
 *
 * CORRECTION (live VM boot test, 2026-07-18): the hardcoded 0x20168 literal
 * above is that ONE SHIPPED BINARY's own fixed per-cpu offset, not a portable
 * constant - confirmed the hard way, a real "BUG: unable to handle kernel
 * paging request" oops at this exact `mov %fs:0x20168,%eax` instruction on
 * this project's own /home/build/linux-kronos build (crash address
 * 0x0266c168 = this run's actual FS.base + 0x20168, landing outside that
 * build's real per-cpu area - the offset genuinely doesn't match this
 * project's own kernel build/config, only the original shipped module's).
 * submit.cpp's own current-task accessor (WaitForNKS4ReadEvent's
 * init_wait_entry) and main.cpp's stg_get_current_task_nks4() both already
 * establish the correct, portable fix for this exact idiom in this same
 * codebase - reference the real, EXPORT_PER_CPU_SYMBOL'd symbol name
 * `per_cpu__current_task` and let the assembler/linker resolve the real
 * per-cpu offset for whatever kernel this actually links/loads against,
 * rather than a baked-in numeric literal. This function was the one place
 * that established pattern hadn't been applied - fixed to match. */
static inline void *nks4_get_current_task_usb(void)
{
	void *task;
	__asm__ __volatile__("mov %%fs:per_cpu__current_task, %0" : "=r"(task));
	return task;
}

/* Block until a free command (or video, type==1) write URB is available.
 *
 * CORRECTION (full-coverage sweep, 2026-07-18): ground truth (fresh
 * disassembly, @0x10290-0x10449) is NOT a stg_msleep(20) poll - this file's
 * own prior comment already flagged that as a known simplification, and a
 * dedicated stray disassembly check during the submit.cpp wait-helper audit
 * (2026-07-18) confirmed the real shape but left the actual fix to a later
 * pass ("both real, not yet fixed"). This is that pass. Real structure,
 * identical for both the video (queue 0x1b680) and command (queue 0x1b674)
 * cases, and the SAME retry/logging pattern already established for
 * WaitForNKS4CommandWrite (submit.cpp): fetch current_task and set the
 * watermark-waiter flag once; if the free list is already non-empty, clear
 * the flag and return immediately (no wait ever entered - matches the real
 * fast-path branch at 0x102cb/0x102d1). Otherwise enter an outer 0x33=51-
 * iteration retry loop; each outer iteration builds a fresh DEFINE_WAIT()-
 * style wait_queue_t and repeatedly calls prepare_to_wait/checks the
 * condition/schedule_timeout(10 jiffies, re-arming with the remaining budget
 * on each pass) until either the condition is satisfied or the 10-jiffy
 * budget is fully exhausted, then finish_wait and a final re-check - success
 * at any point clears the watermark flag and returns directly (0x10309/
 * 0x102d1... via 0x10428/0x10390's post-finish_wait jnz). Exhausting all 51
 * outer iterations does NOT fail the function (it has no error return) - it
 * logs a bare, unprefixed "waiting for Free Bulk <Video|Command> URB\n"
 * (confirmed via read_memory @0x19a80/0x19aa4 - no "<6>OmapNKS4:" prefix or
 * %s/%d args, unlike every other message in this file, matching
 * WaitForNKS4CommandWrite's own already-documented bare-string style) and
 * restarts the ENTIRE outer wait from scratch (jmp back to the original
 * empty-list check, not re-fetching current_task or re-setting the
 * watermark flag - both already correct/unchanged) - i.e. this function
 * blocks indefinitely, however long it takes, logging every ~510 jiffies. */
void WaitForFreeBulkWriteURB(int type)
{
	if (sDisconnect)
		return;

	if (type == 1) {
		void *current_task = nks4_get_current_task_usb();
		sVideoWatermarkWaiter = 1;
		while (sBulkFreeVideoURBList == (struct urb_node *)&sBulkFreeVideoURBList) {
			int give_up = 1;
			for (int outer = 0; outer < 0x33; outer++) {
				struct nks4_usb_wait_entry wait;
				long remaining = 0xa;
				wait.flags = 0;
				wait.priv = current_task;
				wait.func = autoremove_wake_function;
				wait.next = wait.prev = &wait.next;
				do {
					prepare_to_wait(sVideoWaitQueue, &wait, 2 /* TASK_UNINTERRUPTIBLE */);
					if (sBulkFreeVideoURBList != (struct urb_node *)&sBulkFreeVideoURBList)
						break;
					remaining = schedule_timeout(remaining);
				} while (remaining != 0);
				finish_wait(sVideoWaitQueue, &wait);
				if (sBulkFreeVideoURBList != (struct urb_node *)&sBulkFreeVideoURBList) {
					give_up = 0;
					break;
				}
			}
			if (give_up)
				printk("waiting for Free Bulk Video URB\n");
		}
		sVideoWatermarkWaiter = 0;
	} else {
		void *current_task = nks4_get_current_task_usb();
		sCommandWatermarkWaiter = 1;
		while (sBulkFreeCommandURBList == (struct urb_node *)&sBulkFreeCommandURBList) {
			int give_up = 1;
			for (int outer = 0; outer < 0x33; outer++) {
				struct nks4_usb_wait_entry wait;
				long remaining = 0xa;
				wait.flags = 0;
				wait.priv = current_task;
				wait.func = autoremove_wake_function;
				wait.next = wait.prev = &wait.next;
				do {
					prepare_to_wait(sCommandWaitQueue, &wait, 2 /* TASK_UNINTERRUPTIBLE */);
					if (sBulkFreeCommandURBList != (struct urb_node *)&sBulkFreeCommandURBList)
						break;
					remaining = schedule_timeout(remaining);
				} while (remaining != 0);
				finish_wait(sCommandWaitQueue, &wait);
				if (sBulkFreeCommandURBList != (struct urb_node *)&sBulkFreeCommandURBList) {
					give_up = 0;
					break;
				}
			}
			if (give_up)
				printk("waiting for Free Bulk Command URB\n");
		}
		sCommandWatermarkWaiter = 0;
	}
}

/* Ground truth (fresh Ghidra decompile, 2026-07-15, OmapNKS4Probe@0x11180): the real
 * binary has NO separate helper functions for this at all - the interrupt-URB
 * config and the 15+256 command/video URB allocation loops are fully INLINED into
 * OmapNKS4Probe itself. This reconstruction's own decision to factor them into
 * named helpers (calls already present in OmapNKS4Probe below) is a reasonable
 * abstraction, but the bodies were never actually written - found while fixing a
 * real Kbuild build attempt. Implemented here directly from the decompiled logic,
 * kept as close to the real field-level operations as possible rather than
 * "cleaned up" into named pipe-macro semantics I can't independently confirm.
 *
 * Pipe-value encoding: `(devVal<<8) | (epAddr<<0xf) | 0x40000080` (interrupt) /
 * `(devVal<<8) | 0xc0000000 | (epAddr<<0xf)` (bulk) - devVal is the int at
 * *sDeviceInstance (NOT the raw `dev` parameter - ground truth reads through the
 * global, which is already set by the time these run), matching real
 * usb_rcvintpipe()/usb_sndbulkpipe() bit-packing (direction/type in the high bits,
 * device number << 8, endpoint << 15) - reproduced as the literal bit operations
 * ground truth uses, not reimplemented via the macros (which need the real
 * struct usb_device this file deliberately avoids - see omapnks4.h's own comment).
 */
static void configure_interrupt_urb(void *urb, void *intEp, void *buf, int dev)
{
	unsigned short wMaxPacketSize = *(unsigned short *)((char *)intEp + 4);
	unsigned char  bInterval      = *(unsigned char  *)((char *)intEp + 6);
	unsigned char  epAddr         = *(unsigned char  *)((char *)intEp + 2);
	int            devVal         = *sDeviceInstance;

	(void)dev; /* ground truth reads *sDeviceInstance, not this param, at this point */

	sInterruptURBInterval = bInterval;
	*(int **)((char *)urb + 0x28) = sDeviceInstance;
	*(unsigned int *)((char *)urb + 0x30) =
		((unsigned int)devVal << 8) | ((unsigned int)epAddr << 0xf) | 0x40000080u;
	*(void **)((char *)urb + 0x40) = buf;
	*(unsigned int *)((char *)urb + 0x50) = wMaxPacketSize;
	*(void (**)(struct urb *))((char *)urb + 0x74) = InterruptCallback;
	*(unsigned int *)((char *)urb + 0x70) = 0;
	/* sDeviceInstance[7]==3: USB_SPEED_HIGH - interval is 2^(bInterval-1)
	 * microframes, matching usb_fill_int_urb()'s own real high-speed handling;
	 * otherwise interval is bInterval directly (full/low speed, in frames). */
	if (sDeviceInstance[7] == 3)
		sInterruptURBInterval = 1u << ((bInterval - 1) & 0x1f);
	*(unsigned int *)((char *)urb + 0x68) = sInterruptURBInterval;
	*(unsigned int *)((char *)urb + 0x3c) |= 0x100;
	*(int *)((char *)urb + 0x60) = -1;
}

/* Shared by alloc_command_urbs/alloc_video_urbs - allocates `count` URBs with a
 * 0x40-byte kmalloc'd buffer each, configures each for `outEp`, and pushes each
 * onto the given free list. `context_tag` is the value stored at urb+0x70 (0 for
 * command, 1 for video - see this file's own header comment). Returns 0 (failure)
 * and leaves partial state for the caller's free_all_urbs() to clean up, matching
 * every other failure path in this file - on any single allocation failure, or 1
 * (success) once every slot is filled. */
/* `skip_first_on_freelist`: CORRECTION (re-verification pass, 2026-07-17) -
 * ground truth shows the command-URB allocation loop deliberately never
 * links its very first allocated URB (sBulkCommandURBs[0]) onto the free
 * list (real condition: `if (iVar7 != 1) push-to-freelist`, iVar7 being the
 * 1-based allocation counter) - that URB is reserved for
 * CleanupOmapNKS4Driver's own direct, out-of-band emergency-stop use, and
 * must never be handed out by WaitForFreeBulkWriteURB while one is
 * in-flight. The video-URB pool has no such reservation. This project's own
 * earlier draft pushed every allocated URB (including index 0) onto the
 * free list unconditionally - a real correctness/race gap, not just a
 * count mismatch, since sBulkCommandURBs[0] could then be legitimately
 * handed out and in-flight when the emergency-stop path grabbed it. */
static int alloc_urb_pool(void **pool, int count, void *outEp, struct urb_node **freeList,
                           unsigned int context_tag, int skip_first_on_freelist)
{
	unsigned short wMaxPacketSize = *(unsigned short *)((char *)outEp + 4);
	unsigned char  epAddr         = *(unsigned char  *)((char *)outEp + 2);
	int            devVal         = *sDeviceInstance;

	for (int i = 0; i < count; i++) {
		void *urb = stg_usb_alloc_urb(0, 0);
		pool[i] = urb;
		if (!urb)
			return 0;
		/* CORRECTION (full-coverage sweep, 2026-07-18): ground truth (fresh
		 * disassembly, @0x1152c-0x11534 for the command-URB loop and its
		 * video-loop twin @0x115ce-0x11673) allocates each buffer with
		 * size = wMaxPacketSize - the SAME value already computed just
		 * above for the pipe encoding, not a hardcoded 0x40. Confirmed via
		 * a complete, gapless instruction trace: MOVZX EAX,word ptr
		 * [outEp+4] (through a pre-offset cached pointer, traced back to
		 * outEp+4 across the whole pool-clearing block that doesn't touch
		 * the register); MOV EDX,0xd0; CALL __kmalloc, with zero
		 * intervening writes to EAX. Matches urb->length (+0x50) being set
		 * to this exact same wMaxPacketSize value a few lines below -
		 * ground truth ties the buffer's real allocated size and its
		 * claimed length to ONE real value, not two independent constants.
		 * A too-small hardcoded buffer here combined with a larger real
		 * wMaxPacketSize would be a genuine heap-overflow risk the moment
		 * the USB core reads/writes up to the claimed `length` bytes. */
		void *buf = kmalloc_buf(wMaxPacketSize);
		if (!buf)
			return 0;

		*(unsigned int *)((char *)urb + 0x3c) |= 0x100;
		*(int **)((char *)urb + 0x28) = sDeviceInstance;
		*(unsigned int *)((char *)urb + 0x30) =
			((unsigned int)devVal << 8) | 0xc0000000u | ((unsigned int)epAddr << 0xf);
		*(void **)((char *)urb + 0x40) = buf;
		*(unsigned int *)((char *)urb + 0x50) = wMaxPacketSize;
		*(void (**)(struct urb *))((char *)urb + 0x74) = WriteCallback;
		*(unsigned int *)((char *)urb + 0x70) = context_tag;

		if (skip_first_on_freelist && i == 0)
			continue;

		struct urb_node *node = (struct urb_node *)((char *)urb + 0x14);
		node->next = *freeList;
		node->pprev = freeList;
		*freeList = node;
	}
	return 1;
}

static int alloc_command_urbs(int count, void *outEp)
{
	return alloc_urb_pool(sBulkCommandURBs, count, outEp, &sBulkFreeCommandURBList, 0, 1);
}

static int alloc_video_urbs(int count, void *outEp)
{
	return alloc_urb_pool(sBulkVideoURBs, count, outEp, &sBulkFreeVideoURBList, 1, 0);
}

/* Free the interrupt URB + the 16 command + 256 video URBs and reset the free
 * lists. Command URB count corrected from 15 to 16, re-verification pass
 * 2026-07-17 - see sBulkCommandURBs's own comment. */
static void free_all_urbs(void)
{
	/* Ground truth (fresh decompile, 2026-07-18, OmapNKS4Disconnect@0x10ed0 and
	 * OmapNKS4Probe@0x11180's own failure/pre-alloc paths): this exact reset
	 * block - in-use counters zeroed, free lists reset to self, pool zeroed,
	 * URBs freed - appears identically in all four places this logic runs.
	 * The in-use counter resets were missing here (previously relied on BSS
	 * zero-init only being correct on the very first load). */
	sBulkCommandURBsInUse = 0;
	sBulkVideoURBsInUse = 0;
	sBulkFreeCommandURBList = (struct urb_node *)&sBulkFreeCommandURBList;
	/* CORRECTION (full-coverage sweep, 2026-07-18): ground truth's own reset
	 * of this exact pad slot (@0x11423 - PTR_LOOP_0001b014 = &sBulkFreeCommandURBList,
	 * i.e. the SAME value as the head pointer, not &itself) was missing here -
	 * previously only the padding STORAGE was reserved (see the field's own
	 * declaration comment), not its explicit reset alongside the head pointer.
	 * Harmless in practice (nothing reads this slot back), but matches ground
	 * truth's own real behavior at every one of its four real reset sites. */
	sBulkFreeCommandURBListPad = (void *)&sBulkFreeCommandURBList;
	sBulkFreeVideoURBList   = (struct urb_node *)&sBulkFreeVideoURBList;
	sBulkFreeVideoURBListPad = (void *)&sBulkFreeVideoURBList;
	for (int i = 0; i < 16; i++)  { stg_usb_free_urb(sBulkCommandURBs[i]); sBulkCommandURBs[i] = 0; }
	for (int i = 0; i < 256; i++) { stg_usb_free_urb(sBulkVideoURBs[i]);   sBulkVideoURBs[i] = 0; }
	stg_usb_free_urb(sInterruptURB);
	sInterruptURB = 0;
}

void OmapNKS4Disconnect(void)
{
	/* CORRECTION (full-coverage sweep, 2026-07-18): ground truth (fresh
	 * disassembly, @0x10edd-0x10ef4) sets up the same "%s: line %d:"
	 * wrapper used throughout this file, not a bare message - confirmed via
	 * read_memory: format string @0x19c74 is literally
	 * "<6>OmapNKS4:%s: line %d: disconnect\n", name @0x19080 is
	 * "OmapNKS4Disconnect", line=0x50e (1294). */
	printk("<6>OmapNKS4:%s: line %d: disconnect\n", "OmapNKS4Disconnect", 0x50e);
	sDriverState = 2;
	free_all_urbs();
	sDisconnect = 1;
}

void CleanupOmapNKS4Driver(void)
{
	if (sDriverState != 0 && sBulkCommandURBs[0]) {
		/* emergency-stop scan: zero the buffer, set status 4, fire the URB.
		 * CORRECTION (re-verification pass, 2026-07-17): this comment already
		 * said "zero the buffer, set status 4" but the code never actually did
		 * either - it submitted the URB with whatever stale buffer contents
		 * and length it last had. Ground truth (0x10e6d, inside this function)
		 * shows both writes really do happen, immediately before the submit
		 * call: the URB's buffer's first dword is zeroed, and the URB's
		 * length field (offset 0x50) is set to 4. Restored below.
		 *
		 * CORRECTION (full-coverage sweep, 2026-07-18): two more real gaps
		 * found in this same block via fresh disassembly (@0x10e6d-0x10ec1):
		 * (1) ORDER - ground truth zeroes the buffer and sets length=4
		 * BEFORE the "about to emergency stop" printk, not after (swapped
		 * below to match); (2) both printks here are missing the real
		 * "%s: line %d:" wrapper this file uses everywhere else, and -
		 * surprisingly - the real name argument is "EmergencyStopScan"
		 * (confirmed via read_memory @0x190c1), not "CleanupOmapNKS4Driver" -
		 * ground truth's real source almost certainly has a separate
		 * EmergencyStopScan() helper that got inlined here by the compiler
		 * (single call site), with its own printk call sites' compile-time
		 * name string surviving the inlining unchanged. Lines 0x544/0x546. */
		*(int *)*(int **)((char *)sBulkCommandURBs[0] + 0x40) = 0;
		*(unsigned int *)((char *)sBulkCommandURBs[0] + 0x50) = 4;
		printk("<6>OmapNKS4:%s: line %d: about to emergency stop\n",
		       "EmergencyStopScan", 0x544);
		/* mem_flags=0xd0 (GFP_KERNEL) - process context (module cleanup/
		 * unload path), same class as OmapNKS4Init's own interrupt URB
		 * submit below. */
		vm_usb_submit_urb(sBulkCommandURBs[0], 0xd0);
		printk("<6>OmapNKS4:%s: line %d: done!\n", "EmergencyStopScan", 0x546);
	}
	if (gProc) {
		/* Ground truth (fresh decompile, 2026-07-18, CleanupOmapNKS4Driver@0x10d70,
		 * independently re-confirmed by a second full-coverage pass over this file):
		 * this whole block genuinely INLINES OmapNKS4ProcDone's exact body
		 * (procfs.cpp) - same real /proc entry names ("OmapNKS4"/
		 * "OmapNKS4ProgressBar"/"OmapNKS4HardwareVersion"/"OmapNKS4OmapVersion",
		 * not "nks4"/"nks4progress"/etc as this file had before either pass - see
		 * procfs.cpp's own 2026-07-18 correction), same creation order, and the
		 * SAME enter/exit printks (@0x19c34/0x19c54) with func name
		 * "OmapNKS4ProcDone" (not this function's own name) and lines 0x1e7/0x1f3
		 * (0x1f3 = 499 decimal, same value either way) - a real, previously
		 * missing compiler-inlined duplicate, same class of finding as
		 * SendNKS4EventToSTG/COmapNKS4Driver::Initialize elsewhere in this
		 * project. parent=0 (top-level /proc entries, matching create_proc_entry's
		 * own implied parent). */
		printk("<6>OmapNKS4:%s: line %d: enter\n", "OmapNKS4ProcDone", 0x1e7);
		remove_proc_entry("OmapNKS4", 0);
		remove_proc_entry("OmapNKS4ProgressBar", 0);
		remove_proc_entry("OmapNKS4HardwareVersion", 0);
		remove_proc_entry("OmapNKS4OmapVersion", 0);
		gProc = gProcProgress = gProcHardwareVersion = gProcOmapVersion = 0;
		printk("<6>OmapNKS4:%s: line %d: exit\n", "OmapNKS4ProcDone", 0x1f3);
	}
	if (sSTG2NKS4SrqNumber > 0) {
		CSTGOmapNKS4Fifos::sInstance.Initialize(0);
		COmapNKS4Driver_Cleanup();
	}
	/* sOmapNKS4UsbDriver: this module's real struct usb_driver object, now
	 * reconstructed byte-exact from ground truth - see main.cpp's own extensive
	 * comment for the full derivation (read_memory @0x1afa0, cross-checked against
	 * linux-kronos's real struct usb_driver/usb_device_id layouts). */
	stg_usb_deregister((struct usb_driver *)sOmapNKS4UsbDriver);
	rt_free_srq(sSTG2NKS4SrqNumber);
	sSTG2NKS4SrqNumber = 0;
}

/* ---- probe ------------------------------------------------------------- */
/*
 * param_1 = struct usb_interface*.  Reconstructed structurally; the exact struct
 * walks (interface->cur_altsetting->endpoint[i].desc) use the recovered offsets.
 */
int OmapNKS4Probe(struct usb_interface *intf)
{
	int *p = (int *)intf;
	int dev = p[7];				/* usb_device* (interface->...->udev) */

	sDriverState = 3;

	/* 1. identity check: vendor 0x0944 product 0x1005
	 * CORRECTION (full-coverage sweep, 2026-07-18): ground truth (fresh
	 * decompile, @0x11180) wraps this in the same "%s: line %d:" pattern
	 * used throughout this file, and the real message says "vendor %04x
	 * AND product %04x" (with "and"), not "vendor %04x product %04x". */
	if (*(short *)((char *)dev + 0xba) != 0x1005 ||
	    *(short *)((char *)dev + 0xb8) != 0x0944) {
		printk("<6>OmapNKS4:%s: line %d: wrong ID set: vendor %04x and product %04x\n",
		       "OmapNKS4Probe", 0x475,
		       *(short *)((char *)dev + 0xb8), *(short *)((char *)dev + 0xba));
		return -19;			/* -ENODEV */
	}
	if (sDeviceInstance) {
		/* CORRECTION (full-coverage sweep, 2026-07-18): missing "%s: line %d:"
		 * wrapper, same finding as above. */
		printk("<6>OmapNKS4:%s: line %d: DANGER! 2nd OmapNKS4 detected. Not supported\n",
		       "OmapNKS4Probe", 0x47c);
		return -16;			/* -EBUSY */
	}

	CSTGOmapNKS4Fifos::sInstance.Initialize(0);
	COmapNKS4VideoAPI_Initialize();
	sDeviceInstance = (int *)(dev - 100);
	/* Ground truth (fresh decompile, 2026-07-18, OmapNKS4Probe@0x11180): this
	 * diagnostic printk was missing entirely from the previous version. Reads
	 * bcdDevice (usb_device_descriptor field right after idProduct, offset
	 * +0xbc - matches idVendor@+0xb8/idProduct@+0xba's own 2-byte spacing)
	 * purely to log it here. */
	printk("<6>OmapNKS4:%s: line %d: Probe() found: vendor 0x%08x, product 0x%08x, version 0x%08x\n",
	       "OmapNKS4Probe", 0x48d, 0x944, 0x1005, *(unsigned short *)((char *)dev + 0xbc));

	/*
	 * 2. endpoint discovery: require exactly one interrupt-IN and one bulk-OUT
	 *    endpoint.  ifWalk finds &intEp (interrupt) and &outEp (bulk out).
	 */
	void *intEp = 0, *outEp = 0;
	int nInt = 0, nOut = 0;
	/* Ground truth: fresh Ghidra decompile of OmapNKS4Probe@0x11180,
	 * 2026-07-16. Previously just a comment describing what this loop should
	 * do - the loop itself was never actually written, so nInt/nOut stayed 0
	 * forever and probe always failed with "found 0 bulk write and 0
	 * interrupt endpoint/s" (confirmed the hard way on real hardware,
	 * caused a real probe failure -> the sProbeComplete init bug above never
	 * got exercised on a clean success path either). altsetting = *(int
	 * *)intf (param_1[0] in the real disassembly - the FIRST word of the
	 * interface struct, a different field than p[7]'s usb_device* used
	 * above for vendor/product); bNumEndpoints at +4, the endpoint array
	 * pointer at +0xc, each usb_host_endpoint entry 0x2c bytes with the
	 * embedded standard endpoint descriptor at its start (bEndpointAddress
	 * +2, bmAttributes +3, wMaxPacketSize +4, bInterval +6 - matches this
	 * file's existing use of local_2c/local_30's own +2/+4/+6 offsets
	 * further down). Classification: bmAttributes&3==3 (INTERRUPT) + IN
	 * direction (bit 7 of bEndpointAddress set) -> intEp; bmAttributes&3==2
	 * (BULK) + OUT direction (bit 7 clear) -> outEp; anything else
	 * (including the "right type, wrong direction" cases) logged as
	 * unsupported, matching the real binary's own diagnostic printks. Only
	 * the FIRST matching endpoint of each kind is kept - a second one logs
	 * "DANGER! found additional ..." exactly like the real binary, rather
	 * than silently overwriting intEp/outEp. */
	void *altsetting = (void *)p[0];
	unsigned int numEndpoints = *(unsigned char *)((char *)altsetting + 4);
	unsigned char *epArray = *(unsigned char **)((char *)altsetting + 0xc);
	for (unsigned int i = 0; i < numEndpoints; i++) {
		unsigned char *ep = epArray + i * 0x2c;
		unsigned char bEndpointAddress = ep[2];
		unsigned char bmAttributes = ep[3];
		int xferType = bmAttributes & 3;
		bool dirIn = (bEndpointAddress & 0x80) != 0;
		if (xferType == 3 && dirIn) {
			nInt++;
			if (nInt == 1) intEp = ep;
			/* CORRECTION (full-coverage sweep, 2026-07-18): all three of
			 * this loop's diagnostic printks were missing the real
			 * "%s: line %d:" wrapper (confirmed via fresh decompile of
			 * OmapNKS4Probe@0x11180 - ground truth shows "OmapNKS4Probe"/
			 * line 0x49e, 0x4a9, 0x4b3 respectively for these three). */
			else printk("<6>OmapNKS4:%s: line %d: DANGER! found additional interrupt in endpoint!\n",
				    "OmapNKS4Probe", 0x49e);
		} else if (xferType == 2 && !dirIn) {
			nOut++;
			if (nOut == 1) outEp = ep;
			else printk("<6>OmapNKS4:%s: line %d: DANGER! found additional write out endpoint!\n",
				    "OmapNKS4Probe", 0x4a9);
		} else {
			printk("<6>OmapNKS4:%s: line %d: Unsupported endpoint found 0x%02x/0x%04x\n",
			       "OmapNKS4Probe", 0x4b3, bEndpointAddress, bmAttributes);
		}
	}

	if (nInt != 1 || nOut != 1) {
		/* CORRECTION (full-coverage sweep, 2026-07-18): missing "%s: line %d:"
		 * wrapper, same finding as the rest of this function's diagnostics. */
		printk("<6>OmapNKS4:%s: line %d: fatal: found %d bulk write and %d interrupt endpoint/s\n",
		       "OmapNKS4Probe", 0x4b9, nOut, nInt);
		free_all_urbs();
		return -12;			/* -ENOMEM-ish failure path */
	}

	/* 3. bring up the driver with the bulk-out max-packet/4 = words-per-packet */
	COmapNKS4Driver_Initialize(*(unsigned short *)((char *)outEp + 4) >> 2);
	sReadIndexModLock = 0;
	sEventQueueReadIndex = sEventQueueWriteIndex = sNumEventsInQueue = 0;
	for (int i = 0; i < 256; i++) sEventQueue[i] = 0;
	/* Ground truth (fresh disassembly, 2026-07-15, OmapNKS4Probe@0x11180): passes
	 * the raw wMaxPacketSize word (NOT >>2, unlike COmapNKS4Driver_Initialize's arg
	 * above) - found missing an argument entirely via a real Kbuild build attempt;
	 * see COmapNKS4_SetMaxBulkOutMsgSize's own definition (video.cpp). */
	COmapNKS4_SetMaxBulkOutMsgSize(*(unsigned short *)((char *)outEp + 4));

	/* 4. interrupt URB + transfer buffer.
	 * CORRECTION (full-coverage sweep, 2026-07-18): ground truth (fresh
	 * disassembly, @0x11373-0x11377) allocates this buffer with size =
	 * wMaxPacketSize - the real negotiated interrupt-endpoint max-packet-
	 * size, *(unsigned short *)(intEp+4) (the SAME field
	 * configure_interrupt_urb below already re-derives for its own
	 * purposes) - not a hardcoded 0x220. Confirmed via a complete, gapless
	 * instruction trace: MOVZX EAX,word ptr [intEp+4]; MOV EDX,0xd0; CALL
	 * __kmalloc, zero intervening writes to EAX. A hardcoded 0x220 (544
	 * bytes) was never a real ground-truthed constant - implausibly large
	 * for a typical interrupt-IN wMaxPacketSize and a real overflow/
	 * undersized-buffer risk either way if it didn't match real hardware's
	 * negotiated value. */
	/* CORRECTION (full-coverage sweep, 2026-07-18): ground truth's real
	 * control flow (fresh decompile, @0x11180) is NOT one combined check -
	 * a stg_usb_alloc_urb() failure here is SILENT (no printk at all, just
	 * falls straight through to the shared cleanup/-ENOMEM return), while
	 * only the kmalloc failure prints, and with different text than this
	 * reconstruction previously used ("cannot allocate buffer for
	 * transfers", not "cannot allocate interrupt URB/buffer") plus the
	 * usual missing "%s: line %d:" wrapper (name="OmapNKS4Probe",
	 * line=0x4d3). Restructured into two separate checks to match. */
	sInterruptURB = stg_usb_alloc_urb(0, 0);
	if (!sInterruptURB) {
		free_all_urbs();
		return -12;
	}
	unsigned short intMaxPacketSize = *(unsigned short *)((char *)intEp + 4);
	void *intBuf = kmalloc_buf(intMaxPacketSize);
	if (!intBuf) {
		printk("<6>OmapNKS4:%s: line %d: fatal: cannot allocate buffer for transfers\n",
		       "OmapNKS4Probe", 0x4d3);
		free_all_urbs();
		return -12;
	}
	configure_interrupt_urb(sInterruptURB, intEp, intBuf, dev);

	/* CORRECTION (full-coverage sweep, 2026-07-18): ground truth (fresh
	 * disassembly, @0x11405-0x114d7) resets sBulkCommandURBsInUse/
	 * sBulkVideoURBsInUse to 0, both free-list heads (+ their pad slots) to
	 * self-pointing empty, and zeroes both pool arrays UNCONDITIONALLY right
	 * here - before ever attempting to allocate a single command/video URB -
	 * not just on a later failure. This reconstruction previously relied
	 * entirely on whatever these globals already happened to be (BSS zero-
	 * init on a fresh module load - which gives NULL, not ground truth's
	 * real self-pointing convention - or free_all_urbs()'s own resets left
	 * over from a prior failed probe/disconnect cycle). Matches
	 * free_all_urbs()'s own reset shape (same fields, same values) minus
	 * the stg_usb_free_urb() calls - nothing has been allocated into these
	 * arrays yet at this point in a probe, so there is nothing real to free,
	 * only stale slots (from a previous probe cycle) to defensively clear. */
	sBulkCommandURBsInUse = 0;
	sBulkVideoURBsInUse = 0;
	sBulkFreeCommandURBList = (struct urb_node *)&sBulkFreeCommandURBList;
	sBulkFreeCommandURBListPad = (void *)&sBulkFreeCommandURBList;
	sBulkFreeVideoURBList = (struct urb_node *)&sBulkFreeVideoURBList;
	sBulkFreeVideoURBListPad = (void *)&sBulkFreeVideoURBList;
	for (int i = 0; i < 16; i++)  sBulkCommandURBs[i] = 0;
	for (int i = 0; i < 256; i++) sBulkVideoURBs[i] = 0;

	/* 5. 16 command URBs (corrected from 15, re-verification pass 2026-07-17)
	 * + 256 video URBs, each with a kmalloc'd buffer sized to the real
	 * negotiated wMaxPacketSize (corrected from a hardcoded 0x40, see
	 * alloc_urb_pool's own comment) */
	if (!alloc_command_urbs(16, outEp) || !alloc_video_urbs(256, outEp)) {
		free_all_urbs();
		return -12;
	}

	/* 6. success */
	sMaxWritePacketSize = *(unsigned short *)((char *)outEp + 4);
	__init_waitqueue_head(sCommandWaitQueue, (void *)sWaitQueueLockKeyDummy);
	__init_waitqueue_head(sVideoWaitQueue, (void *)sWaitQueueLockKeyDummy);
	__init_waitqueue_head(sReadWaitQueue, (void *)sWaitQueueLockKeyDummy);
	/* submit.cpp's own four wait queues - see that file's own comment (real
	 * NULL-pointer __wake_up_common oops found on a live VM boot test,
	 * 2026-07-17) for why these need the exact same init as the three above. */
	__init_waitqueue_head(sReadEventWaitQueue, (void *)sWaitQueueLockKeyDummy);
	__init_waitqueue_head(sAtmelReadWaitQueue, (void *)sWaitQueueLockKeyDummy);
	__init_waitqueue_head(sVideoMsgWaitQueue, (void *)sWaitQueueLockKeyDummy);
	__init_waitqueue_head(sShutdownSsdWaitQueue, (void *)sWaitQueueLockKeyDummy);
	sDriverState = 1;
	/* CORRECTION (full-coverage sweep, 2026-07-18): missing "%s: line %d:"
	 * wrapper - ground truth (fresh decompile, @0x11180, line=0x4f9) uses
	 * the same pattern as every other diagnostic in this function. */
	printk("<6>OmapNKS4:%s: line %d: probe success\n", "OmapNKS4Probe", 0x4f9);
	/* complete(&sProbeComplete) - wakes OmapNKS4Init, which waits on probe via
	 * wait_for_completion (main.cpp, matching daemon_lifecycle.cpp's own
	 * complete()/wait_for_completion() pairing convention). sProbeComplete itself
	 * is declared and waited on in main.cpp - NOT yet fixed this session (main.cpp
	 * not attempted), so this call is left referencing the not-yet-declared
	 * completion object rather than guessing a wrong stand-in. */
	complete(&sProbeComplete);
	return 0;
}

/* ---- VM-only: synthesize a virtual NKS4 board and self-probe ----------
 * Called from OmapNKS4Init() (main.cpp), guarded by vm_virtual_probe - see
 * that call site's own comment for why this lives inline rather than in a
 * separate module. OmapNKS4Probe() above never dereferences a real kernel
 * usb_interface/usb_device through their real accessors - just the fixed
 * raw offsets documented at this file's own top comment - so correctly-
 * shaped plain memory satisfies it exactly as well as a real enumerated
 * device would, matching OmapNKS4VirtualBoard.c's own vendor 0x0944 /
 * product 0x1005 / one interrupt-IN (0x81) + one bulk-OUT (0x02) layout. */
static void vm_zero_buf(void *p, unsigned int n)
{
	unsigned char *b = (unsigned char *)p;
	for (unsigned int i = 0; i < n; i++) b[i] = 0;
}

static void vm_write_ep(unsigned char *ep, unsigned char addr, unsigned char attrs,
                         unsigned short maxpkt, unsigned char interval)
{
	ep[2] = addr;
	ep[3] = attrs;
	*(unsigned short *)(ep + 4) = maxpkt;
	ep[6] = interval;
}

/* VM-only: real stg_usb_submit_urb() always fails against the synthetic
 * vm_virtual_probe device (vm_virtual_probe_inject, below) - it has no real
 * backing struct usb_device/bus/HCD, by design (that's exactly what sidesteps
 * dummy_hcd's TCG hang). Two cases:
 *
 * - The interrupt-IN URB (identified by comparing against sInterruptURB,
 *   above): accept, never complete. InterruptCallback unconditionally
 *   resubmits itself on every successful completion, so completing it
 *   synchronously here would recurse without bound on this same stack (a
 *   real HCD completes URBs from a later interrupt/softirq context, never
 *   from inside submit itself).
 * - Every other (bulk command/video) URB: WriteCallback (above) never
 *   resubmits anything, so completing THESE synchronously is safe - and
 *   necessary: WaitForNKS4CommandWrite (submit.cpp, every setter in
 *   command.cpp routes through it) blocks on sDoingWait4Write, which only
 *   WriteCallback's own in-use-count-reaches-zero check clears. Without
 *   this, the very first setter call (SetNumberOfAnalogInputs) would spin
 *   in WaitForNKS4CommandWrite's own `for(;;)` forever - confirmed by
 *   inspection before it was ever hit live (WriteCallback's completion path
 *   is the ONLY thing that clears that flag).
 *
 * Minimal, protocol-aware query stub: the plain command-word path uses
 * doReverse=false (submit.cpp's own submit_urb_words, confirmed on real
 * hardware, 2026-07-16 - the reversed form put the panel-side wire order
 * out), so a raw command word lands at the URB's own buffer (+0x40) as a
 * plain little-endian u32, unmodified - read it directly and match against
 * command.cpp's own raw word constants. Answers exactly the query set
 * COmapNKS4Driver_Configure's own real sequence (driver.cpp) sends on this
 * project's confirmed real hardware (hwVer==1, "else" branch: OMAP V01 R08,
 * PSoc V00 R05, see that file's own comment) - not a general protocol
 * emulator, but enough for the real, unmodified init sequence to run to
 * completion against this virtual board. */
int vm_usb_submit_urb(struct urb *urb, unsigned int mem_flags)
{
	if (!sVmVirtualProbe)
		return stg_usb_submit_urb(urb, mem_flags);

	if (urb == sInterruptURB)
		return 0;

	unsigned char *buf = *(unsigned char **)((char *)urb + 0x40);
	unsigned int len = *(unsigned int *)((char *)urb + 0x50);
	if (buf && len >= 4) {
		unsigned int cmd = *(unsigned int *)buf;
		unsigned int reply = 0;
		int haveReply = 1;

		switch (cmd) {
		case 0x00ee0000:	/* CommunicationCheck -> ack tag 0x0066 */
			reply = 0x00660000;
			break;
		case 0x01f10000:	/* ReadPortConfiguration/GetRawDipSwitches (reg
					 * 0xf1) -> tag 0x0171, hwVer=1 (low nibble),
					 * is88=0 (bit8) - matches this project's own
					 * confirmed real hardware (driver.cpp's own
					 * COmapNKS4Driver_Configure comment). */
			reply = 0x01710001;
			break;
		case 0x00f00000:	/* GetVersion (4-out, no-index form, hwVer==1
					 * branch) -> tag 0x0070; byte1=OMAP nibbles,
					 * byte0=PSoC nibbles. Matches this project's own
					 * confirmed real hardware dmesg "OMAP V01 R08,
					 * PSoc V00 R05": byte1=(1<<4)|8=0x18,
					 * byte0=(0<<4)|5=0x05. */
			reply = 0x00701805;
			break;
		default:
			/* GetVersion's OTHER form (1-arg, reg 0xf0 | index<<8, used
			 * by the hwVer==2/3 branches - unconfirmed on real hardware,
			 * see driver.cpp's own COmapNKS4Driver_Configure comment: "not
			 * independently verified... flagged for revisit"). Matched
			 * generically by reg byte (bits 16-23) rather than full-word,
			 * since the index varies - reuses the SAME confirmed tag/
			 * payload shape as the no-index form above rather than
			 * guessing distinct per-index values ground truth doesn't
			 * establish. This exists purely so an indexed GetVersion call
			 * can't leave WaitForNKS4ReadEvent() timing out / the caller
			 * blocked - not a claim that the returned version bytes are
			 * correct for that (currently unreachable on confirmed
			 * hardware) branch. */
			if (((cmd >> 16) & 0xff) == 0xf0) {
				reply = 0x00701805;
				break;
			}
			/* SetLCDBrightness/ResetModule (command.cpp): both OUTSIDE the
			 * real confirmed boot/configure sequence
			 * (COmapNKS4Driver_Configure() never calls either), so the
			 * earlier "every wire command, board stays running" pass
			 * deliberately left them uncovered - see README's own
			 * "Deep wire-protocol fidelity for commands OUTSIDE this real,
			 * confirmed boot sequence... remains unimplemented" note.
			 * Ground truth (command.cpp, re-verified 2026-07-15): both
			 * encode as `opcode<<24 | level_or_mode<<16`, i.e. the level/mode
			 * byte rides in the REG position (bits 16-23), NOT a fixed data
			 * byte - so unlike CommunicationCheck/ReadPortConfiguration/
			 * GetVersion above, these can't be matched as one literal word;
			 * matched here by opcode byte alone (bits 24-31), the same style
			 * already used for GetVersion's indexed 0xf0 fallback just above.
			 * Both are setters - WaitForNKS4CommandWrite (submit.cpp) never
			 * waits on a read event for either, only on sDoingWait4Write
			 * (cleared by WriteCallback) - so, same as every other setter,
			 * no reply is queued; this branch exists purely to name-confirm
			 * (via printk) that the synthetic board actually received and
			 * decoded each command correctly, rather than silently falling
			 * through the generic "no reply" default below indistinguishably
			 * from an unrecognized word. */
			if (((cmd >> 24) & 0xff) == 0xc7) {
				printk("<6>OmapNKS4: vm_virtual_probe: SetLCDBrightness accepted, "
				       "level=%u (cmd=0x%08x)\n", (cmd >> 16) & 0xff, cmd);
				haveReply = 0;
				break;
			}
			if (((cmd >> 24) & 0xff) == 0x06) {
				printk("<6>OmapNKS4: vm_virtual_probe: ResetModule accepted, "
				       "mode=%u (cmd=0x%08x)\n", (cmd >> 16) & 0xff, cmd);
				haveReply = 0;
				break;
			}
			/* every other setter (SetNumberOfAnalogInputs,
					 * SetAllAnalogInputFilter, SetNumberOfLEDs,
					 * ConfigureRotaryEncoders, SetRotaryEncoderSample-
					 * Speed, ConfigureScanning, ...): no reply
					 * expected by the caller (WaitForNKS4CommandWrite
					 * only waits on sDoingWait4Write, not a read
					 * event) - just complete the write below. */
			haveReply = 0;
			break;
		}

		if (haveReply) {
			/* NOT delivered here via SendNKS4EventToLinuxReader()
			 * directly - at this point the caller's own
			 * SubmitNKS4CommandWrite() hasn't even returned yet, so
			 * WaitForNKS4ReadEvent() (called only AFTER it returns)
			 * hasn't set sWaitReadPtr yet either; calling it here would
			 * silently drop the reply against a NULL sWaitReadPtr
			 * (confirmed on a live VM boot test, 2026-07-17). Stashed
			 * instead; WaitForNKS4ReadEvent() (submit.cpp) delivers it
			 * right after setting sWaitReadPtr, on the same single
			 * synchronous call chain - see that function's own comment. */
			printk("<6>OmapNKS4: vm_virtual_probe: queuing reply 0x%08x for cmd 0x%08x\n",
			       reply, cmd);
			sVmPendingReply = reply;
			sVmPendingReplyValid = 1;
		}
	}

	/* Complete the write - see this function's own top comment for why this
	 * is safe (WriteCallback never resubmits) and necessary (setters block
	 * on it via sDoingWait4Write). */
	*(int *)((char *)urb + 0x38) = 0;	/* status = 0 (success) */
	WriteCallback(urb);
	(void)mem_flags;
	return 0;
}

int vm_virtual_probe_inject(void)
{
	/* "dev" must sit >=100 bytes into its own allocation (OmapNKS4Probe's own
	 * "dev - 100" sDeviceInstance read, above) with room past +0xba for
	 * idVendor/idProduct - one 0x200-byte block covers both regions. GFP_KERNEL
	 * = 0xd0, matching this same file's/main.cpp's own already-confirmed usage
	 * (e.g. main.cpp's stg_usb_submit_urb(sInterruptURB, 0xd0)). */
	unsigned char *devBuf     = (unsigned char *)kmalloc_buf(0x200);
	int           *intf       = (int *)kmalloc_buf(0x20);
	unsigned char *altsetting = (unsigned char *)kmalloc_buf(0x10);
	unsigned char *epArray    = (unsigned char *)kmalloc_buf(2 * 0x2c);
	unsigned char *dev;
	int rc;

	if (!devBuf || !intf || !altsetting || !epArray) {
		printk("<6>OmapNKS4: vm_virtual_probe: kmalloc failed\n");
		return -12;
	}
	vm_zero_buf(devBuf, 0x200);
	vm_zero_buf(intf, 0x20);
	vm_zero_buf(altsetting, 0x10);
	vm_zero_buf(epArray, 2 * 0x2c);

	dev = devBuf + 100;
	*(int *)(devBuf + 0x00) = 1;               /* sDeviceInstance[0]: device address */
	*(int *)(devBuf + 0x1c) = 3;               /* sDeviceInstance[7]: USB_SPEED_HIGH */
	*(unsigned short *)(dev + 0xb8) = 0x0944;  /* idVendor */
	*(unsigned short *)(dev + 0xba) = 0x1005;  /* idProduct */

	vm_write_ep(epArray + 0 * 0x2c, 0x81, 0x03, 64, 1);  /* interrupt-IN */
	vm_write_ep(epArray + 1 * 0x2c, 0x02, 0x02, 64, 0);  /* bulk-OUT */

	altsetting[4] = 2;                           /* bNumEndpoints */
	*(void **)(altsetting + 0xc) = epArray;      /* endpoint array pointer */

	intf[0] = (int)altsetting;
	intf[7] = (int)dev;

	printk("<6>OmapNKS4: vm_virtual_probe: calling OmapNKS4Probe() with a synthetic "
	       "vendor=0944 product=1005, 1 int-IN + 1 bulk-OUT ep\n");
	rc = OmapNKS4Probe((struct usb_interface *)intf);
	printk("<6>OmapNKS4: vm_virtual_probe: OmapNKS4Probe() returned %d\n", rc);
	/* Not freed on success: OmapNKS4Probe() stashed pointers derived from these
	 * buffers (sDeviceInstance, each URB's own +0x28 "dev" field) into this
	 * module's live state - freeing them here would leave dangling pointers a
	 * later disconnect/cleanup path could still touch. On failure,
	 * OmapNKS4Probe() has already torn down anything IT allocated, and leaking
	 * ~1KB on a VM-testing-only failure path isn't worth the extra bookkeeping. */
	return rc;
}

/* ---- VM-only, 2026-07-17 continuation: one synthetic runtime event -------
 *
 * Everything above (vm_usb_submit_urb/vm_virtual_probe_inject) only ever
 * exercises the OUTBOUND path (host->panel command words) plus the panel's
 * query replies to them. Nothing yet feeds a real INBOUND interrupt-IN event
 * packet - the kind a real board sends continuously once a key/knob/button on
 * the front panel is actually touched - through InterruptCallback()'s own
 * decode call (COmapNKS4Driver_ReceiveEventBuffer(), driver.cpp). This function
 * closes that gap: it writes a small, format-correct event packet directly
 * into sInterruptURB's own transfer buffer (exactly where a real HCD would
 * have DMA'd inbound data) and calls the real, unmodified InterruptCallback()
 * on it - so the whole decode -> calibration -> filter -> queue chain runs
 * through its actual production code path, not a shortcut/mock of it.
 *
 * Called once from OmapNKS4Init() (main.cpp), after COmapNKS4Driver_Configure()
 * has run to completion and both worker threads + the active-sense thread are
 * already up - i.e. against a fully "running" board, matching this project's
 * own stated goal of proving the runtime traffic path works, not just boot.
 *
 * Packet layout - two NKS4Command records + a Sync terminator, 12 bytes /
 * 3 dwords (InterruptCallback passes numInts = len/4 to ReceiveEventBuffer):
 *
 *   record 0: dLo=0x01 dHi=0x00 idx=0x50 op=0x01
 *     Ordinary button/key event (ReceiveEventBuffer's own `(idx & 0xf0) ==
 *     0x50` branch) - byte-for-byte the same record
 *     verify/test_driver_receive_event_buffer.cpp already established as
 *     known-good ground truth (its "Button/idx=0x50 does not wake the reader"
 *     sanity case feeds this exact same [0x01,0x00,0x50,0x01] tuple - that
 *     test only asserts it does NOT hit the separate idx==0x71 echo path, but
 *     it IS the real, ground-truthed button-event shape). With
 *     fInstallerSupportOn on (enabled just below), v=dLo|dHi<<8=1 is > 0, so
 *     this also calls OmapNKS4ProcAddEvent(0xd) (driver.cpp) - the actual
 *     /proc/nks4 read()-side event queue (OmapNKS4ProcReadEvent(), procfs.cpp)
 *     - as well as the general host<-panel inputFifo every branch feeds via
 *     push_event().
 *   record 1: dLo=0x34 dHi=0x12 idx=0x01 op=0x03
 *     Aftertouch event - byte-for-byte the SAME record
 *     verify/test_driver_receive_event_buffer.cpp's own "Aftertouch
 *     (non-test-mode) byte packing" known-answer case feeds through
 *     ApplyNKS4Calibration() (idx=1 avoids that function's idx==7/idx==0x1b-
 *     0x1e special-case branches, exercising the plain calibration- >
 *     FilterEvent -> push_event leg cleanly). Included so this VM test also
 *     covers the "calibration" leg of the decode chain, not just the
 *     installer-support/button leg above.
 *   record 2: 0x00 0x00 0x00 0x87
 *     Sync terminator (ReceiveEventBuffer's own end-of-buffer marker - read to
 *     end the loop but never itself dispatched through the switch).
 */
void vm_virtual_probe_inject_event(void)
{
	static const unsigned char evbuf[12] = {
		0x01, 0x00, 0x50, 0x01,		/* button, idx=0x50 (installer "dec" + FIFO) */
		0x34, 0x12, 0x01, 0x03,		/* aftertouch, idx=0x01 (calibration + FIFO) */
		0x00, 0x00, 0x00, 0x87,		/* Sync terminator */
	};
	unsigned char *buf;
	unsigned int i;

	if (!sVmVirtualProbe || !sInterruptURB)
		return;

	printk("<6>OmapNKS4: vm_virtual_probe: enabling installer support and injecting "
	       "one synthetic interrupt-IN event packet (button idx=0x50 + aftertouch idx=0x01)\n");
	/* So the button record above also reaches the real /proc/nks4 event
	 * queue, not just the general inputFifo - see this function's own top
	 * comment. */
	COmapNKS4Driver_SetInstallerSupportOn(1);

	buf = *(unsigned char **)((char *)sInterruptURB + 0x40);
	if (!buf) {
		printk("<6>OmapNKS4: vm_virtual_probe: interrupt URB has no buffer, "
		       "cannot inject event\n");
		return;
	}
	for (i = 0; i < sizeof(evbuf); i++)
		buf[i] = evbuf[i];
	/* actual_length (+0x54) - what a real HCD sets on completion to say how
	 * much data it actually DMA'd in; InterruptCallback reads exactly this
	 * field (see this file's own top comment on urb+0x54 vs +0x50). */
	*(unsigned int *)((char *)sInterruptURB + 0x54) = sizeof(evbuf);
	*(int *)((char *)sInterruptURB + 0x38) = 0;	/* status = 0 (OK) */

	/* Call the real, unmodified completion handler directly - exactly what a
	 * real HCD would invoke once hardware DMA'd interrupt-IN data into this
	 * same buffer. InterruptCallback() unconditionally resubmits itself
	 * (vm_usb_submit_urb(urb, 0x20)) at the end - safe here, same as every
	 * other interrupt-IN completion: vm_usb_submit_urb's own `urb ==
	 * sInterruptURB` case (above) just accepts without completing again, so
	 * this does not recurse. */
	InterruptCallback((struct urb *)sInterruptURB);
}

/* ---- VM-only, 2026-07-17 continuation: exercise SetLCDBrightness/ResetModule --
 *
 * Neither command is ever sent by this module's own real, confirmed boot/
 * configure sequence (COmapNKS4Driver_Configure(), driver.cpp) - both are
 * COmapNKS4Command free functions callable from OUTSIDE this module (e.g. a
 * front-panel-brightness control surface or a reset-request path this
 * reconstruction hasn't located a real caller for yet), so nothing in the
 * module's own startup path would ever drive vm_usb_submit_urb's new
 * SetLCDBrightness/ResetModule dispatch branches above. To actually exercise
 * them (not just add dead dispatch code), call the real, unmodified
 * COmapNKS4Command::SetLCDBrightness()/ResetModule() directly - same as any
 * real external caller would, just invoked from inside this module for test
 * purposes. Locally re-declares their signatures the same way driver.cpp's own
 * `namespace COmapNKS4Command { ... }` forward-declaration block already does
 * for the functions IT calls - neither of these two is in that list since
 * driver.cpp's real Configure() never calls them either.
 *
 * mode=0/level=0x80 are arbitrary VM-test values - ResetModule(0) has no real
 * hardware to actually reset here (vm_usb_submit_urb's dispatch just logs and
 * completes the write, exactly like every other setter), so this is safe to
 * call even after the board is fully configured and running, unlike on real
 * hardware where a reset would presumably drop the just-negotiated session. */
namespace COmapNKS4Command {
	bool SetLCDBrightness(unsigned char level);
	bool ResetModule(unsigned char mode);
}

void vm_virtual_probe_test_setters(void)
{
	bool lcdOk, resetOk;

	if (!sVmVirtualProbe)
		return;

	printk("<6>OmapNKS4: vm_virtual_probe: calling SetLCDBrightness(0x80)/ResetModule(0x00) "
	       "directly to exercise their new vm_usb_submit_urb dispatch coverage\n");
	lcdOk   = COmapNKS4Command::SetLCDBrightness(0x80);
	resetOk = COmapNKS4Command::ResetModule(0x00);
	printk("<6>OmapNKS4: vm_virtual_probe: SetLCDBrightness(0x80) -> %s, "
	       "ResetModule(0x00) -> %s\n",
	       lcdOk ? "ok" : "FAILED", resetOk ? "ok" : "FAILED");
}
