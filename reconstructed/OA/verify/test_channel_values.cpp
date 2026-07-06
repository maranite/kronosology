// SPDX-License-Identifier: GPL-2.0
/*
 * test_channel_values.cpp  -  host-side known-answer test for
 * CSTGChannelValues::SetControllerValue() (batch 15), using the real
 * CSTGCCInfo::sCCInfoTable data (linked in from cc_info_table.cpp) rather
 * than a synthetic table, so this test also cross-checks against real
 * ground-truth CC entries.
 *
 * Real table entries used (indices are CC numbers, values are
 * {b0..b9}, see cc_info_table.cpp):
 *   cc=0:  b1=0                         -> gate closed, no computed writes.
 *   cc=4:  b1=8,  b3=0, b6=0            -> computed[8]=field0(as float bits), no b7 write.
 *   cc=1:  b1=4,  b3=0, b6=2, b7=6      -> computed[4]=field0(as float), computed[6]=computed[4]+0.5*fieldAt(0x630).
 *   cc=17: b1=11, b3=1, b6=1, b7=15     -> computed[11]=field4(real float), computed[15]=field0(raw int, NOT field4).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine_init.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
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

int main(void)
{
	printf("CSTGChannelValues::SetControllerValue known-answer test\n");
	printf("=========================================================\n");

	unsigned char *mem = (unsigned char *)mmap32(0x1000);
	memset(mem, 0xcc, 0x1000);
	CSTGChannelValues *cv = (CSTGChannelValues *)mem;

	printf("[1] cc > 0x77 -> full no-op (raw array untouched)\n");
	{
		memset(mem, 0xcc, 0x1000);
		CSTGControllerValue val = { 0x11111111u, 1.5f, 0x2222, 3, 4 };
		cv->SetControllerValue(0x78, val);
		check_eq("raw[0x78] untouched (still poisoned)", mem[0x78 * 12], 0xcc);
	}

	printf("[2] cc=0, table[0].b1==0 -> raw copy happens, but gate closed (no computed writes)\n");
	{
		memset(mem, 0xcc, 0x1000);
		CSTGControllerValue val = { 0xdeadbeefu, 2.5f, 0x1234, 7, 9 };
		cv->SetControllerValue(0, val);
		unsigned int rawf0;
		memcpy(&rawf0, mem + 0, 4);
		check_eq("raw[0].field0 copied", rawf0, 0xdeadbeefu);
		unsigned short rawf8;
		memcpy(&rawf8, mem + 8, 2);
		check_eq("raw[0].field8 copied", rawf8, 0x1234);
		check_eq("raw[0].fieldA copied", mem[0xa], 7);
		check_eq("raw[0].fieldB copied", mem[0xb], 9);
		check_eq("computed[0x628] untouched (gate closed)", *(unsigned int *)(mem + 0x628), 0xcccccccc);
	}

	printf("[3] cc=4: b1=8,b3=0,b6=0 -> computed[8]=field0 as float bits, no b7 write\n");
	{
		memset(mem, 0xcc, 0x1000);
		float fval = 3.25f;
		unsigned int rawBits;
		memcpy(&rawBits, &fval, 4);
		CSTGControllerValue val = { rawBits, 99.0f, 0, 1 /* not 3/4/5 */, 0 };
		cv->SetControllerValue(4, val);
		check_float_eq("computed[8] == field0 reinterpreted as float",
			       *(float *)(mem + 0x628 + 8 * 4), fval);
		/* type==1 (not 3/4/5) -> mirror8[4] should be written (low byte of field8). */
		check_eq("mirror8[4] == field8 low byte", mem[0x8b4 + 4], 0);
	}

	printf("[4] cc=1: b1=4,b3=0,b6=2,b7=6 -> computed[6] = computed[4] + 0.5*fieldAt(0x630)\n");
	{
		memset(mem, 0xcc, 0x1000);
		float fval = 10.0f;
		unsigned int rawBits;
		memcpy(&rawBits, &fval, 4);
		*(float *)(mem + 0x630) = 4.0f;
		CSTGControllerValue val = { rawBits, 0.0f, 0, 5 /* skips mirror8 write */, 0 };
		cv->SetControllerValue(1, val);
		check_float_eq("computed[4] == field0 as float", *(float *)(mem + 0x628 + 4 * 4), 10.0f);
		check_float_eq("computed[6] == computed[4] + 0.5*fieldAt(0x630)",
			       *(float *)(mem + 0x628 + 6 * 4), 12.0f);
		/* type==5 -> mirror8 write must be SKIPPED. */
		check_eq("mirror8[1] left poisoned (type==5 skips it)", mem[0x8b4 + 1], 0xcc);
	}

	printf("[5] cc=17: b1=11,b3=1,b6=1,b7=15 -> computed[11]=field4 (float), computed[15]=field0 (raw int)\n");
	{
		memset(mem, 0xcc, 0x1000);
		CSTGControllerValue val = { 0x2a2a2a2au, 7.5f, 0, 3 /* skips mirror8 */, 0 };
		cv->SetControllerValue(17, val);
		check_float_eq("computed[11] == field4 (b3==1 selects field4)",
			       *(float *)(mem + 0x628 + 11 * 4), 7.5f);
		check_eq("computed[15] == field0 as raw int (b6==1, NOT field4)",
			 *(unsigned int *)(mem + 0x628 + 15 * 4), 0x2a2a2a2au);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
