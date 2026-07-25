// SPDX-License-Identifier: GPL-2.0
/*
 * test_smoother_finalize_all.cpp  -  host-side known-answer test for
 * CSTGSmoother::FinalizeAllSmoothers() (batch 61).
 *
 * Same synthetic 3-node active list technique as test_smoother_cancel.cpp
 * (A <-> B <-> C, A=head, C=tail) -- this function's own unlink/free-list/
 * buffer-zero logic is confirmed identical to CancelAllSmoothers(), so
 * the same list shape exercises it; this test additionally verifies the
 * new DispatchSmoothedValue call each node gets (args + call order +
 * always-true bool), which CancelAllSmoothers() never makes.
 *
 * Own local mock for CSTGSmootherMapping_DispatchSmoothedValue (the
 * project's own established real body lives in bar2_stubs_c.cpp, not
 * linked here) -- matches the smoother_finalize.cpp precedent.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_engine_init.h"

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
static void check_float(const char *label, float got, float want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-60s %f\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %f)\n", want);
}

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

static int g_dispatchCalls;
static void *g_dispatchMapping[8];
static float g_dispatchA[8], g_dispatchB[8];
static bool g_dispatchFlag[8];

extern "C" void CSTGSmootherMapping_DispatchSmoothedValue(void *mapping, float a, float b, bool c)
{
	int i = g_dispatchCalls++;
	if (i < 8) {
		g_dispatchMapping[i] = mapping;
		g_dispatchA[i] = a;
		g_dispatchB[i] = b;
		g_dispatchFlag[i] = c;
	}
}

int main(void)
{
	printf("CSTGSmoother::FinalizeAllSmoothers() known-answer test (batch 61)\n");
	printf("=========================================================\n");

	unsigned char *smoother = mmap32(0x10000);
	memset(smoother, 0, 0x10000);

	unsigned char *buf = mmap32(0x3c00);
	memset(buf, 0xcc, 0x3c00);
	*(unsigned int *)(smoother + 0xf000) = ToU32(buf);

	/* Mapping objects: +0x0 = 16-bit index, +0x4/+0x8 = the two floats
	 * DispatchSmoothedValue() reads. */
	unsigned char *mapA = mmap32(0x10); memset(mapA, 0, 0x10);
	*(unsigned short *)mapA = 5; *(float *)(mapA + 4) = 1.5f; *(float *)(mapA + 8) = -2.5f;
	unsigned char *mapB = mmap32(0x10); memset(mapB, 0, 0x10);
	*(unsigned short *)mapB = 6; *(float *)(mapB + 4) = 3.0f; *(float *)(mapB + 8) = 4.0f;
	unsigned char *mapC = mmap32(0x10); memset(mapC, 0, 0x10);
	*(unsigned short *)mapC = 100; *(float *)(mapC + 4) = 9.0f; *(float *)(mapC + 8) = -1.0f;

	unsigned char *nodeA = mmap32(0x10); memset(nodeA, 0, 0x10);
	unsigned char *nodeB = mmap32(0x10); memset(nodeB, 0, 0x10);
	unsigned char *nodeC = mmap32(0x10); memset(nodeC, 0, 0x10);
	*(unsigned int *)(nodeA + 0x8) = ToU32(mapA);
	*(unsigned int *)(nodeB + 0x8) = ToU32(mapB);
	*(unsigned int *)(nodeC + 0x8) = ToU32(mapC);

	/* Active list: A <-> B <-> C (A=head, C=tail). */
	*(unsigned int *)(nodeA + 0x0) = ToU32(nodeB);
	*(unsigned int *)(nodeA + 0x4) = 0;
	*(unsigned int *)(nodeB + 0x0) = ToU32(nodeC);
	*(unsigned int *)(nodeB + 0x4) = ToU32(nodeA);
	*(unsigned int *)(nodeC + 0x0) = 0;
	*(unsigned int *)(nodeC + 0x4) = ToU32(nodeB);

	*(unsigned int *)(smoother + 0xf010) = ToU32(nodeA);
	*(unsigned int *)(smoother + 0xf014) = ToU32(nodeC);
	*(unsigned int *)(smoother + 0xf018) = 3;

	*(unsigned int *)(smoother + 0xf004) = 0;
	*(unsigned int *)(smoother + 0xf008) = 0;
	*(unsigned int *)(smoother + 0xf00c) = 0;
	*(unsigned int *)(smoother + 0xf01c) = 0xdeadbeef;

	CSTGSmoother *s = (CSTGSmoother *)smoother;
	s->FinalizeAllSmoothers();

	printf("[1] active list fully drained, free list gained all 3 (same shape as CancelAllSmoothers)\n");
	check_eq("activeHead (+0xf010) == 0", *(unsigned int *)(smoother + 0xf010), 0);
	check_eq("activeTail (+0xf014) == 0", *(unsigned int *)(smoother + 0xf014), 0);
	check_eq("activeCount (+0xf018) == 0", *(unsigned int *)(smoother + 0xf018), 0);
	check_eq("freeCount (+0xf00c) == 3", *(unsigned int *)(smoother + 0xf00c), 3);
	check_eq("freeHead (+0xf004) == nodeC (last processed)",
		 *(unsigned int *)(smoother + 0xf004), ToU32(nodeC));
	check_eq("freeTail (+0xf008) == nodeA (first processed)",
		 *(unsigned int *)(smoother + 0xf008), ToU32(nodeA));
	check_eq("+0xf01c cleared", *(unsigned int *)(smoother + 0xf01c), 0);

	printf("[2] the 6 expected interleaved buffer slots were zeroed\n");
	check_eq("idx5 slot0 (offset 0xc8) == 0", *(unsigned int *)(buf + 0xc8), 0);
	check_eq("idx5 slot1 (offset 0xcc) == 0", *(unsigned int *)(buf + 0xcc), 0);
	check_eq("idx6 slot0 (offset 0x120) == 0", *(unsigned int *)(buf + 0x120), 0);
	check_eq("idx6 slot1 (offset 0x124) == 0", *(unsigned int *)(buf + 0x124), 0);
	check_eq("idx100 slot0 (offset 0x12c0) == 0", *(unsigned int *)(buf + 0x12c0), 0);
	check_eq("idx100 slot1 (offset 0x12c4) == 0", *(unsigned int *)(buf + 0x12c4), 0);

	printf("[3] DispatchSmoothedValue() called once per node, in processing order (A,B,C), always flag=true\n");
	check_eq("dispatch called 3 times", g_dispatchCalls, 3);
	check_eq("call 0 mapping == mapA", ToU32(g_dispatchMapping[0]), ToU32(mapA));
	check_float("call 0 a == mapA[0x4] (1.5)", g_dispatchA[0], 1.5f);
	check_float("call 0 b == mapA[0x8] (-2.5)", g_dispatchB[0], -2.5f);
	check_eq("call 0 flag == true", g_dispatchFlag[0], 1);
	check_eq("call 1 mapping == mapB", ToU32(g_dispatchMapping[1]), ToU32(mapB));
	check_float("call 1 a == mapB[0x4] (3.0)", g_dispatchA[1], 3.0f);
	check_eq("call 2 mapping == mapC", ToU32(g_dispatchMapping[2]), ToU32(mapC));
	check_float("call 2 b == mapC[0x8] (-1.0)", g_dispatchB[2], -1.0f);
	check_eq("call 2 flag == true", g_dispatchFlag[2], 1);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
