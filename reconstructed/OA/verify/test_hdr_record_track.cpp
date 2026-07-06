// SPDX-License-Identifier: GPL-2.0
/*
 * test_hdr_record_track.cpp  -  host-side known-answer tests for
 * CSTGRecordTrack::Start()/Pause()/Stop() and
 * CSTGHDRManager::ProcessRecordCommands() (batch 15).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

CSTGFileCloser *CSTGFileCloser::sInstance;

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

int main(void)
{
	printf("CSTGRecordTrack / CSTGHDRManager::ProcessRecordCommands test\n");
	printf("==============================================================\n");

	printf("[1] Start(): state 1->2, meter->+8 set to 2\n");
	{
		unsigned char *trackMem = (unsigned char *)mmap32(0x100);
		memset(trackMem, 0xcc, 0x100);
		unsigned char *meter = (unsigned char *)mmap32(0x100);
		memset(meter, 0xcc, 0x100);

		CSTGRecordTrack *track = (CSTGRecordTrack *)trackMem;
		*(int *)(trackMem + 0x4) = 1;
		*(unsigned int *)(trackMem + 0x8) = ToU32(meter);

		int r = track->Start();
		check_eq("Start() returns 1", (unsigned int)r, 1);
		check_eq("state == 2", *(unsigned int *)(trackMem + 0x4), 2);
		check_eq("meter+0x8 == 2", *(unsigned int *)(meter + 0x8), 2);
	}

	printf("[2] Start(): state != 1 -> no-op\n");
	{
		unsigned char *trackMem = (unsigned char *)mmap32(0x100);
		memset(trackMem, 0xcc, 0x100);
		*(int *)(trackMem + 0x4) = 0;
		CSTGRecordTrack *track = (CSTGRecordTrack *)trackMem;
		int r = track->Start();
		check_eq("Start() returns 0", (unsigned int)r, 0);
		check_eq("state unchanged (0)", *(unsigned int *)(trackMem + 0x4), 0);
	}

	printf("[3] Pause(): state != 0 -> state=1\n");
	{
		unsigned char *trackMem = (unsigned char *)mmap32(0x100);
		memset(trackMem, 0xcc, 0x100);
		*(int *)(trackMem + 0x4) = 2;
		CSTGRecordTrack *track = (CSTGRecordTrack *)trackMem;
		int r = track->Pause();
		check_eq("Pause() returns 1", (unsigned int)r, 1);
		check_eq("state == 1", *(unsigned int *)(trackMem + 0x4), 1);
	}

	printf("[4] Stop(): state==0 -> just resets to 0, no teardown\n");
	{
		unsigned char *trackMem = (unsigned char *)mmap32(0x100);
		memset(trackMem, 0xcc, 0x100);
		*(int *)(trackMem + 0x4) = 0;
		CSTGRecordTrack *track = (CSTGRecordTrack *)trackMem;
		int r = track->Stop();
		check_eq("Stop() returns 1", (unsigned int)r, 1);
		check_eq("state == 0", *(unsigned int *)(trackMem + 0x4), 0);
	}

	printf("[5] Stop(): state==2, activeBuffer==0 -> fallback push into CSTGFileCloser\n");
	{
		unsigned char *trackMem = (unsigned char *)mmap32(0x100);
		memset(trackMem, 0xcc, 0x100);
		unsigned char *meter = (unsigned char *)mmap32(0x100);
		memset(meter, 0xcc, 0x100);
		unsigned char *fc = (unsigned char *)mmap32(0x100);
		memset(fc, 0, 0x100);
		unsigned char *fcRing = (unsigned char *)mmap32(0x100);
		memset(fcRing, 0xcc, 0x100);
		*(unsigned int *)(fc + 0x0) = ToU32(fcRing);
		*(unsigned int *)(fc + 0x4) = 0;
		*(unsigned int *)(fc + 0xc) = 8; /* capacity */
		CSTGFileCloser::sInstance = (CSTGFileCloser *)fc;

		*(int *)(trackMem + 0x4) = 2;
		*(unsigned int *)(trackMem + 0x8) = ToU32(meter);
		*(unsigned int *)(trackMem + 0x1c) = 0; /* activeBuffer == 0 */

		CSTGRecordTrack *track = (CSTGRecordTrack *)trackMem;
		int r = track->Stop();
		check_eq("Stop() returns 1", (unsigned int)r, 1);
		check_eq("meter+0x8 == 3", *(unsigned int *)(meter + 0x8), 3);
		check_eq("meter+0xc == 3 (tag)", *(unsigned int *)(meter + 0xc), 3);
		check_eq("FileCloser ring[0] == meter ptr", *(unsigned int *)(fcRing + 0), ToU32(meter));
		check_eq("FileCloser ring[0].tag == 0", *(unsigned int *)(fcRing + 4), 0);
		check_eq("FileCloser write cursor advanced", *(unsigned int *)(fc + 0x4), 1);
		check_eq("state reset to 0", *(unsigned int *)(trackMem + 0x4), 0);
	}

	printf("[6] Stop(): state==1, activeBuffer!=0 -> pushed into track's own ring\n");
	{
		unsigned char *trackMem = (unsigned char *)mmap32(0x100);
		memset(trackMem, 0xcc, 0x100);
		unsigned char *meter = (unsigned char *)mmap32(0x100);
		memset(meter, 0xcc, 0x100);
		unsigned char *activeBuf = (unsigned char *)mmap32(0x4000);
		memset(activeBuf, 0xcc, 0x4000);
		unsigned char *ownRing = (unsigned char *)mmap32(0x100);
		memset(ownRing, 0xcc, 0x100);

		*(int *)(trackMem + 0x4) = 1;
		*(unsigned int *)(trackMem + 0x8) = ToU32(meter);
		*(unsigned int *)(trackMem + 0xc) = ToU32(ownRing);
		*(unsigned int *)(trackMem + 0x10) = 0; /* ringWriteIdx */
		*(unsigned int *)(trackMem + 0x18) = 4; /* ringCapacity */
		*(unsigned int *)(trackMem + 0x1c) = ToU32(activeBuf);

		CSTGRecordTrack *track = (CSTGRecordTrack *)trackMem;
		int r = track->Stop();
		check_eq("Stop() returns 1", (unsigned int)r, 1);
		check_eq("meter+0x8 == 3", *(unsigned int *)(meter + 0x8), 3);
		check_eq("activeBuffer+0x300c == 1", activeBuf[0x300c], 1);
		check_eq("activeBuffer+0x3008 == 2", *(unsigned int *)(activeBuf + 0x3008), 2);
		check_eq("own ring[0] == activeBuffer ptr", *(unsigned int *)(ownRing + 0), ToU32(activeBuf));
		check_eq("ringWriteIdx advanced to 1", *(unsigned int *)(trackMem + 0x10), 1);
		check_eq("activeBuffer field cleared", *(unsigned int *)(trackMem + 0x1c), 0);
		check_eq("meterPtr field cleared", *(unsigned int *)(trackMem + 0x8), 0);
		check_eq("state reset to 0", *(unsigned int *)(trackMem + 0x4), 0);
	}

	printf("[7] ProcessRecordCommands(): dispatches tag 0/1/2/3 to the right track method\n");
	{
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);

		*(unsigned int *)(hdrMem + 0x18ae8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18aec) = 2; /* producer */
		*(unsigned int *)(hdrMem + 0x18af0) = 0; /* consumer */
		*(unsigned int *)(hdrMem + 0x18af4) = 8; /* capacity */

		/* entry[0]: tag=1 (Start), trackIdx=2 */
		*(unsigned char *)(ring + 0x0) = 1;
		*(unsigned int *)(ring + 0x8) = 2;
		/* entry[1]: tag=2 (Pause), trackIdx=2 */
		*(unsigned char *)(ring + 0x1c) = 2;
		*(unsigned int *)(ring + 0x1c + 0x8) = 2;

		unsigned char *track2 = hdrMem + 0x584 + 2 * 0xc0;
		unsigned char *meter2 = (unsigned char *)mmap32(0x100);
		memset(meter2, 0xcc, 0x100);
		*(int *)(track2 + 0x4) = 1; /* idle->standby, Start() will fire */
		*(unsigned int *)(track2 + 0x8) = ToU32(meter2); /* Start()/Pause() need a real meterPtr */

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessRecordCommands();

		check_eq("consumer index advanced to producer (2)",
			 *(unsigned int *)(hdrMem + 0x18af0), 2);
		check_eq("track[2] state == 1 (Start then Pause)",
			 *(unsigned int *)(track2 + 0x4), 1);
	}

	printf("==============================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
