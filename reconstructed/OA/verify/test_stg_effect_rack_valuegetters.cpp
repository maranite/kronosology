// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_effect_rack_valuegetters.cpp  -  KAT for CSTGEffectRack's
 * Get* family -- all 13 real ctx-only candidates (mixed weak/strong
 * linkage), see ../src/engine/stg_effect_rack_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/ctx-index facts the source file's own
 * disassembly-derived translation used -- not by re-using the .cpp
 * file's C output strings -- against the same deterministic non-trivial
 * byte pattern as the rest of the STG value-getter family's KATs:
 * buf[i] = i times 0x9f plus 0x37, all mod 0x100, ctx index fixed at 3
 * for the per-slot methods.
 *
 * GetValueAlgorithm is additionally exercised at ctx.index = 3, 12 and
 * 14 to independently cover all three of ResolveEffectSlotRecord()'s
 * banks (IFX/MFX/TFX), not just the IFX default.
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x1200
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGEffectRack value-getter family known-answer test (13 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(unsigned int *)(g_ctxbuf + 0x4) = 3;

	CSTGEffectRack *s = (CSTGEffectRack *)g_buf;
	CSTGMessageContext &ctx = *(CSTGMessageContext *)g_ctxbuf;

	s->GetValueAlgorithm(ctx);
	check_eq("CSTGEffectRack::GetValueAlgorithm value (idx=3, IFX bank)", CSTGParamsOwner::sValueGetterTemp.value, 214L);
	s->GetValueDModMIDIRouting(ctx);
	check_eq("CSTGEffectRack::GetValueDModMIDIRouting value (idx=3, IFX bank)", CSTGParamsOwner::sValueGetterTemp.value, 117L);

	*(unsigned int *)(g_ctxbuf + 0x4) = 12;
	s->GetValueAlgorithm(ctx);
	check_eq("CSTGEffectRack::GetValueAlgorithm value (idx=12, MFX bank)", CSTGParamsOwner::sValueGetterTemp.value, 238L);

	*(unsigned int *)(g_ctxbuf + 0x4) = 14;
	s->GetValueAlgorithm(ctx);
	check_eq("CSTGEffectRack::GetValueAlgorithm value (idx=14, TFX bank)", CSTGParamsOwner::sValueGetterTemp.value, 182L);

	*(unsigned int *)(g_ctxbuf + 0x4) = 3;

	s->GetValueIFXEffectChainIndex(ctx);
	check_eq("CSTGEffectRack::GetValueIFXEffectChainIndex value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetValueIFXBusIndex(ctx);
	check_eq("CSTGEffectRack::GetValueIFXBusIndex value", CSTGParamsOwner::sValueGetterTemp.value, 35L);
	s->GetValueIFXFXControlBusIndex(ctx);
	check_eq("CSTGEffectRack::GetValueIFXFXControlBusIndex value", CSTGParamsOwner::sValueGetterTemp.value, -62L);
	s->GetValueIFXHDRBusIndex(ctx);
	check_eq("CSTGEffectRack::GetValueIFXHDRBusIndex value", CSTGParamsOwner::sValueGetterTemp.value, 97L);
	s->GetValueIFXPan(ctx);
	check_eq("CSTGEffectRack::GetValueIFXPan value", CSTGParamsOwner::sValueGetterTemp.value, 2094874271L);
	check_eq("CSTGEffectRack::GetValueIFXPan displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2094874271L);
	s->GetValueIFXSend1Level(ctx);
	check_eq("CSTGEffectRack::GetValueIFXSend1Level value", CSTGParamsOwner::sValueGetterTemp.value, -128337381L);
	s->GetValueIFXSend2Level(ctx);
	check_eq("CSTGEffectRack::GetValueIFXSend2Level value", CSTGParamsOwner::sValueGetterTemp.value, 1960130199L);
	s->GetValueMFXReturnLevel(ctx);
	check_eq("CSTGEffectRack::GetValueMFXReturnLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1004173593L);
	s->GetValueMFXChainDirection(ctx);
	check_eq("CSTGEffectRack::GetValueMFXChainDirection value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetValueMFXChainLevel(ctx);
	check_eq("CSTGEffectRack::GetValueMFXChainLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1219037803L);
	s->GetValueMasterVolume(ctx);
	check_eq("CSTGEffectRack::GetValueMasterVolume value", CSTGParamsOwner::sValueGetterTemp.value, -1711645764L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
