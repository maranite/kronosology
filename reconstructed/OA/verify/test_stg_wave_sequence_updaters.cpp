/*
 * test_stg_wave_sequence_updaters.cpp  -  host-side known-answer test
 * for CSTGWaveSequence's round-56 Update*() setter family (solo,
 * 2026-07-29). See include/oa_global.h for the full derivation.
 */
#include <cstdio>
#include <cstring>
#include "oa_global.h"

extern "C" unsigned char STGWaveSeqDataParams[4] = {0};
extern "C" unsigned char sMessageHandlers[4] = {0};
extern "C" unsigned char sValueGetters[4] = {0};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static STGConvertedParam MakeParam(int v)
{
	STGConvertedParam p;
	memset(&p, 0, sizeof(p));
	p.value = v;
	return p;
}

#define P(v) (*({ static STGConvertedParam _p; _p = MakeParam(v); &_p; }))

int main()
{
	check("GetNumParams() == 0x22", CSTGWaveSequence::GetNumParams() == 0x22);
	check("GetParamDescriptors() == STGWaveSeqDataParams",
	      CSTGWaveSequence::GetParamDescriptors() == (const void *)STGWaveSeqDataParams);
	check("GetMessageHandlers() == sMessageHandlers",
	      CSTGWaveSequence::GetMessageHandlers() == (const void *)sMessageHandlers);
	check("GetValueGetters() == sValueGetters",
	      CSTGWaveSequence::GetValueGetters() == (const void *)sValueGetters);

	unsigned char buf[0x200];
	memset(buf, 0, sizeof(buf));
	CSTGWaveSequence *seq = reinterpret_cast<CSTGWaveSequence *>(buf);
	CSTGWaveSeqDataMessageContext ctx;
	memset(&ctx, 0, sizeof(ctx));

	{
		unsigned char dtorBuf[16];
		memset(dtorBuf, 0xcc, sizeof(dtorBuf));
		CSTGWaveSequence *d = reinterpret_cast<CSTGWaveSequence *>(dtorBuf);
		d->~CSTGWaveSequence();
		check("dtor zeroes this+0x0..3", dtorBuf[0] == 0 && dtorBuf[3] == 0);
	}

	/* [1] whole-sequence bitfield family at +0x4 */
	STGConvertedParam vOn = P(1);
	STGConvertedParam vOff = P(0);
	seq->UpdateRunSequence(ctx, vOn);
	check("UpdateRunSequence sets bit0 of +0x4", buf[4] == 1);
	seq->UpdateNoteOnAdvance(ctx, vOn);
	check("UpdateNoteOnAdvance sets bit1 of +0x4 (bit0 preserved)", buf[4] == 3);
	seq->UpdateTimeTempoMode(ctx, vOn);
	check("UpdateTimeTempoMode sets bit2 of +0x4 (bits 0/1 preserved)", buf[4] == 7);
	seq->UpdateRunSequence(ctx, vOff);
	check("UpdateRunSequence(0) clears bit0 only", buf[4] == 6);

	/* [2] whole-sequence plain fixed-offset fields */
	seq->UpdateStartStep(ctx, P(0x11));
	check("UpdateStartStep writes +0x6", buf[6] == 0x11);
	seq->UpdateEndStep(ctx, P(0x22));
	check("UpdateEndStep writes +0x7", buf[7] == 0x22);
	seq->UpdateSoloStep(ctx, P(0x33));
	check("UpdateSoloStep writes +0x8", buf[8] == 0x33);
	seq->UpdateStartStepAMSSource(ctx, P(0x44));
	check("UpdateStartStepAMSSource writes +0x9", buf[9] == 0x44);
	seq->UpdateStartStepAMSIntensity(ctx, P(0x55));
	check("UpdateStartStepAMSIntensity writes +0xa", buf[0xa] == 0x55);
	seq->UpdateDurationAMSIntensity(ctx, P(0x1234));
	check("UpdateDurationAMSIntensity writes short at +0xc",
	      *(short *)(buf + 0xc) == 0x1234);
	seq->UpdatePositionAMSIntensity(ctx, P(0x66));
	check("UpdatePositionAMSIntensity writes +0xf", buf[0xf] == 0x66);
	seq->UpdateLoopStart(ctx, P(0x77));
	check("UpdateLoopStart writes +0x10", buf[0x10] == 0x77);
	seq->UpdateLoopEnd(ctx, P(0x88));
	check("UpdateLoopEnd writes +0x11", buf[0x11] == (unsigned char)0x88);
	seq->UpdateLoopRepeat(ctx, P(0x99));
	check("UpdateLoopRepeat writes +0x12", buf[0x12] == (unsigned char)0x99);
	seq->UpdateLoopDirection(ctx, P(0x1));
	check("UpdateLoopDirection writes +0x13", buf[0x13] == 1);

	/* [3] ctx.index-scaled per-step fields, band 0 and band 2 */
	ctx.index = 0;
	seq->UpdateStepType(ctx, P(5));
	check("UpdateStepType(idx=0) writes +0x42", buf[0x42] == 5);
	seq->UpdateLevel(ctx, P(0x11223344));
	check("UpdateLevel(idx=0) writes int at +0x24",
	      *(int *)(buf + 0x24) == 0x11223344);
	seq->UpdateMultisampleSelect(ctx, P(0x2233));
	check("UpdateMultisampleSelect(idx=0) writes short at +0x3c",
	      *(short *)(buf + 0x3c) == 0x2233);
	seq->UpdateTune(ctx, P(-5));
	check("UpdateTune(idx=0) writes int at +0x28", *(int *)(buf + 0x28) == -5);
	seq->UpdateTranspose(ctx, P(7));
	check("UpdateTranspose(idx=0) writes +0x43", buf[0x43] == 7);
	seq->UpdateStartOffset(ctx, P(9));
	check("UpdateStartOffset(idx=0) writes +0x44", buf[0x44] == 9);
	seq->UpdateAMS1Output(ctx, P(0x5566));
	check("UpdateAMS1Output(idx=0) writes int at +0x2c", *(int *)(buf + 0x2c) == 0x5566);
	seq->UpdateAMS2Output(ctx, P(0x7788));
	check("UpdateAMS2Output(idx=0) writes int at +0x30", *(int *)(buf + 0x30) == 0x7788);
	seq->UpdateTempoBaseNote(ctx, P(3));
	check("UpdateTempoBaseNote(idx=0) writes +0x45", buf[0x45] == 3);
	seq->UpdateTempoMultiplier(ctx, P(4));
	check("UpdateTempoMultiplier(idx=0) writes +0x46", buf[0x46] == 4);
	seq->UpdateReverse(ctx, P(1));
	check("UpdateReverse(idx=0) sets bit0 of +0x47", buf[0x47] == 1);
	seq->UpdateReverse(ctx, P(0));
	check("UpdateReverse(idx=0, 0) clears bit0 of +0x47", buf[0x47] == 0);

	ctx.index = 2;
	seq->UpdateStepType(ctx, P(9));
	check("UpdateStepType(idx=2) writes +0x42+2*0x34", buf[0x42 + 2 * 0x34] == 9);
	check("UpdateStepType(idx=2) leaves idx=0's own record untouched", buf[0x42] == 5);
	seq->UpdateLevel(ctx, P(0x99));
	check("UpdateLevel(idx=2) writes int at +0x24+2*0x34",
	      *(int *)(buf + 0x24 + 2 * 0x34) == 0x99);

	/* [4] IsStereoSequence: real loop over the per-step record array,
	 * gated on this[7] (EndStep, fixed) as the step-count bound and
	 * each record's own +0x42 (StepType)/+0x23 (UUID stereo-flag bit). */
	memset(buf, 0, sizeof(buf));
	buf[7] = 3; /* EndStep == 3: allow up to 3 iterations before bailing */
	/* record 0: StepType != 0 (keep looping) */
	buf[0x42] = 1;
	buf[0x23] = 1; /* stereo-flag bit set (doesn't matter, StepType gates first) */
	/* record 1 (+0x34): StepType == 0 AND stereo-flag bit SET (odd) -> loop
	 * condition (StepType!=0 || (flag&1)==0) goes false -> loop stops, returns true */
	buf[0x42 + 0x34] = 0;
	buf[0x23 + 0x34] = 1;
	check("IsStereoSequence: stops at record 1 (StepType==0, flag odd) -> true",
	      seq->IsStereoSequence() == true);

	memset(buf, 0, sizeof(buf));
	buf[7] = 1; /* EndStep == 1: only 1 iteration allowed before bailing */
	buf[0x42] = 1; /* record 0: keep looping forever (StepType never 0) */
	buf[0x42 + 0x34] = 1;
	buf[0x42 + 2 * 0x34] = 1;
	check("IsStereoSequence: never stops within EndStep bound -> false",
	      seq->IsStereoSequence() == false);

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
