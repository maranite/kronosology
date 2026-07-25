// SPDX-License-Identifier: GPL-2.0
/*
 * test_set_list_eq_init.cpp  -  host-side known-answer test for
 * CSetListEQ::Initialize(unsigned int) (batch 59).
 *
 * The module-static 9-band {omega, beta} coefficient table
 * set_list_eq_init.cpp builds is not independently observable from
 * outside that file (no getter -- its own real consumer, `SetBand()`,
 * remains a deliberately deferred out-of-scope no-op, sec 10.185 DSP
 * policy). This test instead confirms: (1) Initialize() never crashes
 * across repeated calls with different `count` values (exercising both
 * the guarded once-only table build AND the unconditional per-call
 * instance setup on the SAME run), (2) the confirmed real `+0x0`/`+0x4`
 * instance fields are set correctly per call, matching
 * CSTGMasterLRMixer::Initialize()'s own already-verified `+0x14` formula
 * byte-for-byte (batch 58, same `count*120+12` slot).
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"
#include "oa_engine.h"

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

unsigned char CSTGAudioBusManager::sEffectThreadBusSets[240 * 0x80];

int main(void)
{
	printf("CSetListEQ::Initialize known-answer test\n");
	printf("=========================================================\n");

	unsigned char raw[16];
	memset(raw, 0xCC, sizeof(raw));
	CSetListEQ *eq = (CSetListEQ *)raw;

	printf("[1] First call (count=0) -- builds the static coefficient "
	       "table (via the real CSTGEQ::CalculatePeakingBeta, exercised "
	       "for real, not mocked) and sets +0x0/+0x4\n");
	eq->Initialize(0);
	check_eq("+0x0 == count (0)", *(unsigned int *)(raw + 0x0), 0);
	unsigned int want4_0 = (unsigned int)(unsigned long)
		(CSTGAudioBusManager::sEffectThreadBusSets + 12 * 0x80);
	check_eq("+0x4 == &sEffectThreadBusSets[0*120+12]",
		 *(unsigned int *)(raw + 0x4), want4_0);

	printf("[2] Second call (count=1) -- guard skips rebuilding the "
	       "table (no crash/double-init), instance fields still update\n");
	eq->Initialize(1);
	check_eq("+0x0 == count (1)", *(unsigned int *)(raw + 0x0), 1);
	unsigned int want4_1 = (unsigned int)(unsigned long)
		(CSTGAudioBusManager::sEffectThreadBusSets + (120 + 12) * 0x80);
	check_eq("+0x4 == &sEffectThreadBusSets[1*120+12]",
		 *(unsigned int *)(raw + 0x4), want4_1);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
