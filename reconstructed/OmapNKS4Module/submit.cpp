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
 * omapnks4_internal.h's own comment for the full derivation/precedent). Not
 * wired to real blocking waits (WaitForNKS4ReadEvent/WaitOnAtmelRead use a
 * schedule_timeout_wait()/sleep_on_timeout() polling simplification below, and
 * ProcessMsgRoutine/ShutdownSSDRoutine poll too - see main.cpp's own comment on
 * why), so __wake_up()ing them is currently a harmless no-op, not yet
 * load-bearing - same caveat as usb.cpp's wait queues. */
static unsigned char sReadEventWaitQueue[0xc];
static unsigned char sAtmelReadWaitQueue[0xc];
static unsigned char sVideoMsgWaitQueue[0xc];
static unsigned char sShutdownSsdWaitQueue[0xc];

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

/* schedule_timeout_wait/sleep_on_timeout: real kernel primitives
 * (schedule_timeout(jiffies)/sleep_on_timeout(wait_queue_head_t*, jiffies)) called
 * with elided arguments in the original decompile - modeled here as short-sleep
 * polling helpers (same simplification as WaitForFreeBulkWriteURB's own established
 * stg_msleep(20) polling, see usb.cpp), not real wait-queue blocking. Each call
 * sleeps a fixed small slice and reports how much of the caller's budget remains,
 * matching schedule_timeout()'s real "returns remaining jiffies" contract closely
 * enough for this loop's own use (terminate when the budget is exhausted). */
static int schedule_timeout_wait(int remaining_jiffies)
{
	stg_msleep(1);
	return remaining_jiffies > 0 ? remaining_jiffies - 1 : 0;
}

int WaitForNKS4ReadEvent(unsigned int *resp)
{
	sWaitReadPtr = resp;
	if (!resp)
		return 0;
	/* wait up to ~1000 jiffies for SendNKS4EventToLinuxReader() to fill *resp */
	int t = 1000;
	while (sWaitReadPtr && t)
		t = schedule_timeout_wait(t);
	if (sWaitReadPtr == 0)
		return 0;
	printk("<6>OmapNKS4: WaitForNKS4ReadEvent() timed out\n");
	sWaitReadPtr = 0;
	return -1;
}

static void sleep_on_timeout(void *q, unsigned long timeout_jiffies)
{
	(void)q;
	for (unsigned long waited = 0; waited < timeout_jiffies; waited++)
		stg_msleep(1);
}

void WaitOnAtmelRead(void)            { sleep_on_timeout(sAtmelReadWaitQueue, 1000); }
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

/* pop a free URB node off 'list' (intrusive list, head == &list when empty) */
static struct urb_node *pop_free_urb(struct urb_node **list)
{
	struct urb_node *n = *list;
	if (n == (struct urb_node *)list)
		return 0;
	*list = n->next;
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

	/* TEMP diagnostic (2026-07-16 debug session) - dump the exact wire bytes,
	 * length, flags, and pipe fields right before submission, plus the real
	 * submit return code - remove once the comm-check timeout is
	 * root-caused. */
	{
		unsigned int len = *(unsigned int *)((char *)urb + 0x50);
		unsigned int flags = *(unsigned int *)((char *)urb + 0x3c);
		unsigned int pipe = *(unsigned int *)((char *)urb + 0x30);
		printk("<6>OmapNKS4: DIAG submit_urb_words: len=%u flags=0x%x pipe=0x%x buf=%02x %02x %02x %02x\n",
		       len, flags, pipe,
		       ((unsigned char *)buf)[0], ((unsigned char *)buf)[1],
		       ((unsigned char *)buf)[2], ((unsigned char *)buf)[3]);
	}

	/* mem_flags=0x20 (GFP_ATOMIC) - ground truth: real
	 * SubmitNKS4CommandMultipleWriteNONBLOCKING@0x10d01 disassembly, this is
	 * a command-write submission which can run from atomic/locked contexts,
	 * unlike the interrupt URB's own initial process-context submit in
	 * OmapNKS4Init (which the real binary passes GFP_KERNEL=0xd0). Both were
	 * previously hardcoded 0 - not blocking submission (confirmed: rc==0
	 * either way), but a genuine deviation from ground truth, confirmed
	 * 2026-07-16. */
	int rc = stg_usb_submit_urb(urb, 0x20);
	printk("<6>OmapNKS4: DIAG stg_usb_submit_urb rc=%d\n", rc);
	if (rc == 0) {
		CActiveSenseThread::DoPing();	/* defer the idle active-sense tick */
		return 0;
	}
	printk("<6>OmapNKS4: usb_submit_urb failed\n");
	return -1;
}

int SubmitNKS4CommandMultipleWriteNONBLOCKING(unsigned int *cmds, unsigned int nInts)
{
	struct urb_node *n = pop_free_urb(&sBulkFreeCommandURBList);
	if (!n) {
		printk("<6>OmapNKS4: SubmitNKS4CommandMultipleWriteNONBLOCKING() ran out of urbs\n");
		return -1;
	}
	sBulkCommandURBsInUse++;
	return submit_urb_words(n, cmds, nInts, 0, false);
}

int SubmitNKS4CommandWrite(unsigned int cmd)
{
	WaitForFreeBulkWriteURB(0);
	if (sBulkFreeCommandURBList == (struct urb_node *)&sBulkFreeCommandURBList) {
		printk("<6>OmapNKS4: SubmitNKS4CommandWrite() fails - no free urbs\n");
		return -1;
	}
	return SubmitNKS4CommandMultipleWriteNONBLOCKING(&cmd, 1);
}

/* Ground truth: fresh Ghidra decompile of WaitForNKS4CommandWrite@0x128f0,
 * 2026-07-16. Genuinely undefined previously (only forward-declared, called
 * throughout command.cpp) despite being the core "send a command word and
 * block until the panel finishes writing it" primitive. Same shape as
 * SubmitNKS4CommandWrite above (this file's own real duplicate of that
 * submit sequence, not a refactor to share code, to stay faithful to the
 * real binary's own two separate, near-identical functions) plus a wait for
 * sDoingWait4Write to clear. wait_event(sDoingWait4Write==0) modeled as a
 * plain poll loop (stg_msleep) - same simplification already established
 * elsewhere in this codebase (e.g. usb.cpp's WaitForFreeBulkWriteURB) rather
 * than reintroducing prepare_to_wait/schedule_timeout/finish_wait as new
 * externs. Real binary logs a "waiting for..." message every 0x33 (51) poll
 * iterations if the write hasn't completed yet - reproduced with the same
 * 51-iteration retry-and-log shape. */
int WaitForNKS4CommandWrite(unsigned int cmd)
{
	sDoingWait4Write = 1;
	WaitForFreeBulkWriteURB(0);
	if (sBulkFreeCommandURBList == (struct urb_node *)&sBulkFreeCommandURBList) {
		printk("<6>OmapNKS4:WaitForNKS4CommandWrite: line 868: SubmitNKS4CommandWrite() fails - no free urbs\n");
	} else if (SubmitNKS4CommandMultipleWriteNONBLOCKING(&cmd, 1) == 0) {
		for (;;) {
			for (int i = 0; i < 0x33; i++) {
				if (sDoingWait4Write == 0) {
					sDoingWait4Write = 0;
					return 0;
				}
				stg_msleep(20);
			}
			printk("<6>OmapNKS4:WaitForNKS4CommandWrite: waiting for NKS4 Command write\n");
		}
	}
	printk("<6>OmapNKS4:WaitForNKS4CommandWrite: line 977: WaitForNKS4CommandWrite fails\n");
	sDoingWait4Write = 0;
	return -1;
}

/* command-prefixed bulk write (Atmel etc.): byte 0 = command, then data. */
int SubmitOmapNKS4CmdBulkWrite(unsigned char command, unsigned char *data, unsigned int nBytes)
{
	if (nBytes >= sMaxWritePacketSize) {
		printk("<6>OmapNKS4: SubmitOmapNKS4CmdBulkWrite() message too long\n");
		return -1;
	}
	WaitForFreeBulkWriteURB(0);
	struct urb_node *n = pop_free_urb(&sBulkFreeCommandURBList);
	if (!n)
		return -1;
	sBulkCommandURBsInUse++;
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
	if (stg_usb_submit_urb(urb, 0x20) == 0) {
		CActiveSenseThread::DoPing();
		return 0;
	}
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
 * not among them. */
int SubmitOmapNKS4BulkWrite(unsigned int *data, unsigned int nBytes)
{
	if (nBytes > sMaxWritePacketSize) {
		printk("<6>OmapNKS4: SubmitOmapNKS4VideoWrite() message too long\n");
		return -1;
	}
	WaitForFreeBulkWriteURB(1);
	struct urb_node *n = pop_free_urb(&sBulkFreeVideoURBList);
	if (!n)
		return -1;
	sBulkVideoURBsInUse++;
	return submit_urb_words(n, data, (nBytes + 3) >> 2, 0, false);
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
static int ApplyGenericCalibration(short *table, short raw)
{
	short result = 0;
	if (raw < table[0]) {
		/* below range: stays 0 */
	} else if (raw < table[1]) {
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

extern "C" int ApplyNKS4Calibration(unsigned int chan, short raw)
{
	if (!sCalibrationData)
		return raw;
	/* 9 shorts (18 bytes) per channel - see ApplyGenericCalibration's own
	 * comment for the field layout. Stride not independently verified
	 * against a real populated table (sCalibrationData is always NULL in
	 * practice above, making this dead code today) - best-effort from the
	 * struct fields ApplyGenericCalibration itself demonstrably reads. */
	short *table = (short *)sCalibrationData + chan * 9;
	return ApplyGenericCalibration(table, raw);
}
