// SPDX-License-Identifier: GPL-2.0
/*
 * test_sk_stg_gate.cpp  -  KAT for SKSTGGate_ShouldSyncExternalClock()
 * (see ../src/engine/sk_stg_gate.cpp).
 *
 * Verifies: the real function forwards to CTimerManager::ms_poInstance
 * ->ShouldSyncExternalClock() and returns its result verbatim, for both
 * a true and a false result -- and, separately, that the confirmed real
 * "no null check" quirk is real: with ms_poInstance == 0, the call still
 * reaches CTimerManager::ShouldSyncExternalClock() with this == 0
 * (this mock's own body never dereferences `this`, matching the real
 * disassembly's own confirmed unchecked dispatch).
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

static int g_calls;
static const void *g_lastThis;
static bool g_returnValue;
bool CTimerManager::ShouldSyncExternalClock()
{
	g_calls++;
	g_lastThis = this;
	return g_returnValue;
}

int main(void)
{
	printf("SKSTGGate_ShouldSyncExternalClock() known-answer test\n");
	printf("=========================================================\n");

	CTimerManager tm;
	CTimerManager::ms_poInstance = &tm;

	printf("[1] forwards this == ms_poInstance, returns true verbatim\n");
	g_returnValue = true;
	bool r1 = SKSTGGate_ShouldSyncExternalClock();
	check_eq("call count", g_calls, 1);
	check_eq("this == &tm", (long)(g_lastThis == &tm), 1);
	check_eq("return value == true", r1, true);

	printf("\n[2] returns false verbatim\n");
	g_returnValue = false;
	bool r2 = SKSTGGate_ShouldSyncExternalClock();
	check_eq("call count", g_calls, 2);
	check_eq("return value == false", r2, false);

	printf("\n[3] confirmed real quirk: NO null check on ms_poInstance\n");
	CTimerManager::ms_poInstance = 0;
	g_returnValue = true;
	bool r3 = SKSTGGate_ShouldSyncExternalClock();
	check_eq("call count (dispatch still happened with this == 0)", g_calls, 3);
	check_eq("this == 0", (long)(g_lastThis == 0), 1);
	check_eq("return value == true", r3, true);

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
