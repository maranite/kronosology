/*
 * test_sysapi_instance_round47.cpp  -  host-side known-answer test for
 * CSysApiInstance's round-47 trivial accessor batch (solo, 2026-07-29). See
 * include/sysapi_instance.h for the full derivation.
 *
 * Drives the REAL, live `SysApiInstance` global directly, same convention as
 * test_sysapi_instance_register_api.cpp -- its own real
 * __attribute__((constructor)) already ran before main() in every verify
 * binary, leaving mDrivers (own count/array at absolute +0x28/+0x30) genuinely
 * empty at test start.
 */

#include <cstdio>
#include <cstring>

#include "sysapi_instance.h"
#include "config_manager.h"
#include "module_manager.h"
#include "scheduler.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	CSysApiInstance *sysApi = (CSysApiInstance *)SysApiInstance;

	/* [1] version getters -- CConfigManager::sm_pktVersionInfo is a real
	 * static currently left as a null-terminated placeholder blob; point it
	 * at a known 0x18-byte buffer for this test. */
	unsigned char versionInfo[0x18];
	for (int i = 0; i < 6; i++)
		*(unsigned int *)(versionInfo + i * 4) = 0x1000 + i;
	void *savedVersionInfo = CConfigManager::sm_pktVersionInfo;
	CConfigManager::sm_pktVersionInfo = versionInfo;

	check("GetVersionMajor", CSysApiInstance::GetVersionMajor() == 0x1000);
	check("GetVersionMinor", CSysApiInstance::GetVersionMinor() == 0x1001);
	check("GetVersionBuild", CSysApiInstance::GetVersionBuild() == 0x1002);
	check("GetVersionInfo", CSysApiInstance::GetVersionInfo() == 0x1003);
	check("GetVersionDate", CSysApiInstance::GetVersionDate() == 0x1004);
	check("GetVersionTime", CSysApiInstance::GetVersionTime() == 0x1005);

	CConfigManager::sm_pktVersionInfo = savedVersionInfo;

	/* [2] GetDriversCount/GetDriver: real mDrivers array at absolute +0x28
	 * (count) / +0x30 (array), still genuinely empty from the real static
	 * ctor's own placement-construction. */
	check("GetDriversCount() == 0 (fresh, unpopulated)", sysApi->GetDriversCount() == 0);
	check("GetDriver(0) on empty array returns null", sysApi->GetDriver(0) == 0);

	unsigned char *self = (unsigned char *)sysApi;
	void *fakeDriver0 = (void *)0x1234;
	void *fakeDriver1 = (void *)0x5678;
	void *fakeDriverArray[2] = {fakeDriver0, fakeDriver1};
	*(unsigned int *)(self + 0x28) = 2;
	*(void ***)(self + 0x30) = fakeDriverArray;

	check("GetDriversCount() == 2 (populated)", sysApi->GetDriversCount() == 2);
	check("GetDriver(0) == fakeDriver0", sysApi->GetDriver(0) == fakeDriver0);
	check("GetDriver(1) == fakeDriver1", sysApi->GetDriver(1) == fakeDriver1);
	check("GetDriver(2) out of range returns null", sysApi->GetDriver(2) == 0);

	/* restore empty state */
	*(unsigned int *)(self + 0x28) = 0;
	*(void ***)(self + 0x30) = 0;

	/* [3] IsExitRequested/SetExitRequested */
	check("IsExitRequested() initially false", CSysApiInstance::IsExitRequested() == 0);
	CSysApiInstance::SetExitRequested();
	check("IsExitRequested() true after Set", CSysApiInstance::IsExitRequested() != 0);

	/* [4] ViewerTaskRunning/HostInterfaceBusy: return the flag's ADDRESS */
	unsigned int *viewerFlag = CSysApiInstance::ViewerTaskRunning();
	unsigned int *hostBusyFlag = CSysApiInstance::HostInterfaceBusy();
	check("ViewerTaskRunning() returns a real, distinct address", viewerFlag != 0 && viewerFlag != hostBusyFlag);
	*viewerFlag = 7;
	check("ViewerTaskRunning() address is writable and stable across calls",
	      *CSysApiInstance::ViewerTaskRunning() == 7);

	/* [5] GetDmyMsgInput: own new static, defaults to null */
	check("GetDmyMsgInput() defaults to null", CSysApiInstance::GetDmyMsgInput() == 0);

	/* [6] GetTimeStamp: increments a function-local counter, void return --
	 * just confirm it's callable repeatedly without crashing. */
	CSysApiInstance::GetTimeStamp();
	CSysApiInstance::GetTimeStamp();
	CSysApiInstance::GetTimeStamp();
	check("GetTimeStamp: callable repeatedly, reached here", true);

	/* [7] ResetIndexes: zeroes g_poModuleManager's/g_poScheduler's own first
	 * field. Neither singleton is constructed in this test binary (both are
	 * only ever built inside CKernel::CKernel(), not run automatically like
	 * SysApiInstance's own __attribute__((constructor))) -- point them at
	 * throwaway buffers for the duration of this one check. */
	unsigned char fakeModuleManager[8] = {0xcc, 0xcc, 0xcc, 0xcc, 0, 0, 0, 0};
	unsigned char fakeScheduler[8] = {0xcc, 0xcc, 0xcc, 0xcc, 0, 0, 0, 0};
	void *savedModuleManager = g_poModuleManager;
	CScheduler *savedScheduler = g_poScheduler;
	g_poModuleManager = fakeModuleManager;
	g_poScheduler = (CScheduler *)fakeScheduler;

	CSysApiInstance::ResetIndexes();
	check("ResetIndexes: zeroes g_poModuleManager's own first field",
	      *(unsigned int *)fakeModuleManager == 0);
	check("ResetIndexes: zeroes g_poScheduler's own first field",
	      *(unsigned int *)fakeScheduler == 0);

	g_poModuleManager = savedModuleManager;
	g_poScheduler = savedScheduler;

	/* [8] GetTime: calls HAL_GetSystemTime() and discards the result --
	 * confirm it's callable without crashing. */
	CSysApiInstance::GetTime();
	check("GetTime: callable, reached here", true);

	/* [9] GetUniqueID: monotonically increasing counter */
	int id1 = CSysApiInstance::GetUniqueID();
	int id2 = CSysApiInstance::GetUniqueID();
	check("GetUniqueID: monotonically increasing", id2 == id1 + 1);

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
