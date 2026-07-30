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
