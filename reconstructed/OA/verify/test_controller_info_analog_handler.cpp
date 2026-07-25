// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_info_analog_handler.cpp  -  host-side known-answer
 * test for CSTGControllerInfo::AnalogControllerHandler
 * (src/engine/controller_info_analog_handler.cpp, batch 65).
 *
 * Links src/engine/controller_info_analog_handler.cpp directly. Provides
 * its own local mocks/storage for every class this file touches --
 * matching test_front_panel_key_handlers.cpp's own established
 * "isolated single-TU test" convention. Deliberately never exercises the
 * 8 weak-undefined T18/T916/A18/A916 knob/slider handler slots (matching
 * this project's own finding that real hardware never reaches them
 * either -- see oa_global.h's own comment on those declarations).
 */

#include <cstdio>
#include <cstring>

#include "oa_setup_global_resources.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ---- singleton storage ---- */
static unsigned char g_rtdBuf[0x100];
static unsigned char g_globalBuf[0x29cd000]; /* covers the highest offset this file
					       * uses (CSTGGlobal+0x29cc4dc) with headroom */
static unsigned char g_frontPanelBuf[STGAPI_FRONTPANEL_SIZE];

CSTGControllerRTData *CSTGControllerRTData::sInstance = (CSTGControllerRTData *)g_rtdBuf;
CSTGGlobal *CSTGGlobal::sInstance = (CSTGGlobal *)g_globalBuf;
unsigned char *STGAPIFrontPanelStatus::sInstance = g_frontPanelBuf;
unsigned char CSTGCCInfo::sCCInfoTable[1200];
unsigned char CSTGCCInfo::sNumVoiceModelCCs;

static void ResetFixtures()
{
	memset(g_rtdBuf, 0, sizeof(g_rtdBuf));
	memset(g_globalBuf, 0, sizeof(g_globalBuf));
	memset(g_frontPanelBuf, 0, sizeof(g_frontPanelBuf));
	memset(CSTGCCInfo::sCCInfoTable, 0, sizeof(CSTGCCInfo::sCCInfoTable));
}

/* ---- CJumpCatch mocks ---- */
static int g_checkPositionCalls, g_lastPosition; static bool g_lastFlag, g_checkPositionReturn;
bool CSTGControllerRTData::CJumpCatch::CheckPosition(int position, bool flag)
{
	g_checkPositionCalls++;
	g_lastPosition = position;
	g_lastFlag = flag;
	return g_checkPositionReturn;
}
static int g_updateStatusCalls;
void CSTGControllerRTData::CJumpCatch::UpdateStatus() { g_updateStatusCalls++; }

/* ---- SendUnsolControl2MessageToUI mock ---- */
static int g_unsolCalls;
static int g_lastMsgType, g_lastId, g_lastValue, g_lastSource;
void CSTGControllerRTData::SendUnsolControl2MessageToUI(int msgType, int id, int value, int source)
{
	g_unsolCalls++;
	g_lastMsgType = msgType;
	g_lastId = id;
	g_lastValue = value;
	g_lastSource = source;
}

/* ---- SetControllerAssignment mock ---- */
static int g_setAssignCalls; static void *g_lastSelfRef; static signed char g_lastNewValue; static bool g_lastAssignFlag;
void CSTGControllerRTData::SetControllerAssignment(void *selfRef, signed char newValue, bool flag)
{
	g_setAssignCalls++;
	g_lastSelfRef = selfRef;
	g_lastNewValue = newValue;
	g_lastAssignFlag = flag;
}

/* ---- HandleEditInContextKnob/Slider mocks ---- */
static int g_editKnobCalls, g_editSliderCalls; static bool g_editKnobReturn, g_editSliderReturn;
bool CSTGControllerInfo::HandleEditInContextKnob(unsigned int deviceCode, unsigned short param2, unsigned short param3)
{
	(void)deviceCode; (void)param2; (void)param3;
	g_editKnobCalls++;
	return g_editKnobReturn;
}
bool CSTGControllerInfo::HandleEditInContextSlider(unsigned int deviceCode, unsigned short param2, unsigned short param3)
{
	(void)deviceCode; (void)param2; (void)param3;
	g_editSliderCalls++;
	return g_editSliderReturn;
}

/* ---- SendExtModeSliderEvent mock ---- */
static int g_sliderEventCalls; static int g_lastFader; static unsigned int g_lastSliderValue; static bool g_lastNotify;
void CSTGControllerInfo::SendExtModeSliderEvent(int fader, unsigned int value, bool notify)
{
	g_sliderEventCalls++;
	g_lastFader = fader;
	g_lastSliderValue = value;
	g_lastNotify = notify;
}

/* ---- per-handler call counters (macro-generated, 2-arg and 3-arg shapes) ---- */
#define DEF_HANDLER2(name) \
	static int g_##name##_calls; static unsigned short g_##name##_a, g_##name##_b; \
	void CSTGControllerInfo::name(unsigned short a, unsigned short b) { \
		g_##name##_calls++; g_##name##_a = a; g_##name##_b = b; }
#define DEF_HANDLER3(name) \
	static int g_##name##_calls; static unsigned int g_##name##_idx; static unsigned short g_##name##_a, g_##name##_b; \
	void CSTGControllerInfo::name(unsigned int idx, unsigned short a, unsigned short b) { \
		g_##name##_calls++; g_##name##_idx = idx; g_##name##_a = a; g_##name##_b = b; }

DEF_HANDLER2(AnalogJoystickXHandler)
DEF_HANDLER2(AnalogJoystickYHandler)
DEF_HANDLER2(AnalogRibbonXHandler)
DEF_HANDLER2(AnalogRibbonZHandler)
DEF_HANDLER2(AnalogVectorXHandler)
DEF_HANDLER2(AnalogVectorYHandler)
DEF_HANDLER2(AnalogAftertouchHandler)
DEF_HANDLER2(AnalogValueSliderHandler)
DEF_HANDLER2(AnalogTempoHandler)
DEF_HANDLER2(AnalogFootPedalHandler)
DEF_HANDLER2(AnalogFootSwitchHandler)
DEF_HANDLER2(AnalogDamperHandler)
DEF_HANDLER3(AnalogSliderExtHandler)
DEF_HANDLER3(AnalogSliderRTKHandler)
DEF_HANDLER3(AnalogSliderTAHandler)
DEF_HANDLER3(AnalogSliderAInHandler)
DEF_HANDLER3(AnalogSliderSetListEQHandler)
DEF_HANDLER3(AnalogKnobExtHandler)
DEF_HANDLER3(AnalogKnobRTKHandler)
DEF_HANDLER3(AnalogKnobTAHandler)
DEF_HANDLER3(AnalogKnobAInHandler)
DEF_HANDLER3(AnalogKnobSetListEQHandler)

static void ResetCounters()
{
	g_checkPositionCalls = g_updateStatusCalls = g_unsolCalls = g_setAssignCalls = 0;
	g_editKnobCalls = g_editSliderCalls = g_sliderEventCalls = 0;
	g_checkPositionReturn = false;
	g_editKnobReturn = g_editSliderReturn = false;
#define RESET_COUNTER(name) g_##name##_calls = 0;
	RESET_COUNTER(AnalogJoystickXHandler) RESET_COUNTER(AnalogJoystickYHandler)
	RESET_COUNTER(AnalogRibbonXHandler) RESET_COUNTER(AnalogRibbonZHandler)
	RESET_COUNTER(AnalogVectorXHandler) RESET_COUNTER(AnalogVectorYHandler)
	RESET_COUNTER(AnalogAftertouchHandler) RESET_COUNTER(AnalogValueSliderHandler)
	RESET_COUNTER(AnalogTempoHandler) RESET_COUNTER(AnalogFootPedalHandler)
	RESET_COUNTER(AnalogFootSwitchHandler) RESET_COUNTER(AnalogDamperHandler)
	RESET_COUNTER(AnalogSliderExtHandler) RESET_COUNTER(AnalogSliderRTKHandler)
	RESET_COUNTER(AnalogSliderTAHandler) RESET_COUNTER(AnalogSliderAInHandler)
	RESET_COUNTER(AnalogSliderSetListEQHandler) RESET_COUNTER(AnalogKnobExtHandler)
	RESET_COUNTER(AnalogKnobRTKHandler) RESET_COUNTER(AnalogKnobTAHandler)
	RESET_COUNTER(AnalogKnobAInHandler) RESET_COUNTER(AnalogKnobSetListEQHandler)
#undef RESET_COUNTER
}

int main()
{
	static unsigned char selfBuf[0x20];
	CSTGControllerInfo &self = *(CSTGControllerInfo *)selfBuf;
	memset(selfBuf, 0, sizeof(selfBuf));

	/* --- knob busy-notify (device 9 = knobIndex 1) --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(9, 0x55, 0x1234);
	check_eq("knob-busy: unsol calls", g_unsolCalls, 1);
	check_eq("knob-busy: msgType", g_lastMsgType, 0xe);
	check_eq("knob-busy: id=knobIndex", g_lastId, 1);
	check_eq("knob-busy: value=param2", g_lastValue, 0x55);
	check_eq("knob-busy: source", g_lastSource, 1);
	check_eq("knob-busy: no mode dispatch", g_AnalogKnobExtHandler_calls, 0);

	/* --- knob mode dispatch (device 12 = knobIndex 4, mode 4 = Ext) --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 4;
	self.AnalogControllerHandler(12, 0x11, 0x2222);
	check_eq("knob-mode4: Ext handler calls", g_AnalogKnobExtHandler_calls, 1);
	check_eq("knob-mode4: idx", (long)g_AnalogKnobExtHandler_idx, 4);
	check_eq("knob-mode4: a=param2", g_AnalogKnobExtHandler_a, 0x11);
	check_eq("knob-mode4: b=param3", g_AnalogKnobExtHandler_b, 0x2222);
	check_eq("knob-mode4: no unsol", g_unsolCalls, 0);

	/* --- knob edit-in-context true (device 8) --- */
	ResetFixtures(); ResetCounters();
	*(unsigned int *)(g_globalBuf + 0x29cc4dc) = 1;
	g_editKnobReturn = true;
	self.AnalogControllerHandler(8, 0, 0);
	check_eq("knob-edit-true: edit calls", g_editKnobCalls, 1);
	check_eq("knob-edit-true: no mode dispatch", g_AnalogKnobExtHandler_calls, 0);

	/* --- knob edit-in-context false falls through to mode dispatch --- */
	ResetFixtures(); ResetCounters();
	*(unsigned int *)(g_globalBuf + 0x29cc4dc) = 1;
	g_editKnobReturn = false;
	g_rtdBuf[0x2b] = 5; /* RTK */
	self.AnalogControllerHandler(8, 7, 9);
	check_eq("knob-edit-false: edit calls", g_editKnobCalls, 1);
	check_eq("knob-edit-false: falls through to RTK", g_AnalogKnobRTKHandler_calls, 1);

	/* --- slider busy-notify (device 17 = sliderIndex 1) --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(17, 0x42, 0x99);
	check_eq("slider-busy: msgType", g_lastMsgType, 0xf);
	check_eq("slider-busy: id=sliderIndex", g_lastId, 1);

	/* --- slider mode dispatch (device 20 = sliderIndex 4, mode 8 = SetListEQ) --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 8;
	self.AnalogControllerHandler(20, 3, 4);
	check_eq("slider-mode8: SetListEQ calls", g_AnalogSliderSetListEQHandler_calls, 1);
	check_eq("slider-mode8: idx", (long)g_AnalogSliderSetListEQHandler_idx, 4);

	/* --- fixed 1-7: JoystickX normal dispatch --- */
	ResetFixtures(); ResetCounters();
	self.AnalogControllerHandler(1, 0x10, 0x200);
	check_eq("joystickX: calls", g_AnalogJoystickXHandler_calls, 1);
	check_eq("joystickX: a=param2", g_AnalogJoystickXHandler_a, 0x10);
	check_eq("joystickX: b=param3", g_AnalogJoystickXHandler_b, 0x200);

	/* --- fixed 1-7 busy: JoystickX UI echo --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(1, 0, 0x2ab);
	check_eq("joystickX-busy: echo value", *(unsigned short *)(g_frontPanelBuf + STGAPI_OFF_ANALOG_ECHO_JOYX), 0x2ab);
	check_eq("joystickX-busy: no real handler call", g_AnalogJoystickXHandler_calls, 0);

	/* --- fixed 1-7 busy: RibbonZ has no echo write (just verify no crash + no dispatch) --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(4, 0, 0x1);
	check_eq("ribbonZ-busy: no real handler call", g_AnalogRibbonZHandler_calls, 0);

	/* --- fixed 0x19-0x1D busy specials --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(0x1a, 0, 0x77);
	check_eq("dev1a-busy: msgType", g_lastMsgType, 0xc);
	check_eq("dev1a-busy: id", g_lastId, 0);
	check_eq("dev1a-busy: value=param3", g_lastValue, 0x77);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(0x1b, 0, 0x88);
	check_eq("dev1b-busy: msgType", g_lastMsgType, 0x15);
	check_eq("dev1b-busy: id", g_lastId, 0);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(0x1c, 0, 0x99);
	check_eq("dev1c-busy: msgType", g_lastMsgType, 0x15);
	check_eq("dev1c-busy: id", g_lastId, 1);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(0x1d, 0, 0x100);
	check_eq("dev1d-busy: damper echo", *(unsigned short *)(g_frontPanelBuf + STGAPI_OFF_ANALOG_ECHO_DAMPER), 0x3ff - 0x100);
	check_eq("dev1d-busy: no message sent", g_unsolCalls, 0);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(0x19, 0x66, 0);
	check_eq("dev19-busy: msgType", g_lastMsgType, 0xf);
	check_eq("dev19-busy: id", g_lastId, 9);
	check_eq("dev19-busy: value=param2", g_lastValue, 0x66);

	/* --- fixed 0x19-0x1D normal dispatch --- */
	ResetFixtures(); ResetCounters();
	self.AnalogControllerHandler(0x1d, 1, 2);
	check_eq("dev1d-normal: Damper handler calls", g_AnalogDamperHandler_calls, 1);

	/* --- device 0x18 (Value), mode 4, busy2=0: CheckPosition true --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 4;
	g_checkPositionReturn = true;
	self.AnalogControllerHandler(0x18, 0x50, 0x999);
	check_eq("value-mode4-nonbusy-true: CheckPosition calls", g_checkPositionCalls, 1);
	check_eq("value-mode4-nonbusy-true: position=param2", g_lastPosition, 0x50);
	check_eq("value-mode4-nonbusy-true: flag=true", g_lastFlag, true);
	check_eq("value-mode4-nonbusy-true: SendExtModeSliderEvent calls", g_sliderEventCalls, 1);
	check_eq("value-mode4-nonbusy-true: fader=8", g_lastFader, 8);
	check_eq("value-mode4-nonbusy-true: value=param2", (long)g_lastSliderValue, 0x50);

	/* --- device 0x18, mode 4, busy2=0: CheckPosition false -> no slider event --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 4;
	g_checkPositionReturn = false;
	self.AnalogControllerHandler(0x18, 0x50, 0x999);
	check_eq("value-mode4-nonbusy-false: no slider event", g_sliderEventCalls, 0);

	/* --- device 0x18, mode 4, busy2=1: CC-lookup path --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 4;
	g_rtdBuf[0x2f] = 0x2; /* busy2 set */
	*(unsigned int *)(g_globalBuf + 0x29cc0c8) = 0; /* assignMode = 0 */
	g_globalBuf[0x29cbc50] = 7; /* assigned CC = 7 */
	CSTGCCInfo::sCCInfoTable[7 * 10] = 0x40; /* CC 7's default value */
	self.AnalogControllerHandler(0x18, 0, 0);
	check_eq("value-mode4-busy2: UpdateStatus calls", g_updateStatusCalls, 1);
	check_eq("value-mode4-busy2: SendExtModeSliderEvent calls", g_sliderEventCalls, 1);
	check_eq("value-mode4-busy2: value=ccDefault", (long)g_lastSliderValue, 0x40);
	check_eq("value-mode4-busy2: rtd[0x86] stored", g_rtdBuf[0x86], 0x40);

	/* --- device 0x18, mode 4, busy2=1, CC unassigned (0xff) -> silent return --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 4;
	g_rtdBuf[0x2f] = 0x2;
	g_globalBuf[0x29cbc50] = 0xff;
	self.AnalogControllerHandler(0x18, 0, 0);
	check_eq("value-mode4-busy2-unassigned: no slider event", g_sliderEventCalls, 0);

	/* --- device 0x18, outer busy (&8) --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(0x18, 0x33, 0);
	check_eq("value-outer-busy: msgType", g_lastMsgType, 0xf);
	check_eq("value-outer-busy: id", g_lastId, 8);
	check_eq("value-outer-busy: value=param2", g_lastValue, 0x33);

	/* --- device 0x18, mode 6 (Tempo), busy2=1 --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 6;
	g_rtdBuf[0x2f] = 0x2;
	self.AnalogControllerHandler(0x18, 0x44, 0);
	check_eq("value-mode6-busy2: unsol msgType", g_lastMsgType, 0xf);
	check_eq("value-mode6-busy2: unsol id", g_lastId, 8);
	check_eq("value-mode6-busy2: rtd[0x85] stored", g_rtdBuf[0x85], 0x44);

	/* --- device 0x18, mode 6, non-busy: deferred stub, must not crash or call unsol --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 6;
	self.AnalogControllerHandler(0x18, 0x44, 0x55);
	check_eq("value-mode6-nonbusy: no unsol (deferred stub)", g_unsolCalls, 0);
	check_eq("value-mode6-nonbusy: no slider event", g_sliderEventCalls, 0);

	/* --- device 0x18, mode 8 (EQ): deferred stub for both sub-paths --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 8;
	self.AnalogControllerHandler(0x18, 0x1, 0x2);
	check_eq("value-mode8: no unsol (deferred stub)", g_unsolCalls, 0);
	check_eq("value-mode8: no CheckPosition", g_checkPositionCalls, 0);

	/* --- device 0x18, default mode, busy2=0, CheckPosition false --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 2; /* not 4/6/8 */
	g_globalBuf[0x6af] = 1;
	g_checkPositionReturn = false;
	self.AnalogControllerHandler(0x18, 0x77, 0);
	check_eq("value-default-nonbusy-false: CheckPosition calls", g_checkPositionCalls, 1);
	check_eq("value-default-nonbusy-false: flag from Global[0x6af]", g_lastFlag, true);

	/* --- device 0x1e --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[8] = 0x13;
	self.AnalogControllerHandler(0x1e, 1, 0);
	check_eq("dev1e: SetControllerAssignment calls", g_setAssignCalls, 1);
	check_eq("dev1e: newValue=rtd[8]", g_lastNewValue, 0x13);
	check_eq("dev1e: flag=true", g_lastAssignFlag, true);
	check_eq("dev1e: rtd[5] stored", g_rtdBuf[5], 1);

	ResetFixtures(); ResetCounters();
	self.AnalogControllerHandler(0x1e, 0, 0);
	check_eq("dev1e-param2zero: newValue=0", g_lastNewValue, 0);
	check_eq("dev1e-param2zero: rtd[5] stored", g_rtdBuf[5], 0);

	/* --- out of range: silent no-op --- */
	ResetFixtures(); ResetCounters();
	self.AnalogControllerHandler(0x30, 1, 2);
	check_eq("out-of-range: no unsol", g_unsolCalls, 0);
	check_eq("out-of-range: no slider event", g_sliderEventCalls, 0);
	check_eq("out-of-range: no assign", g_setAssignCalls, 0);

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
