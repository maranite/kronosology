// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_rt_data_set_assignment.cpp  -  host-side known-answer
 * test for CSTGControllerRTData::SetControllerAssignment() (batch 16,
 * sec 10.163), using the real CSTGCCInfo::sCCInfoTable/
 * kControllerCCIdTable data (linked in from cc_info_table.cpp /
 * controller_rt_data_set_assignment.cpp) rather than synthetic tables.
 *
 * Standalone TU (matching test_channel_values.cpp's own precedent): only
 * links controller_rt_data_set_assignment.cpp + cc_info_table.cpp, so it
 * provides its own local storage for CSTGGlobal::sInstance and
 * CSTGPerformanceVarsManager::sInstance, plus its own mocks for
 * CSTGControllerRTData::HandleControllerChange() (still stubbed
 * elsewhere) and CSTGSlotVoiceData::UpdateAllActiveMIDIFilters() (real,
 * but in a DIFFERENT new file, exercised by its own dedicated
 * test_slot_voice_data_midi_filters.cpp instead).
 *
 * Real table entries used (CC11's b0=0x7f=127, confirmed via
 * cc_info_table.cpp's own dump; kControllerCCIdTable[7]==0x0b==11).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
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

/* --- Local storage for statics this TU needs, matching this project's
 * established "standalone test provides its own copy" convention. --- */
CSTGGlobal *CSTGGlobal::sInstance;
unsigned char CSTGPerformanceVarsManager::sInstance[12];

/* --- Mocks --- */
static int g_hccCalls;
static int g_lastAssign;
static unsigned int g_lastValue;
static bool g_lastFlag1, g_lastFlag2;
void CSTGControllerRTData::HandleControllerChange(int assign, unsigned char value, bool flag1, bool flag2)
{
	g_hccCalls++;
	g_lastAssign = assign;
	g_lastValue = value;
	g_lastFlag1 = flag1;
	g_lastFlag2 = flag2;
}

static int g_uaamfCalls;
void CSTGSlotVoiceData::UpdateAllActiveMIDIFilters() { g_uaamfCalls++; }

int main(void)
{
	printf("CSTGControllerRTData::SetControllerAssignment known-answer test\n");
	printf("=================================================================\n");

	unsigned char *thisBuf = (unsigned char *)mmap32(0x1000);
	memset(thisBuf, 0xcc, 0x1000);
	CSTGControllerRTData *crt = (CSTGControllerRTData *)thisBuf;

	unsigned char *globalBuf = (unsigned char *)mmap32(0x2000);
	memset(globalBuf, 0, 0x2000);
	CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;

	/* mgr buffer big enough for channel 0 and 1, ccId up to 0x5d. */
	unsigned char *mgrBuf = (unsigned char *)mmap32(0x92c * 2 + 0x3000);
	memset(mgrBuf, 0, 0x92c * 2 + 0x3000);
	CSTGPerformanceVarsManager::sInstance[8] = 0;	/* curSlot = 0 -> slots[0] */
	*(unsigned int *)(CSTGPerformanceVarsManager::sInstance + 0) = (unsigned int)(unsigned long)mgrBuf;

	signed char target;

	printf("[1] Branch A (CC range): curVal=8 -> ccId=kControllerCCIdTable[7]=0x0b(CC11), b0=0x7f\n");
	{
		target = 8;
		globalBuf[0x6b9] = 0;	/* channel 0 */
		unsigned char *chanVals = mgrBuf + 0 * 0x92c + 0x2410;
		chanVals[11 * 12 + 8] = 0x00;	/* live value != entryB0(0x7f) -> notify */
		g_hccCalls = 0;
		g_uaamfCalls = 0;
		thisBuf[0xf] = 0xcc;
		crt->SetControllerAssignment(&target, 0x22, true);
		check_eq("HandleControllerChange called once", (unsigned int)g_hccCalls, 1);
		check_eq("assign == curVal(8)", (unsigned int)g_lastAssign, 8u);
		check_eq("value == entryB0(0x7f)", g_lastValue, 0x7fu);
		check_eq("flag1 == false", (unsigned int)g_lastFlag1, 0u);
		check_eq("flag2 == notify(true)", (unsigned int)g_lastFlag2, 1u);
		check_eq("*target == newValue(0x22)", (unsigned int)(unsigned char)target, 0x22u);
		check_eq("UpdateAllActiveMIDIFilters called once", (unsigned int)g_uaamfCalls, 1);
		check_eq("this+0xf untouched (newValue!=0xe)", thisBuf[0xf], 0xccu);
	}

	printf("[2] Branch A: live value == entryB0 -> no notify\n");
	{
		target = 8;
		unsigned char *chanVals = mgrBuf + 0 * 0x92c + 0x2410;
		chanVals[11 * 12 + 8] = 0x7f;	/* live value == entryB0 */
		g_hccCalls = 0;
		g_uaamfCalls = 0;
		crt->SetControllerAssignment(&target, 0x01, false);
		check_eq("no HandleControllerChange call", (unsigned int)g_hccCalls, 0);
		check_eq("*target == newValue(0x01)", (unsigned int)(unsigned char)target, 0x01u);
		check_eq("UpdateAllActiveMIDIFilters still called (unconditional)", (unsigned int)g_uaamfCalls, 1);
	}

	printf("[3] Branch A: live value == 0xff -> no notify\n");
	{
		target = 8;
		unsigned char *chanVals = mgrBuf + 0 * 0x92c + 0x2410;
		chanVals[11 * 12 + 8] = 0xff;
		g_hccCalls = 0;
		crt->SetControllerAssignment(&target, 0x02, false);
		check_eq("no HandleControllerChange call", (unsigned int)g_hccCalls, 0);
	}

	printf("[4] Branch A with channel=1 (confirms channel*0x92c offset)\n");
	{
		target = 8;
		globalBuf[0x6b9] = 1;
		unsigned char *chanVals = mgrBuf + 1 * 0x92c + 0x2410;
		chanVals[11 * 12 + 8] = 0x00;
		g_hccCalls = 0;
		crt->SetControllerAssignment(&target, 0x03, false);
		check_eq("HandleControllerChange called once", (unsigned int)g_hccCalls, 1);
		check_eq("value == entryB0(0x7f)", g_lastValue, 0x7fu);
		globalBuf[0x6b9] = 0;	/* restore for subsequent tests */
	}

	printf("[5] Branch B: curVal in [0x2e,0x3d] -> ALWAYS notifies with candidate 0\n");
	{
		target = 0x30;
		g_hccCalls = 0;
		crt->SetControllerAssignment(&target, 0x04, true);
		check_eq("HandleControllerChange called once", (unsigned int)g_hccCalls, 1);
		check_eq("assign == curVal(0x30)", (unsigned int)g_lastAssign, 0x30u);
		check_eq("value == 0 (fixed candidate)", g_lastValue, 0u);
	}

	printf("[6] Branch C, curVal==0x13: computed(127.0*1.0=127) == table(127) -> no notify\n");
	{
		target = 0x13;
		*(float *)(thisBuf + 0x4c) = 1.0f;
		g_hccCalls = 0;
		crt->SetControllerAssignment(&target, 0x05, false);
		check_eq("no HandleControllerChange call", (unsigned int)g_hccCalls, 0);
	}

	printf("[7] Branch C, curVal==0x13: computed(127.0*0.5=63.5, rounds to 63 or 64) != table(127) -> notify with 127\n");
	{
		target = 0x13;
		*(float *)(thisBuf + 0x4c) = 0.5f;
		g_hccCalls = 0;
		crt->SetControllerAssignment(&target, 0x06, true);
		check_eq("HandleControllerChange called once", (unsigned int)g_hccCalls, 1);
		check_eq("value == table's own 127 (NOT the freshly computed value)", g_lastValue, 127u);
	}

	printf("[8] Branch C, curVal==0x14 (table[1]==0): ALWAYS notifies (curVal!=0x13, tableVal!=-1)\n");
	{
		target = 0x14;
		g_hccCalls = 0;
		crt->SetControllerAssignment(&target, 0x07, false);
		check_eq("HandleControllerChange called once", (unsigned int)g_hccCalls, 1);
		check_eq("value == 0 (table[1])", g_lastValue, 0u);
	}

	printf("[9] Branch C, curVal==0x1c (table[9]==-1): no notify at all\n");
	{
		target = 0x1c;
		g_hccCalls = 0;
		crt->SetControllerAssignment(&target, 0x08, false);
		check_eq("no HandleControllerChange call", (unsigned int)g_hccCalls, 0);
	}

	printf("[10] curVal outside all three ranges (0x50): no notify\n");
	{
		target = 0x50;
		g_hccCalls = 0;
		crt->SetControllerAssignment(&target, 0x09, false);
		check_eq("no HandleControllerChange call", (unsigned int)g_hccCalls, 0);
	}

	printf("[11] newValue==0xe clears this+0xf\n");
	{
		target = 0x50;	/* no-notify range, irrelevant to this check */
		thisBuf[0xf] = 0xcc;
		crt->SetControllerAssignment(&target, 0x0e, false);
		check_eq("this+0xf cleared to 0", thisBuf[0xf], 0u);
		check_eq("*target == newValue(0x0e)", (unsigned int)(unsigned char)target, 0x0eu);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return g_fail ? 1 : 0;
}
