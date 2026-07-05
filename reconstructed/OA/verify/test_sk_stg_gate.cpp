// SPDX-License-Identifier: GPL-2.0
/*
 * test_sk_stg_gate.cpp  -  KAT for SKSTGGate_ShouldSyncExternalClock()
 * AND the real CTimerManager::ShouldSyncExternalClock() it forwards to
 * (sec 10.151) (see ../src/engine/sk_stg_gate.cpp).
 *
 * Verifies: the real forwarding function passes ms_poInstance as `this`
 * to CTimerManager::ShouldSyncExternalClock() -- but that function's own
 * REAL body (no longer mocked) ignores `this` entirely and reads
 * CKGBankManager::ms_poInstance instead, confirming the real "no null
 * check on ms_poInstance" quirk is genuinely safe: dispatch with
 * `this == 0` never actually touches `this`.
 */

#include <cstdio>
#include "oa_engine_init.h"

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

/* Static storage (NOT a stack array -- the real +0x97c750 offset means
 * this stand-in object needs to be just under 9.9MB). */
static unsigned char bankMgr[0x97c760];

int main(void)
{
	printf("SKSTGGate_ShouldSyncExternalClock() / CTimerManager::ShouldSyncExternalClock() known-answer test\n");
	printf("=========================================================\n");

	for (unsigned int i = 0; i < sizeof(bankMgr); i++)
		bankMgr[i] = 0;
	CKGBankManager::ms_poInstance = bankMgr;

	CTimerManager tm;
	CTimerManager::ms_poInstance = &tm;

	printf("[1] mode 0 -> false\n");
	*(int *)(bankMgr + 0x97c750) = 0;
	check_eq("mode 0", SKSTGGate_ShouldSyncExternalClock(), false);

	printf("\n[2] mode 1 -> true\n");
	*(int *)(bankMgr + 0x97c750) = 1;
	check_eq("mode 1", SKSTGGate_ShouldSyncExternalClock(), true);

	printf("\n[3] mode 3 -> true\n");
	*(int *)(bankMgr + 0x97c750) = 3;
	check_eq("mode 3", SKSTGGate_ShouldSyncExternalClock(), true);

	printf("\n[4] mode 2 -> defers to byte at +8\n");
	*(int *)(bankMgr + 0x97c750) = 2;
	bankMgr[8] = 0;
	check_eq("mode 2, +8==0 -> false", SKSTGGate_ShouldSyncExternalClock(), false);
	bankMgr[8] = 1;
	check_eq("mode 2, +8!=0 -> true", SKSTGGate_ShouldSyncExternalClock(), true);

	printf("\n[5] mode 4 -> ALSO defers to byte at +8 (same as mode 2)\n");
	*(int *)(bankMgr + 0x97c750) = 4;
	bankMgr[8] = 0;
	check_eq("mode 4, +8==0 -> false", SKSTGGate_ShouldSyncExternalClock(), false);
	bankMgr[8] = 1;
	check_eq("mode 4, +8!=0 -> true", SKSTGGate_ShouldSyncExternalClock(), true);

	printf("\n[6] any other mode -> false\n");
	*(int *)(bankMgr + 0x97c750) = 99;
	check_eq("mode 99", SKSTGGate_ShouldSyncExternalClock(), false);

	printf("\n[7] confirmed real quirk: NO null check on ms_poInstance, and it's "
	       "genuinely safe -- `this` is never dereferenced\n");
	CTimerManager::ms_poInstance = 0;
	*(int *)(bankMgr + 0x97c750) = 1;
	bool r7 = SKSTGGate_ShouldSyncExternalClock();
	check_eq("dispatch with this==0 still works (this is never read)", r7, true);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
