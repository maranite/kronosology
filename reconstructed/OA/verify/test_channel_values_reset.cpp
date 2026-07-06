// SPDX-License-Identifier: GPL-2.0
/*
 * test_channel_values_reset.cpp  -  KAT for batch 18's
 * src/engine/channel_values_reset.cpp: CSTGChannelValues::Reset().
 *
 * Links the REAL src/engine/cc_info_table.cpp (CSTGCCInfo::sCCInfoTable)
 * so this test cross-checks against real ground-truth CC entries, same
 * convention as test_channel_values.cpp. Provides its own local storage
 * for CSTGGlobal::sInstance / CSTGChannelValues::sTemplateReady /
 * CSTGChannelValues::sTemplate (all normally homed in bar2_stubs.cpp /
 * global.cpp, neither linked here) and its own no-op mock of
 * CSTGChannelValues::InitializeLongHand() (a confirmed-real, deliberately
 * deferred stub elsewhere in the project -- calling into it is safe,
 * matching this project's established precedent).
 */

#include "oa_global.h"
#include "oa_engine_init.h"

#include <sys/mman.h>
#include <cstdio>
#include <cstring>

static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-70s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

static void check_float_eq(const char *label, float got, float want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-70s %f\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %f)\n", want);
}

/* ---- local storage / mocks (dedicated TU, matches project convention) ---- */
CSTGGlobal *CSTGGlobal::sInstance;
unsigned char CSTGChannelValues::sTemplateReady;
unsigned char CSTGChannelValues::sTemplate[0x92c];

static int g_initLongHandCalls;
void CSTGChannelValues::InitializeLongHand()
{
	g_initLongHandCalls++;
}

int main(void)
{
	printf("CSTGChannelValues::Reset() known-answer test\n");
	printf("=============================================\n");

	unsigned char *globalBuf = mmap32(0x2000);
	memset(globalBuf, 0, 0x2000);
	CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;

	unsigned char *cvBuf = (unsigned char *)mmap32(0x1000);
	CSTGChannelValues *cv = (CSTGChannelValues *)cvBuf;

	printf("[1] Lazy static init: first call runs InitializeLongHand() once, "
	       "sTemplateReady set, template copied over `this`\n");
	{
		CSTGChannelValues::sTemplateReady = 0;
		g_initLongHandCalls = 0;
		memset(CSTGChannelValues::sTemplate, 0x42, sizeof(CSTGChannelValues::sTemplate));
		memset(globalBuf + 0x6c0, 0, 2); /* VJSX=0, VJSY=0 -- both valid,
						   * but exercised fully in [3]/[4] below;
						   * here we only care about the copy. */
		globalBuf[0x6c0] = 0xff; /* -1 as signed byte: "no CC assigned" */
		globalBuf[0x6c1] = 0xff;
		memset(cvBuf, 0xcc, 0x1000);

		cv->Reset();
		check_eq("InitializeLongHand called exactly once", (unsigned int)g_initLongHandCalls, 1);
		check_eq("sTemplateReady now 1", CSTGChannelValues::sTemplateReady, 1);
		check_eq("template copied: byte[0]", cvBuf[0], 0x42);
		check_eq("template copied: byte[0x92b] (last)", cvBuf[0x92b], 0x42);
		check_eq("bytes past sTemplate untouched (VJSX/VJSY both -1)", cvBuf[0x92c], 0xcc);

		/* Second call: InitializeLongHand must NOT run again. */
		cv->Reset();
		check_eq("InitializeLongHand NOT called again on 2nd Reset()", (unsigned int)g_initLongHandCalls, 1);
	}

	printf("\n[2] VJSX/VJSY both negative (no CC assigned) -> no per-cc effect at all\n");
	{
		memset(CSTGChannelValues::sTemplate, 0, sizeof(CSTGChannelValues::sTemplate));
		globalBuf[0x6c0] = (unsigned char)-1;
		globalBuf[0x6c1] = (unsigned char)-5;
		memset(cvBuf, 0xcc, 0x1000);
		cv->Reset();
		check_eq("rawArray untouched anywhere (template was all-zero, no per-cc write)",
			 cvBuf[0], 0);
		check_eq("computed[] region untouched (0x628, still template zero)",
			 *(unsigned int *)(cvBuf + 0x628), 0);
	}

	printf("\n[3] VJSX=1 (b1=4,b3=0,b6=2,b7=6 real table entry): reset-to-center applied\n"
	       "    matches SetControllerValue's own confirmed cc=1 shape (test_channel_values.cpp [4])\n");
	{
		/* IMPORTANT: Reset() unconditionally memcpy's `sTemplate` over
		 * `this` FIRST, before any per-cc logic runs -- so any field
		 * whose PRE-Reset() value matters (the fieldB read-modify-write
		 * seed, and the fixed +0x630 "scale" field the b6==2 branch
		 * reads) must be seeded in sTemplate itself, NOT poked directly
		 * into cvBuf (a first draft of this test did exactly that and
		 * got two real FAILED results here, silently clobbered by the
		 * template copy -- an own-test bug, not a reconstruction bug). */
		memset(CSTGChannelValues::sTemplate, 0, sizeof(CSTGChannelValues::sTemplate));
		CSTGChannelValues::sTemplate[1 * 12 + 0xb] = 0xcc; /* rec[1].fieldB seed */
		*(float *)(CSTGChannelValues::sTemplate + 0x630) = 4.0f; /* fixed scale field */
		memset(cvBuf, 0xcc, 0x1000);
		globalBuf[0x6c0] = 1;
		globalBuf[0x6c1] = (unsigned char)-1; /* channel B disabled for this check */

		cv->Reset();

		unsigned char *rec = cvBuf + 1 * 12;
		check_float_eq("rawArray[1].field0 == 0.5f", *(float *)(rec + 0), 0.5f);
		check_float_eq("rawArray[1].field4 == 0.0f", *(float *)(rec + 4), 0.0f);
		check_eq("rawArray[1].field8 == 0x40", *(unsigned short *)(rec + 8), 0x40);
		check_eq("rawArray[1].fieldA == 1", rec[0xa], 1);
		check_eq("rawArray[1].fieldB == (0xcc|1)&~2 == 0xcd", rec[0xb], (unsigned char)((0xcc | 1) & ~2));
		check_eq("mirror8[1] == 0x40", cvBuf[0x8b4 + 1], 0x40);

		/* b3==0 -> chosen=0.5f, stored at computed[b1=4]. */
		check_float_eq("computed[4] == 0.5f (chosen, b3!=1)", *(float *)(cvBuf + 0x628 + 4 * 4), 0.5f);
		/* b6==2 -> computed[b7=6] = chosen + 0.5*fieldAt(0x630) = 0.5 + 0.5*4.0 = 2.5. */
		check_float_eq("computed[6] == chosen + 0.5*fieldAt(0x630) == 2.5f",
			       *(float *)(cvBuf + 0x628 + 6 * 4), 2.5f);
	}

	printf("\n[4] VJSY=17 (b1=11,b3=1,b6=1,b7=15 real table entry): b3==1 -> chosen=0.0f, "
	       "b6==1 -> computed[b7]=0.5f's raw bits\n");
	{
		memset(cvBuf, 0xcc, 0x1000);
		globalBuf[0x6c0] = (unsigned char)-1; /* channel A disabled */
		globalBuf[0x6c1] = 17;

		cv->Reset();

		unsigned char *rec = cvBuf + 17 * 12;
		check_float_eq("rawArray[17].field0 == 0.5f", *(float *)(rec + 0), 0.5f);
		check_float_eq("rawArray[17].field4 == 0.0f", *(float *)(rec + 4), 0.0f);

		/* b3==1 -> chosen = field4 = 0.0f, stored at computed[b1=11]. */
		check_float_eq("computed[11] == 0.0f (chosen, b3==1 selects field4)",
			       *(float *)(cvBuf + 0x628 + 11 * 4), 0.0f);
		/* b6==1 -> computed[b7=15] = raw bit pattern of 0.5f (0x3f000000), NOT "chosen". */
		check_eq("computed[15] == 0x3f000000 (0.5f's raw bits, b6==1)",
			 *(unsigned int *)(cvBuf + 0x628 + 15 * 4), 0x3f000000u);
	}

	printf("\n[5] Both VJSX and VJSY valid and DISTINCT: both get reset, in sequence, "
	       "unaffected by ordering\n");
	{
		memset(cvBuf, 0xcc, 0x1000);
		globalBuf[0x6c0] = 4;  /* b1=8,b3=0,b6=0 (test_channel_values.cpp [3]) */
		globalBuf[0x6c1] = 0;  /* b1=0 -- gate closed, but raw array still written */

		cv->Reset();

		check_float_eq("cc=4: rawArray[4].field0 == 0.5f", *(float *)(cvBuf + 4 * 12 + 0), 0.5f);
		check_float_eq("cc=4: computed[8] == 0.5f (b6==0, no extra write)",
			       *(float *)(cvBuf + 0x628 + 8 * 4), 0.5f);
		check_float_eq("cc=0: rawArray[0].field0 == 0.5f (raw write happens even though b1==0)",
			       *(float *)(cvBuf + 0 * 12 + 0), 0.5f);
		check_eq("cc=0: mirror8[0] == 0x40 (raw-array-adjacent write, independent of b1 gate)",
			 cvBuf[0x8b4 + 0], 0x40);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
