// SPDX-License-Identifier: GPL-2.0
/*
 * test_alias_bank_convert.cpp  -  host-side known-answer test for
 * USTGAliasBankTypes's four bank conversion helpers (sec 10.152):
 * ConvertCombiBankToMidiBank / ConvertMidiBankToCombiBank /
 * ConvertAliasPgmBankToMidiBank / ConvertMidiBankToAliasProgramBank.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

static int g_fail;
static void check_eq(const char *label, int got, int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-70s %d\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %d)\n", want);
}

CSTGGlobal *CSTGGlobal::sInstance;

static void SetMode(unsigned char *g, int mode)
{
	g[0x6e4] = (unsigned char)mode;
}

int main(void)
{
	printf("USTGAliasBankTypes bank conversion known-answer test\n");
	printf("=====================================================\n");

	unsigned char *g = (unsigned char *)mmap32(0x1000);
	memset(g, 0, 0x1000);
	CSTGGlobal::sInstance = (CSTGGlobal *)g;

	printf("[1] ConvertCombiBankToMidiBank\n");
	{
		static const unsigned char kExpected[14] = {
			0, 1, 2, 3, 4, 5, 6, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe,
		};
		SetMode(g, 0);
		for (int i = 0; i <= 13; i++) {
			char out1 = 0x55, out2 = 0x55;
			USTGAliasBankTypes::ConvertCombiBankToMidiBank(i, out1, out2);
			char label[64];
			snprintf(label, sizeof(label), "mode0 bankId=%d -> midiBank", i);
			check_eq(label, (unsigned char)out2, kExpected[i]);
			check_eq("  ...flag byte == 0 (mode0)", (unsigned char)out1, 0);
		}
		SetMode(g, 1);
		{
			char out1 = 0, out2 = 0;
			USTGAliasBankTypes::ConvertCombiBankToMidiBank(6, out1, out2);
			check_eq("mode1 flag byte == 0x3f", (unsigned char)out1, 0x3f);
			check_eq("  ...midiBank unaffected by mode", (unsigned char)out2, 6);
		}
		{
			char out1 = 0x11, out2 = 0x22;
			USTGAliasBankTypes::ConvertCombiBankToMidiBank(14, out1, out2);
			check_eq("bankId=14 (out of range) -> midiBank 0", (unsigned char)out2, 0);
		}
	}

	printf("[2] ConvertMidiBankToCombiBank (round trip against [1])\n");
	{
		SetMode(g, 0);
		for (int lsb = 0; lsb <= 0xe; lsb++) {
			int outBankId = -1;
			USTGAliasBankTypes::ConvertMidiBankToCombiBank(0, (char)lsb, outBankId);
			char label[64];
			snprintf(label, sizeof(label), "mode0 lsb=%d -> combiBank", lsb);
			/* lsb==7 is a confirmed gap: no combiBank maps to
			 * midiBank 7, but the reverse table's own index 7
			 * still resolves to 8 (matching the real jump
			 * target), not a "no-op". */
			static const int kExpected[15] = {
				0, 1, 2, 3, 4, 5, 6, 0 /* gap */, 7, 8, 9, 0xa, 0xb, 0xc, 0xd,
			};
			check_eq(label, outBankId, kExpected[lsb]);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToCombiBank(0x3f, 3, outBankId);
			check_eq("mode0, wrong msb (0x3f) -> stays default 0", outBankId, 0);
		}
		SetMode(g, 1);
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToCombiBank(0, 3, outBankId);
			check_eq("mode1, wrong msb (0) -> stays default 0", outBankId, 0);
			USTGAliasBankTypes::ConvertMidiBankToCombiBank(0x3f, 3, outBankId);
			check_eq("mode1, correct msb (0x3f) -> converts", outBankId, 3);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToCombiBank(0x3f, 0x20, outBankId);
			check_eq("lsb out of range (>0xe) -> stays default 0", outBankId, 0);
		}
	}

	printf("[3] ConvertAliasPgmBankToMidiBank\n");
	{
		SetMode(g, 0);
		{
			char out1 = 0x55, out2 = 0x55;
			USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(0, out1, out2);
			check_eq("mode0 aliasId=0 -> (midiBank,progBank)==(0,0)", (unsigned char)out1, 0);
			check_eq("  ...", (unsigned char)out2, 0);
		}
		{
			char out1 = 0, out2 = 0;
			USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(6, out1, out2);
			check_eq("mode0 aliasId=6 -> midiBank==0x79 ('y')", (unsigned char)out1, 0x79);
			check_eq("  ...progBank==0", (unsigned char)out2, 0);
		}
		{
			char out1 = 0, out2 = 0;
			USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(16, out1, out2);
			check_eq("mode0 aliasId=16 -> midiBank==0x78 ('x')", (unsigned char)out1, 0x78);
		}
		{
			char out1 = 0, out2 = 0;
			USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(30, out1, out2);
			check_eq("mode0 aliasId=30 -> (0,21)", (unsigned char)out1, 0);
			check_eq("  ...", (unsigned char)out2, 21);
		}
		{
			char out1 = 0x11, out2 = 0x11;
			USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(31, out1, out2);
			check_eq("mode0 aliasId=31 (out of range) -> (0,0)", (unsigned char)out1, 0);
			check_eq("  ...", (unsigned char)out2, 0);
		}
		SetMode(g, 1);
		{
			char out1 = 0, out2 = 0;
			USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(30, out1, out2);
			check_eq("mode1 aliasId=30 -> (0x3f,21)", (unsigned char)out1, 0x3f);
			check_eq("  ...", (unsigned char)out2, 21);
		}
		{
			char out1 = 0x11, out2 = 0x11;
			USTGAliasBankTypes::ConvertAliasPgmBankToMidiBank(31, out1, out2);
			check_eq("mode1 aliasId=31 (out of range) -> (0x3f,0)", (unsigned char)out1, 0x3f);
			check_eq("  ...", (unsigned char)out2, 0);
		}
	}

	printf("[4] ConvertMidiBankToAliasProgramBank (round trip against [3])\n");
	{
		SetMode(g, 0);
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0, 0, outBankId);
			check_eq("mode0 msb=0 lsb=0 -> aliasId 0", outBankId, 0);
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0, 5, outBankId);
			check_eq("mode0 msb=0 lsb=5 -> aliasId 5", outBankId, 5);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0, 6, outBankId);
			check_eq("mode0 msb=0 lsb=6 (gap) -> stays default 0", outBankId, 0);
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0, 7, outBankId);
			check_eq("mode0 msb=0 lsb=7 (gap) -> stays default 0", outBankId, 0);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x79, 0, outBankId);
			check_eq("mode0 msb=0x79 lsb=0 -> aliasId 6", outBankId, 6);
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x79, 9, outBankId);
			check_eq("mode0 msb=0x79 lsb=9 -> aliasId 15", outBankId, 15);
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x79, 20, outBankId);
			check_eq("mode0 msb=0x79 lsb=20 (>9) -> aliasId 6", outBankId, 6);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x78, 0, outBankId);
			check_eq("mode0 msb=0x78 -> aliasId 16", outBankId, 16);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x3f, 3, outBankId);
			check_eq("mode0, wrong-mode msb (0x3f) -> stays default 0", outBankId, 0);
		}
		printf("  -- mode1 (gm2Mode) --\n");
		SetMode(g, 1);
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x3f, 21, outBankId);
			check_eq("mode1 msb=0x3f lsb=21 -> aliasId 30", outBankId, 30);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0, 3, outBankId);
			check_eq("mode1 msb=0 (confirmed quirk) -> aliasId 6, NOT table lookup", outBankId, 6);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x38, 0, outBankId);
			check_eq("mode1 msb=0x38 -> aliasId 6", outBankId, 6);
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x3e, 0, outBankId);
			check_eq("mode1 msb=0x3e -> aliasId 0x10", outBankId, 0x10);
		}
		{
			int outBankId = 0x55;
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x79, 3, outBankId);
			check_eq("mode1 msb=0x79 (shared tail) -> aliasId 9", outBankId, 9);
			USTGAliasBankTypes::ConvertMidiBankToAliasProgramBank(0x78, 0, outBankId);
			check_eq("mode1 msb=0x78 (shared tail) -> aliasId 16", outBankId, 16);
		}
	}

	printf("=====================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
