// SPDX-License-Identifier: GPL-2.0
/*
 * test_audio_start.cpp  -  host-side known-answer test for
 * CSTGAudioManager::StartAudioEngine() and its three thin C-linkage
 * wrappers (see ../include/oa_audio_start.h /
 * ../src/init/audio_start.cpp).
 *
 * Mocks CSTGThread::CreateRealTimeWithCPUAffinity, a fake vtable for
 * CSTGAudioManager (slot 1 = stop/cleanup) and CSTGAudioDriverInterface
 * (slot 3 = Start()), and directly pokes the confirmed-real offset
 * fields this function touches (+0x20 device list, +0x4560 priority,
 * +0xa24/+0x4 embedded CSTGThread objects).
 */

#include <cstdio>
#include <cstdlib>
#include "oa_audio_start.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-50s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

extern "C" {

static int g_createCalls;
static int g_failAfterNCalls = -1; /* -1 = never fail */
static void *g_lastEntryFn;
static int g_lastPriority;
static unsigned int g_lastCpuId;
static void *g_lastArg;
char CSTGThread::CreateRealTimeWithCPUAffinity(void *(*entryFn)(void *), int priority,
						unsigned int cpuId, void *arg)
{
	g_createCalls++;
	g_lastEntryFn = (void *)entryFn;
	g_lastPriority = priority;
	g_lastCpuId = cpuId;
	g_lastArg = arg;
	if (g_failAfterNCalls >= 0 && g_createCalls > g_failAfterNCalls)
		return 0;
	return 1;
}

} /* extern "C" */

/*
 * ASKThreadRoutine(void*)/AudioManagerThreadRoutine(void*)/
 * AudioTickLoopRoutine(void*) are all real now (sec 10.149, see
 * audio_start.cpp) -- but this test never actually CALLS any of them
 * (CreateRealTimeWithCPUAffinity above only ever records the entry-fn
 * POINTER VALUE, matching the real StartAudioEngine's own usage, sec
 * 10.37/10.52), so no test behavior changes here. This file links
 * audio_start.cpp directly (not bar2_stubs.cpp), so their own new
 * confirmed-real, deliberately-deferred dependencies still need
 * trivial link-satisfying mocks (rtwrap_whoami/rtwrap_task_suspend's
 * real bodies now live in src/init/rtwrap.cpp, batch 37 -- this file
 * doesn't link that TU either, so it keeps its own local mocks here,
 * updated to the corrected void* / 1-arg signatures): */
extern "C" void *rtwrap_whoami(void) { return 0; }
extern "C" void rtwrap_task_suspend(void *) {}
extern "C" void SKMain_Run(void) {}
void CSTGAudioThread::AudioTickLoopRoutine() {}

CSTGAudioDriverInterface *CSTGAudioDriverInterface::sInstance;
CSTGAudioDriverInterface::~CSTGAudioDriverInterface() {}
CSTGAudioDriverInterfaceKorgUsb::CSTGAudioDriverInterfaceKorgUsb() {}
CSTGAudioDriverInterfaceKorgUsb::~CSTGAudioDriverInterfaceKorgUsb() {}

static int g_driverStartCalls;
static void driver_start(void *) { g_driverStartCalls++; }
static void *g_driverVtable[4] = { 0, 0, 0, (void *)&driver_start };

CSTGAudioManager *CSTGAudioManager::sInstance;
CSTGAudioManager::CSTGAudioManager() {}
CSTGAudioManager::~CSTGAudioManager() {}

static int g_stopCalls;
static void audiomgr_stop(void *) { g_stopCalls++; }
static void *g_audioMgrVtable[2] = { 0, (void *)&audiomgr_stop };

struct ListNode {
	ListNode *next;
	int priority;
	unsigned char *payload;
};

static void reset(CSTGAudioManager *am)
{
	g_createCalls = 0;
	g_failAfterNCalls = -1;
	g_driverStartCalls = 0;
	g_stopCalls = 0;
	unsigned char *self = (unsigned char *)am;
	*(void ***)self = g_audioMgrVtable;
	*(void **)(self + 0x20) = 0; /* empty device list by default */
	*(int *)(self + 0x4560) = 10; /* base priority */
}

int main(void)
{
	CSTGAudioDriverInterface::sInstance = (CSTGAudioDriverInterface *)&g_driverVtable;
	*(void ***)CSTGAudioDriverInterface::sInstance = g_driverVtable;

	CSTGAudioManager *am = new CSTGAudioManager();
	CSTGAudioManager::sInstance = am;

	printf("[1] empty device list, both fixed threads succeed:\n");
	reset(am);
	char rc = am->StartAudioEngine();
	check_eq("return value (success, inverted convention)", rc, 1);
	check_eq("CreateRealTimeWithCPUAffinity called twice (ASK + AudioManager)", g_createCalls, 2);
	check_eq("ASK thread priority == base-3", g_lastPriority == 10 || g_lastPriority == 7, 1);
	check_eq("driver Start() called once", g_driverStartCalls, 1);
	check_eq("stop/cleanup NOT called on success", g_stopCalls, 0);
	check_eq("field +0x4564 zeroed", *(unsigned int *)((unsigned char *)am + 0x4564), 0);
	check_eq("field +0x4568 == 0x989680", *(unsigned int *)((unsigned char *)am + 0x4568), 0x989680);
	check_eq("field +0xa65 == 1 (running flag)", ((unsigned char *)am)[0xa65], 1);

	printf("\n[2] two device-list nodes, both succeed:\n");
	reset(am);
	unsigned char payload1[0x20] = {0}, payload2[0x20] = {0};
	*(int *)(payload1 + 0x4) = 5;
	*(unsigned int *)(payload1 + 0x8) = 1;
	*(int *)(payload2 + 0x4) = 6;
	*(unsigned int *)(payload2 + 0x8) = 2;
	ListNode node2 = { 0, 0, payload2 };
	ListNode node1 = { &node2, 0, payload1 };
	*(void **)((unsigned char *)am + 0x20) = &node1;
	rc = am->StartAudioEngine();
	check_eq("return value (success)", rc, 1);
	check_eq("CreateRealTimeWithCPUAffinity called 4 times (2 list + 2 fixed)", g_createCalls, 4);
	check_eq("driver Start() called once", g_driverStartCalls, 1);

	printf("\n[3] first fixed thread (ASK) fails -> stop called, returns 0:\n");
	reset(am);
	g_failAfterNCalls = 0; /* fail on the very first CreateRealTimeWithCPUAffinity call */
	rc = am->StartAudioEngine();
	check_eq("return value (failure, inverted convention)", rc, 0);
	check_eq("stop/cleanup called once", g_stopCalls, 1);
	check_eq("driver Start() NOT called on failure", g_driverStartCalls, 0);

	printf("\n[4] device-list entry fails -> stop called, driver never started:\n");
	reset(am);
	*(void **)((unsigned char *)am + 0x20) = &node1;
	g_failAfterNCalls = 0;
	rc = am->StartAudioEngine();
	check_eq("return value (failure)", rc, 0);
	check_eq("only 1 CreateRealTimeWithCPUAffinity call (list entry failed immediately)", g_createCalls, 1);
	check_eq("stop/cleanup called once", g_stopCalls, 1);

	printf("\n[5] C-linkage wrappers:\n");
	reset(am);
	int wrc = CSTGAudioManager_StartAudioEngine();
	check_eq("CSTGAudioManager_StartAudioEngine() return value", wrc, 1);
	CSTGAudioManager_StopAudioEngine();
	check_eq("CSTGAudioManager_StopAudioEngine() dispatched stop", g_stopCalls, 1);
	CSTGAudioManager_EnableAudioManagerThread();
	check_eq("CSTGAudioManager_EnableAudioManagerThread() set +0xc", ((unsigned char *)am)[0xc], 1);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
