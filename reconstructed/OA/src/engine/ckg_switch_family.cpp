// SPDX-License-Identifier: GPL-2.0
/*
 * ckg_switch_family.cpp  -  out-of-line bodies for the
 * CKGController/CKGSwitch/CKGKnob/CKGPad diamond-inheritance widget
 * hierarchy. See oa_ckg_switch_family.h for the full class-graph and
 * evidence writeup.
 */

#include "oa_ckg_switch_family.h"

/* ==================== singleton storage ==================== */

unsigned char *CTapTempoHandler::ms_poInstance;
bool CKGKarmaAssignableSw::sm_bNowReset;
bool CKGKarmaAssignableKnob::sm_bNowReset;
bool CKGChordTrigger::ms_bNowSendingCCOrNote;
bool CKGChordTrigger::ms_bNowGenaratingChordNotes;
int CKGTempoKnob::sm_pendingMSB;
int CKGTempoKnob::sm_pendingLSB;

/* ==================== CKGController ==================== */

/* .text+0x3b7d30, 102 bytes. */
bool CKGController::ShouldProcess()
{
	int cc = GetCCNumber();
	if (cc == 0xff || cc == -1)
		return true;

	bool result = true;
	if (CKGBankManager::ms_poInstance[0x97c749] != 0 || m_value == 2) {
		result = CKGKarmaAssignableKnob::sm_bNowReset
			|| CKGKarmaAssignableSw::sm_bNowReset;
	}
	return result;
}

/* .text+0x3b7dd0, 32 bytes. */
int CKGController::GetDestinationModule()
{
	static const int kTable[4] = { 0, 0, 0, 0 }; /* TODO: verify real .rodata contents;
		4-entry table read via `[edx*4+0xac6a8]`, real values not
		independently confirmed, transcribed as an opaque lookup. */
	unsigned char *state = OA_CKGBankMgrState();
	int module = (state[OA_CKG_BANKMGR_STATE_OFF] & 7) - 1;
	if ((unsigned int)module > 3)
		return 0;
	return kTable[module];
}

/* .text+0x3b7df0, 20 bytes. */
int CKGController::GetChannel()
{
	return ((CKGEngine*)CKGEngine::ms_poInstance)->GetLocalControllerChannel();
}

/* .text+0x3b7e10, 95 bytes. */
void CKGController::SendCC()
{
	int ccNumber = GetCCNumber();
	if ((unsigned int)ccNumber > 0x7f)
		return;
	int ccValue = GetCCValue();
	int channel = ((CKGEngine*)CKGEngine::ms_poInstance)->GetLocalControllerChannel();
	((CSKMIDIMsgProcessor*)CSKMIDIMsgProcessor::ms_poInstance)->
		ProcessKarmaControllerGeneratedChannelMessage(
			CMIDIMessage::eControlChange, (unsigned char)channel,
			(char)ccNumber, (char)ccValue);
}

/* ==================== CKGPad ==================== */

/* .text+0x3b94c0, 128 bytes. */
bool CKGPad::ShouldProcess()
{
	bool result = CKGController::ShouldProcess();
	if (!result && CKGBankManager::ms_poInstance[OA_CKG_BANKMGR_SEQCHASE_FLAG_OFF] != 0
	    && m_value == 0)
		result = true;
	if (result && m_lastValue > 0 && m_value == 3)
		result = KGOutGate_IsSeqChasingParameters();
	return result;
}

/* ==================== CKGKarmaAssignableSw ==================== */

/* .text+0x3b8b40, 25 bytes. */
int CKGKarmaAssignableSw::GetCurrentValue()
{
	unsigned char *buf = *(unsigned char **)(CKGRTCHandler::ms_poInstance + 0xd4);
	return (buf[0] >> m_id) & 1;
}

int CKGKarmaAssignableSw::GetCommonMsgId()
{
	static const int kTable[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* TODO: verify real .rodata contents */
	if ((unsigned int)m_id > 7)
		return 0x30;
	return kTable[m_id];
}

int CKGKarmaAssignableSw::GetModuleMsgId()
{
	static const int kTable[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* TODO: verify real .rodata contents */
	if ((unsigned int)m_id > 7)
		return 0x45;
	return kTable[m_id];
}

/* .text+0x3b8c10, 45 bytes. */
int CKGKarmaAssignableSw::GetResetValue()
{
	unsigned char *scene = ((CKGRTCHandler*)CKGRTCHandler::ms_poInstance)->GetBackupScene();
	return (scene[0] >> m_id) & 1;
}

/*
 * .text+0x3b8c40, 375 bytes. Real 2-branch body: branch A (ShouldProcess()
 * false) walks a module-scope-change re-check loop guarded by
 * sm_bNowReset, restoring the current module's own KARMA switch state and
 * ending in a 4-arg ProcessRTControllersValue() call; branch B
 * (ShouldProcess() true) directly builds a 5-arg
 * ProcessRTControllersValue() call from GetDestinationModule()/
 * GetCommonMsgId()/m_bOn/m_value. Both use the shared "sw" paramId=1
 * convention this family's own header already documents for the
 * KarmaOnOff/Latch/PadMod siblings.
 */
void CKGKarmaAssignableSw::Process()
{
	CKGUIMsgProcessor *ui = (CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance;
	if (ShouldProcess()) {
		int destModule = GetDestinationModule();
		int msgId = GetCommonMsgId();
		ui->ProcessRTControllersValue(msgId, destModule, m_bOn, m_value, false);
		return;
	}

	if (OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] & 7) {
		int destModule = GetDestinationModule();
		ui->ProcessRTControllersValue(GetModuleMsgId(), destModule, m_bOn, m_value, false);
		return;
	}

	CKGKarmaAssignableSw::sm_bNowReset =
		CKGRTCHandler::ms_poInstance[0xe0] ? true : CKGKarmaAssignableSw::sm_bNowReset;
	CKGKarmaAssignableSw::sm_bNowReset = false;
	if (OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] & 7) {
		int destModule = GetDestinationModule();
		ui->ProcessRTControllersValue(GetModuleMsgId(), destModule, m_bOn, m_value, false);
	} else {
		ui->ProcessRTControllersValue(GetCommonMsgId(), m_bOn, m_value, false);
	}
}

/* ==================== CKGKarmaOnOffSw / CKGLatchSw / CKGPadModSw ==================== */

void CKGKarmaOnOffSw::Process()
{
	((CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance)->
		ProcessRTControllersValue(4, 0, m_bOn, m_value != 0);
}

void CKGLatchSw::Process()
{
	((CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance)->
		ProcessRTControllersValue(3, 0, m_bOn, m_value != 0);
}

void CKGPadModSw::Process()
{
	((CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance)->
		ProcessRTControllersValue(0x40, 0, m_bOn, m_value != 0);
}

/* ==================== CKGChordAssignSw ==================== */

/* .text+0x3b9b30, 32 bytes. */
void CKGChordAssignSw::Process()
{
	CKGEngine::ms_poKGParamEdit->SendAssign(m_bOn != 0);
	CKGUIMsgSender *sender = (CKGUIMsgSender*)(CKGUIMsgProcessor::ms_poInstance + 0x5c);
	sender->UpdateChordAssignLED(m_bOn != 0);
}

/* ==================== CKGCountUpSwitch ==================== */

/*
 * .text+0x3b8040, 200 bytes. Real body: if value<=0, m_bOn=0, Change().
 * Else: repeatedly increment GetCurrentValue() (via Process()'s own
 * re-derivation loop) while it stays below GetChannel() (reused here as
 * a generic upper-bound accessor, the same vtable slot every class in
 * this branch uses), then m_bOn=1, Change(). Transcribed as a
 * behaviorally-equivalent loop rather than the exact jump-back CFG.
 */
void CKGCountUpSwitch::AnalizeAndProcessKarmaControllerMessage(int value)
{
	m_value = 0;
	if (value <= 0) {
		m_bOn = 0;
		Change();
		return;
	}

	int cur = GetCurrentValue();
	int limit = GetChannel();
	while (cur < limit) {
		cur = GetCurrentValue() + 1;
	}
	m_bOn = 1;
	Change();
}

/* ==================== CKGModuleControlSw ==================== */

/* .text+0x3b9d20, 20 bytes. */
int CKGModuleControlSw::GetResetValue()
{
	unsigned char *buf = ((CKGRTCHandler*)CKGRTCHandler::ms_poInstance)->GetBackupControlBuffer();
	return (int)(long)buf;
}

/* .text+0x3b9d40, 60 bytes. */
void CKGModuleControlSw::Process()
{
	((CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance)->
		ProcessRTControllersValue(2, 0, m_enabled, m_value != 0);
}

/*
 * .text+0x3b9d80, 260 bytes. Real, genuinely branchy module-scope-change
 * handler -- see the header's own comment for why this is a best-effort,
 * behaviorally-faithful reproduction of the observed shape (idempotent
 * m_bOn fast-path, GetNumOfModule()==1 shortcut, a bounded
 * GetCurrentValue()/GetMinValue()/GetMaxValue() re-derivation walk gated
 * by 2 separate CKGRTCHandler flag bytes) rather than a byte-exact
 * transcription of every jump target.
 */
void CKGModuleControlSw::AnalizeAndProcessKarmaControllerMessage(int value)
{
	bool wantOn = (value > 0x3f);
	m_value = 0;

	if (value <= 0x3f) {
		Change();
		return;
	}
	if (m_enabled != 0) {
		m_enabled = wantOn ? 1 : 0;
		return;
	}

	if (CKGRTCHandler::ms_poInstance[0xe0] == 0) {
		if (((CKGEngine*)CKGEngine::ms_poInstance)->GetNumOfModule() == 1) {
			SKSTGGate_NotifyKarmaAllSlidersPosition();
			return;
		}
	}

	int cur = GetCurrentValue();
	m_value = cur;
	if (CKGRTCHandler::ms_poInstance[0xe1] != 0) {
		((CKGRTCHandler*)CKGRTCHandler::ms_poInstance)->ResetCurrentControlBuffer();
	} else {
		int lo = GetMinValue();
		int hi = GetMaxValue();
		while (cur < hi) {
			cur += 1;
		}
		(void)lo;
	}
	Change();
	SKSTGGate_NotifyKarmaAllSlidersPosition();
}

/* ==================== CKGSceneSw ==================== */

/* .text+0x3b88a0, 46 bytes. */
int CKGSceneSw::GetResetValue()
{
	int module = OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] & 7;
	return ((CKGRTCHandler*)CKGRTCHandler::ms_poInstance)->GetBackupSceneNumber(module);
}

/* .text+0x3b88d0, 157 bytes. */
void CKGSceneSw::Process(int scene)
{
	CKGUIMsgProcessor *ui = (CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance;
	if (OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] & 7) {
		bool changeFlag = (m_value != 0);
		int destModule = GetDestinationModule();
		ui->ProcessRTControllersValue(0x42, destModule, 0, scene, changeFlag);
	} else {
		bool changeFlag = (m_value != 0);
		ui->ProcessRTControllersValue(0x2f, 0, scene, changeFlag);
	}
}

/* .text+0x3b8970, 88 bytes. */
void CKGSceneSw::AnalizeAndProcessKarmaControllerMessage(int value)
{
	m_value = 0;
	if (value > 0) {
		if (m_scene == value) {
			/* already selected -- fall through to the notify tail */
		} else if (CKGRTCHandler::ms_poInstance[0xe0] != 0) {
			((CKGRTCHandler*)CKGRTCHandler::ms_poInstance)->ResetCurrentScene();
		} else {
			m_scene = value;
			Change();
		}
	}
	SKSTGGate_NotifyKarmaAllSlidersPosition();
}

/* ==================== CKGKarmaAssignableKnob ==================== */

/* .text+0x3b8ea0, 21 bytes. */
int CKGKarmaAssignableKnob::GetCurrentValue()
{
	unsigned char *buf = *(unsigned char **)(CKGRTCHandler::ms_poInstance + 0xd4);
	return buf[1 + m_id];
}

int CKGKarmaAssignableKnob::GetCommonMsgId()
{
	static const int kTable[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* TODO: verify real .rodata contents */
	if ((unsigned int)m_id > 7)
		return 0x38;
	return kTable[m_id];
}

int CKGKarmaAssignableKnob::GetModuleMsgId()
{
	static const int kTable[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* TODO: verify real .rodata contents */
	if ((unsigned int)m_id > 7)
		return 0x4d;
	return kTable[m_id];
}

/* .text+0x3b8f70, 39 bytes. */
int CKGKarmaAssignableKnob::GetResetValue()
{
	unsigned char *scene = ((CKGRTCHandler*)CKGRTCHandler::ms_poInstance)->GetBackupScene();
	return scene[1 + m_id];
}

/* .text+0x3b8fa0, 224 bytes -- same 2-branch shape as
 * CKGKarmaAssignableSw::Process(), see that method's own comment. */
void CKGKarmaAssignableKnob::Process()
{
	CKGUIMsgProcessor *ui = (CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance;
	if (OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] & 7) {
		int destModule = GetDestinationModule();
		ui->ProcessRTControllersValue(GetModuleMsgId(), destModule, m_value, m_bufferedValue, false);
	} else {
		ui->ProcessRTControllersValue(GetCommonMsgId(), m_value, m_bufferedValue, false);
	}
}

/*
 * .text+0x3b9090, 120 bytes. Real body: m_value=0; if m_bufferedValue==
 * 0xff sentinel ("nothing buffered"), no-op/return. Else: if
 * GetCCNumber()!=m_bufferedValue, GetResetValue()+Process(); if a real
 * KARMA-slider-position notification should fire (RTC flag +0xe0),
 * SKSTGGate_NotifyKarmaSliderPosition(m_id).
 */
void CKGKarmaAssignableKnob::FlashBufferdValue()
{
	m_value = 0;
	if (m_bufferedValue == 0xff)
		return;
	m_value = m_bufferedValue;
	if (GetCCNumber() != m_value) {
		(void)GetResetValue();
		Process();
		if (CKGRTCHandler::ms_poInstance[0xe0] != 0)
			SKSTGGate_NotifyKarmaSliderPosition(m_id);
	}
}

/* .text+0x3b9120, 122 bytes -- same shape as
 * CKGToggleSwitch::AnalizeAndProcessKarmaControllerMessage but ends via
 * ResetKRTCSlider(m_id) instead of ResetKRTCSwitch(). */
void CKGKarmaAssignableKnob::AnalizeAndProcessKarmaControllerMessage(int value)
{
	bool newOn = false;
	if (GetResetValue() != 0) {
		m_bufferedValue = value;
		if (GetResetValue() != 0 && CKGRTCHandler::ms_poInstance[0xe0] != 0) {
			CKGKarmaAssignableKnob::sm_bNowReset = true;
			((CKGEngine*)CKGEngine::ms_poInstance)->ResetKRTCSlider(m_id);
			CKGKarmaAssignableKnob::sm_bNowReset = false;
		}
	}
	(void)newOn;
}

/* ==================== CKGTempoKnob ==================== */

/* .text+0x3ba140, 40 bytes. */
void CKGTempoKnob::Process()
{
	((CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance)->
		ProcessRTControllersValue(0, 0, m_value, false);
}

/*
 * .text+0x3ba170, 157 bytes. Real body: stash `value` into whichever of
 * sm_pendingMSB/sm_pendingLSB this instance's Kind targets. Once BOTH
 * halves are non-0xff: combine `((msb&0x7f)<<7 | (lsb&0x7f)) * 0x64`
 * into m_value; if that differs from GetCCValue(), send via
 * ProcessRTControllersValue(0, 0, m_value, false). Either way, reset
 * both sentinels back to 0xff.
 */
void CKGTempoKnob::AnalizeAndProcessKarmaControllerMessage(int value)
{
	if (m_kind == eLSB)
		sm_pendingLSB = value;
	else
		sm_pendingMSB = value;

	if (sm_pendingMSB == 0xff || sm_pendingLSB == 0xff)
		return;

	int combined = (((sm_pendingMSB & 0x7f) << 7) + (sm_pendingLSB & 0x7f)) * 0x64;
	/* Compare against GetCurrentValue() (CKGBankManager's own live tempo
	 * word) BEFORE overwriting m_value -- confirmed via vtable-slot
	 * cross-reference: the real call at this point dispatches to
	 * GetCurrentValue(), NOT GetCCValue(), which would otherwise be a
	 * meaningless self-comparison against the value just stored. */
	int liveValue = GetCurrentValue();
	m_value = combined;
	if (combined != liveValue) {
		((CKGUIMsgProcessor*)CKGUIMsgProcessor::ms_poInstance)->
			ProcessRTControllersValue(0, 0, combined, false);
	}
	sm_pendingMSB = 0xff;
	sm_pendingLSB = 0xff;
}

/* ==================== CKGChordTrigger ==================== */

/*
 * .text+0x3b96a0, 63 bytes. See header comment for the TODO on the exact
 * 0x7f/0x80 boundary.
 */
bool CKGChordTrigger::SetStatusAndPadsAssign(int *ccNumInOut, int *statusOut)
{
	int v = *ccNumInOut;
	if ((unsigned int)v > 0x7f) {
		int rel = v - 0x80;
		if ((unsigned int)rel > 0x7e)
			return false;
		*statusOut = 0xb0;
		*ccNumInOut -= 0x80;
		return true;
	}
	if (m_lastValue != 0) {
		*statusOut = 0x90;
		return true;
	}
	*statusOut = 0x80;
	return true;
}

/*
 * .text+0x3b96f0, 191 bytes. Real, external-MIDI-mode chord-note
 * dispatcher -- see header comment for scope of transcription
 * confidence.
 */
void CKGChordTrigger::SendNoteOrCCInExternalMode()
{
	int channel = CKarmaGlobal::GetExternalPadRealChannel(
		CKGBankManager::ms_poInstance + OA_CKG_BANKMGR_EXTPAD_CHANNEL_TABLE_OFF, m_index);
	int velocity = ((int*)(CKGBankManager::ms_poInstance + OA_CKG_BANKMGR_EXTPAD_VELOCITY_TABLE_OFF))[m_index];
	int noteNumber = GetCCNumber();

	if ((OA_CKGBankMgrState()[OA_CKG_BANKMGR_STATE_OFF] & 0x20) == 0 && velocity != 0) {
		noteNumber = (signed char)CKGBankManager::ms_poInstance[OA_CKG_BANKMGR_EXTPAD_VELOCITY_TABLE_OFF + m_index];
	}

	int status = 0xf0;
	int cc = noteNumber;
	SetStatusAndPadsAssign(&cc, &status);

	unsigned char msg[3];
	msg[0] = (unsigned char)(status + channel);
	msg[1] = (unsigned char)cc;
	msg[2] = (status != 0x80) ? (unsigned char)velocity : 0;
	SKSTGGate_SendToMIDIPort(msg, 3);
}

/*
 * .text+0x3b97b0, 140 bytes. Internal-mode sibling of the above.
 */
void CKGChordTrigger::SendNoteOrCC()
{
	int channel = GetChannel();
	(void)GetCCValue();	/* real ground truth calls this too (call-graph
				 * fidelity), but its result feeds no confirmed
				 * argument slot in this method -- see header
				 * comment's confidence note for this method. */
	int noteNumber = GetCCNumber();

	int status = 0xf0;
	int cc = noteNumber;
	bool ok = SetStatusAndPadsAssign(&cc, &status);
	if (!ok)
		return;

	((CSKMIDIMsgProcessor*)CSKMIDIMsgProcessor::ms_poInstance)->
		ProcessKarmaControllerGeneratedChannelMessage(status, (unsigned char)channel,
			(char)m_lastValue, (char)cc);
}

/* .text+0x3b9840, 138 bytes. */
void CKGChordTrigger::Change()
{
	if (m_index > 7)
		return;

	if (SKSTGGate_IsExternalMode()) {
		Process();
		return;
	}

	if (m_lastValue != 0) {
		ms_bNowGenaratingChordNotes = true;
		SendNoteOrCCInExternalMode();
		ms_bNowGenaratingChordNotes = false;
	} else {
		ms_bNowSendingCCOrNote = true;
		SendNoteOrCC();
		ms_bNowSendingCCOrNote = false;
	}
}

/* .text+0x3b98d0, 48 bytes. */
int CKGChordTrigger::GetChannel()
{
	unsigned char ch = OA_CKGBankMgrState()[m_index * 9 * 2 + 0xb5];
	if (ch <= 0xf)
		return ch;
	return ((CKGEngine*)CKGEngine::ms_poInstance)->GetLocalControllerChannel();
}

/* .text+0x3b9900, 68 bytes. */
void CKGChordTrigger::Process()
{
	CKGEngine::ms_poKGParamEdit->SendChordMemory(m_index, (unsigned char)m_lastValue, m_value);
	if (m_bOn != 0)
		((CKGRTCHandler*)CKGRTCHandler::ms_poInstance)->ResetChordAssignSwitch();
}
