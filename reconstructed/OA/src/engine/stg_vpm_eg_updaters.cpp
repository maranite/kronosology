// SPDX-License-Identifier: GPL-2.0
/*
 * stg_vpm_eg_updaters.cpp  -  CSTGVPMEG's Update*(CSTGPatchMessageContext&,
 * STGConvertedParam&) setter family (round 75, solo). Every field offset
 * here is a cross-check against the already-confirmed sibling `Get*` family
 * (stg_vpm_eg_valuegetters.cpp), not a fresh derivation -- see
 * include/oa_stg_vpm_eg.h's own header comment for the full field-shape
 * summary this mirrors.
 *
 * Dialect: plain single-write field setters, no display predicate and no
 * cross-voice propagation (unlike e.g. CSTGADSRBase's own Update* family,
 * adsr_base.cpp) -- confirmed straight from each method's own ground-truth
 * decompile, no ambiguity.
 */
#include "oa_stg_vpm_eg.h"

void CSTGVPMEG::UpdateAMS1LevelModSource(CSTGPatchMessageContext &, STGConvertedParam &newVal)
{
	unsigned char *base = (unsigned char *)this;
	*(signed char *)(base + 0x3e) = (signed char)newVal.value;
}

void CSTGVPMEG::UpdateAMS1LevelModIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + CtxIndex(ctx, 0x4, 4) + 0x3f) = newVal.value;
}

void CSTGVPMEG::UpdateAMS1TimeModSource(CSTGPatchMessageContext &, STGConvertedParam &newVal)
{
	unsigned char *base = (unsigned char *)this;
	*(signed char *)(base + 0x2d) = (signed char)newVal.value;
}

void CSTGVPMEG::UpdateAMS1TimeModIntensity(CSTGPatchMessageContext &ctx, STGConvertedParam &newVal)
{
	unsigned char *base = (unsigned char *)this;
	*(int *)(base + CtxIndex(ctx, 0x4, 4) + 0x2e) = newVal.value;
}

void CSTGVPMEG::UpdateTriggerAtNoteOn(CSTGPatchMessageContext &, STGConvertedParam &newVal)
{
	unsigned char *base = (unsigned char *)this;
	unsigned char *flags = base + 0x4f;
	*flags = (*flags & 0xfe) | (newVal.value != 0);
}

/* .text+0x5b89f0..0x5b8a80, round 76 -- boilerplate registration
 * accessors + 3 more real AMS accessors, see header comment. */

extern "C" unsigned char STGVPMEGParams[544];
extern "C" unsigned char _ZN9CSTGVPMEG16sMessageHandlersE[96];
extern "C" unsigned char _ZN9CSTGVPMEG13sValueGettersE[96];

int CSTGVPMEG::GetId() { return 0x1d; }
int CSTGVPMEG::GetNumParams() { return 10; }
const void *CSTGVPMEG::GetParamDescriptors() { return STGVPMEGParams; }
const void *CSTGVPMEG::GetMessageHandlers() { return _ZN9CSTGVPMEG16sMessageHandlersE; }
const void *CSTGVPMEG::GetValueGetters() { return _ZN9CSTGVPMEG13sValueGettersE; }

bool CSTGVPMEG::TriggersAtNoteOn(int)
{
	unsigned char *base = (unsigned char *)this;
	return (base[0x4f] & 1) != 0;
}

bool CSTGVPMEG::StateHasLevelAMS(unsigned char index)
{
	return index < 4;
}

int CSTGVPMEG::GetAMSLevelModSource(unsigned char)
{
	unsigned char *base = (unsigned char *)this;
	return (signed char)base[0x3e];
}

int CSTGVPMEG::GetAMSTimeModSource(unsigned char)
{
	unsigned char *base = (unsigned char *)this;
	return (signed char)base[0x2d];
}

/* .text+0x5b8a80, round 78. `unused` is EDX, confirmed dead in the real
 * body (only `this`/EAX and `index`/ECX are read). Real return is a
 * plain float loaded via `flds`; see header comment. */
float CSTGVPMEG::GetAMSLevelModIntensity(unsigned char, unsigned char index)
{
	unsigned char *base = (unsigned char *)this;
	return *(float *)(base + 0x3f + (unsigned)index * 4);
}

/* .text+0x5b8aa0, round 78 -- same shape, AMS1TimeModIntensity field. */
float CSTGVPMEG::GetAMSTimeModIntensity(unsigned char, unsigned char index)
{
	unsigned char *base = (unsigned char *)this;
	return *(float *)(base + 0x2e + (unsigned)index * 4);
}

/* .text+0x5b8d20 (D1) / 0x5b8d30 (D0), round 77 -- both variants
 * byte-identical, see header comment. */
CSTGVPMEG::~CSTGVPMEG()
{
	unsigned char *base = (unsigned char *)this;
	base[0] = base[1] = base[2] = base[3] = 0;
}
