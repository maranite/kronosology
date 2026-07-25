// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_rt_data_reset_send_knobs_jump_catch.cpp  -  host-side
 * known-answer test for CSTGControllerRTData::ResetSendKnobsJumpCatch()
 * (batch 57).
 *
 * Covers the three guard conditions and cases 0/1 (mode dispatch), 4-6
 * (no-op), and 7 (audio-send dispatch) -- all reachable with the SAME
 * small mode-1 CSTGGlobal buffer size as
 * test_controller_rt_data_set_audio_in_solo.cpp (~29.9MB, the smallest of
 * ResolveCurrentPerformance's three real bases). Cases 2/3's own
 * per-track bus-routing table arithmetic (`+0x27cdb08`/`+0x27cea0f`) is
 * NOT independently exercised here (would need an even larger buffer,
 * deferred -- see HARDWARE_REVIEW_LOG.md) -- reconstructed faithfully
 * from the disassembly but not KAT-verified byte-for-byte.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine_init.h"

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-70s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

CSTGGlobal *CSTGGlobal::sInstance;

/* Local verified-identical copy of global.cpp's own (now externally
 * linked) ResolveCurrentPerformance() -- same duplication rationale as
 * test_controller_rt_data_set_audio_in_solo.cpp. */
CSTGPerformance *ResolveCurrentPerformance(unsigned char *base)
{
	int mode = *(int *)(base + 0x684);
	if (mode == 1) {
		int idx = (*(int *)(base + 0x69c)) & 0x7f;
		int bank = *(int *)(base + 0x690);
		int offset = idx * 0x19e7 + bank * 0xcf381 + 0x1c77f10 + 6;
		return (CSTGPerformance *)(base + offset);
	}
	if (mode == 2) {
		int seqIdx = *(int *)(base + 0x6a0);
		int offset = seqIdx * 0x1cad + 0x27cd024;
		return (CSTGPerformance *)(base + offset);
	}
	int progIdx = *(int *)(base + 0x698);
	if (progIdx == 0xfffe)
		return (CSTGPerformance *)(base + 0x2976e33);
	int idx = progIdx & 0x7f;
	int bank = *(int *)(base + 0x68c);
	int offset = idx * 0xcec + bank * 0x67603 + 0x132e4d0 + 3;
	return (CSTGPerformance *)(base + offset);
}

/* Real global.cpp's own IsCurrentlyActive() is a substantial real method
 * (managers.cpp) -- not linked here (would drag in managers.cpp's own
 * large dependency graph); mocked locally instead, matching this
 * project's established per-TU-mock convention. */
static bool g_activeReturn = true;
bool CSTGPerformance::IsCurrentlyActive() const { return g_activeReturn; }

static int g_pgmCalls, g_combiCalls, g_audioCalls;
extern "C" void CSTGControllerRTData_UpdateJumpCatchWithPgmSendKnobValues(void *) { g_pgmCalls++; }
extern "C" void CSTGControllerRTData_UpdateJumpCatchWithCombiSendKnobValues(void *) { g_combiCalls++; }
extern "C" void CSTGControllerRTData_UpdateJumpCatchWithAudioSendKnobValues(void *) { g_audioCalls++; }
extern "C" void CSTGControllerRTData_UpdateAudioTrackSendJumpCatch(void *, void *, unsigned int, unsigned int) {}
extern "C" void CSTGControllerRTData_UpdateJumpCatchWithIFXSendKnobValues(void *, void *, int) {}

int main(void)
{
	printf("CSTGControllerRTData::ResetSendKnobsJumpCatch known-answer test\n");
	printf("=================================================================\n");

	unsigned char *rtBuf = mmap32(0x40);
	memset(rtBuf, 0, 0x40);
	CSTGControllerRTData *crt = (CSTGControllerRTData *)rtBuf;

	const unsigned int kMode1Offset = 0x1c77f10u + 6u;
	unsigned long globalSize = kMode1Offset + 0x10;
	unsigned char *g = mmap32(globalSize);
	memset(g, 0, globalSize);
	CSTGGlobal::sInstance = (CSTGGlobal *)g;
	*(unsigned int *)(g + 0x684) = 1;	/* mode = 1 -> ResolveCurrentPerformance mode-1 branch */
	*(unsigned int *)(g + 0x69c) = 0;
	*(unsigned int *)(g + 0x690) = 0;
	/* perf object lives at g + kMode1Offset -- 0xad8 bytes must fit
	 * inside `globalSize`, already true since kMode1Offset+0x10 only
	 * covers a few bytes past it; bump so +0xad7 is addressable too. */
	munmap(g, globalSize);
	globalSize = kMode1Offset + 0xb00;
	g = mmap32(globalSize);
	memset(g, 0, globalSize);
	CSTGGlobal::sInstance = (CSTGGlobal *)g;
	*(unsigned int *)(g + 0x684) = 1;
	*(unsigned int *)(g + 0x69c) = 0;
	*(unsigned int *)(g + 0x690) = 0;

	printf("[1] IsCurrentlyActive() == false -> no dispatch at all\n");
	{
		g_activeReturn = false;
		g_pgmCalls = g_combiCalls = g_audioCalls = 0;
		rtBuf[0x2b] = 0;
		crt->ResetSendKnobsJumpCatch();
		check_eq("pgm not called", g_pgmCalls, 0);
		check_eq("combi not called", g_combiCalls, 0);
	}

	g_activeReturn = true;

	printf("[2] perf +0xad7 gate byte set -> no dispatch\n");
	{
		g[kMode1Offset + 0xad7] = 1;
		g_pgmCalls = 0;
		rtBuf[0x2b] = 0;
		crt->ResetSendKnobsJumpCatch();
		check_eq("pgm not called (gate byte set)", g_pgmCalls, 0);
		g[kMode1Offset + 0xad7] = 0;
	}

	printf("[3] case index out of range (8) -> no dispatch\n");
	{
		g_pgmCalls = 0;
		rtBuf[0x2b] = 8;
		crt->ResetSendKnobsJumpCatch();
		check_eq("pgm not called (index 8)", g_pgmCalls, 0);
	}

	printf("[4] case 0, global mode==0 -> UpdateJumpCatchWithPgmSendKnobValues\n");
	{
		*(unsigned int *)(g + 0x684) = 0;
		g_pgmCalls = g_combiCalls = 0;
		rtBuf[0x2b] = 0;
		crt->ResetSendKnobsJumpCatch();
		check_eq("pgm called once", g_pgmCalls, 1);
		check_eq("combi not called", g_combiCalls, 0);
		*(unsigned int *)(g + 0x684) = 1;
	}

	printf("[5] case 1, global mode!=0 -> UpdateJumpCatchWithCombiSendKnobValues\n");
	{
		g_pgmCalls = g_combiCalls = 0;
		rtBuf[0x2b] = 1;
		crt->ResetSendKnobsJumpCatch();
		check_eq("combi called once", g_combiCalls, 1);
		check_eq("pgm not called", g_pgmCalls, 0);
	}

	printf("[6] cases 4,5,6 -> no-op (no siblings called)\n");
	{
		for (int i = 4; i <= 6; i++) {
			g_pgmCalls = g_combiCalls = g_audioCalls = 0;
			rtBuf[0x2b] = (unsigned char)i;
			crt->ResetSendKnobsJumpCatch();
			check_eq("no dispatch", g_pgmCalls + g_combiCalls + g_audioCalls, 0);
		}
	}

	printf("[7] case 7 -> UpdateJumpCatchWithAudioSendKnobValues\n");
	{
		g_audioCalls = 0;
		rtBuf[0x2b] = 7;
		crt->ResetSendKnobsJumpCatch();
		check_eq("audio called once", g_audioCalls, 1);
	}

	printf("\n%s\n", g_fail ? "FAILED" : "All tests passed");
	return g_fail ? 1 : 0;
}
