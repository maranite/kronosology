// SPDX-License-Identifier: GPL-2.0
/*
 * test_smoother_cancel.cpp  -  host-side known-answer test for
 * CSTGSmoother::CancelAllSmoothers() (sec 10.154).
 *
 * Builds a minimal 3-node active list by hand (A <-> B <-> C, A=head,
 * C=tail) rather than a real CSTGSmoother::Initialize()-built 320-entry
 * free list -- CancelAllSmoothers() never touches the mapping objects'
 * own array positions or strides, only pointers it's handed, so a small
 * synthetic list exercises the exact same code paths. Each node's own
 * "owning mapping" (+0x8) is a tiny standalone object holding just the
 * confirmed real 16-bit index field.
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

int main(void)
{
	printf("CSTGSmoother::CancelAllSmoothers() known-answer test\n");
	printf("=========================================================\n");

	unsigned char *smoother = mmap32(0x10000);
	memset(smoother, 0, 0x10000);

	/* +0xf000's own real buffer: CSTGBankMemory::AllocAligned(0x3c00,
	 * 0x10), confirmed sec 10.86. Poisoned (not zeroed) so the two
	 * confirmed-zeroed slots per cancelled node can be told apart from
	 * everything else in the buffer. */
	unsigned char *buf = mmap32(0x3c00);
	memset(buf, 0xcc, 0x3c00);
	*(unsigned int *)(smoother + 0xf000) = ToU32(buf);

	/* Three tiny standalone "mapping" objects -- only the confirmed
	 * real 16-bit index field at +0x0 matters here. */
	unsigned char *mapA = mmap32(0x10); memset(mapA, 0, 0x10); *(unsigned short *)mapA = 5;
	unsigned char *mapB = mmap32(0x10); memset(mapB, 0, 0x10); *(unsigned short *)mapB = 6;
	unsigned char *mapC = mmap32(0x10); memset(mapC, 0, 0x10); *(unsigned short *)mapC = 100;

	/* Three TListLink-shaped nodes (+0x0 next, +0x4 prev, +0x8 owning
	 * mapping pointer, +0xc list-owner -- the same 4-field shape
	 * test_smoother_init.cpp's own KAT already confirmed real for the
	 * embedded link sub-object at mapping+0xb0). */
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

	*(unsigned int *)(smoother + 0xf010) = ToU32(nodeA); /* activeHead */
	*(unsigned int *)(smoother + 0xf014) = ToU32(nodeC); /* activeTail */
	*(unsigned int *)(smoother + 0xf018) = 3;             /* activeCount */

	*(unsigned int *)(smoother + 0xf004) = 0; /* freeHead */
	*(unsigned int *)(smoother + 0xf008) = 0; /* freeTail */
	*(unsigned int *)(smoother + 0xf00c) = 0; /* freeCount */
	*(unsigned int *)(smoother + 0xf01c) = 0xdeadbeef; /* poison, must -> 0 */

	CSTGSmoother *s = (CSTGSmoother *)smoother;
	s->CancelAllSmoothers();

	printf("[1] active list fully drained\n");
	check_eq("activeHead (+0xf010) == 0", *(unsigned int *)(smoother + 0xf010), 0);
	check_eq("activeTail (+0xf014) == 0", *(unsigned int *)(smoother + 0xf014), 0);
	check_eq("activeCount (+0xf018) == 0", *(unsigned int *)(smoother + 0xf018), 0);

	printf("[2] free list gained all 3 nodes, push-front order\n");
	check_eq("freeCount (+0xf00c) == 3", *(unsigned int *)(smoother + 0xf00c), 3);
	/* Processed in order A, B, C (always removing the current active
	 * head); each push-FRONTs onto the free list, so the LAST processed
	 * (C) ends up as freeHead, and the FIRST processed (A) -- whose own
	 * push saw an empty free list -- keeps the permanent freeTail slot,
	 * matching Initialize()'s own confirmed "first insertion == tail"
	 * shape. */
	check_eq("freeHead (+0xf004) == nodeC (last processed)",
		 *(unsigned int *)(smoother + 0xf004), ToU32(nodeC));
	check_eq("freeTail (+0xf008) == nodeA (first processed)",
		 *(unsigned int *)(smoother + 0xf008), ToU32(nodeA));

	printf("[3] free list is walkable C -> B -> A -> null, each owner points at +0xf004\n");
	check_eq("nodeC.next == nodeB", *(unsigned int *)(nodeC + 0x0), ToU32(nodeB));
	check_eq("nodeB.next == nodeA", *(unsigned int *)(nodeB + 0x0), ToU32(nodeA));
	check_eq("nodeA.next == 0 (list end)", *(unsigned int *)(nodeA + 0x0), 0);
	check_eq("nodeA.owner (+0xc) == &(+0xf004)", *(unsigned int *)(nodeA + 0xc), ToU32(smoother + 0xf004));
	check_eq("nodeB.owner (+0xc) == &(+0xf004)", *(unsigned int *)(nodeB + 0xc), ToU32(smoother + 0xf004));
	check_eq("nodeC.owner (+0xc) == &(+0xf004)", *(unsigned int *)(nodeC + 0xc), ToU32(smoother + 0xf004));

	printf("[4] +0xf01c unconditionally cleared\n");
	check_eq("+0xf01c == 0", *(unsigned int *)(smoother + 0xf01c), 0);

	printf("[5] exactly the 6 expected interleaved buffer slots were zeroed (idx 5,6,100 x2 each)\n");
	/* slot = (logicalIdx>>2)*0x60 + (logicalIdx&3)*4, logicalIdx = mappingIdx*2 [+1] */
	check_eq("idx5 slot0 (logicalIdx=10, offset 0xc8) == 0", *(unsigned int *)(buf + 0xc8), 0);
	check_eq("idx5 slot1 (logicalIdx=11, offset 0xcc) == 0", *(unsigned int *)(buf + 0xcc), 0);
	check_eq("idx6 slot0 (logicalIdx=12, offset 0x120) == 0", *(unsigned int *)(buf + 0x120), 0);
	check_eq("idx6 slot1 (logicalIdx=13, offset 0x124) == 0", *(unsigned int *)(buf + 0x124), 0);
	check_eq("idx100 slot0 (logicalIdx=200, offset 0x12c0) == 0", *(unsigned int *)(buf + 0x12c0), 0);
	check_eq("idx100 slot1 (logicalIdx=201, offset 0x12c4) == 0", *(unsigned int *)(buf + 0x12c4), 0);
	check_eq("unrelated slot just before idx5's row untouched (still poisoned)",
		 *(unsigned int *)(buf + 0xc4), 0xcccccccc);
	check_eq("unrelated slot just after idx5's used pair untouched (still poisoned)",
		 *(unsigned int *)(buf + 0xd0), 0xcccccccc);
	check_eq("unrelated slot between idx6 and idx100 untouched (still poisoned)",
		 *(unsigned int *)(buf + 0x1000), 0xcccccccc);

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
