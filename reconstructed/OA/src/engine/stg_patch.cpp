// SPDX-License-Identifier: GPL-2.0
/*
 * stg_patch.cpp  -  CSTGPatch method bodies (round 53, solo). See
 * include/oa_stg_patch.h for the full object-layout derivation and the
 * deliberately-deferred methods' 3 distinct reasons.
 */
#include "oa_stg_patch.h"

CSTGPatch::~CSTGPatch()
{
	/* Real dtor (both D2/D0 variants, identical): resets vptr to
	 * &PTR__CSTGParamsOwner_006c04a8 -- see header comment. Same
	 * "opaque placeholder, no real vtable pointer needed" treatment
	 * as CSTGKeyTrack::~CSTGKeyTrack(). */
	mVptrPlaceholder[0] = mVptrPlaceholder[1] = mVptrPlaceholder[2] = mVptrPlaceholder[3] = 0;
}

unsigned int CSTGPatch::GetNumLFOs() { return 0; }
unsigned int CSTGPatch::GetLFO() { return 0; }
unsigned int CSTGPatch::GetNumEGs() { return 0; }
unsigned int CSTGPatch::GetEG() { return 0; }
unsigned int CSTGPatch::GetEGRemapping() { return 0xffffffff; }

unsigned int CSTGPatch::WillHandleNoteOn() { return 0; }
void CSTGPatch::NotifyNoteOff() {}
void CSTGPatch::NotifyAllNotesOff() {}
void CSTGPatch::NotifyKeyReleased() {}

void CSTGPatch::ProcessStaticFront() {}
void CSTGPatch::ProcessStaticBack() {}
void CSTGPatch::ProcessFeedback() {}

void CSTGPatch::UpdateSlotPortamento() {}
void CSTGPatch::UpdateUnisonTuning() {}
unsigned int CSTGPatch::GetNumStaticAllocatedQuads() { return 0; }
unsigned int CSTGPatch::IsAllPortamentoOff() { return 1; }
unsigned int CSTGPatch::WillHandleUnaCorda() { return 0; }
unsigned int CSTGPatch::ShouldHold() { return 0; }
void CSTGPatch::WaveSequenceVoiceInit() {}
unsigned int CSTGPatch::GetMaxWaveSeqSwingResolution() { return 6; }
void CSTGPatch::UpdateWaveSeqSwingResolution() {}
unsigned int CSTGPatch::HasWaveSeqInOscZone() { return 0; }
unsigned int CSTGPatch::GetWaveSeqIdInOscZone() { return 0; }
void CSTGPatch::OverrideWaveform() {}
void CSTGPatch::ResetWaveform() {}
unsigned int CSTGPatch::GetExclusiveGroupForNote() { return 0; }
void CSTGPatch::ApplyRestrikeLevelScaling() {}
unsigned int CSTGPatch::GetRestrikeLimitForNote() { return 0; }
void CSTGPatch::SetOutputLevelMultiplier() {}
void CSTGPatch::SetDModValues() {}
void CSTGPatch::ResetDMod() {}

bool CSTGPatch::CheckMatchingToneAdjustTargetParam(const unsigned char *descriptor,
						    char c1, char c2, int v1, int v2)
{
	if (*(const char *)(descriptor + 6) == c1 && *(const char *)(descriptor + 7) == c2 &&
	    *(const int *)(descriptor + 8) == v1)
		return *(const int *)(descriptor + 0xc) == v2;
	return false;
}

void *CSTGPatch::GetDefaultContext()
{
	/* Real ground truth guards the vtable-slot stamp with a byte that
	 * decompiles as sharing storage with this same field's own low
	 * byte (a decompiler symbol-overlap artifact, see header comment)
	 * -- reconstructed with a real, separate C++ static-init guard
	 * instead, which reproduces the INTENDED one-time-stamp behavior. */
	static unsigned char s_defaultContext[0x31];
	static bool s_inited = false;
	if (!s_inited) {
		/* Real value is &PTR_IsLiveUpdate_006bf728, a genuinely
		 * unmodeled external data symbol -- opaque non-null
		 * sentinel here, never dereferenced by any reconstructed
		 * caller. See header comment. */
		*(void **)s_defaultContext = (void *)(unsigned long)0x1;
		s_inited = true;
	}
	*(unsigned int *)(s_defaultContext + 4) = 0;
	*(unsigned int *)(s_defaultContext + 8) = 1;
	*(unsigned int *)(s_defaultContext + 0xc) = 4;
	*(unsigned int *)(s_defaultContext + 0x10) = 0;
	s_defaultContext[0x14] = 0;
	s_defaultContext[0x15] = 1;
	s_defaultContext[0x16] = 1;
	s_defaultContext[0x17] = 0;
	*(unsigned int *)(s_defaultContext + 0x18) = 0;
	*(unsigned int *)(s_defaultContext + 0x1c) = 0xffffffff;
	*(unsigned int *)(s_defaultContext + 0x20) = 0;
	*(unsigned int *)(s_defaultContext + 0x24) = 0;
	*(unsigned int *)(s_defaultContext + 0x28) = 0;
	*(unsigned int *)(s_defaultContext + 0x2c) = 0;
	s_defaultContext[0x30] &= 0xfe;
	return s_defaultContext;
}
