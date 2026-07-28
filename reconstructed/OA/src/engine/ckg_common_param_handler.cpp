// SPDX-License-Identifier: GPL-2.0
#include "oa_ckg_common_param_msg_handler.h"

/*
 * src/engine/ckg_common_param_handler.cpp  -  CKGCommonParamMsgHandler
 *
 * See oa_ckg_common_param_msg_handler.h for the full family writeup, the
 * shared control-flow skeleton every mechanical method below follows, the
 * Send-call convention -- a genuine deviation from the Module sibling --
 * and the SetTempo/SetScene standalone outliers. Field offsets
 * cross-referenced against CKGSeqBackupCommonParam, karma_seq_backup.cpp.
 */

/*
 * CTimerManager::ms_poInstance's own storage already lives in
 * sk_stg_gate.cpp -- reused here, not redefined.
 */

/*
 * ShouldAttemptSysExShadowWrite / SysExShadowWriteIsNeeded -- byte-
 * identical shared blocks, register allocation aside, confirmed across
 * every one of this batch's own disassembled instances, same "factored
 * out purely to avoid textual duplication" convention as the Module
 * sibling -- ground truth inlines both into every method separately.
 */
bool CKGCommonParamMsgHandler::ShouldAttemptSysExShadowWrite() const
{
	if (CSPREngine::ms_poInstance[0xa] == 0)
		return false;
	if (m_defaultRecordA == 0)
		return false;
	int mode = *(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c);
	if (mode == 4)
		return false;
	return (unsigned int)(mode - 8) > 2u;	/* mode outside {8,9,10} */
}

bool CKGCommonParamMsgHandler::SysExShadowWriteIsNeeded(const CKGCommonParamMsg *msg) const
{
	long unused;
	signed char cfg = *(signed char *)(CKGBankManager::ms_poInstance + 0x97c747);
	return CSPRMIDIMsgProcessor::ms_poSysExPlayBuf->GetValue(
		0x6d, cfg, 5, msg->m_index, 0, m_moduleIndex, 0, &unused) == 0;
}

/* ================= Standalone outliers -- not mechanically derived from
 * the seqbackup cross-reference, traced directly from disassembly. See
 * header comment for the full trace of each. ================= */

void *CKGCommonParamMsgHandler::GetKarmaPerfCommon(const CKGCommonParamMsg *msg)
{
	CKGBankManager *bankMgr = (CKGBankManager *)CKGBankManager::ms_poInstance;

	if (msg->m_kind == 1) {
		return bankMgr->GetProgKarmaPerfCommon((eSTGProgramBankId)msg->m_bankId,
							(unsigned int)msg->m_karmaIndexOrSentinel);
	}
	if (msg->m_kind == 2) {
		return bankMgr->GetSeqKarmaPerfCommon((unsigned int)msg->m_karmaIndexOrSentinel);
	}
	if (msg->m_kind == 0) {
		return bankMgr->GetCombiKarmaPerfCommon((eSTGCombiBankId)msg->m_bankId,
							 (unsigned int)msg->m_karmaIndexOrSentinel);
	}
	/* real explicit fallback -- unlike the Module sibling's own
	 * GetKarmaModule(), any m_kind value outside {0,1,2} returns NULL
	 * here, confirmed via the real disassembly's own 4th arm. */
	return 0;
}

void *CKGCommonParamMsgHandler::GetKarmaPerfCommonForSeqBackup(const CKGCommonParamMsg *msg)
{
	if (CSPREngine::ms_poInstance[0xa] == 0)
		return 0;
	if (msg->m_kind != 2)
		return 0;

	unsigned int index = (unsigned int)msg->m_karmaIndexOrSentinel;
	if (index == 0xffff)
		index = *(unsigned int *)(CKGBankManager::ms_poInstance + 0x97c7d4);

	return ((CKGBankManager *)CKGBankManager::ms_poInstance)->GetSeqKarmaPerfCommon(index);
}

bool CKGCommonParamMsgHandler::ShouldStoreToBackup(const CKGCommonParamMsg *msg)
{
	if (!ShouldAttemptSysExShadowWrite())
		return false;
	if (!SysExShadowWriteIsNeeded(msg))
		return false;
	*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
	return true;
}

/*
 * SetChordMemVelocity -- confirmed real no-op: the entire function body
 * in ground truth is a bare `ret`, `.text+0x3c5220`, 1 byte. Not a
 * transcription gap -- verified via direct disassembly, GCC genuinely
 * compiled this to nothing -- likely a dead-code-eliminated write the
 * compiler proved had no observable effect in the real source, or a
 * deliberately stubbed-out real method; either way, ground truth is
 * unambiguous.
 */
void CKGCommonParamMsgHandler::SetChordMemVelocity(const CKGCommonParamMsg *msg)
{
}

/*
 * SetTempo -- see header comment for the full trace. The one method in
 * this class with a real non-const `CKGCommonParamMsg*` -- the "returns
 * false" path genuinely mutates msg->m_value to report back the OLD live
 * tempo to the caller.
 */
void CKGCommonParamMsgHandler::SetTempo(CKGCommonParamMsg *msg)
{
	if (CKGEngine::ms_poInstance[0xb0] != 0) {
		/* suppressed: skip the whole gate, just mirror the new value */
		*(unsigned short *)m_liveRecord = (unsigned short)msg->m_value;
		return;
	}

	unsigned short oldTempo = *(unsigned short *)m_liveRecord;

	if (KGOutGate_CheckAndSetTempoForOtherModule(msg->m_value)) {
		CKGEngine::ms_poKGParamEdit->SendTempo((unsigned short)msg->m_value);
		if (!CTimerManager::ms_poInstance->ShouldSyncExternalClock()) {
			*(unsigned short *)m_liveRecord = (unsigned short)msg->m_value;
			if (m_defaultRecordA != 0)
				*(unsigned short *)m_defaultRecordA = (unsigned short)msg->m_value;
			((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit(true, oldTempo);
			return;
		}
		/* ShouldSyncExternalClock() true: falls through to the same
		 * "report old value back, no record write" tail as the
		 * CheckAndSetTempoForOtherModule()==false path below. */
	}

	msg->m_value = oldTempo;
	((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit(false, oldTempo);
}

/*
 * SetScene -- see header comment for the full trace, including the real
 * 4-module linked-scene-id broadcast loop -- a genuinely different shape
 * from CKGModuleParamMsgHandler::SetScene's own +0x14-guarded logic.
 */
void CKGCommonParamMsgHandler::SetScene(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + 0x135;
		*p = (unsigned char)((*p & ~0x7) | (msg->m_value & 0x7));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = m_defaultRecordA + 0x135;
				*p = (unsigned char)((*p & ~0x7) | (msg->m_value & 0x7));
			}
			{
				unsigned char *p = m_defaultRecordB + 0x135;
				*p = (unsigned char)((*p & ~0x7) | (msg->m_value & 0x7));
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] != 0)
		return;		/* suppressed: skip Send/Notify AND the broadcast loop below */

	CKGEngine::ms_poKGParamEdit->SendScene(0, (unsigned char)msg->m_value, false);
	((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();

	unsigned char *base = *(unsigned char **)(CKGBankManager::ms_poInstance + 0x4);
	for (int mod = 0; mod < 4; mod++) {
		unsigned char *rec = base + mod * 0x2e8;
		if (rec[0x2e4] & 0x8) {
			int nibIdx = (unsigned char)msg->m_value >> 1;
			unsigned char packed = rec[0x2e4 + nibIdx];
			int scene = (msg->m_value & 1) ? ((packed >> 4) & 0x7) : (packed & 0x7);
			((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->SendModuleSceneMessage(mod, scene);
		}
	}
}

/* ================= Mechanical Shape-B methods, cross-referenced against
 * CKGSeqBackupCommonParam. See header comment for the shared skeleton and
 * the Send-call convention table. ================= */

void CKGCommonParamMsgHandler::SetTimeSig(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0x3) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0x3) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTimeSig((unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetPadMode(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x2);
		unsigned char keep = (unsigned char)~(0x1 << 5);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x2);
				unsigned char keep = (unsigned char)~(0x1 << 5);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendPadMode((unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetModuleControl(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x2);
		unsigned char keep = (unsigned char)~(0x7 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x2);
				unsigned char keep = (unsigned char)~(0x7 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendBufferSelect((unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetLatch(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x2);
		unsigned char keep = (unsigned char)~(0x1 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x2);
				unsigned char keep = (unsigned char)~(0x1 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendLatch((bool)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetOnOff(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x2);
		unsigned char keep = (unsigned char)~(0x1 << 7);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x2);
				unsigned char keep = (unsigned char)~(0x1 << 7);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendOnOff((bool)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSwName(const CKGCommonParamMsg *msg)
{
	*(unsigned short *)(m_liveRecord + msg->m_index * 2 + 0x4) = (unsigned short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned short *)(m_defaultRecordA + msg->m_index * 2 + 0x4) = (unsigned short)msg->m_value;
			*(unsigned short *)(m_defaultRecordB + msg->m_index * 2 + 0x4) = (unsigned short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnobName(const CKGCommonParamMsg *msg)
{
	*(unsigned short *)(m_liveRecord + msg->m_index * 2 + 0x14) = (unsigned short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned short *)(m_defaultRecordA + msg->m_index * 2 + 0x14) = (unsigned short)msg->m_value;
			*(unsigned short *)(m_defaultRecordB + msg->m_index * 2 + 0x14) = (unsigned short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetNoteMapTableValue(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index + 0x17e) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index + 0x17e) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendNoteMapTable((int)msg->m_index, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSceneChangeQuantizeValue(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x135);
		unsigned char keep = (unsigned char)~(0xf << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x135);
				unsigned char keep = (unsigned char)~(0xf << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSceneChangeQuantize((int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIInput(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x24);
		unsigned char keep = (unsigned char)~(0x7 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x24);
				unsigned char keep = (unsigned char)~(0x7 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynInputModule((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIPolarity(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x24);
		unsigned char keep = (unsigned char)~(0x3 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x24);
				unsigned char keep = (unsigned char)~(0x3 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynPolarity((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDISource(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x25) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x25) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynSource((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIDest(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x26) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x26) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynDestination((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIUseA(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x27);
		unsigned char keep = (unsigned char)~(0x1 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x27);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynModule((unsigned char)msg->m_index, (unsigned char)0, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIUseB(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x27);
		unsigned char keep = (unsigned char)~(0x1 << 1);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x27);
				unsigned char keep = (unsigned char)~(0x1 << 1);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynModule((unsigned char)msg->m_index, (unsigned char)1, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIUseC(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x27);
		unsigned char keep = (unsigned char)~(0x1 << 2);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x27);
				unsigned char keep = (unsigned char)~(0x1 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynModule((unsigned char)msg->m_index, (unsigned char)2, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIUseD(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x27);
		unsigned char keep = (unsigned char)~(0x1 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x27);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynModule((unsigned char)msg->m_index, (unsigned char)3, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIUseLast(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x27);
		unsigned char keep = (unsigned char)~(0x1 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x27);
				unsigned char keep = (unsigned char)~(0x1 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynModule((unsigned char)msg->m_index, (unsigned char)4, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIUseAction(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x27);
		unsigned char keep = (unsigned char)~(0x3 << 5);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 5);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x27);
				unsigned char keep = (unsigned char)~(0x3 << 5);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 5);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynAction((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIUseTop(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x28) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x28) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynTop((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDynMIDIUseBottom(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 6 + 0x29) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 6 + 0x29) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDynBottom((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamGroup(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 10 + 0x54) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 10 + 0x54) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPParamGroup((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamAssign(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 10 + 0x55) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 10 + 0x55) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPParamAssign((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamA(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 10 + 0x56);
		unsigned char keep = (unsigned char)~(0x1 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPModule((unsigned char)msg->m_index, (unsigned char)0, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamB(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 10 + 0x56);
		unsigned char keep = (unsigned char)~(0x1 << 1);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x1 << 1);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x1 << 1);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPModule((unsigned char)msg->m_index, (unsigned char)1, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamC(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 10 + 0x56);
		unsigned char keep = (unsigned char)~(0x1 << 2);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x1 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x1 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPModule((unsigned char)msg->m_index, (unsigned char)2, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamD(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 10 + 0x56);
		unsigned char keep = (unsigned char)~(0x1 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPModule((unsigned char)msg->m_index, (unsigned char)3, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamPolarity(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 10 + 0x56);
		unsigned char keep = (unsigned char)~(0x7 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 10 + 0x56);
				unsigned char keep = (unsigned char)~(0x7 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPPolarity((unsigned char)msg->m_index, (char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamKnob(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x57) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 10 + 0x57) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPDestKnob((unsigned char)msg->m_index, (char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamMin(const CKGCommonParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 10 + 0x58) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 10 + 0x58) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 10 + 0x58) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPMinValue((unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamMax(const CKGCommonParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 10 + 0x5a) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 10 + 0x5a) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 10 + 0x5a) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPMaxValue((unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetRTParamValue(const CKGCommonParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 10 + 0x5c) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 10 + 0x5c) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 10 + 0x5c) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTPValue((unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote1(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 18 + 0xa4) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 18 + 0xa4) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemNote((int)msg->m_index, (int)0, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote2(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 18 + 0xa6) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 18 + 0xa6) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemNote((int)msg->m_index, (int)1, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote3(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 18 + 0xa8) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 18 + 0xa8) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemNote((int)msg->m_index, (int)2, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote4(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 18 + 0xaa) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 18 + 0xaa) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemNote((int)msg->m_index, (int)3, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote5(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 18 + 0xac) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 18 + 0xac) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemNote((int)msg->m_index, (int)4, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote6(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 18 + 0xae) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 18 + 0xae) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemNote((int)msg->m_index, (int)5, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote7(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 18 + 0xb0) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 18 + 0xb0) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemNote((int)msg->m_index, (int)6, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote8(const CKGCommonParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 18 + 0xb2) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 18 + 0xb2) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemNote((int)msg->m_index, (int)7, (int)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote1Vel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xa5) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xa5) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemVelocity((int)msg->m_index, (int)0, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote2Vel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xa7) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xa7) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemVelocity((int)msg->m_index, (int)1, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote3Vel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xa9) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xa9) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemVelocity((int)msg->m_index, (int)2, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote4Vel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xab) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xab) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemVelocity((int)msg->m_index, (int)3, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote5Vel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xad) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xad) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemVelocity((int)msg->m_index, (int)4, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote6Vel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xaf) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xaf) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemVelocity((int)msg->m_index, (int)5, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote7Vel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xb1) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xb1) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemVelocity((int)msg->m_index, (int)6, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemNote8Vel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xb3) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xb3) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemVelocity((int)msg->m_index, (int)7, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetChordMemChannel(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 18 + 0xb5) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 18 + 0xb5) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendChordMemChannel((int)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSw1Value(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x136);
		unsigned char keep = (unsigned char)~(0x1 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(0, (int)msg->m_index, 0, (bool)(msg->m_value != 0), false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSw2Value(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x136);
		unsigned char keep = (unsigned char)~(0x1 << 1);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 1);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 1);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(0, (int)msg->m_index, 0, (bool)(msg->m_value != 0), false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSw3Value(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x136);
		unsigned char keep = (unsigned char)~(0x1 << 2);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(0, (int)msg->m_index, 0, (bool)(msg->m_value != 0), false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSw4Value(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x136);
		unsigned char keep = (unsigned char)~(0x1 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(0, (int)msg->m_index, 0, (bool)(msg->m_value != 0), false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSw5Value(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x136);
		unsigned char keep = (unsigned char)~(0x1 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(0, (int)msg->m_index, 0, (bool)(msg->m_value != 0), false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSw6Value(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x136);
		unsigned char keep = (unsigned char)~(0x1 << 5);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 5);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 5);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(0, (int)msg->m_index, 0, (bool)(msg->m_value != 0), false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSw7Value(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x136);
		unsigned char keep = (unsigned char)~(0x1 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(0, (int)msg->m_index, 0, (bool)(msg->m_value != 0), false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetSw8Value(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x136);
		unsigned char keep = (unsigned char)~(0x1 << 7);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 7);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
			}
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x136);
				unsigned char keep = (unsigned char)~(0x1 << 7);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(0, (int)msg->m_index, 0, (bool)(msg->m_value != 0), false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnob1Value(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x137) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x137) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x137) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKnob(0, (int)msg->m_index, 0, (int)msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnob2Value(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x138) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x138) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x138) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKnob(0, (int)msg->m_index, 0, (int)msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnob3Value(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x139) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x139) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x139) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKnob(0, (int)msg->m_index, 0, (int)msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnob4Value(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x13a) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x13a) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x13a) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKnob(0, (int)msg->m_index, 0, (int)msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnob5Value(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x13b) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x13b) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x13b) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKnob(0, (int)msg->m_index, 0, (int)msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnob6Value(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x13c) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x13c) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x13c) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKnob(0, (int)msg->m_index, 0, (int)msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnob7Value(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x13d) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x13d) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x13d) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKnob(0, (int)msg->m_index, 0, (int)msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetKnob8Value(const CKGCommonParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x13e) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x13e) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x13e) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKnob(0, (int)msg->m_index, 0, (int)msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGCommonParamMsgHandler::SetDTRun(const CKGCommonParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x134);
		unsigned char keep = (unsigned char)~(0x1 << msg->m_index);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << msg->m_index);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x134);
				unsigned char keep = (unsigned char)~(0x1 << msg->m_index);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << msg->m_index);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDTRun((unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

