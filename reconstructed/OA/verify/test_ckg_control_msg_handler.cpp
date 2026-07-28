// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_control_msg_handler.cpp  -  KAT for CKGControlMsgHandler
 * (see ../src/engine/ckg_control_msg_handler.cpp).
 */

#include <cstdio>
#include <cstring>
#include "oa_ckg_control_ui_msg.h"

/* host-only mock storage for every out-of-scope singleton/dependency */
unsigned char *CKGEngine::ms_poInstance;
CKGParamEdit *CKGEngine::ms_poKGParamEdit;
unsigned char *CKGBankManager::ms_poInstance;
unsigned char *CKGUIMsgProcessor::ms_poInstance;
void CKGEngine::ResetLocalController() {}

static int g_calls;
static char g_lastCall[64];
static long g_lastArg1, g_lastArg2;
static void mark(const char *name, long a1 = 0, long a2 = 0)
{
	g_calls++;
	strncpy(g_lastCall, name, sizeof(g_lastCall) - 1);
	g_lastCall[sizeof(g_lastCall) - 1] = 0;
	g_lastArg1 = a1;
	g_lastArg2 = a2;
}

void CKGEngine::WritePerformance() { mark("WritePerformance"); }
void CKGEngine::DoCurrentDump() { mark("DoCurrentDump"); }
void CKGEngine::DoCompare() { mark("DoCompare"); }
void CKGEngine::ChangePerformance(eSTGMsgPerfType type, bool b) { mark("ChangePerformance", type, b); }
void CKGEngine::UpdateGEInfo(int a) { mark("CKGEngine::UpdateGEInfo", a); }
void CKGEngine::SendShutUp() { mark("SendShutUp"); }
void CKGEngine::UpdateUserGE(int a, int b) { mark("UpdateUserGE", a, b); }
void CKGEngine::DoInitModule(int a) { mark("DoInitModule", a); }
void CKGEngine::DoAutoAssignRTName(int a) { mark("DoAutoAssignRTName", a); }
void CKGEngine::DoRandomCapture(long a) { mark("DoRandomCapture", a); }
void CKGEngine::DoClearRTCSetup(long a) { mark("DoClearRTCSetup", a); }
void CKGEngine::DoAutoRTCSetup(long a) { mark("DoAutoRTCSetup", a); }
void CKGEngine::OpenGECategoryPopup() { mark("OpenGECategoryPopup"); }
void CKGEngine::CloseGECategoryPopup(bool b) { mark("CloseGECategoryPopup", b); }
void CKGEngine::UpdateRTCDisplay(int a) { mark("UpdateRTCDisplay", a); }
void CKGEngine::UpdateRTCModelName(int a) { mark("UpdateRTCModelName", a); }

void CKGBankManager::ChangeMode(eSTGMsgPerfType t) { mark("ChangeMode", t); }
void CKGBankManager::FinishLoadingGEsAndTemplates(int a, int b) { mark("BM::FinishLoadingGEsAndTemplates", a, b); }
static int g_setupTemplateCalls[64], g_setupTemplateCount;
void CKGBankManager::SetupTemplateAfterLoading(int i) { g_setupTemplateCalls[g_setupTemplateCount++] = i; }

/* CKGRTCHandler::ms_poInstance, CKGMIDIMsgProcessor::ms_poInstance,
 * CSKMIDIMsgProcessor::ms_poInstance,
 * CSKMIDIInMsgHandler::ms_bShouldStopSendingNoteOnsToSTG, and
 * CMIDIFlowParamHolder::ms_poThis are all defined by
 * ckg_control_msg_handler.cpp itself (linked in below) -- not redefined
 * here. */
void CKGRTCHandler::ChangePerformance() { mark("RTC::ChangePerformance"); }
void CKGRTCHandler::ResetAllScene() { mark("RTC::ResetAllScene"); }
void CKGMIDIMsgProcessor::ResetKarmaGeneratedCCValue() { mark("ResetKarmaGeneratedCCValue"); }
void CSKMIDIMsgProcessor::ProcessLocalControlChannelMessage(int status, unsigned char ch, char a, char /*b*/)
{
	mark("ProcessLocalControlChannelMessage", status, (ch << 16) | (unsigned char)a);
}
void CMIDIFlowParamHolder::Start() { mark("MIDIFlow::Start"); }

void CKGParamEdit::SendNoteMapOctaveReplicate(bool b) { mark("SendNoteMapOctaveReplicate", b); }
void CKGParamEdit::SendNoteMapTableReset(bool b) { mark("SendNoteMapTableReset", b); }
void CKGParamEdit::SendForceKarmaOff(bool b) { mark("SendForceKarmaOff", b); }

extern "C" void KGOutGate_StopSendingToMIDIPort(bool stop) { mark("StopSendingToMIDIPort", stop); }
extern "C" void SPRMain_SetAllKARMAAndDrumTrack(bool en) { mark("SetAllKARMAAndDrumTrack", en); }

/* CKGUIMsgSender is stateless, so its own real .cpp can link in directly
 * without any further mocking beyond what it itself needs. */
extern "C" void KGOutGate_SendMessageToUI(const CSKMessage *, bool) { mark("SendMessageToUI"); }
/* CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange is
 * defined by ckg_ui_msg_sender.cpp itself (linked in below). */

static int g_fail;
static void check(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-40s %ld\n", label, got); return; }
	printf("  FAIL  %-40s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}
static void checkStr(const char *label, const char *got, const char *want)
{
	if (strcmp(got, want) == 0) { printf("  ok    %-40s %s\n", label, got); return; }
	printf("  FAIL  %-40s got=%s want=%s\n", label, got, want);
	g_fail++;
}

#define ENGSZ 0x200
/* CKGBankManager::ms_poInstance is a real ~9.9MB aggregate pointer
 * (SetSendingBulkDump/SharedMem* touch offsets up to 0x97c7bc, see
 * oa_engine_init.h's own header comment) -- sized generously here. */
#define BANKSZ (10 * 1024 * 1024)
static unsigned char g_engine[ENGSZ], g_rtc[ENGSZ], g_kgmidi[ENGSZ], g_skmidi[ENGSZ], g_pe[ENGSZ];
static unsigned char g_bank[BANKSZ];

int main(void)
{
	printf("CKGControlMsgHandler known-answer test\n");
	printf("========================================================================\n");

	memset(g_engine, 0, ENGSZ);
	CKGEngine::ms_poInstance = g_engine;
	CKGEngine::ms_poKGParamEdit = (CKGParamEdit *)g_pe;
	CKGBankManager::ms_poInstance = g_bank;
	CKGRTCHandler::ms_poInstance = g_rtc;
	CKGMIDIMsgProcessor::ms_poInstance = g_kgmidi;
	CSKMIDIMsgProcessor::ms_poInstance = g_skmidi;
	CMIDIFlowParamHolder::ms_poThis = (unsigned char *)1; /* nonnull, unused fields */
	CKGUIMsgProcessor::ms_poInstance = g_engine; /* +0x5c must stay in-bounds */

	CKGControlMsgHandler h;
	check("ctor: guards zeroed", CKGControlMsgHandler::ms_bIsNowDumpingSong, false);

	/* --- ChangeProgram/ChangeCombi/ChangeSong: 0xffff sentinel gate --- */
	CKGControlMsg msg;
	msg.m_mode = 0; msg.m_value = 0x1234;
	g_calls = 0;
	h.ChangeProgram(&msg);
	check("ChangeProgram: non-sentinel is a no-op", g_calls, 0);

	msg.m_value = 0xffff;
	g_calls = 0;
	h.ChangeProgram(&msg);
	check("ChangeProgram: call count", g_calls, 2);
	checkStr("ChangeProgram: 2nd call", g_lastCall, "ChangePerformance");
	check("ChangeProgram: type=Program(1)", g_lastArg1, eSTGMsgPerfType_Program);
	check("ChangeProgram: bool=true", g_lastArg2, 1);
	check("ChangeProgram: guard restored", CKGControlMsgHandler::ms_bIsNowDumpingProg, false);

	g_calls = 0;
	h.ChangeCombi(&msg);
	check("ChangeCombi: call count", g_calls, 2);
	check("ChangeCombi: type=Combi(0)", g_lastArg1, eSTGMsgPerfType_Combi);
	check("ChangeCombi: bool=false", g_lastArg2, 0);

	g_calls = 0;
	h.ChangeSong(&msg);
	check("ChangeSong: call count (no ResetKarmaGeneratedCCValue)", g_calls, 1);
	check("ChangeSong: type=Song(2)", g_lastArg1, eSTGMsgPerfType_Song);

	/* --- SetMode: raw 0/1/2 -> table {1,0,2} -- swap of 0/1 --- */
	msg.m_mode = 0;
	h.SetMode(&msg);
	checkStr("SetMode(0)", g_lastCall, "ChangeMode");
	check("SetMode(0) -> Program(1)", g_lastArg1, eSTGMsgPerfType_Program);
	msg.m_mode = 1;
	h.SetMode(&msg);
	check("SetMode(1) -> Combi(0)", g_lastArg1, eSTGMsgPerfType_Combi);
	msg.m_mode = 2;
	h.SetMode(&msg);
	check("SetMode(2) -> Song(2)", g_lastArg1, eSTGMsgPerfType_Song);
	g_calls = 0;
	msg.m_mode = 3;
	h.SetMode(&msg);
	check("SetMode(3): out of range, no-op", g_calls, 0);

	/* --- Start/Stop --- */
	h.Start(&msg);
	check("Start: engine+0xb0=0", g_engine[0xb0], 0);
	h.Stop(&msg);
	check("Stop: engine+0xb0=1", g_engine[0xb0], 1);

	/* --- NoteMapTableOctaveReplicate / ResetNoteMapTable --- */
	msg.m_mode = 0; msg.m_value = 5;
	h.NoteMapTableOctaveReplicate(&msg);
	checkStr("NoteMapTableOctaveReplicate call", g_lastCall, "SendNoteMapOctaveReplicate");
	check("NoteMapTableOctaveReplicate: mode==0 -> false", g_lastArg1, 0);
	h.ResetNoteMapTable(&msg);
	checkStr("ResetNoteMapTable call", g_lastCall, "SendNoteMapTableReset");
	check("ResetNoteMapTable: value!=0 -> true", g_lastArg1, 1);

	/* --- NotifyUIOperation: 3-way switch --- */
	msg.m_mode = 0; h.NotifyUIOperation(&msg); checkStr("NotifyUIOperation(0)", g_lastCall, "WritePerformance");
	msg.m_mode = 1; h.NotifyUIOperation(&msg); checkStr("NotifyUIOperation(1)", g_lastCall, "DoCompare");
	msg.m_mode = 2; h.NotifyUIOperation(&msg); checkStr("NotifyUIOperation(2)", g_lastCall, "DoCurrentDump");
	g_calls = 0; msg.m_mode = 3; h.NotifyUIOperation(&msg);
	check("NotifyUIOperation(3): no-op", g_calls, 0);

	/* --- ExecPageMenuCommand: real (non-address-order) table mapping --- */
	msg.m_mode = 0; msg.m_value = 0x11; h.ExecPageMenuCommand(&msg);
	checkStr("ExecPageMenuCommand(0)", g_lastCall, "DoAutoRTCSetup"); check("  arg", g_lastArg1, 0x11);
	msg.m_mode = 1; msg.m_value = 0x22; h.ExecPageMenuCommand(&msg);
	checkStr("ExecPageMenuCommand(1)", g_lastCall, "DoClearRTCSetup"); check("  arg", g_lastArg1, 0x22);
	msg.m_mode = 2; msg.m_value = 0x33; h.ExecPageMenuCommand(&msg);
	checkStr("ExecPageMenuCommand(2)", g_lastCall, "DoRandomCapture"); check("  arg", g_lastArg1, 0x33);
	msg.m_mode = 3; msg.m_value = 0x44; h.ExecPageMenuCommand(&msg);
	checkStr("ExecPageMenuCommand(3)", g_lastCall, "DoAutoAssignRTName"); check("  arg", g_lastArg1, 0x44);
	msg.m_mode = 4; msg.m_value = 0x55; h.ExecPageMenuCommand(&msg);
	checkStr("ExecPageMenuCommand(4)", g_lastCall, "DoInitModule"); check("  arg", g_lastArg1, 0x55);
	g_calls = 0; msg.m_mode = 5; h.ExecPageMenuCommand(&msg);
	check("ExecPageMenuCommand(5): out of range, no-op", g_calls, 0);

	/* --- NotifyGECategoryPopupStatus --- */
	msg.m_mode = 0; h.NotifyGECategoryPopupStatus(&msg);
	checkStr("NotifyGECategoryPopupStatus(mode=0)", g_lastCall, "OpenGECategoryPopup");
	msg.m_mode = 1; msg.m_value = 1; h.NotifyGECategoryPopupStatus(&msg);
	checkStr("NotifyGECategoryPopupStatus(mode=1,val=1)", g_lastCall, "CloseGECategoryPopup");
	check("  arg=true", g_lastArg1, 1);
	msg.m_value = 0; h.NotifyGECategoryPopupStatus(&msg);
	check("NotifyGECategoryPopupStatus(mode=1,val=0) arg=false", g_lastArg1, 0);

	/* --- UpdateUserGEs --- */
	msg.m_mode = 3; msg.m_value = 7;
	h.UpdateUserGEs(&msg);
	checkStr("UpdateUserGEs call", g_lastCall, "UpdateUserGE");
	check("UpdateUserGEs arg1 (+0x800)", g_lastArg1, 3 + 0x800);
	check("UpdateUserGEs arg2 (+0x800)", g_lastArg2, 7 + 0x800);

	/* --- UpdateUserTemplates: loop [mode+2, value+2] inclusive --- */
	g_setupTemplateCount = 0;
	msg.m_mode = 1; msg.m_value = 3; /* loop i=3..5 */
	h.UpdateUserTemplates(&msg);
	check("UpdateUserTemplates: iteration count", g_setupTemplateCount, 3);
	check("UpdateUserTemplates: first i", g_setupTemplateCalls[0], 3);
	check("UpdateUserTemplates: last i", g_setupTemplateCalls[2], 5);

	/* empty-range case: SendShutUp still called */
	g_calls = 0; g_setupTemplateCount = 0;
	msg.m_mode = 5; msg.m_value = 1; /* mode+2=7 > value+2=3, no iterations */
	h.UpdateUserTemplates(&msg);
	check("UpdateUserTemplates: empty range, no iterations", g_setupTemplateCount, 0);
	checkStr("UpdateUserTemplates: SendShutUp still called", g_lastCall, "SendShutUp");

	/* --- UpdateGEInfo: 2 calls (CKGEngine 1-arg, then CKGUIMsgSender's
	 * own 2-arg overload via the +0x5c subobject trick, which itself
	 * ends by calling the mocked KGOutGate_SendMessageToUI -- so only
	 * the call COUNT is checked here, not the final g_lastCall, since
	 * the 2nd real call overwrites it). */
	g_calls = 0;
	msg.m_mode = 9; msg.m_value = 11;
	h.UpdateGEInfo(&msg);
	check("UpdateGEInfo: 2 real calls made", g_calls, 2);

	/* --- UpdateSoftPedalStatus --- */
	msg.m_mode = 1;
	h.UpdateSoftPedalStatus(&msg);
	checkStr("UpdateSoftPedalStatus call", g_lastCall, "ProcessLocalControlChannelMessage");
	check("UpdateSoftPedalStatus: guard restored", CKGControlMsgHandler::ms_bIsNowProcessingSoftPedalMessage, false);

	/* --- SetSendingBulkDump: snapshot/restore round trip --- */
	g_bank[0x97c7bb] = 0x42;
	msg.m_mode = 1; /* stop == true, entering bulk-dump */
	h.SetSendingBulkDump(&msg);
	check("SetSendingBulkDump(enter): flag forced to 1", g_bank[0x97c7bb], 1);
	check("SetSendingBulkDump(enter): ShouldStopSendingNoteOns", CSKMIDIInMsgHandler::ms_bShouldStopSendingNoteOnsToSTG, true);

	msg.m_mode = 0; /* leaving bulk-dump */
	h.SetSendingBulkDump(&msg);
	check("SetSendingBulkDump(leave): flag restored to snapshot", g_bank[0x97c7bb], 0x42);
	check("SetSendingBulkDump(leave): ShouldStopSendingNoteOns cleared", CSKMIDIInMsgHandler::ms_bShouldStopSendingNoteOnsToSTG, false);

	printf("========================================================================\n");
	if (g_fail) { printf("%d FAILURES\n", g_fail); return 1; }
	printf("ALL PASS\n");
	return 0;
}
