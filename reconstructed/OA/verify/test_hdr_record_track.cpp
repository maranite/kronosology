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
#include "oa_bank_memory.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/* Not linked from audio_bus_manager.cpp in this test binary -- own local
 * storage, matching this project's established per-test-file precedent. */
unsigned char CSTGAudioBusManager::sGlobalBusSet[34 * 0x80];

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

	printf("[3] CSTGMonitorMixerChannel::Initialize(busIndex) (batch 22)\n");
	{
		unsigned char *chanMem = (unsigned char *)mmap32(0x1000);
		memset(chanMem, 0xcc, 0x1000);
		unsigned char *targetObj = (unsigned char *)mmap32(0x1000);
		memset(targetObj, 0xcc, 0x1000);
		/* Real caveat, documented in this method's own header comment:
		 * its own ctor is still a no-op, so +0x4 is never really
		 * populated -- provide it explicitly here. */
		*(unsigned int *)(chanMem + 0x4) = ToU32(targetObj);

		CSTGMonitorMixerChannel *chan = (CSTGMonitorMixerChannel *)chanMem;
		chan->Initialize(5);

		check_eq("+0x0 == busIndex (5)", chanMem[0x0], 5);
		check_eq("target+0x60 == &sGlobalBusSet[5]",
			 *(unsigned int *)(targetObj + 0x60),
			 ToU32(CSTGAudioBusManager::sGlobalBusSet + 5 * 0x80));
	}

	printf("[4] CSTGRecordTrack::Initialize(trackIdx) (batch 22)\n");
	{
		unsigned char *pool = (unsigned char *)mmap32(0x10000);
		CSTGBankMemory::Initialize(pool, 0x10000);

		unsigned char *trackMem = (unsigned char *)mmap32(0x1000);
		memset(trackMem, 0xcc, 0x1000);
		/* `this+0x24` IS the embedded CSTGMonitorMixerChannel's own
		 * `+0x4` field (`this+0x20+0x4` == `this+0x24`) -- ONE shared
		 * buffer, not two independent ones (an earlier draft of this
		 * KAT used two separate buffers and got a real check FAILED
		 * as a result -- see this method's own header comment in
		 * hdr_record_track.cpp for the full derivation). That
		 * pointee's own ctor is still a no-op stub (same real,
		 * pre-existing gap as [3] above), so it needs an explicit
		 * valid backing buffer here too. */
		unsigned char *otherObj = (unsigned char *)mmap32(0x1000);
		memset(otherObj, 0xcc, 0x1000);
		*(unsigned int *)(trackMem + 0x24) = ToU32(otherObj);

		CSTGRecordTrack *track = (CSTGRecordTrack *)trackMem;
		track->Initialize(9);

		check_eq("+0x0 == trackIdx (9)", *(unsigned short *)(trackMem + 0x0), 9);
		check_eq("+0x4 state == 0", *(unsigned int *)(trackMem + 0x4), 0);
		check_eq("+0x8 meterPtr == 0", *(unsigned int *)(trackMem + 0x8), 0);
		check_eq("+0x18 ringCapacity == 0x61", *(unsigned int *)(trackMem + 0x18), 0x61);
		check_eq("+0x1c activeBuffer == 0", *(unsigned int *)(trackMem + 0x1c), 0);
		check_eq("+0xc ringBase non-null", *(unsigned int *)(trackMem + 0xc) != 0, 1);
		check_eq("+0xac == 0 (gap)", *(unsigned int *)(trackMem + 0xac), 0);
		check_eq("+0xb0 == 1", trackMem[0xb0], 1);
		check_eq("+0xb4 == 0x20", *(unsigned int *)(trackMem + 0xb4), 0x20);
		check_eq("+0xb8 == 0", *(unsigned int *)(trackMem + 0xb8), 0);
		check_eq("+0xbc == 1.0f", *(unsigned int *)(trackMem + 0xbc), 0x3f800000);
		check_eq("otherObj+0x68 == 1.0f", *(unsigned int *)(otherObj + 0x68), 0x3f800000);
		check_eq("embedded MonitorMixerChannel +0x0 == 9 (trackIdx)",
			 trackMem[0x20 + 0x0], 9);
		check_eq("otherObj+0x60 == &sGlobalBusSet[9] (same pointee as +0x68 above)",
			 *(unsigned int *)(otherObj + 0x60),
			 ToU32(CSTGAudioBusManager::sGlobalBusSet + 9 * 0x80));
	}

	printf("==============================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
