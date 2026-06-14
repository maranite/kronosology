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

/* ---- URB pools (free lists are intrusive doubly-linked, head points to self) --- */
struct urb_node { struct urb_node *next, **pprev; };
static struct urb_node *sBulkFreeCommandURBList;
static struct urb_node *sBulkFreeVideoURBList;
static int sBulkCommandURBsInUse, sBulkVideoURBsInUse;
static void *sBulkCommandURBs[15];	/* command URB pool (sBulkCommandURBs + 14 dwords) */
static void *sBulkVideoURBs[256];	/* video URB pool   */
static void *sInterruptURB;
static unsigned int sInterruptURBInterval;

static int sDriverState;		/* 0 none, 1 up, 2 disconnected, 3 probing */
static int sDisconnect;
static int *sDeviceInstance;
unsigned int sMaxWritePacketSize;
static int sSTG2NKS4SrqNumber;
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
			__wake_up(0);
	} else {						/* command URB */
		COmapNKS4Driver_NotifyTransmittedCommandComplete();
		sBulkCommandURBsInUse--;
		node->next = sBulkFreeCommandURBList;
		node->pprev = &sBulkFreeCommandURBList;
		sBulkFreeCommandURBList = node;
		if (sCommandWatermarkWaiter)
			__wake_up(0);
		if (sBulkCommandURBsInUse == 0 && sDoingWait4Write) {
			sDoingWait4Write = 0;
			__wake_up(0);
		}
	}
}

void InterruptCallback(struct urb *urb)
{
	int status = *(int *)((char *)urb + 0x38);

	if (status == -115 || status == 0) {	/* -EINPROGRESS or OK */
		unsigned int len = *(unsigned int *)((char *)urb + 0x54);
		if (len != 0)
			COmapNKS4Driver_ReceiveEventBuffer(
				(unsigned char *)*(void **)((char *)urb + 0x40),
				len / 4);
		*(unsigned int *)((char *)urb + 0x68) = sInterruptURBInterval;
		stg_usb_submit_urb(urb, 0);
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
		stg_usb_submit_urb(sBulkCommandURBs[0], 0);
		printk("<6>OmapNKS4: done!\n");
	}
	if (gProc) {
		remove_proc_entry();		/* progress, hwversion, omapversion, root */
		remove_proc_entry();
		remove_proc_entry();
		remove_proc_entry();
		gProc = gProcProgress = gProcHardwareVersion = gProcOmapVersion = 0;
	}
	if (sSTG2NKS4SrqNumber > 0) {
		CSTGOmapNKS4Fifos::sInstance.Initialize(0);
		COmapNKS4Driver_Cleanup();
	}
	stg_usb_deregister();
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
	/* ... (endpoint table walk over intf->cur_altsetting->endpoint[], 0x2c bytes each;
	 *      classifies by bmAttributes&3 == interrupt(3)/bulk(2) and direction bit) ... */

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
	COmapNKS4_SetMaxBulkOutMsgSize();

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
	__init_waitqueue_head();		/* command / video / read wait queues */
	__init_waitqueue_head();
	__init_waitqueue_head();
	sDriverState = 1;
	printk("<6>OmapNKS4: probe success\n");
	complete();				/* wake OmapNKS4Init which waits on probe */
	return 0;
}
