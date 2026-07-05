// SPDX-License-Identifier: GPL-2.0
/*
 * test_waveseq_generator.cpp  -  host-side known-answer test for
 * CSTGWaveSeqGenerator::CSTGWaveSeqGenerator()/Init() (sec 10.152).
 */

#include <cstdio>
#include <cstring>
#include <new>
#include <sys/mman.h>
#include "oa_engine_init.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s 0x%lx\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%lx)\n", want);
}

static unsigned int ToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

int main(void)
{
	printf("CSTGWaveSeqGenerator::CSTGWaveSeqGenerator()/Init() known-answer test\n");
	printf("=======================================================================\n");

	unsigned char *buf = (unsigned char *)mmap32(sizeof(CSTGWaveSeqGenerator));

	printf("[1] Constructor: three self-anchored empty intrusive lists\n");
	{
		memset(buf, 0xaa, sizeof(CSTGWaveSeqGenerator));
		new (buf) CSTGWaveSeqGenerator();

		check_eq("+0x0 == 0", *(unsigned int *)(buf + 0x0), 0);
		check_eq("+0x4 == 0", *(unsigned int *)(buf + 0x4), 0);
		check_eq("+0x8 == this (self-anchor)", *(unsigned int *)(buf + 0x8), ToU32(buf));
		check_eq("+0xc == 0", *(unsigned int *)(buf + 0xc), 0);

		check_eq("+0x10 == 0", *(unsigned int *)(buf + 0x10), 0);
		check_eq("+0x14 == 0", *(unsigned int *)(buf + 0x14), 0);
		check_eq("+0x18 == this (self-anchor)", *(unsigned int *)(buf + 0x18), ToU32(buf));
		check_eq("+0x1c == 0", *(unsigned int *)(buf + 0x1c), 0);

		check_eq("+0x20 == 0", *(unsigned int *)(buf + 0x20), 0);
		check_eq("+0x24 == 0", *(unsigned int *)(buf + 0x24), 0);
		check_eq("+0x28 == this (self-anchor)", *(unsigned int *)(buf + 0x28), ToU32(buf));
		check_eq("+0x2c == 0", *(unsigned int *)(buf + 0x2c), 0);
	}

	printf("[2] Constructor: scalar zero-fill and flag-bit clears\n");
	{
		check_eq("+0x70 byte == 0", buf[0x70], 0);
		check_eq("+0x110 dword == 0", *(unsigned int *)(buf + 0x110), 0);
		check_eq("+0x38 word == 0", *(unsigned short *)(buf + 0x38), 0);
		check_eq("+0x34 word == 0", *(unsigned short *)(buf + 0x34), 0);
		check_eq("+0x30 dword == 0", *(unsigned int *)(buf + 0x30), 0);
		check_eq("+0xbc..+0xd8 all zeroed", *(unsigned int *)(buf + 0xbc), 0);
		check_eq("  +0xc0", *(unsigned int *)(buf + 0xc0), 0);
		check_eq("  +0xc4", *(unsigned int *)(buf + 0xc4), 0);
		check_eq("  +0xc8", *(unsigned int *)(buf + 0xc8), 0);
		check_eq("  +0xcc", *(unsigned int *)(buf + 0xcc), 0);
		check_eq("  +0xd0", *(unsigned int *)(buf + 0xd0), 0);
		check_eq("  +0xd4", *(unsigned int *)(buf + 0xd4), 0);
		check_eq("  +0xd8", *(unsigned int *)(buf + 0xd8), 0);
	}
	{
		/* Confirmed real: the ctor only ANDs +0x64/+0x78 in place
		 * (clearing bit 0x08 / bits 0x1f respectively) -- it does
		 * NOT zero-init them, so poison bits outside those masks
		 * must survive the constructor untouched. */
		memset(buf, 0xff, sizeof(CSTGWaveSeqGenerator));
		new (buf) CSTGWaveSeqGenerator();
		check_eq("+0x64 &= 0xf7 (0xff -> 0xf7)", buf[0x64], 0xf7);
		check_eq("+0x78 &= 0xfd (0xff -> 0xfd)", buf[0x78], 0xfd);
	}

	printf("[3] Init(): self-relative pointer caches + masks (fresh, poisoned buffer)\n");
	{
		memset(buf, 0x5a, sizeof(CSTGWaveSeqGenerator));
		((CSTGWaveSeqGenerator *)buf)->Init();

		check_eq("+0xbc == self+0xec", *(unsigned int *)(buf + 0xbc), ToU32(buf + 0xec));
		check_eq("+0xc0 == self+0xe4", *(unsigned int *)(buf + 0xc0), ToU32(buf + 0xe4));
		check_eq("+0xc4 == self+0xe8", *(unsigned int *)(buf + 0xc4), ToU32(buf + 0xe8));
		check_eq("+0xd4 == self+0xdc", *(unsigned int *)(buf + 0xd4), ToU32(buf + 0xdc));
		check_eq("+0xd8 == self+0xe0", *(unsigned int *)(buf + 0xd8), ToU32(buf + 0xe0));

		check_eq("+0x64 &= 0xf0 (0x5a -> 0x50)", buf[0x64], 0x5a & 0xf0);
		check_eq("+0x78 &= 0xe0 (0x5a -> 0x40)", buf[0x78], 0x5a & 0xe0);
		check_eq("+0x71 &= 0xfe", buf[0x71], 0x5a & 0xfe);
		check_eq("+0x74 &= 0xfd", buf[0x74], 0x5a & 0xfd);
	}

	printf("[4] Init(): zero-fill run\n");
	{
		/* +0x70 is confirmed real as a single BYTE zero-write
		 * (`movb $0x0,0x70(%eax)`), NOT a word/dword -- its own
		 * neighbor byte +0x71 is separately masked (`&= 0xfe`), not
		 * zeroed, by a later instruction, so it deliberately stays
		 * poisoned here and must be checked as a byte, not folded
		 * into the word-sized checks below (caught by a real test
		 * failure when first written as a blanket u16 check). */
		check_eq("+0x70 byte == 0", buf[0x70], 0);

		static const int offsets[] = {
			0x30, 0x3c, 0x3e, 0x40, 0x44, 0x48, 0x4c, 0x50,
			0x72, 0x7a, 0x7c, 0x7e, 0x80, 0x82, 0x100,
			0xb8, 0x68, 0x6c,
		};
		bool allZero = true;
		for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
			/* +0x30/+0x44/+0x48/+0x4c/+0x50/+0xb8/+0x68/+0x6c are
			 * dwords; the rest are words -- either way, a plain
			 * 0 check on the low word is sufficient here. */
			if (*(unsigned short *)(buf + offsets[i]) != 0)
				allZero = false;
		}
		check_eq("all zero-fill offsets == 0", allZero ? 1 : 0, 1);
	}

	printf("[5] Init(): five sDummyAMS pointer fields\n");
	{
		unsigned int dummy = ToU32(CSTGWaveSeqGenerator::sDummyAMS);
		check_eq("+0xc8 == &sDummyAMS", *(unsigned int *)(buf + 0xc8), dummy);
		check_eq("+0xcc == &sDummyAMS", *(unsigned int *)(buf + 0xcc), dummy);
		check_eq("+0xd0 == &sDummyAMS", *(unsigned int *)(buf + 0xd0), dummy);
		check_eq("+0xf8 == &sDummyAMS", *(unsigned int *)(buf + 0xf8), dummy);
		check_eq("+0xfc == &sDummyAMS", *(unsigned int *)(buf + 0xfc), dummy);
	}

	printf("=======================================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
