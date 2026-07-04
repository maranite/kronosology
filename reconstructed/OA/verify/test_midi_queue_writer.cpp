// SPDX-License-Identifier: GPL-2.0
/*
 * test_midi_queue_writer.cpp  -  host-side known-answer test for
 * CSTGMidiQueueWriter::Write() (sec 10.83).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"

/* Real ringCtl/buffer fields are packed 32-bit pointers -- a plain host
 * stack/heap address isn't guaranteed to fit in 32 bits, so allocate
 * both via mmap(MAP_32BIT), matching this project's own established
 * convention. */
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

/* Real layout (see oa_global.h): ringCtl at +0x0, buffer base at +0x4.
 * ringCtl: +0x8 mask, +0xc write cursor, +0x10+i*4 reader cursors,
 * +0x20 reader count. */
struct RingCtl {
	unsigned char pad0[8];
	unsigned int mask;		/* +0x8 */
	unsigned int writeCursor;	/* +0xc */
	unsigned int readerPos[4];	/* +0x10..+0x1f */
	unsigned char readerCount;	/* +0x20 */
};

int main(void)
{
	printf("CSTGMidiQueueWriter::Write() known-answer test\n");
	printf("=========================================================\n");

	unsigned char *bufSpace = (unsigned char *)mmap32(64); /* mask = 0xf -> 16-byte ring, comfortably within 64 */
	RingCtl *ctlPtr = (RingCtl *)mmap32(sizeof(RingCtl));
	RingCtl &ctl = *ctlPtr;
	memset(ctlPtr, 0, sizeof(RingCtl));
	ctl.mask = 0xf; /* 16-byte ring buffer */
	memset(bufSpace, 0xCC, 64);

	unsigned char *writerObj = (unsigned char *)mmap32(8);
	*(unsigned int *)(writerObj + 0) = (unsigned int)(unsigned long)ctlPtr;
	*(unsigned int *)(writerObj + 4) = (unsigned int)(unsigned long)bufSpace;
	CSTGMidiQueueWriter *w = (CSTGMidiQueueWriter *)writerObj;

	printf("[1] no readers -- worst backlog is 0, plenty of room\n");
	{
		ctl.writeCursor = 0;
		ctl.readerCount = 0;
		unsigned char msg[3] = { 0x90, 0x40, 0x7f };
		w->Write(msg, 3, false);
		check_eq("write cursor advanced by 3", ctl.writeCursor, 3);
		check_eq("byte 0 written", bufSpace[0], 0x90);
		check_eq("byte 1 written", bufSpace[1], 0x40);
		check_eq("byte 2 written", bufSpace[2], 0x7f);
		check_eq("byte 3 untouched (poison preserved)", bufSpace[3], 0xCC);
	}

	printf("[2] wrap-around write (crosses the 16-byte boundary)\n");
	{
		memset(bufSpace, 0xCC, 64);
		ctl.writeCursor = 14; /* wrapped pos = 14 & 0xf = 14, only 2 bytes until wrap */
		ctl.readerCount = 0;
		unsigned char msg[5] = { 1, 2, 3, 4, 5 };
		w->Write(msg, 5, false);
		check_eq("first chunk (2 bytes) at wrapped pos 14", bufSpace[14], 1);
		check_eq("first chunk byte 2 at pos 15", bufSpace[15], 2);
		check_eq("second chunk wraps to buffer start (pos 0)", bufSpace[0], 3);
		check_eq("second chunk byte 2 at pos 1", bufSpace[1], 4);
		check_eq("second chunk byte 3 at pos 2", bufSpace[2], 5);
		check_eq("write cursor advanced by full length (5)", ctl.writeCursor, 14 + 5);
	}

	printf("[3] drop-on-full: one slow reader makes backlog exceed capacity\n");
	{
		memset(bufSpace, 0xCC, 64);
		ctl.writeCursor = 100;
		ctl.readerCount = 1;
		ctl.readerPos[0] = 100 - 16; /* backlog == capacity (16) already, zero free space */
		unsigned char msg[1] = { 0xAA };
		unsigned int cursorBefore = ctl.writeCursor;
		w->Write(msg, 1, false);
		check_eq("write cursor unchanged (dropped)", ctl.writeCursor, cursorBefore);
		check_eq("buffer untouched", bufSpace[100 & 0xf], 0xCC);
	}

	printf("[4] drop-on-full: worst-case backlog picked across multiple readers\n");
	{
		memset(bufSpace, 0xCC, 64);
		ctl.writeCursor = 100;
		ctl.readerCount = 3;
		ctl.readerPos[0] = 100 - 4;  /* backlog 4 -- fast reader */
		ctl.readerPos[1] = 100 - 16; /* backlog 16 -- slowest reader, zero free space */
		ctl.readerPos[2] = 100 - 8;  /* backlog 8 */
		unsigned char msg[1] = { 0xAA };
		unsigned int cursorBefore = ctl.writeCursor;
		w->Write(msg, 1, false);
		check_eq("write cursor unchanged (worst-case reader gates the write)",
			 ctl.writeCursor, cursorBefore);
	}

	printf("[5] exactly-fits write (length == free space) succeeds\n");
	{
		memset(bufSpace, 0xCC, 64);
		ctl.writeCursor = 100;
		ctl.readerCount = 1;
		ctl.readerPos[0] = 100 - 12; /* backlog 12, free space = 16-12 = 4 */
		unsigned char msg[4] = { 9, 9, 9, 9 };
		w->Write(msg, 4, false);
		check_eq("write cursor advanced by exactly 4", ctl.writeCursor, 104);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
