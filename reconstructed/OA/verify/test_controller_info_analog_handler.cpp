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
static unsigned char g_midiPortMgrBuf[0x300];

CSTGControllerRTData *CSTGControllerRTData::sInstance = (CSTGControllerRTData *)g_rtdBuf;
CSTGGlobal *CSTGGlobal::sInstance = (CSTGGlobal *)g_globalBuf;
unsigned char *STGAPIFrontPanelStatus::sInstance = g_frontPanelBuf;
unsigned char CSTGCCInfo::sCCInfoTable[1200];
unsigned char CSTGCCInfo::sNumVoiceModelCCs;
CSTGMidiPortManager *CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)g_midiPortMgrBuf;

static void ResetFixtures()
{
	memset(g_rtdBuf, 0, sizeof(g_rtdBuf));
	memset(g_globalBuf, 0, sizeof(g_globalBuf));
	memset(g_frontPanelBuf, 0, sizeof(g_frontPanelBuf));
	memset(CSTGCCInfo::sCCInfoTable, 0, sizeof(CSTGCCInfo::sCCInfoTable));
	memset(g_midiPortMgrBuf, 0, sizeof(g_midiPortMgrBuf));
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
/* AnalogJoystickXHandler/AnalogAftertouchHandler/AnalogKnobExtHandler/
 * AnalogSliderExtHandler/AnalogKnobRTKHandler/AnalogSliderRTKHandler are
 * now REAL (batch 68) -- see the new mocks below for their OWN deferred
 * callees (CPitchBendFilter::Filter, CSTGMidiQueueWriter::Write,
 * SendExtModeKnobEvent, SendKarmaCCToKG, SetRTKModeKnob/
 * ResetRTKModeKnob) instead of a generic call-counter stub for the
 * handler itself. AnalogTempoHandler/AnalogSliderTAHandler/
 * AnalogSliderAInHandler/AnalogSliderSetListEQHandler/AnalogKnobTAHandler/
 * AnalogKnobAInHandler/AnalogKnobSetListEQHandler remain confirmed DSP
 * (batch 68 disassembly-confirmed, not just carried forward) -- still
 * plain call-counter mocks.
 */
DEF_HANDLER2(AnalogTempoHandler)
DEF_HANDLER3(AnalogSliderTAHandler)
DEF_HANDLER3(AnalogSliderAInHandler)
DEF_HANDLER3(AnalogSliderSetListEQHandler)
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

/* ---- new mocks for the 6 newly-real handlers' own deferred callees (batch 68) ---- */
static int g_pitchBendFilterCalls; static unsigned short g_lastPitchBendFilterValue;
static bool g_pitchBendFilterReturn; static unsigned char g_pitchBendFilterChannel;
static unsigned short g_pitchBendFilterOutValue;
bool CSTGControllerRTData::CPitchBendFilter::Filter(unsigned short newValue)
{
	g_pitchBendFilterCalls++;
	g_lastPitchBendFilterValue = newValue;
	/* real Filter() mutates its own object's state as a side effect;
	 * the mock reproduces that via test-controlled fixture values. */
	channel = g_pitchBendFilterChannel;
	value = g_pitchBendFilterOutValue;
	return g_pitchBendFilterReturn;
}

static int g_midiWriteCalls; static unsigned char g_lastMidiMsg[5]; static unsigned int g_lastMidiLen;
void CSTGMidiQueueWriter::Write(const unsigned char *data, unsigned int length, bool)
{
	g_midiWriteCalls++;
	g_lastMidiLen = length;
	for (unsigned int i = 0; i < length && i < 5; i++)
		g_lastMidiMsg[i] = data[i];
}

static int g_extKnobEventCalls; static int g_lastKnobIndex; static unsigned int g_lastKnobValue; static bool g_lastKnobNotify;
void CSTGControllerInfo::SendExtModeKnobEvent(int knobIndex, unsigned int value, bool notify)
{
	g_extKnobEventCalls++;
	g_lastKnobIndex = knobIndex;
	g_lastKnobValue = value;
	g_lastKnobNotify = notify;
}

static int g_karmaCCCalls; static int g_lastKarmaCCNo; static unsigned char g_lastKarmaCCValue;
void CSTGControllerRTData::SendKarmaCCToKG(int karmaCCNo, unsigned char value)
{
	g_karmaCCCalls++;
	g_lastKarmaCCNo = karmaCCNo;
	g_lastKarmaCCValue = value;
}

static int g_setRTKKnobCalls; static unsigned short g_lastSetRTKIdx, g_lastSetRTKValue;
void CSTGControllerInfo::SetRTKModeKnob(unsigned short idx, unsigned short value, bool, int, bool)
{
	g_setRTKKnobCalls++;
	g_lastSetRTKIdx = idx;
	g_lastSetRTKValue = value;
}

static int g_resetRTKKnobCalls; static unsigned short g_lastResetRTKIdx;
void CSTGControllerInfo::ResetRTKModeKnob(unsigned short idx)
{
	g_resetRTKKnobCalls++;
	g_lastResetRTKIdx = idx;
}

static void ResetCounters()
{
	g_checkPositionCalls = g_updateStatusCalls = g_unsolCalls = g_setAssignCalls = 0;
	g_editKnobCalls = g_editSliderCalls = g_sliderEventCalls = 0;
	g_checkPositionReturn = false;
	g_editKnobReturn = g_editSliderReturn = false;
#define RESET_COUNTER(name) g_##name##_calls = 0;
	RESET_COUNTER(AnalogTempoHandler)
	RESET_COUNTER(AnalogSliderTAHandler) RESET_COUNTER(AnalogSliderAInHandler)
	RESET_COUNTER(AnalogSliderSetListEQHandler)
	RESET_COUNTER(AnalogKnobTAHandler)
	RESET_COUNTER(AnalogKnobAInHandler) RESET_COUNTER(AnalogKnobSetListEQHandler)
#undef RESET_COUNTER
	g_sendCCToKG2Calls = g_sendCCToKG3Calls = 0;
	g_footSwitchCalls = g_footPedalCalls = g_processJoystickYCalls = 0;
	g_pedalFilterCalls = 0; g_pedalFilterReturn = false;
	g_pitchBendFilterCalls = 0; g_pitchBendFilterReturn = false;
	g_pitchBendFilterChannel = 0; g_pitchBendFilterOutValue = 0;
	g_midiWriteCalls = 0; g_lastMidiLen = 0;
	memset(g_lastMidiMsg, 0, sizeof(g_lastMidiMsg));
	g_extKnobEventCalls = 0;
	g_karmaCCCalls = 0;
	g_setRTKKnobCalls = 0;
	g_resetRTKKnobCalls = 0;
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
	check_eq("knob-busy: no mode dispatch", g_checkPositionCalls, 0);

	/* --- knob mode dispatch (device 12 = knobIndex 4, mode 4 = Ext) --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 4;
	g_checkPositionReturn = true;
	self.AnalogControllerHandler(12, 0x11, 0x2222);
	check_eq("knob-mode4: CheckPosition calls", g_checkPositionCalls, 1);
	check_eq("knob-mode4: CheckPosition position=a(raw)", g_lastPosition, 0x11);
	check_eq("knob-mode4: Ext event calls", g_extKnobEventCalls, 1);
	check_eq("knob-mode4: idx=knobIndex", (long)g_lastKnobIndex, 4);
	check_eq("knob-mode4: value=a(raw)", (long)g_lastKnobValue, 0x11);
	check_eq("knob-mode4: notify=true", g_lastKnobNotify, true);
	check_eq("knob-mode4: no unsol", g_unsolCalls, 0);

	/* --- knob edit-in-context true (device 8) --- */
	ResetFixtures(); ResetCounters();
	*(unsigned int *)(g_globalBuf + 0x29cc4dc) = 1;
	g_editKnobReturn = true;
	self.AnalogControllerHandler(8, 0, 0);
	check_eq("knob-edit-true: edit calls", g_editKnobCalls, 1);
	check_eq("knob-edit-true: no mode dispatch", g_checkPositionCalls, 0);

	/* --- knob edit-in-context false falls through to mode dispatch (RTK) --- */
	ResetFixtures(); ResetCounters();
	*(unsigned int *)(g_globalBuf + 0x29cc4dc) = 1;
	g_editKnobReturn = false;
	g_rtdBuf[0x2b] = 5; /* RTK */
	g_checkPositionReturn = true;
	self.AnalogControllerHandler(8, 7, 9);
	check_eq("knob-edit-false: edit calls", g_editKnobCalls, 1);
	check_eq("knob-edit-false: falls through to RTK, SetRTKModeKnob calls", g_setRTKKnobCalls, 1);
	check_eq("knob-edit-false: RTK idx=knobIndex", g_lastSetRTKIdx, 0);
	check_eq("knob-edit-false: RTK curved value (b=9, low branch)", g_lastSetRTKValue, 126);

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

	/* --- fixed 1-7: JoystickX real body, exact center, both Filter calls
	 * false -> no MIDI send. --- */
	ResetFixtures(); ResetCounters();
	self.AnalogControllerHandler(1, 0x10, 0x200);
	check_eq("joystickX-center: echo=b (STGAPI+0xfe, confirmed real -- see HARDWARE_REVIEW_LOG.md)",
		 *(unsigned short *)(g_frontPanelBuf + STGAPI_OFF_ANALOG_ECHO_VECX), 0x200);
	check_eq("joystickX-center: Filter calls (both attempts, both false)", g_pitchBendFilterCalls, 2);
	check_eq("joystickX-center: Filter arg=0x2000 (exact center)", g_lastPitchBendFilterValue, 0x2000);
	check_eq("joystickX-center: no MIDI send", g_midiWriteCalls, 0);

	/* --- JoystickX locked: plain return, no echo, no Filter calls. --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x14] = 1; g_rtdBuf[0x15] = 1; g_rtdBuf[0x16] = 1; /* table[1]=1 -> &1 nonzero -> locked */
	self.AnalogJoystickXHandler(0, 0x200);
	check_eq("joystickX-locked: no echo write", *(unsigned short *)(g_frontPanelBuf + STGAPI_OFF_ANALOG_ECHO_VECX), 0);
	check_eq("joystickX-locked: no Filter calls", g_pitchBendFilterCalls, 0);

	/* --- JoystickX far-from-center curve, first Filter() true -> sends
	 * immediate, then second Filter() also true -> sends again. --- */
	ResetFixtures(); ResetCounters();
	g_pitchBendFilterReturn = true;
	g_pitchBendFilterChannel = 3;
	g_pitchBendFilterOutValue = 0x1234;
	self.AnalogJoystickXHandler(0, 0);
	check_eq("joystickX-far: Filter calls (both true)", g_pitchBendFilterCalls, 2);
	check_eq("joystickX-far: MIDI sends (both branches fire)", g_midiWriteCalls, 2);
	check_eq("joystickX-far: msg[0]=0xe0|channel", g_lastMidiMsg[0], 0xe0 | 3);
	check_eq("joystickX-far: msg[1]=value&0x7f", g_lastMidiMsg[1], 0x1234 & 0x7f);
	check_eq("joystickX-far: msg[2]=(value>>7)&0x7f", g_lastMidiMsg[2], (0x1234 >> 7) & 0x7f);
	check_eq("joystickX-far: msg[3]=1", g_lastMidiMsg[3], 1);
	check_eq("joystickX-far: msg[4]=0xff (2-data-byte terminator)", g_lastMidiMsg[4], 0xff);
	check_eq("joystickX-far: msg len=5", (long)g_lastMidiLen, 5);

	/* --- JoystickX near-center linear branch (dist<=0x200): b=0x300,
	 * dist=0x3ff-0x300=0xff, curved=(0xff<<4)&0xfff0=0xff0. --- */
	ResetFixtures(); ResetCounters();
	g_pitchBendFilterReturn = false;
	self.AnalogJoystickXHandler(0, 0x300);
	check_eq("joystickX-nearcenter: Filter arg=(dist<<4)&0xfff0", g_lastPitchBendFilterValue, 0xff0);

	/* --- fixed 1-7 busy (dispatcher-level, unrelated to AnalogJoystickXHandler's
	 * own internal STGAPI+0xfe echo above): device 1-7 busy&8 path echoes to
	 * the SEPARATE per-device JOYX slot and returns before ever reaching the
	 * real per-device handler. --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 0x8;
	self.AnalogControllerHandler(1, 0, 0x2ab);
	check_eq("joystickX-busy: echo value", *(unsigned short *)(g_frontPanelBuf + STGAPI_OFF_ANALOG_ECHO_JOYX), 0x2ab);
	check_eq("joystickX-busy: no real handler call", g_pitchBendFilterCalls, 0);

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

	/* --- AnalogAftertouchHandler: not locked, sends Channel Pressure. --- */
	ResetFixtures(); ResetCounters();
	g_globalBuf[0x6b9] = 5; /* MIDI channel */
	self.AnalogAftertouchHandler(0x40, 0x2ab);
	check_eq("aftertouch-unlocked: rtd[0x1c]=(u8)a (always)", g_rtdBuf[0x1c], 0x40);
	check_eq("aftertouch-unlocked: rtd[0x1f]=(u8)a (not-locked path)", g_rtdBuf[0x1f], 0x40);
	check_eq("aftertouch-unlocked: MIDI send calls", g_midiWriteCalls, 1);
	check_eq("aftertouch-unlocked: msg[0]=0xd0|channel", g_lastMidiMsg[0], 0xd0 | 5);
	check_eq("aftertouch-unlocked: msg[1]=(u8)a", g_lastMidiMsg[1], 0x40);
	check_eq("aftertouch-unlocked: msg[3]=1", g_lastMidiMsg[3], 1);
	check_eq("aftertouch-unlocked: msg[4]=0xff (2-data-byte terminator)", g_lastMidiMsg[4], 0xff);
	check_eq("aftertouch-unlocked: echo=b(raw)",
		 *(unsigned short *)(g_frontPanelBuf + STGAPI_OFF_ANALOG_ECHO_ATOUCH), 0x2ab);

	/* --- AnalogAftertouchHandler: locked (bit 0x10) -- echo still
	 * happens, but no MIDI send, and rtd[0x1f] is NOT touched (only
	 * rtd[0x1c], which is unconditional). --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x14] = 10; g_rtdBuf[0x15] = 10; g_rtdBuf[0x16] = 10; /* table[10]=0x10 -> &0x10 nonzero -> locked */
	self.AnalogAftertouchHandler(0x40, 0x2ab);
	check_eq("aftertouch-locked: rtd[0x1c] still set", g_rtdBuf[0x1c], 0x40);
	check_eq("aftertouch-locked: rtd[0x1f] NOT set", g_rtdBuf[0x1f], 0);
	check_eq("aftertouch-locked: no MIDI send", g_midiWriteCalls, 0);
	check_eq("aftertouch-locked: echo still happens",
		 *(unsigned short *)(g_frontPanelBuf + STGAPI_OFF_ANALOG_ECHO_ATOUCH), 0x2ab);

	/* --- AnalogKnobExtHandler direct: busy2 SET, CC assigned in-range. --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2; /* busy2 */
	g_globalBuf[0x29cc0c8] = 1; /* active ext set = 1 */
	g_globalBuf[0x29ca3c8 + 1 * 8 + 3] = 20; /* knob idx=3, set=1 -> assigned CC 20 */
	CSTGCCInfo::sCCInfoTable[20 * 10] = 0x55;
	self.AnalogKnobExtHandler(3, 0, 0);
	check_eq("knobExt-busy2: UpdateStatus calls", g_updateStatusCalls, 1);
	check_eq("knobExt-busy2: Ext event calls", g_extKnobEventCalls, 1);
	check_eq("knobExt-busy2: idx=3", (long)g_lastKnobIndex, 3);
	check_eq("knobExt-busy2: value=ccDefault", (long)g_lastKnobValue, 0x55);
	check_eq("knobExt-busy2: jumpcatch array target (+0x54+idx*3+2) stored",
		 g_rtdBuf[0x54 + 3 * 3 + 2], 0x55);

	/* --- AnalogKnobExtHandler direct: busy2 SET, CC unassigned (0xff)
	 * -> silent return. --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	g_globalBuf[0x29ca3c8 + 3] = 0xff; /* set=0, idx=3 */
	self.AnalogKnobExtHandler(3, 0, 0);
	check_eq("knobExt-busy2-unassigned: no Ext event", g_extKnobEventCalls, 0);
	check_eq("knobExt-busy2-unassigned: no UpdateStatus", g_updateStatusCalls, 0);

	/* --- AnalogKnobExtHandler direct: busy2 CLEAR, CheckPosition false
	 * -> no event. --- */
	ResetFixtures(); ResetCounters();
	g_checkPositionReturn = false;
	self.AnalogKnobExtHandler(2, 0x30, 0);
	check_eq("knobExt-clear-false: CheckPosition calls", g_checkPositionCalls, 1);
	check_eq("knobExt-clear-false: no Ext event", g_extKnobEventCalls, 0);

	/* --- AnalogSliderExtHandler direct: busy2 SET, CC assigned. --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	g_globalBuf[0x29cc0c8] = 0;
	g_globalBuf[0x29cbc48 + 0 * 9 + 2] = 30; /* slider idx=2, set=0 -> CC 30 */
	CSTGCCInfo::sCCInfoTable[30 * 10] = 0x22;
	self.AnalogSliderExtHandler(2, 0, 0);
	check_eq("sliderExt-busy2: SendExtModeSliderEvent calls", g_sliderEventCalls, 1);
	check_eq("sliderExt-busy2: fader=idx", g_lastFader, 2);
	check_eq("sliderExt-busy2: value=ccDefault", (long)g_lastSliderValue, 0x22);
	check_eq("sliderExt-busy2: jumpcatch array target (+0x6c+idx*3+2) stored",
		 g_rtdBuf[0x6c + 2 * 3 + 2], 0x22);

	/* --- AnalogSliderExtHandler direct: busy2 CLEAR, CheckPosition true. --- */
	ResetFixtures(); ResetCounters();
	g_checkPositionReturn = true;
	self.AnalogSliderExtHandler(1, 0x60, 0);
	check_eq("sliderExt-clear-true: SendExtModeSliderEvent calls", g_sliderEventCalls, 1);
	check_eq("sliderExt-clear-true: value=a(raw)", (long)g_lastSliderValue, 0x60);

	/* --- AnalogKnobRTKHandler direct: busy2 SET -> plain
	 * ResetRTKModeKnob(idx), no curve computed at all. --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	self.AnalogKnobRTKHandler(5, 0, 0x999 /* would be a huge, invalid curve input if used */);
	check_eq("knobRTK-busy2: ResetRTKModeKnob calls", g_resetRTKKnobCalls, 1);
	check_eq("knobRTK-busy2: idx", g_lastResetRTKIdx, 5);
	check_eq("knobRTK-busy2: no CheckPosition (curve never computed)", g_checkPositionCalls, 0);
	check_eq("knobRTK-busy2: no SetRTKModeKnob", g_setRTKKnobCalls, 0);

	/* --- AnalogKnobRTKHandler direct: busy2 CLEAR, mid dead-zone
	 * (b=500, in [482,542]) -> curved=0x40, CheckPosition false -> no send. --- */
	ResetFixtures(); ResetCounters();
	g_checkPositionReturn = false;
	self.AnalogKnobRTKHandler(0, 0, 500);
	check_eq("knobRTK-clear-midband: CheckPosition arg=0x40 (dead zone)", g_lastPosition, 0x40);
	check_eq("knobRTK-clear-midband-false: no SetRTKModeKnob", g_setRTKKnobCalls, 0);

	/* --- AnalogKnobRTKHandler direct: busy2 CLEAR, high branch
	 * (b=1023 -> curved=floor((1024-1023)*0.13257262110710144)=0),
	 * CheckPosition true -> SetRTKModeKnob(idx, curved). --- */
	ResetFixtures(); ResetCounters();
	g_checkPositionReturn = true;
	g_globalBuf[0x6af] = 1;
	self.AnalogKnobRTKHandler(7, 0, 1023);
	check_eq("knobRTK-clear-high: CheckPosition flag=Global[0x6af]!=0", g_lastFlag, true);
	check_eq("knobRTK-clear-high: CheckPosition arg=0 (high branch)", g_lastPosition, 0);
	check_eq("knobRTK-clear-high: SetRTKModeKnob calls", g_setRTKKnobCalls, 1);
	check_eq("knobRTK-clear-high: idx", g_lastSetRTKIdx, 7);
	check_eq("knobRTK-clear-high: value=curved", g_lastSetRTKValue, 0);

	/* --- AnalogSliderRTKHandler direct: busy2 SET -> writes curve into
	 * jumpcatch array +1, UpdateStatus, SendKarmaCCToKG with FIXED 0x40
	 * (NOT the curve value). --- */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	self.AnalogSliderRTKHandler(4, 0, 500 /* mid dead-zone -> curved=0x40 */);
	check_eq("sliderRTK-busy2: array+1 stores curve", g_rtdBuf[0x6c + 4 * 3 + 1], 0x40);
	check_eq("sliderRTK-busy2: UpdateStatus calls", g_updateStatusCalls, 1);
	check_eq("sliderRTK-busy2: SendKarmaCCToKG calls", g_karmaCCCalls, 1);
	check_eq("sliderRTK-busy2: karmaCCNo=idx+0x14", g_lastKarmaCCNo, 4 + 0x14);
	check_eq("sliderRTK-busy2: value=FIXED 0x40 (not curve)", g_lastKarmaCCValue, 0x40);

	/* --- AnalogSliderRTKHandler direct: busy2 CLEAR, low branch
	 * (b=0 -> curved=127-floor(0*scale)=127), CheckPosition true ->
	 * SendKarmaCCToKG with the REAL curve value. --- */
	ResetFixtures(); ResetCounters();
	g_checkPositionReturn = true;
	self.AnalogSliderRTKHandler(6, 0, 0);
	check_eq("sliderRTK-clear-low: CheckPosition arg=127 (low branch, b=0)", g_lastPosition, 127);
	check_eq("sliderRTK-clear-low: SendKarmaCCToKG calls", g_karmaCCCalls, 1);
	check_eq("sliderRTK-clear-low: karmaCCNo=idx+0x14", g_lastKarmaCCNo, 6 + 0x14);
	check_eq("sliderRTK-clear-low: value=curve(127)", g_lastKarmaCCValue, 127);

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
