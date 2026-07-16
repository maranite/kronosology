// SPDX-License-Identifier: GPL-2.0
/*
 * usb.c  -  USB driver glue for the NKS4 panel (vendor 0x0944, product 0x1005).
 *
 *   - OmapNKS4Probe       : bind the device, discover endpoints (1 interrupt-IN +
 *                           1 bulk-OUT), bring up the subsystems, build the URB pools
 *                           (1 interrupt URB, 15 command URBs, 256 video URBs).
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
struct urb_node *sBulkFreeVideoURBList;
int sBulkCommandURBsInUse, sBulkVideoURBsInUse;
static void *sBulkCommandURBs[15];	/* command URB pool (sBulkCommandURBs + 14 dwords) */
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
char sCommandWatermarkWaiter, sVideoWatermarkWaiter;
extern int sDoingWait4Write;		/* submit.c */

/* proc handles (procfs.c) */
extern void *gProc, *gProcProgress, *gProcHardwareVersion, *gProcOmapVersion;

/* event queue (procfs.c) */
extern unsigned int sEventQueue[256], sEventQueueReadIndex, sEventQueueWriteIndex,
		    sNumEventsInQueue, sReadIndexModLock;

/* ---- the URB completion handlers (the byte at urb+0x70 tags command vs video) -- */

void WriteCallback(struct urb *urb)
{
	int status = *(int *)((char *)urb + 0x38);
	/* TEMP diagnostic (2026-07-16 debug session) - the real binary is also
	 * silent on a clean status==0 completion (confirmed via decompile), so
	 * this is the only way to see whether bulk-OUT writes are actually
	 * completing at all - remove once the comm-check timeout is
	 * root-caused. */
	printk("<6>OmapNKS4: DIAG WriteCallback fired, status=%d tag=%d\n",
	       status, *(int *)((char *)urb + 0x70));

	if (status != 0) {
		if (status == -104)	printk("<6>OmapNKS4: ERROR: urb wc CONNRESET\n");
		else if (status == -2)	printk("<6>OmapNKS4: ERROR: urb wc NOENT\n");
		else			printk("<6>OmapNKS4: ERROR: urb wc status %02x\n", status);
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
		COmapNKS4Driver_NotifyTransmittedCommandComplete();
		sBulkCommandURBsInUse--;
		node->next = sBulkFreeCommandURBList;
		node->pprev = &sBulkFreeCommandURBList;
		sBulkFreeCommandURBList = node;
		if (sCommandWatermarkWaiter)
			__wake_up(sCommandWaitQueue, 3, 1, 0);
		if (sBulkCommandURBsInUse == 0 && sDoingWait4Write) {
			sDoingWait4Write = 0;
			__wake_up(sCommandWaitQueue, 3, 1, 0);
		}
	}
}

void InterruptCallback(struct urb *urb)
{
	int status = *(int *)((char *)urb + 0x38);
	/* TEMP diagnostic (2026-07-16 debug session) - unconditional, to confirm
	 * whether this callback fires at all during comm-check, independent of
	 * status/len - remove once the comm-check timeout is root-caused. */
	printk("<6>OmapNKS4: DIAG InterruptCallback fired, status=%d len=%u\n",
	       status, *(unsigned int *)((char *)urb + 0x54));

	if (status == -115 || status == 0) {	/* -EINPROGRESS or OK */
		unsigned int len = *(unsigned int *)((char *)urb + 0x54);
		if (len != 0) {
			/* TEMP diagnostic (2026-07-16) - dump the raw interrupt-IN
			 * bytes BEFORE decode, so a reply that arrives but gets
			 * dropped/misrouted by ReceiveEventBuffer's decode path looks
			 * different from a reply that never arrives at all. */
			unsigned char *buf =
				(unsigned char *)*(void **)((char *)urb + 0x40);
			unsigned int dump = len < 64 ? len : 64;
			unsigned int i;
			printk("<6>OmapNKS4: DIAG raw interrupt-IN bytes (len=%u):", len);
			for (i = 0; i < dump; i++)
				printk("<6> %02x", buf[i]);
			printk("<6>\n");
			COmapNKS4Driver_ReceiveEventBuffer(buf, len / 4);
		}
		*(unsigned int *)((char *)urb + 0x68) = sInterruptURBInterval;
		/* mem_flags=0x20 (GFP_ATOMIC) - ground truth: real
		 * InterruptCallback@0x10244 disassembly - this resubmit runs from
		 * interrupt/completion context. */
		stg_usb_submit_urb(urb, 0x20);
	} else {
		printk("<6>OmapNKS4: InterruptCallback() urb->status %d\n", status);
	}
}

/* Block until a free command (or video, type==1) write URB is available. */
void WaitForFreeBulkWriteURB(int type)
{
	if (sDisconnect)
		return;
	if (type == 1) {
		sVideoWatermarkWaiter = 1;
		while (sBulkFreeVideoURBList == (struct urb_node *)&sBulkFreeVideoURBList)
			stg_msleep(20);		/* (binary uses prepare_to_wait/schedule_timeout) */
		sVideoWatermarkWaiter = 0;
	} else {
		sCommandWatermarkWaiter = 1;
		while (sBulkFreeCommandURBList == (struct urb_node *)&sBulkFreeCommandURBList)
			stg_msleep(20);
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
static int alloc_urb_pool(void **pool, int count, void *outEp, struct urb_node **freeList,
                           unsigned int context_tag)
{
	unsigned short wMaxPacketSize = *(unsigned short *)((char *)outEp + 4);
	unsigned char  epAddr         = *(unsigned char  *)((char *)outEp + 2);
	int            devVal         = *sDeviceInstance;

	for (int i = 0; i < count; i++) {
		void *urb = stg_usb_alloc_urb(0, 0);
		pool[i] = urb;
		if (!urb)
			return 0;
		void *buf = kmalloc_buf(0x40);
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

		struct urb_node *node = (struct urb_node *)((char *)urb + 0x14);
		node->next = *freeList;
		node->pprev = freeList;
		*freeList = node;
	}
	return 1;
}

static int alloc_command_urbs(int count, void *outEp)
{
	return alloc_urb_pool(sBulkCommandURBs, count, outEp, &sBulkFreeCommandURBList, 0);
}

static int alloc_video_urbs(int count, void *outEp)
{
	return alloc_urb_pool(sBulkVideoURBs, count, outEp, &sBulkFreeVideoURBList, 1);
}

/* Free the interrupt URB + the 15 command + 256 video URBs and reset the free lists. */
static void free_all_urbs(void)
{
	sBulkFreeCommandURBList = (struct urb_node *)&sBulkFreeCommandURBList;
	sBulkFreeVideoURBList   = (struct urb_node *)&sBulkFreeVideoURBList;
	for (int i = 0; i < 15; i++)  { stg_usb_free_urb(sBulkCommandURBs[i]); sBulkCommandURBs[i] = 0; }
	for (int i = 0; i < 256; i++) { stg_usb_free_urb(sBulkVideoURBs[i]);   sBulkVideoURBs[i] = 0; }
	stg_usb_free_urb(sInterruptURB);
	sInterruptURB = 0;
}

void OmapNKS4Disconnect(void)
{
	printk("<6>OmapNKS4: disconnect\n");
	sDriverState = 2;
	free_all_urbs();
	sDisconnect = 1;
}

void CleanupOmapNKS4Driver(void)
{
	if (sDriverState != 0 && sBulkCommandURBs[0]) {
		/* emergency-stop scan: zero the buffer, set status 4, fire the URB */
		printk("<6>OmapNKS4: about to emergency stop\n");
		/* mem_flags=0xd0 (GFP_KERNEL) - process context (module cleanup/
		 * unload path), same class as OmapNKS4Init's own interrupt URB
		 * submit below. */
		stg_usb_submit_urb(sBulkCommandURBs[0], 0xd0);
		printk("<6>OmapNKS4: done!\n");
	}
	if (gProc) {
		/* Ground truth: names match procfs.cpp's own create_proc_entry() calls
		 * exactly (decompile order confirmed progress/hwversion/omapversion/
		 * root - see CleanupOmapNKS4Driver@0x10d70). parent=0 (top-level /proc
		 * entries, matching create_proc_entry's own implied parent). */
		remove_proc_entry("nks4progress", 0);
		remove_proc_entry("nks4hwversion", 0);
		remove_proc_entry("nks4omapversion", 0);
		remove_proc_entry("nks4", 0);
		gProc = gProcProgress = gProcHardwareVersion = gProcOmapVersion = 0;
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

	/* 1. identity check: vendor 0x0944 product 0x1005 */
	if (*(short *)((char *)dev + 0xba) != 0x1005 ||
	    *(short *)((char *)dev + 0xb8) != 0x0944) {
		printk("<6>OmapNKS4: wrong ID set: vendor %04x product %04x\n",
		       *(short *)((char *)dev + 0xb8), *(short *)((char *)dev + 0xba));
		return -19;			/* -ENODEV */
	}
	if (sDeviceInstance) {
		printk("<6>OmapNKS4: DANGER! 2nd OmapNKS4 detected. Not supported\n");
		return -16;			/* -EBUSY */
	}

	CSTGOmapNKS4Fifos::sInstance.Initialize(0);
	COmapNKS4VideoAPI_Initialize();
	sDeviceInstance = (int *)(dev - 100);

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
			else printk("<6>OmapNKS4: DANGER! found additional interrupt in endpoint!\n");
		} else if (xferType == 2 && !dirIn) {
			nOut++;
			if (nOut == 1) outEp = ep;
			else printk("<6>OmapNKS4: DANGER! found additional write out endpoint!\n");
		} else {
			printk("<6>OmapNKS4: Unsupported endpoint found 0x%02x/0x%04x\n",
			       bEndpointAddress, bmAttributes);
		}
	}

	if (nInt != 1 || nOut != 1) {
		printk("<6>OmapNKS4: fatal: found %d bulk write and %d interrupt endpoint/s\n",
		       nOut, nInt);
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

	/* 4. interrupt URB + transfer buffer (0x220 bytes) */
	sInterruptURB = stg_usb_alloc_urb(0, 0);
	void *intBuf = sInterruptURB ? kmalloc_buf(0x220) : 0;
	if (!sInterruptURB || !intBuf) {
		printk("<6>OmapNKS4: fatal: cannot allocate interrupt URB/buffer\n");
		free_all_urbs();
		return -12;
	}
	configure_interrupt_urb(sInterruptURB, intEp, intBuf, dev);

	/* 5. 15 command URBs + 256 video URBs, each with a kmalloc'd buffer (0x40) */
	if (!alloc_command_urbs(15, outEp) || !alloc_video_urbs(256, outEp)) {
		free_all_urbs();
		return -12;
	}

	/* 6. success */
	sMaxWritePacketSize = *(unsigned short *)((char *)outEp + 4);
	__init_waitqueue_head(sCommandWaitQueue, (void *)sWaitQueueLockKeyDummy);
	__init_waitqueue_head(sVideoWaitQueue, (void *)sWaitQueueLockKeyDummy);
	__init_waitqueue_head(sReadWaitQueue, (void *)sWaitQueueLockKeyDummy);
	sDriverState = 1;
	printk("<6>OmapNKS4: probe success\n");
	/* complete(&sProbeComplete) - wakes OmapNKS4Init, which waits on probe via
	 * wait_for_completion (main.cpp, matching daemon_lifecycle.cpp's own
	 * complete()/wait_for_completion() pairing convention). sProbeComplete itself
	 * is declared and waited on in main.cpp - NOT yet fixed this session (main.cpp
	 * not attempted), so this call is left referencing the not-yet-declared
	 * completion object rather than guessing a wrong stand-in. */
	complete(&sProbeComplete);
	return 0;
}
