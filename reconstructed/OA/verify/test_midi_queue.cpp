// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_queue.cpp  -  host-side known-answer test for
 * CSTGMidiQueue::GetNumWritableBytes() (sec 10.150) and Reset() (batch
 * 12). See test_midi_queue_writer.cpp for CSTGMidiQueueWriter::Write()
 * (sec 10.83), the same shared ringCtl memory but a separate file/KAT.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine_init.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

/* Real layout (see oa_global.h): +0x8 mask, +0xc write cursor,
 * +0x10+i*4 reader cursors, +0x20 reader count -- `this` IS this block
 * directly for GetNumWritableBytes (no wrapper indirection). */
struct RingCtl {
	unsigned char pad0[8];
	unsigned int mask;		/* +0x8 */
	unsigned int writeCursor;	/* +0xc */
	unsigned int readerPos[4];	/* +0x10..+0x1f */
	unsigned char readerCount;	/* +0x20 */
};

int main(void)
{
	printf("CSTGMidiQueue::GetNumWritableBytes() known-answer test\n");
	printf("=========================================================\n");

	RingCtl *ctlPtr = (RingCtl *)mmap32(sizeof(RingCtl));
	RingCtl &ctl = *ctlPtr;
	CSTGMidiQueue *q = (CSTGMidiQueue *)ctlPtr;

	printf("[1] no readers -> full capacity (mask+1) writable\n");
	{
		memset(ctlPtr, 0, sizeof(RingCtl));
		ctl.mask = 0xf;
		ctl.writeCursor = 0;
		ctl.readerCount = 0;
		check_eq("GetNumWritableBytes() == 0x10", q->GetNumWritableBytes(), 0x10);
	}

	printf("[2] one reader -- free space == mask+1 - backlog\n");
	{
		ctl.writeCursor = 100;
		ctl.readerCount = 1;
		ctl.readerPos[0] = 100 - 12; /* backlog 12 */
		check_eq("GetNumWritableBytes() == 16-12=4", q->GetNumWritableBytes(), 4);
	}

	printf("[3] worst-case (slowest) reader picked across multiple readers\n");
	{
		ctl.readerCount = 3;
		ctl.readerPos[0] = 100 - 4;  /* backlog 4 */
		ctl.readerPos[1] = 100 - 16; /* backlog 16, slowest -- zero free space */
		ctl.readerPos[2] = 100 - 8;  /* backlog 8 */
		check_eq("GetNumWritableBytes() == 0", q->GetNumWritableBytes(), 0);
	}

	printf("[4] mask=0xffffffff, no readers -> wraps to exactly 0\n"
	       "    (the exact fake-ringCtl trick test_global.cpp's own\n"
	       "    SubmitPerfChangeRequest tests rely on, sec 10.150)\n");
	{
		ctl.mask = 0xffffffff;
		ctl.writeCursor = 12345;
		ctl.readerCount = 0;
		check_eq("GetNumWritableBytes() == 0 (always congested)",
			 q->GetNumWritableBytes(), 0);
	}

	printf("[5] Reset() -- zeroes writeCursor + all 4 reader cursors,\n"
	       "    leaves mask/readerCount untouched (confirmed real gap)\n");
	{
		memset(ctlPtr, 0xCC, sizeof(RingCtl));
		ctl.mask = 0xf;
		ctl.readerCount = 4;
		q->Reset();
		check_eq("writeCursor == 0", ctl.writeCursor, 0);
		check_eq("readerPos[0] == 0", ctl.readerPos[0], 0);
		check_eq("readerPos[1] == 0", ctl.readerPos[1], 0);
		check_eq("readerPos[2] == 0", ctl.readerPos[2], 0);
		check_eq("readerPos[3] == 0", ctl.readerPos[3], 0);
		check_eq("mask untouched (0xf)", ctl.mask, 0xf);
		check_eq("readerCount untouched (4)", ctl.readerCount, 4);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
