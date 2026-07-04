// SPDX-License-Identifier: GPL-2.0
/*
 * test_smoother_init.cpp  -  host-side known-answer test for
 * CSTGSmoother::Initialize() (sec 10.86).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine_init.h"
#include "oa_bank_memory.h"
#include "oa_setup_global_resources.h"

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

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

CSTGCPUInfo *CSTGCPUInfo::sInstance;

int main(void)
{
	printf("CSTGSmoother::Initialize() known-answer test\n");
	printf("=========================================================\n");

	/* CSTGBankMemory needs a real backing pool (a plain bump allocator,
	 * confirmed real, already reconstructed). */
	unsigned char *pool = mmap32(0x10000);
	CSTGBankMemory::Initialize(pool, 0x10000);

	/* Raw allocation, not the real (already-reconstructed elsewhere,
	 * sec 10.57) constructor -- this test only needs field8 set, not a
	 * real CSTGCPUInfo construction. */
	unsigned char *cpuInfoBuf = mmap32(0x100);
	memset(cpuInfoBuf, 0, 0x100);
	CSTGCPUInfo *cpuInfo = (CSTGCPUInfo *)cpuInfoBuf;
	cpuInfo->field8 = 1000.0f; /* cyclesPerTick */
	CSTGCPUInfo::sInstance = cpuInfo;

	/* CSTGSmoother itself: 320 mapping objects x 0xc0 bytes each, plus
	 * the +0xf000..+0xf024 header fields -- matches the real confirmed
	 * size (0xf028 rounded up). Zeroed, matching what the real (still
	 * deferred here) constructor is confirmed to do to these same
	 * fields -- see smoother_init.cpp's own file header. Confirmed
	 * real quirk: the FIRST-inserted node's own link+0x4 ("prev") is
	 * never written by Initialize() itself (matches the already-
	 * verified CSTGWaveSeqManager::Initialize() precedent, sec 10.62) --
	 * a zeroed buffer is what makes that read safe here, not a test
	 * workaround. */
	unsigned char *buf = mmap32(0x10000);
	memset(buf, 0, 0x10000);
	CSTGSmoother *s = (CSTGSmoother *)buf;

	s->Initialize();

	printf("[1] allocation\n");
	check_eq("+0xf000 buffer pointer is non-null", *(unsigned int *)(buf + 0xf000) != 0, 1);

	printf("[2] free list built: count == 320, head/tail both set\n");
	check_eq("count (+0xf00c) == 320", *(unsigned int *)(buf + 0xf00c), 0x140);
	unsigned int head = *(unsigned int *)(buf + 0xf004);
	unsigned int tail = *(unsigned int *)(buf + 0xf008);
	check_eq("head (+0xf004) non-zero", head != 0, 1);
	check_eq("tail (+0xf008) non-zero", tail != 0, 1);
	check_eq("tail == &mapping[0]+0xb0 (first inserted, permanent tail)",
		 tail, ToU32(buf + 0 * 0xc0 + 0xb0));
	check_eq("head == &mapping[319]+0xb0 (last inserted, push-front)",
		 head, ToU32(buf + 319 * 0xc0 + 0xb0));

	printf("[3] every mapping object's own index field is set correctly\n");
	check_eq("mapping[0] index == 0", *(unsigned short *)(buf + 0 * 0xc0), 0);
	check_eq("mapping[1] index == 1", *(unsigned short *)(buf + 1 * 0xc0), 1);
	check_eq("mapping[319] index == 319", *(unsigned short *)(buf + 319 * 0xc0), 319);

	printf("[4] list walkability: walking from head via 'next' (+0x0) reaches all 320\n");
	{
		unsigned int cur = head;
		int walked = 0;
		while (cur != 0 && walked <= 320) {
			walked++;
			cur = *(unsigned int *)(unsigned long)cur;
		}
		check_eq("walked exactly 320 nodes before hitting null", (unsigned int)walked, 320);
	}

	printf("[5] every node's owner (+0xc relative to the link sub-object) points at the head field\n");
	check_eq("mapping[0]'s link owner == &(+0xf004)",
		 *(unsigned int *)(buf + 0 * 0xc0 + 0xb0 + 0xc), ToU32(buf + 0xf004));
	check_eq("mapping[319]'s link owner == &(+0xf004)",
		 *(unsigned int *)(buf + 319 * 0xc0 + 0xb0 + 0xc), ToU32(buf + 0xf004));

	printf("[6] tail fields cleared, timing constant computed\n");
	check_eq("+0xf01c == 0", *(unsigned int *)(buf + 0xf01c), 0);
	check_eq("+0xf020 == 0", *(unsigned int *)(buf + 0xf020), 0);
	check_eq("+0xf024 == (int)(0.04 * 1000.0) == 40", *(int *)(buf + 0xf024), 40);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
