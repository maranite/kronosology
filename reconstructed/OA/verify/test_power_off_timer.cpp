// SPDX-License-Identifier: GPL-2.0
/*
 * test_power_off_timer.cpp  -  host-side KAT for CPowerOffTimer's 7
 * methods reconstructed in src/engine/power_off_timer.cpp (batch
 * 2026-07-25): ReloadTimer/UpdateTimeoutValue/UpdateWarningThreshold/
 * PowerOffPrepComplete/DoTimerTick/BeginLongProcess/EndLongProcess.
 *
 * Deliberately self-contained: links ONLY power_off_timer.cpp. Every
 * dependency class (CSTGAudioBusManager/CSTGMessageProcessor/
 * CSTGGlobal/STGAPIFrontPanelStatus) is only ever touched via raw
 * offset casts by the code under test, so this file points each
 * `sInstance` at a hand-built raw buffer instead of linking any real
 * constructor -- avoids managers.cpp's own large unrelated dependency
 * chain (see test_engine_startup_bits2.cpp's own mock list for what
 * that would otherwise pull in).
 */

#include <cstdio>
#include <cstring>
#include "oa_engine.h"
#include "oa_setup_global_resources.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-60s %ld\n", label, got); return; }
	printf("  FAIL  %-60s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ---- link-satisfying globals/mocks ---- */
CPowerOffTimer *CPowerOffTimer::sInstance;
CSTGAudioBusManager *CSTGAudioBusManager::sInstance;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance;
CSTGGlobal *CSTGGlobal::sInstance;
unsigned char *STGAPIFrontPanelStatus::sInstance;

static int g_pushCalls;
static unsigned int g_lastMsgSubtype;
extern "C" void PushUnsolicitedMessage(void *msg)
{
	g_pushCalls++;
	g_lastMsgSubtype = *(unsigned int *)((unsigned char *)msg + 0x8);
	check_eq("  (msg size tag)", *(unsigned short *)msg, 0x10);
	check_eq("  (msg source tag)", *(unsigned short *)((unsigned char *)msg + 2), 1);
}

static int g_lockCalls, g_unlockCalls;
extern "C" void rtwrap_pthread_mutex_lock(void *) { g_lockCalls++; }
extern "C" void rtwrap_pthread_mutex_unlock(void *) { g_unlockCalls++; }

static int g_writeCommandCalls;
static int g_lastCommand;
extern "C" int OmapNKS4OutputFifo_WriteCommand(int command)
{
	g_writeCommandCalls++;
	g_lastCommand = command;
	return 0;
}

static int g_testModeReturn;
extern "C" int COmapNKS4Driver_GetTestMode(void) { return g_testModeReturn; }

/* ---- fixture ---- */
static unsigned char g_busMgrBuf[16];
static unsigned char g_msgProcBuf[0x60];
static unsigned char g_globalBuf[0x6b0];
static unsigned char g_panelBuf[0x29200];
static unsigned char g_potBuf[28];
static void *g_mutexToken = (void *)0x1234;

static CPowerOffTimer *resetFixture()
{
	memset(g_busMgrBuf, 0, sizeof(g_busMgrBuf));
	*(float *)(g_busMgrBuf + 4) = 1500.0f; /* busGainScale */
	CSTGAudioBusManager::sInstance = (CSTGAudioBusManager *)g_busMgrBuf;

	memset(g_msgProcBuf, 0, sizeof(g_msgProcBuf));
	CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)g_msgProcBuf;

	memset(g_globalBuf, 0, sizeof(g_globalBuf));
	CSTGGlobal::sInstance = (CSTGGlobal *)g_globalBuf;

	memset(g_panelBuf, 0, sizeof(g_panelBuf));
	STGAPIFrontPanelStatus::sInstance = g_panelBuf;

	memset(g_potBuf, 0, sizeof(g_potBuf));
	*(unsigned int *)(g_potBuf + 0x18) = (unsigned int)(unsigned long)g_mutexToken;
	CPowerOffTimer *pot = (CPowerOffTimer *)g_potBuf;
	CPowerOffTimer::sInstance = pot;

	g_pushCalls = 0; g_lastMsgSubtype = 0;
	g_lockCalls = 0; g_unlockCalls = 0;
	g_writeCommandCalls = 0; g_lastCommand = 0;
	g_testModeReturn = 0;
	return pot;
}

static unsigned int F(CPowerOffTimer *pot, int off)
{
	return *(unsigned int *)((unsigned char *)pot + off);
}
static void SetF(CPowerOffTimer *pot, int off, unsigned int v)
{
	*(unsigned int *)((unsigned char *)pot + off) = v;
}

int main()
{
	CPowerOffTimer *pot;

	printf("[1] ReloadTimer() -- plain reload, state stays 1, no message\n");
	pot = resetFixture();
	SetF(pot, 0x8, 500); SetF(pot, 0x4, 1); SetF(pot, 0x14, 1);
	pot->ReloadTimer();
	check_eq("ticksRemaining == ticksTotal", F(pot, 0x4), 500);
	check_eq("no message sent", g_pushCalls, 0);
	check_eq("state unchanged (1)", F(pot, 0x14), 1);

	printf("[2] ReloadTimer() -- state==2 (warning showing) -> cleared + msg 0x29\n");
	pot = resetFixture();
	SetF(pot, 0x8, 700); SetF(pot, 0x14, 2);
	pot->ReloadTimer();
	check_eq("ticksRemaining == ticksTotal", F(pot, 0x4), 700);
	check_eq("state -> 1", F(pot, 0x14), 1);
	check_eq("one message sent", g_pushCalls, 1);
	check_eq("message subtype 0x29", (long)g_lastMsgSubtype, 0x29);

	printf("[3] UpdateTimeoutValue(0) -- disables the timer\n");
	pot = resetFixture();
	pot->UpdateTimeoutValue(0);
	check_eq("ticksTotal == -1", (long)F(pot, 0x8), (long)0xffffffffu);
	check_eq("state == 0", F(pot, 0x14), 0);
	check_eq("warningThreshold == 120*1500", (long)F(pot, 0xc), 120 * 1500);
	check_eq("ticksRemaining == ticksTotal", (long)F(pot, 0x4), (long)0xffffffffu);

	printf("[4] UpdateTimeoutValue(900) -- <=1800s bucket: 120s lead\n");
	pot = resetFixture();
	pot->UpdateTimeoutValue(900);
	check_eq("ticksTotal == 900*1500", (long)F(pot, 0x8), 900 * 1500);
	check_eq("warningThreshold == 120*1500", (long)F(pot, 0xc), 120 * 1500);
	check_eq("state == 1", F(pot, 0x14), 1);
	check_eq("ticksRemaining == ticksTotal", (long)F(pot, 0x4), 900 * 1500);

	printf("[5] UpdateTimeoutValue(2400) -- 1800<x<=3600 bucket: 180s lead\n");
	pot = resetFixture();
	pot->UpdateTimeoutValue(2400);
	check_eq("ticksTotal == 2400*1500", (long)F(pot, 0x8), 2400 * 1500);
	check_eq("warningThreshold == 180*1500", (long)F(pot, 0xc), 180 * 1500);
	check_eq("ticksRemaining == ticksTotal", (long)F(pot, 0x4), 2400 * 1500);

	printf("[6] UpdateTimeoutValue(7200) -- >3600s bucket: 300s lead\n");
	pot = resetFixture();
	pot->UpdateTimeoutValue(7200);
	check_eq("ticksTotal == 7200*1500", (long)F(pot, 0x8), 7200 * 1500);
	check_eq("warningThreshold == 300*1500", (long)F(pot, 0xc), 300 * 1500);

	printf("[7] UpdateWarningThreshold(50)\n");
	pot = resetFixture();
	pot->UpdateWarningThreshold(50);
	check_eq("warningThreshold == 50*1500", (long)F(pot, 0xc), 50 * 1500);

	printf("[8] PowerOffPrepComplete() -- state==3 -> 4, hardware command sent\n");
	pot = resetFixture();
	SetF(pot, 0x14, 3);
	pot->PowerOffPrepComplete();
	check_eq("state -> 4", F(pot, 0x14), 4);
	check_eq("WriteCommand called once", g_writeCommandCalls, 1);
	check_eq("command == 0x9000000", (long)g_lastCommand, 0x9000000);

	printf("[9] PowerOffPrepComplete() -- state!=3 -> no-op\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1);
	pot->PowerOffPrepComplete();
	check_eq("state unchanged", F(pot, 0x14), 1);
	check_eq("WriteCommand NOT called", g_writeCommandCalls, 0);

	printf("[10] DoTimerTick() -- state==0: disabled, no-op\n");
	pot = resetFixture();
	SetF(pot, 0x14, 0); SetF(pot, 0x4, 100);
	pot->DoTimerTick();
	check_eq("ticksRemaining unchanged", F(pot, 0x4), 100);
	check_eq("no message", g_pushCalls, 0);

	printf("[11] DoTimerTick() -- longProcessCount!=0: suppressed, no-op\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x4, 100); SetF(pot, 0x10, 1);
	pot->DoTimerTick();
	check_eq("ticksRemaining unchanged", F(pot, 0x4), 100);

	printf("[12] DoTimerTick() -- activity flag set -> reset path (ticksRemaining=ticksTotal)\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x8, 900); SetF(pot, 0x4, 5);
	((unsigned char *)pot)[0] = 1; /* activity flag */
	pot->DoTimerTick();
	check_eq("ticksRemaining reset to ticksTotal", F(pot, 0x4), 900);
	check_eq("activity flag cleared", ((unsigned char *)pot)[0], 0);
	check_eq("no message (state was 1, not 2)", g_pushCalls, 0);

	printf("[13] DoTimerTick() -- activity flag set while state==2 -> reset + msg 0x29\n");
	pot = resetFixture();
	SetF(pot, 0x14, 2); SetF(pot, 0x8, 900); SetF(pot, 0x4, 5);
	((unsigned char *)pot)[0] = 1;
	pot->DoTimerTick();
	check_eq("ticksRemaining reset to ticksTotal", F(pot, 0x4), 900);
	check_eq("state -> 1", F(pot, 0x14), 1);
	check_eq("message sent", g_pushCalls, 1);
	check_eq("message subtype 0x29", (long)g_lastMsgSubtype, 0x29);

	printf("[14] DoTimerTick() -- front-panel inhibit flag (+0x109c) -> reset path\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x8, 900); SetF(pot, 0x4, 5);
	*(unsigned int *)(g_panelBuf + 0x109c) = 1;
	pot->DoTimerTick();
	check_eq("ticksRemaining reset", F(pot, 0x4), 900);

	printf("[15] DoTimerTick() -- CSTGMessageProcessor +0x48 gate -> reset path\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x8, 900); SetF(pot, 0x4, 5);
	g_msgProcBuf[0x48] = 1;
	pot->DoTimerTick();
	check_eq("ticksRemaining reset", F(pot, 0x4), 900);

	printf("[16] DoTimerTick() -- CSTGGlobal +0x6a8 mode==1 -> reset path\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x8, 900); SetF(pot, 0x4, 5);
	*(unsigned int *)(g_globalBuf + 0x6a8) = 1;
	pot->DoTimerTick();
	check_eq("ticksRemaining reset", F(pot, 0x4), 900);

	printf("[17] DoTimerTick() -- CSTGGlobal +0x6a8 mode==2 -> reset path\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x8, 900); SetF(pot, 0x4, 5);
	*(unsigned int *)(g_globalBuf + 0x6a8) = 2;
	pot->DoTimerTick();
	check_eq("ticksRemaining reset", F(pot, 0x4), 900);

	printf("[18] DoTimerTick() -- CSTGGlobal +0x6a8 mode==3 (NOT gated) -> normal decrement\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x8, 900); SetF(pot, 0x4, 500); SetF(pot, 0xc, 10);
	*(unsigned int *)(g_globalBuf + 0x6a8) = 3;
	pot->DoTimerTick();
	check_eq("ticksRemaining decremented normally", F(pot, 0x4), 499);

	printf("[19] DoTimerTick() -- NKS4 test mode active -> reset path\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x8, 900); SetF(pot, 0x4, 5);
	g_testModeReturn = 1;
	pot->DoTimerTick();
	check_eq("ticksRemaining reset", F(pot, 0x4), 900);

	printf("[20] DoTimerTick() -- plain decrement, well above threshold: no message\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x4, 1000); SetF(pot, 0xc, 10);
	pot->DoTimerTick();
	check_eq("ticksRemaining decremented", F(pot, 0x4), 999);
	check_eq("no message", g_pushCalls, 0);
	check_eq("state unchanged", F(pot, 0x14), 1);

	printf("[21] DoTimerTick() -- decrement hits exactly 0 -> state=3, msg 0x28\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x4, 1); SetF(pot, 0xc, 0);
	pot->DoTimerTick();
	check_eq("ticksRemaining == 0", F(pot, 0x4), 0);
	check_eq("state -> 3", F(pot, 0x14), 3);
	check_eq("message sent", g_pushCalls, 1);
	check_eq("message subtype 0x28", (long)g_lastMsgSubtype, 0x28);

	printf("[22] DoTimerTick() -- ticksRemaining already 0 at entry: idle no-op\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x4, 0);
	pot->DoTimerTick();
	check_eq("stays 0", F(pot, 0x4), 0);
	check_eq("no message", g_pushCalls, 0);
	check_eq("state unchanged (not re-triggered)", F(pot, 0x14), 1);

	printf("[23] DoTimerTick() -- crosses into warning zone (state 1->2), msg 0x27 + display update\n");
	pot = resetFixture();
	SetF(pot, 0x14, 1); SetF(pot, 0x4, 3001); SetF(pot, 0xc, 3000); /* 1500 ticks/sec */
	pot->DoTimerTick();
	check_eq("ticksRemaining decremented", F(pot, 0x4), 3000);
	check_eq("state -> 2", F(pot, 0x14), 2);
	check_eq("message sent", g_pushCalls, 1);
	check_eq("message subtype 0x27", (long)g_lastMsgSubtype, 0x27);
	check_eq("display seconds == 3000/1500 == 2", (long)*(unsigned int *)(g_panelBuf + 0x2911c), 2);

	printf("[24] DoTimerTick() -- already in warning (state==2): no NEW 0x27, display still updates\n");
	pot = resetFixture();
	SetF(pot, 0x14, 2); SetF(pot, 0x4, 1501); SetF(pot, 0xc, 3000);
	pot->DoTimerTick();
	check_eq("ticksRemaining decremented", F(pot, 0x4), 1500);
	check_eq("state stays 2", F(pot, 0x14), 2);
	check_eq("no message (already state 2)", g_pushCalls, 0);
	check_eq("display seconds == 1500/1500 == 1", (long)*(unsigned int *)(g_panelBuf + 0x2911c), 1);

	printf("[25] BeginLongProcess()/EndLongProcess() -- nesting + mutex use\n");
	pot = resetFixture();
	pot->BeginLongProcess();
	pot->BeginLongProcess();
	check_eq("longProcessCount == 2", F(pot, 0x10), 2);
	check_eq("lock called twice", g_lockCalls, 2);
	check_eq("unlock called twice", g_unlockCalls, 2);
	SetF(pot, 0x8, 800); SetF(pot, 0x4, 1); SetF(pot, 0x14, 1);
	pot->EndLongProcess();
	check_eq("longProcessCount == 1 (still nested)", F(pot, 0x10), 1);
	check_eq("ticksRemaining NOT reset yet (still nested)", F(pot, 0x4), 1);
	pot->EndLongProcess();
	check_eq("longProcessCount == 0", F(pot, 0x10), 0);
	check_eq("ticksRemaining reset to ticksTotal on last EndLongProcess", F(pot, 0x4), 800);
	check_eq("lock/unlock called 4 times total", g_lockCalls, 4);

	printf("[26] EndLongProcess() reaching 0 while state==2 -> clears warning, msg 0x29\n");
	pot = resetFixture();
	SetF(pot, 0x10, 1); SetF(pot, 0x8, 600); SetF(pot, 0x14, 2);
	pot->EndLongProcess();
	check_eq("longProcessCount == 0", F(pot, 0x10), 0);
	check_eq("ticksRemaining reset", F(pot, 0x4), 600);
	check_eq("state -> 1", F(pot, 0x14), 1);
	check_eq("message sent", g_pushCalls, 1);
	check_eq("message subtype 0x29", (long)g_lastMsgSubtype, 0x29);

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
