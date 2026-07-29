/*
 * test_stg_patch.cpp  -  host-side known-answer test for CSTGPatch's 34
 * real methods landed in round 53 (solo, 2026-07-29). See
 * include/oa_stg_patch.h for the full derivation.
 */
#include <cstdio>
#include <cstring>
#include "oa_stg_patch.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	/* [1] LFO/EG default-override family */
	check("GetNumLFOs() == 0", CSTGPatch::GetNumLFOs() == 0);
	check("GetLFO() == 0", CSTGPatch::GetLFO() == 0);
	check("GetNumEGs() == 0", CSTGPatch::GetNumEGs() == 0);
	check("GetEG() == 0", CSTGPatch::GetEG() == 0);
	check("GetEGRemapping() == 0xffffffff", CSTGPatch::GetEGRemapping() == 0xffffffffu);

	/* [2] note-on/off notification family */
	check("WillHandleNoteOn() == 0", CSTGPatch::WillHandleNoteOn() == 0);
	CSTGPatch::NotifyNoteOff();
	CSTGPatch::NotifyAllNotesOff();
	CSTGPatch::NotifyKeyReleased();
	check("NotifyNoteOff/AllNotesOff/KeyReleased: no-op, reached here", true);

	/* [3] static/feedback processing */
	CSTGPatch::ProcessStaticFront();
	CSTGPatch::ProcessStaticBack();
	CSTGPatch::ProcessFeedback();
	check("ProcessStaticFront/Back/Feedback: no-op, reached here", true);

	/* [4] portamento/unison/wave-seq/misc default-override family */
	CSTGPatch::UpdateSlotPortamento();
	CSTGPatch::UpdateUnisonTuning();
	check("GetNumStaticAllocatedQuads() == 0", CSTGPatch::GetNumStaticAllocatedQuads() == 0);
	check("IsAllPortamentoOff() == 1", CSTGPatch::IsAllPortamentoOff() == 1);
	check("WillHandleUnaCorda() == 0", CSTGPatch::WillHandleUnaCorda() == 0);
	check("ShouldHold() == 0", CSTGPatch::ShouldHold() == 0);
	CSTGPatch::WaveSequenceVoiceInit();
	check("GetMaxWaveSeqSwingResolution() == 6", CSTGPatch::GetMaxWaveSeqSwingResolution() == 6);
	CSTGPatch::UpdateWaveSeqSwingResolution();
	check("HasWaveSeqInOscZone() == 0", CSTGPatch::HasWaveSeqInOscZone() == 0);
	check("GetWaveSeqIdInOscZone() == 0", CSTGPatch::GetWaveSeqIdInOscZone() == 0);
	CSTGPatch::OverrideWaveform();
	CSTGPatch::ResetWaveform();
	check("GetExclusiveGroupForNote() == 0", CSTGPatch::GetExclusiveGroupForNote() == 0);
	CSTGPatch::ApplyRestrikeLevelScaling();
	check("GetRestrikeLimitForNote() == 0", CSTGPatch::GetRestrikeLimitForNote() == 0);
	CSTGPatch::SetOutputLevelMultiplier();
	CSTGPatch::SetDModValues();
	CSTGPatch::ResetDMod();

	/* [5] CheckMatchingToneAdjustTargetParam: 4-field descriptor compare */
	unsigned char descriptor[0x10];
	memset(descriptor, 0, sizeof(descriptor));
	descriptor[6] = 'A';
	descriptor[7] = 'B';
	*(int *)(descriptor + 8) = 100;
	*(int *)(descriptor + 0xc) = 200;
	check("CheckMatchingToneAdjustTargetParam: all 4 fields match",
	      CSTGPatch::CheckMatchingToneAdjustTargetParam(descriptor, 'A', 'B', 100, 200));
	check("CheckMatchingToneAdjustTargetParam: c1 mismatch short-circuits",
	      !CSTGPatch::CheckMatchingToneAdjustTargetParam(descriptor, 'X', 'B', 100, 200));
	check("CheckMatchingToneAdjustTargetParam: c2 mismatch short-circuits",
	      !CSTGPatch::CheckMatchingToneAdjustTargetParam(descriptor, 'A', 'X', 100, 200));
	check("CheckMatchingToneAdjustTargetParam: v1 mismatch short-circuits",
	      !CSTGPatch::CheckMatchingToneAdjustTargetParam(descriptor, 'A', 'B', 999, 200));
	check("CheckMatchingToneAdjustTargetParam: v1/v2/c1/c2 all match but v2 differs",
	      !CSTGPatch::CheckMatchingToneAdjustTargetParam(descriptor, 'A', 'B', 100, 999));

	/* [6] GetDefaultContext: same pointer every call, fields reset each call */
	void *ctx1 = CSTGPatch::GetDefaultContext();
	unsigned char *c = (unsigned char *)ctx1;
	check("GetDefaultContext: +0x8 == 1", *(unsigned int *)(c + 8) == 1);
	check("GetDefaultContext: +0xc == 4", *(unsigned int *)(c + 0xc) == 4);
	check("GetDefaultContext: +0x1c == 0xffffffff", *(unsigned int *)(c + 0x1c) == 0xffffffffu);
	check("GetDefaultContext: +0x15/+0x16 == 1", c[0x15] == 1 && c[0x16] == 1);
	/* mutate a field, call again, confirm it's unconditionally reset */
	*(unsigned int *)(c + 8) = 0xdead;
	void *ctx2 = CSTGPatch::GetDefaultContext();
	check("GetDefaultContext: same static instance across calls", ctx1 == ctx2);
	check("GetDefaultContext: mutated field reset to 1 on 2nd call", *(unsigned int *)(c + 8) == 1);

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
