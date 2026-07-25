// SPDX-License-Identifier: GPL-2.0
/*
 * test_smoother_finalize.cpp  -  host-side known-answer test for
 * CSTGSmoother::FinalizeSmoother(TListLink<CSTGSmootherMapping>*, bool)
 * (batch 57), matching test_smoother_cancel.cpp's own synthetic-list
 * construction style.
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

static unsigned char *mmap32(unsigned long size)
{
	return (unsigned char *)mmap(0, size, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

static int g_dispatchCalls;
static float g_lastA, g_lastB;
static void *g_lastMapping;
extern "C" void CSTGSmootherMapping_DispatchSmoothedValue(void *mapping, float a, float b, bool c)
{
	g_dispatchCalls++;
	g_lastMapping = mapping;
	g_lastA = a;
	g_lastB = b;
	(void)c;
}

int main(void)
{
	printf("CSTGSmoother::FinalizeSmoother() known-answer test\n");
	printf("=======================================================\n");

	unsigned char *smoother = mmap32(0x10000);
	memset(smoother, 0, 0x10000);

	unsigned char *mapA = mmap32(0x10); memset(mapA, 0, 0x10);
	unsigned char *mapB = mmap32(0x10); memset(mapB, 0, 0x10);
	*(float *)(mapB + 0x4) = 1.5f;
	*(float *)(mapB + 0x8) = 2.5f;
	unsigned char *mapC = mmap32(0x10); memset(mapC, 0, 0x10);

	/* Active list A <-> B <-> C, owner = &smoother[0xf010]. */
	unsigned char *nodeA = mmap32(0x10); memset(nodeA, 0, 0x10);
	unsigned char *nodeB = mmap32(0x10); memset(nodeB, 0, 0x10);
	unsigned char *nodeC = mmap32(0x10); memset(nodeC, 0, 0x10);
	unsigned char *activeOwner = smoother + 0xf010;

	*(unsigned int *)(nodeA + 0x0) = ToU32(nodeB);
	*(unsigned int *)(nodeA + 0x4) = 0;
	*(unsigned int *)(nodeA + 0x8) = ToU32(mapA);
	*(unsigned int *)(nodeA + 0xc) = ToU32(activeOwner);

	*(unsigned int *)(nodeB + 0x0) = ToU32(nodeC);
	*(unsigned int *)(nodeB + 0x4) = ToU32(nodeA);
	*(unsigned int *)(nodeB + 0x8) = ToU32(mapB);
	*(unsigned int *)(nodeB + 0xc) = ToU32(activeOwner);

	*(unsigned int *)(nodeC + 0x0) = 0;
	*(unsigned int *)(nodeC + 0x4) = ToU32(nodeB);
	*(unsigned int *)(nodeC + 0x8) = ToU32(mapC);
	*(unsigned int *)(nodeC + 0xc) = ToU32(activeOwner);

	*(unsigned int *)(smoother + 0xf010) = ToU32(nodeA); /* activeHead */
	*(unsigned int *)(smoother + 0xf014) = ToU32(nodeC); /* activeTail */
	*(unsigned int *)(smoother + 0xf018) = 3;             /* activeCount */
	*(unsigned int *)(smoother + 0xf01c) = ToU32(nodeB);  /* cursor == B */

	CSTGSmoother *sm = (CSTGSmoother *)smoother;

	printf("[1] Remove middle node B (cursor==B, flag=false)\n");
	{
		sm->FinalizeSmoother(nodeB, false);

		check_eq("cursor advanced to B->next (C)", *(unsigned int *)(smoother + 0xf01c), ToU32(nodeC));
		check_eq("A->next == C", *(unsigned int *)(nodeA + 0x0), ToU32(nodeC));
		check_eq("C->prev == A", *(unsigned int *)(nodeC + 0x4), ToU32(nodeA));
		check_eq("activeCount decremented to 2", *(unsigned int *)(smoother + 0xf018), 2);
		check_eq("B->next zeroed", *(unsigned int *)(nodeB + 0x0), 0);
		check_eq("B pushed as free head", *(unsigned int *)(smoother + 0xf004), ToU32(nodeB));
		check_eq("B pushed as free tail (was empty)", *(unsigned int *)(smoother + 0xf008), ToU32(nodeB));
		check_eq("freeCount incremented to 1", *(unsigned int *)(smoother + 0xf00c), 1);
		check_eq("B->owner now points at free head slot", *(unsigned int *)(nodeB + 0xc), ToU32(smoother + 0xf004));
		check_eq("DispatchSmoothedValue NOT called (flag=false)", g_dispatchCalls, 0);
	}

	printf("[2] Remove head node A (flag=true -> dispatch)\n");
	{
		sm->FinalizeSmoother(nodeA, true);

		check_eq("activeHead == C", *(unsigned int *)(smoother + 0xf010), ToU32(nodeC));
		check_eq("C->prev == 0", *(unsigned int *)(nodeC + 0x4), 0);
		check_eq("activeCount decremented to 1", *(unsigned int *)(smoother + 0xf018), 1);
		check_eq("A pushed in FRONT of B on free list", *(unsigned int *)(smoother + 0xf004), ToU32(nodeA));
		check_eq("A->next == old free head (B)", *(unsigned int *)(nodeA + 0x0), ToU32(nodeB));
		check_eq("B->prev == A", *(unsigned int *)(nodeB + 0x4), ToU32(nodeA));
		check_eq("freeCount incremented to 2", *(unsigned int *)(smoother + 0xf00c), 2);
	}
	check_eq("DispatchSmoothedValue called once (flag=true)", g_dispatchCalls, 1);
	check_eq("dispatched mapping == mapA", (unsigned int)(unsigned long)g_lastMapping, ToU32(mapA));

	printf("\n%s\n", g_fail ? "FAILED" : "All tests passed");
	return g_fail ? 1 : 0;
}
