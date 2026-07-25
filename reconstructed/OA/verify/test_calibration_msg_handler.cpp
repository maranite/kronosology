// SPDX-License-Identifier: GPL-2.0
/*
 * test_calibration_msg_handler.cpp  -  host-side known-answer test for
 * CSTGCalibrationMsgHandler (src/init/calibration_msg_handler.cpp).
 *
 * Links src/init/calibration_msg_handler.cpp directly. Provides its own
 * local mocks/storage for PushMessage/PushUnsolicitedMessage,
 * CSTGKeybedInterface_Start/End/CancelCalibration/
 * ApplyCalibrationAndAfterTouchTable, GetSTGTickCount,
 * CSTGFrontPanel::HandleAnalogController, and singleton storage for
 * CSTGGlobal/CSTGFrontPanel/STGAPIFrontPanelStatus/
 * CSTGPerformanceVarsManager -- same isolated-TU convention as
 * test_front_panel_key_handlers.cpp.
 *
 * CSTGGlobal::sInstance points at a real (not offset-fabricated) 44MB+
 * buffer since this cluster reads THREE distant CSTGGlobal offsets
 * (0x6ac, 0x29c9fa8 via GetSTGTickCount(), 0x29c9fbc) that can't all be
 * reconciled with test_tick_count.cpp's single-offset pointer-arithmetic
 * trick.
 */

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <sys/mman.h>

#include "oa_calibration.h"
#include "oa_setup_global_resources.h"

/* CSTGPerformanceVarsManager::sInstance packs a real 32-bit pointer
 * slot; a plain host global can live above 4GB under ASLR/PIE, which
 * would silently truncate -- allocate via MAP_32BIT, same established
 * pattern as test_audio_input_mixer.cpp's own mmap32(). */
static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ---- singleton storage ---- */
static unsigned char g_globalBuf[0x29ca000];
CSTGGlobal *CSTGGlobal::sInstance = (CSTGGlobal *)g_globalBuf;
unsigned char *STGAPIFrontPanelStatus::sInstance;
static unsigned char g_panelBuf[0x30000]; /* must cover panel+0x29124 (touch-panel-mode byte) */
CSTGFrontPanel *CSTGFrontPanel::sInstance;
static unsigned char g_frontPanelBuf[64];
unsigned char CSTGPerformanceVarsManager::sInstance[12];

/* ---- link-satisfying mocks ---- */

static unsigned int g_tickCount;
extern "C" unsigned int GetSTGTickCount(void) { return g_tickCount; }

static int g_pushMessageCalls;
static STGCalibrationReplyMsg g_lastReply;
extern "C" void PushMessage(void *msg)
{
	g_pushMessageCalls++;
	memcpy(&g_lastReply, msg, sizeof(g_lastReply));
}

static int g_pushUnsolCalls;
static STGCalibrationUnsolMsg g_lastUnsol;
extern "C" void PushUnsolicitedMessage(void *msg)
{
	g_pushUnsolCalls++;
	memcpy(&g_lastUnsol, msg, sizeof(g_lastUnsol));
}

static int g_kbStartCalls, g_kbEndCalls, g_kbCancelCalls;
static unsigned int g_kbLastController;
static short g_applyTableReturn;
extern "C" {
void CSTGKeybedInterface_StartCalibration(unsigned int controller)
{
	g_kbStartCalls++;
	g_kbLastController = controller;
}
void CSTGKeybedInterface_EndCalibration(void) { g_kbEndCalls++; }
void CSTGKeybedInterface_CancelCalibration(void) { g_kbCancelCalls++; }
short CSTGKeybedInterface_ApplyCalibrationAndAfterTouchTable(short)
{
	return g_applyTableReturn;
}
} /* extern "C" */

static int g_hacCalls;
static unsigned int g_hacDevice, g_hacParam2, g_hacParam3;
void CSTGFrontPanel::HandleAnalogController(unsigned int deviceCode, unsigned char param2,
					     unsigned short param3)
{
	g_hacCalls++;
	g_hacDevice = deviceCode;
	g_hacParam2 = param2;
	g_hacParam3 = param3;
}

static void ResetMockCounters()
{
	g_pushMessageCalls = g_pushUnsolCalls = 0;
	g_kbStartCalls = g_kbEndCalls = g_kbCancelCalls = 0;
	g_hacCalls = 0;
	memset(&g_lastReply, 0, sizeof(g_lastReply));
	memset(&g_lastUnsol, 0, sizeof(g_lastUnsol));
}

/* Touch-panel-only mode selector: STGAPIFrontPanelStatus+0x29124 == 3 */
static void SetTouchPanelOnlyMode(bool on)
{
	STGAPIFrontPanelStatus::sInstance[0x29124] = on ? 3 : 0;
}
/* NKS4TestMode selector: CSTGGlobal+0x6ac != 0 */
static void SetNKS4TestMode(bool on)
{
	*(g_globalBuf + 0x6ac) = on ? 1 : 0;
}
/* CSTGPerformanceVarsManager active-slot focus: sInstance[8]=idx,
 * sInstance[idx*4..+4)]=mgr ptr, mgr[0x23d1]==2 means "in focus". */
static unsigned char *g_perfMgrBuf;
static void SetPerfVarsInFocus(bool on)
{
	if (!g_perfMgrBuf) {
		g_perfMgrBuf = (unsigned char *)mmap32(0x23d2);
		memset(g_perfMgrBuf, 0, 0x23d2);
	}
	CSTGPerformanceVarsManager::sInstance[8] = 0;
	unsigned int addr = (unsigned int)(uintptr_t)g_perfMgrBuf;
	memcpy(CSTGPerformanceVarsManager::sInstance, &addr, 4);
	g_perfMgrBuf[0x23d1] = on ? 2 : 0;
}

int main()
{
	CSTGFrontPanel::sInstance = (CSTGFrontPanel *)g_frontPanelBuf;
	STGAPIFrontPanelStatus::sInstance = g_panelBuf;
	memset(g_panelBuf, 0, sizeof(g_panelBuf));
	memset(g_globalBuf, 0, sizeof(g_globalBuf));
	SetPerfVarsInFocus(true);

	CSTGCalibrationMsgHandler handler; /* runs the real ctor, populates sMsgHandler */

	printf("[1] StartJSXCalibration -- touch-panel-only mode: local raw-range reset, no keybed call\n");
	SetTouchPanelOnlyMode(true);
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartJSXCalibration();
	check_eq("keybed StartCalibration NOT called", g_kbStartCalls, 0);

	printf("[2] StartJSXCalibration -- keybed-hw mode: forwards controller id 5\n");
	SetTouchPanelOnlyMode(false);
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartJSXCalibration();
	check_eq("keybed StartCalibration called", g_kbStartCalls, 1);
	check_eq("controller id == 5 (JSX)", (long)g_kbLastController, 5);

	printf("[3] StartJSYCalibration -- keybed-hw mode: forwards controller id 7\n");
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartJSYCalibration();
	check_eq("controller id == 7 (JSY)", (long)g_kbLastController, 7);

	printf("[4] StartRibbonXCalibration -- keybed-hw mode: forwards controller id 8\n");
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartRibbonXCalibration();
	check_eq("controller id == 8 (RibbonX)", (long)g_kbLastController, 8);

	printf("[5] StartAftertouchCalibration -- keybed-hw mode: forwards controller id 9\n");
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartAftertouchCalibration();
	check_eq("controller id == 9 (Aftertouch)", (long)g_kbLastController, 9);

	printf("[6] EndJSXCalibration -- wrong state (sCalibrationOp left at 3 from JSY start): fail reply\n");
	SetTouchPanelOnlyMode(true);
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartJSYCalibration();  /* sCalibrationOp = 3 */
	CSTGCalibrationMsgHandler::EndJSXCalibration();     /* expects op==0 */
	check_eq("PushMessage called", g_pushMessageCalls, 1);
	check_eq("reply.size == 0xc", g_lastReply.size, 0xc);
	check_eq("reply.echoTag == 0xc", g_lastReply.echoTag, 0xc);
	check_eq("reply.result == -1 (fail)", g_lastReply.result, -1);

	printf("[7] StartJSXCalibration + EndJSXCalibration -- touch-panel success path, panel fields\n");
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartJSXCalibration(); /* op=0, ranges reset */
	/* feed two samples via ProcessCalibration (deviceCode=1 for JSX) */
	CSTGCalibrationMsgHandler::ProcessCalibration(1, 0x100);
	CSTGCalibrationMsgHandler::ProcessCalibration(1, 0x300);
	CSTGCalibrationMsgHandler::EndJSXCalibration();
	check_eq("PushMessage called", g_pushMessageCalls, 1);
	check_eq("reply.result == 0 (success)", g_lastReply.result, 0);
	short xMinAdj, xMaxAdj;
	memcpy(&xMinAdj, g_panelBuf + 0x20, 2);
	memcpy(&xMaxAdj, g_panelBuf + 0x26, 2);
	check_eq("panel+0x20 (xMinAdj) == 0x100+0x28", xMinAdj, 0x100 + 0x28);
	check_eq("panel+0x26 (xMaxAdj) == 0x300-0x28", xMaxAdj, 0x300 - 0x28);

	printf("[8] EndTouchScreenCalibration -- always sends a reply, ignores sCalibrationOp\n");
	ResetMockCounters();
	CSTGCalibrationMsgHandler::EndTouchScreenCalibration();
	check_eq("PushMessage called", g_pushMessageCalls, 1);
	check_eq("reply.result == -1 (real quirk: always -1 regardless of state)", g_lastReply.result, -1);

	printf("[9] ProcessCalibration -- default/no-op state leaves raw range untouched\n");
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartTouchScreenCalibration(); /* op = 8, not in ProcessCalibration's switch */
	CSTGCalibrationMsgHandler::ProcessCalibration(1, 0x321);   /* must be a pure no-op */
	CSTGCalibrationMsgHandler::EndJSXCalibration();            /* wrong state (8 != 0) -> fail reply, no field writes */
	check_eq("reply.result == -1", g_lastReply.result, -1);

	printf("[10] ResetController -- NKS4TestMode ON: unsolicited msg, param3(dead scanCode ignored)\n");
	SetNKS4TestMode(true);
	ResetMockCounters();
	CSTGCalibrationMsgHandler::ResetController(7 /*deviceCode*/, 0xAA /*dead scanCode*/,
						    0x55 /*param3*/, 0x3ff /*param4*/);
	check_eq("PushUnsolicitedMessage called", g_pushUnsolCalls, 1);
	check_eq("unsol.size == 0x18", g_lastUnsol.size, 0x18);
	check_eq("unsol.subtype == 0x12", g_lastUnsol.subtype, 0x12);
	check_eq("unsol.deviceCode == 7", g_lastUnsol.deviceCode, 7);
	check_eq("unsol.value == param4 (0x3ff)", g_lastUnsol.value, 0x3ff);
	check_eq("unsol.scanCode == param3 (0x55), NOT the dead 2nd formal", g_lastUnsol.scanCode, 0x55);

	printf("[11] ResetController -- NKS4TestMode OFF, perf-vars in focus: HandleAnalogController(deviceCode, param3, param4)\n");
	SetNKS4TestMode(false);
	ResetMockCounters();
	CSTGCalibrationMsgHandler::ResetController(7, 0xAA, 0x55, 0x3ff);
	check_eq("HandleAnalogController called", g_hacCalls, 1);
	check_eq("param2 == param3 (0x55)", g_hacParam2, 0x55);
	check_eq("param3 == param4 (0x3ff)", g_hacParam3, 0x3ff);

	printf("[12] ResetDamper -- polarity==0, NKS4TestMode ON: {deviceCode=0x1d,value=0,scanCode=0x7f}\n");
	*(g_globalBuf + 0x29c9fbc) = 0;
	SetNKS4TestMode(true);
	ResetMockCounters();
	CSTGCalibrationMsgHandler::ResetDamper();
	check_eq("unsol.deviceCode == 0x1d", g_lastUnsol.deviceCode, 0x1d);
	check_eq("unsol.value == 0", g_lastUnsol.value, 0);
	check_eq("unsol.scanCode == 0x7f", g_lastUnsol.scanCode, 0x7f);

	printf("[13] ResetDamper -- polarity!=0, NKS4TestMode ON: {deviceCode=0x1d,value=0x3ff,scanCode=0}\n");
	*(unsigned int *)(g_globalBuf + 0x29c9fbc) = 1;
	ResetMockCounters();
	CSTGCalibrationMsgHandler::ResetDamper();
	check_eq("unsol.value == 0x3ff", g_lastUnsol.value, 0x3ff);
	check_eq("unsol.scanCode == 0", g_lastUnsol.scanCode, 0);

	printf("[14] HandleKeybedCalibrationResult -- state 4 (JSX end ack), success=true -> reply 0\n");
	*(unsigned int *)(g_globalBuf + 0x29c9fbc) = 0;
	SetTouchPanelOnlyMode(false);
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartJSXCalibration();  /* op=0 */
	CSTGCalibrationMsgHandler::EndJSXCalibration();     /* op != 0? no: op==0 matches, goes hw path -> op=4 */
	ResetMockCounters();
	CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult(true);
	check_eq("PushMessage called", g_pushMessageCalls, 1);
	check_eq("reply.result == 0 (success passed through)", g_lastReply.result, 0);

	printf("[15] HandleKeybedCalibrationResult -- state 4, success=false -> reply -1\n");
	CSTGCalibrationMsgHandler::StartJSXCalibration();
	CSTGCalibrationMsgHandler::EndJSXCalibration();  /* op=4 again */
	ResetMockCounters();
	CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult(false);
	check_eq("reply.result == -1", g_lastReply.result, -1);

	printf("[16] HandleKeybedCalibrationResult -- state 5 (cancel ack): always reply 0, success ignored\n");
	CSTGCalibrationMsgHandler::StartJSXCalibration();
	CSTGCalibrationMsgHandler::CancelJSXCalibration(); /* op=5 */
	ResetMockCounters();
	CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult(false);
	check_eq("reply.result == 0 (forced success)", g_lastReply.result, 0);

	printf("[17] HandleKeybedCalibrationResult -- state 0x11 (aftertouch cancel ack): forces success + HandleAnalogController(7,0,0x3ff)\n");
	SetNKS4TestMode(false);
	CSTGCalibrationMsgHandler::StartAftertouchCalibration(); /* op=0xf */
	CSTGCalibrationMsgHandler::CancelAftertouchCalibration(); /* op=0x11 */
	ResetMockCounters();
	CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult(false /* ignored */);
	check_eq("HandleAnalogController called", g_hacCalls, 1);
	check_eq("deviceCode == 7", g_hacDevice, 7);
	check_eq("param3 == 0x3ff", g_hacParam3, 0x3ff);
	check_eq("reply.result == 0", g_lastReply.result, 0);

	printf("[18] HandleKeybedCalibrationResult -- out-of-range/idle state (0x12): plain no-op, no reply\n");
	ResetMockCounters();
	CSTGCalibrationMsgHandler::HandleKeybedCalibrationResult(true);
	check_eq("PushMessage NOT called", g_pushMessageCalls, 0);

	printf("[19] HalfDamper polarity auto-detect -- ProcessCalibration feed, then EndHalfDamperCalibration\n");
	*(unsigned int *)(g_globalBuf + 0x29c9fbc) = 0; /* polarity 0 */
	ResetMockCounters();
	CSTGCalibrationMsgHandler::StartHalfDamperCalibration(); /* op=0xd, ranges reset, sDamperCalibrator=0 */
	g_tickCount = 1000;
	CSTGCalibrationMsgHandler::ProcessCalibration(0x1d, 0x200); /* first sample: no min/max update yet */
	g_tickCount = 1035; /* elapsed 35 >= 0x1e(30): stale -> commits previous sample into range */
	CSTGCalibrationMsgHandler::ProcessCalibration(0x1d, 0x210);
	CSTGCalibrationMsgHandler::EndHalfDamperCalibration();
	check_eq("PushMessage called", g_pushMessageCalls, 1);
	check_eq("reply.result == 0", g_lastReply.result, 0);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
	return g_fail ? 1 : 0;
}
