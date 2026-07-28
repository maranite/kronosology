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
#include "oa_internal.h"	/* placement operator new(size_t, void*), used by
				 * CSKMIDIInMsgHandler's own ctor below */
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

/*
 * ==================== UPDATE 2026-07-28: CSKMIDIInMsgHandler and its 5
 * children ====================
 * See oa_ckg_midi_msg_handler.h's own "UPDATE" comment for the full
 * class-graph writeup and evidence. Every `call [edx+N]` in the raw
 * disassembly below was resolved via `rodata_offset = N + 8` against
 * each class's own captured vtable relocation dump before being written
 * as a plain C++ virtual call -- not eyeballed.
 */

bool CSKMIDIInMsgHandler::ms_bShouldStopSendingNoteOnsToSTG;

/* .text+0x354210, 922 bytes -- fully unrolled in ground truth (16
 * straight-line repetitions for the 2 CDyingNoteInfo arrays + the 4
 * per-channel field groups, 128 straight-line repetitions for the 3
 * per-note byte/dword arrays); reconstructed here as plain loops since
 * loop order has no effect on the final zeroed state. */
CSKMIDIInMsgHandler::CSKMIDIInMsgHandler() : CSKMIDIMsgHandler()
{
	/* Real ground truth allocates exactly 0x3c (60) bytes -- NOT
	 * `sizeof(CSKSysExMsgHandler)`, which host g++ computes as 56 here
	 * because that class's own (pre-existing, prior-batch) declaration
	 * has no explicit padding field for the real 4-byte gap between
	 * CSKMIDIMsgHandler's 8-byte base and CSKSysExMsgHandler's own
	 * m_bufIndex at ground-truth offset +0xc. Using the literal keeps
	 * *this* function's own allocation faithful regardless of that
	 * unrelated class's C++ layout. */
	unsigned char *buf = CSTGBankMemory::AllocAligned(0x3c, 0x10);
	m_sysExHandler = new (buf) CSKSysExMsgHandler();

	for (int i = 0; i < 128; i++) {
		m_noteDownCount[i] = 0;
		m_bypassKarmaNoteOnEvent[i] = 0;
		m_noteOnHoldCount[i] = 0;
	}

	m_noteOnCount = 0;
	m_bDamperOn = false;
	m_bSostenutoOn = false;
	m_softPedal = false;

	for (int i = 0; i < 16; i++) {
		m_lastNotePerChannel[i] = 0xff;
		m_dyingNoteMIDIPort[i].Initialize();
		m_dyingNoteSTG[i].Initialize();
		m_dyingDamperTicks[i] = 0;
		m_dyingDamperFlag[i] = 0;
	}

	ms_bShouldStopSendingNoteOnsToSTG = false;
}

/* .text+0x353390, 93 bytes -- NOT pure, a real base body, see header. */
void CSKMIDIInMsgHandler::ProcessPadTriggerNote()
{
	if (CKGBankManager::ms_poInstance[0x97c7ba] != 0)
		return;
	if ((m_flags & 0xf) == 1)
		SendChannelMessageToMIDIPortWithCorrectLength();
	ConvertPostMIDINote();
	if (CheckGlobalParameterPreSendToSTG())
		RecChannelMessageToSequencer();
}

/* .text+0x353400, 56 bytes. */
bool CSKMIDIInMsgHandler::ShouldSendChannelMessageToKarmaEngine()
{
	unsigned char statusType = m_status & 0xf0;
	if (statusType == 0xc0)
		return true;
	if (statusType == 0xb0) {
		if (m_data1 == 0 || m_data1 == 0x20)
			return true;
		return ShouldRecChannelMessageToSequencer();
	}
	return ShouldRecChannelMessageToSequencer();
}

/* .text+0x353440, 116 bytes -- +0xc/+0x8c are m_noteDownCount and
 * m_noteOnCount, already-established field names matching the ctor's
 * own zero-init of both. Calls the still-pure NotifyNoteCountToUI,
 * rodata 0x9c, call_off 0x94. */
void CSKMIDIInMsgHandler::StoreNoteEvent()
{
	unsigned char statusType = m_status & 0xf0;
	if (statusType != 0x90 && statusType != 0x80) {
		NotifyNoteCountToUI();
		return;
	}

	int note = m_data1;
	if (statusType == 0x90) {
		m_noteDownCount[note]++;
		m_noteOnCount++;
	} else {
		if (m_noteDownCount[note] != 0) {
			m_noteDownCount[note]--;
		} else if (m_noteOnCount <= 0) {
			return;
		} else {
			m_noteOnCount--;
		}
	}
	NotifyNoteCountToUI();
}

/* .text+0x3534d0, 59 bytes. */
void CSKMIDIInMsgHandler::CheckDamperStatus()
{
	if ((m_status & 0xf0) != 0xb0)
		return;
	if (m_data1 != 0x40)
		return;
	bool on = (m_data2 > 0x3f);
	if (m_bDamperOn == on)
		return;
	m_bDamperOn = on;
	NotifyDamperStatusToUI();
}

/* .text+0x353510, 59 bytes. */
void CSKMIDIInMsgHandler::CheckSostenutoStatus()
{
	if ((m_status & 0xf0) != 0xb0)
		return;
	if (m_data1 != 0x42)
		return;
	bool on = (m_data2 > 0x3f);
	if (m_bSostenutoOn == on)
		return;
	m_bSostenutoOn = on;
	NotifySostenutoStatusToUI();
}

/* .text+0x353550, 1 byte -- empty. */
void CSKMIDIInMsgHandler::NotifyDamperStatusToUI()
{
}

/* .text+0x353560, 1 byte -- empty. */
void CSKMIDIInMsgHandler::NotifySostenutoStatusToUI()
{
}

/* .text+0x353570, 8 bytes. */
bool CSKMIDIInMsgHandler::IsDamperOn()
{
	return m_bDamperOn;
}

/* .text+0x353580, 8 bytes. */
bool CSKMIDIInMsgHandler::IsSostenutoOn()
{
	return m_bSostenutoOn;
}

/* .text+0x353590, 6 bytes. */
bool CSKMIDIInMsgHandler::CheckDuplicateMessage()
{
	return true;
}

/* .text+0x3535a0, 134 bytes -- uses regparm 3 calling convention. */
bool CSKMIDIInMsgHandler::AnalizeAndProcessNoteOffWhilePerformanceChange(unsigned char *buf, int len)
{
	if (!AnalizeAndSetParameter(buf, len))
		return false;

	ConvertNoteOnVelocity0IntoNoteOff();
	if ((m_status & 0xf0) != 0x80)
		return false;

	StoreNoteEvent();
	ConvertPreMIDINote();
	if (ShouldSendChannelMessageToMIDIPort())
		SendChannelMessageToMIDIPort();

	ConvertPostMIDINote();
	ConvertPostMIDIVelocity();
	m_flags |= 0x40;
	SendChannelMessageToSTG();
	return true;
}

/* .text+0x353630, 41 bytes. */
void CSKMIDIInMsgHandler::ReserveBypassKARMANoteOnEvent(int note)
{
	unsigned char statusType = m_status & 0xf0;
	if (statusType == 0x90)
		m_bypassKarmaNoteOnEvent[note] = *(unsigned int *)&m_status;
	else if (statusType == 0x80)
		m_bypassKarmaNoteOnEvent[note] = 0;
}

/* .text+0x353670, 176 bytes. */
bool CSKMIDIInMsgHandler::CheckBypassKARMANoteOnEvent(int note)
{
	if ((m_status & 0xf0) != 0x80)
		return false;

	unsigned char offVelocity = m_data2;
	unsigned int origEvent = *(unsigned int *)&m_status;
	CSKMIDIMsgHandler scratch;	/* real ground truth places a scratch
					 * CSKMIDIMsgHandler on the stack here
					 * and never reads it again afterward --
					 * kept for faithful side effects
					 * (install-only ctor, no observable
					 * effect either way). */

	unsigned int reserved = m_bypassKarmaNoteOnEvent[note];
	bool result = false;
	if ((reserved & 0xf0) == 0x90 && (reserved & 0xf) == (m_status & 0xf)) {
		*(unsigned int *)&m_status = reserved;
		m_status = 0x80 | (reserved & 0xf);
		m_data2 = offVelocity;
		SendChannelMessageToSTG();
		*(unsigned int *)&m_status = origEvent;
		result = true;
	}
	m_bypassKarmaNoteOnEvent[note] = 0;
	return result;
}

/* .text+0x353730, 216 bytes. */
bool CSKMIDIInMsgHandler::CheckDyingNoteForMIDIPort()
{
	int channel = m_status & 0xf;
	int note = (signed char)m_data1;
	unsigned char statusType = m_status & 0xf0;

	if (statusType == 0x90) {
		m_dyingNoteSTG[channel].TurnOn(note);
		return true;
	}
	if (statusType != 0x80)
		return true;

	if (m_dyingNoteMIDIPort[channel].IsNoteOn(note)) {
		ProcessForDyingNote();
		m_dyingNoteMIDIPort[channel].TurnOff(note);
		return false;
	}
	if (m_dyingNoteSTG[channel].IsNoteOn(note)) {
		m_dyingNoteSTG[channel].TurnOff(note);
		return true;
	}
	return false;
}

/* .text+0x353820, 67 bytes. */
void CSKMIDIInMsgHandler::ProcessForDyingNote()
{
	((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->SetStatus(CMIDIFlowParamHolder::eStatus_1);
	if (ShouldSendChannelMessageToMIDIPort())
		SendChannelMessageToMIDIPort();
	((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->SetStatus(CMIDIFlowParamHolder::eStatus_0);
}

/* .text+0x353870, 227 bytes. */
bool CSKMIDIInMsgHandler::IsEnableViaRPPR()
{
	unsigned char *bm = CKGBankManager::ms_poInstance;

	if (((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->GetVoiceMode() != 2) {
		int statusChannel = m_status & 0xf;
		if (statusChannel != (signed char)bm[0x97c747])
			return true;
	}

	if (bm[0x97c749] == 0 && (m_flags & 0xf) != 0)
		return true;

	unsigned char statusType = m_status & 0xf0;
	if (statusType == 0x90)
		return SPRMain_KeyboardOn(true, m_data1, m_status & 0xf, m_data2);
	if (statusType != 0x80)
		return true;
	return SPRMain_KeyboardOn(false, m_data1, m_status & 0xf, m_data2);
}

/* .text+0x353970, 235 bytes -- CKGBankManager::ms_poInstance[+8] is
 * itself a POINTER, dereferenced once, to a separate note-display
 * buffer, NOT the giant opaque blob's own raw offsets like every other
 * `ms_poInstance[N]` access in this project -- a real, confirmed extra
 * indirection. */
void CSKMIDIInMsgHandler::NotifyNoteEventToUI()
{
	unsigned char statusType = m_status & 0xf0;
	if (statusType != 0x90 && statusType != 0x80)
		return;

	unsigned char *sub = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
	int note = (signed char)m_data1;

	if (statusType == 0x80) {
		((CKGEventDisplayManager *)CKGEngine::ms_poKGEventDisplayManager)->NoteOff(note);
		sub[0x147a6] = m_data1;
		sub[0x147a7] = 0;
		if (note < 0) {
			for (int i = 0; i < 0x80; i++)
				sub[0x147a8 + i] = 0;
			sub[0x147a6] = 0;
			sub[0x147a7] = 0;
		} else {
			sub[0x147a8 + note] = 0;
		}
	} else {
		((CKGEventDisplayManager *)CKGEngine::ms_poKGEventDisplayManager)->NoteOn(note);
		sub[0x147a6] = m_data1;
		sub[0x147a7] = m_data2;
		if (note < 0) {
			for (int i = 0; i < 0x80; i++)
				sub[0x147a8 + i] = 0;
			sub[0x147a6] = 0;
			sub[0x147a7] = 0;
		} else {
			sub[0x147a8 + note] = m_data2;
		}
	}
}

/* .text+0x353a60, 100 bytes. */
void CSKMIDIInMsgHandler::CheckSoftPedalStatus()
{
	unsigned char *bm = CKGBankManager::ms_poInstance;
	if ((m_status & 0xf) != (signed char)bm[0x97c747])
		return;
	if ((m_status & 0xf0) != 0xb0)
		return;
	if (m_data1 != 0x43)
		return;

	bool on = (m_data2 > 0x3f);
	m_softPedal = on;
	if (CKGControlMsgHandler::ms_bIsNowProcessingSoftPedalMessage)
		return;

	CKGUIMsgSender *sender = (CKGUIMsgSender *)(CKGUIMsgProcessor::ms_poInstance + 0x5c);
	sender->UpdateSoftPedalStatus(on);
}

/* .text+0x353ad0, 352 bytes. */
bool CSKMIDIInMsgHandler::ShouldRecChannelMessageToSequencer()
{
	bool result = CheckGlobalParameterPreSendToSTG();
	unsigned char statusType = m_status & 0xf0;

	if (statusType == 0xc0)
		return true;
	if (statusType == 0xb0 && (m_data1 == 0 || m_data1 == 0x20))
		return SKSTGGate_CheckVJSCCToMIDIPortFilter((signed char)m_data1, m_flags & 0xf);

	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	int n = mf->GetNumOfKARMAModule();
	for (int i = 0; i < n; i++) {
		int in = mf->GetKARMARealInputChannel(i);
		int out = mf->GetKARMARealOutputChannel(i);
		int loc = mf->GetRealInputLocalControllerChannel(i);
		int statusChannel = m_status & 0xf;

		if (((out == in) || (out == loc)) && out == statusChannel) {
			if (mf->IsKARMAOn() || mf->IsKARMATimbreThruInternalAction(i))
				return false;
		}
	}

	if (!result)
		return false;
	if (statusType == 0xb0)
		return SKSTGGate_CheckVJSCCToMIDIPortFilter((signed char)m_data1, m_flags & 0xf);
	if (statusType == 0xe0 && (m_flags & 0xf) == 4)
		return CKGBankManager::ms_poInstance[0x97c7b9] != 0;
	return result;
}

/* .text+0x353c40, 316 bytes -- same per-module KARMA-channel scan as
 * ShouldRecChannelMessageToSequencer above, but a DIFFERENT tail gate:
 * m_flags&0xf==4 plus a GetLocalControlChannel match, no CC/PitchBend
 * distinction at all. */
bool CSKMIDIInMsgHandler::ShouldSendChannelMessageToSTG()
{
	bool result = CheckGlobalParameterPreSendToSTG();
	unsigned char statusType = m_status & 0xf0;

	if (statusType == 0xc0)
		return true;
	if (statusType == 0xb0 && (m_data1 == 0 || m_data1 == 0x20))
		return SKSTGGate_CheckVJSCCToMIDIPortFilter((signed char)m_data1, m_flags & 0xf);

	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	int n = mf->GetNumOfKARMAModule();
	for (int i = 0; i < n; i++) {
		int in = mf->GetKARMARealInputChannel(i);
		int out = mf->GetKARMARealOutputChannel(i);
		int loc = mf->GetRealInputLocalControllerChannel(i);
		int statusChannel = m_status & 0xf;

		if (((out == in) || (out == loc)) && out == statusChannel) {
			if (mf->IsKARMAOn() || mf->IsKARMATimbreThruInternalAction(i))
				return false;
		}
	}

	if (!result)
		return false;
	if ((m_flags & 0xf) != 4)
		return true;
	if ((m_status & 0xf) != mf->GetLocalControlChannel())
		return false;
	return result;
}

/* .text+0x353d90, 60 bytes. */
void CSKMIDIInMsgHandler::SendChannelMessageToKarmaEngine()
{
	CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
	eng->SendChannelMessage(m_status & 0xf0, m_status & 0xf, (signed char)m_data1, (signed char)m_data2);
}

/* .text+0x353dd0, 95 bytes -- NOT pure at this level, see header. The
 * 5th, "EChangeSource", argument to CKGRTCHandler::
 * AnalizeAndProcessNoteMessage is really the STACK-arg-5 computed
 * value below, NOT m_data1 -- a naive Ghidra-order reading gets this
 * backwards: data1/data2 are stack args 3/4, the computed 1-or-2 value
 * is stack arg 5, confirmed by the real store offsets [esp]/[esp+4]/
 * [esp+8] at the real call site. */
bool CSKMIDIInMsgHandler::CheckNoteMessageAndTriggerPad()
{
	if (!ShouldNotifyToKarmaController())
		return false;

	int src = ((m_flags & 0xf) == 0) ? 2 : 1;
	CKGRTCHandler *rtc = (CKGRTCHandler *)CKGRTCHandler::ms_poInstance;
	return rtc->AnalizeAndProcessNoteMessage(m_status & 0xf, m_status & 0xf0,
						  (signed char)m_data1, (signed char)m_data2, src);
}

/* .text+0x353e30, 81 bytes -- same call shape as
 * CheckNoteMessageAndTriggerPad above, no return-value use. */
void CSKMIDIInMsgHandler::NotifyCCToKarmaController()
{
	int src = ((m_flags & 0xf) == 0) ? 2 : 1;
	CKGRTCHandler *rtc = (CKGRTCHandler *)CKGRTCHandler::ms_poInstance;
	rtc->AnalizeAndProcessCCMessage(m_status & 0xf, m_status & 0xf0,
					 (signed char)m_data1, (signed char)m_data2, src);
}

/* .text+0x353e90, 652 bytes -- the real ground-truth `edi`/`consumed`
 * local is reused across the NoteOn/NoteOff/generic sub-paths: for a
 * NoteOff whose per-note hold counter was already nonzero, it becomes
 * the CKGBankManager::ms_poInstance[0x97c749] gate-flag value itself,
 * 0 or 1, preserved -- not reset -- into the shared StoreNoteEvent-
 * onward tail; every other path resets it to 0 first. See the header's
 * own writeup for the full derivation -- this function needed the most
 * careful re-derivation of the whole batch, an earlier pass on this
 * exact function twice mis-resolved 2 different `call [edx+N]` targets
 * before insisting on the definitive per-class call_off table. */
void CSKMIDIInMsgHandler::Process()
{
	if (m_status > 0xef)
		return;

	ConvertNoteOnVelocity0IntoNoteOff();
	unsigned char statusType = m_status & 0xf0;
	int note = (signed char)m_data1;
	bool consumed = false;

	if (statusType == 0x90) {
		if (*(int *)(CKGEngine::ms_poInstance + 0x14) != 4)
			return;
		if (ms_bShouldStopSendingNoteOnsToSTG)
			return;
		if (CKGBankManager::ms_poInstance[0x97c749] == 0)
			m_noteOnHoldCount[note]++;
	} else if (statusType == 0x80) {
		if (m_noteOnHoldCount[note] != 0) {
			m_noteOnHoldCount[note]--;
			consumed = (CKGBankManager::ms_poInstance[0x97c749] != 0);
			if (consumed)
				SKSTGGate_StartMonitorSTGQueue();
		}
	}

	StoreNoteEvent();
	CheckDamperStatus();
	CheckSostenutoStatus();
	CheckSoftPedalStatus();
	ConvertPreMIDINote();
	ConvertPreMIDIAfterTouch();

	if (CheckDuplicateMessage() && IsEnableViaRPPR()) {
		bool triggered = false;
		if (CheckNoteMessageAndTriggerPad()) {
			ProcessPadTriggerNote();
			triggered = true;
		}
		if (!triggered) {
			if (CheckDyingNoteForMIDIPort()) {
				if (ShouldSendChannelMessageToMIDIPort())
					SendChannelMessageToMIDIPort();
			}
			if (ShouldNotifyToKarmaController())
				NotifyCCToKarmaController();

			ConvertPostMIDINote();
			if (ShouldSendChannelMessageToKarmaEngine())
				SendChannelMessageToKarmaEngine();
			if (ShouldSendChannelMessageToSTG())
				RecChannelMessageToSequencer();
			if (ShouldSendChannelMessageToMIDIPort()) {
				ConvertPostMIDIAfterTouch();
				ConvertPostMIDIVelocity();

				CKGEngine *eng = (CKGEngine *)CKGEngine::ms_poInstance;
				bool forceBypass = eng->ShouldForceTimbreZoneBypass(m_status & 0xf, m_flags & 0xf);
				if (forceBypass)
					m_flags |= 0x40;
				else
					m_flags &= ~0x40;

				bool karmaOn = ((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->IsKARMAOn();
				if (karmaOn)
					m_flags |= 0x20;
				else
					m_flags &= ~0x20;

				SendChannelMessageToSTG();
				ReserveBypassKARMANoteOnEvent(note);
			}
		}
	}

	NotifyNoteEventToUI();
	CheckBypassKARMANoteOnEvent(note);

	if (consumed) {
		if (!SKSTGGate_EndMonitorSTGQueue())
			SendChannelMessageToSTG();
	}
}

/* .text+0x354130, 209 bytes -- uses regparm 3 calling convention. */
bool CSKMIDIInMsgHandler::AnalizeAndProcess(unsigned char *buf, int len)
{
	if (m_sysExHandler->AnalizeAndProcess(buf, len))
		return true;

	if (!AnalizeAndSetParameter(buf, len))
		return false;

	unsigned char flagsChannel = m_flags & 0xf;
	if (flagsChannel == 2) {
		SPRMain_RecAutomationTrackMessage(m_status & 0xf0, (signed char)m_data1,
						   (signed char)m_data2, m_status & 0xf);
	} else if (flagsChannel == 0xa) {
		SPRMain_RecMIDITrackMessage(m_status & 0xf0, (signed char)m_data1,
					     (signed char)m_data2, m_status & 0xf);
	} else {
		Process();
	}
	return true;
}

/* .text+0x3445b0, 631 bytes. */
void CSKMIDIInMsgHandler::KillAllDyingNotes()
{
	for (int ch = 0; ch < 16; ch++) {
		if (m_dyingNoteMIDIPort[ch].IsAnyNotesOn()) {
			m_status = (m_status & 0xf0) | ch;
			for (int note = 0; note < 128; note++) {
				if (m_dyingNoteMIDIPort[ch].IsNoteOn(note)) {
					m_data1 = (unsigned char)note;
					ProcessForDyingNote();
					m_dyingNoteMIDIPort[ch].TurnOff(note);
				}
			}
		}
	}

	/* real ground truth: per-channel `rep movs` (sizeof(CDyingNoteInfo)
	 * = 0x84) copying the STG-side snapshot into the MIDI-port-side
	 * array, then resetting the STG-side slot -- reconstructed as a
	 * plain struct assignment (CDyingNoteInfo is a trivially-copyable
	 * byte blob). */
	for (int ch = 0; ch < 16; ch++) {
		m_dyingNoteMIDIPort[ch] = m_dyingNoteSTG[ch];
		m_dyingNoteSTG[ch].Initialize();
	}
}

/* .text+0x354850, 46 bytes. */
void CSKMIDIInMsgHandler::ClearKeyboardStatus()
{
	for (int i = 0; i < 128; i++)
		m_noteDownCount[i] = 0;
	m_noteOnCount = 0;
	NotifyNoteCountToUI();
}

/* .text+0x354880, 312 bytes. */
void CSKMIDIInMsgHandler::CheckDyingDamper()
{
	if (((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->GetVoiceMode() != 0)
		return;

	for (int i = 0; i < 16; i++) {
		if (m_dyingDamperTicks[i] > 0)
			m_dyingDamperFlag[i] = 1;
	}
}

/* ==================== CSKMIDIPortMsgHandler ==================== */

/* .text+0x355d10, 47 bytes. */
CSKMIDIPortMsgHandler::CSKMIDIPortMsgHandler() : CSKMIDIInMsgHandler()
{
	m_flags &= 0xf0;
	m_sysExHandler->m_flags &= 0xf0;
}

/* .text+0x355a30, 3 bytes. */
bool CSKMIDIPortMsgHandler::ShouldSendChannelMessageToMIDIPort()
{
	return false;
}

/* .text+0x355a40, 1 byte -- empty. */
void CSKMIDIPortMsgHandler::SendChannelMessageToMIDIPort()
{
}

/* .text+0x355a50, 1 byte -- empty. */
void CSKMIDIPortMsgHandler::ConvertPreMIDINote()
{
}

/* .text+0x355a60, 1 byte -- empty; own vtable slot inherited from
 * CSKMIDIMsgHandler, not CSKMIDIInMsgHandler's own extension. */
void CSKMIDIPortMsgHandler::ConvertPreMIDIAfterTouch()
{
}

/* .text+0x355a70, 93 bytes. */
bool CSKMIDIPortMsgHandler::CheckGlobalParameterPreSendToSTG()
{
	unsigned char *bm = CKGBankManager::ms_poInstance;
	unsigned char statusType = m_status & 0xf0;

	if (statusType == 0xc0)
		return bm[0x97c748] & 1;
	if (statusType == 0xe0)
		return (bm[0x97c748] >> 4) & 1;
	if (statusType != 0xb0)
		return true;
	if (m_data1 != 0 && m_data1 != 0x20)
		return (bm[0x97c748] >> 4) & 1;
	return (bm[0x97c748] >> 1) & 1;
}

/* .text+0x355ae0, 37 bytes. */
bool CSKMIDIPortMsgHandler::ShouldNotifyToKarmaController()
{
	if ((m_status & 0xf0) != 0xb0)
		return false;
	return (CKGBankManager::ms_poInstance[0x97c748] >> 4) & 1;
}

/* .text+0x355b10, 178 bytes -- uses regparm 3 calling convention. */
bool CSKMIDIPortMsgHandler::AnalizeAndSetParameter(unsigned char *buf, int len)
{
	(void)len;
	m_status = buf[0];
	m_data1  = buf[1];
	m_data2  = buf[2];
	m_flags  = buf[3] & 0xf0;

	unsigned char status = buf[0];
	if (status > 0xef)
		return false;

	unsigned char statusType = status & 0xf0;
	if (statusType == 0x80 || statusType == 0x90 || statusType == 0xa0 ||
	    statusType == 0xb0 || statusType == 0xe0) {
		if ((signed char)buf[1] < 0 || (signed char)buf[2] < 0)
			return false;
		m_status = status;
		m_data1 = buf[1];
		m_data2 = buf[2];
		return true;
	}
	if (statusType == 0xc0 || statusType == 0xd0) {
		if ((signed char)buf[1] < 0)
			return false;
		m_status = status;
		m_data1 = buf[1];
		return true;
	}
	return false;
}

/* .text+0x355bd0, 42 bytes. */
void CSKMIDIPortMsgHandler::NotifyNoteCountToUI()
{
	unsigned char *sub = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
	sub[0x147a2] = (m_noteOnCount != 0);
}

/* .text+0x355c20, 223 bytes. */
bool CSKMIDIPortMsgHandler::CheckGlobalParameterPreSendToKarmaEngine()
{
	unsigned char statusType = m_status & 0xf0;
	unsigned char *bm = CKGBankManager::ms_poInstance;

	if (statusType == 0xd0) {
		if ((bm[0x97c748] & 0x8) == 0)
			return false;
	} else if (statusType == 0xe0 || statusType == 0xb0) {
		if ((bm[0x97c748] & 0x10) == 0)
			return false;
	}

	if (bm[0x97c7c0] != 0)
		return true;

	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	int channel = m_status & 0xf;
	int n = mf->GetNumOfKARMAModule();
	for (int i = 0; i < n; i++) {
		if (mf->GetKARMARealInputChannel(i) == channel)
			return false;
		if (mf->GetRealInputLocalControllerChannel(i) == channel)
			return false;
	}
	return true;
}

/* ==================== CSKPadNoteByMIDIPortMsgHandler ==================== */

/* .text+0x355c00, 3 bytes. */
bool CSKPadNoteByMIDIPortMsgHandler::ShouldNotifyToKarmaController()
{
	return false;
}

/* .text+0x355c10, 3 bytes. */
bool CSKPadNoteByMIDIPortMsgHandler::CheckNoteMessageAndTriggerPad()
{
	return false;
}

/* ==================== CSKMIDILocalCtrlMsgHandler ==================== */

/* .text+0x3458d0, 245 bytes. */
CSKMIDILocalCtrlMsgHandler::CSKMIDILocalCtrlMsgHandler() : CSKMIDIInMsgHandler()
{
	m_flags = (m_flags & 0xf0) | 1;
	m_sysExHandler->m_flags = (m_sysExHandler->m_flags & 0xf0) | 1;

	for (int note = 0; note < 128; note++)
		for (int timbre = 0; timbre < 16; timbre++)
			m_perNoteTimbreTranspose[note][timbre] = 0;
}

/* .text+0x3449c0, 1090 bytes -- iterates the 16-entry
 * m_dyingDamperFlag[] array set by CheckDyingDamper; for each flagged
 * channel, synthesizes+sends a Sustain-CC, same CC#/value as the
 * triggering message with the channel swapped, then updates
 * m_dyingDamperTicks[i] from the response and maybe clears the flag. */
void CSKMIDILocalCtrlMsgHandler::SendDyingDamperMessageToMIDIPort()
{
	if ((m_status & 0xf0) != 0xb0)
		return;
	if (m_data1 != 0x40)
		return;

	unsigned char origStatus = m_status;
	for (int i = 0; i < 16; i++) {
		if (m_dyingDamperFlag[i]) {
			m_status = 0xb0 | i;
			SendChannelMessageToMIDIPortWithCorrectLength();
			signed char data2 = (signed char)m_data2;
			m_dyingDamperTicks[i] = data2;
			if (data2 == 0)
				m_dyingDamperFlag[i] = 0;
		}
	}
	m_status = origStatus;
}

/* .text+0x344e40, 13 bytes. */
bool CSKMIDILocalCtrlMsgHandler::CheckGlobalParameterPreSendToKarmaEngine()
{
	return CKGBankManager::ms_poInstance[0x97c749] != 0;
}

/* .text+0x344e50, 13 bytes. */
bool CSKMIDILocalCtrlMsgHandler::CheckGlobalParameterPreSendToSTG()
{
	return CKGBankManager::ms_poInstance[0x97c749] != 0;
}

/* .text+0x344e60, 69 bytes. */
void CSKMIDILocalCtrlMsgHandler::ConvertPreMIDINote()
{
	unsigned char *bm = CKGBankManager::ms_poInstance;
	if (*(int *)(bm + 0x97c74c) != 0)
		return;
	unsigned char statusType = m_status & 0xf0;
	if (statusType != 0x90 && statusType != 0x80)
		return;

	int note = (signed char)m_data1 + (signed char)bm[0x97c744];
	if (note > 0x7f) {
		note -= 12;
	} else if (note < 0) {
		note += 12;
	}
	m_data1 = (unsigned char)note;
}

/* .text+0x344ec0, 13 bytes. */
bool CSKMIDILocalCtrlMsgHandler::ShouldNotifyToKarmaController()
{
	return CKGBankManager::ms_poInstance[0x97c749] != 0;
}

/* .text+0x344ed0, 28 bytes. */
void CSKMIDILocalCtrlMsgHandler::InitializeExtNoteOnChecker()
{
	for (int i = 0; i < 128; i++)
		m_extNoteOnChecker[i] = 0;
}

/* .text+0x344ef0, 14 bytes. */
void CSKMIDILocalCtrlMsgHandler::RegistExtNoteOn(int note)
{
	if ((unsigned int)note <= 0x7f)
		m_extNoteOnChecker[note]++;
}

/* .text+0x344f00, 28 bytes. */
void CSKMIDILocalCtrlMsgHandler::UnRegistExtNoteOn(int note)
{
	if ((unsigned int)note <= 0x7f && m_extNoteOnChecker[note] != 0)
		m_extNoteOnChecker[note]--;
}

/* .text+0x344f20, 21 bytes. */
bool CSKMIDILocalCtrlMsgHandler::IsSendingNoteOnToExt(int note)
{
	return (unsigned int)note <= 0x7f && m_extNoteOnChecker[note] != 0;
}

/* .text+0x344f40, 51 bytes. */
void CSKMIDILocalCtrlMsgHandler::CopyNoteOnStatus(unsigned char *dst)
{
	for (int i = 0; i < 128; i++)
		dst[i] = m_noteDownCount[i];
}

/* .text+0x344f80, 31 bytes. */
bool CSKMIDILocalCtrlMsgHandler::IsKeyboardAllOff()
{
	for (int i = 0; i < 128; i++)
		if (m_noteDownCount[i] != 0)
			return false;
	return true;
}

/* .text+0x344fb0, 94 bytes. */
void CSKMIDILocalCtrlMsgHandler::ClearNoteStatus()
{
	for (int i = 0; i < 128; i++)
		m_noteDownCount[i] = 0;
	m_noteOnCount = 0;
	m_bDamperOn = false;
	m_bSostenutoOn = false;
	NotifyNoteCountToUI();
	NotifyDamperStatusToUI();
	NotifySostenutoStatusToUI();
}

/* .text+0x345010, 107 bytes -- uses regparm 3 calling convention. */
bool CSKMIDILocalCtrlMsgHandler::AnalizeAndSetParameter(unsigned char *buf, int len)
{
	(void)len;
	m_status = buf[0];
	m_data1  = buf[1];
	m_data2  = buf[2];
	m_flags  = buf[3];

	if ((signed char)m_status >= 0)
		return false;
	if ((signed char)m_data1 < 0 || (signed char)m_data2 < 0 || (signed char)m_flags < 0)
		return false;

	return (m_flags & 0xf) != 5;
}

/* .text+0x345080, 42 bytes. */
void CSKMIDILocalCtrlMsgHandler::NotifyNoteCountToUI()
{
	unsigned char *sub = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
	sub[0x147a1] = (m_noteOnCount != 0);
}

/* .text+0x3450b0, 23 bytes. */
void CSKMIDILocalCtrlMsgHandler::NotifyDamperStatusToUI()
{
	unsigned char *sub = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
	sub[0x147a3] = m_bDamperOn;
}

/* .text+0x3450d0, 23 bytes. */
void CSKMIDILocalCtrlMsgHandler::NotifySostenutoStatusToUI()
{
	unsigned char *sub = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
	sub[0x147a4] = m_bSostenutoOn;
}

/* .text+0x3450f0, 84 bytes -- ChannelAftertouch, status 0xd0,
 * dup-suppression REUSES m_extNoteOnChecker[], indexed by CHANNEL
 * (0-15 this time, not by note number) -- a real, confirmed dual use of
 * the same storage, see the header comment, not a transcription error. */
bool CSKMIDILocalCtrlMsgHandler::CheckDuplicateMessage()
{
	unsigned char statusType = m_status & 0xf0;
	int channel = m_status & 0xf;

	if (statusType == 0xd0) {
		bool dup = (m_extNoteOnChecker[channel] == (signed char)m_data1);
		m_extNoteOnChecker[channel] = m_data1;
		return !dup;
	}

	m_extNoteOnChecker[channel] = m_data1;
	return true;
}

/* .text+0x345170, 61 bytes. */
void CSKMIDILocalCtrlMsgHandler::SendChannelMessageToMIDIPort()
{
	SendChannelMessageToSTGWithCorrectLength();
	if (((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->GetVoiceMode() == 0)
		SendChannelMessageInCombiOtherTimbreToMIDIPort();
	SendChannelMessageInCombiOtherTimbreToMIDIPort();
}

/* .text+0x345380, 181 bytes -- the `mov edi,[eax+0xd8]` in ground truth
 * reads this object's OWN vtable at vptr+0xd8, i.e. rodata_offset
 * 0xd8+8=0xe0, which is IsNotThruKarma's own slot, then calls it via
 * the raw function-pointer value with `this`/`channel` in EAX/EDX --
 * i.e. a perfectly ordinary IsNotThruKarma virtual call through
 * `channel`, just compiled through an explicit pointer load instead of
 * the usual `call [edx+N]` shape, same +8 rule, applied consistently. */
bool CSKMIDILocalCtrlMsgHandler::ShouldSendChannelMessageToMIDIPort()
{
	unsigned char *bm = CKGBankManager::ms_poInstance;
	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;

	if (bm[0x97c749] != 0 && mf->IsKARMAOn() && mf->GetVoiceMode() != 0) {
		if (mf->GetVoiceMode() != 2)
			return false;
		int channel = mf->GetLocalControlChannel();
		if (!IsNotThruKarma(channel))
			return false;
		/* falls through to the common tail below */
	}

	if (mf->GetVoiceMode() == 2) {
		int status = mf->GetCurrentTrackStatus();
		return status <= 1;
	}
	return CheckGlobalParameterPreSendToMIDIPort();
}

/* .text+0x345440, 145 bytes. */
bool CSKMIDILocalCtrlMsgHandler::CheckGlobalParameterPreSendToMIDIPort()
{
	unsigned char statusType = m_status & 0xf0;
	unsigned char *bm = CKGBankManager::ms_poInstance;

	if (statusType == 0xd0)
		return (bm[0x97c748] >> 3) & 1;

	if (statusType == 0xe0) {
		if ((bm[0x97c748] & 0x10) == 0)
			return false;
		if ((m_flags & 0xf) != 4)
			return true;
		return CKGBankManager::ms_poInstance[0x97c7b9] != 0;
	}

	if (statusType != 0xb0)
		return true;

	if ((bm[0x97c748] & 0x10) == 0)
		return true;
	return SKSTGGate_CheckVJSCCToMIDIPortFilter((signed char)m_data1, m_flags & 0xf);
}

/* .text+0x345670, 262 bytes. */
bool CSKMIDILocalCtrlMsgHandler::CheckTimbreParameterPreSendToMIDIPort(int timbre)
{
	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	unsigned char statusType = m_status & 0xf0;

	if (statusType == 0xb0)
		return mf->IsEnableTimbreCC((signed char)m_data1, timbre);

	if (statusType == 0x80 || statusType == 0x90) {
		int note = (signed char)m_data1;
		if (note < mf->GetTimbreBottomKey(timbre))
			return false;
		if (note > mf->GetTimbreTopKey(timbre))
			return false;
		if (statusType == 0x80)
			return false;
		int velocity = (signed char)m_data2;
		if (velocity < mf->GetTimbreLowVelocity(timbre))
			return false;
		return velocity <= mf->GetTimbreHighVelocity(timbre);
	}

	if (statusType == 0xe0)
		return mf->IsEnableTimbrePitchBend(timbre);
	if (statusType == 0xd0)
		return mf->IsEnableTimbreAftertouch(timbre);

	return false;
}

/* .text+0x345790, 146 bytes. */
unsigned int CSKMIDILocalCtrlMsgHandler::GetKarmaControlledChannelPat(bool includeAllModules)
{
	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	unsigned int pat = 0;
	int n = mf->GetNumOfKARMAModule();

	for (int i = 0; i < n; i++) {
		if (includeAllModules) {
			pat |= (1u << mf->GetKARMARealOutputChannel(i));
		} else if (mf->IsKARMATimbreThru(i)) {
			pat |= (1u << mf->GetKARMARealOutputChannel(i));
		}
	}
	return pat;
}

/* .text+0x345830, 156 bytes. */
bool CSKMIDILocalCtrlMsgHandler::IsNotThruKarma(int channel)
{
	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	int n = mf->GetNumOfKARMAModule();

	for (int i = 0; i < n; i++) {
		int in  = mf->GetKARMARealInputChannel(i);
		int out = mf->GetKARMARealOutputChannel(i);
		int loc = mf->GetRealInputLocalControllerChannel(i);

		if (((in == channel) || (loc == channel)) && out == channel)
			return false;
	}
	return true;
}

/*
 * .text+0x3451b0, 444 bytes, and .text+0x3454e0, 371 bytes -- LOWER
 * CONFIDENCE than every other method in this file, see the class's own
 * header comment. Real control flow, real call targets, and the real
 * `m_perNoteTimbreTranspose[note][timbre]` addressing math, confirmed
 * independently via the ctor's own 128-row zero-init loop, are all
 * traced from raw disassembly; per-branch fidelity has not been
 * independently re-verified the way the rest of this batch was. The
 * 2-arg overload's real shape: gate on GetTimbreStatus in {3,4} and a
 * GetLocalControlChannel mismatch against channel, then
 * CheckTimbreParameterPreSendToMIDIPort; on pass, synthesize a
 * per-timbre-channel copy of the current event, remapping the note
 * through a per-note-per-timbre transpose value that is COMPUTED --
 * via CMIDIFlowParamHolder's own GetTimbreTranspose -- and REMEMBERED
 * on Note On, then RECALLED, not recomputed, on the matching Note Off
 * -- so a note stays correctly paired even if the live timbre
 * transpose setting changes while the note is held.
 */
void CSKMIDILocalCtrlMsgHandler::SendChannelMessageInCombiOtherTimbreToMIDIPort()
{
	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	bool karmaOn = mf->IsKARMAOn();
	bool statusIsNoteOn = (m_status & 0xf0) == 0x90;

	for (int timbre = 0; timbre < 16; timbre++) {
		int channel = mf->GetTimbreChannel(timbre);

		if (channel <= 0xf) {
			if (statusIsNoteOn && !mf->IsEnableTimbreNoteOn(timbre))
				continue;
			if (!karmaOn) {
				SendChannelMessageInCombiOtherTimbreToMIDIPort(timbre, false);
				continue;
			}
		} else {
			if (statusIsNoteOn && !mf->IsEnableTimbreNoteOn(timbre))
				continue;
		}

		unsigned char statusType = m_status & 0xf0;
		if (statusType == 0x80) {
			continue;
		}
		if (statusType == 0xb0 && m_data1 == 0x40)
			SendChannelMessageInCombiOtherTimbreToMIDIPort(timbre, true);
	}
}

void CSKMIDILocalCtrlMsgHandler::SendChannelMessageInCombiOtherTimbreToMIDIPort(int timbre, bool applySustainFilter)
{
	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	int channel = mf->GetTimbreChannel(timbre);
	if (channel > 0xf)
		channel = mf->GetLocalControlChannel();

	int timbreStatus = mf->GetTimbreStatus(timbre) - 3;
	unsigned char savedStatus = m_status;
	if ((unsigned int)timbreStatus > 1)
		return;

	if (mf->GetLocalControlChannel() == channel)
		return;
	if (!CheckTimbreParameterPreSendToMIDIPort(timbre))
		return;

	unsigned char savedData1 = m_data1;
	unsigned char statusType = savedStatus & 0xf0;
	bool wasNoteOn = (statusType == 0x90);
	m_status = (unsigned char)(channel | statusType);

	int note = (signed char)savedData1;
	int transposedNote;
	if (wasNoteOn) {
		int transpose = mf->GetTimbreTranspose(timbre);
		m_perNoteTimbreTranspose[note & 0x7f][timbre] = transpose;
		transposedNote = note + transpose;
	} else if (statusType == 0x80) {
		transposedNote = note + m_perNoteTimbreTranspose[note & 0x7f][timbre];
	} else {
		transposedNote = -1;	/* not used below (neither NoteOn nor NoteOff) */
	}

	if ((statusType == 0x80 || wasNoteOn) && (unsigned int)transposedNote <= 0x7f) {
		m_data1 = (unsigned char)transposedNote;
	} else {
		m_data1 = savedData1;
	}

	if (!applySustainFilter) {
		SendChannelMessageToSTGWithCorrectLength();
	} else if (statusType == 0xb0 && note == 0x40) {
		if (m_dyingDamperFlag[channel] && !wasNoteOn) {
			m_dyingDamperFlag[channel] = 0;
		}
	}

	m_data1 = savedData1;
	m_status = savedStatus;
}

/* ==================== CSKMIDIKarmaCtrlMsgHandler ==================== */

/* .text+0x345a00, 33 bytes. */
CSKMIDIKarmaCtrlMsgHandler::CSKMIDIKarmaCtrlMsgHandler() : CSKMIDILocalCtrlMsgHandler()
{
}

/* .text+0x3459d0, 3 bytes. */
bool CSKMIDIKarmaCtrlMsgHandler::ShouldNotifyToKarmaController()
{
	return false;
}

/* .text+0x3459e0, 24 bytes. */
bool CSKMIDIKarmaCtrlMsgHandler::CheckNoteMessageAndTriggerPad()
{
	unsigned char statusType = m_status & 0xf0;
	return statusType == 0x90 || statusType == 0x80;
}

/* ==================== CSKPadNoteByLocalCtrlMsgHandler ==================== */

/* .text+0x345150, 3 bytes. */
bool CSKPadNoteByLocalCtrlMsgHandler::ShouldNotifyToKarmaController()
{
	return false;
}

/* .text+0x345160, 3 bytes. */
bool CSKPadNoteByLocalCtrlMsgHandler::CheckNoteMessageAndTriggerPad()
{
	return false;
}
