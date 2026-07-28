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

/* ==================== CSKParameterChangeMessage ====================
 * Found while sweeping the CKG/CSK cluster for a next tractable slice
 * after CSKMIDIInMsgHandler's own family (2026-07-28 continuation). See
 * this class's own header comment (oa_ckg_midi_msg_handler.h) for the
 * full 14-byte SysEx-shaped layout derivation.
 */

/* .text+0x341760, 142 bytes, regparm(3). */
void CSKParameterChangeMessage::SetParameters(unsigned char param1, unsigned char param2, unsigned char param3,
					       unsigned char param4, unsigned char param5, int value)
{
	m_bytes[0x2] = (unsigned char)(SPROutGate_GetGlobalChannel() | 0x30);
	m_bytes[0x4] = param1;
	m_bytes[0x5] = param2;
	m_bytes[0x6] = param3;
	m_bytes[0x8] = param4;
	m_bytes[0x9] = param5;
	m_bytes[0xa] = (unsigned char)((value >> 14) & 0x7f);
	m_bytes[0xb] = (unsigned char)((value >> 7) & 0x7f);
	m_bytes[0xc] = (unsigned char)(value & 0x7f);
}

/* .text+0x3417f0, 126 bytes, regparm(3). Same shape as the 5-byte
 * overload above, but the extra explicit param (param4) lands at +0x7
 * (left as a fixed 0 by the other overload). */
void CSKParameterChangeMessage::SetParameters(int param1, int param2, int param3, int param4, int param5,
					       int param6, long value)
{
	m_bytes[0x2] = (unsigned char)(SPROutGate_GetGlobalChannel() | 0x30);
	m_bytes[0x4] = (unsigned char)param1;
	m_bytes[0x5] = (unsigned char)param2;
	m_bytes[0x6] = (unsigned char)param3;
	m_bytes[0x7] = (unsigned char)param4;
	m_bytes[0x8] = (unsigned char)param5;
	m_bytes[0x9] = (unsigned char)param6;
	m_bytes[0xa] = (unsigned char)((value >> 14) & 0x7f);
	m_bytes[0xb] = (unsigned char)((value >> 7) & 0x7f);
	m_bytes[0xc] = (unsigned char)(value & 0x7f);
}

/* .text+0x341890, 81 bytes. Real body: false unless +0x1==0x42 AND
 * +0x2 == (sign-extended CKGBankManager::ms_poInstance[0x97c747] | 0x30)
 * AND +0x3==0x68 AND the +0x4 kind byte is 'm'/'C'/'n'/'A'. The
 * sign-extend-then-compare-to-a-zero-extended-byte shape means this can
 * only ever match when the global-channel byte is non-negative -- a
 * real, confirmed quirk, transcribed as observed rather than simplified
 * away. */
bool CSKParameterChangeMessage::IsThisParamChage()
{
	signed char globalChanByte = *(signed char *)(CKGBankManager::ms_poInstance + 0x97c747);
	int expected = (int)globalChanByte | 0x30;

	if (m_bytes[0x1] != 0x42)
		return false;
	if ((int)(unsigned int)m_bytes[0x2] != expected)
		return false;
	if (m_bytes[0x3] != 0x68)
		return false;

	unsigned char kind = m_bytes[0x4];
	return kind == 'm' || kind == 'C' || kind == 'n' || kind == 'A';
}

/* .text+0x3418f0, 47 bytes. Real body: reassembles the 3-byte value
 * split into a single int, sign-extending the upper bits based on bit 6
 * of the MSB byte (the 21-bit combined value's own sign bit). */
unsigned int CSKParameterChangeMessage::GetValue()
{
	unsigned char msb = m_bytes[0xa];
	unsigned char mid = m_bytes[0xb];
	unsigned char lsb = m_bytes[0xc];
	unsigned int value = ((unsigned int)msb << 14) | ((unsigned int)mid << 7) | lsb;

	if (msb & 0x40)
		value |= 0xffe00000u;
	return value;
}

/* .text+0x341920, 100 bytes, regparm(3). Real body: builds a whole new
 * message directly from a CSeqEvent's own raw bytes -- fixed SOX/EOX
 * like the ctor, everything else copied verbatim from seqEvent+9..+15
 * and seqEvent+0x11..+0x15 (CSeqEvent's own field names/layout beyond
 * these fixed byte offsets are out of scope). */
void CSKParameterChangeMessage::SetValue(CSeqEvent *seqEvent)
{
	const unsigned char *src = reinterpret_cast<const unsigned char *>(seqEvent);

	m_bytes[0x0] = 0xf0;
	m_bytes[0x1] = src[0x9];
	m_bytes[0x2] = src[0xa];
	m_bytes[0x3] = src[0xb];
	m_bytes[0x4] = src[0xc];
	m_bytes[0x5] = src[0xd];
	m_bytes[0x6] = src[0xe];
	m_bytes[0x7] = src[0xf];
	m_bytes[0x8] = src[0x11];
	m_bytes[0x9] = src[0x12];
	m_bytes[0xa] = src[0x13];
	m_bytes[0xb] = src[0x14];
	m_bytes[0xc] = src[0x15];
	m_bytes[0xd] = 0xf7;
}

/* ==================== CSKMIDIMsgProcessor ====================
 * See oa_ckg_midi_msg_handler.h's own class comment for the full field
 * layout derivation (ctor's own AllocAligned()+placement-ctor sequence,
 * cross-checked via objdump -r) and the vtable call_off -> real slot
 * name table this whole file's dispatch bodies below were derived from.
 */

/* .text+0x340e70, 217 bytes. */
CSKMIDIMsgProcessor::CSKMIDIMsgProcessor()
	: m_lastMsgKind(0), m_activeRawEvent(0), m_lastMsgSentinel(0)
{
	ms_poInstance = (unsigned char *)this;

	m_port = new (CSTGBankMemory::AllocAligned(0x142c, 0x10)) CSKMIDIPortMsgHandler();
	m_localCtrl = new (CSTGBankMemory::AllocAligned(0x34ac, 0x10)) CSKMIDILocalCtrlMsgHandler();
	m_special = new (CSTGBankMemory::AllocAligned(0xc, 0x10)) CSKSpecialMsgHandler();
	m_karmaCtrl = new (CSTGBankMemory::AllocAligned(0x34ac, 0x10)) CSKMIDIKarmaCtrlMsgHandler();
	/* Real ground truth constructs these 2 via their own PARENT ctor
	 * (same-sized allocation) then overwrites the vptr slot in place to
	 * "reclassify" the object -- placement-new through the derived type
	 * directly produces the identical end state. */
	m_padByPort = new (CSTGBankMemory::AllocAligned(0x142c, 0x10)) CSKPadNoteByMIDIPortMsgHandler();
	m_padByLocal = new (CSTGBankMemory::AllocAligned(0x34ac, 0x10)) CSKPadNoteByLocalCtrlMsgHandler();

	/* CMIDIFlowParamHolder is a pure opaque cast-through-`ms_poThis`
	 * singleton everywhere else it's used in this project (every one of
	 * its own methods has its body out of scope) -- just allocate
	 * storage and point the singleton at it, matching that convention,
	 * rather than modeling a ctor body this batch has no other evidence
	 * for. */
	CMIDIFlowParamHolder::ms_poThis = (unsigned char *)operator new(0x10);
	SKSTGGate_ResistReadQueus();
}

/* .text+0x340f50, 117 bytes. */
CSKMIDIMsgProcessor::~CSKMIDIMsgProcessor()
{
	if (CMIDIFlowParamHolder::ms_poThis) {
		operator delete(CMIDIFlowParamHolder::ms_poThis);
		CMIDIFlowParamHolder::ms_poThis = 0;
	}

	delete m_padByLocal;
	delete m_padByPort;
	delete m_karmaCtrl;
	delete m_localCtrl;
	delete m_port;
	/* m_special (+0x8) is deliberately never deleted -- see class
	 * comment in the header. */

	ms_poInstance = 0;
}

/* .text+0x340fd0, 209 bytes. Real body: dequeue loop over
 * SKSTGGate_ReceiveFromLocalControlQueus(). Each received message is
 * first offered to m_special->AnalizeAndProcess() (which, as a side
 * effect, copies the 4 raw bytes into m_special's own m_status/m_data1/
 * m_data2/m_flags fields regardless of whether it recognizes the
 * status); if m_special reports "handled" (a real ProgramChange/
 * PitchBend/ResetAllControllers status), the message is fully consumed
 * and the loop just fetches the next one. Otherwise the channel nibble
 * is re-read from m_special's own (just-populated) m_flags byte, and
 * dispatch continues via CKGEngine::ms_poInstance[+0x14]'s own "mode"
 * word: mode==2 routes to AnalizeAndProcessNoteOffWhilePerformanceChange()
 * (a "we are mid-performance-change, treat this specially" path);
 * anything else is the normal AnalizeAndProcess() dispatch, additionally
 * forwarded to KGMain_ReceiveControllerMessage()/
 * SPRMain_ReceiveControllerMessage(). A received length of exactly 5
 * bytes additionally stashes the message's own 5th byte into
 * m_lastMsgSentinel before the same mode check -- transcribed as
 * observed, exact semantic meaning of that 5-byte case not independently
 * confirmed beyond the literal byte offset. */
void CSKMIDIMsgProcessor::ReadMIDILocalControlQueue()
{
	unsigned char buf[0x20];
	int gotLen;

	while (SKSTGGate_ReceiveFromLocalControlQueus(buf, 0x20, &gotLen)) {
		SKMain_CheckAndProcessPreemption();

		if (m_special->AnalizeAndProcess(buf))
			continue;

		int channel = m_special->m_flags & 0xf;
		m_lastMsgKind = channel;
		if (gotLen == 5)
			m_lastMsgSentinel = buf[4];

		bool inPerfChangeMode = (*(int *)(CKGEngine::ms_poInstance + 0x14) == 2);

		m_activeRawEvent = m_localCtrl ? (unsigned char *)m_localCtrl + 4 : 0;
		if (inPerfChangeMode) {
			m_localCtrl->AnalizeAndProcessNoteOffWhilePerformanceChange(buf, gotLen);
		} else {
			m_localCtrl->AnalizeAndProcess(buf, gotLen);
		}
		m_activeRawEvent = 0;

		if (!inPerfChangeMode) {
			KGMain_ReceiveControllerMessage(buf);
			SPRMain_ReceiveControllerMessage(buf);
		}
	}
}

/* .text+0x3410c0, 184 bytes. Real body: preemption check, then (unless
 * QueuesFilteredDuringPerfChange() is true) a pad-note drain loop that
 * only runs while CKGEngine::ms_poInstance[+0x14]'s own mode word is 4,
 * followed unconditionally by the same MIDI-port dequeue loop
 * ReadMIDIPortQueue() implements on its own (this class's own ground
 * truth duplicates that loop's body inline here rather than calling
 * ReadMIDIPortQueue() -- reproduced the same way, not refactored). */
void CSKMIDIMsgProcessor::Process()
{
	SKMain_CheckAndProcessPreemption();

	if (!QueuesFilteredDuringPerfChange()) {
		if (*(int *)(CKGEngine::ms_poInstance + 0x14) == 4) {
			unsigned char padBuf[0x20];
			while (SKSTGGate_ReceivePads(padBuf)) {
				do {
					SKMain_CheckAndProcessPreemption();
					m_lastMsgKind = 1;
					m_lastMsgSentinel = -2;
					KGMain_ReceiveControllerMessage(padBuf);
				} while (SKSTGGate_ReceivePads(padBuf));
			}
		}
	}

	unsigned char buf[0x20];
	int gotLen;
	while (SKSTGGate_ReceiveFromMIDIPortQueus(buf, 0x20, &gotLen)) {
		m_lastMsgKind = 0;
		m_activeRawEvent = m_port ? (unsigned char *)m_port + 4 : 0;
		m_port->AnalizeAndProcess(buf, gotLen);
		m_activeRawEvent = 0;
	}
}

/* .text+0x341180, 63 bytes. */
void CSKMIDIMsgProcessor::ReadMIDIPads()
{
	unsigned char padBuf[0x20];

	while (SKSTGGate_ReceivePads(padBuf)) {
		SKMain_CheckAndProcessPreemption();
		m_lastMsgKind = 1;
		m_lastMsgSentinel = -2;
		KGMain_ReceiveControllerMessage(padBuf);
	}
}

/* .text+0x3411c0, 100 bytes. */
void CSKMIDIMsgProcessor::ReadMIDIPortQueue()
{
	unsigned char buf[0x20];
	int gotLen;

	while (SKSTGGate_ReceiveFromMIDIPortQueus(buf, 0x20, &gotLen)) {
		SKMain_CheckAndProcessPreemption();
		m_lastMsgKind = 0;
		m_activeRawEvent = m_port ? (unsigned char *)m_port + 4 : 0;
		m_port->AnalizeAndProcess(buf, gotLen);
		m_activeRawEvent = 0;
	}
}

/* .text+0x341230, 34 bytes, regparm(3). Real body: m_lastMsgKind=0, then
 * forwards straight through to m_port->AnalizeAndProcess(buf,len). */
void CSKMIDIMsgProcessor::ProcessMessageForDebug(unsigned char *buf, int len)
{
	m_lastMsgKind = 0;
	m_port->AnalizeAndProcess(buf, len);
}

/* .text+0x341260, 44 bytes. */
void CSKMIDIMsgProcessor::InitializeExtNoteOnChecker()
{
	m_localCtrl->InitializeExtNoteOnChecker();
	m_padByLocal->InitializeExtNoteOnChecker();
}

/* .text+0x341290, 233 bytes. Real body: snapshots m_localCtrl's own
 * note-on status via CopyNoteOnStatus() into a 128-byte local buffer,
 * then for every set note builds a synthetic Note-Off and dispatches it
 * -- ALWAYS through m_localCtrl (confirmed: both the first pass, keyed
 * off m_localCtrl's own snapshot, AND the second pass, keyed off
 * m_padByLocal's own snapshot, write the synthetic event into
 * m_localCtrl and call m_localCtrl->Process() -- a real, confirmed
 * asymmetry, not a copy-paste bug in this reconstruction). */
void CSKMIDIMsgProcessor::TrunAllNotesFromKeyboardOff()
{
	unsigned char buf[0x80];

	m_localCtrl->CopyNoteOnStatus(buf);
	for (int note = 0; note < 0x80; note++) {
		if (!buf[note])
			continue;
		/* TODO: verify -- GetLocalControllerChannel()'s return value
		 * minus 0x80, transcribed byte-exact from the observed
		 * `add eax,0xffffff80` / `mov [edx+4],al` sequence; the
		 * exact status-byte encoding this produces was not
		 * independently re-derived from a live channel capture. */
		int status = ((CKGEngine *)CKGEngine::ms_poInstance)->GetLocalControllerChannel() - 0x80;
		m_lastMsgKind = 1;
		m_lastMsgSentinel = -1;
		m_localCtrl->m_status = (unsigned char)status;
		m_localCtrl->m_data1 = (unsigned char)note;
		m_localCtrl->m_data2 = 0x40;
		m_localCtrl->m_flags = 0x01;
		m_localCtrl->Process();
	}

	m_padByLocal->CopyNoteOnStatus(buf);
	for (int note = 0; note < 0x80; note++) {
		if (!buf[note])
			continue;
		int status = ((CKGEngine *)CKGEngine::ms_poInstance)->GetLocalControllerChannel() - 0x80;
		m_lastMsgKind = 1;
		m_lastMsgSentinel = -1;
		m_localCtrl->m_status = (unsigned char)status;
		m_localCtrl->m_data1 = (unsigned char)note;
		m_localCtrl->m_data2 = 0x40;
		m_localCtrl->m_flags = 0x01;
		m_localCtrl->Process();
	}
}

/* .text+0x341380, 93 bytes. Real body: false unless m_localCtrl AND
 * m_padByLocal both report IsKeyboardAllOff(), m_localCtrl's own
 * IsDamperOn() is false, and returns !m_localCtrl->IsSostenutoOn(). */
bool CSKMIDIMsgProcessor::IsKeyboardAllOff()
{
	if (!m_localCtrl->IsKeyboardAllOff())
		return false;
	if (!m_padByLocal->IsKeyboardAllOff())
		return false;
	if (m_localCtrl->IsDamperOn())
		return false;
	return !m_localCtrl->IsSostenutoOn();
}

/* .text+0x3413f0, 21 bytes. */
void CSKMIDIMsgProcessor::LeaveDownloadMode()
{
	m_localCtrl->ClearNoteStatus();
}

/* .text+0x341410, 18 bytes. */
bool CSKMIDIMsgProcessor::IsDamperOn()
{
	return m_localCtrl->IsDamperOn();
}

/*
 * All 5 Process*ChannelMessage() overloads below share an identical
 * prologue in ground truth: `lea edx,[ecx+edx*1]` -- i.e. `status` and
 * `channel` are ADDED together (truncated to a byte) into the stored
 * m_status field, not `status` stored alone. This combines a bare status
 * nibble (e.g. 0x80/0x90/0xb0) with a 0-15 channel into the complete
 * MIDI status byte, confirmed identically at the start of all 5
 * functions' own raw disassembly.
 */

/* .text+0x341430, 89 bytes, regparm(3). */
void CSKMIDIMsgProcessor::ProcessLocalControlChannelMessage(int status, unsigned char channel, char data1, char data2)
{
	m_lastMsgKind = 1;
	m_lastMsgSentinel = -1;
	m_localCtrl->m_status = (unsigned char)(status + channel);
	m_localCtrl->m_data1 = (unsigned char)data1;
	m_localCtrl->m_data2 = (unsigned char)data2;
	m_localCtrl->m_flags = 0x01;
	m_localCtrl->Process();
}

/* .text+0x341490, 80 bytes, regparm(3). */
void CSKMIDIMsgProcessor::ProcessMIDIPortChannelMessage(int status, unsigned char channel, char data1, char data2)
{
	m_lastMsgKind = 0;
	m_port->m_status = (unsigned char)(status + channel);
	m_port->m_data1 = (unsigned char)data1;
	m_port->m_data2 = (unsigned char)data2;
	m_port->m_flags = 0x00;
	m_port->Process();
}

/* .text+0x3414e0, 89 bytes, regparm(3). */
void CSKMIDIMsgProcessor::ProcessKarmaControllerGeneratedChannelMessage(int status, unsigned char channel, char data1, char data2)
{
	m_lastMsgKind = 1;
	m_lastMsgSentinel = -2;
	m_karmaCtrl->m_status = (unsigned char)(status + channel);
	m_karmaCtrl->m_data1 = (unsigned char)data1;
	m_karmaCtrl->m_data2 = (unsigned char)data2;
	m_karmaCtrl->m_flags = 0x01;
	m_karmaCtrl->Process();
}

/* .text+0x341540, 89 bytes, regparm(3). */
void CSKMIDIMsgProcessor::ProcessPadNoteByLocalControlMessage(int status, unsigned char channel, char data1, char data2)
{
	m_lastMsgKind = 1;
	m_lastMsgSentinel = -2;
	m_padByLocal->m_status = (unsigned char)(status + channel);
	m_padByLocal->m_data1 = (unsigned char)data1;
	m_padByLocal->m_data2 = (unsigned char)data2;
	m_padByLocal->m_flags = 0x01;
	m_padByLocal->Process();
}

/* .text+0x3415a0, 82 bytes, regparm(3). */
void CSKMIDIMsgProcessor::ProcessPadNoteByMIDIPortMessage(int status, unsigned char channel, char data1, char data2)
{
	m_lastMsgKind = 0;
	m_padByPort->m_status = (unsigned char)(status + channel);
	m_padByPort->m_data1 = (unsigned char)data1;
	m_padByPort->m_data2 = (unsigned char)data2;
	m_padByPort->m_flags = 0x00;
	m_padByPort->Process();
}

/* .text+0x341600, 83 bytes. */
void CSKMIDIMsgProcessor::KillAllDyingNotes()
{
	m_localCtrl->CheckDyingDamper();

	m_localCtrl->m_status = 0x80;
	m_localCtrl->m_data1 = 0x00;
	m_localCtrl->m_data2 = 0x40;
	m_localCtrl->m_flags = 0x01;
	m_localCtrl->CSKMIDIInMsgHandler::KillAllDyingNotes();

	m_localCtrl->m_status = 0x80;
	m_localCtrl->m_data1 = 0x00;
	m_localCtrl->m_data2 = 0x40;
	m_localCtrl->m_flags = 0x00;
	m_port->CSKMIDIInMsgHandler::KillAllDyingNotes();
}

/* .text+0x341660, 51 bytes, regparm(3). Both owned non-virtual overloads
 * are called with the same msg pointer -- m_localCtrl's own, then
 * m_port's own. */
void CSKMIDIMsgProcessor::StoreDyingNoteInfoForSTG(CMIDIMessage *msg)
{
	m_localCtrl->CSKMIDIInMsgHandler::StoreDyingNoteInfoForSTG(msg);
	m_port->CSKMIDIInMsgHandler::StoreDyingNoteInfoForSTG(msg);
}

/* .text+0x3416a0, 51 bytes, regparm(3). */
void CSKMIDIMsgProcessor::StoreDyingNoteInfoForMIDPort(CMIDIMessage *msg)
{
	m_localCtrl->CSKMIDIInMsgHandler::StoreDyingNoteInfoForMIDPort(msg);
	m_port->CSKMIDIInMsgHandler::StoreDyingNoteInfoForMIDPort(msg);
}

/* .text+0x3416e0, 35 bytes, regparm(3). */
bool CSKMIDIMsgProcessor::GetNowProcessingNoteOffVelocity(int *out)
{
	unsigned char *ev = m_activeRawEvent;

	if (ev && (ev[0] & 0xf0) == 0x80)
		*out = (signed char)ev[2];
	return true;
}

/* .text+0x341710, 38 bytes. */
void CSKMIDIMsgProcessor::ResetNotesAfterStopSequencer()
{
	m_padByLocal->ClearKeyboardStatus();
	m_localCtrl->ClearKeyboardStatus();
}

/* ==================== CKGEventDisplayManager ====================
 * See oa_ckg_midi_msg_handler.h's own class comment for the full flat
 * dword-index layout derivation.
 */

/* .text+0x3b40b0, 3408 bytes. Real body: zero the whole [0,0xedc) byte
 * range except the 2 read-cursor dwords (seeded to 1), then zero a
 * 96-byte range on the FOREIGN CKGBankManager::ms_poInstance[+8]
 * sub-object (the note/CC "visibly on" bitmasks). */
void CKGEventDisplayManager::Initialize()
{
	__builtin_memset(m_flat, 0, sizeof(m_flat));
	m_flat[OA_KGEVTDISP_NOTE_RCURSOR] = 1;
	m_flat[OA_KGEVTDISP_CC_RCURSOR] = 1;

	unsigned char *sub = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
	__builtin_memset(sub + 0x723c, 0, 0x729c - 0x723c);
}

/* Shared helper -- real ground truth duplicates this across NoteOn()/
 * NoteOnByKarma()/NoteOn(int,int) verbatim (confirmed identical from this
 * step onward in all 3). Always increments the live count, then
 * unconditionally ORs the ring bit (no gating -- that's NoteOff's own
 * shape, see MarkNoteOff below). */
void CKGEventDisplayManager::MarkNoteOn(int objectIndex, int note)
{
	m_flat[objectIndex * 128 + note]++;

	if (objectIndex > 4 || (unsigned int)note > 0x7f)
		return;

	unsigned int *onMask = (unsigned int *)(*(unsigned char **)(CKGBankManager::ms_poInstance + 8) + 0x723c);
	onMask[objectIndex * 4 + (note >> 5)] |= 1u << (note & 0x1f);
}

/* Shared helper -- real ground truth duplicates this across NoteOff()/
 * NoteOffByKarma()/NoteOff(int,int) verbatim. Ring-gated: the FIRST
 * NoteOff for a given note within the current write-window just marks
 * the ring bit (the count is decremented later, once, when
 * CheckAndProcessNoteStatus() ages that ring slot out); any FURTHER
 * (redundant) NoteOff for the SAME note within the same window decrements
 * the live count directly instead. */
void CKGEventDisplayManager::MarkNoteOff(int objectIndex, int note)
{
	int V = m_flat[OA_KGEVTDISP_NOTE_WCURSOR];
	unsigned int *ring = (unsigned int *)&m_flat[OA_KGEVTDISP_NOTE_RING + V * 20 + objectIndex * 4];
	unsigned int bit = 1u << (note & 0x1f);

	if (!(ring[note >> 5] & bit)) {
		ring[note >> 5] |= bit;
		return;
	}

	int idx = objectIndex * 128 + note;
	if (m_flat[idx] > 0)
		m_flat[idx]--;
}

/* Shared helper -- real ground truth duplicates this across
 * CCOnByKarma()/CCOn()/BendOnByKarma() verbatim (confirmed byte-identical
 * from this step onward in all 3 -- only the caller-side divisor differs,
 * 8 for CC, 1024 for pitch bend). Always increments the live count; if
 * this exact (module,groupIndex) bit was ALREADY marked in the current
 * write-window's ring slot, immediately decrements back (net effect: the
 * count only grows once per NEW window, repeated marks within the same
 * window cancel out). */
void CKGEventDisplayManager::MarkCCOrBendOn(int module, int groupIndex)
{
	int idx = OA_KGEVTDISP_CC_DATA + module * 16 + groupIndex;
	m_flat[idx]++;

	if (groupIndex > 0xf || module > 3)
		return;

	int V = m_flat[OA_KGEVTDISP_CC_WCURSOR];
	unsigned int *ring = (unsigned int *)&m_flat[OA_KGEVTDISP_CC_RING + V * 4];
	unsigned int bit = 1u << groupIndex;

	if (!(ring[module] & bit)) {
		ring[module] |= bit;
		return;
	}

	if (m_flat[idx] > 0)
		m_flat[idx]--;
}

/* .text+0x3b4e00, 64 bytes. */
void CKGEventDisplayManager::NoteOn(int note)
{
	MarkNoteOn(0, note);
}

/* .text+0x3b4e40, 92 bytes. */
void CKGEventDisplayManager::NoteOnByKarma(int module, int note)
{
	MarkNoteOn(GetNoteObjectIndex(module), note);
}

/* .text+0x3b4ea0, 110 bytes. */
void CKGEventDisplayManager::NoteOff(int note)
{
	MarkNoteOff(0, note);
}

/* .text+0x3b4f20, 147 bytes. */
void CKGEventDisplayManager::NoteOffByKarma(int module, int note)
{
	MarkNoteOff(GetNoteObjectIndex(module), note);
}

/* .text+0x3b4fc0, 59 bytes. Direct-objectIndex overload -- no lookup
 * table, the caller already supplies the object index. */
void CKGEventDisplayManager::NoteOn(int objectIndex, int note)
{
	MarkNoteOn(objectIndex, note);
}

/* .text+0x3b5000, 120 bytes. */
void CKGEventDisplayManager::NoteOff(int objectIndex, int note)
{
	MarkNoteOff(objectIndex, note);
}

/* .text+0x3b5080, 148 bytes. groupIndex = cc/8 (real assembly's
 * negative-rounding fixup before the arithmetic shift is exactly C's own
 * truncating `/`, so plain integer division reproduces it exactly). */
void CKGEventDisplayManager::CCOnByKarma(int module, int cc)
{
	MarkCCOrBendOn(module, cc / 8);
}

/* .text+0x3b5120, 151 bytes. groupIndex = bendValue/1024 -- confirmed via
 * disasm to feed the exact SAME storage as CC events (own array, ring,
 * and sub-bitmask all shared with CCOnByKarma/CCOn). */
void CKGEventDisplayManager::BendOnByKarma(int module, int bendValue)
{
	MarkCCOrBendOn(module, bendValue / 1024);
}

/* .text+0x3b51c0, 148 bytes. Byte-identical body to CCOnByKarma() -- 2
 * distinct real functions (via-Karma vs local source), same underlying
 * storage. */
void CKGEventDisplayManager::CCOn(int module, int cc)
{
	MarkCCOrBendOn(module, cc / 8);
}

/* .text+0x3b5260, 347 bytes. Ages out ring slot V0 = m_flat[NOTE_RCURSOR]:
 * for every note bit still set in that ring slot (across all 5
 * objectIndex rows), decrements the matching live count, and once a
 * count reaches 0 also clears the UI-facing "note visibly on" bit on the
 * foreign sub-object. Both the note-ring READ cursor (+0xed0) and WRITE
 * cursor (+0xecc) advance by 1 (mod 10) once per call, independently. */
void CKGEventDisplayManager::CheckAndProcessNoteStatus()
{
	int V0 = m_flat[OA_KGEVTDISP_NOTE_RCURSOR];
	unsigned char *sub = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
	unsigned int *onMask = (unsigned int *)(sub + 0x723c);

	for (int block = 0; block < 5; block++) {
		unsigned int *ring = (unsigned int *)&m_flat[OA_KGEVTDISP_NOTE_RING + V0 * 20 + block * 4];

		for (int group = 0; group < 4; group++) {
			for (int bit = 0; bit < 32; bit++) {
				if (!(ring[group] & (1u << bit)))
					continue;

				int note = group * 32 + bit;
				int idx = block * 128 + note;

				if (m_flat[idx] > 0)
					m_flat[idx]--;
				if (m_flat[idx] == 0)
					onMask[block * 4 + group] &= ~(1u << bit);

				ring[group] &= ~(1u << bit);
			}
		}
	}

	m_flat[OA_KGEVTDISP_NOTE_RCURSOR] = (V0 + 1) % 10;
	m_flat[OA_KGEVTDISP_NOTE_WCURSOR] = (m_flat[OA_KGEVTDISP_NOTE_WCURSOR] + 1) % 10;
}

/* .text+0x3b53d0, 289 bytes. Same ring-aging shape as
 * CheckAndProcessNoteStatus() above, over the CC/bend ring instead (V0 =
 * m_flat[CC_RCURSOR], one dword per module holding 16 group bits). */
void CKGEventDisplayManager::CheckAndProcessCCStatus()
{
	int V0 = m_flat[OA_KGEVTDISP_CC_RCURSOR];
	unsigned int *ring = (unsigned int *)&m_flat[OA_KGEVTDISP_CC_RING + V0 * 4];

	for (int module = 0; module < 4; module++) {
		for (int cc = 0; cc < 16; cc++) {
			if (!(ring[module] & (1u << cc)))
				continue;

			int idx = OA_KGEVTDISP_CC_DATA + module * 16 + cc;

			if (m_flat[idx] > 0)
				m_flat[idx]--;
			if (m_flat[idx] == 0) {
				unsigned char *sub = *(unsigned char **)(CKGBankManager::ms_poInstance + 8);
				unsigned int *onMask = (unsigned int *)(sub + 0x728c);
				onMask[module] &= ~(1u << cc);
			}

			ring[module] &= ~(1u << cc);
		}
	}

	m_flat[OA_KGEVTDISP_CC_RCURSOR] = (V0 + 1) % 10;
	m_flat[OA_KGEVTDISP_CC_WCURSOR] = (m_flat[OA_KGEVTDISP_CC_WCURSOR] + 1) % 10;
}

/* .text+0x3b5500, 91 bytes. Spreads note/CC aging across calls using a
 * "now" tick counter (+0xec0, written elsewhere, out of scope) against 2
 * separate checkpoints -- calls CheckAndProcessNoteStatus()/
 * CheckAndProcessCCStatus() once per 0x14 (20) ticks of backlog. */
void CKGEventDisplayManager::Idle()
{
	int now = m_flat[OA_KGEVTDISP_TICK_NOW];

	int noteDiff = now - m_flat[OA_KGEVTDISP_NOTE_CHECKPOINT];
	while (noteDiff > 0x13) {
		noteDiff -= 0x14;
		CheckAndProcessNoteStatus();
	}
	m_flat[OA_KGEVTDISP_NOTE_CHECKPOINT] = now - noteDiff;

	int ccDiff = now - m_flat[OA_KGEVTDISP_CC_CHECKPOINT];
	while (ccDiff > 0x13) {
		ccDiff -= 0x14;
		CheckAndProcessCCStatus();
	}
	m_flat[OA_KGEVTDISP_CC_CHECKPOINT] = now - ccDiff;
}

/* ==================== CKGMIDIOutMsgHandler ==================== */

/* .text+0x3bb6a0, 117 bytes. Fully self-contained -- every call here
 * targets an already-declared sibling virtual (own or inherited from
 * CSKMIDIMsgHandler); real semantics recovered directly from the
 * disassembly's own call/branch structure, not guessed from naming. */
void CKGMIDIOutMsgHandler::Process()
{
	if ((unsigned char)m_status > 0xef)
		return;

	if (CheckDyingNoteForMIDIPort()) {
		if (ShouldSendChannelMessageToMIDIPort())
			SendChannelMessageToMIDIPort();
	}
	if (ShouldRecChannelMessageToSequencer())
		RecChannelMessageToSequencer();
	if (!ShouldSendChannelMessageToSTG())
		return;

	ConvertPostMIDIAfterTouch();
	ConvertPostMIDIVelocity();
	SendChannelMessageToSTG();
}

/* .text+0x3bb7a0, regparm(3). Saves/restores m_data1/m_data2 around the
 * dispatch; recomputes m_data2 via the base class's own
 * CheckAndGetCorrectCCValue(). When CKGBankManager::ms_poInstance[+0x97c74c]
 * (an int flag) == 1 AND the message is NoteOn/NoteOff, applies an
 * octave-wrap transpose (+/-0xc) to m_data1 sourced from
 * CKGBankManager::ms_poInstance[+0x97c744] (a signed byte) before the
 * dispatch either way -- own field semantics on CKGBankManager's giant
 * opaque aggregate out of scope, transcribed as raw byte offsets matching
 * the established convention elsewhere in this file. */
void CKGMIDIOutMsgHandler::SendChannelMessageToMIDIPort()
{
	unsigned char savedData1 = m_data1;
	unsigned char savedData2 = m_data2;
	m_data2 = (unsigned char)CheckAndGetCorrectCCValue();

	unsigned char *bm = CKGBankManager::ms_poInstance;
	if (*(int *)(bm + 0x97c74c) == 1 &&
	    ((m_status & 0xf0) == 0x90 || (m_status & 0xf0) == 0x80)) {
		int t = (signed char)savedData1 - (signed char)bm[0x97c744];
		if (t > 0x7f)
			t -= 0xc;
		else if (t < 0)
			t += 0xc;
		m_data1 = (unsigned char)t;
	}
	SendChannelMessageOfActiveTimbreToMIDIPort();
	m_data1 = savedData1;
	m_data2 = savedData2;
}

/* .text+0x3bbb80, regparm(3). Gate chain: channel!=0, then
 * CKGBankManager::ms_poInstance[+0x97c749]!=0, then base
 * CheckGlobalFilter(). If (status&0xf) == CMIDIFlowParamHolder's
 * GetLocalControlChannel(), additionally requires
 * ShouldSendChannelMessageToMIDIPortInEachMode(); either way then folds
 * into the shared tail: default true unless
 * CKGBankManager::ms_poInstance[+0x97c7bf]==0 (forces false), or the
 * message is CC with m_data1>=0x78 (also forces false). */
bool CKGMIDIOutMsgHandler::ShouldSendChannelMessageToMIDIPort()
{
	if ((m_flags & 0xf) == 0)
		return false;
	unsigned char *bm = CKGBankManager::ms_poInstance;
	if (bm[0x97c749] == 0)
		return false;
	if (!CheckGlobalFilter())
		return false;

	unsigned channel = m_status & 0xf;
	int localCh = ((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->GetLocalControlChannel();
	if ((unsigned)localCh == channel) {
		if (!ShouldSendChannelMessageToMIDIPortInEachMode())
			return false;
	}
	bool result = (bm[0x97c7bf] != 0);
	if ((m_status & 0xf0) == 0xb0 && (signed char)m_data1 >= 0x78)
		result = false;
	return result;
}

/* .text+0x3bba70, regparm(3). CC/PitchBend-shaped gate over
 * CKGBankManager::ms_poInstance's giant opaque aggregate (bits 0x10/0x1
 * of +0x97c748 for the channel==0 cases, +0x97c7b9 for PitchBend
 * channel==4) plus a CMIDIFlowParamHolder::GetLocalControlChannel()
 * comparison for the channel==4 CC/PitchBend cases; every other status
 * type defaults to true. */
bool CKGMIDIOutMsgHandler::ShouldSendChannelMessageToSTG()
{
	unsigned char *bm = CKGBankManager::ms_poInstance;
	unsigned char statusHi = m_status & 0xf0;

	if (statusHi == 0xb0) {
		unsigned char cc = m_data1;
		bool result = true;
		if (cc == 0 || cc == 0x20)
			result = (bm[0x97c748] >> 1) & 1;

		unsigned char channel = m_flags & 0xf;
		if (channel == 0) {
			if (!(bm[0x97c748] & 0x10))
				result = false;
			return result;
		}
		if (channel != 4)
			return true;
		int localCh = ((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->GetLocalControlChannel();
		return channel != (unsigned)localCh;
	}
	if (statusHi == 0xe0) {
		unsigned char channel = m_flags & 0xf;
		if (channel == 0)
			return (bm[0x97c748] >> 4) & 1;
		if (channel != 4)
			return true;
		return bm[0x97c7b9] != 0;
	}
	return true;
}

/* .text+0x3bb980, regparm(3). Same CC/PitchBend gate shape as
 * ShouldSendChannelMessageToSTG() above but against
 * SKSTGGate_CheckVJSCCToMIDIPortFilter() for CC and different
 * CKGBankManager bit offsets/positions for PitchBend -- verified
 * independently, not assumed to mirror the sibling method. */
bool CKGMIDIOutMsgHandler::ShouldRecChannelMessageToSequencer()
{
	unsigned char *bm = CKGBankManager::ms_poInstance;
	unsigned char statusHi = m_status & 0xf0;

	if (statusHi == 0xb0) {
		unsigned char cc = m_data1;
		bool result = (cc == 0 || cc == 0x20) ? (bool)((bm[0x97c748] >> 1) & 1) : true;

		unsigned char channel = m_flags & 0xf;
		if (channel == 0 && !(bm[0x97c748] & 0x10))
			result = false;
		if (!SKSTGGate_CheckVJSCCToMIDIPortFilter((signed char)cc, channel))
			result = false;
		return result;
	}
	if (statusHi == 0xe0) {
		unsigned char channel = m_flags & 0xf;
		if (channel == 0)
			return (bm[0x97c748] >> 4) & 1;
		if (channel == 4)
			return bm[0x97c7b9] != 0;
		return true;
	}
	return true;
}

/* .text+0x3bbc20, regparm(3). Fully self-contained dispatcher --
 * CMIDIFlowParamHolder::GetVoiceMode() selects between the 3 own
 * sibling virtuals (mode 1=Program, 2=Song, 0=Combi; confirmed by
 * cross-checking each branch's own vtable-slot call target, NOT
 * assumed from the mode-number/method-name ordering). */
void CKGMIDIOutMsgHandler::SendChannelMessageOfActiveTimbreToMIDIPort()
{
	int mode = ((CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis)->GetVoiceMode();
	if (mode == 1)
		SendExecToMIDIPortInProgram();
	else if (mode == 2)
		SendExecToMIDIPortInSong();
	else if (mode == 0)
		SendExecToMIDIPortInCombi();
}

/* .text+0x3bb730, 15 bytes. Trivial forward to the base class's own
 * SendChannelMessageToMIDIPortWithCorrectLength(). */
void CKGMIDIOutMsgHandler::SendExecToMIDIPortInProgram()
{
	SendChannelMessageToMIDIPortWithCorrectLength();
}

/* .text+0x3bb740, regparm(3): this=EAX, hiNote=EDX, loNote=ECX,
 * hiVel=stack0, loVel=stack4 (confirmed by the comparison directions,
 * not assumed from the declared parameter names). */
bool CKGMIDIOutMsgHandler::CheckZoneOfNoteOn(int hiNote, int loNote, int hiVel, int loVel)
{
	if (m_flags & 0x40)
		return true;
	signed char note = (signed char)m_data1;
	if (loNote > note || note > hiNote)
		return false;
	signed char vel = (signed char)m_data2;
	return (loVel <= vel) && (vel <= hiVel);
}

/* .text+0x3bb780, regparm(3): this=EAX, hiNote=EDX, loNote=ECX. Same
 * zone shape as CheckZoneOfNoteOn() above, minus the velocity check. */
bool CKGMIDIOutMsgHandler::CheckZoneOfNoteOff(int hiNote, int loNote)
{
	if (m_flags & 0x40)
		return true;
	signed char note = (signed char)m_data1;
	return (loNote <= note) && (note <= hiNote);
}

/* .text+0x3bb840, regparm(3). NoteOn always marks the BACKUP array
 * (m_dyingNoteInfoBackup[channel]) and unconditionally reports true.
 * NoteOff checks the PRIMARY array first: if already on there, fires
 * ProcessForDyingNote() (self, using the message currently in
 * m_status/m_data1) and clears it, reporting false; otherwise falls
 * back to the backup array, clearing it and reporting true only if it
 * was set there. Any other status type reports true (default). */
bool CKGMIDIOutMsgHandler::CheckDyingNoteForMIDIPort()
{
	int note = (signed char)m_data1;
	unsigned char statusHi = m_status & 0xf0;
	unsigned channel = m_status & 0xf;

	if (statusHi == 0x90) {
		m_dyingNoteInfoBackup[channel].TurnOn(note);
		return true;
	}
	if (statusHi == 0x80) {
		CDyingNoteInfo *primary = &m_dyingNoteInfo[channel];
		if (primary->IsNoteOn(note)) {
			ProcessForDyingNote();
			primary->TurnOff(note);
			return false;
		}
		CDyingNoteInfo *backup = &m_dyingNoteInfoBackup[channel];
		if (!backup->IsNoteOn(note))
			return false;
		backup->TurnOff(note);
		return true;
	}
	return true;
}

/* .text+0x3bb930, regparm(3). Brackets the dispatch with
 * CMIDIFlowParamHolder::SetStatus(1)/SetStatus(0) -- confirmed via the
 * literal 1/0 EStatus arguments, real enumerator names beyond that
 * unconfirmed (see CMIDIFlowParamHolder's own class comment). */
void CKGMIDIOutMsgHandler::ProcessForDyingNote()
{
	CMIDIFlowParamHolder *mf = (CMIDIFlowParamHolder *)CMIDIFlowParamHolder::ms_poThis;
	mf->SetStatus(CMIDIFlowParamHolder::eStatus_1);
	if (ShouldSendChannelMessageToMIDIPort())
		SendChannelMessageToMIDIPort();
	mf->SetStatus(CMIDIFlowParamHolder::eStatus_0);
}

/* .text+0x3bc320, 621 bytes, regparm(3). Outer loop over the 16
 * per-channel CDyingNoteInfo::IsAnyNotesOn() slots; inner loop over all
 * 128 notes draining each one still-on note via
 * ProcessForDyingNote()+TurnOff() (compiled as a do-while re-check --
 * functionally a plain "while (IsNoteOn(note))" per note). Once every
 * channel is drained, restores the primary array from the backup array
 * and re-Initialize()s the backup (compiler software-pipelined this
 * pair of loops across iterations in the real binary -- see
 * re-decompiler agent memory for the raw scheduling trace -- but the
 * observable effect is exactly the 2 straight-line loops below). */
void CKGMIDIOutMsgHandler::KillAllDyingNotes()
{
	for (unsigned objIdx = 0; objIdx < 16; objIdx++) {
		CDyingNoteInfo *dying = &m_dyingNoteInfo[objIdx];
		if (!dying->IsAnyNotesOn())
			continue;
		m_status = (unsigned char)((m_status & 0xf0) | (objIdx & 0xf));
		for (int note = 0; note < 128; note++) {
			while (dying->IsNoteOn(note)) {
				m_data1 = (unsigned char)note;
				ProcessForDyingNote();
				dying->TurnOff(note);
			}
		}
	}
	for (unsigned i = 0; i < 16; i++) {
		m_dyingNoteInfo[i] = m_dyingNoteInfoBackup[i];
		m_dyingNoteInfoBackup[i].Initialize();
	}
}

/* ==================== CKGCCResetHandler ==================== */

/* .text+0x3baf90, 76 bytes. Calls its own InitializeControllerMembers()
 * (rodata 0xc, out of scope), forces raw offsets +0xc/+0x2c to 0xff, then
 * duplicates a 0x66-byte chunk [+0xc,+0x72) to [+0x72,+0xd8) -- real
 * per-byte field meaning not modeled, transcribed as a raw byte-level
 * memcpy matching the exact real length/offsets. */
void CKGCCResetHandler::Initialize()
{
	InitializeControllerMembers();

	m_raw[0xc - 4] = 0xff;
	m_raw[0x2c - 4] = 0xff;
	__builtin_memcpy(m_raw + (0x72 - 4), m_raw + (0xc - 4), 0x66);
}

/* .text+0x3bafe0, 128 bytes. Two zero-fill runs (ground truth offset
 * [0xd,0x2b) and [0x2d,0x71), deliberately skipping indices 0xc and
 * 0x2c -- both are the 2 bytes Initialize() itself pokes to 0xff right
 * after calling this, so the skip is real, not an oversight) followed
 * by ~20 individual literal byte pokes, all transcribed at their exact
 * real offsets. */
void CKGCCResetHandler::InitializeControllerMembers()
{
	for (unsigned d = 1; d != 0x20; d++)
		m_raw[(0xc + d) - 4] = 0;
	for (unsigned d = 0x21; d != 0x66; d++)
		m_raw[(0xc + d) - 4] = 0;

	m_raw[0x16 - 4] = 0x40;
	m_raw[0x14 - 4] = 0x40;
	m_raw[0x1c - 4] = 0x40;
	m_raw[0x1d - 4] = 0xff;
	m_raw[0x1f - 4] = 0xff;
	m_raw[0x20 - 4] = 0xff;
	m_raw[0x21 - 4] = 0xff;
	m_raw[0x52 - 4] = 0x40;
	m_raw[0x53 - 4] = 0x40;
	m_raw[0x54 - 4] = 0x40;
	m_raw[0x55 - 4] = 0x40;
	m_raw[0x56 - 4] = 0x40;
	m_raw[0x57 - 4] = 0x40;
	m_raw[0x58 - 4] = 0x40;
	m_raw[0x59 - 4] = 0x40;
	m_raw[0x5a - 4] = 0x40;
	m_raw[0x5b - 4] = 0x40;
	m_raw[0x13 - 4] = 0x7f;
	m_raw[0x17 - 4] = 0x7f;
	m_raw[0x4d - 4] = 0xff;
	m_raw[0x11 - 4] = 0xff;
	m_raw[0xd8 - 4] = 0;
}

/* .text+0x3bb070, 83 bytes. Same InitializeControllerMembers() call +
 * 0x66-byte [+0xc,+0x72)->[+0x72,+0xd8) copy as Initialize() above, but
 * WITHOUT Initialize()'s own 2 extra 0xff pokes. */
void CKGCCResetHandler::InitializeValue()
{
	InitializeControllerMembers();
	__builtin_memcpy(m_raw + (0x72 - 4), m_raw + (0xc - 4), 0x66);
}

/* .text+0x3bb0c0, 96 bytes, regparm(3): this=EAX, msg=EDX.
 * CMIDIMessage's own raw layout is a packed status/data1/data2/flags
 * dword at msg+0 (a genuinely different convention from this project's
 * usual +4..+7 CSKMIDIMsgHandler fields -- see CKGMIDIMsgProcessor::
 * StoreCCMessage()'s own comment for the same real quirk), copied
 * whole into this object's own +4..+7 fields. For CC messages
 * (data1<=0x65) additionally records the value into 2 parallel
 * 0x66-byte tables (+0xc and, unless channel==5, +0x72), then fires the
 * NRPN/reset-all-controllers handlers unconditionally. */
void CKGCCResetHandler::StoreValue(CMIDIMessage *msg)
{
	*(unsigned int *)m_raw = *(unsigned int *)msg;

	if ((m_raw[0] & 0xf0) == 0xb0) {
		unsigned char data1 = m_raw[1];
		if ((signed char)data1 <= 0x65) {
			unsigned char channel = m_raw[3] & 0xf;
			unsigned char data2 = m_raw[2];
			m_raw[(0xc + data1) - 4] = data2;
			if (channel != 5)
				m_raw[(0x72 + data1) - 4] = data2;
		}
		HandleNRPNMessage();
		HandleccidResetAllController();
	}
}

/* .text+0x3bb4e0, 297 bytes, regparm(3). Resets this handler's own
 * KARMA-generated CC value on STG (data1=0x79 "Reset All Controllers"),
 * then re-sends every one of the ~0x66 tracked CC indices via
 * SendResetValue() EXCEPT index 5 -- 3 individually-unrolled skips
 * (7/8/9, 0xb..0x1f, 0x21..0x40, 0x42..0x5a, 0x5c/0x5e/0x5f) matching
 * the real disassembly's own literal sequence exactly, including index
 * 7 only being sent when CKGEngine::ms_poInstance[+0xa0] (an opaque
 * pointer, own semantics out of scope) is NULL. */
void CKGCCResetHandler::ResetKarmaGeneratedValue()
{
	unsigned char ch = m_raw[0xdc - 4];
	((CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance)->
		ProcessKarmaGeneratedChannelMessage(0xb0, ch, 0x79, 0, false);

	if (*(void **)(CKGEngine::ms_poInstance + 0xa0) == 0)
		SendResetValue(7);

	SendResetValue(8);
	SendResetValue(9);
	for (int i = 0xb; i != 0x20; i++)
		SendResetValue(i);
	for (int i = 0x21; i != 0x41; i++)
		SendResetValue(i);
	for (int i = 0x42; i != 0x5b; i++)
		SendResetValue(i);
	SendResetValue(0x5c);
	SendResetValue(0x5e);
	SendResetValue(0x5f);
}

/* .text+0x3bb120, 37 bytes, regparm(3). Own m_data1==0x79 ("Reset All
 * Controllers") gate, skipped entirely on channel 5, otherwise
 * forwards into InitializeValue() (rodata 0x10). */
void CKGCCResetHandler::HandleccidResetAllController()
{
	if (m_raw[1] != 0x79)
		return;
	if ((m_raw[3] & 0xf) == 5)
		return;
	InitializeValue();
}

/* .text+0x3bb150, 381 bytes, regparm(3). Switches on this object's own
 * m_data1 (own last-stored CC number, +5): a handful of literal NRPN
 * sub-opcodes (0x06/0x26/0x60/0x61/0x63/0x64/0x65) each gate on/mutate
 * a flags byte at +0xd8 and, for 3 of them, forward into
 * ProcessNRPN()/ProcessNRPNIncDec(); every other CC number is a no-op.
 * The 4 "OR mask then re-mask on a bit test" cases (0x62/0x63/0x64/0x65)
 * share a real jump into a common `&= ~0x30` tail in ground truth --
 * modeled here as identical inline logic per case rather than a
 * literal goto, same observable effect. */
void CKGCCResetHandler::HandleNRPNMessage()
{
	unsigned char dl = m_raw[0x5 - 4];

	switch (dl) {
	case 0x60:
		if ((m_raw[0xd8 - 4] & 0x8c) == 0x8c)
			ProcessNRPNIncDec(1);
		break;
	case 0x61:
		if ((m_raw[0xd8 - 4] & 0x8c) == 0x8c)
			ProcessNRPNIncDec(-1);
		break;
	case 0x26:
		if ((m_raw[0xd8 - 4] & 0x43) == 0x43)
			m_raw[0xd8 - 4] |= 0x20;
		break;
	case 0x06: {
		unsigned char v = m_raw[0xd8 - 4];
		if ((v & 0x43) == 0x43)
			m_raw[0xd8 - 4] = v | 0x10;
		else if ((v & 0x8c) == 0x8c)
			ProcessNRPN((signed char)m_raw[0x6 - 4]);
		break;
	}
	case 0x63: {
		unsigned char v = (unsigned char)((m_raw[0xd8 - 4] & ~0x40) | 0x84);
		m_raw[0xd8 - 4] = v;
		if (v & 0x8)
			m_raw[0xd8 - 4] = v & ~0x30;
		break;
	}
	case 0x64: {
		unsigned char v = (unsigned char)((m_raw[0xd8 - 4] & 0x7f) | 0x42);
		m_raw[0xd8 - 4] = v;
		if (v & 0x1)
			m_raw[0xd8 - 4] = v & ~0x30;
		break;
	}
	case 0x65: {
		unsigned char v = (unsigned char)((m_raw[0xd8 - 4] & 0x7f) | 0x41);
		m_raw[0xd8 - 4] = v;
		if (v & 0x2)
			m_raw[0xd8 - 4] = v & ~0x30;
		break;
	}
	case 0x62: {
		unsigned char v = (unsigned char)((m_raw[0xd8 - 4] & ~0x40) | 0x88);
		m_raw[0xd8 - 4] = v;
		if (v & 0x4)
			m_raw[0xd8 - 4] = v & ~0x30;
		break;
	}
	default:
		break;
	}
}

/* .text+0x3bb3a0, 114 bytes, regparm(3): this=EAX, val=EDX. Gated on
 * +0x6f==1 (own flag, semantics out of scope); maps this object's own
 * +0x6e byte through ConvertToneModifyToCC(), then AdjustNRPN()s `val`
 * against that mapped CC number and stores the result into both
 * parallel CC tables (+0xc and +0x72, unconditionally -- unlike
 * StoreValue() there is no channel==5 skip here). Real ground truth
 * leaves EAX = a stale `this`-derived value on the early-exit path;
 * every real caller of this method ignores its return value, so
 * returning 0 there is a faithful-enough substitute. */
int CKGCCResetHandler::ProcessNRPN(int val)
{
	if (m_raw[0x6f - 4] != 1)
		return 0;
	int cc = ConvertToneModifyToCC(m_raw[0x6e - 4]);
	if (cc == 0xff)
		return 0;
	int adjusted = AdjustNRPN(cc, val);
	m_raw[(0xc + cc) - 4] = (unsigned char)adjusted;
	m_raw[(0x72 + cc) - 4] = (unsigned char)adjusted;
	return adjusted;
}

/* .text+0x3bb420, 108 bytes, regparm(3): this=EAX, delta=EDX. Same
 * +0x6f/+0x6e/ConvertToneModifyToCC() gate as ProcessNRPN() above, but
 * adds `delta` to the CC table's CURRENT value (clamped to [0,0x7f])
 * instead of computing a fresh value via AdjustNRPN(). */
int CKGCCResetHandler::ProcessNRPNIncDec(int delta)
{
	if (m_raw[0x6f - 4] != 1)
		return 0;
	int cc = ConvertToneModifyToCC(m_raw[0x6e - 4]);
	if (cc == 0xff)
		return 0;
	int cur = m_raw[(0xc + cc) - 4];
	int sum = delta + cur;
	int clamped = (sum <= 0x7f) ? sum : 0x7f;
	if (clamped < 0)
		clamped = 0;
	m_raw[(0xc + cc) - 4] = (unsigned char)clamped;
	m_raw[(0x72 + cc) - 4] = (unsigned char)clamped;
	return clamped;
}

/* .text+0x3bb2f0, 92 bytes, regparm(3): this unused (pure function of
 * `val`). Fixed literal NRPN-number -> CC-number map; 0xff sentinel for
 * anything unmapped. */
int CKGCCResetHandler::ConvertToneModifyToCC(int val)
{
	switch (val) {
	case 0x63: return 0x49;
	case 0x64: return 0x4b;
	case 0x66: return 0x48;
	case 0x20: return 0x4a;
	case 0x09: return 0x4d;
	case 0x0a: return 0x4e;
	case 0x08: return 0x4c;
	case 0x21: return 0x47;
	default: return 0xff;
	}
}

/* .text+0x3bb350, 80 bytes, regparm(3): this unused, which=EDX,
 * val=ECX. Fixed per-CC-number rescale formulas for 3 CC numbers
 * (0x4d/0x4e/0x47); everything else passes `val` through unchanged. */
int CKGCCResetHandler::AdjustNRPN(int which, int val)
{
	switch (which) {
	case 0x4d:
		if (val <= 0x40)
			return val;
		return (((3 * val) - 0xc0) >> 2) + 0x40;
	case 0x4e:
		return ((((5 * val) - 0x140) * 2) >> 4) + 0x40;
	case 0x47:
		return (val >> 1) + 0x20;
	default:
		return val;
	}
}

/* .text+0x3bb490, 66 bytes, regparm(3): this=EAX, ccIndex=EDX. No-op if
 * the 2 parallel CC tables already agree at this index; otherwise
 * forwards a KARMA-reset-CC channel message via
 * CKGMIDIMsgProcessor::ms_poInstance, tagged with this handler's own
 * channel/index (+0xdc, set by the ctor). */
void CKGCCResetHandler::SendResetValue(int ccIndex)
{
	unsigned char secondary = m_raw[(0x72 + ccIndex) - 4];
	if (secondary == m_raw[(0xc + ccIndex) - 4])
		return;
	unsigned char ownChannel = m_raw[0xdc - 4];
	((CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance)->
		ProcessKarmaResetCCChannelMessage(0xb0, ownChannel,
						   (char)ccIndex, (char)secondary);
}

/* ==================== CKGMIDIMsgProcessor ====================
 * See oa_ckg_midi_msg_handler.h's own class comment for the full field
 * layout derivation.
 */

/* .text+0x3ba740, 705 bytes. */
CKGMIDIMsgProcessor::CKGMIDIMsgProcessor()
{
	ms_poInstance = (unsigned char *)this;

	m_karmaGen = new (CSTGBankMemory::AllocAligned(0x3090, 0x10)) CKGMIDIKarmaGeneratedMsgHandler();
	m_timbreThru = new (CSTGBankMemory::AllocAligned(0x3090, 0x10)) CKGMIDITimbreThruMsgHandler();
	m_karmaResetCC = new (CSTGBankMemory::AllocAligned(0x3090, 0x10)) CKGMIDIKarmaResetCCMsgHandler();
	m_bendRange = new (CSTGBankMemory::AllocAligned(0xc, 0x10)) CKGBendRangeHandler();

	for (int i = 0; i < 16; i++) {
		m_ccReset[i] = new (CSTGBankMemory::AllocAligned(0xe0, 0x10)) CKGCCResetHandler(i);
		m_ccReset[i]->Initialize();
	}

	m_bSuspended = 0;
}

/* .text+0x3baa20, 207 bytes, regparm(3). */
void CKGMIDIMsgProcessor::ProcessKarmaGeneratedChannelMessage(int statusType, unsigned char channel,
								char data1, char data2, bool changeSource)
{
	if (m_bSuspended)
		return;

	unsigned char combined = (unsigned char)(channel + statusType);
	m_karmaGen->m_status = combined;
	m_karmaGen->m_data1 = (unsigned char)data1;
	m_karmaGen->m_data2 = (unsigned char)data2;
	m_karmaGen->m_flags = (unsigned char)((m_karmaGen->m_flags & 0xf0) | 0x5);

	if (((CKGEngine *)CKGEngine::ms_poInstance)->IsKarmaOn()) {
		m_karmaGen->m_flags |= 0x20;
		m_karmaGen->m_flags = (unsigned char)((m_karmaGen->m_flags & ~0x40) | (changeSource ? 0x40 : 0));
	} else {
		m_karmaGen->m_flags &= 0xdf;
		m_karmaGen->m_flags = (unsigned char)((m_karmaGen->m_flags & ~0x40) | (changeSource ? 0x40 : 0));
	}

	if (((CKGEngine *)CKGEngine::ms_poInstance)
		    ->ShouldForceTimbreZoneBypass(channel, ((CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance)->m_lastMsgKind))
		m_karmaGen->m_flags |= 0x40;

	m_karmaGen->Process();
}

/* .text+0x3bab00, 138 bytes, regparm(3). No changeSource bit -- only the
 * IsKarmaOn() gate on m_flags bit 0x20. */
void CKGMIDIMsgProcessor::ProcessKarmaResetCCChannelMessage(int statusType, unsigned char channel,
							      char data1, char data2)
{
	if (m_bSuspended)
		return;

	unsigned char combined = (unsigned char)(channel + statusType);
	m_karmaResetCC->m_status = combined;
	m_karmaResetCC->m_data1 = (unsigned char)data1;
	m_karmaResetCC->m_data2 = (unsigned char)data2;
	m_karmaResetCC->m_flags = (unsigned char)((m_karmaResetCC->m_flags & 0xf0) | 0x5);

	if (((CKGEngine *)CKGEngine::ms_poInstance)->IsKarmaOn())
		m_karmaResetCC->m_flags |= 0x20;
	else
		m_karmaResetCC->m_flags &= 0xdf;

	m_karmaResetCC->Process();
}

/* .text+0x3bab90, 220 bytes, regparm(3). Same shape as
 * ProcessKarmaGeneratedChannelMessage() above, via m_timbreThru -- but
 * the flags low nibble comes from CSKMIDIMsgProcessor::ms_poInstance's
 * own raw +0x18 byte (m_lastMsgKind's low byte) instead of the literal 5
 * the other 3 Process*ChannelMessage() overloads use. Confirmed via
 * disasm (`or al,0x18(ecx)`), not assumed from the name. */
void CKGMIDIMsgProcessor::ProcessTimbreThruChannelMessage(int statusType, unsigned char channel,
							    char data1, char data2, bool changeSource)
{
	if (m_bSuspended)
		return;

	unsigned char combined = (unsigned char)(channel + statusType);
	m_timbreThru->m_status = combined;
	m_timbreThru->m_data1 = (unsigned char)data1;
	m_timbreThru->m_data2 = (unsigned char)data2;

	unsigned char kindByte = ((unsigned char *)CSKMIDIMsgProcessor::ms_poInstance)[0x18];
	m_timbreThru->m_flags = (unsigned char)((m_timbreThru->m_flags & 0xf0) | kindByte);

	if (((CKGEngine *)CKGEngine::ms_poInstance)->IsKarmaOn()) {
		m_timbreThru->m_flags |= 0x20;
		m_timbreThru->m_flags = (unsigned char)((m_timbreThru->m_flags & ~0x40) | (changeSource ? 0x40 : 0));
	} else {
		m_timbreThru->m_flags &= 0xdf;
		m_timbreThru->m_flags = (unsigned char)((m_timbreThru->m_flags & ~0x40) | (changeSource ? 0x40 : 0));
	}

	if (((CKGEngine *)CKGEngine::ms_poInstance)
		    ->ShouldForceTimbreZoneBypass(channel, ((CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance)->m_lastMsgKind))
		m_timbreThru->m_flags |= 0x40;

	m_timbreThru->Process();
}

/* .text+0x3bac80, 214 bytes, regparm(3). Reuses m_timbreThru (same
 * sub-object as ProcessTimbreThruChannelMessage above), tagged with a
 * fixed low-flags-nibble of 1 -- confirmed via disasm, not assumed from
 * the name. */
void CKGMIDIMsgProcessor::ProcessResetControllerChannelMessage(int statusType, unsigned char channel,
								 char data1, char data2, bool changeSource)
{
	if (m_bSuspended)
		return;

	unsigned char combined = (unsigned char)(channel + statusType);
	m_timbreThru->m_status = combined;
	m_timbreThru->m_data1 = (unsigned char)data1;
	m_timbreThru->m_data2 = (unsigned char)data2;
	m_timbreThru->m_flags = (unsigned char)((m_timbreThru->m_flags & 0xf0) | 0x1);

	if (((CKGEngine *)CKGEngine::ms_poInstance)->IsKarmaOn()) {
		m_timbreThru->m_flags |= 0x20;
		m_timbreThru->m_flags = (unsigned char)((m_timbreThru->m_flags & ~0x40) | (changeSource ? 0x40 : 0));
	} else {
		m_timbreThru->m_flags &= 0xdf;
		m_timbreThru->m_flags = (unsigned char)((m_timbreThru->m_flags & ~0x40) | (changeSource ? 0x40 : 0));
	}

	if (((CKGEngine *)CKGEngine::ms_poInstance)
		    ->ShouldForceTimbreZoneBypass(channel, ((CSKMIDIMsgProcessor *)CSKMIDIMsgProcessor::ms_poInstance)->m_lastMsgKind))
		m_timbreThru->m_flags |= 0x40;

	m_timbreThru->Process();
}

/* .text+0x3bad60, 67 bytes, regparm(3). No IsKarmaOn()/changeSource
 * gating -- unconditionally sets flags bit 0x10 and calls Process().
 * m_status = channel-0x20 (NOT channel directly), confirmed via
 * `lea edx,[edx-0x20]` on the raw register. */
void CKGMIDIMsgProcessor::ProcessKarmaGeneratedBendRangeChannelMessage(unsigned char channel, char data1)
{
	m_bendRange->m_status = (unsigned char)(channel - 0x20);
	m_bendRange->m_data1 = (unsigned char)data1;
	m_bendRange->m_data2 = 0;
	m_bendRange->m_flags = (unsigned char)((m_bendRange->m_flags & 0xf0) | 0x5);
	m_bendRange->m_flags |= 0x10;
	m_bendRange->Process();
}

/* .text+0x3badb0, 25 bytes, regparm(3). msg's own raw byte 0's low
 * nibble selects which of the 16 m_ccReset[] to forward to -- reads
 * msg's FIRST byte directly (no +4 offset, unlike CSKMIDIMsgHandler's
 * own m_status/m_data1/m_data2/m_flags convention elsewhere in this
 * file), matching CMIDIMessage's own distinct raw-byte-0-is-status
 * layout. */
void CKGMIDIMsgProcessor::StoreCCMessage(CMIDIMessage *msg)
{
	int channel = *(unsigned char *)msg & 0xf;
	m_ccReset[channel]->StoreValue(msg);
}

/* .text+0x3badd0, 104 bytes. For every MIDI channel 0-15 that is
 * currently some KARMA module's real output channel (per
 * CKGEngine::GetRealOutputChannel()), calls that channel's own
 * m_ccReset[]->ResetKarmaGeneratedValue(). */
void CKGMIDIMsgProcessor::ResetKarmaGeneratedCCValue()
{
	CKGEngine *engine = (CKGEngine *)CKGEngine::ms_poInstance;

	for (int channel = 0; channel < 16; channel++) {
		int numModules = engine->GetNumOfModule();
		for (int module = 0; module < numModules; module++) {
			if (engine->GetRealOutputChannel(module) == channel) {
				m_ccReset[channel]->ResetKarmaGeneratedValue();
				break;
			}
		}
	}
}

/* .text+0x3bae40, 19 bytes. Direct 1-channel overload -- no module
 * search. */
void CKGMIDIMsgProcessor::ResetKarmaGeneratedCCValue(int channel)
{
	m_ccReset[channel]->ResetKarmaGeneratedValue();
}

/* .text+0x3bae60, 150 bytes. Real body is 16 unrolled calls; identical
 * observable effect as this loop. */
void CKGMIDIMsgProcessor::InitializeCCValue()
{
	for (int channel = 0; channel < 16; channel++)
		m_ccReset[channel]->InitializeValue();
}

/* .text+0x3baf00, 19 bytes. */
void CKGMIDIMsgProcessor::InitializeCCValue(int channel)
{
	m_ccReset[channel]->InitializeValue();
}

/* .text+0x3baf20, 37 bytes. Forces m_timbreThru's raw event to a fixed
 * NoteOff-shaped quad THEN calls its own (non-virtual)
 * CKGMIDIOutMsgHandler::KillAllDyingNotes() -- not a vtable call,
 * confirmed via a direct R_386_PC32 relocation. */
void CKGMIDIMsgProcessor::KillAllDyingNotes()
{
	m_timbreThru->m_status = 0x80;
	m_timbreThru->m_data1 = 0x00;
	m_timbreThru->m_data2 = 0x40;
	m_timbreThru->m_flags = 0x01;
	m_timbreThru->CKGMIDIOutMsgHandler::KillAllDyingNotes();
}
