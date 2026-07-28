// SPDX-License-Identifier: GPL-2.0
/*
 * ckg_control_msg_handler.cpp  -  CKGControlMsgHandler, the KARMA UI
 * control-action dispatch target. See oa_ckg_control_ui_msg.h for the
 * full family writeup, field derivations, and the list of deliberately
 * deferred methods (HandleMessage, SharedMemProgramDump/CombiDump/
 * SongDump).
 */

#include "oa_ckg_control_ui_msg.h"

bool CKGControlMsgHandler::ms_bIsNowDumpingSong;
bool CKGControlMsgHandler::ms_bIsNowDumpingCombi;
bool CKGControlMsgHandler::ms_bIsNowDumpingProg;
bool CKGControlMsgHandler::ms_bIsNowProcessingSoftPedalMessage;

unsigned char *CKGRTCHandler::ms_poInstance;
unsigned char *CKGMIDIMsgProcessor::ms_poInstance;
unsigned char *CSKMIDIMsgProcessor::ms_poInstance;
bool CSKMIDIInMsgHandler::ms_bShouldStopSendingNoteOnsToSTG;
unsigned char *CMIDIFlowParamHolder::ms_poThis;

/* .text+0x3c7ed0, 35 bytes (C1) -- installs the real vtable pointer
 * (install-only, see header) and zero-inits the 4 static guard bools.
 * The real ctor never touches m_savedDumpFlag. */
CKGControlMsgHandler::CKGControlMsgHandler()
{
	ms_bIsNowDumpingSong = false;
	ms_bIsNowDumpingCombi = false;
	ms_bIsNowDumpingProg = false;
	ms_bIsNowProcessingSoftPedalMessage = false;
}

/* .text+0x3c7f00, 67 bytes */
void CKGControlMsgHandler::ChangeProgram(const CKGControlMsg *msg)
{
	if (msg->m_value != 0xffff)
		return;
	CKGMIDIMsgProcessor *proc = (CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance;
	proc->ResetKarmaGeneratedCCValue();
	ms_bIsNowDumpingProg = true;
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->ChangePerformance(eSTGMsgPerfType_Program, true);
	ms_bIsNowDumpingProg = false;
}

/* .text+0x3c7f50, 61 bytes */
void CKGControlMsgHandler::ChangeCombi(const CKGControlMsg *msg)
{
	if (msg->m_value != 0xffff)
		return;
	CKGMIDIMsgProcessor *proc = (CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance;
	proc->ResetKarmaGeneratedCCValue();
	ms_bIsNowDumpingCombi = true;
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->ChangePerformance(eSTGMsgPerfType_Combi, false);
	ms_bIsNowDumpingCombi = false;
}

/* .text+0x3c7fa0, 54 bytes -- unlike ChangeProgram/ChangeCombi, this one
 * does NOT call ResetKarmaGeneratedCCValue() first (a real, confirmed
 * asymmetry, preserved as-is). */
void CKGControlMsgHandler::ChangeSong(const CKGControlMsg *msg)
{
	if (msg->m_value != 0xffff)
		return;
	ms_bIsNowDumpingSong = true;
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->ChangePerformance(eSTGMsgPerfType_Song, false);
	ms_bIsNowDumpingSong = false;
}

/* .text+0x3c7fe0, 39 bytes -- raw mode (0/1/2) translated through a
 * fixed .rodata lookup table {1,0,2} (indices 0,1,2 -> values). Values
 * >2 (unsigned compare) are ignored entirely; the table itself never
 * produces the dead ==3 skip case (kept for byte-fidelity anyway). */
void CKGControlMsgHandler::SetMode(const CKGControlMsg *msg)
{
	static const int table[3] = { 1, 0, 2 };
	unsigned int mode = (unsigned int)msg->m_mode;
	if (mode > 2)
		return;
	int type = table[mode];
	if (type == 3)
		return;
	CKGBankManager *bm = (CKGBankManager *)CKGBankManager::ms_poInstance;
	bm->ChangeMode((eSTGMsgPerfType)type);
}

/* .text+0x3c8010, 13 bytes */
void CKGControlMsgHandler::Start(const CKGControlMsg *)
{
	CKGEngine::ms_poInstance[0xb0] = 0;
}

/* .text+0x3c8020, 13 bytes */
void CKGControlMsgHandler::Stop(const CKGControlMsg *)
{
	CKGEngine::ms_poInstance[0xb0] = 1;
}

/* .text+0x3c8030, 47 bytes */
void CKGControlMsgHandler::FinishLoadingGEsAndTemplates(const CKGControlMsg *msg)
{
	CKGBankManager *bm = (CKGBankManager *)CKGBankManager::ms_poInstance;
	bm->FinishLoadingGEsAndTemplates(msg->m_mode, msg->m_value);
	CKGEngine::ms_poInstance[0xb0] = 0;
	CMIDIFlowParamHolder *mfph = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	mfph->Start();
}

/* .text+0x3c8060, 20 bytes */
void CKGControlMsgHandler::RestoreRTCBackupValue(const CKGControlMsg *)
{
	CKGRTCHandler *h = (CKGRTCHandler *)CKGRTCHandler::ms_poInstance;
	h->ChangePerformance();
}

/* .text+0x3c8080, 20 bytes */
void CKGControlMsgHandler::ResetAllRTCValue(const CKGControlMsg *)
{
	CKGRTCHandler *h = (CKGRTCHandler *)CKGRTCHandler::ms_poInstance;
	h->ResetAllScene();
}

/* .text+0x3c80a0, 30 bytes */
void CKGControlMsgHandler::NoteMapTableOctaveReplicate(const CKGControlMsg *msg)
{
	CKGEngine::ms_poKGParamEdit->SendNoteMapOctaveReplicate(msg->m_mode != 0);
}

/* .text+0x3c80c0, 31 bytes */
void CKGControlMsgHandler::ResetNoteMapTable(const CKGControlMsg *msg)
{
	CKGEngine::ms_poKGParamEdit->SendNoteMapTableReset(msg->m_value != 0);
}

/* .text+0x3c81a0, 64 bytes -- real ja-then-jump-table 3-way switch on
 * m_mode (0/1/2), anything else (>2, unsigned) is a silent no-op. */
void CKGControlMsgHandler::NotifyUIOperation(const CKGControlMsg *msg)
{
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	switch (msg->m_mode) {
	case 0:
		eng->WritePerformance();
		break;
	case 1:
		eng->DoCompare();
		break;
	case 2:
		eng->DoCurrentDump();
		break;
	default:
		break;
	}
}

/* .text+0x3c81f0, 1 byte -- confirmed real no-op (bare ret). */
void CKGControlMsgHandler::NotifyDiagnosticMode(const CKGControlMsg *)
{
}

/* .text+0x3c8200, 137 bytes -- 5-entry .rodata jump table, real
 * index->target mapping read directly from ground truth (NOT the address
 * order the case bodies happen to appear in): 0->DoAutoRTCSetup,
 * 1->DoClearRTCSetup, 2->DoRandomCapture, 3->DoAutoAssignRTName,
 * 4->DoInitModule. Indices >4 (unsigned) are a no-op. */
void CKGControlMsgHandler::ExecPageMenuCommand(const CKGControlMsg *msg)
{
	if ((unsigned int)msg->m_mode > 4)
		return;
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	switch (msg->m_mode) {
	case 0:
		eng->DoAutoRTCSetup(msg->m_value);
		break;
	case 1:
		eng->DoClearRTCSetup(msg->m_value);
		break;
	case 2:
		eng->DoRandomCapture(msg->m_value);
		break;
	case 3:
		eng->DoAutoAssignRTName(msg->m_value);
		break;
	case 4:
		eng->DoInitModule(msg->m_value);
		break;
	}
}

/* .text+0x3c8290, 22 bytes */
void CKGControlMsgHandler::UpdateRTCDisplay(const CKGControlMsg *msg)
{
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->UpdateRTCDisplay(msg->m_mode);
}

/* .text+0x3c82b0, 22 bytes */
void CKGControlMsgHandler::UpdateRTCModelName(const CKGControlMsg *msg)
{
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->UpdateRTCModelName(msg->m_mode);
}

/* .text+0x3c82d0, 50 bytes -- m_mode==0 -> Open, else Close(m_value==1). */
void CKGControlMsgHandler::NotifyGECategoryPopupStatus(const CKGControlMsg *msg)
{
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	if (msg->m_mode != 0)
		eng->CloseGECategoryPopup(msg->m_value == 1);
	else
		eng->OpenGECategoryPopup();
}

/* .text+0x3c8310, 1 byte -- confirmed real no-op (bare ret). */
void CKGControlMsgHandler::PrepareResetBuffer(const CKGControlMsg *)
{
}

/* .text+0x3c8320, 37 bytes */
void CKGControlMsgHandler::UpdateUserGEs(const CKGControlMsg *msg)
{
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->UpdateUserGE(msg->m_mode + 0x800, msg->m_value + 0x800);
}

/* .text+0x3c8350, 69 bytes -- SendShutUp() runs unconditionally BEFORE
 * the loop-emptiness check (called even if the loop body never runs). */
void CKGControlMsgHandler::UpdateUserTemplates(const CKGControlMsg *msg)
{
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->SendShutUp();
	CKGBankManager *bm = (CKGBankManager *)CKGBankManager::ms_poInstance;
	int lo = msg->m_mode + 2;
	int hi = msg->m_value + 2;
	for (int i = lo; i <= hi; i++)
		bm->SetupTemplateAfterLoading(i);
}

/* .text+0x3c83a0, 64 bytes -- two separate calls: CKGEngine's own
 * single-int UpdateGEInfo(int), then CKGUIMsgSender's 2-int overload
 * reached through a CKGUIMsgSender subobject embedded inside
 * CKGUIMsgProcessor at +0x5c (CKGUIMsgSender is stateless, so this is
 * purely a pointer-arithmetic detail, not a behavioural one). */
void CKGControlMsgHandler::UpdateGEInfo(const CKGControlMsg *msg)
{
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->UpdateGEInfo(msg->m_mode);
	CKGUIMsgSender *sender = (CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c);
	sender->UpdateGEInfo(msg->m_mode, msg->m_value);
}

/* .text+0x3c83e0, 75 bytes -- reuses the identical "toggle
 * ms_bIsNowProcessingSoftPedalMessage around a fixed local-control-
 * channel MIDI message" shape HandleMessage's own inline soft-pedal case
 * duplicates (status=0x43, controller=0xb0, value computed from
 * m_mode==1). */
void CKGControlMsgHandler::UpdateSoftPedalStatus(const CKGControlMsg *msg)
{
	unsigned char value = (msg->m_mode == 1) ? 0x7f : 0x00;
	ms_bIsNowProcessingSoftPedalMessage = true;
	CSKMIDIMsgProcessor *proc = (CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance;
	proc->ProcessLocalControlChannelMessage(0x43, 0xb0, (char)value, 0);
	ms_bIsNowProcessingSoftPedalMessage = false;
}

/* .text+0x3c8430, 173 bytes -- entering bulk-dump mode (m_mode!=0):
 * snapshot CKGBankManager's own dump flag into this instance
 * (m_savedDumpFlag) then force it on; leaving (m_mode==0): restore it. */
void CKGControlMsgHandler::SetSendingBulkDump(const CKGControlMsg *msg)
{
	bool stop = (msg->m_mode != 0);
	CSKMIDIInMsgHandler::ms_bShouldStopSendingNoteOnsToSTG = stop;
	KGOutGate_StopSendingToMIDIPort(stop);

	unsigned char *bm = CKGBankManager::ms_poInstance;
	if (stop) {
		m_savedDumpFlag = bm[0x97c7bb];
		CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
		eng->SendShutUp();
		bm[0x97c7bb] = 1;
		CKGEngine::ms_poKGParamEdit->SendForceKarmaOff(true);
		SPRMain_SetAllKARMAAndDrumTrack(true);
	} else {
		bm[0x97c7bb] = m_savedDumpFlag;
		CKGEngine::ms_poKGParamEdit->SendForceKarmaOff(m_savedDumpFlag != 0);
		SPRMain_SetAllKARMAAndDrumTrack(bm[0x97c7bc] != 0);
	}
}
