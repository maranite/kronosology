// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_ui_msg_sender.cpp  -  KAT for CKGUIMsgSender
 * (see ../src/engine/ckg_ui_msg_sender.cpp). Every check inspects the raw
 * CSKMessage bytes handed to a mocked KGOutGate_SendMessageToUI(), plus
 * the immediate flag and the send/drop decision -- no field is verified
 * indirectly through a "looks about right" helper.
 */

#include <cstdio>
#include <cstring>
#include "oa_ckg_control_ui_msg.h"

unsigned char *CKGEngine::ms_poInstance;
CKGParamEdit *CKGEngine::ms_poKGParamEdit;
void CKGEngine::ResetLocalController() {}
bool CKGControlMsgHandler::ms_bIsNowDumpingSong;
bool CKGControlMsgHandler::ms_bIsNowDumpingCombi;
bool CKGControlMsgHandler::ms_bIsNowDumpingProg;
bool CKGControlMsgHandler::ms_bIsNowProcessingSoftPedalMessage;
CKGControlMsgHandler::CKGControlMsgHandler() {}

static int g_sendCalls;
static unsigned char g_lastMsg[0x30];
static bool g_lastImmediate;
extern "C" void KGOutGate_SendMessageToUI(const CSKMessage *msg, bool immediate)
{
	g_sendCalls++;
	memcpy(g_lastMsg, msg, sizeof(g_lastMsg));
	g_lastImmediate = immediate;
}

static int g_fail;
static void check(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld (0x%lx)\n", label, got, (unsigned long)got); return; }
	printf("  FAIL  %-45s got=%ld(0x%lx) want=%ld(0x%lx)\n", label, got, (unsigned long)got, want, (unsigned long)want);
	g_fail++;
}
typedef unsigned short u16;
typedef unsigned int u32;
static u16 rd16(int off) { return *(unsigned short *)(g_lastMsg + off); }
static u32 rd32(int off) { return *(unsigned int *)(g_lastMsg + off); }

#define ENGSZ 0x200
static unsigned char g_engine[ENGSZ];

int main(void)
{
	printf("CKGUIMsgSender known-answer test\n");
	printf("========================================================================\n");

	memset(g_engine, 0, ENGSZ);
	*(int *)(g_engine + 0xa0) = 0x7788;
	CKGEngine::ms_poInstance = g_engine;

	CKGUIMsgSender s;

	/* --- ParameterChangeMessage shape: Common --- */
	g_sendCalls = 0;
	s.SendCommonParamMessage(0x55, 0x1000, 2 /* not Dim */, 0x99);
	check("SendCommonParamMessage: sent (guards clear)", g_sendCalls, 1);
	check("  class", rd16(0x00), 0x24);
	check("  subtype", rd16(0x02), 5);
	check("  const", rd32(0x04), 2);
	check("  msgId", rd32(0x08), 0x55);
	check("  engineField", rd32(0x0c), 0x7788);
	check("  zero field", rd32(0x10), 0);
	check("  sentinel", rd32(0x14), 0xffff);
	check("  value", rd32(0x18), 0x1000);
	check("  operation", rd32(0x1c), 2);
	check("  arg4", rd32(0x20), 0x99);
	check("  immediate=false", g_lastImmediate, false);

	g_sendCalls = 0;
	s.SendCommonParamMessage(1, 2, 5 /* Dim */, 3);
	check("SendCommonParamMessage(Dim): always sent", g_sendCalls, 1);
	check("SendCommonParamMessage(Dim): immediate=true", g_lastImmediate, true);

	/* --- guard-suppression: any dumping guard blocks a non-Dim send --- */
	CKGControlMsgHandler::ms_bIsNowDumpingSong = true;
	g_sendCalls = 0;
	s.SendCommonParamMessage(1, 2, 2, 3);
	check("SendCommonParamMessage: dropped while dumping song", g_sendCalls, 0);
	g_sendCalls = 0;
	s.SendCommonParamMessage(1, 2, 5, 3);
	check("SendCommonParamMessage(Dim): still sent while dumping song", g_sendCalls, 1);
	CKGControlMsgHandler::ms_bIsNowDumpingSong = false;

	/* --- UpdateCommonParam / SetCommonParamMax / SetCommonParamMin /
	 * RefreshCommonParam / DimOnCommonParam: operation literals --- */
	s.UpdateCommonParam(1, 2, 3, true);
	check("UpdateCommonParam: operation", rd32(0x1c), 0);
	s.SetCommonParamMax(1, 2, 3);
	check("SetCommonParamMax: operation", rd32(0x1c), 3);
	s.SetCommonParamMin(1, 2, 3);
	check("SetCommonParamMin: operation", rd32(0x1c), 2);
	s.RefreshCommonParam(1, 2);
	check("RefreshCommonParam: operation", rd32(0x1c), 4);
	check("RefreshCommonParam: trailing field is 0 (no 3rd param)", rd32(0x20), 0);
	s.DimOnCommonParam(1, 2, true);
	check("DimOnCommonParam: operation", rd32(0x1c), 5);
	check("DimOnCommonParam: trailing field = dim", rd32(0x20), 1);
	check("DimOnCommonParam: immediate=true", g_lastImmediate, true);

	/* --- Module shape --- */
	g_sendCalls = 0;
	s.SendModuleParamMessage(0x10, 0x20, 0x30, 2, 0x40);
	check("SendModuleParamMessage: sent", g_sendCalls, 1);
	check("  class", rd16(0x00), 0x28);
	check("  const", rd32(0x04), 3);
	check("  msgId", rd32(0x08), 0x10);
	check("  deviceIndex", rd32(0x18), 0x20);
	check("  gap at +0x1c (never written)", rd32(0x1c), 0);
	check("  operation", rd32(0x20), 2);
	check("  value", rd32(0x24), 0x30);

	s.SetModuleParamMax(0x11, 0x22, 0x33, 0x44);
	check("SetModuleParamMax: deviceIndex", rd32(0x18), 0x22);
	check("SetModuleParamMax: value at +0x1c", rd32(0x1c), 0x33);
	check("SetModuleParamMax: operation", rd32(0x20), 3);
	check("SetModuleParamMax: trailing arg4 at +0x24", rd32(0x24), 0x44);

	s.SetModuleParamMin(0x11, 0x22, 0x33, 0x44);
	check("SetModuleParamMin: operation", rd32(0x20), 2);

	s.UpdateModuleParam(0x11, 0x22, 0x33, 0x44, true);
	check("UpdateModuleParam: operation", rd32(0x20), 0);
	check("UpdateModuleParam: value at +0x1c", rd32(0x1c), 0x33);
	check("UpdateModuleParam: arg4 at +0x24", rd32(0x24), 0x44);

	s.DimOnModuleParam(0x11, 0x22, 0x33, true);
	check("DimOnModuleParam: value at +0x1c", rd32(0x1c), 0x33);
	check("DimOnModuleParam: operation", rd32(0x20), 5);
	check("DimOnModuleParam: dim at +0x24", rd32(0x24), 1);
	check("DimOnModuleParam: immediate=true", g_lastImmediate, true);

	/* --- "UI action" shape: fixed class 0x14, always sent immediate=false --- */
	g_sendCalls = 0;
	s.UpdateChordAssignLED(true);
	check("UpdateChordAssignLED: sent", g_sendCalls, 1);
	check("  class", rd16(0x00), 0x14);
	check("  const", rd32(0x04), 0);
	check("  sub-opcode", rd32(0x08), 0x10);
	check("  engineField", rd32(0x0c), 0x7788);
	check("  param", rd32(0x10), 1);
	check("  immediate=false", g_lastImmediate, false);

	s.ChangeGE(0x123);
	check("ChangeGE: sub-opcode", rd32(0x08), 0x11);
	check("ChangeGE: param", rd32(0x10), 0x123);

	s.ChangePerformance(false);
	check("ChangePerformance: sub-opcode", rd32(0x08), 0x12);

	s.UpdateDynamicMIDIAction(0x77);
	check("UpdateDynamicMIDIAction: sub-opcode", rd32(0x08), 0x13);
	check("UpdateDynamicMIDIAction: param", rd32(0x10), 0x77);

	s.ResetValuesInControlBuffer(9);
	check("ResetValuesInControlBuffer: sub-opcode", rd32(0x08), 0x14);

	s.ResetCurrentScene();
	check("ResetCurrentScene: sub-opcode", rd32(0x08), 0x15);
	check("ResetCurrentScene: engineField present, no param", rd32(0x0c), 0x7788);

	s.UpdateNoteMapTable(0x88);
	check("UpdateNoteMapTable: sub-opcode", rd32(0x08), 0x17);

	s.UpdateGERTParamNames();
	check("UpdateGERTParamNames: sub-opcode", rd32(0x08), 0x18);

	/* variant sub-shape: no engineField pointer */
	s.UpdateKarmaInitialInfo();
	check("UpdateKarmaInitialInfo: sub-opcode", rd32(0x08), 6);
	check("UpdateKarmaInitialInfo: no engineField (0)", rd32(0x0c), 0);
	check("UpdateKarmaInitialInfo: field+0x10 also 0", rd32(0x10), 0);

	s.NotifyPadStatus(5, 6);
	check("NotifyPadStatus: sub-opcode", rd32(0x08), 0x1a);
	check("NotifyPadStatus: a at +0xc (no engineField)", rd32(0x0c), 5);
	check("NotifyPadStatus: b at +0x10", rd32(0x10), 6);

	s.UpdateNoteMapSmallTable(7, 8);
	check("UpdateNoteMapSmallTable: sub-opcode", rd32(0x08), 0x1b);
	check("UpdateNoteMapSmallTable: a at +0xc", rd32(0x0c), 7);
	check("UpdateNoteMapSmallTable: b at +0x10", rd32(0x10), 8);

	s.UpdateRTCModelName();
	check("UpdateRTCModelName: sub-opcode", rd32(0x08), 0xf);
	check("UpdateRTCModelName: no engineField", rd32(0x0c), 0);

	s.UpdateAutoClockSource(true);
	check("UpdateAutoClockSource: sub-opcode", rd32(0x08), 0x1c);
	check("UpdateAutoClockSource: on at +0xc", rd32(0x0c), 1);

	s.UpdateSceneChangeCaption(0x2a);
	check("UpdateSceneChangeCaption: sub-opcode", rd32(0x08), 0x21);
	check("UpdateSceneChangeCaption: caption at +0xc", rd32(0x0c), 0x2a);

	s.UpdateValuesForScene();
	check("UpdateValuesForScene: sub-opcode", rd32(0x08), 0x22);
	check("UpdateValuesForScene: engineField present", rd32(0x0c), 0x7788);

	s.UpdateGEInfo(10, 20);
	check("UpdateGEInfo: sub-opcode", rd32(0x08), 0x23);
	check("UpdateGEInfo: a at +0xc (no engineField)", rd32(0x0c), 10);
	check("UpdateGEInfo: b at +0x10", rd32(0x10), 20);

	s.UpdateSoftPedalStatus(true);
	check("UpdateSoftPedalStatus: sub-opcode", rd32(0x08), 0x27);
	check("UpdateSoftPedalStatus: on at +0xc", rd32(0x0c), 1);

	printf("========================================================================\n");
	if (g_fail) { printf("%d FAILURES\n", g_fail); return 1; }
	printf("ALL PASS\n");
	return 0;
}
