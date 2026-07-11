// SPDX-License-Identifier: GPL-2.0
/*
 * test_hdr_playback_commands.cpp  -  host-side known-answer tests for
 * CSTGHDRManager::ProcessPlaybackCommands() (batch 51).
 *
 * Links src/engine/hdr_playback_commands.cpp AND src/engine/
 * file_opener_events.cpp for real -- unlike CSTGSampler's four deliberately
 * deferred no-op callees (test_hdr_sampler_commands.cpp's own caveat),
 * CSTGFileOpener::AddPlaybackEvent/AddRecordEvent are genuinely real this
 * same batch and have real, directly observable ring-write side effects,
 * so linking them for real gives a much stronger KAT than mocking would.
 *
 * `CSTGPlaybackBuffer::SetCurrentReadEvent`/`RemoveEvent` (already real,
 * batch 24/25, playback_buffer_events.cpp) and `signal_daemon()` (already
 * real, batch 51, stg_daemons.cpp) are deliberately MOCKED here rather than
 * linked -- both pull in substantial unrelated dependency chains
 * (CSTGHDRCircularBuffer/TSTGArrayManager/CSTGDiskCostManager for the
 * former, gStgDaemons/GetSTGTickCount/rt_pend_linux_srq for the latter)
 * that this function's own logic doesn't need to exercise; call-tracking
 * mocks let these tests verify exactly what ProcessPlaybackCommands()
 * itself does -- which object/event it dispatches to, in what order --
 * without dragging in or re-verifying those other already-tested classes.
 *
 * `CSTGFileOpener::sInstance` needs its own local storage here (this file
 * does not link managers.cpp, which normally defines it) -- same
 * established "give it its own local storage" precedent as sec 10.160.
 *
 * Every object whose address is round-tripped through a packed 32-bit
 * field is mmap32()/MAP_32BIT-backed, per this project's standing pointer-
 * width discipline.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_daemons.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

CSTGFileOpener *CSTGFileOpener::sInstance;

/* ---- CSTGPlaybackBuffer::SetCurrentReadEvent/RemoveEvent call-tracking mocks ---- */
struct CallLog {
	const char *what;
	unsigned int self;
	unsigned int arg;
};
static CallLog g_calls[32];
static int g_callCount;

void CSTGPlaybackBuffer::SetCurrentReadEvent(CSTGPlaybackEvent *newEvt)
{
	if (g_callCount < 32)
		g_calls[g_callCount++] = { "SetCurrentReadEvent", ToU32(this), ToU32(newEvt) };
}
void CSTGPlaybackBuffer::RemoveEvent(CSTGPlaybackEvent *event)
{
	if (g_callCount < 32)
		g_calls[g_callCount++] = { "RemoveEvent", ToU32(this), ToU32(event) };
}

/* ---- signal_daemon call-tracking mock ---- */
static unsigned int g_signalled[8];
static int g_signalCount;
extern "C" void signal_daemon(unsigned int daemonIndex)
{
	if (g_signalCount < 8)
		g_signalled[g_signalCount++] = daemonIndex;
}

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok) g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok) printf("        (wanted 0x%x)\n", want);
}
static void check_true(const char *label, bool ok)
{
	if (!ok) g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static void reset_mocks()
{
	g_callCount = 0;
	memset(g_calls, 0, sizeof(g_calls));
	g_signalCount = 0;
	memset(g_signalled, 0, sizeof(g_signalled));
}

int main(void)
{
	printf("CSTGHDRManager::ProcessPlaybackCommands test\n");
	printf("==============================================================\n");

	printf("[1] Empty ring (consumer == producer): no-op\n");
	{
		reset_mocks();
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);

		*(unsigned int *)(hdrMem + 0x18ad8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18adc) = 3; /* producer */
		*(unsigned int *)(hdrMem + 0x18ae0) = 3; /* consumer, same */
		*(unsigned int *)(hdrMem + 0x18ae4) = 8; /* capacity */

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessPlaybackCommands();

		check_eq("consumer index unchanged", *(unsigned int *)(hdrMem + 0x18ae0), 3);
		check_eq("no CSTGPlaybackBuffer calls", g_callCount, 0);
		check_eq("no signal_daemon calls", g_signalCount, 0);
	}

	printf("[2] tag==3: walks the +0x18a98 array, SetCurrentReadEvent per node, clears +0x18a95\n");
	{
		reset_mocks();
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);
		hdrMem[0x18a95] = 0xff; /* nonzero, must be cleared by tag==3 */

		unsigned char *nodeA = (unsigned char *)mmap32(0x100);
		unsigned char *nodeB = (unsigned char *)mmap32(0x100);
		unsigned char *ownerA = (unsigned char *)mmap32(0x100);
		unsigned char *ownerB = (unsigned char *)mmap32(0x100);
		*(unsigned int *)(nodeA + 0x30) = ToU32(ownerA);
		*(unsigned int *)(nodeB + 0x30) = ToU32(ownerB);
		*(unsigned int *)(hdrMem + 0x18a98 + 0 * 4) = ToU32(nodeA);
		*(unsigned int *)(hdrMem + 0x18a98 + 1 * 4) = ToU32(nodeB);

		unsigned char *e0 = ring + 0 * 0xc;
		e0[0x00] = 3;
		*(unsigned int *)(e0 + 0x08) = 2; /* count */

		*(unsigned int *)(hdrMem + 0x18ad8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18adc) = 1; /* producer */
		*(unsigned int *)(hdrMem + 0x18ae0) = 0; /* consumer */
		*(unsigned int *)(hdrMem + 0x18ae4) = 8; /* capacity */

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessPlaybackCommands();

		check_eq("consumer advanced to producer", *(unsigned int *)(hdrMem + 0x18ae0), 1);
		check_eq("two SetCurrentReadEvent calls", g_callCount, 2);
		check_true("call[0] == ownerA->SetCurrentReadEvent(nodeA)",
			   g_calls[0].self == ToU32(ownerA) && g_calls[0].arg == ToU32(nodeA));
		check_true("call[1] == ownerB->SetCurrentReadEvent(nodeB)",
			   g_calls[1].self == ToU32(ownerB) && g_calls[1].arg == ToU32(nodeB));
		check_eq("+0x18a95 cleared", hdrMem[0x18a95], 0);
	}

	printf("[3] tag==2: AddPlaybackEvent(opener), AddRecordEvent(sInstance), fieldC cleared, signal_daemon(0)\n");
	{
		reset_mocks();
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);

		unsigned char *eventMem = (unsigned char *)mmap32(0x100);
		memset(eventMem, 0, 0x100);
		((CSTGAudioEvent *)eventMem)->fieldC = 4; /* ctor-default sentinel */

		unsigned char *openerMem = (unsigned char *)mmap32(0x300);
		memset(openerMem, 0, 0x300);
		*(unsigned int *)(openerMem + 0x0) = 9; /* opener's own field0 -> AddRecordEvent's index */

		unsigned char *pbLaneBase = (unsigned char *)mmap32(0x1000);
		memset(pbLaneBase, 0, 0x1000);
		/* The ring only has ONE populated entry below (producer==1), and
		 * producerIdx doubles as AddPlaybackEvent's own index argument
		 * (see hdr_playback_commands.cpp's derivation) -- so the lane
		 * under test must be index 1, matching producer==1 exactly. */
		unsigned char *pbLane = openerMem + 1 * 0x10;
		*(unsigned int *)(pbLane + 0x0) = ToU32(pbLaneBase);
		*(unsigned int *)(pbLane + 0x8) = 5;  /* != (writeIdx+1)%capacity, avoid unintended overflow path */
		*(unsigned int *)(pbLane + 0xc) = 8;

		unsigned char *sInstMem = (unsigned char *)mmap32(0x300);
		memset(sInstMem, 0, 0x300);
		CSTGFileOpener::sInstance = (CSTGFileOpener *)sInstMem;
		unsigned char *recLaneBase = (unsigned char *)mmap32(0x1000);
		memset(recLaneBase, 0, 0x1000);
		/* AddRecordEvent's own real lane formula is this+index*0x10+0x100
		 * (see file_opener_events.cpp) -- index 9, from openerMem+0x0. */
		unsigned char *recLane = sInstMem + 9 * 0x10 + 0x100;
		*(unsigned int *)(recLane + 0x0) = ToU32(recLaneBase);
		*(unsigned int *)(recLane + 0x8) = 5;  /* != (writeIdx+1)%capacity, avoid unintended overflow path */
		*(unsigned int *)(recLane + 0xc) = 8;

		unsigned char *e0 = ring + 0 * 0xc;
		e0[0x00] = 2;
		*(unsigned int *)(e0 + 0x04) = ToU32(eventMem);
		*(unsigned int *)(e0 + 0x08) = ToU32(openerMem);

		*(unsigned int *)(hdrMem + 0x18ad8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18adc) = 1; /* producer: one ring entry; also == the index AddPlaybackEvent gets */
		*(unsigned int *)(hdrMem + 0x18ae0) = 0; /* consumer */
		*(unsigned int *)(hdrMem + 0x18ae4) = 8; /* capacity */

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessPlaybackCommands();

		check_eq("consumer advanced", *(unsigned int *)(hdrMem + 0x18ae0), 1);
		check_eq("opener's playback lane[0] holds the event", ((unsigned int *)pbLaneBase)[0], ToU32(eventMem));
		check_eq("sInstance's record lane[0] holds the event", ((unsigned int *)recLaneBase)[0], ToU32(eventMem));
		check_eq("event->fieldC cleared to 0", ((CSTGAudioEvent *)eventMem)->fieldC, 0);
		check_eq("signal_daemon called once", g_signalCount, 1);
		check_eq("...with index 0", g_signalled[0], 0);
	}

	printf("[4] tag==1: index from event+0x2e (u16), AddRecordEvent via sInstance, fieldC cleared\n");
	{
		reset_mocks();
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);

		unsigned char *eventMem = (unsigned char *)mmap32(0x100);
		memset(eventMem, 0, 0x100);
		((CSTGAudioEvent *)eventMem)->fieldC = 4;
		*(unsigned short *)(eventMem + 0x2e) = 6;

		unsigned char *sInstMem = (unsigned char *)mmap32(0x300);
		memset(sInstMem, 0, 0x300);
		CSTGFileOpener::sInstance = (CSTGFileOpener *)sInstMem;
		unsigned char *recLaneBase = (unsigned char *)mmap32(0x1000);
		memset(recLaneBase, 0, 0x1000);
		unsigned char *recLane = sInstMem + 6 * 0x10 + 0x100;
		*(unsigned int *)(recLane + 0x0) = ToU32(recLaneBase);
		*(unsigned int *)(recLane + 0x8) = 5;  /* != (writeIdx+1)%capacity, avoid unintended overflow path */
		*(unsigned int *)(recLane + 0xc) = 8;

		unsigned char *e0 = ring + 0 * 0xc;
		e0[0x00] = 1;
		*(unsigned int *)(e0 + 0x04) = ToU32(eventMem);

		*(unsigned int *)(hdrMem + 0x18ad8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18adc) = 1;
		*(unsigned int *)(hdrMem + 0x18ae0) = 0;
		*(unsigned int *)(hdrMem + 0x18ae4) = 8;

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessPlaybackCommands();

		check_eq("sInstance's record lane[0] (index 6) holds the event",
			 ((unsigned int *)recLaneBase)[0], ToU32(eventMem));
		check_eq("event->fieldC cleared to 0", ((CSTGAudioEvent *)eventMem)->fieldC, 0);
		check_eq("signal_daemon(0) called once", g_signalCount, 1);
	}

	printf("[5] tag==0: child-list walk (state transitions + conditional RemoveEvent) then top-level event\n");
	{
		reset_mocks();
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);

		unsigned char *eventMem = (unsigned char *)mmap32(0x100);
		memset(eventMem, 0, 0x100);
		unsigned char *ownerMem = (unsigned char *)mmap32(0x100);

		/* Child list: node1 -> node2 -> NULL. node1: state=1 (mutate),
		 * fieldC==4 (default) -> triggers RemoveEvent. node2: state=3
		 * (already finished) -> skipped entirely. */
		unsigned char *node1 = (unsigned char *)mmap32(0x100);
		memset(node1, 0, 0x100);
		unsigned char *node2 = (unsigned char *)mmap32(0x100);
		memset(node2, 0, 0x100);
		((CSTGAudioEvent *)node1)->field8 = 1;
		((CSTGAudioEvent *)node1)->fieldC = 4;
		*(unsigned int *)(node1 + 0x30) = ToU32(ownerMem);
		*(unsigned int *)(node1 + 0x58) = ToU32(node2);
		((CSTGAudioEvent *)node2)->field8 = 3; /* already finished, must be skipped */
		*(unsigned int *)(node2 + 0x58) = 0;

		*(unsigned int *)(eventMem + 0x58) = ToU32(node1);
		eventMem[0x16] = 0x55; /* must be cleared */
		((CSTGAudioEvent *)eventMem)->fieldC = 0; /* NOT the sentinel -> top-level takes the "else" branch */

		unsigned char *e0 = ring + 0 * 0xc;
		e0[0x00] = 0;
		*(unsigned int *)(e0 + 0x04) = ToU32(eventMem);

		*(unsigned int *)(hdrMem + 0x18ad8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18adc) = 1;
		*(unsigned int *)(hdrMem + 0x18ae0) = 0;
		*(unsigned int *)(hdrMem + 0x18ae4) = 8;

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessPlaybackCommands();

		check_eq("event->+0x16 cleared", eventMem[0x16], 0);
		check_eq("node1 state mutated to 3", ((CSTGAudioEvent *)node1)->field8, 3);
		check_eq("node2 state untouched (still 3)", ((CSTGAudioEvent *)node2)->field8, 3);
		check_eq("exactly one RemoveEvent call (node1 only)", g_callCount, 1);
		check_true("...it was ownerMem->RemoveEvent(node1)",
			   g_calls[0].self == ToU32(ownerMem) && g_calls[0].arg == ToU32(node1) &&
			   strcmp(g_calls[0].what, "RemoveEvent") == 0);
		check_eq("top-level event->field8 set to 3 (fieldC wasn't 4)",
			 ((CSTGAudioEvent *)eventMem)->field8, 3);
	}

	printf("[6] Unknown tag: entry silently consumed, no dispatch\n");
	{
		reset_mocks();
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);
		ring[0] = 99; /* unrecognized tag */

		*(unsigned int *)(hdrMem + 0x18ad8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18adc) = 1;
		*(unsigned int *)(hdrMem + 0x18ae0) = 0;
		*(unsigned int *)(hdrMem + 0x18ae4) = 8;

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessPlaybackCommands();

		check_eq("consumer advanced past unknown tag", *(unsigned int *)(hdrMem + 0x18ae0), 1);
		check_eq("no CSTGPlaybackBuffer calls", g_callCount, 0);
		check_eq("no signal_daemon calls", g_signalCount, 0);
	}

	printf("==============================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
