// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_vpm_eg_valuegetters.cpp  -  KAT for CSTGVPMEG's Get* family --
 * all 5 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_vpm_eg_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/ctx-index facts the source
 * file's own decoder used -- not by re-using the .cpp file's C output
 * strings -- against the same deterministic non-trivial byte pattern as
 * the rest of the STG value-getter family's KATs: buf[i] = i times 0x9f
 * plus 0x37, all mod 0x100. ctx's own dynamic-index field at +0x4 is
 * fixed at 3, matching the established KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_vpm_eg.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

/* Host-link-only definitions for the round-76 registration-table externs --
 * `make ko` leaves these genuinely unresolved (real target-side symbols,
 * resolved at insmod time), same convention as test_lfo_component.cpp's own
 * STGLFOParams/sMessageHandlers/sValueGetters. */
extern "C" unsigned char STGVPMEGParams[544] = { 0 };
extern "C" unsigned char _ZN9CSTGVPMEG16sMessageHandlersE[96] = { 0 };
extern "C" unsigned char _ZN9CSTGVPMEG13sValueGettersE[96] = { 0 };

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x900
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGVPMEG value-getter family known-answer test (5 methods)\n");
	printf("=============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGVPMEG *s = (CSTGVPMEG *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetAMS1LevelModSource(ctx);
	check_eq("CSTGVPMEG::GetAMS1LevelModSource value", CSTGParamsOwner::sValueGetterTemp.value, -71L);
	s->GetAMS1LevelModIntensity(ctx);
	check_eq("CSTGVPMEG::GetAMS1LevelModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1458934836L);
	check_eq("CSTGVPMEG::GetAMS1LevelModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1458934836L);
	s->GetAMS1TimeModSource(ctx);
	check_eq("CSTGVPMEG::GetAMS1TimeModSource value", CSTGParamsOwner::sValueGetterTemp.value, 42L);
	s->GetAMS1TimeModIntensity(ctx);
	check_eq("CSTGVPMEG::GetAMS1TimeModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 444324925L);
	check_eq("CSTGVPMEG::GetAMS1TimeModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 444324925L);
	s->GetTriggerAtNoteOn(ctx);
	check_eq("CSTGVPMEG::GetTriggerAtNoteOn value", CSTGParamsOwner::sValueGetterTemp.value, 0L);

	printf("\nCSTGVPMEG boilerplate + AMS accessors (round 76)\n");
	{
		check_eq("GetId()", CSTGVPMEG::GetId(), 0x1d);
		check_eq("GetNumParams()", CSTGVPMEG::GetNumParams(), 10);
		check_eq("GetParamDescriptors() non-null", CSTGVPMEG::GetParamDescriptors() != 0, 1);
		check_eq("GetMessageHandlers() non-null", CSTGVPMEG::GetMessageHandlers() != 0, 1);
		check_eq("GetValueGetters() non-null", CSTGVPMEG::GetValueGetters() != 0, 1);
		/* Same deterministic buffer as the Get* family above: buf[0x4f]==0x48,
		 * bit0==0 -> false (matches GetTriggerAtNoteOn's own check above). */
		check_eq("TriggersAtNoteOn() matches GetTriggerAtNoteOn()", s->TriggersAtNoteOn(0), 0);
		check_eq("StateHasLevelAMS(3) true (< 4)", s->StateHasLevelAMS(3), 1);
		check_eq("StateHasLevelAMS(4) false (not < 4)", s->StateHasLevelAMS(4), 0);
		/* buf[0x3e]==0xb9 signed == -71, matches GetAMS1LevelModSource's own
		 * check above -- same field, index argument genuinely ignored. */
		check_eq("GetAMSLevelModSource(anyIndex) matches GetAMS1LevelModSource()",
			 s->GetAMSLevelModSource(7), -71L);
		/* buf[0x2d]==42 signed, matches GetAMS1TimeModSource's own check
		 * above -- same field, index argument genuinely ignored. */
		check_eq("GetAMSTimeModSource(anyIndex) matches GetAMS1TimeModSource()",
			 s->GetAMSTimeModSource(2), 42L);
	}

	printf("\n~CSTGVPMEG (round 77): both D1/D0 collapse to one vptr-zeroing body\n");
	{
		unsigned char dtorbuf[0x60];
		memset(dtorbuf, 0xAA, sizeof(dtorbuf));
		CSTGVPMEG *d = (CSTGVPMEG *)dtorbuf;
		d->~CSTGVPMEG();
		check_eq("dtor zeroes first 4 bytes (vptr placeholder)",
			 *(unsigned int *)dtorbuf, 0L);
	}

	printf("\nCSTGVPMEG updater family (round 75, write-then-readback via the Get* siblings above)\n");
	{
		STGConvertedParam p;
		p.value = 0x11;
		s->UpdateAMS1LevelModSource(ctx, p);
		s->GetAMS1LevelModSource(ctx);
		check_eq("UpdateAMS1LevelModSource round-trip", CSTGParamsOwner::sValueGetterTemp.value, 0x11L);

		p.value = 0x1234;
		s->UpdateAMS1LevelModIntensity(ctx, p);
		s->GetAMS1LevelModIntensity(ctx);
		check_eq("UpdateAMS1LevelModIntensity round-trip", CSTGParamsOwner::sValueGetterTemp.value, 0x1234L);

		p.value = -5;
		s->UpdateAMS1TimeModSource(ctx, p);
		s->GetAMS1TimeModSource(ctx);
		check_eq("UpdateAMS1TimeModSource round-trip", CSTGParamsOwner::sValueGetterTemp.value, -5L);

		p.value = 0x5678;
		s->UpdateAMS1TimeModIntensity(ctx, p);
		s->GetAMS1TimeModIntensity(ctx);
		check_eq("UpdateAMS1TimeModIntensity round-trip", CSTGParamsOwner::sValueGetterTemp.value, 0x5678L);

		p.value = 1;
		s->UpdateTriggerAtNoteOn(ctx, p);
		s->GetTriggerAtNoteOn(ctx);
		check_eq("UpdateTriggerAtNoteOn(1) round-trip", CSTGParamsOwner::sValueGetterTemp.value, 1L);
		p.value = 0;
		s->UpdateTriggerAtNoteOn(ctx, p);
		s->GetTriggerAtNoteOn(ctx);
		check_eq("UpdateTriggerAtNoteOn(0) round-trip", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	}

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
