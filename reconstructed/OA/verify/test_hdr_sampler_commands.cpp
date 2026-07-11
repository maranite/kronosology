// SPDX-License-Identifier: GPL-2.0
/*
 * test_hdr_sampler_commands.cpp  -  host-side known-answer tests for
 * CSTGHDRManager::ProcessSamplerCommands() (batch 50).
 *
 * IMPORTANT caveat, matching this project's own honest-testing discipline:
 * all four of CSTGSampler::StandbyDisk()/StandbyRAM()/Start(bool)/Stop()
 * are DELIBERATELY deferred no-op bodies (see hdr_sampler_commands.cpp's
 * own header comment -- genuine audio-DSP/hardware-adjacent code, out of
 * scope per the sec 10.185 policy). Unlike CSTGHDRManager::
 * ProcessRecordCommands()'s own test (test_hdr_record_track.cpp), where
 * 3 of 4 dispatch targets were real and their side effects directly
 * observable, NONE of ProcessSamplerCommands()'s four dispatch targets
 * have any observable side effect here. These tests therefore verify what
 * IS genuinely this function's own real logic -- the ring-buffer
 * bookkeeping (consumer/producer index handling, capacity-modulus
 * wraparound, unknown-tag no-op-but-still-consumed behavior) -- and that
 * every tag's field extraction/dispatch runs to completion without
 * crashing (exercising the FromU32 reinterpretation + call for tags 0/1),
 * rather than claiming to verify dispatch correctness beyond what the
 * confirmed relocation-level derivation in hdr_sampler_commands.cpp's own
 * header comment already established.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

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

int main(void)
{
	printf("CSTGHDRManager::ProcessSamplerCommands test\n");
	printf("==============================================================\n");

	printf("[1] Empty ring (consumer == producer): no-op\n");
	{
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0xcc, 0x1000);

		*(unsigned int *)(hdrMem + 0x18af8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18afc) = 3; /* producer */
		*(unsigned int *)(hdrMem + 0x18b00) = 3; /* consumer, same as producer */
		*(unsigned int *)(hdrMem + 0x18b04) = 8; /* capacity */

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessSamplerCommands();

		check_eq("consumer index unchanged (3)",
			 *(unsigned int *)(hdrMem + 0x18b00), 3);
	}

	printf("[2] Four entries, tags 0/1/2/3: consumer catches up to producer\n");
	{
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);
		unsigned char *nameBuf = (unsigned char *)mmap32(0x100);
		strcpy((char *)nameBuf, "S010");
		unsigned char *ramA = (unsigned char *)mmap32(0x1000);
		unsigned char *ramB = (unsigned char *)mmap32(0x1000);

		*(unsigned int *)(hdrMem + 0x18af8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18afc) = 4; /* producer */
		*(unsigned int *)(hdrMem + 0x18b00) = 0; /* consumer */
		*(unsigned int *)(hdrMem + 0x18b04) = 8; /* capacity */

		/* entry[0]: tag=0 (StandbyDisk), name=nameBuf, p2/p3/busId/busType/mode/p7/p8 */
		unsigned char *e0 = ring + 0 * 0x2c;
		e0[0x00] = 0;
		*(unsigned int *)(e0 + 0x04) = ToU32(nameBuf);
		*(unsigned int *)(e0 + 0x10) = 0x1234;   /* p2 */
		*(unsigned int *)(e0 + 0x14) = 0x5678;   /* p3 */
		*(unsigned int *)(e0 + 0x18) = 1;        /* busId */
		*(unsigned int *)(e0 + 0x1c) = 2;        /* busType */
		*(unsigned int *)(e0 + 0x20) = 3;        /* mode */
		*(unsigned int *)(e0 + 0x24) = 0x9abc;   /* p7 */
		e0[0x28] = 0x42;                          /* p8 */

		/* entry[1]: tag=1 (StandbyRAM), two short* buffers */
		unsigned char *e1 = ring + 1 * 0x2c;
		e1[0x00] = 1;
		*(unsigned int *)(e1 + 0x08) = ToU32(ramA);
		*(unsigned int *)(e1 + 0x0c) = ToU32(ramB);
		*(unsigned int *)(e1 + 0x14) = 0x100;
		*(unsigned int *)(e1 + 0x18) = 4;
		*(unsigned int *)(e1 + 0x1c) = 5;
		*(unsigned int *)(e1 + 0x20) = 6;
		*(unsigned int *)(e1 + 0x24) = 0x200;
		e1[0x28] = 7;

		/* entry[2]: tag=2 (Start(true)) */
		unsigned char *e2 = ring + 2 * 0x2c;
		e2[0x00] = 2;

		/* entry[3]: tag=3 (Stop()) */
		unsigned char *e3 = ring + 3 * 0x2c;
		e3[0x00] = 3;

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessSamplerCommands();

		check_eq("consumer index advanced to producer (4)",
			 *(unsigned int *)(hdrMem + 0x18b00), 4);
	}

	printf("[3] Unknown tag: still consumed (no crash), consumer advances\n");
	{
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);

		*(unsigned int *)(hdrMem + 0x18af8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18afc) = 1; /* producer */
		*(unsigned int *)(hdrMem + 0x18b00) = 0; /* consumer */
		*(unsigned int *)(hdrMem + 0x18b04) = 8; /* capacity */

		ring[0x00] = 99; /* unrecognized tag */

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessSamplerCommands();

		check_eq("consumer index advanced past unknown tag (1)",
			 *(unsigned int *)(hdrMem + 0x18b00), 1);
	}

	printf("[4] Consumer index wraparound at capacity boundary\n");
	{
		unsigned char *hdrMem = (unsigned char *)mmap32(0x20000);
		memset(hdrMem, 0, 0x20000);
		unsigned char *ring = (unsigned char *)mmap32(0x1000);
		memset(ring, 0, 0x1000);

		*(unsigned int *)(hdrMem + 0x18af8) = ToU32(ring);
		*(unsigned int *)(hdrMem + 0x18b04) = 4; /* capacity: only 4 slots */
		*(unsigned int *)(hdrMem + 0x18b00) = 3; /* consumer starts at last slot */
		*(unsigned int *)(hdrMem + 0x18afc) = 1; /* producer has wrapped to slot 1 */

		ring[3 * 0x2c] = 2; /* Start() -- cheap tag, no extra fields needed */
		ring[0 * 0x2c] = 2;

		CSTGHDRManager *hdr = (CSTGHDRManager *)hdrMem;
		hdr->ProcessSamplerCommands();

		check_eq("consumer wrapped 3 -> 0 -> 1",
			 *(unsigned int *)(hdrMem + 0x18b00), 1);
	}

	printf("==============================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
