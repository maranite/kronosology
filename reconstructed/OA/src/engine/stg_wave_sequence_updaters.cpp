// SPDX-License-Identifier: GPL-2.0
/*
 * stg_wave_sequence_updaters.cpp  -  CSTGWaveSequence's
 * Update*(CSTGWaveSeqDataMessageContext&, STGConvertedParam&) setter
 * family (round 56, solo). See include/oa_global.h's own header comment
 * above `struct CSTGWaveSequence` for the full derivation -- every
 * offset here is a cross-check against the already-confirmed sibling
 * `Getter*` family (stg_wave_sequence_valuegetters.cpp), not a fresh
 * derivation.
 */
#include "oa_global.h"

extern "C" unsigned char STGWaveSeqDataParams[];
extern "C" unsigned char sMessageHandlers[];
extern "C" unsigned char sValueGetters[];

CSTGWaveSequence::~CSTGWaveSequence()
{
	/* Real dtor (both D2/D0 variants, identical): resets vptr to
	 * &PTR__CSTGParamsOwner_006c04a8 -- see header comment. Same
	 * "opaque placeholder, no real vtable pointer needed" treatment
	 * as CSTGKeyTrack/CSTGPatch/CSTGMultibandDelay/
	 * CSTGProgramModeDrumTrackSlot. */
	unsigned char *base = (unsigned char *)this;
	base[0] = base[1] = base[2] = base[3] = 0;
}

unsigned int CSTGWaveSequence::GetNumParams() { return 0x22; }
const void *CSTGWaveSequence::GetParamDescriptors() { return STGWaveSeqDataParams; }
const void *CSTGWaveSequence::GetMessageHandlers() { return sMessageHandlers; }
const void *CSTGWaveSequence::GetValueGetters() { return sValueGetters; }

bool CSTGWaveSequence::IsStereoSequence() const
{
	unsigned char *rec = (unsigned char *)this;
	int count = 0;
	while (*(signed char *)(rec + 0x42) != 0 || (*(unsigned char *)(rec + 0x23) & 1) == 0) {
		count++;
		rec += 0x34;
		if (count > *(unsigned char *)(((unsigned char *)this) + 7))
			return false;
	}
	return true;
}

void CSTGWaveSequence::UpdateRunSequence(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	base[4] = (unsigned char)((base[4] & 0xfe) | (val.value != 0));
}

void CSTGWaveSequence::UpdateNoteOnAdvance(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	base[4] = (unsigned char)((base[4] & 0xfd) | ((val.value != 0) << 1));
}

void CSTGWaveSequence::UpdateTimeTempoMode(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	base[4] = (unsigned char)((base[4] & 0xfb) | ((val.value != 0) << 2));
}

void CSTGWaveSequence::UpdateStartStep(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[6] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateEndStep(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[7] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateSoloStep(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[8] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateStartStepAMSSource(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[9] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateStartStepAMSIntensity(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0xa] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateDurationAMSIntensity(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	*(short *)(((unsigned char *)this) + 0xc) = (short)val.value;
}

void CSTGWaveSequence::UpdatePositionAMSIntensity(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0xf] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateLoopStart(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x10] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateLoopEnd(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x11] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateLoopRepeat(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x12] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateLoopDirection(CSTGWaveSeqDataMessageContext &, STGConvertedParam &val)
{
	((unsigned char *)this)[0x13] = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateStepType(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned char *)(base + ctx.index * 0x34 + 0x42) = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateLevel(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + ctx.index * 0x34 + 0x24) = val.value;
}

void CSTGWaveSequence::UpdateMultisampleSelect(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(short *)(base + ctx.index * 0x34 + 0x3c) = (short)val.value;
}

void CSTGWaveSequence::UpdateTune(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + ctx.index * 0x34 + 0x28) = val.value;
}

void CSTGWaveSequence::UpdateTranspose(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned char *)(base + ctx.index * 0x34 + 0x43) = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateStartOffset(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned char *)(base + ctx.index * 0x34 + 0x44) = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateAMS1Output(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + ctx.index * 0x34 + 0x2c) = val.value;
}

void CSTGWaveSequence::UpdateAMS2Output(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + ctx.index * 0x34 + 0x30) = val.value;
}

void CSTGWaveSequence::UpdateTempoBaseNote(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned char *)(base + ctx.index * 0x34 + 0x45) = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateTempoMultiplier(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	*(unsigned char *)(base + ctx.index * 0x34 + 0x46) = (unsigned char)val.value;
}

void CSTGWaveSequence::UpdateReverse(CSTGWaveSeqDataMessageContext &ctx, STGConvertedParam &val)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *field = base + ctx.index * 0x34 + 0x47;
	*field = (unsigned char)((*field & 0xfe) | (val.value != 0));
}
