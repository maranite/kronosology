// SPDX-License-Identifier: GPL-2.0
/*
 * video.cpp  -  COmapNKS4VideoAPI: the colour-LCD draw pipeline.
 *
 *  Producers (InitLCDRegs / XAxisByteSize / SendPixelDataRegion / SendFillData /
 *  UpdateColorPal) push 33-byte draw "events" into a 384-deep ring and wake the worker.
 *  The worker (ProcessEvents, driven by the video message-processor thread) pops one
 *  event, emits the matching USB video-bulk packet, and for a pixel-region blit
 *  (opcode 0xc2) streams the frame-buffer rows out in 0x200-byte chunks
 *  (ContinueProcessingEvent) until the region is done.
 *
 *  Event opcodes:  0xc0 init-LCD-reg  0x81 x-axis-bytesize  0xc2 pixel-region
 *                  0xc4 fill  0xc5 colour-palette   (0xc6 data / 0x83 end on the wire)
 */

#include "omapnks4_internal.h"

/* shared with the worker thread */
extern "C" {
volatile int    sEventsToProcess;	/* events queued for the worker */
unsigned char  *sCurrentRegionTransferInfo;	/* frame-buffer cursor for the blit */
/* NOT extern: module-private state, assigned throughout this file, never
 * given a real definition anywhere - confirmed genuinely unresolved at
 * insmod ("Unknown symbol DAT_0000ed0c/DAT_0000ed10", 2026-07-16). Same bug
 * class as usb.cpp's sBulkFreeCommandURBList/etc (extern where a plain
 * local definition was needed), just for a leftover Ghidra placeholder name
 * instead of a real one - kept as-is (not renamed) since it's already
 * documented above. */
int             DAT_0000ed0c;		/* remaining columns in current row */
int             DAT_0000ed10;		/* row width (pixels) of the region */
}

struct COmapNKS4VideoAPI g_video;	/* COmapNKS4VideoAPI::sInstance */

/*
 * The producers spin-wait (msleep 1ms, up to 12 times) while the ring is full, then
 * give up with -3.  Returns true if it gave up (ring still full).
 */
static bool ring_full_wait(void)
{
	for (int i = 0; i < 12; i++) {
		if (sEventsToProcess <= 0x17f)
			return false;
		stg_msleep(1);
	}
	return true;
}

/* commit pEvents[dwWriteIndex] and wake the worker */
static unsigned int commit_event(struct COmapNKS4VideoAPI *p)
{
	p->dwWriteIndex++;
	if (p->dwWriteIndex > 0x17f)
		p->dwWriteIndex = 0;
	__sync_fetch_and_add(&sEventsToProcess, 1);
	SignalVideoMessageProcessor();
	return 0;
}

COmapNKS4VideoAPI::COmapNKS4VideoAPI(void)
{
	dwWriteIndex = 0;
	dwMaxEventIndex = 0x17f;
	dwReadIndex = 0;
	dwProcessingActive = 0;
	dwTransferRowSize = 0x200;
	dwScreenBase = 0;
	dwScreenWidth = 800;
	dwScreenHeight = 600;
	sEventsToProcess = 0;
	printk("COmapNKS4VideoAPI::UpdateScreenInfo() base = 0x%x, X= %d, Y=%d\n", 0, 800, 600);
}

/* ---- draw builders ----------------------------------------------------- */

int COmapNKS4VideoAPI::InitLCDRegs(char reg, char val, int data)
{
	if (ring_full_wait())
		return -3;
	struct OmapNKS4VideoAPIEvent *e = &pEvents[dwWriteIndex];
	e->bOpcode = 0xc0;
	((char *)&e->dwArg0)[0] = reg;
	((char *)&e->dwArg0)[1] = val;
	e->dwArg1 = data;
	return commit_event(this);
}

int COmapNKS4VideoAPI::XAxisByteSize(int bytes)
{
	if (ring_full_wait())
		return -3;
	struct OmapNKS4VideoAPIEvent *e = &pEvents[dwWriteIndex];
	e->bOpcode = 0x81;
	e->dwArg0 = bytes;
	return commit_event(this);
}

int COmapNKS4VideoAPI::SendPixelDataRegion(int width, int offset, int rowBytes)
{
	if (ring_full_wait())
		return -3;
	struct OmapNKS4VideoAPIEvent *e = &pEvents[dwWriteIndex];
	e->bOpcode = 0xc2;
	e->dwArg0 = width;	/* columns to copy per row */
	e->dwArg1 = offset;	/* byte offset into the frame buffer */
	e->dwArg2 = rowBytes;	/* row width (stride) of the region */
	return commit_event(this);
}

int COmapNKS4VideoAPI::SendFillData(unsigned char color, int width, int base, int height)
{
	if (ring_full_wait())
		return -3;
	struct OmapNKS4VideoAPIEvent *e = &pEvents[dwWriteIndex];
	e->bOpcode = 0xc4;
	((unsigned char *)&e->dwArg0)[0] = color;
	e->dwArg2 = base;	/* +9  */
	e->dwArg3 = height;	/* +13 */
	e->dwArg1 = width;	/* +5  */
	return commit_event(this);
}

int COmapNKS4VideoAPI::UpdateColorPal(char a, char b, char c, char d)
{
	if (ring_full_wait())
		return -3;
	struct OmapNKS4VideoAPIEvent *e = &pEvents[dwWriteIndex];
	e->bOpcode = 0xc5;
	((char *)&e->dwArg0)[0] = a;
	((char *)&e->dwArg0)[1] = b;
	((char *)&e->dwArg0)[2] = c;
	((char *)&e->dwArg0)[3] = d;
	return commit_event(this);
}

int COmapNKS4VideoAPI::UpdateScreenInfo(char *base, int x, int y)
{
	dwScreenHeight = y;
	dwScreenBase = (unsigned int)base;
	dwScreenWidth = x;
	printk("COmapNKS4VideoAPI::UpdateScreenInfo() base = 0x%x, X= %d, Y=%d\n", base, x, y);
	return 0;
}

/* ---- ring helpers ------------------------------------------------------ */

struct OmapNKS4VideoAPIEvent *COmapNKS4VideoAPI::GetNextFreeFifoEvent(void)
{
	if (ring_full_wait())
		return 0;
	return &pEvents[dwWriteIndex];
}

unsigned int COmapNKS4VideoAPI::AddFifoEvent(struct OmapNKS4VideoAPIEvent *event)
{
	if (event != &pEvents[dwWriteIndex])
		return 0xfffffffb;
	return commit_event(this);
}

unsigned int COmapNKS4VideoAPI::GetNextEventToProcess(struct OmapNKS4VideoAPIEvent *event)
{
	if (sEventsToProcess == 0)
		return 0;
	*event = pEvents[dwReadIndex];
	__sync_fetch_and_sub(&sEventsToProcess, 1);
	dwReadIndex++;
	if (dwReadIndex > 0x17f)
		dwReadIndex = 0;
	return 1;
}

/* ---- worker ------------------------------------------------------------ */

unsigned int COmapNKS4VideoAPI::ProcessEvents(void)
{
	if (dwProcessingActive == 1) {
		ContinueProcessingEvent(&currentEvent);
		return 1;
	}
	if (sEventsToProcess == 0)
		return 0;

	currentEvent = pEvents[dwReadIndex];
	__sync_fetch_and_sub(&sEventsToProcess, 1);
	dwReadIndex++;
	if (dwReadIndex > 0x17f)
		dwReadIndex = 0;

	dwProcessingActive = 1;
	unsigned char op = currentEvent.bOpcode;

	if (op == 0xc2) {		/* pixel-region blit: set up the streaming state */
		SubmitOmapNKS4VideoWrite(0, 0);
		sCurrentRegionTransferInfo = (unsigned char *)(dwScreenBase + currentEvent.dwArg1);
		DAT_0000ed0c = currentEvent.dwArg0;
		DAT_0000ed10 = currentEvent.dwArg2;
		ContinueProcessingEvent(&currentEvent);
		return 1;
	}
	if (op == 0x81 || op == 0xc0 || op == 0xc4 || op == 0xc5)
		SubmitOmapNKS4VideoWrite(0, 0);

	dwProcessingActive = 0;
	return 1;
}

/*
 * Stream one 0x200-byte chunk of the pixel region out as a 0xc6 data packet
 * (byte-swapped), advancing the frame-buffer cursor row by row.  When the region is
 * exhausted, send a 0x83 end marker and clear dwProcessingActive.
 */
void COmapNKS4VideoAPI::ContinueProcessingEvent(struct OmapNKS4VideoAPIEvent *event)
{
	if (event->bOpcode != 0xc2)
		return;

	unsigned int packet[0x88];	/* 0x220 bytes on stack in the binary */
	unsigned char *out = (unsigned char *)packet;
	out[0] = 0xc6;

	int limit = dwTransferRowSize - 1;
	int i = 0;
	int colsLeft = DAT_0000ed0c;
	if (limit > 0) {
		do {
			while (DAT_0000ed10 > 0) {
				if (DAT_0000ed0c < 1)
					goto pad;
				out[i + 1] = *sCurrentRegionTransferInfo++;
				DAT_0000ed10--;
				i++;
				DAT_0000ed0c--;
				colsLeft = DAT_0000ed0c;
				if (i >= limit)
					goto done;
			}
			DAT_0000ed10 = event->dwArg2;
			sCurrentRegionTransferInfo += (dwScreenWidth - DAT_0000ed10);
			if (DAT_0000ed0c > 0)
				continue;
pad:
			out[i + 1] = 0;
			i++;
			DAT_0000ed10--;
		} while (i < limit);
	}
done:
	DAT_0000ed0c = colsLeft;
	/* byte-swap the 16-bit halves of every word in the packet */
	for (unsigned int *q = packet; q != packet + 0x88; q++) {
		unsigned int v = *q;
		unsigned short lo = (unsigned short)v, hi = (unsigned short)(v >> 16);
		*q = ((unsigned int)((lo >> 8) | (lo << 8))) |
		     ((unsigned int)((hi >> 8) | (hi << 8)) << 16);
	}
	SubmitOmapNKS4VideoWrite(packet, sizeof(packet));

	if (DAT_0000ed0c < 1) {		/* region complete -> end marker */
		out[3] = 0x83;
		SubmitOmapNKS4VideoWrite(packet, sizeof(packet));
		dwProcessingActive = 0;
	}
}

/* ---- C-ABI wrappers (operate on the singleton) ------------------------- */
extern "C" {
void COmapNKS4VideoAPI_Initialize(void)                 { new (&g_video) COmapNKS4VideoAPI(); }
void OmapNKS4VideoAPIProcessEvents(void)                { g_video.ProcessEvents(); }
int  OmapNKS4InitLCDRegs(char r, char v, int d)         { return g_video.InitLCDRegs(r, v, d); }
int  OmapNKS4XAxisByteSize(int b)                       { return g_video.XAxisByteSize(b); }
int  OmapNKS4SendPixelDataRegion(int w, int o, int rb)  { return g_video.SendPixelDataRegion(w, o, rb); }
int  OmapNKS4SendFillData(unsigned char c, int w, int base, int h) { return g_video.SendFillData(c, w, base, h); }
int  OmapNKS4UpdateColorPal(char a, char b, char c, char d){ return g_video.UpdateColorPal(a, b, c, d); }
int  OmapNKS4UpdateScreenInfo(char *base, int x, int y) { return g_video.UpdateScreenInfo(base, x, y); }
/* used by driver.cpp's progress bar */
int  OmapNKS4VideoAPI_SendFillData(struct COmapNKS4VideoAPI *self, unsigned char color,
				   int w, int base, int h) { return self->SendFillData(color, w, base, h); }
/* Ground truth (fresh Ghidra decompile + disassembly, 2026-07-15,
 * COmapNKS4_SetMaxBulkOutMsgSize@0x17540): a one-line setter for
 * dwTransferRowSize (the pixel-data-chunk copy-loop bound ContinueProcessingEvent
 * uses - see that function's own comment). Was previously entirely undefined
 * (only forward-declared in main.cpp, never implemented anywhere) - found while
 * fixing a real Kbuild build attempt. usb.cpp's call site (OmapNKS4Probe,
 * `MOVZX EAX,word ptr [outEp+4]` immediately before the call) passes the bulk-OUT
 * endpoint's raw wMaxPacketSize - NOT divided by 4 like COmapNKS4Driver_Initialize's
 * argument - so at runtime this overrides the constructor's default 0x200 with the
 * real negotiated USB max packet size. */
void COmapNKS4_SetMaxBulkOutMsgSize(unsigned int maxPacketSize)
{
	g_video.dwTransferRowSize = maxPacketSize;
}
}
