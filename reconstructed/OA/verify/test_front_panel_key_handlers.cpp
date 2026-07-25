// SPDX-License-Identifier: GPL-2.0
/*
 * test_front_panel_key_handlers.cpp  -  host-side known-answer test for
 * CSTGFrontPanel::SetLED/SetLEDBlinking/ResetLED and HandleKeyOn/
 * HandleKeyOff (src/engine/front_panel_handlers.cpp, batch-63 candidates
 * 1-2 of the front-panel-driver un-triaged sweep).
 *
 * Links src/engine/front_panel_handlers.cpp directly. Provides its own
 * local mocks for CSTGKeybedInterface_sInstance()/::SetLED (a plain byte
 * buffer + a call-counting stub, NOT src/init/keybed_init.cpp's real
 * storage -- this test stays isolated, matching test_keybed_debounce.cpp's
 * own established convention for classes with heavy real-construction
 * side effects elsewhere), OmapNKS4OutputFifo_WriteCommand, and
 * CSTGMidiQueueWriter::Write.
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

/* ---- link-satisfying mocks ---- */

/* Static singleton storage -- none of these classes' own real
 * constructors/managers.cpp are linked here, matching test_keybed_
 * debounce.cpp's own established "own local storage, own plain
 * instance" convention for isolated single-TU tests. */
CPowerOffTimer *CPowerOffTimer::sInstance;
CSTGMidiPortManager *CSTGMidiPortManager::sInstance;
CSTGGlobal *CSTGGlobal::sInstance;
CSTGControllerRTData *CSTGControllerRTData::sInstance;
unsigned char *STGAPIFrontPanelStatus::sInstance;

static int g_writeCommandCalls;
static int g_lastCommand;
extern "C" int OmapNKS4OutputFifo_WriteCommand(int command)
{
	g_writeCommandCalls++;
	g_lastCommand = command;
	return 0;
}

static int g_kbSetLedCalls;
static unsigned int g_kbLastCode, g_kbLastAction;
void CSTGKeybedInterface::SetLED(unsigned int code, unsigned int action)
{
	g_kbSetLedCalls++;
	g_kbLastCode = code;
	g_kbLastAction = action;
}

static unsigned char g_keybedBuf[0x10];
extern "C" unsigned char *CSTGKeybedInterface_sInstance(void)
{
	return g_keybedBuf;
}

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

void CSTGControllerInfo::ButtonPressHandler(unsigned int, bool) {}
void CSTGControllerInfo::AnalogControllerHandler(unsigned int, unsigned short, unsigned short) {}
void CSTGControllerRTData::SendKarmaCCToKG(int, unsigned char) {}
extern "C" void PushUnsolicitedMessage(void *) {}

int main()
{
	static unsigned char powerOffTimerBuf[4];
	static unsigned char midiPortMgrBuf[0x300];
	static unsigned char globalBuf[0x700];
	static unsigned char rtdBuf[0x40];
	static unsigned char panelBuf[0x200];

	CPowerOffTimer::sInstance = (CPowerOffTimer *)powerOffTimerBuf;
	CSTGMidiPortManager::sInstance = (CSTGMidiPortManager *)midiPortMgrBuf;
	CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;
	CSTGControllerRTData::sInstance = (CSTGControllerRTData *)rtdBuf;
	STGAPIFrontPanelStatus::sInstance = panelBuf;

	static unsigned char frontPanelBuf[0x120];
	memset(frontPanelBuf, 0, sizeof(frontPanelBuf));
	for (unsigned int i = 0; i < 0x80; i++)
		frontPanelBuf[4 + i] = (unsigned char)i; /* CSTGFrontPanel::Initialize()'s
							   * own confirmed identity-mapped
							   * table shape */
	CSTGFrontPanel *fp = (CSTGFrontPanel *)frontPanelBuf;

	printf("== SetLED/SetLEDBlinking/ResetLED ==\n");

	printf("[1] out-of-range code (0x30) forwards a packed command word to OmapNKS4OutputFifo_WriteCommand\n");
	*(unsigned char *)CPowerOffTimer::sInstance = 0; /* SetLED doesn't touch this */
	g_writeCommandCalls = 0;
	fp->SetLED(0x1234);
	/* packed = ((0x34)<<8) | (0x12) = 0x3412, OR 0x1500000 */
	check_eq("SetLED(0x1234) calls WriteCommand once", g_writeCommandCalls, 1);
	check_eq("SetLED(0x1234) command word", g_lastCommand, (long)(0x1500000 | 0x3412));

	g_writeCommandCalls = 0;
	fp->SetLEDBlinking(0x1234);
	check_eq("SetLEDBlinking(0x1234) command word", g_lastCommand, (long)(0x1510000 | 0x3412));

	g_writeCommandCalls = 0;
	fp->ResetLED(0x1234);
	check_eq("ResetLED(0x1234) command word", g_lastCommand, (long)(0x1520000 | 0x3412));

	printf("[2] in-range code (0x49/0x4a) forwards to CSTGKeybedInterface::SetLED with a fixed action, code unchanged\n");
	g_kbSetLedCalls = 0;
	g_writeCommandCalls = 0;
	fp->SetLED(0x49);
	check_eq("SetLED(0x49) does NOT call WriteCommand", g_writeCommandCalls, 0);
	check_eq("SetLED(0x49) calls CSTGKeybedInterface::SetLED once", g_kbSetLedCalls, 1);
	check_eq("SetLED(0x49) passes code unchanged", (long)g_kbLastCode, 0x49);
	check_eq("SetLED(0x49) action == 1", (long)g_kbLastAction, 1);

	g_kbSetLedCalls = 0;
	fp->SetLEDBlinking(0x4a);
	check_eq("SetLEDBlinking(0x4a) passes code unchanged", (long)g_kbLastCode, 0x4a);
	check_eq("SetLEDBlinking(0x4a) action == 2", (long)g_kbLastAction, 2);

	g_kbSetLedCalls = 0;
	fp->ResetLED(0x49);
	check_eq("ResetLED(0x49) action == 0", (long)g_kbLastAction, 0);

	printf("== HandleKeyOn/HandleKeyOff ==\n");

	printf("[3] MIDI-port-manager not-ready gate: HandleKeyOn/Off no-op except CPowerOffTimer flag\n");
	midiPortMgrBuf[0] = 0; /* not ready */
	*(unsigned char *)CPowerOffTimer::sInstance = 0;
	g_writeCalls = 0;
	fp->HandleKeyOn(10, 100);
	check_eq("gated HandleKeyOn sets CPowerOffTimer flag", powerOffTimerBuf[0], 1);
	check_eq("gated HandleKeyOn sends no MIDI message", g_writeCalls, 0);
	check_eq("gated HandleKeyOn leaves per-key table untouched", frontPanelBuf[4 + 10], 10);

	midiPortMgrBuf[0] = 1; /* ready, for the rest of this test */

	printf("[4] in-range note: keyNum(60) + transpose sum(rtd[0x28]+[0x29]+[0x2a]=0) -> note 60, channel from CSTGGlobal+0x6b9\n");
	memset(rtdBuf, 0, sizeof(rtdBuf));
	globalBuf[0x6b9] = 3; /* MIDI channel 3 */
	g_writeCalls = 0;
	fp->HandleKeyOn(60, 100);
	check_eq("HandleKeyOn(60,100) sends one MIDI message", g_writeCalls, 1);
	check_eq("HandleKeyOn message length", (long)g_lastLen, 5);
	check_eq("HandleKeyOn message[0] = channel|0x90", g_lastMsg[0], 0x90 | 3);
	check_eq("HandleKeyOn message[1] = note", g_lastMsg[1], 60);
	check_eq("HandleKeyOn message[2] = velocity", g_lastMsg[2], 100);
	check_eq("HandleKeyOn message[3] = 1", g_lastMsg[3], 1);
	check_eq("HandleKeyOn message[4] = 0xfe", g_lastMsg[4], 0xfe);
	check_eq("HandleKeyOn records note in per-key table +4", frontPanelBuf[4 + 60], 60);
	check_eq("HandleKeyOn records channel in per-key table +0x84", frontPanelBuf[0x84 + 60], 3);
	check_eq("HandleKeyOn echoes note to STGAPI_OFF_MIDI_ECHO0", panelBuf[STGAPI_OFF_MIDI_ECHO0], 60);
	check_eq("HandleKeyOn echoes velocity to STGAPI_OFF_MIDI_ECHO1", panelBuf[STGAPI_OFF_MIDI_ECHO1], 100);

	printf("[5] high overflow fold: keyNum(127) + transpose(+10) = 137 -> folds into [0x74..0x7f]\n");
	memset(rtdBuf, 0, sizeof(rtdBuf));
	rtdBuf[0x28] = 10; /* transpose sum contributes +10 */
	g_writeCalls = 0;
	fp->HandleKeyOn(127, 64);
	/* noteRaw = 137; v = 129; q = 10; r = 129-120=9; final = 9+0x74 = 0x7d = 125 */
	check_eq("HandleKeyOn(127,+10) folds to 125", g_lastMsg[1], 125);

	printf("[6] low underflow fold, non-multiple-of-12: keyNum(0) + transpose(-13) = -13 -> sum+12 = -1 (low byte 0xff)\n");
	memset(rtdBuf, 0, sizeof(rtdBuf));
	rtdBuf[0x28] = (unsigned char)-13;
	g_writeCalls = 0;
	fp->HandleKeyOn(0, 64);
	check_eq("HandleKeyOn(0,-13) low byte", g_lastMsg[1], (unsigned char)-1);

	printf("[7] low underflow fold, exact multiple of 12: keyNum(0) + transpose(-24) = -24 -> 0\n");
	memset(rtdBuf, 0, sizeof(rtdBuf));
	rtdBuf[0x28] = (unsigned char)-24;
	g_writeCalls = 0;
	fp->HandleKeyOn(0, 64);
	check_eq("HandleKeyOn(0,-24) exact-multiple-of-12 -> note 0", g_lastMsg[1], 0);

	printf("[8] HandleKeyOff reads back the note/channel HandleKeyOn recorded, sends Note-Off\n");
	memset(rtdBuf, 0, sizeof(rtdBuf));
	globalBuf[0x6b9] = 5;
	fp->HandleKeyOn(70, 90);
	g_writeCalls = 0;
	fp->HandleKeyOff(70, 0);
	check_eq("HandleKeyOff sends one MIDI message", g_writeCalls, 1);
	check_eq("HandleKeyOff message[0] = storedChannel|0x80", g_lastMsg[0], 0x80 | 5);
	check_eq("HandleKeyOff message[1] = storedNote", g_lastMsg[1], 70);
	check_eq("HandleKeyOff message[2] = velocity passthrough", g_lastMsg[2], 0);
	check_eq("HandleKeyOff message[3] = 1", g_lastMsg[3], 1);
	check_eq("HandleKeyOff message[4] = 0xfe", g_lastMsg[4], 0xfe);

	printf("[9] HandleKeyOff gate: not-ready MIDI port manager -> only CPowerOffTimer flag set\n");
	midiPortMgrBuf[0] = 0;
	*(unsigned char *)CPowerOffTimer::sInstance = 0;
	g_writeCalls = 0;
	fp->HandleKeyOff(70, 5);
	check_eq("gated HandleKeyOff sets CPowerOffTimer flag", powerOffTimerBuf[0], 1);
	check_eq("gated HandleKeyOff sends no MIDI message", g_writeCalls, 0);

	printf("== Beep/SetLED16Bits ==\n");

	printf("[10] Beep() ignores `this`, always sends the fixed 0x04000000 command\n");
	g_writeCommandCalls = 0;
	fp->Beep();
	check_eq("Beep() calls WriteCommand once", g_writeCommandCalls, 1);
	check_eq("Beep() command word", g_lastCommand, 0x04000000);

	printf("[11] SetLED16Bits(m) drops byte1 (bits 8-15), rearranges byte0/byte2/byte3\n");
	g_writeCommandCalls = 0;
	fp->SetLED16Bits(0x12345678u);
	/* byte0=0x78 byte1=0x56(dropped) byte2=0x34 byte3=0x12
	 * cmd = 0x05000000 | (0x78<<16) | (0x34<<8) | 0x12 */
	check_eq("SetLED16Bits calls WriteCommand once", g_writeCommandCalls, 1);
	check_eq("SetLED16Bits(0x12345678) command word", g_lastCommand,
		 (long)(0x05000000 | (0x78 << 16) | (0x34 << 8) | 0x12));

	g_writeCommandCalls = 0;
	fp->SetLED16Bits(0x0000ffffu); /* plain 16-bit mask, byte0=0xff byte1=0xff(dropped) */
	check_eq("SetLED16Bits(0xffff) command word", g_lastCommand,
		 (long)(0x05000000 | (0xff << 16)));

	if (g_fail) {
		printf("\n%d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
