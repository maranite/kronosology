// SPDX-License-Identifier: GPL-2.0
/*
 * test_file_opener_events.cpp  -  host-side known-answer tests for
 * CSTGFileOpener::AddPlaybackEvent(CSTGAudioEvent*, unsigned int)/
 * AddRecordEvent(CSTGAudioEvent*, unsigned int) (batch 51).
 *
 * Links only src/engine/file_opener_events.cpp -- both methods are fully
 * self-contained (zero relocations in the real disassembly), so no mocks
 * are needed. Drives a raw mmap32'd CSTGFileOpener object directly (same
 * "cast raw memory to the class, no constructor" convention already used
 * by test_hdr_sampler_commands.cpp/test_hdr_record_track.cpp for these
 * still-partially-opaque manager classes).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok) g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok) printf("        (wanted 0x%x)\n", want);
}

int main(void)
{
	printf("CSTGFileOpener::AddPlaybackEvent/AddRecordEvent test\n");
	printf("======================================================\n");

	printf("[1] AddPlaybackEvent: normal (non-full) lane enqueue\n");
	{
		unsigned char *opMem = (unsigned char *)mmap32(0x300);
		memset(opMem, 0, 0x300);
		unsigned char *laneBase = (unsigned char *)mmap32(0x1000);
		memset(laneBase, 0, 0x1000);

		unsigned int index = 2;
		unsigned char *lane = opMem + index * 0x10;
		*(unsigned int *)(lane + 0x0) = ToU32(laneBase);
		*(unsigned int *)(lane + 0x4) = 0;  /* writeIdx */
		*(unsigned int *)(lane + 0x8) = 5;  /* readIdx (not equal to nextWrite) */
		*(unsigned int *)(lane + 0xc) = 8;  /* capacity */

		CSTGFileOpener *opener = (CSTGFileOpener *)opMem;
		CSTGAudioEvent *fakeEvent = (CSTGAudioEvent *)0x1000;
		opener->AddPlaybackEvent(fakeEvent, index);

		check_eq("lane[0] holds the event pointer", ((unsigned int *)laneBase)[0], ToU32(fakeEvent));
		check_eq("lane writeIdx advanced 0 -> 1", *(unsigned int *)(lane + 0x4), 1);
		check_eq("lane readIdx unchanged", *(unsigned int *)(lane + 0x8), 5);
	}

	printf("[2] AddPlaybackEvent: full lane -> falls back to the fixed +0x200 lane\n");
	{
		unsigned char *opMem = (unsigned char *)mmap32(0x300);
		memset(opMem, 0, 0x300);
		unsigned char *laneBase = (unsigned char *)mmap32(0x1000);
		memset(laneBase, 0xcc, 0x1000);
		unsigned char *fbBase = (unsigned char *)mmap32(0x1000);
		memset(fbBase, 0, 0x1000);

		unsigned int index = 1;
		unsigned char *lane = opMem + index * 0x10;
		*(unsigned int *)(lane + 0x0) = ToU32(laneBase);
		*(unsigned int *)(lane + 0x4) = 3;  /* writeIdx */
		*(unsigned int *)(lane + 0x8) = 4;  /* readIdx == nextWrite (3+1 % 8 == 4) -> FULL */
		*(unsigned int *)(lane + 0xc) = 8;  /* capacity */

		unsigned char *fallback = opMem + 0x200;
		*(unsigned int *)(fallback + 0x0) = ToU32(fbBase);
		*(unsigned int *)(fallback + 0x4) = 7;  /* fallback writeIdx */
		*(unsigned int *)(fallback + 0xc) = 16; /* fallback capacity */

		CSTGFileOpener *opener = (CSTGFileOpener *)opMem;
		CSTGAudioEvent *fakeEvent = (CSTGAudioEvent *)0x2000;
		opener->AddPlaybackEvent(fakeEvent, index);

		check_eq("normal lane writeIdx untouched (still 3)", *(unsigned int *)(lane + 0x4), 3);
		check_eq("fallback lane[7] holds the event pointer", ((unsigned int *)fbBase)[7], ToU32(fakeEvent));
		check_eq("fallback writeIdx advanced 7 -> 8", *(unsigned int *)(fallback + 0x4), 8);
	}

	printf("[3] AddRecordEvent: lands 0x100 past the same index's playback lane, doesn't clobber it\n");
	{
		unsigned char *opMem = (unsigned char *)mmap32(0x300);
		memset(opMem, 0, 0x300);
		unsigned char *pbBase = (unsigned char *)mmap32(0x1000);
		memset(pbBase, 0, 0x1000);
		unsigned char *recBase = (unsigned char *)mmap32(0x1000);
		memset(recBase, 0, 0x1000);

		unsigned int index = 3;
		unsigned char *pbLane = opMem + index * 0x10;
		*(unsigned int *)(pbLane + 0x0) = ToU32(pbBase);
		*(unsigned int *)(pbLane + 0x8) = 5;
		*(unsigned int *)(pbLane + 0xc) = 8;

		unsigned char *recLane = opMem + index * 0x10 + 0x100;
		*(unsigned int *)(recLane + 0x0) = ToU32(recBase);
		*(unsigned int *)(recLane + 0x8) = 5;
		*(unsigned int *)(recLane + 0xc) = 8;

		CSTGFileOpener *opener = (CSTGFileOpener *)opMem;
		CSTGAudioEvent *fakeEvent = (CSTGAudioEvent *)0x3000;
		opener->AddRecordEvent(fakeEvent, index);

		check_eq("record lane[0] holds the event pointer", ((unsigned int *)recBase)[0], ToU32(fakeEvent));
		check_eq("record lane writeIdx advanced 0 -> 1", *(unsigned int *)(recLane + 0x4), 1);
		check_eq("sibling playback lane writeIdx untouched", *(unsigned int *)(pbLane + 0x4), 0);
		check_eq("sibling playback lane base data untouched",
			 ((unsigned int *)pbBase)[0], 0);
	}

	printf("[4] AddPlaybackEvent: writeIdx wraps at the capacity boundary\n");
	{
		unsigned char *opMem = (unsigned char *)mmap32(0x300);
		memset(opMem, 0, 0x300);
		unsigned char *laneBase = (unsigned char *)mmap32(0x1000);
		memset(laneBase, 0, 0x1000);

		unsigned int index = 0;
		unsigned char *lane = opMem + index * 0x10;
		*(unsigned int *)(lane + 0x0) = ToU32(laneBase);
		*(unsigned int *)(lane + 0x4) = 3;  /* writeIdx: last slot of a 4-slot ring */
		*(unsigned int *)(lane + 0x8) = 1;  /* readIdx, well clear of (3+1)%4==0 */
		*(unsigned int *)(lane + 0xc) = 4;  /* capacity */

		CSTGFileOpener *opener = (CSTGFileOpener *)opMem;
		CSTGAudioEvent *fakeEvent = (CSTGAudioEvent *)0x4000;
		opener->AddPlaybackEvent(fakeEvent, index);

		check_eq("event written at slot 3 (old writeIdx)", ((unsigned int *)laneBase)[3], ToU32(fakeEvent));
		check_eq("writeIdx wrapped 3 -> 0", *(unsigned int *)(lane + 0x4), 0);
	}

	printf("======================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
