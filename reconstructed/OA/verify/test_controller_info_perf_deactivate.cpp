// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_info_perf_deactivate.cpp  -  KAT for
 * CSTGControllerRTData::ResetPerfSwitches() and
 * CSTGControllerInfo::OnPerformanceDeactivate() (batch 36, see
 * ../src/engine/controller_info_perf_deactivate.cpp).
 *
 * Mocks CSTGControllerInfo::SetPerfSwitch(int, bool) (a confirmed real,
 * deliberately deferred sibling -- its own real body isn't reconstructed,
 * only called) to record each call's arguments.
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"

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

static int g_setPerfSwitchCalls;
static int g_lastPerfSwitch;
static bool g_lastValue;
void CSTGControllerInfo::SetPerfSwitch(int perfSwitch, bool value)
{
	g_setPerfSwitchCalls++;
	g_lastPerfSwitch = perfSwitch;
	g_lastValue = value;
}

CSTGControllerRTData *CSTGControllerRTData::sInstance;

int main(void)
{
	printf("CSTGControllerRTData::ResetPerfSwitches / "
	       "CSTGControllerInfo::OnPerformanceDeactivate KAT\n");
	printf("=========================================================\n");

	printf("[1] ResetPerfSwitches() -- straight-line field zeroing\n");
	{
		unsigned char buf[64];
		memset(buf, 0xcc, sizeof(buf));
		CSTGControllerRTData *rt = (CSTGControllerRTData *)buf;
		rt->ResetPerfSwitches();

		static const unsigned int zeroedBytes[] = {
			0x14, 0x15, 0x1c, 0x1d, 0x1e, 0x1f, 0x27, 0x28, 0x29, 0x2a
		};
		int allZero = 1;
		for (unsigned int i = 0; i < sizeof(zeroedBytes) / sizeof(zeroedBytes[0]); i++)
			if (buf[zeroedBytes[i]] != 0)
				allZero = 0;
		check_eq("all confirmed-zeroed bytes are 0", allZero, 1);
		check_eq("+0x20 == 0x40", buf[0x20], 0x40);
		check_eq("+0x18 (word) == 0x0200", *(unsigned short *)(buf + 0x18), 0x0200);
		check_eq("+0x1a (word) == 0x0200", *(unsigned short *)(buf + 0x1a), 0x0200);
		/* untouched neighbour bytes still poisoned */
		check_eq("+0x13 untouched (still poisoned)", buf[0x13], 0xcc);
		check_eq("+0x2b untouched (still poisoned)", buf[0x2b], 0xcc);
	}

	printf("\n[2] OnPerformanceDeactivate() -- both flags clear: no "
	       "SetPerfSwitch calls, ResetPerfSwitches + field clears still run\n");
	{
		unsigned char rtBuf[64];
		memset(rtBuf, 0, sizeof(rtBuf));
		rtBuf[0x21] = 0xff;
		rtBuf[0x26] = 0xff;
		*(unsigned short *)(rtBuf + 0x24) = 0xffff;
		*(unsigned short *)(rtBuf + 0x22) = 0xffff;
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)rtBuf;

		unsigned char infoBuf[8];
		CSTGControllerInfo *info = (CSTGControllerInfo *)infoBuf;
		g_setPerfSwitchCalls = 0;

		info->OnPerformanceDeactivate();

		check_eq("SetPerfSwitch NOT called (both flags clear)", g_setPerfSwitchCalls, 0);
		check_eq("+0x14 zeroed by ResetPerfSwitches", rtBuf[0x14], 0);
		check_eq("+0x21 &= 0xfc", rtBuf[0x21], 0xff & 0xfc);
		check_eq("+0x26 == 0", rtBuf[0x26], 0);
		check_eq("+0x24 (word) == 0", *(unsigned short *)(rtBuf + 0x24), 0);
		check_eq("+0x22 (word) == 0", *(unsigned short *)(rtBuf + 0x22), 0);
	}

	printf("\n[3] OnPerformanceDeactivate() -- +0x14 set: SetPerfSwitch(0,false), "
	       "then re-reads sInstance (real compiler reload)\n");
	{
		unsigned char rtBuf[64];
		memset(rtBuf, 0, sizeof(rtBuf));
		rtBuf[0x14] = 1;
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)rtBuf;

		unsigned char infoBuf[8];
		CSTGControllerInfo *info = (CSTGControllerInfo *)infoBuf;
		g_setPerfSwitchCalls = 0;

		info->OnPerformanceDeactivate();

		check_eq("SetPerfSwitch called exactly once", g_setPerfSwitchCalls, 1);
		check_eq("...with perfSwitch == 0", g_lastPerfSwitch, 0);
		check_eq("...with value == false", (long)g_lastValue, 0);
	}

	printf("\n[4] OnPerformanceDeactivate() -- +0x15 set: SetPerfSwitch(1,false)\n");
	{
		unsigned char rtBuf[64];
		memset(rtBuf, 0, sizeof(rtBuf));
		rtBuf[0x15] = 1;
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)rtBuf;

		unsigned char infoBuf[8];
		CSTGControllerInfo *info = (CSTGControllerInfo *)infoBuf;
		g_setPerfSwitchCalls = 0;

		info->OnPerformanceDeactivate();

		check_eq("SetPerfSwitch called exactly once", g_setPerfSwitchCalls, 1);
		check_eq("...with perfSwitch == 1", g_lastPerfSwitch, 1);
		check_eq("...with value == false", (long)g_lastValue, 0);
	}

	printf("\n[5] OnPerformanceDeactivate() -- both +0x14 and +0x15 set: TWO "
	       "SetPerfSwitch calls (0,false) then (1,false)\n");
	{
		unsigned char rtBuf[64];
		memset(rtBuf, 0, sizeof(rtBuf));
		rtBuf[0x14] = 1;
		rtBuf[0x15] = 1;
		CSTGControllerRTData::sInstance = (CSTGControllerRTData *)rtBuf;

		unsigned char infoBuf[8];
		CSTGControllerInfo *info = (CSTGControllerInfo *)infoBuf;
		g_setPerfSwitchCalls = 0;

		info->OnPerformanceDeactivate();

		check_eq("SetPerfSwitch called exactly twice", g_setPerfSwitchCalls, 2);
		check_eq("...last call perfSwitch == 1 (the +0x15 branch runs last)",
			 g_lastPerfSwitch, 1);
	}

	printf("=========================================================\n");
	printf("RESULT: %s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail;
}
