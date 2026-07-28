// SPDX-License-Identifier: GPL-2.0
#include "oa_ckg_module_param_msg_handler.h"

/*
 * src/engine/ckg_module_param_handler.cpp  -  CKGModuleParamMsgHandler
 *
 * See oa_ckg_module_param_msg_handler.h for the full family writeup, the
 * shared control-flow skeleton every Shape-B method below follows, and
 * the cross-reference against CKGSeqBackupModuleParam (karma_seq_backup.cpp)
 * that supplied every field offset/shift/mask/stride used here.
 *
 * Static singleton definitions (declared extern via `static` class members
 * in the header) -- own storage lives here, matching every other
 * not-yet-reconstructed singleton in this project (see CKGBankManager::
 * ms_poInstance in src/engine/sk_stg_gate.cpp for the same convention).
 */
unsigned char *CKGEngine::ms_poInstance;
CKGParamEdit *CKGEngine::ms_poKGParamEdit;
unsigned char *CKGEngine::ms_poKGEventDisplayManager;
unsigned char *CKGUIMsgProcessor::ms_poInstance;
CSPRSysExBufManager *CSPRMIDIMsgProcessor::ms_poSysExPlayBuf;

/*
 * ShouldAttemptSysExShadowWrite / SysExShadowWriteIsNeeded, the two
 * byte-identical (register allocation aside) shared blocks factored out of
 * every Shape-B method below (confirmed identical across every
 * independently-checked instance -- see header comment). Not real
 * ground-truth functions of their own; ground truth inlines both into
 * every one of the 85 Shape-B methods separately. Factored here purely to
 * avoid 85x textual duplication -- semantically identical either way.
 *
 * BUG FIX (2026-07-28, found while tracing the deferred 18 methods'
 * SetKnob1Value/SetSw1Value): the range-check sense here was INVERTED from
 * ground truth in the original 113-method batch. Re-derived independently
 * from 3 separate real disassemblies (SetValue @.text+0x3cd650, SetKnob1Value
 * @.text+0x3cf930, SetGenCC @.text+0x3cb1a0) -- all three share the byte-
 * identical `lea eax,[eax-0x8]; cmp eax,0x2; ja <shadow-lookup-block>` shape,
 * and in EVERY case the `ja` (taken when (mode-8) is unsigned > 2, i.e. mode
 * NOT in {8,9,10}) is what jumps INTO the CSPRSysExBufManager::GetValue
 * lookup/shadow-write block; the fall-through case (mode WITHIN {8,9,10})
 * skips straight past the shadow attempt to the CKGEngine suppression check
 * (step 3). The original code had this backwards (`<= 2u`, "mode in
 * {8,9,10}") -- was self-consistent with its own KAT test (which set
 * mode=9 expecting the shadow branch) but not with ground truth. Fixed by
 * flipping the comparison; the KAT test's mode value was updated to match
 * (see verify/test_ckg_module_param_handler.cpp).
 */
bool CKGModuleParamMsgHandler::ShouldAttemptSysExShadowWrite() const
{
	if (CSPREngine::ms_poInstance[0xa] == 0)
		return false;
	if (m_defaultRecordA == 0)
		return false;
	int mode = *(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c);
	if (mode == 4)
		return false;
	return (unsigned int)(mode - 8) > 2u;	/* mode NOT in {8,9,10} */
}

bool CKGModuleParamMsgHandler::SysExShadowWriteIsNeeded(const CKGModuleParamMsg *msg) const
{
	long unused;
	signed char cfg = *(signed char *)(CKGBankManager::ms_poInstance + 0x97c747);
	return CSPRMIDIMsgProcessor::ms_poSysExPlayBuf->GetValue(
		0x6d, cfg, 5, msg->m_deviceIndex, 0, m_moduleIndex, (int)msg->m_index,
		&unused) == 0;
}

/* ================= Shape A: unconditional ctx-indexed writes, no
 * check/notify/shadow of any kind -- stride 10, matches
 * CKGSeqBackupModuleParam's own SetModified* read-side offsets exactly.
 * ================= */

void CKGModuleParamMsgHandler::SetModifiedKnob1(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x296) = (signed char)msg->m_value;
}

void CKGModuleParamMsgHandler::SetModifiedKnob2(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x297) = (signed char)msg->m_value;
}

void CKGModuleParamMsgHandler::SetModifiedKnob3(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x298) = (signed char)msg->m_value;
}

void CKGModuleParamMsgHandler::SetModifiedKnob4(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x299) = (signed char)msg->m_value;
}

void CKGModuleParamMsgHandler::SetModifiedKnob5(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x29a) = (signed char)msg->m_value;
}

void CKGModuleParamMsgHandler::SetModifiedKnob6(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x29b) = (signed char)msg->m_value;
}

void CKGModuleParamMsgHandler::SetModifiedKnob7(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x29c) = (signed char)msg->m_value;
}

void CKGModuleParamMsgHandler::SetModifiedKnob8(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 10 + 0x29d) = (signed char)msg->m_value;
}

void CKGModuleParamMsgHandler::SetModifiedSw1Value(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x294;
	*p = (unsigned char)((*p & ~(1 << 0)) | ((msg->m_value & 1) << 0));
}

void CKGModuleParamMsgHandler::SetModifiedSw2Value(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x294;
	*p = (unsigned char)((*p & ~(1 << 1)) | ((msg->m_value & 1) << 1));
}

void CKGModuleParamMsgHandler::SetModifiedSw3Value(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x294;
	*p = (unsigned char)((*p & ~(1 << 2)) | ((msg->m_value & 1) << 2));
}

void CKGModuleParamMsgHandler::SetModifiedSw4Value(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x294;
	*p = (unsigned char)((*p & ~(1 << 3)) | ((msg->m_value & 1) << 3));
}

void CKGModuleParamMsgHandler::SetModifiedSw5Value(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x294;
	*p = (unsigned char)((*p & ~(1 << 4)) | ((msg->m_value & 1) << 4));
}

void CKGModuleParamMsgHandler::SetModifiedSw6Value(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x294;
	*p = (unsigned char)((*p & ~(1 << 5)) | ((msg->m_value & 1) << 5));
}

void CKGModuleParamMsgHandler::SetModifiedSw7Value(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x294;
	*p = (unsigned char)((*p & ~(1 << 6)) | ((msg->m_value & 1) << 6));
}

void CKGModuleParamMsgHandler::SetModifiedSw8Value(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x294;
	*p = (unsigned char)((*p & ~(1 << 7)) | ((msg->m_value & 1) << 7));
}

void CKGModuleParamMsgHandler::SetModifiedSw1Status(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x295;
	*p = (unsigned char)((*p & ~(1 << 0)) | ((msg->m_value & 1) << 0));
}

void CKGModuleParamMsgHandler::SetModifiedSw2Status(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x295;
	*p = (unsigned char)((*p & ~(1 << 1)) | ((msg->m_value & 1) << 1));
}

void CKGModuleParamMsgHandler::SetModifiedSw3Status(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x295;
	*p = (unsigned char)((*p & ~(1 << 2)) | ((msg->m_value & 1) << 2));
}

void CKGModuleParamMsgHandler::SetModifiedSw4Status(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x295;
	*p = (unsigned char)((*p & ~(1 << 3)) | ((msg->m_value & 1) << 3));
}

void CKGModuleParamMsgHandler::SetModifiedSw5Status(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x295;
	*p = (unsigned char)((*p & ~(1 << 4)) | ((msg->m_value & 1) << 4));
}

void CKGModuleParamMsgHandler::SetModifiedSw6Status(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x295;
	*p = (unsigned char)((*p & ~(1 << 5)) | ((msg->m_value & 1) << 5));
}

void CKGModuleParamMsgHandler::SetModifiedSw7Status(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x295;
	*p = (unsigned char)((*p & ~(1 << 6)) | ((msg->m_value & 1) << 6));
}

void CKGModuleParamMsgHandler::SetModifiedSw8Status(const CKGModuleParamMsg *msg)
{
	unsigned char *p = m_liveRecord + msg->m_index * 10 + 0x295;
	*p = (unsigned char)((*p & ~(1 << 7)) | ((msg->m_value & 1) << 7));
}

/* ================= Shape B: the checked-write skeleton, see header. ================= */

void CKGModuleParamMsgHandler::SetClkAdvCtrig(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x8);
		unsigned char keep = (unsigned char)~(0xf << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x8);
				unsigned char keep = (unsigned char)~(0xf << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendClkAdvTrig((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetClkAdvMode(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x7);
		unsigned char keep = (unsigned char)~(0x3 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x7);
				unsigned char keep = (unsigned char)~(0x3 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendClkAdvMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetClkAdvSize(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x8);
		unsigned char keep = (unsigned char)~(0xf << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x8);
				unsigned char keep = (unsigned char)~(0xf << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendClkAdvSize((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetClkAdvVSence(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0x9) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0x9) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendClkAdvVelSense((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetCollapse(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x7);
		unsigned char keep = (unsigned char)~(0x7 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x7);
				unsigned char keep = (unsigned char)~(0x7 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendForceRange((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetDelayMode(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0xc) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0xc) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDelayMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetDelayTime(const CKGModuleParamMsg *msg)
{
	*(short *)(m_liveRecord + 0xa) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + 0xa) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendDelayTime((unsigned char)msg->m_deviceIndex, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetEnv1Latch(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x10);
		unsigned char keep = (unsigned char)~(0xf << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x10);
				unsigned char keep = (unsigned char)~(0xf << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendEnvLatchMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetEnv1Trig(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x10);
		unsigned char keep = (unsigned char)~(0xf << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x10);
				unsigned char keep = (unsigned char)~(0xf << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendEnvTrigMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetEnv2Latch(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x11);
		unsigned char keep = (unsigned char)~(0xf << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x11);
				unsigned char keep = (unsigned char)~(0xf << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendEnvLatchMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetEnv2Trig(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x11);
		unsigned char keep = (unsigned char)~(0xf << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x11);
				unsigned char keep = (unsigned char)~(0xf << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendEnvTrigMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetEnv3Latch(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x12);
		unsigned char keep = (unsigned char)~(0xf << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x12);
				unsigned char keep = (unsigned char)~(0xf << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendEnvLatchMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetEnv3Trig(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x12);
		unsigned char keep = (unsigned char)~(0xf << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x12);
				unsigned char keep = (unsigned char)~(0xf << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendEnvTrigMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetForceRangeWrap(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0x192) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0x192) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendForceRangeWrap((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetFreezeLoop(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x19);
		unsigned char keep = (unsigned char)~(0x3f << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3f) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x19);
				unsigned char keep = (unsigned char)~(0x3f << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3f) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendFreezeLoop((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetFreezeRetrig(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x19);
		unsigned char keep = (unsigned char)~(0x1 << 7);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x19);
				unsigned char keep = (unsigned char)~(0x1 << 7);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendFreezeLoopRetrig((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetGE(const CKGModuleParamMsg *msg)
{
	*(short *)(m_liveRecord + 0x0) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + 0x0) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetGenCC(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 2 + 0x11e) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 2 + 0x11e) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendCCSource((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetGenCCValue(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 2 + 0x11f) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 2 + 0x11f) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendCCValue((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetInputCh(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x2);
		unsigned char keep = (unsigned char)~(0x1f << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1f) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x2);
				unsigned char keep = (unsigned char)~(0x1f << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1f) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendInputCh((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKIZoneTrans(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + 0x17) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + 0x17) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKbdInTranspose((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKOZoneTrans(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + 0x18) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + 0x18) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKbdOutTranspose((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKbdInZone(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x126);
		unsigned char keep = (unsigned char)~(0x1 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x126);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKeyboardIn((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKbdOutZone(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x126);
		unsigned char keep = (unsigned char)~(0x1 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x126);
				unsigned char keep = (unsigned char)~(0x1 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKeyboardOut((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKeyBottom(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0x6) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0x6) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKeyBottom((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKeyTop(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0x5) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0x5) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendKeyTop((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKnob(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 8 + 0x1e) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 8 + 0x1e) = (signed char)msg->m_value;
			*(signed char *)(m_defaultRecordB + msg->m_index * 8 + 0x1e) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEDestKnob((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKnobForModuleControl(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + msg->m_index * 8 + 0x194) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + msg->m_index * 8 + 0x194) = (signed char)msg->m_value;
			*(signed char *)(m_defaultRecordB + msg->m_index * 8 + 0x194) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEDestKnobForModuleControl((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetKnobName(const CKGModuleParamMsg *msg)
{
	*(unsigned short *)(m_liveRecord + msg->m_index * 2 + 0x138) = (unsigned short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned short *)(m_defaultRecordA + msg->m_index * 2 + 0x138) = (unsigned short)msg->m_value;
			*(unsigned short *)(m_defaultRecordB + msg->m_index * 2 + 0x138) = (unsigned short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetLinkToDT(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x126);
		unsigned char keep = (unsigned char)~(0x1 << 7);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x126);
				unsigned char keep = (unsigned char)~(0x1 << 7);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendLinkToDT((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetMaxValue(const CKGModuleParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 8 + 0x22) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 8 + 0x22) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 8 + 0x22) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEMaxValue((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetMaxValueForModuleControl(const CKGModuleParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 8 + 0x198) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 8 + 0x198) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 8 + 0x198) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEMaxValueForModuleControl((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetMinValue(const CKGModuleParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 8 + 0x20) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 8 + 0x20) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 8 + 0x20) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEMinValue((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetMinValueForModuleControl(const CKGModuleParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 8 + 0x196) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 8 + 0x196) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 8 + 0x196) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEMinValueForModuleControl((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetModCutoff(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0xd);
		unsigned char keep = (unsigned char)~(0x1 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0xd);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendModCutOff((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetModPercent(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0xe) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0xe) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendCutoffPercent((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetNoteLatch(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0xf);
		unsigned char keep = (unsigned char)~(0x1 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0xf);
				unsigned char keep = (unsigned char)~(0x1 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendNoteLatchMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetNoteMap(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0x190) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0x190) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendNoteMap((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetNoteMapChdTrack(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x191);
		unsigned char keep = (unsigned char)~(0x1 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x191);
				unsigned char keep = (unsigned char)~(0x1 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendNoteMapChdTrack((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetNoteMapKbdTrack(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x191);
		unsigned char keep = (unsigned char)~(0x1 << 7);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x191);
				unsigned char keep = (unsigned char)~(0x1 << 7);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendNoteMapKbdTrack((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetNoteMapOnMode(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x191);
		unsigned char keep = (unsigned char)~(0x3 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x191);
				unsigned char keep = (unsigned char)~(0x3 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendNoteMapOnMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetNoteMapTranspose(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + 0x193) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + 0x193) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendNoteMapTranspose((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetNoteTrig(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0xf);
		unsigned char keep = (unsigned char)~(0xf << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0xf);
				unsigned char keep = (unsigned char)~(0xf << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendNoteTrigMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetOutputCh(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + 0x3) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + 0x3) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendOutputCh((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetPolarity(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 8 + 0x1f) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 8 + 0x1f) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 8 + 0x1f) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEPolarity((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetPolarityForModuleControl(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 8 + 0x195) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 8 + 0x195) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 8 + 0x195) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEPolarityForModuleControl((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetQuantize(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x126);
		unsigned char keep = (unsigned char)~(0x1 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x126);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendQuantize((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetQuantizeWindow(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0xf);
		unsigned char keep = (unsigned char)~(0x7 << 5);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 5);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0xf);
				unsigned char keep = (unsigned char)~(0x7 << 5);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x7) << 5);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendQuantizeWindow((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRTCIsLinked(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x2e4);
		unsigned char keep = (unsigned char)~(0x1 << 7);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x2e4);
				unsigned char keep = (unsigned char)~(0x1 << 7);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRTCIsLinked((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRndCluster(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x15);
		unsigned char keep = (unsigned char)~(0x3 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x15);
				unsigned char keep = (unsigned char)~(0x3 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSeedCluster((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRndDrum(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x16);
		unsigned char keep = (unsigned char)~(0x3 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x16);
				unsigned char keep = (unsigned char)~(0x3 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSeedDrum((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRndDuration(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x15);
		unsigned char keep = (unsigned char)~(0x3 << 2);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 2);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x15);
				unsigned char keep = (unsigned char)~(0x3 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 2);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSeedDuration((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRndNote(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x15);
		unsigned char keep = (unsigned char)~(0x3 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x15);
				unsigned char keep = (unsigned char)~(0x3 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSeedNote((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRndPan(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x16);
		unsigned char keep = (unsigned char)~(0x3 << 2);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 2);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x16);
				unsigned char keep = (unsigned char)~(0x3 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 2);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSeedPan((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRndRhythm(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x15);
		unsigned char keep = (unsigned char)~(0x3 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x15);
				unsigned char keep = (unsigned char)~(0x3 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSeedRhythm((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRndVelocity(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x16);
		unsigned char keep = (unsigned char)~(0x3 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x16);
				unsigned char keep = (unsigned char)~(0x3 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSeedVelocity((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRndWaveform(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x16);
		unsigned char keep = (unsigned char)~(0x3 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x16);
				unsigned char keep = (unsigned char)~(0x3 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x3) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSeedWaveform((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRootPosition(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x7);
		unsigned char keep = (unsigned char)~(0x1 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x7);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRootPosition((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRun(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x126);
		unsigned char keep = (unsigned char)~(0x1 << 5);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x126);
				unsigned char keep = (unsigned char)~(0x1 << 5);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRun((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRxAfter(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x14);
		unsigned char keep = (unsigned char)~(0x1 << 1);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x14);
				unsigned char keep = (unsigned char)~(0x1 << 1);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRxAfter((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRxBend(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x14);
		unsigned char keep = (unsigned char)~(0x1 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x14);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRxBend((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRxDamper(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x14);
		unsigned char keep = (unsigned char)~(0x1 << 2);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x14);
				unsigned char keep = (unsigned char)~(0x1 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRxDamper((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRxJSYM(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x14);
		unsigned char keep = (unsigned char)~(0x1 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x14);
				unsigned char keep = (unsigned char)~(0x1 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRxJSYM((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRxJSYP(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x14);
		unsigned char keep = (unsigned char)~(0x1 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x14);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRxJSYP((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRxOther(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x14);
		unsigned char keep = (unsigned char)~(0x1 << 5);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x14);
				unsigned char keep = (unsigned char)~(0x1 << 5);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRxOther((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetRxRibbon(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x14);
		unsigned char keep = (unsigned char)~(0x1 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x14);
				unsigned char keep = (unsigned char)~(0x1 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendRxRibbon((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSceneIsLinked(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x2e4);
		unsigned char keep = (unsigned char)~(0x1 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x2e4);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSceneIsLinked((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSeed(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index + 0x1a) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index + 0x1a) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendStartSeed((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSwName(const CKGModuleParamMsg *msg)
{
	*(unsigned short *)(m_liveRecord + msg->m_index * 2 + 0x138) = (unsigned short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned short *)(m_defaultRecordA + msg->m_index * 2 + 0x138) = (unsigned short)msg->m_value;
			*(unsigned short *)(m_defaultRecordB + msg->m_index * 2 + 0x138) = (unsigned short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTZoneBypass(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x126);
		unsigned char keep = (unsigned char)~(0x1 << 1);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x126);
				unsigned char keep = (unsigned char)~(0x1 << 1);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetThru(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x126);
		unsigned char keep = (unsigned char)~(0x1 << 2);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x126);
				unsigned char keep = (unsigned char)~(0x1 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTimbreThru((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTranspose(const CKGModuleParamMsg *msg)
{
	*(signed char *)(m_liveRecord + 0x4) = (signed char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(signed char *)(m_defaultRecordA + 0x4) = (signed char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTranspose((unsigned char)msg->m_deviceIndex, (char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTrigModule(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0xd);
		unsigned char keep = (unsigned char)~(0xf << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0xd);
				unsigned char keep = (unsigned char)~(0xf << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0xf) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTrigModMode((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTxBend(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x13);
		unsigned char keep = (unsigned char)~(0x1 << 0);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x13);
				unsigned char keep = (unsigned char)~(0x1 << 0);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 0);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTxBend((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTxCCA(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x13);
		unsigned char keep = (unsigned char)~(0x1 << 1);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x13);
				unsigned char keep = (unsigned char)~(0x1 << 1);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 1);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTxCCA((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTxCCB(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x13);
		unsigned char keep = (unsigned char)~(0x1 << 2);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x13);
				unsigned char keep = (unsigned char)~(0x1 << 2);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 2);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTxCCB((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTxEnv1(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x13);
		unsigned char keep = (unsigned char)~(0x1 << 3);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x13);
				unsigned char keep = (unsigned char)~(0x1 << 3);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 3);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTxEnv1((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTxEnv2(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x13);
		unsigned char keep = (unsigned char)~(0x1 << 4);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x13);
				unsigned char keep = (unsigned char)~(0x1 << 4);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 4);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTxEnv2((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTxEnv3(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x13);
		unsigned char keep = (unsigned char)~(0x1 << 5);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x13);
				unsigned char keep = (unsigned char)~(0x1 << 5);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 5);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTxEnv3((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTxNote(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x13);
		unsigned char keep = (unsigned char)~(0x1 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x13);
				unsigned char keep = (unsigned char)~(0x1 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTxNote((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetTxWaveform(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x13);
		unsigned char keep = (unsigned char)~(0x1 << 7);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x13);
				unsigned char keep = (unsigned char)~(0x1 << 7);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 7);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendTxWaveform((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetUseGChAlso(const CKGModuleParamMsg *msg)
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
		CKGEngine::ms_poKGParamEdit->SendUseGChAlso((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetUseNoteOffs(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = (unsigned char *)(m_liveRecord + 0x126);
		unsigned char keep = (unsigned char)~(0x1 << 6);
		*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			{
				unsigned char *p = (unsigned char *)(m_defaultRecordA + 0x126);
				unsigned char keep = (unsigned char)~(0x1 << 6);
				*p = (*p & keep) | ((unsigned char)(msg->m_value & 0x1) << 6);
			}
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendUpdateOnRelease((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetValue(const CKGModuleParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 8 + 0x24) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 8 + 0x24) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 8 + 0x24) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEValue((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetValueForModuleControl(const CKGModuleParamMsg *msg)
{
	*(short *)(m_liveRecord + msg->m_index * 8 + 0x19a) = (short)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(short *)(m_defaultRecordA + msg->m_index * 8 + 0x19a) = (short)msg->m_value;
			*(short *)(m_defaultRecordB + msg->m_index * 8 + 0x19a) = (short)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendGEValueForModuleControl((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_index, (short)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

/* ================= Special-shaped methods ================= */

/*
 * SetSolo -- the one Shape-B-adjacent method with NO CSPREngine/
 * shadow-write gate at all (own disassembly goes straight from the
 * unconditional field-free body into the CKGEngine edit-suppression
 * check); also the only method in this batch that never touches
 * m_liveRecord/m_defaultRecordA/B -- "solo" apparently isn't itself a
 * persisted per-module record field, purely a live Send-plus-Notify.
 */
void CKGModuleParamMsgHandler::SetSolo(const CKGModuleParamMsg *msg)
{
	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		CKGEngine::ms_poKGParamEdit->SendSolo((unsigned char)msg->m_deviceIndex, (unsigned char)msg->m_value);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

/*
 * ShouldStoreToBackup reuses the exact same gate as
 * ShouldAttemptSysExShadowWrite/SysExShadowWriteIsNeeded -- confirmed via
 * direct disassembly comparison: identical CSPREngine/m_defaultRecordA/
 * CKGUIMsgProcessor-mode check, identical CSPRSysExBufManager GetValue
 * call shape -- just returns whether a shadow write WOULD happen instead
 * of actually performing one.
 */
bool CKGModuleParamMsgHandler::ShouldStoreToBackup(const CKGModuleParamMsg *msg)
{
	if (!ShouldAttemptSysExShadowWrite())
		return false;
	if (!SysExShadowWriteIsNeeded(msg))
		return false;
	*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
	return true;
}

/*
 * GetKarmaModule is a real 3-way switch on msg->m_kind -- 1=Program,
 * 2=Seq, anything else including the common case 0=Combi, confirmed via
 * the real disassembly's own `cmp eax,1`/`cmp eax,2`/`test eax,eax`
 * fallthrough order. Sibling of the already-reconstructed
 * CKGSeqBackupModuleParam's own GetKarmaPerfModuleForSeqBackup, same
 * CKGBankManager Get*KarmaPerfModule plus-0x2e8-stride pattern, just
 * switched over 3 bank kinds instead of always Seq. The Program case
 * real-forgoes the plus-index-times-0x2e8 add entirely -- its own
 * disassembly returns CKGBankManager's own result verbatim, confirmed by
 * direct comparison, not a transcription slip.
 */
void *CKGModuleParamMsgHandler::GetKarmaModule(const CKGModuleParamMsg *msg)
{
	CKGBankManager *bankMgr = (CKGBankManager *)CKGBankManager::ms_poInstance;

	if (msg->m_kind == 1) {
		return bankMgr->GetProgKarmaPerfModule((eSTGProgramBankId)msg->m_bankId,
							(unsigned int)msg->m_karmaIndexOrSentinel);
	}
	if (msg->m_kind == 2) {
		unsigned char *p = bankMgr->GetSeqKarmaPerfModule((unsigned int)msg->m_karmaIndexOrSentinel);
		return p + (unsigned int)msg->m_deviceIndex * 0x2e8;
	}
	/* real fallthrough default (every other m_kind value, including the
	 * common case 0): Combi */
	unsigned char *p = bankMgr->GetCombiKarmaPerfModule((eSTGCombiBankId)msg->m_bankId,
							     (unsigned int)msg->m_karmaIndexOrSentinel);
	return p + (unsigned int)msg->m_deviceIndex * 0x2e8;
}

/*
 * GetKarmaPerfModuleForSeqBackup is the Handler-side sibling of
 * CKGSeqBackupModuleParam's own GetKarmaPerfModuleForSeqBackup taking a
 * bare int index -- same CSPREngine gate, same CKGBankManager
 * ms_poInstance-plus-0x97c7d4 sentinel-fallback, same plus-index-times-
 * 0x2e8 shape -- just gated on msg->m_kind being exactly 2, Seq only,
 * and sourcing its own index from msg->m_karmaIndexOrSentinel -- 0xffff
 * is the sentinel -- instead of a bare int argument.
 */
void *CKGModuleParamMsgHandler::GetKarmaPerfModuleForSeqBackup(const CKGModuleParamMsg *msg)
{
	if (CSPREngine::ms_poInstance[0xa] == 0)
		return 0;
	if (msg->m_kind != 2)
		return 0;

	unsigned int index = (unsigned int)msg->m_karmaIndexOrSentinel;
	if (index == 0xffff)
		index = *(unsigned int *)(CKGBankManager::ms_poInstance + 0x97c7d4);

	unsigned char *base = ((CKGBankManager *)CKGBankManager::ms_poInstance)->GetSeqKarmaPerfModule(index);
	if (!base)
		return 0;
	return base + (unsigned int)msg->m_deviceIndex * 0x2e8;
}

/* ================= Shape C/D: RTParm-indirected Knob/Sw value group
 * (batch 2, 2026-07-28). See header comment for the full trace. ================= */

void CKGModuleParamMsgHandler::SetKnob1Value(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x149) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x149) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x149) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
		if (*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) != 1)
			SKSTGGate_NotifyKarmaSliderPosition(0);
	}
}

void CKGModuleParamMsgHandler::SetKnob2Value(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x14a) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x14a) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x14a) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
		if (*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) != 1)
			SKSTGGate_NotifyKarmaSliderPosition(0);
	}
}

void CKGModuleParamMsgHandler::SetKnob3Value(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x14b) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x14b) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x14b) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
		if (*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) != 1)
			SKSTGGate_NotifyKarmaSliderPosition(0);
	}
}

void CKGModuleParamMsgHandler::SetKnob4Value(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x14c) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x14c) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x14c) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
		if (*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) != 1)
			SKSTGGate_NotifyKarmaSliderPosition(0);
	}
}

void CKGModuleParamMsgHandler::SetKnob5Value(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x14d) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x14d) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x14d) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
		if (*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) != 1)
			SKSTGGate_NotifyKarmaSliderPosition(0);
	}
}

void CKGModuleParamMsgHandler::SetKnob6Value(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x14e) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x14e) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x14e) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
		if (*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) != 1)
			SKSTGGate_NotifyKarmaSliderPosition(0);
	}
}

void CKGModuleParamMsgHandler::SetKnob7Value(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x14f) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x14f) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x14f) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
		if (*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) != 1)
			SKSTGGate_NotifyKarmaSliderPosition(0);
	}
}

void CKGModuleParamMsgHandler::SetKnob8Value(const CKGModuleParamMsg *msg)
{
	*(unsigned char *)(m_liveRecord + msg->m_index * 9 + 0x150) = (unsigned char)msg->m_value;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_defaultRecordA + msg->m_index * 9 + 0x150) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + msg->m_index * 9 + 0x150) = (unsigned char)msg->m_value;
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendKnob(selectId, (int)msg->m_index, 0, msg->m_value, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
		if (*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) != 1)
			SKSTGGate_NotifyKarmaSliderPosition(0);
	}
}

void CKGModuleParamMsgHandler::SetSw1Value(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + msg->m_index * 9 + 0x148;
		*p = (unsigned char)((*p & ~(1u << 0)) | (unsigned char)(msg->m_value << 0));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			unsigned char *pA = m_defaultRecordA + msg->m_index * 9 + 0x148;
			*pA = (unsigned char)((*pA & ~(1u << 0)) | (unsigned char)(msg->m_value << 0));
			unsigned char *pB = m_defaultRecordB + msg->m_index * 9 + 0x148;
			*pB = (unsigned char)((*pB & ~(1u << 0)) | (unsigned char)(msg->m_value << 0));
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(selectId, (int)msg->m_index, 0, msg->m_value != 0, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSw2Value(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + msg->m_index * 9 + 0x148;
		*p = (unsigned char)((*p & ~(1u << 1)) | (unsigned char)(msg->m_value << 1));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			unsigned char *pA = m_defaultRecordA + msg->m_index * 9 + 0x148;
			*pA = (unsigned char)((*pA & ~(1u << 1)) | (unsigned char)(msg->m_value << 1));
			unsigned char *pB = m_defaultRecordB + msg->m_index * 9 + 0x148;
			*pB = (unsigned char)((*pB & ~(1u << 1)) | (unsigned char)(msg->m_value << 1));
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(selectId, (int)msg->m_index, 0, msg->m_value != 0, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSw3Value(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + msg->m_index * 9 + 0x148;
		*p = (unsigned char)((*p & ~(1u << 2)) | (unsigned char)(msg->m_value << 2));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			unsigned char *pA = m_defaultRecordA + msg->m_index * 9 + 0x148;
			*pA = (unsigned char)((*pA & ~(1u << 2)) | (unsigned char)(msg->m_value << 2));
			unsigned char *pB = m_defaultRecordB + msg->m_index * 9 + 0x148;
			*pB = (unsigned char)((*pB & ~(1u << 2)) | (unsigned char)(msg->m_value << 2));
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(selectId, (int)msg->m_index, 0, msg->m_value != 0, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSw4Value(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + msg->m_index * 9 + 0x148;
		*p = (unsigned char)((*p & ~(1u << 3)) | (unsigned char)(msg->m_value << 3));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			unsigned char *pA = m_defaultRecordA + msg->m_index * 9 + 0x148;
			*pA = (unsigned char)((*pA & ~(1u << 3)) | (unsigned char)(msg->m_value << 3));
			unsigned char *pB = m_defaultRecordB + msg->m_index * 9 + 0x148;
			*pB = (unsigned char)((*pB & ~(1u << 3)) | (unsigned char)(msg->m_value << 3));
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(selectId, (int)msg->m_index, 0, msg->m_value != 0, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSw5Value(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + msg->m_index * 9 + 0x148;
		*p = (unsigned char)((*p & ~(1u << 4)) | (unsigned char)(msg->m_value << 4));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			unsigned char *pA = m_defaultRecordA + msg->m_index * 9 + 0x148;
			*pA = (unsigned char)((*pA & ~(1u << 4)) | (unsigned char)(msg->m_value << 4));
			unsigned char *pB = m_defaultRecordB + msg->m_index * 9 + 0x148;
			*pB = (unsigned char)((*pB & ~(1u << 4)) | (unsigned char)(msg->m_value << 4));
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(selectId, (int)msg->m_index, 0, msg->m_value != 0, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSw6Value(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + msg->m_index * 9 + 0x148;
		*p = (unsigned char)((*p & ~(1u << 5)) | (unsigned char)(msg->m_value << 5));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			unsigned char *pA = m_defaultRecordA + msg->m_index * 9 + 0x148;
			*pA = (unsigned char)((*pA & ~(1u << 5)) | (unsigned char)(msg->m_value << 5));
			unsigned char *pB = m_defaultRecordB + msg->m_index * 9 + 0x148;
			*pB = (unsigned char)((*pB & ~(1u << 5)) | (unsigned char)(msg->m_value << 5));
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(selectId, (int)msg->m_index, 0, msg->m_value != 0, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSw7Value(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + msg->m_index * 9 + 0x148;
		*p = (unsigned char)((*p & ~(1u << 6)) | (unsigned char)(msg->m_value << 6));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			unsigned char *pA = m_defaultRecordA + msg->m_index * 9 + 0x148;
			*pA = (unsigned char)((*pA & ~(1u << 6)) | (unsigned char)(msg->m_value << 6));
			unsigned char *pB = m_defaultRecordB + msg->m_index * 9 + 0x148;
			*pB = (unsigned char)((*pB & ~(1u << 6)) | (unsigned char)(msg->m_value << 6));
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(selectId, (int)msg->m_index, 0, msg->m_value != 0, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

void CKGModuleParamMsgHandler::SetSw8Value(const CKGModuleParamMsg *msg)
{
	{
		unsigned char *p = m_liveRecord + msg->m_index * 9 + 0x148;
		*p = (unsigned char)((*p & ~(1u << 7)) | (unsigned char)(msg->m_value << 7));
	}

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			unsigned char *pA = m_defaultRecordA + msg->m_index * 9 + 0x148;
			*pA = (unsigned char)((*pA & ~(1u << 7)) | (unsigned char)(msg->m_value << 7));
			unsigned char *pB = m_defaultRecordB + msg->m_index * 9 + 0x148;
			*pB = (unsigned char)((*pB & ~(1u << 7)) | (unsigned char)(msg->m_value << 7));
		}
	}

	if (CKGEngine::ms_poInstance[0xb0] == 0) {
		int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
		CKGEngine::ms_poKGParamEdit->SendAssignableSwitch(selectId, (int)msg->m_index, 0, msg->m_value != 0, false);
		((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
	}
}

/* ================= Real idx-dependent outliers: SetScene / SetLinkedSceneId
 * (batch 2, 2026-07-28). See header comment for the full trace -- both
 * deviate from the Shape-B skeleton's usual step ordering. ================= */

void CKGModuleParamMsgHandler::SetScene(const CKGModuleParamMsg *msg)
{
	bool shadowWriteHappened = false;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			shadowWriteHappened = true;
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
			*(unsigned char *)(m_liveRecord + 0x127) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordA + 0x127) = (unsigned char)msg->m_value;
			*(unsigned char *)(m_defaultRecordB + 0x127) = (unsigned char)msg->m_value;
		}
	}
	if (!shadowWriteHappened)
		*(unsigned char *)(m_liveRecord + 0x127) = (unsigned char)msg->m_value;

	/* unlike every Shape-B method, suppression gates EVERYTHING past the
	 * primary write above -- no LinkedSceneId-array sync, no Send, no
	 * Notify when suppressed */
	if (CKGEngine::ms_poInstance[0xb0] != 0)
		return;

	{
		unsigned char curSceneIdx = *(unsigned char *)(*(unsigned char **)CKGBankManager::ms_poInstance + 0x135);
		unsigned char value7 = (unsigned char)msg->m_value & 0x7;
		unsigned char *p = m_liveRecord + 0x2e4 + (curSceneIdx >> 1);
		if (!(curSceneIdx & 1))
			*p = (unsigned char)((*p & 0xf8) | value7);
		else
			*p = (unsigned char)((*p & 0x8f) | (value7 << 4));
	}

	if (shadowWriteHappened) {
		unsigned char curSceneIdx = *(unsigned char *)(*(unsigned char **)CKGBankManager::ms_poInstance + 0x135);
		unsigned char value7 = (unsigned char)msg->m_value & 0x7;

		unsigned char *pA = m_defaultRecordA + 0x2e4 + (curSceneIdx >> 1);
		if (!(curSceneIdx & 1))
			*pA = (unsigned char)((*pA & 0xf8) | value7);
		else
			*pA = (unsigned char)((*pA & 0x8f) | (value7 << 4));

		unsigned char *pB = m_defaultRecordB + 0x2e4 + (curSceneIdx >> 1);
		if (!(curSceneIdx & 1))
			*pB = (unsigned char)((*pB & 0xf8) | value7);
		else
			*pB = (unsigned char)((*pB & 0x8f) | (value7 << 4));
	}

	if (m_pendingSceneSendGuard != 0)
		return;

	int selectId = CKGEngine::ms_poKGParamEdit->GetRTParmBufferSelectId(msg->m_deviceIndex);
	CKGEngine::ms_poKGParamEdit->SendScene(selectId, (unsigned char)msg->m_value, false);
	((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
}

void CKGModuleParamMsgHandler::SetLinkedSceneId(const CKGModuleParamMsg *msg)
{
	bool shadowWriteHappened = false;

	if (ShouldAttemptSysExShadowWrite()) {
		if (SysExShadowWriteIsNeeded(msg)) {
			shadowWriteHappened = true;
			*(unsigned char *)(CKGUIMsgProcessor::ms_poInstance + 0x74) = 1;
		}
	}

	/* real per-scene packed-nibble write (matches CKGSeqBackupModuleParam::
	 * SetLinkedSceneId's own read-side formula exactly) -- unconditional
	 * against m_liveRecord regardless of suppression below; mirrored to
	 * both shadow records only when the SysEx lookup above missed */
	{
		int idx = (int)msg->m_index;
		int byteIdx = idx / 2;	/* real signed-division-by-2 idiom in ground truth */
		unsigned char value7 = (unsigned char)msg->m_value & 0x7;

		unsigned char *p = m_liveRecord + 0x2e4 + byteIdx;
		if (!(idx & 1))
			*p = (unsigned char)((*p & 0xf8) | value7);
		else
			*p = (unsigned char)((*p & 0x8f) | (value7 << 4));

		if (shadowWriteHappened) {
			unsigned char *pA = m_defaultRecordA + 0x2e4 + byteIdx;
			if (!(idx & 1))
				*pA = (unsigned char)((*pA & 0xf8) | value7);
			else
				*pA = (unsigned char)((*pA & 0x8f) | (value7 << 4));

			unsigned char *pB = m_defaultRecordB + 0x2e4 + byteIdx;
			if (!(idx & 1))
				*pB = (unsigned char)((*pB & 0xf8) | value7);
			else
				*pB = (unsigned char)((*pB & 0x8f) | (value7 << 4));
		}
	}

	/* unlike the packed-nibble write above, the simple 0x127 mirror field
	 * (same offset SetScene owns) and the Send/Notify pair ARE gated on
	 * suppression, same as every Shape-B method */
	if (CKGEngine::ms_poInstance[0xb0] != 0)
		return;

	*(unsigned char *)(m_liveRecord + 0x127) = (unsigned char)msg->m_value;
	if (shadowWriteHappened) {
		*(unsigned char *)(m_defaultRecordA + 0x127) = (unsigned char)msg->m_value;
		*(unsigned char *)(m_defaultRecordB + 0x127) = (unsigned char)msg->m_value;
	}

	CKGEngine::ms_poKGParamEdit->SendLinkedSceneID((unsigned char)msg->m_deviceIndex,
							(unsigned char)msg->m_index,
							(unsigned char)msg->m_value);
	((CKGUIMsgProcessor *)CKGUIMsgProcessor::ms_poInstance)->NotifyAfterEdit();
}
