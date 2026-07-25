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

/* AnalogJoystickYHandler/AnalogRibbonXHandler/AnalogRibbonZHandler/
 * AnalogVectorXHandler/AnalogVectorYHandler/AnalogValueSliderHandler/
 * AnalogFootPedalHandler/AnalogFootSwitchHandler/AnalogDamperHandler are
 * now REAL (follow-up pass) -- see the new mocks/tests below for their
 * own deferred callees (SendCCToKG, HandleFootSwitchChange,
 * HandleFootPedalChange, ProcessJoystickY, CPedalFilter::Filter)
 * instead of a generic call-counter stub for the handler itself.
 */
DEF_HANDLER2(AnalogJoystickXHandler)
DEF_HANDLER2(AnalogAftertouchHandler)
DEF_HANDLER2(AnalogTempoHandler)
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

/* ---- new deferred-callee mocks for the 9 newly-real handlers ---- */
static int g_sendCCToKG2Calls; static unsigned char g_lastCC2A, g_lastCC2B;
void CSTGControllerRTData::SendCCToKG(unsigned char a, unsigned char b)
{
	g_sendCCToKG2Calls++;
	g_lastCC2A = a;
	g_lastCC2B = b;
}
static int g_sendCCToKG3Calls; static unsigned char g_lastCC3A, g_lastCC3B, g_lastCC3C;
void CSTGControllerRTData::SendCCToKG(unsigned char a, unsigned char b, unsigned char c)
{
	g_sendCCToKG3Calls++;
	g_lastCC3A = a;
	g_lastCC3B = b;
	g_lastCC3C = c;
}
static int g_footSwitchCalls; static bool g_lastFootSwitchPressed;
void CSTGControllerRTData::HandleFootSwitchChange(bool pressed)
{
	g_footSwitchCalls++;
	g_lastFootSwitchPressed = pressed;
}
static int g_footPedalCalls; static unsigned char g_lastFootPedalValue;
void CSTGControllerRTData::HandleFootPedalChange(unsigned char value)
{
	g_footPedalCalls++;
	g_lastFootPedalValue = value;
}
static int g_processJoystickYCalls; static unsigned short g_lastProcessJoystickYValue;
void CSTGControllerInfo::ProcessJoystickY(unsigned short value)
{
	g_processJoystickYCalls++;
	g_lastProcessJoystickYValue = value;
}
static int g_pedalFilterCalls; static unsigned char g_lastPedalFilterValue; static bool g_pedalFilterReturn;
bool CSTGControllerRTData::CPedalFilter::Filter(unsigned char value)
{
	g_pedalFilterCalls++;
	g_lastPedalFilterValue = value;
	return g_pedalFilterReturn;
}

static void ResetCounters()
{
	g_checkPositionCalls = g_updateStatusCalls = g_unsolCalls = g_setAssignCalls = 0;
	g_editKnobCalls = g_editSliderCalls = g_sliderEventCalls = 0;
	g_checkPositionReturn = false;
	g_editKnobReturn = g_editSliderReturn = false;
#define RESET_COUNTER(name) g_##name##_calls = 0;
	RESET_COUNTER(AnalogJoystickXHandler)
	RESET_COUNTER(AnalogAftertouchHandler) RESET_COUNTER(AnalogTempoHandler)
	RESET_COUNTER(AnalogSliderExtHandler) RESET_COUNTER(AnalogSliderRTKHandler)
	RESET_COUNTER(AnalogSliderTAHandler) RESET_COUNTER(AnalogSliderAInHandler)
	RESET_COUNTER(AnalogSliderSetListEQHandler) RESET_COUNTER(AnalogKnobExtHandler)
	RESET_COUNTER(AnalogKnobRTKHandler) RESET_COUNTER(AnalogKnobTAHandler)
	RESET_COUNTER(AnalogKnobAInHandler) RESET_COUNTER(AnalogKnobSetListEQHandler)
#undef RESET_COUNTER
	g_sendCCToKG2Calls = g_sendCCToKG3Calls = 0;
	g_footSwitchCalls = g_footPedalCalls = g_processJoystickYCalls = 0;
	g_pedalFilterCalls = 0; g_pedalFilterReturn = false;
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

	/* --- fixed 1-7 busy: RibbonZ has no echo write (just verify no crash) --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(4, 0, 0x1);
	check_eq("ribbonZ-busy: no CC send (busy path doesn't call the handler)", g_sendCCToKG2Calls + g_sendCCToKG3Calls, 0);

	/* --- fixed 1-7 non-busy: RibbonZ real body is a literal no-op --- */
	ResetFixtures(); ResetCounters();
	self.AnalogControllerHandler(4, 5, 6);
	check_eq("ribbonZ-nonbusy: no CC send", g_sendCCToKG2Calls + g_sendCCToKG3Calls, 0);
	check_eq("ribbonZ-nonbusy: no unsol", g_unsolCalls, 0);

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

	/* --- fixed 0x19-0x1D normal dispatch: AnalogDamperHandler, no invert,
	 * Filter() returns true -- real table-verified filtered byte 0x7a
	 * (kDamperFilterTable[102], (0x100+0x98)>>2=102). */
	ResetFixtures(); ResetCounters();
	g_pedalFilterReturn = true;
	self.AnalogControllerHandler(0x1d, 1, 0x100);
	check_eq("dev1d-normal: PedalFilter calls", g_pedalFilterCalls, 1);
	check_eq("dev1d-normal: PedalFilter arg (real curve table value)", g_lastPedalFilterValue, 0x7a);
	check_eq("dev1d-normal: SendCCToKG(3-arg) calls", g_sendCCToKG3Calls, 1);
	check_eq("dev1d-normal: SendCCToKG cc=rtd[0xc]", g_lastCC3A, g_rtdBuf[0xc]);
	check_eq("dev1d-normal: SendCCToKG fixed 0x40", g_lastCC3B, 0x40);
	check_eq("dev1d-normal: SendCCToKG value=filtered", g_lastCC3C, 0x7a);
	check_eq("dev1d-normal: STGAPI+0x106 echo = 0x3ff-b",
		 *(unsigned short *)(g_frontPanelBuf + 0x106), 0x3ff - 0x100);

	/* --- same, but with the CSTGGlobal+0x29c9fbc polarity-invert flag
	 * set: filtered value changes, echo write still uses the RAW b. --- */
	ResetFixtures(); ResetCounters();
	*(unsigned int *)(g_globalBuf + 0x29c9fbc) = 1;
	g_pedalFilterReturn = true;
	self.AnalogControllerHandler(0x1d, 1, 0x50);
	check_eq("dev1d-invert: PedalFilter arg (inverted curve value)", g_lastPedalFilterValue, 0x7f);
	check_eq("dev1d-invert: STGAPI echo STILL uses raw b",
		 *(unsigned short *)(g_frontPanelBuf + 0x106), 0x3ff - 0x50);

	/* --- Filter() returns false: no CC send, no echo write --- */
	ResetFixtures(); ResetCounters();
	g_pedalFilterReturn = false;
	self.AnalogControllerHandler(0x1d, 1, 0x100);
	check_eq("dev1d-filterfalse: no CC send", g_sendCCToKG3Calls, 0);
	check_eq("dev1d-filterfalse: no echo write",
		 *(unsigned short *)(g_frontPanelBuf + 0x106), 0);

	/* --- AnalogJoystickYHandler: self-discards a, tail-calls
	 * ProcessJoystickY(b) on self. --- */
	ResetFixtures(); ResetCounters();
	self.AnalogControllerHandler(2, 0x11, 0x22);
	check_eq("joystickY: ProcessJoystickY calls", g_processJoystickYCalls, 1);
	check_eq("joystickY: value=b(param3)", g_lastProcessJoystickYValue, 0x22);

	/* --- AnalogFootSwitchHandler/AnalogFootPedalHandler: reached via
	 * AnalogControllerHandler's own devices 0x1c/0x1b non-busy dispatch
	 * (only the busy-flag-SET path is exercised in the "fixed 0x19-0x1D
	 * busy" block above) -- exercise the real handler bodies directly
	 * here instead, same as this file's own established convention for
	 * device-0x18 sub-handlers elsewhere. */
	ResetFixtures(); ResetCounters();
	self.AnalogFootSwitchHandler(0x40, 0);
	check_eq("footswitch: HandleFootSwitchChange calls", g_footSwitchCalls, 1);
	check_eq("footswitch: pressed=true (a>0x3f)", g_lastFootSwitchPressed, true);

	ResetFixtures(); ResetCounters();
	self.AnalogFootSwitchHandler(0x3f, 0);
	check_eq("footswitch-boundary: pressed=false (a==0x3f)", g_lastFootSwitchPressed, false);

	ResetFixtures(); ResetCounters();
	self.AnalogFootPedalHandler(0x55, 0);
	check_eq("footpedal: HandleFootPedalChange calls", g_footPedalCalls, 1);
	check_eq("footpedal: value=(u8)a", g_lastFootPedalValue, 0x55);

	/* --- AnalogValueSliderHandler: b unused, rtd[0x49]&1 gates an
	 * extra SendCCToKG before the always-sent UI message. --- */
	ResetFixtures(); ResetCounters();
	self.AnalogValueSliderHandler(0x33, 0x999);
	check_eq("valueslider-flagclear: no CC send", g_sendCCToKG2Calls, 0);
	check_eq("valueslider-flagclear: unsol msgType/id", g_lastMsgType * 100 + g_lastId, 0xf * 100 + 9);
	check_eq("valueslider-flagclear: unsol value=a", g_lastValue, 0x33);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x49] = 1;
	self.AnalogValueSliderHandler(0x33, 0x999);
	check_eq("valueslider-flagset: CC send calls", g_sendCCToKG2Calls, 1);
	check_eq("valueslider-flagset: CC arg0=0x12", g_lastCC2A, 0x12);
	check_eq("valueslider-flagset: CC arg1=(u8)a", g_lastCC2B, 0x33);
	check_eq("valueslider-flagset: unsol still sent", g_unsolCalls, 1);

	/* --- AnalogVectorYHandler / AnalogVectorXHandler --- */
	ResetFixtures(); ResetCounters();
	g_globalBuf[0x6c0] = 5;
	g_globalBuf[0x6c1] = 7;
	self.AnalogVectorYHandler(0x20, 0);
	check_eq("vectorY-nonbusy: CC send calls", g_sendCCToKG2Calls, 1);
	check_eq("vectorY-nonbusy: CC arg0=Global[0x6c1]", g_lastCC2A, 7);
	check_eq("vectorY-nonbusy: CC arg1=(u8)a", g_lastCC2B, 0x20);

	ResetFixtures(); ResetCounters();
	g_globalBuf[0x6c1] = 0xff; /* signed -1: unassigned */
	self.AnalogVectorYHandler(0x20, 0);
	check_eq("vectorY-unassigned: no CC send", g_sendCCToKG2Calls, 0);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2; /* busy2 */
	g_globalBuf[0x6c0] = 3;
	g_globalBuf[0x6c1] = 9;
	self.AnalogVectorYHandler(0x20, 0);
	check_eq("vectorY-busy2: CC send calls (both axes recentered)", g_sendCCToKG2Calls, 2);
	check_eq("vectorY-busy2: last CC arg0=Global[0x6c1]", g_lastCC2A, 9);
	check_eq("vectorY-busy2: last CC arg1=0x40", g_lastCC2B, 0x40);

	ResetFixtures(); ResetCounters();
	g_globalBuf[0x6c0] = 4;
	g_globalBuf[0x6c1] = 6;
	self.AnalogVectorXHandler(0x30, 0);
	check_eq("vectorX-nonbusy: CC arg0=Global[0x6c0]", g_lastCC2A, 4);
	check_eq("vectorX-nonbusy: CC arg1=(u8)a", g_lastCC2B, 0x30);

	/* --- AnalogRibbonXHandler --- */
	ResetFixtures(); ResetCounters();
	self.AnalogRibbonXHandler(0x40, 0x100);
	check_eq("ribbonX-firsttouch: CC send calls", g_sendCCToKG2Calls, 1);
	check_eq("ribbonX-firsttouch: CC arg0=0x10", g_lastCC2A, 0x10);
	check_eq("ribbonX-firsttouch: CC arg1=(u8)a", g_lastCC2B, 0x40);
	check_eq("ribbonX-firsttouch: rtd[0x20]=a", g_rtdBuf[0x20], 0x40);
	check_eq("ribbonX-firsttouch: rtd[0x2f] bit0 latched", g_rtdBuf[0x2f] & 1, 1);
	check_eq("ribbonX-firsttouch: STGAPI+0x10a=1", g_frontPanelBuf[0x10a], 1);
	check_eq("ribbonX-firsttouch: STGAPI+0x108=0x3ff-b",
		 *(unsigned short *)(g_frontPanelBuf + 0x108), 0x3ff - 0x100);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 1;
	g_rtdBuf[0x20] = 0x40;
	self.AnalogRibbonXHandler(0x40, 0x100); /* same value, Global[0x6ac]==0 -> no re-send */
	check_eq("ribbonX-unchanged: no CC send", g_sendCCToKG2Calls, 0);
	check_eq("ribbonX-unchanged: bit0 stays latched", g_rtdBuf[0x2f] & 1, 1);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 1;
	g_rtdBuf[0x20] = 0x40;
	self.AnalogRibbonXHandler(0x41, 0x100); /* different value -> re-send */
	check_eq("ribbonX-changed: CC send calls", g_sendCCToKG2Calls, 1);
	check_eq("ribbonX-changed: rtd[0x20] updated", g_rtdBuf[0x20], 0x41);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 1;
	g_rtdBuf[0x20] = 0x40;
	g_globalBuf[0x6ac] = 1; /* force re-send even if unchanged */
	self.AnalogRibbonXHandler(0x40, 0x100);
	check_eq("ribbonX-forced: CC send calls", g_sendCCToKG2Calls, 1);

	ResetFixtures(); ResetCounters();
	self.AnalogRibbonXHandler(0x40, 0); /* b==0: release */
	check_eq("ribbonX-release-notactive: no CC send", g_sendCCToKG2Calls, 0);
	check_eq("ribbonX-release-notactive: STGAPI+0x10a cleared", g_frontPanelBuf[0x10a], 0);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 1; /* active latch set; rtd[0x14/0x15/0x16] default 0 -> real
			     * table[0]|table[0]|table[0] = 0 (unlocked) */
	self.AnalogRibbonXHandler(0x40, 0); /* b==0, active -> recenter+send */
	check_eq("ribbonX-release-active-unlocked: CC send calls", g_sendCCToKG2Calls, 1);
	check_eq("ribbonX-release-active-unlocked: CC arg0=0x10", g_lastCC2A, 0x10);
	check_eq("ribbonX-release-active-unlocked: CC arg1=0x40(recenter)", g_lastCC2B, 0x40);
	check_eq("ribbonX-release-active-unlocked: rtd[0x20] recentered", g_rtdBuf[0x20], 0x40);
	check_eq("ribbonX-release-active-unlocked: bit0 cleared", g_rtdBuf[0x2f] & 1, 0);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 1;
	g_rtdBuf[0x14] = 5; g_rtdBuf[0x15] = 5; g_rtdBuf[0x16] = 5; /* table[5]=8 -> &8 nonzero -> locked */
	self.AnalogRibbonXHandler(0x40, 0);
	check_eq("ribbonX-release-active-locked: no CC send", g_sendCCToKG2Calls, 0);
	check_eq("ribbonX-release-active-locked: bit0 cleared", g_rtdBuf[0x2f] & 1, 0);

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
