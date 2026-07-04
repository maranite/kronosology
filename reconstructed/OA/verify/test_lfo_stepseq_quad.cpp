// SPDX-License-Identifier: GPL-2.0
/*
 * test_lfo_stepseq_quad.cpp  -  KAT for CSTGLFOBase::InitializeQuad()/
 * CSTGStepSeqBase::InitializeQuad() (see src/engine/lfo_stepseq_quad.cpp).
 *
 * Uses MAP_32BIT for the three singleton "target" buffers (CSTGGlobal/
 * CSTGLFOTables/CSTGMIDIClockSync) since these functions truncate their
 * addresses to `unsigned int`, matching the real 32-bit target -- the
 * same host/target pointer-width hazard hit repeatedly elsewhere in
 * this project.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"

/* Defined here directly (not by linking global.cpp, which pulls in
 * far more than this test needs) -- this test only needs the pointer
 * VALUE, never calls any of CSTGGlobal's own methods. */
CSTGGlobal *CSTGGlobal::sInstance;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-55s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

static unsigned char *map32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

int main(void)
{
	printf("CSTGLFOBase/CSTGStepSeqBase::InitializeQuad known-answer test\n");
	printf("=========================================================\n");

	unsigned long bigBuf = 0x2d00000; /* > 0x29c9fa0 + a little slack */
	unsigned char *globalBuf = map32(bigBuf);
	unsigned char *lfoTablesBuf = map32(0x2000);
	unsigned char *midiClockSyncBuf = map32(0x2000);

	CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;
	CSTGLFOTables::sInstance = (CSTGLFOTables *)lfoTablesBuf;
	CSTGMIDIClockSync::sInstance = (CSTGMIDIClockSync *)midiClockSyncBuf;

	unsigned int ctrlRTData = ToU32(globalBuf + 0x29c9fa0);
	unsigned int lfoTables = ToU32(lfoTablesBuf + 0x408);
	unsigned int midiClockSync = ToU32(midiClockSyncBuf + 0xa0);

	printf("[1] CSTGLFOBase::InitializeQuad()\n");
	unsigned char quadBuf[0x250];
	memset(quadBuf, 0xcc, sizeof(quadBuf));
	CSTGLFOBase::InitializeQuad((STGLFOSubRateParams *)quadBuf);

	int allEdxOk = 1;
	static const unsigned int kBaseOffsets[4] = { 0x10, 0x14, 0x18, 0x1c };
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 5; i++) {
			unsigned int off = kBaseOffsets[j] + i * 0x20;
			if (*(unsigned int *)(quadBuf + off) != ctrlRTData)
				allEdxOk = 0;
		}
	}
	check_eq("all 20 ctrlRTData slots correct", allEdxOk, 1);

	int allZeroOk = 1, allMidiOk = 1, allLfoOk = 1;
	for (int j = 0; j < 4; j++) {
		if (*(unsigned int *)(quadBuf + 0x240 + j * 4) != 0) allZeroOk = 0;
		if (*(unsigned int *)(quadBuf + 0x160 + j * 4) != midiClockSync) allMidiOk = 0;
		if (*(unsigned int *)(quadBuf + 0x190 + j * 4) != lfoTables) allLfoOk = 0;
	}
	check_eq("+0x240/244/248/24c all zeroed", allZeroOk, 1);
	check_eq("+0x160/164/168/16c all == midiClockSync", allMidiOk, 1);
	check_eq("+0x190/194/198/19c all == lfoTables", allLfoOk, 1);

	printf("\n[2] CSTGStepSeqBase::InitializeQuad()\n");
	unsigned char stepBuf[0x100];
	memset(stepBuf, 0xcc, sizeof(stepBuf));
	CSTGStepSeqBase::InitializeQuad((STGStepSeqSubRateParams *)stepBuf);

	int stepOk = 1;
	for (int i = 0; i < 4; i++) {
		if (*(unsigned int *)(stepBuf + 0x40 + i * 4) != 0) stepOk = 0;
		if (*(unsigned int *)(stepBuf + 0x60 + i * 4) != ctrlRTData) stepOk = 0;
		if (*(unsigned int *)(stepBuf + 0x80 + i * 4) != ctrlRTData) stepOk = 0;
		if (*(unsigned int *)(stepBuf + 0xb0 + i * 4) != ctrlRTData) stepOk = 0;
		if (*(unsigned int *)(stepBuf + 0xc0 + i * 4) != 0) stepOk = 0;
		if (*(unsigned int *)(stepBuf + 0xf0 + i * 4) != 0) stepOk = 0;
	}
	check_eq("all 4 iterations' fields correct", stepOk, 1);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
