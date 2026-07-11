// SPDX-License-Identifier: GPL-2.0
/*
 * test_tone_adjust_descriptors.cpp  -  host-side known-answer test for
 * CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors()
 * (batch 53). Only the confirmed DETERMINISTIC fields are asserted --
 * six fields per descriptor are genuine uninitialized-stack-read garbage
 * in ground truth (see tone_adjust_descriptors.cpp's own header comment)
 * and have no canonical expected value to check against.
 */

#include <cstdio>
#include "oa_global.h"
#include "oa_engine.h"

static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

static int g_fail;
static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}
static void check_ptr(const char *label, const void *got, const void *want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s %p\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %p)\n", want);
}

int main(void)
{
	printf("CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors() KAT\n");
	printf("=======================================================================\n");

	((void)0); // no `this` needed -- a static method
	CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors();

	printf("[1] table entry count / stride sanity (37 entries confirmed via nm)\n");
	{
		check_eq("sizeof(STGToneAdjustParamEntry) == 0x10", sizeof(STGToneAdjustParamEntry), 0x10);
	}

	printf("[2] entry 0 (offParamDesc)\n");
	{
		check_eq("b4", STGToneAdjustCommonParams[0].b4, 1);
		check_eq("b5", STGToneAdjustCommonParams[0].b5, 2);
		check_eq("b6", STGToneAdjustCommonParams[0].b6, 0xff);
		check_eq("b7", STGToneAdjustCommonParams[0].b7, 0xff);
		check_eq("f8 == -1", STGToneAdjustCommonParams[0].f8, 0xffffffff);
		check_eq("fc == 0", STGToneAdjustCommonParams[0].fc, 0);
		/* offParamDesc's own deterministic fields, read back through the
		 * entry's own pointer. */
		unsigned char *d = FromU32(STGToneAdjustCommonParams[0].ptr32);
		check_eq("offParamDesc+0x00 == 0", *(unsigned int *)(d + 0x00), 0);
		check_eq("offParamDesc+0x1c == 0", *(unsigned int *)(d + 0x1c), 0);
		check_eq("offParamDesc+0x20 == 0", *(unsigned int *)(d + 0x20), 0);
		check_eq("offParamDesc+0x24 == 0", *(unsigned int *)(d + 0x24), 0);
		check_eq("offParamDesc+0x28 == 1.0f bits", *(unsigned int *)(d + 0x28), 0x3f800000);
		check_eq("offParamDesc+0x2c == 0x10", d[0x2c], 0x10);
		check_eq("offParamDesc+0x2d == 0", d[0x2d], 0);
		check_eq("offParamDesc+0x2e == 0", d[0x2e], 0);
		check_eq("offParamDesc+0x2f == 0", d[0x2f], 0);
		check_eq("offParamDesc+0x30 low bit set (OR'd)", d[0x30] & 1, 1);
		/* +0x18's own low 16 bits are deterministic even though the
		 * dword as a whole has genuine uninitialized upper bits. */
		check_eq("offParamDesc+0x18 low byte == 1", d[0x18], 1);
		check_eq("offParamDesc+0x19 == 0", d[0x19], 0);
	}

	printf("[3] entries 1-3 (smoothed99ToFloat)\n");
	{
		for (int i = 1; i <= 3; i++) {
			check_eq("b4", STGToneAdjustCommonParams[i].b4, 0);
			check_eq("b5", STGToneAdjustCommonParams[i].b5, 3);
			check_eq("f8 == 0", STGToneAdjustCommonParams[i].f8, 0);
			check_ptr("ptr == entry1's ptr (same object)",
				  FromU32(STGToneAdjustCommonParams[i].ptr32), FromU32(STGToneAdjustCommonParams[1].ptr32));
		}
		unsigned char *d = FromU32(STGToneAdjustCommonParams[1].ptr32);
		check_eq("smoothed99ToFloat+0x1c == -99", *(unsigned int *)(d + 0x1c), 0xffffff9d);
		check_eq("smoothed99ToFloat+0x20 == 99", *(unsigned int *)(d + 0x20), 99);
		check_eq("smoothed99ToFloat+0x24 == -1.0f bits", *(unsigned int *)(d + 0x24), 0xbf800000);
		check_eq("smoothed99ToFloat+0x28 == 1.0f bits", *(unsigned int *)(d + 0x28), 0x3f800000);
		check_eq("smoothed99ToFloat+0x2c == 1", d[0x2c], 1);
		check_eq("smoothed99ToFloat+0x2e == 1", d[0x2e], 1);
	}

	printf("[4] entries 4-24, 26-28 (CSTGParamDescriptor::sTypical99ToFloatParamDesc, b5=3)\n");
	{
		int idxs[] = {4,5,10,15,20,24,26,27,28};
		for (int idx : idxs) {
			check_ptr("ptr == &sTypical99ToFloatParamDesc",
				  FromU32(STGToneAdjustCommonParams[idx].ptr32),
				  CSTGParamDescriptor::sTypical99ToFloatParamDesc);
			check_eq("b5 == 3", STGToneAdjustCommonParams[idx].b5, 3);
		}
	}

	printf("[5] entry 30 (sTypical99ToFloatParamDesc but b5=2, the one exception)\n");
	{
		check_eq("b5 == 2", STGToneAdjustCommonParams[30].b5, 2);
		check_ptr("ptr == &sTypical99ToFloatParamDesc",
			  FromU32(STGToneAdjustCommonParams[30].ptr32),
			  CSTGParamDescriptor::sTypical99ToFloatParamDesc);
	}

	printf("[6] entries 25, 29 (lfoStopParamDesc)\n");
	{
		check_ptr("entry25 ptr == entry29 ptr (same object)",
			  FromU32(STGToneAdjustCommonParams[25].ptr32), FromU32(STGToneAdjustCommonParams[29].ptr32));
		check_eq("entry25 b4 == 1", STGToneAdjustCommonParams[25].b4, 1);
		check_eq("entry25 f8 == -1", STGToneAdjustCommonParams[25].f8, 0xffffffff);
		unsigned char *d = FromU32(STGToneAdjustCommonParams[25].ptr32);
		check_eq("lfoStopParamDesc+0x00 == -1 (the one asymmetric case)",
			 *(unsigned int *)(d + 0x00), 0xffffffff);
		check_eq("lfoStopParamDesc+0x1c == -1", *(unsigned int *)(d + 0x1c), 0xffffffff);
		check_eq("lfoStopParamDesc+0x20 == 1", *(unsigned int *)(d + 0x20), 1);
		check_eq("lfoStopParamDesc+0x24 == 0", *(unsigned int *)(d + 0x24), 0);
	}

	printf("[7] entries 31-34 (STGProgramParams+offset, incrementing id 16-19)\n");
	{
		unsigned int offs[4] = {832, 884, 936, 988};
		unsigned int ids[4] = {16,17,18,19};
		for (int i = 0; i < 4; i++) {
			check_ptr("ptr == STGProgramParams+off", FromU32(STGToneAdjustCommonParams[31+i].ptr32),
				  STGProgramParams + offs[i]);
			check_eq("b4 == 1", STGToneAdjustCommonParams[31+i].b4, 1);
			check_eq("b5 == 0", STGToneAdjustCommonParams[31+i].b5, 0);
			check_eq("f8 == id", STGToneAdjustCommonParams[31+i].f8, ids[i]);
		}
	}

	printf("[8] entries 35-36 (STGCommonStepSeqParams+offset)\n");
	{
		check_ptr("entry35 ptr == STGCommonStepSeqParams+260", FromU32(STGToneAdjustCommonParams[35].ptr32),
			  STGCommonStepSeqParams + 260);
		check_eq("entry35 f8 == 5", STGToneAdjustCommonParams[35].f8, 5);
		check_ptr("entry36 ptr == STGCommonStepSeqParams+624", FromU32(STGToneAdjustCommonParams[36].ptr32),
			  STGCommonStepSeqParams + 624);
		check_eq("entry36 f8 == 12", STGToneAdjustCommonParams[36].f8, 12);
		check_eq("entry35/36 b4 == 1", STGToneAdjustCommonParams[35].b4, 1);
		check_eq("entry35/36 b5 == 1", STGToneAdjustCommonParams[35].b5, 1);
	}

	printf("[9] guard re-entrancy: second call must NOT reset the three\n"
	       "    descriptors' deterministic fields (matches ground truth's\n"
	       "    own once-only guard, -fno-threadsafe-statics codegen)\n");
	{
		unsigned char *before = FromU32(STGToneAdjustCommonParams[0].ptr32);
		before[0x2c] = 0xAB; /* poison a deterministic field */
		CSTGToneAdjustDescriptor::InitializeCommonToneAdjustDescriptors();
		check_eq("offParamDesc+0x2c NOT re-touched by 2nd call (guard held)",
			 before[0x2c], 0xAB);
		/* But the always-rerun STGToneAdjustCommonParams table itself IS
		 * rewritten every call -- confirm it's still correct afterward. */
		check_eq("table entry0 b4 still correct after 2nd call",
			 STGToneAdjustCommonParams[0].b4, 1);
	}

	if (g_fail) {
		printf("\n%d CHECK(S) FAILED\n", g_fail);
		return 1;
	}
	printf("\nALL CHECKS PASSED\n");
	return 0;
}
