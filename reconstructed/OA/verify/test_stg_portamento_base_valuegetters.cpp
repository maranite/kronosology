// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_portamento_base_valuegetters.cpp  -  KAT for
 * CSTGPortamentoBase's Get* family -- all 6 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_portamento_base_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/bitfield facts the source file's
 * own decoder used -- not by re-using the .cpp file's C output strings
 * -- against the same deterministic non-trivial byte pattern as the
 * rest of the STG value-getter family's KATs: buf[i] = i times 0x9f
 * plus 0x37, all mod 0x100. This class has no ctx-dynamic-index
 * methods, so ctx's own fields are never read, but the buffer is still
 * filled the same way for consistency with the family's KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_portamento_base.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

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
	printf("CSTGPortamentoBase value-getter family known-answer test (6 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPortamentoBase *s = (CSTGPortamentoBase *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetPortamentoTime(ctx);
	check_eq("CSTGPortamentoBase::GetPortamentoTime value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGPortamentoBase::GetPortamentoTime displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetPortamentoEnabled(ctx);
	check_eq("CSTGPortamentoBase::GetPortamentoEnabled value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetPortamentoFingered(ctx);
	check_eq("CSTGPortamentoBase::GetPortamentoFingered value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetPortamentoConstantTime(ctx);
	check_eq("CSTGPortamentoBase::GetPortamentoConstantTime value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetPortamentoAMSSource(ctx);
	check_eq("CSTGPortamentoBase::GetPortamentoAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -101L);
	s->GetPortamentoAMSIntensity(ctx);
	check_eq("CSTGPortamentoBase::GetPortamentoAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CSTGPortamentoBase::GetPortamentoAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
