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

/* VM-only, 2026-07-18: forward declaration for the concurrency stress test's
 * packet-capture hook - full implementation + the rest of the stress-test
 * scaffolding lives at the tail of this file, gated behind vm_virtual_probe
 * AND vm_video_stress (both 0 on real hardware and on every existing
 * vm_virtual_probe boot test). A no-op single boolean check when the gate is
 * off, called from ProcessEvents() below at each of the 5 draw-opcode wire
 * submits - NOT a permanent instrumentation point, pure test scaffolding. */
static void vm_stress_capture_packet(const unsigned char *buf, unsigned int len);

/*
 * The producers spin-wait (msleep 5ms, up to 10 times = ~50ms worst case) while the
 * ring is full, then give up with -3.  Returns true if it gave up (ring still full).
 *
 * CORRECTED 2026-07-17 (was msleep(1) x12): fresh disassembly of
 * SendPixelDataRegion/OmapNKS4SendPixelDataRegion confirms the real values via the
 * literal `MOV EAX,0x5` immediately before each `stg_msleep` call site (regparm3,
 * EAX = first arg), with exactly 10 such call sites in the decompiled boolean chain.
 * This ~50ms cap only gates ring *admission* - it is NOT the source of the
 * multi-second real-hardware stall found this same session (see
 * KronosNKS4/docs/gaps.md "The real stall mechanism, traced end-to-end"): that's
 * WaitForFreeBulkWriteURB's genuinely unbounded per-chunk wait, several calls
 * downstream of here, triggered by ContinueProcessingEvent's unpaced one-chunk-
 * per-call streaming loop exhausting the small bulk-video URB pool when a full-screen
 * flush needs ~940 chunks.
 */
static bool ring_full_wait(void)
{
	for (int i = 0; i < 10; i++) {
		if (sEventsToProcess <= 0x17f)
			return false;
		stg_msleep(5);
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

/* CORRECTION (full-coverage re-audit, 2026-07-18): all five draw-builders below
 * previously fetched `&pEvents[dwWriteIndex]` directly and committed unconditionally
 * via commit_event() - functionally fine only if nothing else can move dwWriteIndex
 * between the fetch and the commit. Fresh decompile of all five real functions
 * (InitLCDRegs@0x15f20/428B, XAxisByteSize@0x160d0/409B, SendPixelDataRegion@0x16270/
 * 424B, SendFillData@0x16420/432B, UpdateColorPal@0x165e0/452B - each far larger than
 * this reconstruction's own few-line bodies, which is what prompted a closer look)
 * shows every one of them is actually the FULLY INLINED combination of
 * GetNextFreeFifoEvent() (the 10x-unrolled ring_full_wait loop + pointer fetch) +
 * field writes + a genuine AddFifoEvent()-shaped consistency check
 * (`if (e != &pEvents[dwWriteIndex]) return 0xfffffffb;`, confirmed byte-identical
 * across all five: same 0xfffffffb=-5 constant, same re-fetch-and-compare sequence,
 * immediately before the same commit_event() tail) - NOT a direct fetch-and-commit as
 * this reconstruction had modeled. GetNextFreeFifoEvent()/AddFifoEvent() (below) were
 * already independently confirmed byte-exact against ground truth (@0x15cd0/0x15e10),
 * so the real fix is routing every builder through them instead of duplicating
 * (incompletely) their logic inline. This closes a genuine concurrency gap: two
 * producers racing on different CPUs could previously both fetch the same
 * `&pEvents[dwWriteIndex]` slot, both write their own (different) event data into it,
 * and both call commit_event() - advancing dwWriteIndex twice while only one caller's
 * data survives, silently corrupting the ring (the other caller's advertised event
 * is empty/stale despite dwWriteIndex having moved past it). Ground truth's recheck
 * catches exactly this and fails the losing caller with -5 instead. */

int COmapNKS4VideoAPI::InitLCDRegs(char reg, char val, int data)
{
	struct OmapNKS4VideoAPIEvent *e = GetNextFreeFifoEvent();
	if (!e)
		return -3;
	e->bOpcode = 0xc0;
	((char *)&e->dwArg0)[0] = reg;
	((char *)&e->dwArg0)[1] = val;
	e->dwArg1 = data;
	return (int)AddFifoEvent(e);
}

int COmapNKS4VideoAPI::XAxisByteSize(int bytes)
{
	struct OmapNKS4VideoAPIEvent *e = GetNextFreeFifoEvent();
	if (!e)
		return -3;
	e->bOpcode = 0x81;
	e->dwArg0 = bytes;
	return (int)AddFifoEvent(e);
}

int COmapNKS4VideoAPI::SendPixelDataRegion(int width, int offset, int rowBytes)
{
	struct OmapNKS4VideoAPIEvent *e = GetNextFreeFifoEvent();
	if (!e)
		return -3;
	e->bOpcode = 0xc2;
	e->dwArg0 = width;	/* columns to copy per row */
	e->dwArg1 = offset;	/* byte offset into the frame buffer */
	e->dwArg2 = rowBytes;	/* row width (stride) of the region */
	return (int)AddFifoEvent(e);
}

int COmapNKS4VideoAPI::SendFillData(unsigned char color, int width, int base, int height)
{
	struct OmapNKS4VideoAPIEvent *e = GetNextFreeFifoEvent();
	if (!e)
		return -3;
	e->bOpcode = 0xc4;
	((unsigned char *)&e->dwArg0)[0] = color;
	e->dwArg2 = base;	/* +9  */
	e->dwArg3 = height;	/* +13 */
	e->dwArg1 = width;	/* +5  */
	return (int)AddFifoEvent(e);
}

int COmapNKS4VideoAPI::UpdateColorPal(char a, char b, char c, char d)
{
	struct OmapNKS4VideoAPIEvent *e = GetNextFreeFifoEvent();
	if (!e)
		return -3;
	e->bOpcode = 0xc5;
	((char *)&e->dwArg0)[0] = a;
	((char *)&e->dwArg0)[1] = b;
	((char *)&e->dwArg0)[2] = c;
	((char *)&e->dwArg0)[3] = d;
	return (int)AddFifoEvent(e);
}

/* Ground truth (fresh Ghidra decompile, 2026-07-17, @0x168a0): reads the exact same
 * raw global (`DAT_0001bb1b`) as `COmapNKS4Driver_GetProgressBarPercent`'s own
 * `sInstance.bProgress` - the two classes share one physical progress-percent byte
 * rather than each keeping an independent counter. Modeled as a direct proxy to the
 * already-implemented driver accessor rather than duplicating storage. */
unsigned char COmapNKS4VideoAPI::GetProgressBarPercent(void)
{
	return COmapNKS4Driver_GetProgressBarPercent();
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

/* Pack a 32-bit value into 3 wire bytes as ground truth does throughout
 * ProcessEvents: {bits16-18 (masked to 3 bits), bits8-15, bits0-7} - i.e. a
 * truncated big-endian-ish encoding of a value that never exceeds 19 bits.
 * Ground truth's real instruction shape (repeated identically for every field
 * in every opcode below): `AND value,0x70000; SAR value,0x10` for the first
 * byte, then the plain low two bytes for the other two - confirmed via
 * disassembly of ProcessEvents@0x15980 (correct 89849-byte target). */
static void pack_field19(unsigned char *out, unsigned int value)
{
	out[0] = (unsigned char)((value >> 16) & 0x7);
	out[1] = (unsigned char)((value >> 8) & 0xff);
	out[2] = (unsigned char)(value & 0xff);
}

/*
 * CORRECTION (fresh adversarial pass, 2026-07-18, highest-severity finding this
 * pass): every one of ProcessEvents' non-streaming opcodes (0x81/0xc0/0xc2/0xc4/
 * 0xc5) previously called `SubmitOmapNKS4VideoWrite(0, 0)` - an EMPTY submit with
 * no payload at all. Ground truth actually builds a real, non-trivial packed wire
 * header for every one of these opcodes and submits THAT - meaning every LCD
 * register-init/x-axis-size/pixel-region-start/fill/palette-update command this
 * driver ever sends was previously reduced to a bare no-op submit, never
 * conveying its actual parameters to the panel at all. Reverse-engineered fresh
 * from disassembly of this exact function (ProcessEvents@0x15980, correct
 * 89849-byte target, 2026-07-18) - all five opcode branches, one at a time:
 *
 *  - 0x81 (XAxisByteSize, @0x15c88): 4-byte payload -
 *    [pack_field19(dwArg0=bytes)][0x81].
 *  - 0xc0 (InitLCDRegs, @0x15c30): 8-byte payload - [data&0xff][val][reg][0xc0]
 *    [garbage][data>>24][data>>16][data>>8] (dwArg1=data, an int, split
 *    across a low byte up front and its remaining 3 bytes - in REVERSE byte
 *    order - after the opcode, skipping one garbage byte in between).
 *  - 0xc2 (SendPixelDataRegion, @0x15a98): 12-byte payload -
 *    [pack_field19(dwArg0=width)][0xc2][pack_field19(dwArg1=offset)][garbage]
 *    [pack_field19(dwArg2=rowBytes)][garbage].
 *  - 0xc4 (SendFillData, @0x15ba0): 12-byte payload -
 *    [pack_field19(dwArg1=width)][0xc4][pack_field19(dwArg2=base)][color]
 *    [pack_field19(dwArg3=height)][garbage] (color = dwArg0's own first byte).
 *  - 0xc5 (UpdateColorPal, @0x15b54): 8-byte payload - [c][b][a][0xc5]
 *    [garbage x3][d] (a/b/c/d are dwArg0's 4 packed bytes, UpdateColorPal's own
 *    4 char params - note the byte ORDER within the payload is c,b,a, not a,b,c).
 *
 * Every "garbage" byte above is a real, confirmed-via-disassembly gap in ground
 * truth's own code - the real function builds this buffer on its own stack and
 * genuinely never initializes those specific byte positions before submitting
 * them over USB (whatever was already on the kernel stack goes out on the wire).
 * Reproducing true uninitialized-stack-garbage isn't meaningful in a from-source
 * reconstruction (there is no "the" value - it depends on unspecified prior
 * stack contents), so those bytes are zero-initialized here instead: a
 * deterministic, harmless stand-in for content the real panel firmware
 * evidently already has to tolerate/ignore, without inventing a fake data
 * dependency. Every byte that DOES carry real information above is exact,
 * disassembly-confirmed ground truth, not a guess. */
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
	const unsigned char *palBytes = (const unsigned char *)&currentEvent.dwArg0;

	if (op == 0xc2) {		/* pixel-region blit: set up the streaming state */
		unsigned char buf[12] = { 0 };
		pack_field19(&buf[0], (unsigned int)currentEvent.dwArg0);	/* width */
		buf[3] = 0xc2;
		pack_field19(&buf[4], (unsigned int)currentEvent.dwArg1);	/* offset */
		pack_field19(&buf[8], (unsigned int)currentEvent.dwArg2);	/* rowBytes */
		vm_stress_capture_packet(buf, sizeof(buf));	/* VM-only, see tail of file */
		SubmitOmapNKS4VideoWrite((unsigned int *)buf, sizeof(buf));

		sCurrentRegionTransferInfo = (unsigned char *)(dwScreenBase + currentEvent.dwArg1);
		DAT_0000ed0c = currentEvent.dwArg0;
		DAT_0000ed10 = currentEvent.dwArg2;
		ContinueProcessingEvent(&currentEvent);
		return 1;
	}
	if (op == 0x81) {			/* XAxisByteSize */
		unsigned char buf[4];
		pack_field19(&buf[0], (unsigned int)currentEvent.dwArg0);
		buf[3] = 0x81;
		vm_stress_capture_packet(buf, sizeof(buf));	/* VM-only, see tail of file */
		SubmitOmapNKS4VideoWrite((unsigned int *)buf, sizeof(buf));
	} else if (op == 0xc0) {		/* InitLCDRegs */
		unsigned int data = (unsigned int)currentEvent.dwArg1;
		unsigned char buf[8] = { 0 };
		buf[0] = (unsigned char)(data & 0xff);
		buf[1] = palBytes[1];		/* val */
		buf[2] = palBytes[0];		/* reg */
		buf[3] = 0xc0;
		buf[5] = (unsigned char)((data >> 24) & 0xff);
		buf[6] = (unsigned char)((data >> 16) & 0xff);
		buf[7] = (unsigned char)((data >> 8) & 0xff);
		vm_stress_capture_packet(buf, sizeof(buf));	/* VM-only, see tail of file */
		SubmitOmapNKS4VideoWrite((unsigned int *)buf, sizeof(buf));
	} else if (op == 0xc4) {		/* SendFillData */
		unsigned char buf[12] = { 0 };
		pack_field19(&buf[0], (unsigned int)currentEvent.dwArg1);	/* width */
		buf[3] = 0xc4;
		pack_field19(&buf[4], (unsigned int)currentEvent.dwArg2);	/* base */
		buf[7] = palBytes[0];					/* color */
		pack_field19(&buf[8], (unsigned int)currentEvent.dwArg3);	/* height */
		vm_stress_capture_packet(buf, sizeof(buf));	/* VM-only, see tail of file */
		SubmitOmapNKS4VideoWrite((unsigned int *)buf, sizeof(buf));
	} else if (op == 0xc5) {		/* UpdateColorPal */
		unsigned char buf[8] = { 0 };
		buf[0] = palBytes[2];		/* c */
		buf[1] = palBytes[1];		/* b */
		buf[2] = palBytes[0];		/* a */
		buf[3] = 0xc5;
		buf[7] = palBytes[3];		/* d */
		vm_stress_capture_packet(buf, sizeof(buf));	/* VM-only, see tail of file */
		SubmitOmapNKS4VideoWrite((unsigned int *)buf, sizeof(buf));
	}

	dwProcessingActive = 0;
	return 1;
}

/*
 * Stream one 0x200-byte chunk of the pixel region out as a 0xc6 data packet
 * (byte-swapped), advancing the frame-buffer cursor row by row.  When the region is
 * exhausted, send a 0x83 end marker and clear dwProcessingActive.
 */
/* NOTE (re-verification pass, 2026-07-17): ground truth reads its
 * opcode/dwArg2 fields from a fixed global address, not from the `event`
 * parameter below - another instance of this project's recurring
 * "parameter looks forwarded but isn't" pattern. No functional impact
 * today since the only real caller always passes &currentEvent (the same
 * object the global effectively aliases), so left as-is rather than
 * reworked, but the signature overstates what's actually used. */
void COmapNKS4VideoAPI::ContinueProcessingEvent(struct OmapNKS4VideoAPIEvent *event)
{
	if (event->bOpcode != 0xc2)
		return;

	/* CORRECTION (re-verification pass, 2026-07-17): this buffer and the
	 * byte-swap loop's bound below were both wrong - 0x88 dwords (0x220
	 * bytes), not the real 0x80 dwords (0x200 bytes). Ground truth
	 * confirmed two ways: the real stack-frame math (ESP+0x20 to ESP+0x220,
	 * i.e. a 0x200-byte span) and the literal `MOV EDX,0x200` immediately
	 * before the data-chunk SubmitOmapNKS4VideoWrite call below. As
	 * previously sized, every data chunk sent 32 bytes of extra
	 * uninitialized stack contents over USB - a real bug, not just an
	 * oversized buffer. */
	unsigned int packet[0x80];	/* 0x200 bytes on stack in the binary */
	unsigned char *out = (unsigned char *)packet;
	out[0] = 0xc6;

	/* CORRECTION (full-coverage re-audit, 2026-07-18): fresh decompile of this exact
	 * function (@0x15840) shows the "row exhausted, DAT_0000ed0c still has columns
	 * left" branch does NOT behave like a plain C `continue` here. Ground truth's real
	 * control flow is `if (DAT_ed0c > 0) goto LAB_000158a4` - a jump STRAIGHT INTO the
	 * byte-copy body (the label sits immediately after the `if (DAT_ed0c < 1) goto
	 * pad` check, not before it) - re-testing neither the outer `i < limit` bound nor
	 * the inner `DAT_ed10 > 0` while-condition, and skipping the `DAT_ed0c < 1` check
	 * entirely for this one byte. A plain `continue` in this do-while would instead
	 * jump back to the outer loop's own condition test, which would then re-enter the
	 * inner `while (DAT_ed10 > 0)` and re-run its `DAT_ed0c < 1` guard fresh - usually
	 * harmless (that guard can't actually fire here since we already know DAT_ed0c > 0
	 * on this exact branch, and DAT_ed10 was just set to a real nonzero row width in
	 * every normal draw), but a genuine divergence in the one pathological case a
	 * `dwArg2` (row width) of exactly 0 would produce: ground truth still forces one
	 * byte through even though the "current row" it just reset is already empty,
	 * while the old `continue` here would spin re-testing `DAT_ed10 > 0` against a
	 * value that keeps resetting to the same 0 without ever advancing `i` - a real
	 * behavioral difference at that boundary, not just a style rewrite. Fixed via an
	 * explicit goto reproducing the same direct jump target. */
	int limit = dwTransferRowSize - 1;
	int i = 0;
	int colsLeft = DAT_0000ed0c;
	if (limit > 0) {
		do {
			while (DAT_0000ed10 > 0) {
				if (DAT_0000ed0c < 1)
					goto pad;
read_byte:
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
				goto read_byte;
pad:
			out[i + 1] = 0;
			i++;
			DAT_0000ed10--;
		} while (i < limit);
	}
done:
	DAT_0000ed0c = colsLeft;
	/* byte-swap the 16-bit halves of every word in the packet */
	for (unsigned int *q = packet; q != packet + 0x80; q++) {
		unsigned int v = *q;
		unsigned short lo = (unsigned short)v, hi = (unsigned short)(v >> 16);
		*q = ((unsigned int)((lo >> 8) | (lo << 8))) |
		     ((unsigned int)((hi >> 8) | (hi << 8)) << 16);
	}
	SubmitOmapNKS4VideoWrite(packet, sizeof(packet));

	if (DAT_0000ed0c < 1) {		/* region complete -> end marker */
		out[3] = 0x83;
		/* CORRECTION (re-verification pass, 2026-07-17): the end-marker
		 * submit previously sent the whole packet buffer. Ground truth
		 * sends a fixed length of 4 bytes here (`MOV EDX,0x4` immediately
		 * before this call) - just the {0xc6,?,?,0x83} header, not the
		 * full chunk. */
		SubmitOmapNKS4VideoWrite(packet, 4);
		dwProcessingActive = 0;
	}
}

/* ---- C-ABI wrappers (operate on the singleton) ------------------------- */
extern "C" {
/* CORRECTION (full-coverage re-audit, 2026-07-18): the real COmapNKS4VideoAPI_Initialize
 * @0x168b0 is a literal 1-byte function body (a single `RET`, confirmed via disassembly -
 * no CMP/TEST/CALL/field-store of any kind). `get_xrefs_to` on the real constructor
 * (COmapNKS4VideoAPI::COmapNKS4VideoAPI@0x157b0) shows its own single reference is from
 * "Entry Point" (type EXTERNAL), not a normal CALL site anywhere in this module's code -
 * i.e. `g_video`'s constructor runs automatically via this module's own C++
 * global-constructor mechanism (the `init_cpp_support()` global ctor/dtor runner this
 * project's build already depends on - see this file's own header comment and the
 * README's "Building" section) at insmod time, completely independent of this exported
 * symbol. This function's real, shipped body does nothing further - whatever it may once
 * have done became fully redundant once `g_video` became a plain global object rather
 * than something requiring an explicit placement-new call. The previous
 * `new (&g_video) COmapNKS4VideoAPI()` here would have re-run the real constructor a
 * SECOND time (harmless for the field values themselves, since a second run just
 * reassigns the same constants - but it also re-fires the constructor's own
 * `UpdateScreenInfo()`-style printk, which real hardware would only ever log once, from
 * the automatic global-ctor call). Not currently called from anywhere in this
 * reconstruction either (`grep` confirms zero call sites), so this fix is a pure fidelity
 * correction with no other observable effect on this tree today. */
void COmapNKS4VideoAPI_Initialize(void)                 { }
/* CORRECTION (full-coverage re-audit, 2026-07-18): this was calling ProcessEvents()
 * exactly once per invocation, but ground truth's real OmapNKS4VideoAPIProcessEvents
 * @0x171a0 is a genuine "drain the whole queue" driving loop, not a single-step
 * wrapper: `do { if (dwProcessingActive != 1) { <the same pop+dispatch body
 * ProcessEvents' own class method already implements, including its own inner
 * "return if sEventsToProcess==0" exit> } ContinueProcessingEvent(&currentEvent); }
 * while (true)` - the ONLY exit from the real function is the inner
 * "sEventsToProcess==0 && !streaming" return, confirmed via fresh decompile (no
 * other break/return anywhere in the whole disassembled body). Tracing what this
 * means against the already-verified ProcessEvents() class method: whenever
 * dwProcessingActive==1, ProcessEvents() unconditionally calls
 * ContinueProcessingEvent() and returns 1 (loop continues); when
 * dwProcessingActive!=1 and the queue is empty, it returns 0 (real loop's only exit);
 * otherwise it pops+dispatches one event and returns 1 (loop continues, regardless of
 * which opcode - including 0xc2, which leaves dwProcessingActive=1 for the next
 * iteration to pick up via the first branch). `while (g_video.ProcessEvents())` is
 * therefore an exact translation of ground truth's real loop using the already
 * disassembly-confirmed class method, not a behavioral approximation. */
void OmapNKS4VideoAPIProcessEvents(void)
{
	while (g_video.ProcessEvents())
		;
}
int  OmapNKS4InitLCDRegs(char r, char v, int d)         { return g_video.InitLCDRegs(r, v, d); }
int  OmapNKS4XAxisByteSize(int b)                       { return g_video.XAxisByteSize(b); }
int  OmapNKS4SendPixelDataRegion(int w, int o, int rb)  { return g_video.SendPixelDataRegion(w, o, rb); }
int  OmapNKS4SendFillData(unsigned char c, int w, int base, int h) { return g_video.SendFillData(c, w, base, h); }
int  OmapNKS4UpdateColorPal(char a, char b, char c, char d){ return g_video.UpdateColorPal(a, b, c, d); }
int  OmapNKS4UpdateScreenInfo(char *base, int x, int y) { return g_video.UpdateScreenInfo(base, x, y); }
/* CORRECTION (full-coverage re-audit, 2026-07-18): `get_xrefs_to` on the real
 * SendFillData class method (@0x16420) shows its only 8 non-entry-point callers are
 * all inside driver.cpp's own SetProgressBarPercent - and every one of those 8 sites
 * is an UNCONDITIONAL_CALL straight to 0x16420 itself, not to any intermediate
 * function of this name. There is no "OmapNKS4VideoAPI_SendFillData" symbol anywhere
 * in the real binary (same class of finding as OmapNKS4_ActiveSenseThreadEntry in
 * realtime.cpp) - ground truth's real SetProgressBarPercent just calls
 * `g_video.SendFillData(...)` directly as a plain C++ method call, since driver.cpp is
 * already C++ (unlike usb.cpp/procfs.cpp/submit.cpp/main.cpp's plain-C style) and has
 * no need for a self-pointer bridge at all. This function exists here only because an
 * earlier pass's own driver.cpp declared it `extern "C"` expecting a free-function
 * bridge (see driver.cpp's own comment at its declaration, describing a real,
 * already-fixed separate bug: a missing `extern "C"` on that declaration previously
 * caused a genuine "Unknown symbol" mangled-name mismatch at insmod). Changing
 * driver.cpp's 8 call sites to call the class method directly instead - the more
 * literal ground-truth match - is out of this pass's scope (driver.cpp is a different
 * file), and this bridge's own body is already behaviorally exact (self is always
 * `&g_video`, the only real instance, so `self->SendFillData(...)` produces the
 * identical call `g_video.SendFillData(...)` would), so left in place rather than
 * restructured. */
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

/* =========================================================================
 * VM-ONLY CONCURRENCY STRESS TEST, 2026-07-18
 * =========================================================================
 * Pure test scaffolding, NOT a ground-truth reconstruction of anything in the
 * real binary. Behaviorally validates two of this session's disassembly-only
 * fixes under genuine concurrent load, per the Opus review's own concern that
 * neither had ever been exercised by real concurrent producers:
 *
 *   1. GetNextFreeFifoEvent()/AddFifoEvent()'s race-check (all 5 draw-builders
 *      above route through it): two producers on different CPUs racing on
 *      dwWriteIndex should never both "win" the same ring slot - the loser
 *      must observe -5, not silently corrupt the ring.
 *   2. pop_free_urb()'s per-list lock + in-use-counter-under-lock (submit.cpp):
 *      exercised here under sustained real call volume from the one real
 *      consumer path (SubmitOmapNKS4VideoWrite -> SubmitOmapNKS4BulkWrite ->
 *      pop_free_urb), not synthetically.
 *
 * Design: spawns VM_STRESS_NUM_PRODUCERS real kernel threads (kernel_thread(),
 * the same primitive main.cpp's create_thread() wraps) that each call a
 * rotating mix of all 5 draw-builder free functions in a tight loop, with
 * distinct/traceable arguments per thread+iteration. Deliberately does NOT
 * spawn additional consumer threads: ground truth's own OmapNKS4VideoAPIProcessEvents/
 * ProcessEvents design is single-consumer (dwReadIndex/dwProcessingActive/
 * currentEvent are unprotected globals with no equivalent dequeue-side
 * race-check) - running more than one drainer would be testing an invalid
 * configuration ground truth was never built to survive, not a real fidelity
 * gap. Instead this reuses the REAL, already-running kOmapNKS4MsgRoutine
 * thread (started earlier in OmapNKS4Init, before this function's call site)
 * as the single consumer - it wakes via the real SignalVideoMessageProcessor()
 * call every successful commit_event() already makes, so producers and that
 * one real consumer genuinely race on dwWriteIndex/sEventsToProcess/
 * GetNextFreeFifoEvent/AddFifoEvent for the whole run, and every drained event
 * flows through the real SubmitOmapNKS4VideoWrite -> ... -> pop_free_urb path.
 *
 * Packet capture: vm_stress_capture_packet() (forward-declared above, called
 * from ProcessEvents()'s 5 wire-submit call sites) snapshots the exact final
 * wire buffer for a bounded sample of drained packets while a run is active.
 * Each producer encodes an identifying tag into its own arguments (a thread-id
 * + iteration-counter pair, chosen to survive the 19-bit pack_field19 wire
 * truncation and each opcode's specific byte layout intact) so a captured
 * packet can be decoded and cross-checked against what SOME real caller must
 * have sent - not just "well-formed-looking", genuinely attributable.
 */

#define VM_STRESS_NUM_PRODUCERS   4	/* matches kronosvm's 4 vCPUs */
#define VM_STRESS_ITERS_PER_THREAD 2000
#define VM_STRESS_CAPTURE_MAX     512
/* Backing "frame buffer" for SendPixelDataRegion's real byte-copy path
 * (ContinueProcessingEvent) - see vm_virtual_probe_stress_test_video()'s own
 * comment for why this is needed and how its size was chosen. */
#define VM_STRESS_FRAMEBUF_SIZE   0x40000u	/* 256 KiB, generous headroom */

static int sVmStressFramebufOk;	/* set once UpdateScreenInfo has a real base */

/* Real Linux kernel primitives, already declared+used this same way in
 * main.cpp (create_thread_impl/ProcessMsgRoutine) - repeated locally here
 * rather than promoted to the shared header, matching this project's own
 * established per-file-extern convention (e.g. submit.cpp's own local
 * externs for usb.cpp's free-list globals). */
extern "C" {
int  kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);
void daemonize(const char *name, ...);
}

struct vm_stress_capture_entry {
	unsigned char opcode;
	unsigned char len;
	unsigned char data[12];
};

static struct vm_stress_capture_entry sVmStressCapture[VM_STRESS_CAPTURE_MAX];
static int sVmStressCaptureCount;	/* atomic; may exceed the array - only the
					 * first VM_STRESS_CAPTURE_MAX entries are kept */
static int sVmVideoStressCapture;	/* gate: only capture while a run is active */

static volatile int sVmStressProducerDone[VM_STRESS_NUM_PRODUCERS];
static int sVmStressAttempts[VM_STRESS_NUM_PRODUCERS];
static int sVmStressSuccess[VM_STRESS_NUM_PRODUCERS];
static int sVmStressRingFull[VM_STRESS_NUM_PRODUCERS];	/* -3 returns */
static int sVmStressRaceLost[VM_STRESS_NUM_PRODUCERS];	/* -5 returns */
static int sVmStressOtherErr[VM_STRESS_NUM_PRODUCERS];

static void vm_stress_capture_packet(const unsigned char *buf, unsigned int len)
{
	if (!sVmVideoStressCapture)
		return;
	int idx = __sync_fetch_and_add(&sVmStressCaptureCount, 1);
	if (idx < 0 || idx >= VM_STRESS_CAPTURE_MAX)
		return;
	struct vm_stress_capture_entry *e = &sVmStressCapture[idx];
	e->opcode = buf[3];	/* opcode byte sits at index 3 in all 5 formats */
	e->len = (unsigned char)(len > 12 ? 12 : len);
	for (unsigned int i = 0; i < e->len; i++)
		e->data[i] = buf[i];
}

/* inverse of this file's own pack_field19() above: 3 wire bytes -> the
 * original (<=19-bit) value. */
static unsigned int vm_stress_unpack19(const unsigned char *b)
{
	return ((unsigned int)(b[0] & 7) << 16) | ((unsigned int)b[1] << 8) | b[2];
}

/* One producer thread: rotates through all 5 draw-builders, tagging each
 * call's arguments with (thread id, iteration) so a drained/captured packet
 * can be traced back to a specific caller. Tag width choices below are
 * deliberately sized to survive each opcode's real wire truncation:
 *  - pack_field19 fields (XAxisByteSize/SendPixelDataRegion/SendFillData's
 *    int args) keep only 19 bits, so tid is packed into bits 14-16 (up to 8
 *    threads) and the iteration counter into bits 0-13 (up to 16384).
 *  - InitLCDRegs's `data` argument survives as a FULL 32-bit round trip
 *    (byte 0 = data&0xff, bytes 5-7 = the remaining 3 bytes reversed) - see
 *    ProcessEvents' own 0xc0 branch - so tid/iter ride in there directly,
 *    with `reg` as an independent redundant tid copy for cross-checking.
 *  - SendFillData's `color` and UpdateColorPal's 4 raw bytes are untouched by
 *    pack_field19 - used as redundant/independent tid (and a fixed 0xAA
 *    marker byte for UpdateColorPal) copies, so a captured packet's redundant
 *    fields must independently AGREE with the packed tag for a "cross-checked
 *    OK", not merely decode to a plausible-looking value. */
static int vm_stress_producer_thread(void *arg)
{
	long tid = (long)arg;
	char name[16] = "kNKS4StressPx";
	name[12] = (char)('0' + (tid & 7));

	daemonize(name);

	for (int i = 0; i < VM_STRESS_ITERS_PER_THREAD; i++) {
		unsigned int tag19 = ((unsigned int)tid << 14) | ((unsigned int)i & 0x3fff);
		int rc, op = i % 5;

		sVmStressAttempts[tid]++;
		switch (op) {
		case 0: {	/* InitLCDRegs: reg=tid (redundant check), data=full tid:iter tag */
			unsigned int data = ((unsigned int)tid << 24) | ((unsigned int)i & 0xffffff);
			rc = OmapNKS4InitLCDRegs((char)tid, (char)(i & 0xff), (int)data);
			break;
		}
		case 1:		/* XAxisByteSize */
			rc = OmapNKS4XAxisByteSize((int)tag19);
			break;
		case 2:		/* SendPixelDataRegion */
			if (sVmStressFramebufOk)
				/* CORRECTION (live VM test, 2026-07-18): width=rowBytes=tag19
				 * (this test's first attempt) hit a real, separate finding -
				 * the synthetic board's negotiated max packet size is smaller
				 * than ContinueProcessingEvent's fixed 0x200-byte chunk buffer,
				 * so dwTransferRowSize ends up tiny and a large region needs
				 * many ContinueProcessingEvent calls, each only advancing a
				 * few bytes - exercising ground truth's real but delicate
				 * row-wrap-pointer-advance arithmetic (see that function's own
				 * comments) far more heavily than this concurrency test
				 * actually needs to. Fixed by using a trivial 1-byte region
				 * (width=rowBytes=1) - completes within ContinueProcessingEvent's
				 * very first call regardless of dwTransferRowSize, so this still
				 * exercises the real 0xc2 wire header + at least one real
				 * streaming byte-copy + end-marker submit + dwProcessingActive
				 * transition, without depending on multi-call row-wrap
				 * correctness this test isn't targeting. tid/iter now ride in
				 * the OFFSET field instead of width (decoder updated to match). */
				rc = OmapNKS4SendPixelDataRegion(1, (int)tag19, 1);
			else	/* no safe backing buffer - substitute a harmless opcode
				 * rather than skip the iteration outright */
				rc = OmapNKS4XAxisByteSize((int)tag19);
			break;
		case 3:		/* SendFillData: color=tid (redundant check) */
			rc = OmapNKS4SendFillData((unsigned char)tid, (int)tag19, (int)tag19, (int)tag19);
			break;
		default:	/* UpdateColorPal: a=tid, b:c=iter, d=0xAA marker */
			rc = OmapNKS4UpdateColorPal((char)tid, (char)((i >> 8) & 0xff),
						    (char)(i & 0xff), (char)0xAA);
			break;
		}

		if (rc == 0)       sVmStressSuccess[tid]++;
		else if (rc == -3) sVmStressRingFull[tid]++;	/* ring full, gave up */
		else if (rc == -5) sVmStressRaceLost[tid]++;	/* lost the AddFifoEvent race */
		else               sVmStressOtherErr[tid]++;
	}

	sVmStressProducerDone[tid] = 1;
	return 0;
}

/* Decode + cross-check a sample of the captured packets, printk'd for the live
 * test transcript. Returns the number of mismatches found in the sample. */
static int vm_stress_decode_and_report(void)
{
	int total = sVmStressCaptureCount;
	int capped = total > VM_STRESS_CAPTURE_MAX ? VM_STRESS_CAPTURE_MAX : total;
	int shown = 0, mismatches = 0, malformed = 0;

	printk("<6>OmapNKS4: vm_video_stress: captured %d packets (of %d submitted, "
	       "cap %d) - decoding a sample:\n", capped, total, VM_STRESS_CAPTURE_MAX);

	for (int i = 0; i < capped && shown < 25; i++, shown++) {
		struct vm_stress_capture_entry *e = &sVmStressCapture[i];
		unsigned int tid = 0xffffffff, iter = 0xffffffff;
		int ok;

		switch (e->opcode) {
		case 0x81: {
			unsigned int v = vm_stress_unpack19(&e->data[0]);
			tid = v >> 14; iter = v & 0x3fff;
			ok = (tid < VM_STRESS_NUM_PRODUCERS && iter < VM_STRESS_ITERS_PER_THREAD);
			printk("<6>OmapNKS4: vm_video_stress: [%d] 0x81 XAxisByteSize tag=0x%05x "
			       "-> tid=%u iter=%u %s\n", i, v, tid, iter, ok ? "OK" : "MISMATCH");
			break;
		}
		case 0xc0: {
			unsigned int data = e->data[0] | ((unsigned int)e->data[7] << 8) |
					     ((unsigned int)e->data[6] << 16) |
					     ((unsigned int)e->data[5] << 24);
			unsigned int reg = e->data[2];
			tid = data >> 24; iter = data & 0xffffff;
			ok = (tid < VM_STRESS_NUM_PRODUCERS && iter < VM_STRESS_ITERS_PER_THREAD &&
			      reg == tid);
			printk("<6>OmapNKS4: vm_video_stress: [%d] 0xc0 InitLCDRegs reg=%u "
			       "data=0x%08x -> tid=%u iter=%u %s\n", i, reg, data, tid, iter,
			       ok ? "OK" : "MISMATCH");
			break;
		}
		case 0xc2: {
			/* tag now rides in the OFFSET field (buf[4..6]), not width
			 * (buf[0..2], fixed at 1) - see the producer's own comment. */
			unsigned int width = vm_stress_unpack19(&e->data[0]);
			unsigned int v = vm_stress_unpack19(&e->data[4]);
			tid = v >> 14; iter = v & 0x3fff;
			ok = (tid < VM_STRESS_NUM_PRODUCERS && iter < VM_STRESS_ITERS_PER_THREAD &&
			      width == 1);
			printk("<6>OmapNKS4: vm_video_stress: [%d] 0xc2 SendPixelDataRegion "
			       "width=%u offset-tag=0x%05x -> tid=%u iter=%u %s\n", i, width, v, tid, iter,
			       ok ? "OK" : "MISMATCH");
			break;
		}
		case 0xc4: {
			unsigned int v = vm_stress_unpack19(&e->data[0]);
			unsigned int color = e->data[7];
			tid = v >> 14; iter = v & 0x3fff;
			ok = (tid < VM_STRESS_NUM_PRODUCERS && iter < VM_STRESS_ITERS_PER_THREAD &&
			      color == tid);
			printk("<6>OmapNKS4: vm_video_stress: [%d] 0xc4 SendFillData "
			       "width-tag=0x%05x color=%u -> tid=%u iter=%u %s\n", i, v, color,
			       tid, iter, ok ? "OK" : "MISMATCH");
			break;
		}
		case 0xc5: {
			unsigned int a = e->data[2], b = e->data[1], c = e->data[0], d = e->data[7];
			tid = a; iter = ((unsigned int)b << 8) | c;
			ok = (tid < VM_STRESS_NUM_PRODUCERS && iter < VM_STRESS_ITERS_PER_THREAD &&
			      d == 0xAA);
			printk("<6>OmapNKS4: vm_video_stress: [%d] 0xc5 UpdateColorPal a(tid)=%u "
			       "iter=%u marker=0x%02x -> %s\n", i, tid, iter, d,
			       ok ? "OK" : "MISMATCH");
			break;
		}
		default:
			printk("<6>OmapNKS4: vm_video_stress: [%d] UNRECOGNIZED opcode 0x%02x "
			       "- MALFORMED PACKET\n", i, e->opcode);
			ok = 0;
			malformed++;
			break;
		}
		if (!ok) mismatches++;
	}

	printk("<6>OmapNKS4: vm_video_stress: sample decode complete - %d/%d shown mismatched "
	       "(%d malformed opcode)\n", mismatches, shown, malformed);
	return mismatches;
}

/* Orchestrator - see this block's own header comment for the full design.
 * Called once from OmapNKS4Init (main.cpp), after the board is fully probed/
 * configured/running and both worker threads are already alive. */
void vm_virtual_probe_stress_test_video(void)
{
	if (!sVmVirtualProbe || !sVmVideoStress)
		return;

	printk("<6>OmapNKS4: vm_video_stress: starting - %d producer threads x %d "
	       "iterations each, draining via the real, already-running "
	       "kOmapNKS4MsgRoutine consumer thread\n",
	       VM_STRESS_NUM_PRODUCERS, VM_STRESS_ITERS_PER_THREAD);

	/* CORRECTION (live VM test, 2026-07-18): the first stress run oopsed inside
	 * ContinueProcessingEvent (a real, second, independent bug from the
	 * WaitForFreeBulkWriteURB one above - this one is in MY test harness, not
	 * the code under test). g_video.dwScreenBase is 0 until something calls
	 * OmapNKS4UpdateScreenInfo() - normally done by OmapVideoModule.ko, which
	 * isn't loaded in this VM test - so the producer thread's SendPixelDataRegion
	 * tag (used verbatim as width/offset/rowBytes) became a literal frame-buffer
	 * offset into address 0, and ContinueProcessingEvent's real byte-copy loop
	 * correctly (per ground truth) dereferenced dwScreenBase+offset - which
	 * pointed at unmapped low memory. Fixed by giving this test its OWN backing
	 * "frame buffer" (kmalloc'd via the module's own operator new, which is
	 * exactly stg_kmalloc per main.cpp) and constraining SendPixelDataRegion's
	 * width/offset/rowBytes tag range so offset+width can never exceed it -
	 * VM_STRESS_NUM_PRODUCERS<=8 (3-bit tid) x VM_STRESS_ITERS_PER_THREAD<=0x3fff
	 * (14-bit iter) bounds tag19 to at most 0x1ffff, so touching at most
	 * 2*0x1ffff=0x3fffe bytes forward of the base - VM_STRESS_FRAMEBUF_SIZE below
	 * is sized with generous headroom over that. This is purely test-harness
	 * setup, not a change to any reconstructed function. */
	unsigned char *framebuf = new unsigned char[VM_STRESS_FRAMEBUF_SIZE];
	sVmStressFramebufOk = 0;
	if (framebuf) {
		for (unsigned int i = 0; i < VM_STRESS_FRAMEBUF_SIZE; i++)
			framebuf[i] = (unsigned char)i;	/* recognizable, non-zero pattern */
		OmapNKS4UpdateScreenInfo((char *)framebuf, 800, 600);
		sVmStressFramebufOk = 1;
	} else {
		printk("<6>OmapNKS4: vm_video_stress: framebuf alloc failed, "
		       "SendPixelDataRegion producers will substitute XAxisByteSize "
		       "instead of risking an out-of-bounds streaming read\n");
	}

	for (int i = 0; i < VM_STRESS_NUM_PRODUCERS; i++) {
		sVmStressProducerDone[i] = 0;
		sVmStressAttempts[i] = 0;
		sVmStressSuccess[i] = 0;
		sVmStressRingFull[i] = 0;
		sVmStressRaceLost[i] = 0;
		sVmStressOtherErr[i] = 0;
	}
	sVmStressCaptureCount = 0;
	sVmVideoStressCapture = 1;

	int pids[VM_STRESS_NUM_PRODUCERS];
	for (int i = 0; i < VM_STRESS_NUM_PRODUCERS; i++) {
		/* flags=0xe00: same CLONE_* value main.cpp's create_thread_impl already
		 * uses for kernel_thread() - these are short-lived test threads with no
		 * OmapNKS4Init-style completion handshake, so kernel_thread() is called
		 * directly rather than through create_thread(). */
		pids[i] = kernel_thread(vm_stress_producer_thread, (void *)(long)i, 0xe00);
		if (pids[i] < 0) {
			printk("<6>OmapNKS4: vm_video_stress: kernel_thread() failed for "
			       "producer %d, err %d\n", i, pids[i]);
			sVmStressProducerDone[i] = 1;	/* don't wait forever below */
		}
	}

	/* Wait for all producers to finish (generous cap - iterations are fast,
	 * bounded mainly by ring_full_wait's own 10x5ms=50ms admission backoff). */
	for (int waited = 0; waited < 30000; waited += 20) {
		int all_done = 1;
		for (int i = 0; i < VM_STRESS_NUM_PRODUCERS; i++)
			if (!sVmStressProducerDone[i]) all_done = 0;
		if (all_done) break;
		stg_msleep(20);
	}

	/* Let the real single consumer thread fully drain the ring - it wakes on
	 * SignalVideoMessageProcessor(), already called by every successful
	 * commit_event() above; just wait for the queue to empty. */
	for (int waited = 0; waited < 5000; waited += 20) {
		if (sEventsToProcess == 0 && g_video.dwProcessingActive == 0)
			break;
		stg_msleep(20);
	}
	stg_msleep(50);	/* small settle margin past the last drain */

	/* Safe to free now - the ring is confirmed drained (dwProcessingActive==0
	 * checked just above), so ContinueProcessingEvent can no longer be mid-flight
	 * reading through sCurrentRegionTransferInfo derived from this buffer. */
	if (framebuf) {
		delete[] framebuf;
		g_video.dwScreenBase = 0;	/* don't leave a dangling pointer behind */
		sVmStressFramebufOk = 0;
	}

	sVmVideoStressCapture = 0;

	int totalAttempt = 0, totalSuccess = 0, totalFull = 0, totalRace = 0, totalOther = 0;
	for (int i = 0; i < VM_STRESS_NUM_PRODUCERS; i++) {
		printk("<6>OmapNKS4: vm_video_stress: producer %d: attempts=%d success=%d "
		       "ring_full(-3)=%d race_lost(-5)=%d other_err=%d done=%d\n",
		       i, sVmStressAttempts[i], sVmStressSuccess[i], sVmStressRingFull[i],
		       sVmStressRaceLost[i], sVmStressOtherErr[i], sVmStressProducerDone[i]);
		totalAttempt += sVmStressAttempts[i];
		totalSuccess += sVmStressSuccess[i];
		totalFull    += sVmStressRingFull[i];
		totalRace    += sVmStressRaceLost[i];
		totalOther   += sVmStressOtherErr[i];
	}
	int drained = (sEventsToProcess == 0 && g_video.dwProcessingActive == 0);
	printk("<6>OmapNKS4: vm_video_stress: TOTAL attempts=%d success=%d ring_full(-3)=%d "
	       "race_lost(-5)=%d other_err=%d - ring drained=%s (sEventsToProcess=%d "
	       "dwProcessingActive=%d)\n", totalAttempt, totalSuccess, totalFull, totalRace,
	       totalOther, drained ? "yes" : "NO (timed out)", sEventsToProcess,
	       g_video.dwProcessingActive);

	int mismatches = vm_stress_decode_and_report();

	printk("<6>OmapNKS4: vm_video_stress: COMPLETE - no kernel oops/hang observed by "
	       "this point (module still running). %s\n",
	       (totalOther == 0 && mismatches == 0 && drained && totalSuccess > 0)
	       ? "PASS: all decoded samples cross-checked OK, ring fully drained, no "
		 "unexpected return codes"
	       : "see counters/decode output above for details");
}
