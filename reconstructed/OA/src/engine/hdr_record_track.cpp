// SPDX-License-Identifier: GPL-2.0
/*
 * hdr_record_track.cpp  -  CSTGRecordTrack::Start()/Pause()/Stop() and
 * CSTGHDRManager::ProcessRecordCommands() (batch 15).
 *
 * Deliberately a SEPARATE translation unit from managers.cpp (matching
 * the CSTGStreamingEventManager/CSetList precedent, sec 10.145/10.158):
 * managers.cpp is directly linked by five separate verify/ binaries
 * (test_managers/test_engine/test_global/test_global_ctor/
 * test_engine_startup_bits2, sec 10.160's own "5 separate verify
 * binaries" finding) and CSTGRecordTrack is a brand-new class with no
 * pre-existing mocks in any of them -- adding it there would force every
 * one of those five to gain new link-satisfying declarations just to
 * keep compiling. Giving this its own dedicated TU + its own dedicated
 * `verify/test_hdr_record_track.cpp` avoids touching any of them.
 *
 * CSTGHDRManager::ProcessRecordCommands() (`.text+0xd5b20`, 303 bytes)
 * confirmed: a ring-buffer consumer loop, same overall shape as
 * CSTGCDWorker/CSTGSamplingDaemon::ProcessCommands() (sec 10.158/10.160),
 * at THIS class's own field offsets:
 *   +0x18ae8 ring base pointer (packed 32-bit)
 *   +0x18aec producer index (never written here)
 *   +0x18af0 consumer index (advanced here)
 *   +0x18af4 capacity (modulus for wraparound)
 * Each entry is 28 bytes (7 dwords), confirmed via `imul eax,edx,0x1c`:
 *   +0x00 tag (only the low byte is read)
 *   +0x04 A  (passed to StandbyRec's arg1, a `const char*`)
 *   +0x08 trackIdx (0..15, selects `this+0x584 + trackIdx*0xc0`, NOT
 *         passed to StandbyRec -- confirmed via full register tracing:
 *         it's consumed entirely as the array index, never loaded into
 *         an argument register/stack slot)
 *   +0x0c C  (passed to StandbyRec's arg2)
 *   +0x10 D  (passed to StandbyRec's arg3)
 *   +0x14 E  (passed to StandbyRec's arg4)
 *   +0x18 F  (byte, passed to StandbyRec's arg5)
 * Dispatch on `tag`:
 *   tag==0: `track->StandbyRec(A, C, D, E, F)`
 *   tag==1: `track->Start()`
 *   tag==2: `track->Pause()`
 *   tag==3: `track->Stop()`
 *   any other tag: no-op (entry silently consumed, consumer index still
 *   advances) -- matches CSTGCDWorker/CSTGSamplingDaemon's own established
 *   "unhandled tag" quirk.
 *
 * CSTGRecordTrack::Start()/Pause()/Stop() (`.text+0xd7470`/`0xd7560`/
 * `0xd74a0`, 33/23/177 bytes) fully reconstructed -- see field map in
 * oa_engine.h's own CSTGRecordTrack comment. `Stop()`'s own "no active
 * CSTGRecordBuffer" fallback (`activeBuffer==0`) reuses the EXACT SAME
 * tag-then-push mechanism already confirmed for CSTGSamplingDaemon::
 * ProcessCommands()'s own tag==0 case (sec 10.160): write `meterPtr+0xc =
 * 3`, then push `{meterPtr, 0}` onto `CSTGFileCloser::sInstance`'s FIRST
 * embedded ring (`+0x0` base/`+0x4` write-cursor/`+0xc` capacity) --
 * independent cross-confirmation of that same mechanism from a THIRD
 * caller.
 */

#include "oa_global.h"
#include "oa_engine.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

int CSTGRecordTrack::Start()
{
	unsigned char *base = (unsigned char *)this;
	int result = 0;

	if (*(int *)(base + 0x4) == 1) {
		*(int *)(base + 0x4) = 2;
		unsigned char *meter = FromU32(*(unsigned int *)(base + 0x8));
		result = 1;
		*(unsigned int *)(meter + 0x8) = 2;
	}
	return result;
}

int CSTGRecordTrack::Pause()
{
	unsigned char *base = (unsigned char *)this;
	int result = 0;

	if (*(int *)(base + 0x4) != 0) {
		*(int *)(base + 0x4) = 1;
		result = 1;
	}
	return result;
}

int CSTGRecordTrack::Stop()
{
	unsigned char *base = (unsigned char *)this;
	int state = *(int *)(base + 0x4);

	if (state != 1 && state != 2) {
		*(int *)(base + 0x4) = 0;
		return 1;
	}

	unsigned char *meter = FromU32(*(unsigned int *)(base + 0x8));
	*(unsigned int *)(meter + 0x8) = 3;

	unsigned char *activeBuffer = FromU32(*(unsigned int *)(base + 0x1c));
	if (activeBuffer == 0) {
		/* Fallback: no CSTGRecordBuffer currently owned -- push the
		 * meter pointer straight into CSTGFileCloser's first ring
		 * instead (same tag-then-push mechanism as
		 * CSTGSamplingDaemon::ProcessCommands()'s tag==0 case). */
		*(unsigned int *)(meter + 0xc) = 3;

		unsigned char *fc = (unsigned char *)CSTGFileCloser::sInstance;
		unsigned int fcCursor = *(unsigned int *)(fc + 0x4);
		unsigned char *fcEntry = FromU32(*(unsigned int *)(fc + 0x0)) + fcCursor * 8;
		*(unsigned int *)(fcEntry + 0) = ToU32(meter);
		*(unsigned int *)(fcEntry + 4) = 0;
		*(unsigned int *)(fc + 0x4) = (fcCursor + 1) % *(unsigned int *)(fc + 0xc);

		*(int *)(base + 0x4) = 0;
		return 1;
	}

	*(unsigned char *)(activeBuffer + 0x300c) = 1;
	*(unsigned int *)(activeBuffer + 0x3008) = 2;

	unsigned int ringWriteIdx = *(unsigned int *)(base + 0x10);
	unsigned char *ringBase = FromU32(*(unsigned int *)(base + 0xc));
	*(unsigned int *)(ringBase + ringWriteIdx * 4) = ToU32(activeBuffer);

	unsigned int nextIdx = (ringWriteIdx + 1) % *(unsigned int *)(base + 0x18);
	*(unsigned int *)(base + 0x1c) = 0;
	*(unsigned int *)(base + 0x10) = nextIdx;
	*(unsigned int *)(base + 0x8) = 0;
	*(int *)(base + 0x4) = 0;
	return 1;
}

void CSTGHDRManager::ProcessRecordCommands()
{
	unsigned char *base = (unsigned char *)this;

	unsigned int consumerIdx = *(unsigned int *)(base + 0x18af0);
	unsigned int producerIdx = *(unsigned int *)(base + 0x18aec);

	while (consumerIdx != producerIdx) {
		unsigned char *entry = FromU32(*(unsigned int *)(base + 0x18ae8)) + consumerIdx * 0x1c;

		unsigned char tag = *(unsigned char *)(entry + 0x0);
		unsigned int A = *(unsigned int *)(entry + 0x4);
		unsigned int trackIdx = *(unsigned int *)(entry + 0x8);
		unsigned int C = *(unsigned int *)(entry + 0xc);
		unsigned int D = *(unsigned int *)(entry + 0x10);
		unsigned int E = *(unsigned int *)(entry + 0x14);
		unsigned char F = *(unsigned char *)(entry + 0x18);

		unsigned int nextIdx = (consumerIdx + 1) % *(unsigned int *)(base + 0x18af4);
		*(unsigned int *)(base + 0x18af0) = nextIdx;

		CSTGRecordTrack *track = (CSTGRecordTrack *)(base + 0x584 + trackIdx * 0xc0);

		if (tag == 0) {
			track->StandbyRec((const char *)FromU32(A), C, D, (int)E, F);
		} else if (tag == 1) {
			track->Start();
		} else if (tag == 2) {
			track->Pause();
		} else if (tag == 3) {
			track->Stop();
		}

		consumerIdx = *(unsigned int *)(base + 0x18af0);
		producerIdx = *(unsigned int *)(base + 0x18aec);
	}
}

/*
 * CSTGRecordTrack::StandbyRec() -- confirmed real, deliberately deferred
 * (see the declaration's own comment in oa_engine.h): 440 bytes, own body
 * not reconstructed this pass. A deliberate no-op stand-in, matching this
 * project's established "Bar 2" convention (bar2_stubs.cpp) -- given its
 * own dedicated body here (rather than in bar2_stubs.cpp) purely so this
 * file links standalone, since bar2_stubs.cpp is never linked into any
 * verify/ binary.
 */
void CSTGRecordTrack::StandbyRec(const char *, unsigned int, unsigned long, int, unsigned char)
{
}
