// SPDX-License-Identifier: GPL-2.0
/*
 * test_controller_info_button_handler.cpp  -  host-side known-answer
 * test for CSTGControllerInfo::ButtonPressHandler
 * (src/engine/controller_info_button_handler.cpp, batch 66).
 *
 * Links src/engine/controller_info_button_handler.cpp directly. Provides
 * its own local mocks/storage for every class this file touches --
 * matching test_controller_info_analog_handler.cpp's own established
 * "isolated single-TU test" convention.
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
static unsigned char g_globalBuf[0x29cd000];
static unsigned char g_midiPortMgrBuf[0x300];

CSTGControllerRTData *CSTGControllerRTData::sInstance = (CSTGControllerRTData *)g_rtdBuf;
CSTGGlobal *CSTGGlobal::sInstance = (CSTGGlobal *)g_globalBuf;
CSTGMidiPortManager *CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)g_midiPortMgrBuf;

static void ResetFixtures()
{
	memset(g_rtdBuf, 0, sizeof(g_rtdBuf));
	memset(g_globalBuf, 0, sizeof(g_globalBuf));
	memset(g_midiPortMgrBuf, 0, sizeof(g_midiPortMgrBuf));
}

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

/* ---- CSTGMidiQueueWriter::Write mock (SendGlobalMidiMessage idiom) ---- */
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

/* ---- HandleEditInContextButton mock ---- */
static int g_editCalls; static unsigned int g_lastEditCode; static bool g_lastEditPressed, g_editReturn;
bool CSTGControllerInfo::HandleEditInContextButton(unsigned int code, bool pressed)
{
	g_editCalls++;
	g_lastEditCode = code;
	g_lastEditPressed = pressed;
	return g_editReturn;
}

/* ---- ProcessMixerSwitchPress mock ---- */
static int g_mixerSwitchCalls; static unsigned int g_lastMixerCode; static bool g_lastMixerPressed;
void CSTGControllerInfo::ProcessMixerSwitchPress(unsigned int code, bool pressed)
{
	g_mixerSwitchCalls++;
	g_lastMixerCode = code;
	g_lastMixerPressed = pressed;
}

/* ---- SetMixerKnobMode / SetSoloSelected / ResetAllKnobCCs /
 * ResetAllExtModeControllers / NotifyParam(weak) mocks ---- */
static int g_setMixerKnobModeCalls; static int g_lastMixerKnobMode;
void CSTGControllerInfo::SetMixerKnobMode(int mode)
{
	g_setMixerKnobModeCalls++;
	g_lastMixerKnobMode = mode;
}
static int g_setSoloSelectedCalls; static bool g_lastSoloSelected;
void CSTGControllerInfo::SetSoloSelected(bool selected)
{
	g_setSoloSelectedCalls++;
	g_lastSoloSelected = selected;
}
static int g_resetAllKnobCCsCalls;
void CSTGControllerInfo::ResetAllKnobCCs() { g_resetAllKnobCCsCalls++; }
static int g_resetAllExtModeControllersCalls;
void CSTGControllerInfo::ResetAllExtModeControllers() { g_resetAllExtModeControllersCalls++; }
static int g_notifyParamCalls; static unsigned int g_lastNotifyParamId; static long g_lastNotifyParamValue;
void CSTGControllerInfo::NotifyParam(unsigned int paramId, long value)
{
	g_notifyParamCalls++;
	g_lastNotifyParamId = paramId;
	g_lastNotifyParamValue = value;
}

static void ResetCounters()
{
	g_unsolCalls = 0;
	g_writeCalls = 0;
	g_editCalls = 0; g_editReturn = false;
	g_mixerSwitchCalls = 0;
	g_setMixerKnobModeCalls = 0;
	g_setSoloSelectedCalls = 0;
	g_resetAllKnobCCsCalls = 0;
	g_resetAllExtModeControllersCalls = 0;
	g_notifyParamCalls = 0;
}

static unsigned int BitmapWord(unsigned int code)
{
	return *(unsigned int *)(g_rtdBuf + 0x30 + 4 * (code >> 5));
}

static void SetBitmapBit(unsigned int code)
{
	*(unsigned int *)(g_rtdBuf + 0x30 + 4 * (code >> 5)) |= (1u << (code & 0x1f));
}

int main()
{
	static unsigned char selfBuf[0x20];
	CSTGControllerInfo &self = *(CSTGControllerInfo *)selfBuf;
	memset(selfBuf, 0, sizeof(selfBuf));

	/* =================== edit-in-context gate =================== */
	ResetFixtures(); ResetCounters();
	*(unsigned int *)(g_globalBuf + 0x29cc4dc) = 1;
	g_editReturn = true;
	self.ButtonPressHandler(1, true);
	check_eq("edit-true: edit calls", g_editCalls, 1);
	check_eq("edit-true: no dispatch", g_unsolCalls, 0);

	ResetFixtures(); ResetCounters();
	*(unsigned int *)(g_globalBuf + 0x29cc4dc) = 1;
	g_editReturn = false;
	self.ButtonPressHandler(1, true);
	check_eq("edit-false: edit calls", g_editCalls, 1);
	check_eq("edit-false: falls through to dispatch", g_unsolCalls, 1);
	check_eq("edit-false: msgType", g_lastMsgType, 7);
	check_eq("edit-false: id", g_lastId, 8);

	/* =================== Pattern A: sample broadly =================== */
	static const struct { unsigned int code; int msgType, id; } patternA[] = {
		{1, 7, 8}, {7, 7, 14}, {8, 6, 2}, {10, 8, 1}, {21, 6, 5},
		{23, 6, 16}, {24, 9, 0}, {34, 9, 10}, {0x23, 9, 11}, {0x25, 9, 13},
		{0x2d, 10, 0}, {0x2e, 10, 1}, {0x33, 6, 0}, {0x34, 6, 1},
	};
	for (auto &e : patternA) {
		char label[64];
		ResetFixtures(); ResetCounters();
		self.ButtonPressHandler(e.code, true);
		snprintf(label, sizeof(label), "patternA press code=0x%x msgType", e.code);
		check_eq(label, g_lastMsgType, e.msgType);
		snprintf(label, sizeof(label), "patternA press code=0x%x id", e.code);
		check_eq(label, g_lastId, e.id);
		snprintf(label, sizeof(label), "patternA press code=0x%x value=0x7f", e.code);
		check_eq(label, g_lastValue, 0x7f);

		ResetCounters();
		self.ButtonPressHandler(e.code, false);
		snprintf(label, sizeof(label), "patternA release code=0x%x value=0", e.code);
		check_eq(label, g_lastValue, 0);
		snprintf(label, sizeof(label), "patternA release code=0x%x clears bit", e.code);
		check_eq(label, BitmapWord(e.code) & (1u << (e.code & 0x1f)), 0);
	}

	/* =================== Pattern B: gated (10 codes) =================== */
	static const struct { unsigned int code; int msgType, id; } patternB[] = {
		{0x26, 11, 5}, {0x27, 11, 4}, {0x28, 11, 3}, {0x29, 11, 2}, {0x2a, 11, 1},
		{0x2b, 11, 0}, {0x2f, 3, 0}, {0x30, 2, 0}, {0x31, 2, 1}, {0x32, 2, 3},
	};
	for (auto &e : patternB) {
		char label[64];

		/* busy flag set: real send + bit set */
		ResetFixtures(); ResetCounters();
		g_rtdBuf[0x2f] = 8;
		self.ButtonPressHandler(e.code, true);
		snprintf(label, sizeof(label), "patternB busy-set code=0x%x msgType", e.code);
		check_eq(label, g_lastMsgType, e.msgType);
		snprintf(label, sizeof(label), "patternB busy-set code=0x%x id", e.code);
		check_eq(label, g_lastId, e.id);
		snprintf(label, sizeof(label), "patternB busy-set code=0x%x bit set", e.code);
		check_eq(label, (BitmapWord(e.code) >> (e.code & 0x1f)) & 1, 1);

		/* busy flag clear: deferred, no send */
		ResetFixtures(); ResetCounters();
		self.ButtonPressHandler(e.code, true);
		snprintf(label, sizeof(label), "patternB busy-clear code=0x%x no send", e.code);
		check_eq(label, g_unsolCalls, 0);

		/* not pressed: deferred, no send */
		ResetFixtures(); ResetCounters();
		self.ButtonPressHandler(e.code, false);
		snprintf(label, sizeof(label), "patternB release code=0x%x no send", e.code);
		check_eq(label, g_unsolCalls, 0);
	}

	/* =================== Mixer-switch dispatch (0x3a-0x49) =================== */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x3a, true);
	check_eq("mixer-switch first: calls", g_mixerSwitchCalls, 1);
	check_eq("mixer-switch first: code", (long)g_lastMixerCode, 0x3a);
	check_eq("mixer-switch first: pressed", g_lastMixerPressed, true);

	ResetCounters();
	self.ButtonPressHandler(0x49, false);
	check_eq("mixer-switch last: calls", g_mixerSwitchCalls, 1);
	check_eq("mixer-switch last: code", (long)g_lastMixerCode, 0x49);
	check_eq("mixer-switch last: pressed", g_lastMixerPressed, false);
	check_eq("mixer-switch last: clears bit on release",
		 BitmapWord(0x49) & (1u << (0x49 & 0x1f)), 0);

	/* =================== 12 special buttons: press side =================== */

	/* code 9: busy2 clear -> sets busy(&8), sends(8,0,0x7f,1) */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(9, true);
	check_eq("special9 press: unsol calls", g_unsolCalls, 1);
	check_eq("special9 press: msgType/id/value", g_lastMsgType * 1000 + g_lastId, 8000);
	check_eq("special9 press: value", g_lastValue, 0x7f);
	check_eq("special9 press: busy flag set", g_rtdBuf[0x2f] & 8, 8);

	/* code 9: busy2 set -> no-op */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	self.ButtonPressHandler(9, true);
	check_eq("special9 press busy2: no-op", g_unsolCalls, 0);

	/* code 0x2c (44): busy flag set -> send(0xb,6,0x7f,1) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 8;
	self.ButtonPressHandler(0x2c, true);
	check_eq("special44 press: msgType", g_lastMsgType, 0xb);
	check_eq("special44 press: id", g_lastId, 6);

	/* code 0x2c: busy flag clear -> deferred, no send */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x2c, true);
	check_eq("special44 press busy-clear: no send", g_unsolCalls, 0);

	/* code 0x35 (53): busy2 set -> send(7,0,0x7f,1) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	self.ButtonPressHandler(0x35, true);
	check_eq("special53 press: msgType/id", g_lastMsgType * 10 + g_lastId, 70);

	/* code 0x36 (54): busy2 set -> send(7,1,0x7f,1) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	self.ButtonPressHandler(0x36, true);
	check_eq("special54 press: msgType/id", g_lastMsgType * 10 + g_lastId, 71);

	/* code 0x37 (55): busy clear, busy2 set, mode==4 -> ResetAllExtModeControllers() */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	g_rtdBuf[0x2b] = 4;
	self.ButtonPressHandler(0x37, true);
	check_eq("special55 press mode4: ResetAllExtModeControllers calls", g_resetAllExtModeControllersCalls, 1);
	check_eq("special55 press mode4: no unsol", g_unsolCalls, 0);

	/* code 0x37: mode != 4 -> real no-op */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	g_rtdBuf[0x2b] = 1;
	self.ButtonPressHandler(0x37, true);
	check_eq("special55 press mode1: no-op", g_resetAllExtModeControllersCalls, 0);

	/* code 0x37: busy set -> deferred */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 8;
	self.ButtonPressHandler(0x37, true);
	check_eq("special55 press busy-set: no-op observed", g_resetAllExtModeControllersCalls, 0);

	/* code 0x38 (56): busy clear, busy2 set, mode==5 -> full real body */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	g_rtdBuf[0x2b] = 5;
	g_globalBuf[0x6b9] = 3; /* channel */
	self.ButtonPressHandler(0x38, true);
	check_eq("special56 press mode5: unsol msgType/id", g_lastMsgType * 10 + g_lastId, 73);
	check_eq("special56 press mode5: ResetAllKnobCCs calls", g_resetAllKnobCCsCalls, 1);
	check_eq("special56 press mode5: midi write calls", g_writeCalls, 1);
	check_eq("special56 press mode5: midi status byte", g_lastMsg[0], (unsigned char)(3 | 0xb0));
	check_eq("special56 press mode5: midi data1", g_lastMsg[1], 0x79);
	check_eq("special56 press mode5: midi data2", g_lastMsg[2], 0x1c);
	check_eq("special56 press mode5: midi len byte", g_lastMsg[3], 5);
	check_eq("special56 press mode5: midi marker", g_lastMsg[4], 0xfe);
	check_eq("special56 press mode5: write len arg", (long)g_lastLen, 5);
	check_eq("special56 press mode5: write flag arg", g_lastFlag, false);

	/* code 0x39 (57): busy2 set -> send(7,4,0x7f,1) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	self.ButtonPressHandler(0x39, true);
	check_eq("special57 press: msgType/id", g_lastMsgType * 10 + g_lastId, 74);

	/* code 0x4a (74): mode==7, busy clear -> SetMixerKnobMode + NotifyParam(weak) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 7;
	selfBuf[4] = 0; /* field+4 == 0 -> newMode = true -> mode arg 1 */
	self.ButtonPressHandler(0x4a, true);
	check_eq("special74 press mode7 field0: SetMixerKnobMode calls", g_setMixerKnobModeCalls, 1);
	check_eq("special74 press mode7 field0: mode arg", g_lastMixerKnobMode, 1);
	check_eq("special74 press mode7 field0: NotifyParam calls", g_notifyParamCalls, 1);
	check_eq("special74 press mode7 field0: NotifyParam id", (long)g_lastNotifyParamId, 0);
	check_eq("special74 press mode7 field0: NotifyParam value", g_lastNotifyParamValue, 0);

	/* code 0x4a: field+4 != 0 -> newMode = false -> mode arg 0 */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 0; /* in-range: mode<=3 */
	selfBuf[4] = 5;
	self.ButtonPressHandler(0x4a, true);
	check_eq("special74 press mode0 field5: mode arg", g_lastMixerKnobMode, 0);
	check_eq("special74 press mode0 field5: NotifyParam value", g_lastNotifyParamValue, 5);

	/* code 0x4a: mode out of range (5) -> deferred, no dispatch */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 5;
	self.ButtonPressHandler(0x4a, true);
	check_eq("special74 press mode5 out-of-range: no dispatch", g_setMixerKnobModeCalls, 0);

	/* code 0x4a: busy flag set -> deferred even if mode in range */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 0;
	g_rtdBuf[0x2f] = 8;
	self.ButtonPressHandler(0x4a, true);
	check_eq("special74 press busy-set: no dispatch", g_setMixerKnobModeCalls, 0);

	/* code 0x4b (75): busy clear, Global[0x29cc4e8]!=0 -> send(8,2,0x7f,1) */
	ResetFixtures(); ResetCounters();
	g_globalBuf[0x29cc4e8] = 1;
	self.ButtonPressHandler(0x4b, true);
	check_eq("special75 press: msgType/id", g_lastMsgType * 10 + g_lastId, 82);

	/* code 0x4b: Global[0x29cc4e8]==0 -> deferred, no send */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x4b, true);
	check_eq("special75 press flag-clear: no send", g_unsolCalls, 0);

	/* code 0x4c (76): mode==0, busy clear, busy2 clear, rtd[0x21] even -> SetSoloSelected(true) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 0;
	g_rtdBuf[0x21] = 0;
	self.ButtonPressHandler(0x4c, true);
	check_eq("special76 press: SetSoloSelected calls", g_setSoloSelectedCalls, 1);
	check_eq("special76 press: selected arg", g_lastSoloSelected, true);
	check_eq("special76 press: NotifyParam id", (long)g_lastNotifyParamId, 1);
	check_eq("special76 press: NotifyParam value", g_lastNotifyParamValue, 1);

	/* code 0x4c: rtd[0x21] odd -> SetSoloSelected(false) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 0;
	g_rtdBuf[0x21] = 1;
	self.ButtonPressHandler(0x4c, true);
	check_eq("special76 press odd: selected arg", g_lastSoloSelected, false);

	/* code 0x4c: mode out of range -> real no-op */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 5;
	self.ButtonPressHandler(0x4c, true);
	check_eq("special76 press mode5: no-op", g_setSoloSelectedCalls, 0);

	/* code 0x4c: busy2 set -> deferred */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2b] = 0;
	g_rtdBuf[0x2f] = 2;
	self.ButtonPressHandler(0x4c, true);
	check_eq("special76 press busy2-set: no dispatch", g_setSoloSelectedCalls, 0);

	/* code 0x4d/0x4e (77/78): busy set -> send(1,id,0x7f,1) + set bit */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 8;
	self.ButtonPressHandler(0x4d, true);
	check_eq("special77 press: msgType/id", g_lastMsgType * 10 + g_lastId, 10);
	check_eq("special77 press: bit set", (BitmapWord(0x4d) >> (0x4d & 0x1f)) & 1, 1);

	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 8;
	self.ButtonPressHandler(0x4e, true);
	check_eq("special78 press: msgType/id", g_lastMsgType * 10 + g_lastId, 11);

	/* code 0x4d: busy clear -> deferred, no send */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x4d, true);
	check_eq("special77 press busy-clear: no send", g_unsolCalls, 0);

	/* out-of-range high code (>78): confirmed real no-op */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x50, true);
	check_eq("code80 press: no-op", g_unsolCalls, 0);
	self.ButtonPressHandler(0x50, false);
	check_eq("code80 release: no-op", g_unsolCalls, 0);

	/* code 0: always no-op regardless of pressed */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0, true);
	check_eq("code0 press: no-op", g_unsolCalls, 0);
	self.ButtonPressHandler(0, false);
	check_eq("code0 release: no-op", g_unsolCalls, 0);

	/* =================== 12 special buttons: release side =================== */

	/* code 9 release: clears busy(&8), sends(8,0,0,1), clears bit9 */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 8;
	SetBitmapBit(9);
	self.ButtonPressHandler(9, false);
	check_eq("special9 release: msgType/id", g_lastMsgType * 10 + g_lastId, 80);
	check_eq("special9 release: value=0", g_lastValue, 0);
	check_eq("special9 release: busy flag cleared", g_rtdBuf[0x2f] & 8, 0);
	check_eq("special9 release: bit9 cleared", (BitmapWord(9) >> 9) & 1, 0);

	/* code 0x2c (44) release: send(0xb,6,0,1), clears bit44 (harmless, never set) */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x2c, false);
	check_eq("special44 release: msgType/id", g_lastMsgType * 100 + g_lastId, 1106);

	/* code 0x35 (53) release: send(7,0,0,1) */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x35, false);
	check_eq("special53 release: msgType/id", g_lastMsgType * 10 + g_lastId, 70);

	/* code 0x36 (54) release: send(7,1,0,1) */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x36, false);
	check_eq("special54 release: msgType/id", g_lastMsgType * 10 + g_lastId, 71);

	/* code 0x37 (55) release: unconditional send(7,2,0,1) -- asymmetric with press */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x37, false);
	check_eq("special55 release: msgType/id", g_lastMsgType * 10 + g_lastId, 72);

	/* code 0x38 (56) release: bit set -> send(7,3,0,1), then bit cleared by generic tail */
	ResetFixtures(); ResetCounters();
	SetBitmapBit(0x38);
	self.ButtonPressHandler(0x38, false);
	check_eq("special56 release bit-set: msgType/id", g_lastMsgType * 10 + g_lastId, 73);
	check_eq("special56 release bit-set: bit cleared after", (BitmapWord(0x38) >> (0x38 & 0x1f)) & 1, 0);

	/* code 0x38 release: bit clear -> deferred, no send */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x38, false);
	check_eq("special56 release bit-clear: no send", g_unsolCalls, 0);

	/* code 0x39 (57) release: clears flags&4 (distinct from press's &2 gate), send(7,4,0,1) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 4;
	self.ButtonPressHandler(0x39, false);
	check_eq("special57 release: msgType/id", g_lastMsgType * 10 + g_lastId, 74);
	check_eq("special57 release: flags&4 cleared", g_rtdBuf[0x2f] & 4, 0);

	/* code 0x4a (74) release: unconditional send(7,5,0,1) -- asymmetric with press */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x4a, false);
	check_eq("special74 release: msgType/id", g_lastMsgType * 10 + g_lastId, 75);

	/* code 0x4b (75) release: clears busy2(&2) unconditionally; bit set -> send(8,2,0,1) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	SetBitmapBit(0x4b);
	self.ButtonPressHandler(0x4b, false);
	check_eq("special75 release bit-set: msgType/id", g_lastMsgType * 10 + g_lastId, 82);
	check_eq("special75 release bit-set: busy2 cleared", g_rtdBuf[0x2f] & 2, 0);

	/* code 0x4b release: bit clear -> deferred (busy2 still cleared first) */
	ResetFixtures(); ResetCounters();
	g_rtdBuf[0x2f] = 2;
	self.ButtonPressHandler(0x4b, false);
	check_eq("special75 release bit-clear: no send", g_unsolCalls, 0);
	check_eq("special75 release bit-clear: busy2 still cleared", g_rtdBuf[0x2f] & 2, 0);

	/* code 0x4c (76) release: unconditional send(7,6,0,1) -- asymmetric with press */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x4c, false);
	check_eq("special76 release: msgType/id", g_lastMsgType * 10 + g_lastId, 76);

	/* code 0x4d/0x4e (77/78) release: bit set -> send(1,id,0,1), bit cleared after */
	ResetFixtures(); ResetCounters();
	SetBitmapBit(0x4d);
	self.ButtonPressHandler(0x4d, false);
	check_eq("special77 release bit-set: msgType/id", g_lastMsgType * 10 + g_lastId, 10);
	check_eq("special77 release bit-set: bit cleared after", (BitmapWord(0x4d) >> (0x4d & 0x1f)) & 1, 0);

	ResetFixtures(); ResetCounters();
	SetBitmapBit(0x4e);
	self.ButtonPressHandler(0x4e, false);
	check_eq("special78 release bit-set: msgType/id", g_lastMsgType * 10 + g_lastId, 11);

	/* code 0x4d release: bit clear -> deferred, no send */
	ResetFixtures(); ResetCounters();
	self.ButtonPressHandler(0x4d, false);
	check_eq("special77 release bit-clear: no send", g_unsolCalls, 0);

	printf(g_fail ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
