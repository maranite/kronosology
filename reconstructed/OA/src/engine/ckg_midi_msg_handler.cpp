// SPDX-License-Identifier: GPL-2.0
/*
 * ckg_midi_msg_handler.cpp  -  out-of-line bodies for CSKMIDIMsgHandler /
 * CSKSpecialMsgHandler / CSKSysExMsgHandler. See oa_ckg_midi_msg_handler.h
 * for the full class-graph, offset-math gotchas, and evidence writeup.
 *
 * Every raw-offset access through CKGBankManager::ms_poInstance below was
 * recomputed as `0x9652e0 + N` from Ghidra's own invented
 * `s_aiFlags[ms_poInstance + N]` display (see the header's own comment) and
 * cross-checked against 2 already-committed offsets elsewhere in this
 * project (0x97c747, 0x97c749) that land in the same field cluster.
 *
 * Several call-site argument mappings below were caught wrong by the
 * naive Ghidra decompile and fixed only after re-reading the RAW
 * disassembly (register-by-register): CSKSpecialMsgHandler::
 * ProcessPitchBendMessage()'s real 3-arg SetBendRange() call (Ghidra
 * showed a single bogus arg); ProcessProgramChangeMessage()'s Combi/
 * Program bankId-vs-index parameter order (EDX=bankId, ECX=index --
 * verified independently against CKGBankManager::ChangeKarmaPerfForCombi's
 * OWN prologue, which bounds-checks ECX against 0x7f and stores EDX
 * unchecked) and its 0xfe "sentinel bankId" sub-path that Ghidra collapsed
 * into an apparent no-op duplicate branch; ShouldRecThisParameterChange()'s
 * bar/beat wraparound source register.
 */

#include "oa_ckg_midi_msg_handler.h"
/* CKGChordTrigger's 2 static flags (CheckPadsMIDIOutFilter()'s own gate) --
 * canonical declaration lives in oa_ckg_switch_family.h, not included here
 * (its own #include chain re-enters oa_ckg_control_ui_msg.h in a way that's
 * fragile across translation units). Alias the 2 real mangled symbols
 * directly instead -- confirmed via `nm -C` against ckg_switch_family.o:
 * `_ZN15CKGChordTrigger27ms_bNowGenaratingChordNotesE` /
 * `_ZN15CKGChordTrigger22ms_bNowSendingCCOrNoteE`. Both are DEFINED once,
 * in ckg_switch_family.cpp; this is a declaration-only alias.
 */
extern bool g_CKGChordTrigger_ms_bNowGenaratingChordNotes asm("_ZN15CKGChordTrigger27ms_bNowGenaratingChordNotesE");
extern bool g_CKGChordTrigger_ms_bNowSendingCCOrNote asm("_ZN15CKGChordTrigger22ms_bNowSendingCCOrNoteE");

/* ==================== static storage ==================== */

/* CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange is already
 * DEFINED in ckg_ui_msg_sender.cpp (from before this class had a full
 * definition, when it was still a 1-field opaque stand-in) -- not
 * redefined here to avoid a duplicate-symbol link error. */
bool CSKSpecialMsgHandler::m_NowHandlingPerformanceChangeBySTG;
bool CSKSysExMsgHandler::sm_bNowHandlingProgramChange;

/* ==================== CSKMIDIMsgHandler ==================== */

/* .text+0x343360, 7 bytes -- vtable install only, no other real body;
 * defined out-of-line here, instead of inline in the class body, purely
 * so the manifest generator's address and name heuristics can find it. */
CSKMIDIMsgHandler::CSKMIDIMsgHandler()
{
}

/* .text+0x342e80, 68 bytes. */
void CSKMIDIMsgHandler::ConvertPostMIDINote()
{
	if (*(int *)(CKGBankManager::ms_poInstance + 0x97c74c) != 1)
		return;
	unsigned char st = m_status & 0xf0;
	if (st != 0x90 && st != 0x80)
		return;
	int shifted = (int)*(signed char *)(CKGBankManager::ms_poInstance + 0x97c744)
		+ (int)(signed char)m_data1;
	signed char result;
	if (shifted < 0x80) {
		if (shifted < 0)
			shifted += 0xc;
		result = (signed char)shifted;
	} else {
		result = (signed char)(shifted - 0xc);
	}
	m_data1 = (unsigned char)result;
}

/* .text+0x342ed0, 35 bytes. */
void CSKMIDIMsgHandler::ConvertNoteOnVelocity0IntoNoteOff()
{
	if ((m_status & 0xf0) == 0x90 && m_data2 == 0) {
		m_data2 = 0x40;
		m_status = (unsigned char)((m_status & 0xf) | 0x80);
	}
}

/* .text+0x342f00, 62 bytes. */
void CSKMIDIMsgHandler::MakeKGMidiEvent(KGMidiEvent &ev)
{
	*(unsigned int *)ev.raw = *(unsigned int *)&m_status;
	unsigned char nib = m_flags & 0xf;
	if (nib < 0xe) {
		unsigned int bit = 1u << nib;
		if ((bit & 0x3424) != 0) {
			ev.kind = 0xfe;
			return;
		}
		if ((bit & 0x1a) != 0) {
			ev.kind = *(unsigned char *)(CSKMIDIMsgProcessor::ms_poInstance + 0x20);
			return;
		}
	}
	ev.kind = 0xff;
}

/* .text+0x342f50, 56 bytes -- CSWTCH_42, real 5-entry .rodata table, read
 * directly from ground truth at .rodata offset 0xab0e4. */
int CSKMIDIMsgHandler::CheckAndGetCorrectCCValue()
{
	static const int kCSWTCH_42[5] = { 0x40, 0x00, 0x40, 0x40, 0x40 };
	int result = (signed char)m_data2;
	if ((m_status & 0xf0) == 0xb0 && (signed char)m_data2 == -1) {
		result = 0;
		unsigned char idx = (unsigned char)(m_data1 - 0x11);
		if (idx < 5)
			return kCSWTCH_42[idx];
	}
	return result;
}

/* .text+0x342f90, 36 bytes. */
unsigned char CSKMIDIMsgHandler::CheckPadsMIDIOutFilter()
{
	if (*(unsigned char *)(CKGBankManager::ms_poInstance + 0x97c7ba) == 0)
		return g_CKGChordTrigger_ms_bNowGenaratingChordNotes ^ 1;
	return g_CKGChordTrigger_ms_bNowSendingCCOrNote ^ 1;
}

/* .text+0x342fc0, 23 bytes. */
void CSKMIDIMsgHandler::StoreDyingNoteInfoForMIDPort()
{
	((CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance)->
		StoreDyingNoteInfoForMIDPort((CMIDIMessage *)&m_status);
}

/* .text+0x342fe0, 23 bytes. */
void CSKMIDIMsgHandler::StoreDyingNoteInfoForSTG()
{
	((CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance)->
		StoreDyingNoteInfoForSTG((CMIDIMessage *)&m_status);
}

/* .text+0x343000, 150 bytes. */
void CSKMIDIMsgHandler::RecChannelMessageToSequencer()
{
	if (!CheckPadsMIDIOutFilter())
		return;
	int note = (signed char)m_data1;
	if (*(int *)(CKGBankManager::ms_poInstance + 0x97c74c) == 1) {
		unsigned char st = m_status & 0xf0;
		if (st == 0x90 || st == 0x80) {
			note = note - (int)*(signed char *)(CKGBankManager::ms_poInstance + 0x97c744);
			if (note < 0x80) {
				if (note < 0)
					note += 0xc;
			} else {
				note += -0xc;
			}
		}
	}
	int ccVal = CheckAndGetCorrectCCValue();
	SPRMain_RecChannelMessage(m_status & 0xf0, note, ccVal, m_status & 0xf);
}

/* .text+0x3430a0, 145 bytes. */
unsigned int CSKMIDIMsgHandler::CheckGlobalFilter()
{
	unsigned char st = m_status & 0xf0;
	if (st == 0xd0)
		return (*(unsigned char *)(CKGBankManager::ms_poInstance + 0x97c748) >> 3) & 1;
	if (st == 0xe0) {
		if ((*(unsigned char *)(CKGBankManager::ms_poInstance + 0x97c748) & 0x10) == 0)
			return 0;
		if ((m_flags & 0xf) != 4)
			return 1;
		return *(unsigned char *)(CKGBankManager::ms_poInstance + 0x97c7b9) != 0;
	}
	if (st == 0xb0) {
		if ((*(unsigned char *)(CKGBankManager::ms_poInstance + 0x97c748) & 0x10) == 0)
			return 0;
		return SKSTGGate_CheckVJSCCToMIDIPortFilter((int)(signed char)m_data1, m_flags & 0xf);
	}
	return 1;
}

/* .text+0x343140, 97 bytes. */
void CSKMIDIMsgHandler::SendChannelMessageToMIDIPortWithCorrectLength()
{
	if (!CheckPadsMIDIOutFilter())
		return;
	StoreDyingNoteInfoForMIDPort();
	if ((m_status & 0xf0) != 0xc0 && (m_status & 0xf0) != 0xd0) {
		SKSTGGate_SendToMIDIPort(&m_status, 3);
		return;
	}
	SKSTGGate_SendToMIDIPort(&m_status, 2);
}

/* .text+0x3431b0, 56 bytes. */
void CSKMIDIMsgHandler::SendChannelMessageToSTGWithCorrectLength()
{
	unsigned char buf[0x11];
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 0xfe;
	MakeKGMidiEvent(*reinterpret_cast<KGMidiEvent *>(buf));
	SKSTGGate_SendToSTG(buf, 5);
}

/* .text+0x3431f0, 49 bytes. */
void CSKMIDIMsgHandler::SendChannelMessageToSTG()
{
	((CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance)->
		StoreCCMessage((CMIDIMessage *)&m_status);
	StoreDyingNoteInfoForSTG();
	SendChannelMessageToSTGWithCorrectLength();
}

/* .text+0x343230, 88 bytes. */
void CSKMIDIMsgHandler::ConvertPostMIDIAfterTouch()
{
	if (*(int *)(CKGBankManager::ms_poInstance + 0x97c74c) != 1)
		return;
	unsigned char st = m_status & 0xf0;
	if (st == 0xd0) {
		m_data1 = CAfterTouchConverter_ConvertPostMIDI(
			(int)*(signed char *)(CKGBankManager::ms_poInstance + 0x97c746), m_data1);
	} else if (st == 0xa0) {
		m_data2 = CAfterTouchConverter_ConvertPostMIDI(
			(int)*(signed char *)(CKGBankManager::ms_poInstance + 0x97c746), m_data2);
	}
}

/* .text+0x343290, 102 bytes. */
void CSKMIDIMsgHandler::ConvertPreMIDIAfterTouch()
{
	int curve = 2;
	if (*(int *)(CKGBankManager::ms_poInstance + 0x97c74c) == 0)
		curve = (int)*(signed char *)(CKGBankManager::ms_poInstance + 0x97c746);
	unsigned char st = m_status & 0xf0;
	if (st == 0xd0) {
		m_data1 = CAfterTouchConverter_ConvertPreMIDI(curve, m_data1);
	} else if (st == 0xa0) {
		m_data2 = CAfterTouchConverter_ConvertPreMIDI(curve, m_data2);
	}
}

/* .text+0x343300, 79 bytes. */
void CSKMIDIMsgHandler::ConvertPostMIDIVelocity()
{
	if (*(int *)(CKGBankManager::ms_poInstance + 0x97c74c) != 1)
		return;
	if ((m_status & 0xf0) != 0x90)
		return;
	if (m_data2 != 0) {
		m_data2 = CVelocityConverter_ConvertPostMIDI(
			(int)*(signed char *)(CKGBankManager::ms_poInstance + 0x97c745), m_data2);
	}
}

/* ==================== CSKSpecialMsgHandler ==================== */

/* .text+0x3461a0, 7 bytes -- vtable install only, no other real body;
 * defined out-of-line so the manifest generator's own heuristics can find
 * it, same reasoning as CSKMIDIMsgHandler's own constructor above. */
CSKSpecialMsgHandler::CSKSpecialMsgHandler()
{
}

/* .text+0x345d40, 132 bytes. */
unsigned int CSKSpecialMsgHandler::AnalizeAndProcess(unsigned char *msg)
{
	m_status = msg[0];
	m_data1 = msg[1];
	m_data2 = msg[2];
	m_flags = msg[3];
	if (m_status < 0xf0) {
		unsigned char st = m_status & 0xf0;
		if (st == 0xc0)
			return ProcessProgramChangeMessage();
		if (st == 0xe0)
			return ProcessPitchBendMessage();
		if (st == 0xb0 && m_data1 == 0x79)
			return ProcessResetAllControllerMessage();
	}
	return 0;
}

/* .text+0x345dd0, 10 bytes. */
void CSKSpecialMsgHandler::MakeKGMidiEvent(KGMidiEvent &ev)
{
	*(unsigned int *)ev.raw = *(unsigned int *)&m_status;
	ev.kind = 0xff;
}

/* .text+0x345de0, 107 bytes -- real 3-arg SetBendRange call taking the
 * channel plus the transform of m_data1 and the transform of m_data2,
 * verified via raw disassembly since the naive Ghidra decompile showed
 * only a single, wrongly-attributed arg. `channel` here is `m_status &
 * 0xf`, the status byte's own low nibble -- valid MIDI encoding for a
 * channel-voice status byte -- NOT `m_flags & 0xf` like the rest of this
 * file's channel reads. */
bool CSKSpecialMsgHandler::ProcessPitchBendMessage()
{
	if ((m_flags & 0x10) == 0)
		return false;
	int a = (signed char)m_data1;
	int b = (signed char)m_data2;
	int argA = (a & 0x40) ? (int)(0xffffffc0u | (unsigned int)a) : a;
	int argB = (b & 0x40) ? (int)(0xffffffc0u | (unsigned int)b) : b;
	int channel = m_status & 0xf;
	((CKGEngine *)CKGEngine::ms_poInstance)->SetBendRange(channel, (unsigned int)argA, (unsigned int)argB);
	return true;
}

/* .text+0x345e50, 419 bytes. Real bankId/index parameter mapping (EDX=
 * bankId, ECX=index) confirmed by cross-checking CKGBankManager::
 * ChangeKarmaPerfForCombi's own prologue: it bounds-checks ECX against
 * 0x7f (index-shaped) and stores EDX unchecked (bankId-shaped). Program
 * case has a real sentinel sub-path (m_data2==0xfe or >0x80) passing a
 * literal 0xfffe bankId instead of the raw data2 byte -- collapsed by
 * Ghidra's decompile into an apparently-identical duplicate branch;
 * confirmed genuinely different only via raw disassembly. */
unsigned int CSKSpecialMsgHandler::ProcessProgramChangeMessage()
{
	if ((m_flags & 0x10) == 0)
		return 0;

	CKGRTCHandler *rtc = (CKGRTCHandler *)CKGRTCHandler::ms_poInstance;
	rtc->FlashBufferdValue();
	rtc->StartBuffering();
	m_NowHandlingPerformanceChangeBySTG = true;

	unsigned char data1 = m_data1;
	unsigned char data2 = m_data2;
	int sel = data1 >> 6;	/* top 2 bits of data1 */

	if (sel == 1) {
		/* bankId is ALWAYS data1&0x3f (sentinel or not) -- confirmed
		 * against the callee's own prologue (CKGBankManager::
		 * ChangeKarmaPerfForCombi, same param shape): EDX/param1 is
		 * stored unconditionally (bankId-shaped), ECX/param2 is
		 * bounds-checked against 0x7f (index-shaped). The 0xfe/>0x80
		 * sentinel therefore replaces INDEX with 0xfffe (a deliberate
		 * out-of-range value selecting the callee's own "invalid
		 * index" fallback path), not bankId -- the opposite of what
		 * the naive decompile's apparently-duplicate if/else branches
		 * suggested. */
		unsigned int bankId = data1 & 0x3f;
		unsigned int index;
		if (data2 == 0xfe || data2 > 0x80) {
			m_NowHandlingSamplingPerformanceChange = true;
			index = 0xfffe;
		} else {
			index = data2;
		}
		((CKGBankManager *)CKGBankManager::ms_poInstance)->
			ChangeKarmaPerfForProgram((eSTGProgramBankId)bankId, index);
		((CKGEngine *)CKGEngine::ms_poInstance)->ChangePerformance(eSTGMsgPerfType_Program, false);
		((CDrumTrackBankManager *)CDrumTrackBankManager::ms_poInstance)->
			ChangeProgram((eSTGProgramBankId)bankId, index);
		((CSPREngine *)CSPREngine::ms_poInstance)->ChangePerformance(eSTGMsgPerfType_Program);
		m_NowHandlingSamplingPerformanceChange = false;
	} else if (sel == 2) {
		unsigned int index = data2;
		((CKGBankManager *)CKGBankManager::ms_poInstance)->ChangeKarmaPerfForSeq(index);
		((CKGEngine *)CKGEngine::ms_poInstance)->ChangePerformance(eSTGMsgPerfType_Song, false);
		((CDrumTrackBankManager *)CDrumTrackBankManager::ms_poInstance)->ChangeSeq(index);
		((CSPREngine *)CSPREngine::ms_poInstance)->ChangePerformance(eSTGMsgPerfType_Song);
	} else if (sel == 0) {
		/* Same bankId/index mapping as the Program case above: EDX
		 * (param1=bankId)=data1&0x3f, ECX(param2=index)=data2 raw. */
		unsigned int bankId = data1 & 0x3f;
		unsigned int index = data2;
		((CKGBankManager *)CKGBankManager::ms_poInstance)->
			ChangeKarmaPerfForCombi((eSTGCombiBankId)bankId, index);
		((CKGEngine *)CKGEngine::ms_poInstance)->ChangePerformance(eSTGMsgPerfType_Combi, false);
		((CDrumTrackBankManager *)CDrumTrackBankManager::ms_poInstance)->
			ChangeCombi((eSTGCombiBankId)bankId, index);
		((CSPREngine *)CSPREngine::ms_poInstance)->ChangePerformance(eSTGMsgPerfType_Combi);
	}

	((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->SetCurrentVoiceMode();
	m_NowHandlingPerformanceChangeBySTG = false;
	return 1;
}

/* .text+0x346010, 351 bytes. */
unsigned int CSKSpecialMsgHandler::ProcessResetAllControllerMessage()
{
	if ((m_flags & 0xf) != 5)
		return 1;

	switch (m_data2) {
	case 3: {
		bool keep = ((CKGEngine *)CKGEngine::ms_poInstance)->ShouldKeepKarmaPerformance();
		if (!keep)
			((CKGEngine *)CKGEngine::ms_poInstance)->SendShutUp();
		((CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance)->KillAllDyingNotes();
		((CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance)->KillAllDyingNotes();
		((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->ChangePerformance();
		*(int *)(CKGEngine::ms_poInstance + 0x14) = 2;
		((CKGEngine *)CKGEngine::ms_poInstance)->ClearScheduler();
		unsigned char buf[5] = { 0, 0, 0, 0, 0xfe };
		MakeKGMidiEvent(*reinterpret_cast<KGMidiEvent *>(buf));
		SKSTGGate_SendToSTG(buf, 5);
		return 1;
	}
	case 4: {
		*(int *)(CKGEngine::ms_poInstance + 0x14) = 3;
		unsigned char buf[5] = { 0, 0, 0, 0, 0xfe };
		MakeKGMidiEvent(*reinterpret_cast<KGMidiEvent *>(buf));
		SKSTGGate_SendToSTG(buf, 5);
		return 1;
	}
	case 5:
		*(unsigned char *)(CKGBankManager::ms_poInstance + 0x97c749) = 1;
		((CKGEngine *)CKGEngine::ms_poInstance)->KarmaTurnOnWhenFinishDump();
		return 1;
	case 6:
		((CKGEngine *)CKGEngine::ms_poInstance)->ResetLocalController();
		*(unsigned char *)(CKGBankManager::ms_poInstance + 0x97c749) = 0;
		return 1;
	case 0xd:
		((CKGEngine *)CKGEngine::ms_poInstance)->KarmaTurnOffWhenStartDump();
		return 1;
	case 0xe:
		((CKGEngine *)CKGEngine::ms_poInstance)->KarmaTurnOnWhenFinishDump();
		((CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance)->LeaveDownloadMode();
		return 1;
	case 0x1c:
		((CKGEngine *)CKGEngine::ms_poInstance)->SendCCOffsetBack();
		return 1;
	default:
		return 0;
	}
}

/* ==================== CSKSysExMsgHandler ==================== */

/* .text+0x346880, 33 bytes -- base-class construction plus vtable
 * install only, no other real body; defined out-of-line so the manifest
 * generator's own heuristics can find it. */
CSKSysExMsgHandler::CSKSysExMsgHandler()
{
}

/* .text+0x3461b0, 73 bytes -- loops `len` times calling the 1-byte
 * AnalizeAndProcess(unsigned char) overload with each successive byte of
 * `buf` (confirmed via raw disassembly: EDX reloaded with `buf[i]` before
 * each virtual call -- the naive decompile showed no argument at all). */
unsigned int CSKSysExMsgHandler::AnalizeAndProcess(unsigned char *buf, int len)
{
	unsigned int result = 0;
	if ((unsigned int)(len - 1) < 0x20) {
		for (int i = 0; i < len; i++)
			result = AnalizeAndProcess(buf[i]);
	}
	return result;
}

/* .text+0x346330, 151 bytes. Per-byte SysEx state machine, verified slot-
 * by-slot against this class's own vtable relocation dump. */
unsigned int CSKSysExMsgHandler::AnalizeAndProcess(unsigned char eoxByte)
{
	if (!Analize(eoxByte))
		return 0;

	CopyToBuffer(eoxByte);
	if (SPROutGate_IsEnableExclusive())
		RecToSequencerExceptParameterChange(eoxByte);
	SendToMIDIPortExceptParameterChange(eoxByte);

	if (m_buf[m_bufIndex - 1] == (unsigned char)0xf7 && m_buf[0] == (unsigned char)0xf0) {
		RecParameterChange();
		if (ShouldSendToOtherModules()) {
			SendToMIDIPortParameterChange();
			SendToOtherModules();
		}
	}
	return 1;
}

/* .text+0x3462d0, 73 bytes. */
bool CSKSysExMsgHandler::Analize(unsigned char byte)
{
	if ((signed char)byte == -0x10) {	/* 0xf0, SOX */
		m_inSysEx = true;
		m_bufIndex = 0;
		for (int i = 0; i < 8; i++)
			((unsigned int *)m_buf)[i] = 0;
		return true;
	}
	if (!m_inSysEx)
		return false;
	if ((signed char)byte >= 0)
		return true;			/* normal data byte, still in frame */
	m_inSysEx = false;
	return byte == 0xf7;			/* EOX cleanly closes the frame */
}

/* .text+0x346200, 22 bytes. */
void CSKSysExMsgHandler::CopyToBuffer(unsigned char byte)
{
	m_buf[m_bufIndex] = byte;
	int next = m_bufIndex + 1;
	m_bufIndex = (next != 0x20) ? next : 0;
}

/* .text+0x3464f0, 24 bytes. */
void CSKSysExMsgHandler::RecToSequencerExceptParameterChange(unsigned char byte)
{
	if ((m_flags & 0xf) == 0)
		SPRMain_RecSysExMessageFromMIDIPort(byte);
}

/* .text+0x3463e0, 257 bytes -- transport-position duplicate suppression,
 * plus program-change-inside-SysEx detection using message type 4, subtype
 * 8 or 9. The wraparound source register -- `loc[1]`, the 3rd
 * GetCurrentLocation/GetPrecountLocation out-param -- and the +1 bar
 * adjustment on wraparound were both confirmed only via raw disassembly;
 * the naive decompile conflated them with the wrong local and dropped the
 * +1 entirely. */
bool CSKSysExMsgHandler::ShouldRecThisParameterChange(CSKParameterChangeMessage *msg)
{
	int loc[4];	/* [0]=stack local 0x10 (4th out-param), [1]=0x14 (3rd),
			 * [2]=0x18 (2nd, "beat"), [3]=0x1c (1st, "bar") -- indices
			 * match the real stack-slot layout from disassembly;
			 * exact musical meaning of CSPRClockHandler's 4 outputs
			 * not independently confirmed beyond this ordering. */
	if ((CSPRClockHandler::ms_oStatusMaster & 0x40) == 0)
		((CSPRClockHandler *)CSPRClockHandler::ms_poInstance)->
			GetCurrentLocation(&loc[3], &loc[2], &loc[1], &loc[0]);
	else
		((CSPRClockHandler *)CSPRClockHandler::ms_poInstance)->
			GetPrecountLocation(&loc[3], &loc[2], &loc[1], &loc[0]);

	unsigned char msgType = *((unsigned char *)msg + 5);
	bool result;
	if (msgType == 4 && (*((unsigned char *)msg + 8) == 8 || *((unsigned char *)msg + 8) == 9)) {
		sm_bNowHandlingProgramChange = true;
		result = true;
	} else {
		int bar = loc[3];
		int beat = loc[2];
		int prevBeat = beat - 2;
		int barCmp = bar;
		if (prevBeat < 0) {
			prevBeat += loc[1];
			barCmp = bar + 1;
		}
		if (barCmp > m_lastBar || (barCmp == m_lastBar && prevBeat > m_lastBeat))
			sm_bNowHandlingProgramChange = false;
		result = true;
		if (msgType == 0x1c)
			result = !sm_bNowHandlingProgramChange;
	}
	m_lastBar = loc[3];
	m_lastBeat = loc[2];
	return result;
}

/* .text+0x346510, 316 bytes. Both the automation-track side effect and
 * the "should record" gate converge onto the same shared tail regardless
 * of which fires -- confirmed via raw disassembly, not obvious from the
 * naive decompile's apparent 2-way branch shape. */
void CSKSysExMsgHandler::RecParameterChange()
{
	if ((m_flags & 0xf) != 1) {
		return;
	}

	CSKParameterChangeMessage *msg = (CSKParameterChangeMessage *)m_buf;
	if (msg->IsThisParamChage() && m_buf[4] == 'C') {
		if (m_buf[5] == ' ') {
			unsigned char velocityByte = m_buf[6];
			unsigned int value = msg->GetValue();
			int kind;
			if (SPROutGate_GetAutomationSysExEventKind(m_buf[8], &kind))
				SPRMain_RecSysExMessageOnAutomationTrack(velocityByte, kind, value);
		}
		if (ShouldRecThisParameterChange(msg)) {
			for (int i = 0; i < 14; i++)
				SPRMain_RecInternalSysExMessage(m_buf[i]);
		}
		return;
	}

	if (m_bufIndex > 0) {
		for (int i = 0; i < m_bufIndex; i++)
			SPRMain_RecInternalSysExMessage(m_buf[i]);
	}
}

/* .text+0x346660, 517 bytes -- dispatches by the buffer's own
 * manufacturer-ID-shaped bytes. `sendToSTGCommon` mirrors the real ground
 * truth's shared goto target, LAB_0035672b: reaching it via the goto SKIPS
 * the CSPREngine-less-than-7 guard entirely, while falling through to the
 * same tail normally still applies it -- both eventually call
 * StoreSPRParamChange, only the guard differs. */
void CSKSysExMsgHandler::SendToOtherModules()
{
	CSKParameterChangeMessage *msg = (CSKParameterChangeMessage *)m_buf;
	char exclusive = *((char *)CSTGMessageProcessor::sInstance + 0x57);

	if (msg->IsThisParamChage() && m_buf[4] == 'm') {
		if (exclusive == 0)
			return;
		unsigned char flags = m_flags;
		if ((flags & 0xf) == 0) {
			KGMain_ReceiveParameterChangeMessageFromMIDIPort(m_buf);
			flags = m_flags;
		}
		if ((flags & 0xf) != 2 && *CSPREngine::ms_poInstance < 7)
			return;
		if ((flags & 0xf) == 2) {
			KGMain_ReceiveParameterChangeMessageFromSeqEvent(m_buf);
			flags = m_flags;
			if ((flags & 0xf) != 2 && *CSPREngine::ms_poInstance < 7)
				return;
		}
		StoreKGParamChange();
		return;
	}

	bool sendToSTGCommon = false;

	if (msg->IsThisParamChage() && m_buf[4] == 'A') {
		if (exclusive == 0)
			return;
		unsigned char flags = m_flags;
		if ((flags & 0xf) == 0) {
			SPRMain_ReceiveParameterChangeMessageFromMIDIPort(m_buf);
			flags = m_flags;
		}
		if ((flags & 0xf) == 2) {
			SPRMain_ReceiveParameterChangeMessageFromSeqEvent(m_buf);
			flags = m_flags;
			if ((flags & 0xf) == 2)
				sendToSTGCommon = true;
		}
	} else if (msg->IsThisParamChage() && m_buf[4] == 'n') {
		if (exclusive == 0)
			return;
		unsigned char flags = m_flags;
		if ((flags & 0xf) == 0) {
			SPRMain_ReceiveDrumTrackParameterChangeMessageFromMIDIPort(m_buf);
			flags = m_flags;
		}
		if ((flags & 0xf) == 2) {
			SPRMain_ReceiveDrumTrackParameterChangeMessageFromSeqEvent(m_buf);
			sendToSTGCommon = true;
		}
	} else if (msg->IsThisParamChage() && m_buf[4] == 'C') {
		if (exclusive == 0)
			return;
		SendToSTG();
		if ((m_flags & 0xf) != 2 && *CSPREngine::ms_poInstance < 7)
			return;
		StoreSTGParamChange();
		return;
	} else {
		if (m_buf[1] == 'B' &&
		    m_buf[2] == (unsigned char)((*(signed char *)(CKGBankManager::ms_poInstance + 0x97c747)) | 0x30) &&
		    m_buf[3] == 'h' &&
		    *(short *)&m_buf[4] == 0x7f7f) {
			if ((m_flags & 0xf) != 0)
				return;
			KGMain_ReceiveKarmaDisableInputMessage(m_buf, m_bufIndex);
			return;
		}
		SendToSTG();
		return;
	}

	if (!sendToSTGCommon && *CSPREngine::ms_poInstance < 7)
		return;
	StoreSPRParamChange();
}

/* .text+0x346220, 1 byte -- empty. */
void CSKSysExMsgHandler::SendToMIDIPortParameterChange()
{
}

/* .text+0x346230, 1 byte -- empty. */
void CSKSysExMsgHandler::SendToMIDIPortExceptParameterChange(unsigned char)
{
}

/* .text+0x346320, 15 bytes -- see header comment: return value is a
 * genuine, if Ghidra-hidden, tail-call passthrough. */
bool CSKSysExMsgHandler::ShouldSendToOtherModules()
{
	return SPROutGate_IsEnableExclusive();
}

/* .text+0x346240, 34 bytes. */
void CSKSysExMsgHandler::SendToSTG()
{
	if ((m_flags & 0xf) != 1)
		SKSTGGate_SendToSTG(m_buf, m_bufIndex);
}

/* .text+0x3462b0, 23 bytes. */
void CSKSysExMsgHandler::StoreKGParamChange()
{
	((CSPRSysExBufManager *)CSPRMIDIMsgProcessor::ms_poSysExPlayBuf)->
		SetValue((CSKParameterChangeMessage *)m_buf);
}

/* .text+0x346290, 23 bytes. */
void CSKSysExMsgHandler::StoreSPRParamChange()
{
	((CSPRSysExBufManager *)CSPRMIDIMsgProcessor::ms_poSysExPlayBuf)->
		SetValue((CSKParameterChangeMessage *)m_buf);
}

/* .text+0x346270, 23 bytes. */
void CSKSysExMsgHandler::StoreSTGParamChange()
{
	((CSPRSysExBufManager *)CSPRMIDIMsgProcessor::ms_poSysExPlayBuf)->
		SetValue((CSKParameterChangeMessage *)m_buf);
}

/* .text+0x3468b0, 8 bytes. */
void CSKSysExMsgHandler::StartRecording()
{
	sm_bNowHandlingProgramChange = false;
}

/* .text+0x3468c0, 22 bytes. */
bool CSKSysExMsgHandler::IsThisEnableForX2100()
{
	if (m_buf[m_bufIndex - 1] != (unsigned char)0xf7)
		return false;
	return m_buf[0] == (unsigned char)0xf0;
}

/* .text+0x3468e0, 45 bytes. */
bool CSKSysExMsgHandler::IsThisKGParamChage()
{
	CSKParameterChangeMessage *msg = (CSKParameterChangeMessage *)m_buf;
	if (!msg->IsThisParamChage())
		return false;
	return m_buf[4] == 'm';
}

/* .text+0x346910, 45 bytes. */
bool CSKSysExMsgHandler::IsThisSPRParamChage()
{
	CSKParameterChangeMessage *msg = (CSKParameterChangeMessage *)m_buf;
	if (!msg->IsThisParamChage())
		return false;
	return m_buf[4] == 'A';
}

/* .text+0x346940, 45 bytes. */
bool CSKSysExMsgHandler::IsThisDrumTrackParamChage()
{
	CSKParameterChangeMessage *msg = (CSKParameterChangeMessage *)m_buf;
	if (!msg->IsThisParamChage())
		return false;
	return m_buf[4] == 'n';
}

/* .text+0x346970, 45 bytes. */
bool CSKSysExMsgHandler::IsThisSTGParamChage()
{
	CSKParameterChangeMessage *msg = (CSKParameterChangeMessage *)m_buf;
	if (!msg->IsThisParamChage())
		return false;
	return m_buf[4] == 'C';
}

/* .text+0x3469a0, 57 bytes. */
bool CSKSysExMsgHandler::IsThisKarmaDisableInput()
{
	if (m_buf[1] != 'B')
		return false;
	if (m_buf[2] != (unsigned char)((*(signed char *)(CKGBankManager::ms_poInstance + 0x97c747)) | 0x30))
		return false;
	if (m_buf[3] != 'h')
		return false;
	return *(short *)&m_buf[4] == 0x7f7f;
}

/* .text+0x3469e0, 99 bytes. */
void CSKSysExMsgHandler::ProcessKGParamChange()
{
	unsigned char flags = m_flags;
	if ((flags & 0xf) == 0) {
		KGMain_ReceiveParameterChangeMessageFromMIDIPort(m_buf);
		flags = m_flags;
	}
	bool consumedBySeq = false;
	if ((flags & 0xf) == 2) {
		KGMain_ReceiveParameterChangeMessageFromSeqEvent(m_buf);
		consumedBySeq = (m_flags & 0xf) == 2;
	}
	if (consumedBySeq || *CSPREngine::ms_poInstance > 6)
		StoreKGParamChange();
}

/* .text+0x346a50, 99 bytes. */
void CSKSysExMsgHandler::ProcessSPRParamChange()
{
	unsigned char flags = m_flags;
	if ((flags & 0xf) == 0) {
		SPRMain_ReceiveParameterChangeMessageFromMIDIPort(m_buf);
		flags = m_flags;
	}
	bool consumedBySeq = false;
	if ((flags & 0xf) == 2) {
		SPRMain_ReceiveParameterChangeMessageFromSeqEvent(m_buf);
		consumedBySeq = (m_flags & 0xf) == 2;
	}
	if (consumedBySeq || *CSPREngine::ms_poInstance > 6)
		StoreSPRParamChange();
}

/* .text+0x346ac0, 99 bytes. */
void CSKSysExMsgHandler::ProcessDrumTrackaramChange()
{
	unsigned char flags = m_flags;
	if ((flags & 0xf) == 0) {
		SPRMain_ReceiveDrumTrackParameterChangeMessageFromMIDIPort(m_buf);
		flags = m_flags;
	}
	bool consumedBySeq = false;
	if ((flags & 0xf) == 2) {
		SPRMain_ReceiveDrumTrackParameterChangeMessageFromSeqEvent(m_buf);
		consumedBySeq = (m_flags & 0xf) == 2;
	}
	if (consumedBySeq || *CSPREngine::ms_poInstance > 6)
		StoreSPRParamChange();
}

/* .text+0x346b30, 55 bytes. */
void CSKSysExMsgHandler::ProcessSTGParamChange()
{
	SendToSTG();
	if ((m_flags & 0xf) == 2 || *CSPREngine::ms_poInstance > 6)
		StoreSTGParamChange();
}

/* .text+0x346b70, 27 bytes. */
void CSKSysExMsgHandler::ProcessKarmaDisableInput()
{
	if ((m_flags & 0xf) == 0)
		KGMain_ReceiveKarmaDisableInputMessage(m_buf, m_bufIndex);
}
