// SPDX-License-Identifier: GPL-2.0
/*
 * test_smoother_ctor.cpp  -  host-side known-answer test for
 * CSTGSmoother::CSTGSmoother() (batch 22).
 *
 * This test does NOT link engine_init.cpp (which owns
 * CSTGSmoother::sInstance's real storage) -- provide our own local copy
 * here, matching this project's established per-test-file storage
 * precedent (e.g. test_engine.cpp/test_global.cpp's own copies of the
 * same static for other classes).
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine_init.h"
#include "oa_internal.h"	/* placement operator new(size_t, void*) */

CSTGSmoother *CSTGSmoother::sInstance;

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

extern "C" unsigned char _ZTV25CSTGProgramMessageContext[12];
extern "C" unsigned char _ZTV29CSTGProgramSlotMessageContext[12];
extern "C" unsigned char _ZTV23CSTGPatchMessageContext[12];
extern "C" unsigned char _ZTV18CSTGMessageContext[12];

int main(void)
{
	printf("CSTGSmoother::CSTGSmoother() known-answer test\n");
	printf("=========================================================\n");

	unsigned char *buf = mmap32(0x10000);
	memset(buf, 0xcc, 0x10000);

	CSTGSmoother *s = new (buf) CSTGSmoother();
	(void)s;

	printf("[1] slot 0: four MessageContext-shaped vtable installs\n");
	check_eq("slot[0]+0x1c == &_ZTV25CSTGProgramMessageContext+8",
		 *(unsigned int *)(buf + 0x1c), ToU32(_ZTV25CSTGProgramMessageContext + 8));
	check_eq("slot[0]+0x38 == &_ZTV29CSTGProgramSlotMessageContext+8",
		 *(unsigned int *)(buf + 0x38), ToU32(_ZTV29CSTGProgramSlotMessageContext + 8));
	check_eq("slot[0]+0x54 == &_ZTV23CSTGPatchMessageContext+8",
		 *(unsigned int *)(buf + 0x54), ToU32(_ZTV23CSTGPatchMessageContext + 8));
	check_eq("slot[0]+0x88 == &_ZTV18CSTGMessageContext+8",
		 *(unsigned int *)(buf + 0x88), ToU32(_ZTV18CSTGMessageContext + 8));

	printf("[2] slot 0: a sample of confirmed scalar constants\n");
	check_eq("slot[0]+0x24 == 1", *(unsigned int *)(buf + 0x24), 1);
	check_eq("slot[0]+0x28 == 6", *(unsigned int *)(buf + 0x28), 6);
	check_eq("slot[0]+0x44 == 5", *(unsigned int *)(buf + 0x44), 5);
	check_eq("slot[0]+0x60 == 4", *(unsigned int *)(buf + 0x60), 4);
	check_eq("slot[0]+0x70 == 0xffffffff", *(unsigned int *)(buf + 0x70), 0xffffffffu);
	check_eq("slot[0]+0x90 == 1", *(unsigned int *)(buf + 0x90), 1);
	check_eq("slot[0]+0xb8 == self (owner back-ref)",
		 *(unsigned int *)(buf + 0xb8), ToU32(buf + 0));
	check_eq("slot[0]+0xb0 == 0 (link)", *(unsigned int *)(buf + 0xb0), 0);
	check_eq("slot[0]+0xb4 == 0 (link)", *(unsigned int *)(buf + 0xb4), 0);

	printf("[3] bit0 of +0x84 explicitly cleared (poisoned 0xcc -> 0xcc&0xfe)\n");
	check_eq("slot[0]+0x84 == 0xcc&0xfe", buf[0x84], 0xcc & 0xfe);

	printf("[4] slot 319 (last of 320): same vtable pattern, own owner back-ref\n");
	unsigned char *last = buf + 319 * 0xc0;
	check_eq("slot[319]+0x1c == &_ZTV25CSTGProgramMessageContext+8",
		 *(unsigned int *)(last + 0x1c), ToU32(_ZTV25CSTGProgramMessageContext + 8));
	check_eq("slot[319]+0xb8 == self", *(unsigned int *)(last + 0xb8), ToU32(last));

	printf("[5] top-level list-management fields all zeroed\n");
	check_eq("+0xf004 == 0", *(unsigned int *)(buf + 0xf004), 0);
	check_eq("+0xf008 == 0", *(unsigned int *)(buf + 0xf008), 0);
	check_eq("+0xf00c == 0", *(unsigned int *)(buf + 0xf00c), 0);
	check_eq("+0xf010 == 0", *(unsigned int *)(buf + 0xf010), 0);
	check_eq("+0xf014 == 0", *(unsigned int *)(buf + 0xf014), 0);
	check_eq("+0xf018 == 0", *(unsigned int *)(buf + 0xf018), 0);

	printf("[6] sInstance == this\n");
	check_eq("sInstance == buf", ToU32(CSTGSmoother::sInstance), ToU32(buf));

	printf("=========================================================\n");
	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
