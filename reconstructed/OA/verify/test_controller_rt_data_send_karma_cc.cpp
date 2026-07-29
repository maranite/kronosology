// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_rt_data_send_karma_cc.cpp  -  host-side known-answer
 * test for CSTGControllerRTData::SendKarmaCCToKG(int, unsigned char)
 * (round 44, 2026-07-29, solo).
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
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

CSTGGlobal *CSTGGlobal::sInstance;
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;

/* ---- CSTGMidiQueueWriter::Write mock (same idiom as
 * test_controller_info_button_handler.cpp) ---- */
static int g_writeCalls;
static unsigned char g_lastMsg[5];
static unsigned int g_lastLen;
static bool g_lastFlag;
void CSTGMidiQueueWriter::Write(const unsigned char *data, unsigned int length, bool flag)
{
	g_writeCalls++;
	g_lastLen = length;
	g_lastFlag = flag;
	memcpy(g_lastMsg, data, length < sizeof(g_lastMsg) ? length : sizeof(g_lastMsg));
}
void CSTGMidiQueueWriter::Write(unsigned char) {}

int main()
{
	printf("CSTGControllerRTData::SendKarmaCCToKG known-answer test\n");
	printf("=========================================================\n");

	static unsigned char globalBuf[0x6c0];
	memset(globalBuf, 0, sizeof(globalBuf));
	CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;

	static unsigned char portMgrBuf[0x210];
	memset(portMgrBuf, 0, sizeof(portMgrBuf));
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)portMgrBuf;

	/* CSTGControllerRTData's real ctor (controller_rt_data_ctor.cpp) is
	 * deliberately NOT linked into this TU -- SendKarmaCCToKG's own real
	 * body never reads `this` (see header comment), so a raw, unconstructed
	 * buffer cast is sufficient and avoids pulling in that ctor's own
	 * dependency chain, same convention as test [3] below. */
	static unsigned char rtdBuf1[64];

	printf("[1] channel byte 0x03, ccNo 0x2c, value 0x7f\n");
	{
		globalBuf[0x6b8] = 0x03;
		g_writeCalls = 0;
		CSTGControllerRTData *rtd = (CSTGControllerRTData *)rtdBuf1;
		rtd->SendKarmaCCToKG(0x2c, 0x7f);
		check_eq("Write called once", g_writeCalls, 1);
		check_eq("length == 5", g_lastLen, 5);
		check_eq("flag == false", g_lastFlag, 0);
		check_eq("status byte == channel|0xb0", g_lastMsg[0], 0xb3);
		check_eq("byte[1] == ccNo", g_lastMsg[1], 0x2c);
		check_eq("byte[2] == value", g_lastMsg[2], 0x7f);
		check_eq("byte[3] == 0x05", g_lastMsg[3], 0x05);
		check_eq("byte[4] == 0xff", g_lastMsg[4], 0xff);
	}

	printf("[2] channel byte 0x0f (already has high nibble set beyond 4 bits used)\n");
	{
		globalBuf[0x6b8] = 0x0f;
		g_writeCalls = 0;
		CSTGControllerRTData *rtd = (CSTGControllerRTData *)rtdBuf1;
		rtd->SendKarmaCCToKG(0x00, 0x00);
		check_eq("status byte == 0x0f|0xb0", g_lastMsg[0], 0xbf);
		check_eq("byte[1] == 0", g_lastMsg[1], 0);
		check_eq("byte[2] == 0", g_lastMsg[2], 0);
	}

	printf("[3] `this` is never read by the real body (channel comes from CSTGGlobal, not this)\n");
	{
		globalBuf[0x6b8] = 0x05;
		g_writeCalls = 0;
		/* Deliberately construct rtd at a DIFFERENT, otherwise-unused
		 * address to confirm the result doesn't depend on `this`. */
		static unsigned char rtdBuf[64];
		memset(rtdBuf, 0xcd, sizeof(rtdBuf)); /* poison */
		CSTGControllerRTData *rtd = (CSTGControllerRTData *)rtdBuf;
		rtd->SendKarmaCCToKG(0x10, 0x20);
		check_eq("still dispatches correctly despite poisoned `this`", g_writeCalls, 1);
		check_eq("status byte still from CSTGGlobal", g_lastMsg[0], 0xb5);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
