// SPDX-License-Identifier: GPL-2.0
#include "oa_ckg_global_param_msg_handler.h"

/*
 * src/engine/ckg_global_param_handler.cpp  -  CKGGlobalParamMsgHandler
 *
 * See oa_ckg_global_param_msg_handler.h for the full family writeup and
 * every method's own individually-traced shape -- unlike the Module/Common
 * siblings, this class has no shared control-flow skeleton to factor out.
 */

/*
 * (a) plain field write only, nothing else.
 */
void CKGGlobalParamMsgHandler::SetGlobalVelocityCurve(CKGGlobalParamMsg *msg)
{
	m_globalData[0x1] = (unsigned char)msg->m_value;
}

void CKGGlobalParamMsgHandler::SetAfterTouchCurve(CKGGlobalParamMsg *msg)
{
	m_globalData[0x2] = (unsigned char)msg->m_value;
}

void CKGGlobalParamMsgHandler::SetConvertPosition(CKGGlobalParamMsg *msg)
{
	*(int *)(m_globalData + 0x8) = msg->m_value;
}

void CKGGlobalParamMsgHandler::SetControllerCCNo(CKGGlobalParamMsg *msg)
{
	m_globalData[0x10 + msg->m_index] = (unsigned char)msg->m_value;
}

void CKGGlobalParamMsgHandler::SetPadCCNo(CKGGlobalParamMsg *msg)
{
	*(int *)(m_globalData + 0x24 + msg->m_index * 4) = msg->m_value;
}

void CKGGlobalParamMsgHandler::SetExternalPadChannel(CKGGlobalParamMsg *msg)
{
	m_globalData[0x44 + msg->m_index] = (unsigned char)msg->m_value;
}

void CKGGlobalParamMsgHandler::SetExternalPadAssign(CKGGlobalParamMsg *msg)
{
	*(int *)(m_globalData + 0x4c + msg->m_index * 4) = msg->m_value;
}

void CKGGlobalParamMsgHandler::SetExternalPadVelocity(CKGGlobalParamMsg *msg)
{
	m_globalData[0x6c + msg->m_index] = (unsigned char)msg->m_value;
}

/*
 * (c) setne-style plain boolean field write only.
 */
void CKGGlobalParamMsgHandler::SetRecieveExtCommands(CKGGlobalParamMsg *msg)
{
	m_globalData[0x74] = (msg->m_value != 0);
}

void CKGGlobalParamMsgHandler::SetVectorMIDIOut(CKGGlobalParamMsg *msg)
{
	m_globalData[0x75] = (msg->m_value != 0);
}

void CKGGlobalParamMsgHandler::SetPadsMIDIOut(CKGGlobalParamMsg *msg)
{
	m_globalData[0x76] = (msg->m_value != 0);
}

void CKGGlobalParamMsgHandler::SetAutoKarmaProg(CKGGlobalParamMsg *msg)
{
	m_globalData[0x79] = (msg->m_value != 0);
}

void CKGGlobalParamMsgHandler::SetAutoKarmaCombi(CKGGlobalParamMsg *msg)
{
	m_globalData[0x7a] = (msg->m_value != 0);
}

void CKGGlobalParamMsgHandler::SetEnableKarmaModuleToMIDIOut(CKGGlobalParamMsg *msg)
{
	m_globalData[0x7b] = (msg->m_value != 0);
}

void CKGGlobalParamMsgHandler::SetSendStartStopInProgCombi(CKGGlobalParamMsg *msg)
{
	m_globalData[0x7d] = (msg->m_value != 0);
}

/*
 * (b) plain field write, then a real Send/free-function side effect, no
 * suppression check of any kind.
 */
void CKGGlobalParamMsgHandler::SetGlobalKeyTranspose(CKGGlobalParamMsg *msg)
{
	((CKGEngine *)CKGEngine::ms_poInstance)->ResetLocalController();
	m_globalData[0x0] = (unsigned char)msg->m_value;
	CKGEngine::ms_poKGParamEdit->SendGlobalTranspose((signed char)msg->m_value);
}

void CKGGlobalParamMsgHandler::SetMIDIChannel(CKGGlobalParamMsg *msg)
{
	((CKGEngine *)CKGEngine::ms_poInstance)->ResetLocalController();
	m_globalData[0x3] = (unsigned char)msg->m_value;
	CKGEngine::ms_poKGParamEdit->SendGlobalMIDICh((signed char)msg->m_value);
	SPRMain_RenewMIDIChannel();
}

void CKGGlobalParamMsgHandler::SetForceKarmaOff(CKGGlobalParamMsg *msg)
{
	m_globalData[0x77] = (msg->m_value != 0);
	CKGEngine::ms_poKGParamEdit->SendForceKarmaOff((bool)(msg->m_value != 0));
}

void CKGGlobalParamMsgHandler::SetForceDTOff(CKGGlobalParamMsg *msg)
{
	m_globalData[0x78] = (msg->m_value != 0);
	SPRMain_SetAllKARMAAndDrumTrack((bool)(msg->m_value != 0));
}

void CKGGlobalParamMsgHandler::SetProgDrumTrackMIDIOutputChannel(CKGGlobalParamMsg *msg)
{
	unsigned char *p = m_globalData + 0x7e;
	*p = (unsigned char)((*p & ~0xf) | (msg->m_value & 0xf));
	SPRMain_ProcessProgDrumTrackMIDIOutputChannel(msg->m_value);
}

void CKGGlobalParamMsgHandler::SetProgDrumTrackEnableMIDIOutput(CKGGlobalParamMsg *msg)
{
	unsigned char *p = m_globalData + 0x7e;
	*p = (unsigned char)((*p & ~0x10) | ((msg->m_value & 1) << 4));
	SPRMain_ProcessEnableProgDrumTrackMIDIOutput((bool)(msg->m_value != 0));
}

/*
 * Real no-ops -- confirmed, entire body in ground truth is a bare `ret`
 * (1 byte each).
 */
void CKGGlobalParamMsgHandler::SetLocalOn(CKGGlobalParamMsg *msg)
{
}

void CKGGlobalParamMsgHandler::SetNoteReceive(CKGGlobalParamMsg *msg)
{
}

/*
 * SetLocalControllerMIDICh -- no field write at all, just the same
 * 2-call tail SetMIDIChannel makes.
 */
void CKGGlobalParamMsgHandler::SetLocalControllerMIDICh(CKGGlobalParamMsg *msg)
{
	CKGEngine::ms_poKGParamEdit->SendGlobalMIDICh((signed char)msg->m_value);
	SPRMain_RenewMIDIChannel();
}

/*
 * SetMIDIFilter -- real per-bit set/clear on a single byte field keyed
 * by msg->m_index. Ground truth implements the clear side via a real
 * `rol`-of-0xfffffffe idiom, a compiler-generated bitwise-NOT-of-1-shifted
 * equivalent for n<8, not a distinct behavior -- rendered as plain bit
 * ops here,
 * semantically identical.
 */
void CKGGlobalParamMsgHandler::SetMIDIFilter(CKGGlobalParamMsg *msg)
{
	if (msg->m_value != 0)
		m_globalData[0x4] |= (unsigned char)(1 << msg->m_index);
	else
		m_globalData[0x4] &= (unsigned char)~(1 << msg->m_index);
}

/*
 * SetMIDIClockSource -- the one method with a real conditional gate. See
 * header comment for the full trace.
 */
void CKGGlobalParamMsgHandler::SetMIDIClockSource(CKGGlobalParamMsg *msg)
{
	if (msg->m_value != 0) {
		eSTGMidiPort port = CKarmaGlobal::GetMIDIClockPortForSource(
			(ESyncClockSource)msg->m_value);
		if (!SKSTGGate_IsMidiPortAvailable(port))
			return;
	}

	*(int *)(m_globalData + 0xc) = msg->m_value;
	CKGEngine::ms_poKGParamEdit->SendMIDIClockSource((bool)(msg->m_value != 0));
}

/*
 * SetEnableMIDIInToKarmaModule -- no field write at all, just the one
 * real call -- see the header comment for the "Set" vs "Send" naming note.
 */
void CKGGlobalParamMsgHandler::SetEnableMIDIInToKarmaModule(CKGGlobalParamMsg *msg)
{
	CKGEngine::ms_poKGParamEdit->SetEnableMIDIInToKarmaModule((bool)(msg->m_value != 0));
}
