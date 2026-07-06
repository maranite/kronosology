// SPDX-License-Identifier: GPL-2.0
/*
 * test_playback_event_methods.cpp  -  KAT for
 * ../src/engine/playback_event_methods.cpp's CSTGPlaybackEvent methods
 * (batch 25): CSTGAudioEvent::Reset(), CSTGPlaybackEvent::Reset()/
 * HandleFileOpened()/HandleFileClosed()/HandleErrorOpening()/
 * HandleErrorReading()/GetDispositionForReadAttempt()/
 * IncrementBufferStartLocation()/SeekSkipFileBytes()/~CSTGPlaybackEvent().
 *
 * Deliberately lightweight -- links ONLY playback_event_methods.cpp, not
 * playback_buffer_events.cpp (that pair's own real interaction --
 * HandleFileClosed() -> CSTGPlaybackBuffer::RemoveEvent() -> event->
 * Reset() -- is exercised for real in test_playback_buffer_events.cpp,
 * which links both files together). HandleFileClosed()'s OWN dispatch
 * logic (does it call RemoveEvent with the right event, only when
 * state==3?) is still verified here, against a local mock of
 * CSTGPlaybackBuffer::RemoveEvent that just counts calls and records its
 * argument -- the real RemoveEvent() body isn't re-tested a second time.
 *
 * IncrementBufferStartLocation()'s "owner buffer" fake is just a raw
 * mmap32()'d buffer with CSTGHDRCircularBuffer's own bufferEnd(+0x14)/
 * effectiveSize(+0x1c) fields poked directly -- doesn't need a real
 * CSTGPlaybackBuffer::Initialize() call, since this method only ever
 * does address arithmetic on those two fields, never dereferences them.
 *
 * Every object whose own address gets round-tripped through a packed
 * 32-bit field is mmap32()/MAP_32BIT-backed, per this project's own
 * standing sec 10.156/10.157 pointer-width discipline.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine.h"
#include "oa_engine_init.h"

/* ---- mocks needed to link playback_event_methods.cpp standalone ---- */
extern "C" unsigned int CSTGFile_GetFileSize(void *handle);

static unsigned int g_fakeFileSize;
extern "C" unsigned int CSTGFile_GetFileSize(void *handle)
{
	(void)handle;
	return g_fakeFileSize;
}

/* CSTGPlaybackEvent's own real vtable placeholder -- same 40-byte
 * confirmed size (readelf) as every other sibling in this family
 * (_ZTV14CSTGAudioEvent/_ZTV15CSTGRecordEvent), same per-test local-
 * storage convention already established in test_engine_init.cpp. */
unsigned char _ZTV14CSTGAudioEvent[40];
unsigned char _ZTV17CSTGPlaybackEvent[40];

/* The ONLY CSTGPlaybackBuffer member HandleFileClosed() (the sole
 * caller reached from this file) ever references -- no other
 * CSTGPlaybackBuffer method/ctor is pulled in, so none of its siblings
 * need a mock here (unreferenced symbols aren't needed at link time). */
static int g_removeEventCalls;
static unsigned int g_removeEventArg;
void CSTGPlaybackBuffer::RemoveEvent(CSTGPlaybackEvent *event)
{
	g_removeEventCalls++;
	g_removeEventArg = (unsigned int)(unsigned long)event;
}

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%lx)\n", want);
}

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

int main(void)
{
	printf("playback_event_methods known-answer test\n");
	printf("=========================================================\n");

	printf("[1] CSTGAudioEvent::Reset() -- identical to the ctor's own fields\n");
	{
		unsigned char *buf = mmap32(sizeof(CSTGAudioEvent));
		memset(buf, 0xcc, sizeof(CSTGAudioEvent));
		CSTGAudioEvent *e = (CSTGAudioEvent *)buf;
		e->vtablePtr32 = 0xdeadbeef;	/* Reset() must NOT touch this */
		e->Reset();
		check_eq("field8 == 0", e->field8, 0);
		check_eq("fieldC == 4", e->fieldC, 4);
		check_eq("field10 == 0", e->field10, 0);
		check_eq("field1c == 1", e->field1c, 1);
		check_eq("field18 == 0", e->field18, 0);
		check_eq("field24 == 0", e->field24, 0);
		check_eq("field28 == 0", e->field28, 0);
		check_eq("field14 == 0", e->field14, 0);
		check_eq("field15 == 0", e->field15, 0);
		check_eq("field16 == 0", e->field16, 0);
		check_eq("field1d == 2", e->field1d, 2);
		check_eq("sampleRate == 0xbb80", e->sampleRate, 0xbb80);
		check_eq("vtablePtr32 untouched by Reset()", e->vtablePtr32, 0xdeadbeef);
	}

	unsigned char *evtBuf = mmap32(0x68);
	CSTGPlaybackEvent *evt = (CSTGPlaybackEvent *)evtBuf;

	printf("[2] CSTGPlaybackEvent::Reset() -- base Reset() + 14 confirmed zero-stores\n");
	{
		memset(evtBuf, 0xcc, 0x68);
		evt->Reset();
		CSTGAudioEvent *base = (CSTGAudioEvent *)evt;
		check_eq("base field8 == 0 (via CSTGAudioEvent::Reset())", base->field8, 0);
		check_eq("base sampleRate == 0xbb80", base->sampleRate, 0xbb80);
		check_eq("+0x30 == 0", *(unsigned int *)(evtBuf + 0x30), 0);
		check_eq("+0x34 == 0", *(unsigned int *)(evtBuf + 0x34), 0);
		check_eq("+0x38 == 0", *(unsigned int *)(evtBuf + 0x38), 0);
		check_eq("+0x3c == 0", *(unsigned int *)(evtBuf + 0x3c), 0);
		check_eq("+0x40 == 0", *(unsigned int *)(evtBuf + 0x40), 0);
		check_eq("+0x44 == 0", *(unsigned int *)(evtBuf + 0x44), 0);
		check_eq("+0x48 == 0", *(unsigned int *)(evtBuf + 0x48), 0);
		check_eq("+0x4c == 0", *(unsigned int *)(evtBuf + 0x4c), 0);
		check_eq("+0x50 == 0", *(unsigned int *)(evtBuf + 0x50), 0);
		check_eq("+0x54 == 0", *(unsigned int *)(evtBuf + 0x54), 0);
		check_eq("+0x58 == 0", *(unsigned int *)(evtBuf + 0x58), 0);
		check_eq("+0x60 == 0", evtBuf[0x60], 0);
		check_eq("+0x61 == 0", evtBuf[0x61], 0);
		check_eq("+0x64 == 0", *(unsigned int *)(evtBuf + 0x64), 0);
	}

	printf("[3] CSTGPlaybackEvent::HandleFileOpened()\n");
	{
		memset(evtBuf, 0, 0x68);
		/* real file handle value is opaque here -- only its round-trip
		 * through FromU32/ToU32 matters, not what it points to. */
		*(unsigned int *)(evtBuf + 0x24) = 0x1234;
		g_fakeFileSize = 1000;

		/* [3a] +0x34 (fileStartOffset) == 200, +0x48 (cap) starts at
		 * 5000 -- remaining (800) < cap, so cap gets clamped down. */
		*(unsigned int *)(evtBuf + 0x34) = 200;
		*(unsigned int *)(evtBuf + 0x48) = 5000;
		evt->HandleFileOpened();
		check_eq("+0x4c cached fileSize == 1000", *(unsigned int *)(evtBuf + 0x4c), 1000);
		check_eq("+0x48 clamped to remaining (800)", *(unsigned int *)(evtBuf + 0x48), 800);

		/* [3b] cap (50) already smaller than remaining (800) -- left
		 * untouched (real `jae` skip). */
		*(unsigned int *)(evtBuf + 0x48) = 50;
		evt->HandleFileOpened();
		check_eq("+0x48 NOT raised back up (still 50)", *(unsigned int *)(evtBuf + 0x48), 50);
	}

	printf("[4] CSTGPlaybackEvent::HandleFileClosed()\n");
	{
		memset(evtBuf, 0, 0x68);
		CSTGAudioEvent *base = (CSTGAudioEvent *)evt;
		unsigned char *ownerFake = mmap32(4);	/* address-only, never dereferenced by RemoveEvent's own mock */
		*(unsigned int *)(evtBuf + 0x30) = ToU32(ownerFake);

		/* [4a] state != 3: no-op. */
		base->field8 = 2;
		g_removeEventCalls = 0;
		evt->HandleFileClosed();
		check_eq("state!=3: RemoveEvent NOT called", g_removeEventCalls, 0);

		/* [4b] state == 3: dispatches to the OWNER's own RemoveEvent(this). */
		base->field8 = 3;
		evt->HandleFileClosed();
		check_eq("state==3: RemoveEvent called once", g_removeEventCalls, 1);
		check_eq("RemoveEvent's own argument == this event", g_removeEventArg, ToU32(evt));
	}

	printf("[5] CSTGPlaybackEvent::HandleErrorOpening()/HandleErrorReading() -- confirmed no-ops\n");
	{
		memset(evtBuf, 0xab, 0x68);
		unsigned char snapshot[0x68];
		memcpy(snapshot, evtBuf, 0x68);
		evt->HandleErrorOpening();
		evt->HandleErrorReading();
		check_eq("object memory byte-for-byte unchanged",
			 (unsigned long)(memcmp(evtBuf, snapshot, 0x68) == 0), 1);
	}

	printf("[6] CSTGPlaybackEvent::GetDispositionForReadAttempt()\n");
	{
		memset(evtBuf, 0, 0x68);
		CSTGAudioEvent *base = (CSTGAudioEvent *)evt;
		base->field1d = 4;			/* chunkSize */
		*(unsigned int *)(evtBuf + 0x44) = 100;	/* threshold */
		*(unsigned int *)(evtBuf + 0x38) = 50;		/* windowSize */
		*(unsigned int *)(evtBuf + 0x3c) = 10;		/* consumed -> span = 40 */
		*(unsigned int *)(evtBuf + 0x48) = 60;		/* maxReadBytes -> avail = 20 */

		/* [6a] pos < threshold -> 0 */
		check_eq("pos(50) < threshold(100) -> 0", evt->GetDispositionForReadAttempt(50), 0);

		/* [6b] pos == threshold, avail(20) >= 1 -> 3 */
		check_eq("pos==threshold, avail>=1 -> 3", evt->GetDispositionForReadAttempt(100), 3);

		/* [6c] pos == threshold, avail == 0 (maxReadBytes<=span) -> 1 */
		*(unsigned int *)(evtBuf + 0x48) = 40;	/* maxReadBytes == span -> avail clamped to 0 */
		check_eq("pos==threshold, avail==0 -> 1", evt->GetDispositionForReadAttempt(100), 1);
		*(unsigned int *)(evtBuf + 0x48) = 60;	/* restore */

		/* [6d] pos > threshold, within boundary (threshold + avail/chunkSize
		 * = 100 + 20/4 = 105) -> pos=104 < 105 -> 3 */
		check_eq("pos(104) < boundary(105) -> 3", evt->GetDispositionForReadAttempt(104), 3);

		/* [6e] pos >= boundary, avail(20) >= 1 -> 2 */
		check_eq("pos(105) >= boundary, avail>=1 -> 2", evt->GetDispositionForReadAttempt(105), 2);

		/* [6f] pos >= boundary, avail == 0 -> 1 */
		*(unsigned int *)(evtBuf + 0x48) = 40;	/* avail clamped to 0 */
		check_eq("pos>=boundary, avail==0 -> 1", evt->GetDispositionForReadAttempt(200), 1);
	}

	printf("[7] CSTGPlaybackEvent::IncrementBufferStartLocation()\n");
	{
		memset(evtBuf, 0, 0x68);
		unsigned char *ringFake = mmap32(0x34);
		memset(ringFake, 0, 0x34);
		CSTGHDRCircularBuffer *ring = (CSTGHDRCircularBuffer *)ringFake;
		ring->bufferEnd = 1000;
		ring->effectiveSize = 800;
		*(unsigned int *)(evtBuf + 0x30) = ToU32(ringFake);

		/* [7a] plain advance, no wrap, n within cachedFileSize. */
		*(unsigned int *)(evtBuf + 0x4c) = 500;	/* cachedFileSize */
		*(unsigned int *)(evtBuf + 0x40) = 100;
		evt->IncrementBufferStartLocation(50);
		check_eq("advanced by 50, no wrap", *(unsigned int *)(evtBuf + 0x40), 150);

		/* [7b] n clamped to cachedFileSize (500), landing exactly at
		 * bufferEnd -> wraps by effectiveSize. */
		*(unsigned int *)(evtBuf + 0x40) = 900;
		evt->IncrementBufferStartLocation(600);	/* clamped to 500 -> pos = 1400 >= 1000 -> wrap */
		check_eq("clamped-then-wrapped: (900+500)-800 == 600",
			 *(unsigned int *)(evtBuf + 0x40), 600);
	}

	printf("[8] CSTGPlaybackEvent::SeekSkipFileBytes()\n");
	{
		memset(evtBuf, 0, 0x68);
		*(unsigned int *)(evtBuf + 0x3c) = 10;
		*(unsigned int *)(evtBuf + 0x34) = 20;
		*(unsigned int *)(evtBuf + 0x48) = 1000;
		evt->SeekSkipFileBytes(30);
		check_eq("+0x3c += 30", *(unsigned int *)(evtBuf + 0x3c), 40);
		check_eq("+0x34 += 30", *(unsigned int *)(evtBuf + 0x34), 50);
		check_eq("+0x48 -= 30", *(unsigned int *)(evtBuf + 0x48), 970);
	}

	printf("[9] ~CSTGPlaybackEvent()\n");
	{
		memset(evtBuf, 0xcc, 0x68);
		evt->~CSTGPlaybackEvent();
		check_eq("vtable pointer reset to _ZTV14CSTGAudioEvent+8",
			 *(unsigned int *)evtBuf, ToU32(_ZTV14CSTGAudioEvent + 8));
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
