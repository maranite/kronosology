// SPDX-License-Identifier: GPL-2.0
/*
 * test_file_opener_events.cpp  -  host-side known-answer tests for
 * CSTGFileOpener::AddPlaybackEvent(CSTGAudioEvent*, unsigned int)/
 * AddRecordEvent(CSTGAudioEvent*, unsigned int) (batch 51), plus
 * CSTGFileOpener::Initialize() (batch 63) and ProcessCommands()
 * (2026-07-25).
 *
 * Links src/engine/file_opener_events.cpp + src/mem/bank_memory.cpp (only
 * Initialize() needs the latter, via CSTGBankMemory::AllocAligned() --
 * AddPlaybackEvent/AddRecordEvent remain fully self-contained, zero
 * relocations in the real disassembly). Drives a raw mmap32'd
 * CSTGFileOpener object directly (same "cast raw memory to the class, no
 * constructor" convention already used by test_hdr_sampler_commands.cpp/
 * test_hdr_record_track.cpp for these still-partially-opaque manager
 * classes).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine.h"
#include "oa_bank_memory.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/* Fixed-slot vtable dispatch targets for CSTGFileOpener::ProcessCommands()
 * (2026-07-25) -- same idiom as CSTGEffectRackVars::UpdateDModRoutings()
 * (global.cpp): a plain vtable slot call on an untyped payload object, not
 * the "unrecovered PTM table" this function was long documented as
 * blocked by. */
static int g_slot2Calls;
static void Slot2Handler(void *) { g_slot2Calls++; }
static int g_slot4Calls;
static void Slot4Handler(void *) { g_slot4Calls++; }
/* CSTGFileCloser::sInstance's own real storage lives in managers.cpp (not
 * linked here) -- this file needs its own local definition, same "give it
 * its own local storage" treatment already established elsewhere in this
 * project (e.g. test_managers.cpp's own TSTGArrayManager<CSTGRecordBuffer>::
 * sInstance). */
CSTGFileCloser *CSTGFileCloser::sInstance;

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

	printf("[5] CSTGFileOpener::Initialize(): 32 A/B lanes + fallback lane + own ring, sEventListMap\n");
	{
		unsigned char *pool = (unsigned char *)mmap32(0x200000);
		CSTGBankMemory::Initialize(pool, 0x200000);

		unsigned char *opMem = (unsigned char *)mmap32(0x300);
		memset(opMem, 0xcc, 0x300);

		CSTGFileOpener *opener = (CSTGFileOpener *)opMem;
		opener->Initialize();

		/* A lane 0 (== the object header itself) and A lane 7 (mid-loop) */
		check_eq("A lane 0 base non-null", *(unsigned int *)(opMem + 0x0) != 0, 1);
		check_eq("A lane 0 capacity == 0xc9", *(unsigned int *)(opMem + 0xc), 0xc9);
		check_eq("A lane 7 base non-null", *(unsigned int *)(opMem + 0x70) != 0, 1);
		check_eq("A lane 7 capacity == 0xc9", *(unsigned int *)(opMem + 0x7c), 0xc9);

		/* B lane 0 and B lane 15 (last of the loop) */
		check_eq("B lane 0 base non-null", *(unsigned int *)(opMem + 0x100) != 0, 1);
		check_eq("B lane 0 capacity == 0xc9", *(unsigned int *)(opMem + 0x10c), 0xc9);
		check_eq("B lane 15 base non-null", *(unsigned int *)(opMem + 0x1f0) != 0, 1);
		check_eq("B lane 15 capacity == 0xc9", *(unsigned int *)(opMem + 0x1fc), 0xc9);

		/* Fallback lane (+0x200) and the object's own command ring (+0x210) */
		check_eq("fallback lane base non-null", *(unsigned int *)(opMem + 0x200) != 0, 1);
		check_eq("fallback lane capacity == 0x349", *(unsigned int *)(opMem + 0x20c), 0x349);
		check_eq("own ring base non-null", *(unsigned int *)(opMem + 0x210) != 0, 1);
		check_eq("own ring capacity == 0x1069", *(unsigned int *)(opMem + 0x21c), 0x1069);

		/* sEventListMap: [0]==&opMem (A lane 0 IS the header), [15]==&A lane15,
		 * [16]==&B lane0, [31]==&B lane15, [32]==&fallback lane. */
		check_eq("sEventListMap[0] == opMem", ToU32(sEventListMap[0]), ToU32(opMem));
		check_eq("sEventListMap[15] == opMem+0xf0", ToU32(sEventListMap[15]), ToU32(opMem + 0xf0));
		check_eq("sEventListMap[16] == opMem+0x100", ToU32(sEventListMap[16]), ToU32(opMem + 0x100));
		check_eq("sEventListMap[31] == opMem+0x1f0", ToU32(sEventListMap[31]), ToU32(opMem + 0x1f0));
		check_eq("sEventListMap[32] == opMem+0x200", ToU32(sEventListMap[32]), ToU32(opMem + 0x200));

		/* Ring control fields (write/read idx) are NOT touched by Initialize()
		 * -- left exactly as the (simulated) ctor's zeroing, or here, whatever
		 * pre-fill was already there before the alloc'd base/capacity writes
		 * (Initialize only ever writes +0x0 and +0xc of each lane). */
		check_eq("A lane 0 writeIdx untouched (still 0xcccccccc)",
			 *(unsigned int *)(opMem + 0x4), 0xcccccccc);
	}

	printf("[6] CSTGFileOpener::ProcessCommands(): tag 0/1/2 + an unhandled tag\n");
	{
		/* Drains this object's own command ring at +0x210 (Initialize()'s
		 * "ownRing" above). Four entries: [0] tag==0 (vtable slot 2,
		 * payload+0xc=2), [1] tag==1 (vtable slot 4, payload+0xc=4,
		 * +0x10=1), [2] tag==2 (payload+0xc=3, push {payload,0} onto
		 * CSTGFileCloser::sInstance's +0x00 ring, no vtable call),
		 * [3] tag==5 (unhandled -- real, faithfully-preserved no-op). */
		unsigned char *opMem = (unsigned char *)mmap32(0x300);
		memset(opMem, 0, 0x300);
		unsigned char *ringBuf = (unsigned char *)mmap32(0x1000);
		memset(ringBuf, 0, 0x1000);
		unsigned char *payload0 = (unsigned char *)mmap32(0x20);
		unsigned char *payload1 = (unsigned char *)mmap32(0x20);
		unsigned char *payload2 = (unsigned char *)mmap32(0x20);
		unsigned char *payload3 = (unsigned char *)mmap32(0x20);
		memset(payload0, 0xcc, 0x20);
		memset(payload1, 0xcc, 0x20);
		memset(payload2, 0xcc, 0x20);
		memset(payload3, 0xcc, 0x20);

		void *vtbl[5] = { 0, 0, (void *)&Slot2Handler, 0, (void *)&Slot4Handler };
		*(void **)payload0 = vtbl;
		*(void **)payload1 = vtbl;

		unsigned char *ring = opMem + 0x210;
		ringBuf[0 * 8 + 0] = 0;
		*(unsigned int *)(ringBuf + 0 * 8 + 4) = ToU32(payload0);
		ringBuf[1 * 8 + 0] = 1;
		*(unsigned int *)(ringBuf + 1 * 8 + 4) = ToU32(payload1);
		ringBuf[2 * 8 + 0] = 2;
		*(unsigned int *)(ringBuf + 2 * 8 + 4) = ToU32(payload2);
		ringBuf[3 * 8 + 0] = 5;
		*(unsigned int *)(ringBuf + 3 * 8 + 4) = ToU32(payload3);

		*(unsigned int *)(ring + 0x0) = ToU32(ringBuf);
		*(unsigned int *)(ring + 0xc) = 0x10;	/* capacity */
		*(unsigned int *)(ring + 0x4) = 4;	/* write idx: 4 entries */
		*(unsigned int *)(ring + 0x8) = 0;	/* read idx */

		unsigned char fcMem[32];
		memset(fcMem, 0, 32);
		unsigned char *fcRing = (unsigned char *)mmap32(0x1000);
		*(unsigned int *)(fcMem + 0x0) = ToU32(fcRing);
		*(unsigned int *)(fcMem + 0xc) = 0x10;
		CSTGFileCloser::sInstance = (CSTGFileCloser *)fcMem;

		g_slot2Calls = 0;
		g_slot4Calls = 0;
		CSTGFileOpener *opener = (CSTGFileOpener *)opMem;
		opener->ProcessCommands();

		check_eq("read idx (+0x218) advanced to 4", *(unsigned int *)(ring + 0x8), 4);
		check_eq("tag==0: vtable slot 2 called exactly once", g_slot2Calls, 1);
		check_eq("tag==0: payload0's own +0xc field set to 2", *(unsigned int *)(payload0 + 0xc), 2);
		check_eq("tag==1: vtable slot 4 called exactly once", g_slot4Calls, 1);
		check_eq("tag==1: payload1's own +0xc field set to 4", *(unsigned int *)(payload1 + 0xc), 4);
		check_eq("tag==1: payload1's own +0x10 field set to 1", *(unsigned int *)(payload1 + 0x10), 1);
		check_eq("tag==2: payload2's own +0xc field set to 3", *(unsigned int *)(payload2 + 0xc), 3);
		check_eq("tag==2: CSTGFileCloser ring cursor (+0x4) advanced to 1",
			 *(unsigned int *)(fcMem + 0x4), 1);
		check_eq("tag==2: CSTGFileCloser ring entry[0] dword0 == payload2",
			 ((unsigned int *)fcRing)[0], ToU32(payload2));
		check_eq("tag==2: CSTGFileCloser ring entry[0] dword4 == 0",
			 ((unsigned int *)fcRing)[1], 0);
		check_eq("tag==5 (unhandled): payload3's own +0xc field left untouched (still 0xcc pattern)",
			 *(unsigned int *)(payload3 + 0xc), 0xcccccccc);
	}

	printf("======================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
