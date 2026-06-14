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
unsigned int sMaxWritePacketSize;

static unsigned int *sWaitReadPtr;	/* where to deliver the next response word */
int sDoingWait4Write;
static int sShutdownDelay;

/* thread run/signal flags (main.c) */
extern int sProcessMsgThreadRunning, sVideoMsgSignalled;
extern int sShutdownSSDThreadRunning, sShutdownSSDSignaled;

/* ---- small helpers ----------------------------------------------------- */

unsigned int NumBytesToNumInts(int bytes)		{ return (unsigned)(bytes + 3) >> 2; }

/* byte-swap the two 16-bit halves of each of nInts words (panel is big-endian) */
void ReverseMessage(unsigned int *words, int nInts)
{
	for (; nInts; nInts--, words++) {
		unsigned short lo = (unsigned short)*words, hi = (unsigned short)(*words >> 16);
		*words = ((unsigned)((lo >> 8) | (lo << 8))) |
			 ((unsigned)((hi >> 8) | (hi << 8)) << 16);
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
		__wake_up(0);
	}
}

int WaitForNKS4ReadEvent(unsigned int *resp)
{
	sWaitReadPtr = resp;
	if (!resp)
		return 0;
	/* wait up to ~1000 jiffies for SendNKS4EventToLinuxReader() to fill *resp */
	int t = 1000;
	while (sWaitReadPtr && t)
		t = schedule_timeout_wait();
	if (sWaitReadPtr == 0)
		return 0;
	printk("<6>OmapNKS4: WaitForNKS4ReadEvent() timed out\n");
	sWaitReadPtr = 0;
	return -1;
}

void WaitOnAtmelRead(void)            { sleep_on_timeout(); }
void SignalAtmelReadComplete(void)    { __wake_up(0); }

void SignalVideoMessageProcessor(void)
{
	if (sProcessMsgThreadRunning) { sVideoMsgSignalled = 1; __wake_up(0); }
}

void SignalShutdownSSD(void)
{
	if (sShutdownSSDThreadRunning) { sShutdownSSDSignaled = 1; __wake_up(0); }
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

/* fill the URB transfer buffer with the (byte-swapped) command words and submit */
static int submit_urb_words(struct urb_node *node, const unsigned int *words, unsigned int nInts,
			    unsigned int prefixLenBytes)
{
	void *urb = (char *)node - 0x14;
	unsigned int *buf = (unsigned int *)*(void **)((char *)urb + 0x2c);

	*(unsigned int *)((char *)urb + 0x3c) = nInts * 4 + prefixLenBytes;
	for (unsigned int i = 0; i < nInts; i++)
		buf[i] = words[i];
	ReverseMessage(buf, NumBytesToNumInts(nInts * 4 + prefixLenBytes));

	if (stg_usb_submit_urb(urb, 0) == 0) {
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
	return submit_urb_words(n, cmds, nInts, 0);
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
	void *urb = (char *)n - 0x14;
	unsigned char *buf = (unsigned char *)*(void **)((char *)urb + 0x2c);
	buf[0] = command;
	for (unsigned int i = 0; i < nBytes; i++)
		buf[i + 1] = data[i];
	unsigned int words = (nBytes + 4) >> 2;
	*(unsigned int *)((char *)urb + 0x3c) = words * 4;
	ReverseMessage((unsigned int *)buf, words);
	if (stg_usb_submit_urb(urb, 0) == 0) {
		CActiveSenseThread::DoPing();
		return 0;
	}
	return -1;
}

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
	return submit_urb_words(n, data, (nBytes + 3) >> 2, 0);
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
extern void *sCalibrationData;
int ApplyNKS4Calibration(unsigned int chan, short raw)
{
	if (!sCalibrationData)
		return raw;
	return ApplyGenericCalibration(chan, raw);	/* per-channel curve lookup */
}
