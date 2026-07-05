// SPDX-License-Identifier: GPL-2.0
/*
 * test_audio_bus_manager.cpp  -  host-side known-answer test for
 * CSTGAudioBusManager::LRBusIndivMirror() (sec 10.153, see
 * src/engine/audio_bus_manager.cpp).
 */

#include <cstdio>
#include <cstring>
#include "oa_engine.h"
#include "oa_global.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got != want) {
		printf("  FAILED: %s (got %lu, want %lu)\n", label, got, want);
		g_fail++;
	} else {
		printf("  ok: %s\n", label);
	}
}

unsigned char CSTGPerformanceVarsManager::sInstance[12];

/* Mock ctor -- the real CSTGAudioBusManager::CSTGAudioBusManager() lives
 * in managers.cpp (deliberately not linked here, see this file's own
 * header comment); this test only exercises LRBusIndivMirror() and
 * fully re-initializes the object's fields itself right after
 * construction. */
CSTGAudioBusManager *CSTGAudioBusManager::sInstance;
CSTGAudioBusManager::CSTGAudioBusManager() {}

int main(void)
{
	printf("CSTGAudioBusManager::LRBusIndivMirror() known-answer test\n");
	printf("===========================================================\n");

	CSTGAudioBusManager abm;
	memset(&abm, 0, sizeof(abm));
	CSTGAudioBusManager::sInstance = &abm;
	memset(CSTGAudioBusManager::sGlobalBusSet, 0xcc, sizeof(CSTGAudioBusManager::sGlobalBusSet));
	memset(CSTGAudioBusManager::sEffectThreadBusSets, 0xcc, sizeof(CSTGAudioBusManager::sEffectThreadBusSets));

	/* Stamp a recognizable pattern into the FIXED source slot (slot 12
	 * of half 0) so we can confirm it's really the source used. */
	unsigned char *srcHalf0 = CSTGAudioBusManager::sEffectThreadBusSets + 12 * 0x80;
	for (int i = 0; i < 0x100; i++)
		srcHalf0[i] = (unsigned char)(0x40 + i);

	printf("\n[1] curBufIdx=0, physBusIdTableHead=5 (<=33) -> sGlobalBusSet[5]\n");
	CSTGPerformanceVarsManager::sInstance[9] = 0;
	abm.physBusIdTableHead = 5;
	abm.LRBusIndivMirror();
	unsigned char *dst1 = CSTGAudioBusManager::sGlobalBusSet + 5 * 0x80;
	check_eq("sGlobalBusSet[5][0] == 0x40", dst1[0], 0x40);
	check_eq("sGlobalBusSet[5][0xff] == 0x3f (wrapped byte)", dst1[0xff], (unsigned char)(0x40 + 0xff));
	check_eq("sGlobalBusSet[4] untouched (still 0xcc)",
		 (CSTGAudioBusManager::sGlobalBusSet + 4 * 0x80)[0], 0xcc);

	printf("\n[2] curBufIdx=0, physBusIdTableHead=33 (boundary, still <=33)\n");
	memset(CSTGAudioBusManager::sGlobalBusSet, 0xcc, sizeof(CSTGAudioBusManager::sGlobalBusSet));
	abm.physBusIdTableHead = 33;
	abm.LRBusIndivMirror();
	unsigned char *dst2 = CSTGAudioBusManager::sGlobalBusSet + 33 * 0x80;
	check_eq("sGlobalBusSet[33][0] == 0x40", dst2[0], 0x40);

	printf("\n[3] curBufIdx=0, physBusIdTableHead=34 (boundary, now effect-thread path)\n");
	memset(CSTGAudioBusManager::sEffectThreadBusSets, 0xcc, sizeof(CSTGAudioBusManager::sEffectThreadBusSets));
	for (int i = 0; i < 0x100; i++)
		srcHalf0[i] = (unsigned char)(0x40 + i);
	abm.physBusIdTableHead = 34;
	abm.LRBusIndivMirror();
	unsigned char *dst3 = CSTGAudioBusManager::sEffectThreadBusSets + 0 * 0x80; /* slot (34-34)=0 of half 0 */
	check_eq("effectThread half0 slot0 [0] == 0x40", dst3[0], 0x40);

	printf("\n[4] curBufIdx=1: source moves to half 1's slot 12, dest to half 1's slot range\n");
	memset(CSTGAudioBusManager::sEffectThreadBusSets, 0xcc, sizeof(CSTGAudioBusManager::sEffectThreadBusSets));
	unsigned char *srcHalf1 = CSTGAudioBusManager::sEffectThreadBusSets + (120 + 12) * 0x80;
	for (int i = 0; i < 0x100; i++)
		srcHalf1[i] = (unsigned char)(0x80 + i);
	CSTGPerformanceVarsManager::sInstance[9] = 1;
	abm.physBusIdTableHead = 34; /* slot (34-34)=0 within half 1 -> absolute slot 120 */
	abm.LRBusIndivMirror();
	unsigned char *dst4 = CSTGAudioBusManager::sEffectThreadBusSets + 120 * 0x80;
	check_eq("effectThread half1 slot0 [0] == 0x80 (copied from half1's own source slot)", dst4[0], 0x80);
	check_eq("effectThread half0 slot0 untouched (still 0xcc)",
		 (CSTGAudioBusManager::sEffectThreadBusSets + 0 * 0x80)[0], 0xcc);

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
