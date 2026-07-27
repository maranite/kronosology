/*
 * test_job_stack.cpp  -  host-side known-answer test for CJobStack's
 * construction/destruction-only reconstruction (include/job_stack.h,
 * src/editor/job_stack.cpp -- Eva "size is not depth" re-check, 2026-07-27).
 *
 * Checks:
 *   [1] CJobStack_Construct(): final vtables installed at +0x00/+0x08,
 *       empty job-queue TVector<CRMJob> triple (+0x0c/+0x10/+0x14 all NULL),
 *       owned CRMJob (+0x04) non-NULL with real CRMJob ctor sentinel fields
 *       readable at its own known offsets (rm_job.h).
 *   [2] PTR__CJobStack_08e88608 slot [1] (D0) really is CJobStack_DeletingDtor
 *       -- the function CRMApiInstance::PostKernelDestructor's opaque
 *       vtable+4 dispatch (mains.cpp) actually calls.
 *   [3] CJobStack_DeletingDtor(): doesn't crash on a freshly constructed
 *       object (frees the owned CRMJob + the CJobStack block itself).
 *   [4] Full round trip via the real opaque-dispatch idiom itself (read
 *       vtbl, call vtbl[1] as a plain `void(*)(void*)`) -- exactly
 *       mirrors what CRMApiInstance_PostKernelDestructor does with
 *       RMApiInstance+0x24 -- doesn't crash.
 */

#include <cstdio>
#include <cstdlib>

#include "job_stack.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static void   *P(const void *obj, int off) { return *(void **)((const char *)obj + off); }
static int     I32(const void *obj, int off) { return *(const int *)((const char *)obj + off); }
static unsigned char U8(const void *obj, int off) { return *((const unsigned char *)obj + off); }

typedef void (*OpaqueDtorFn)(void *);

int main()
{
	printf("CJobStack construction/destruction known-answer test\n");
	printf("=======================================================\n");

	printf("[1] CJobStack_Construct()\n");
	{
		void *self = malloc(0x18);
		CJobStack_Construct(self);

		check("+0x00 == PTR__CJobStack_08e88608 (own final vtable)",
		      P(self, 0x00) == (void *)PTR__CJobStack_08e88608);
		check("+0x08 == PTR__CJobStack_secondary_08e886c0 (secondary vtable)",
		      P(self, 0x08) == (void *)PTR__CJobStack_secondary_08e886c0);
		check("+0x0c == NULL (empty job-queue vector begin)", P(self, 0x0c) == 0);
		check("+0x10 == NULL (empty job-queue vector end)", P(self, 0x10) == 0);
		check("+0x14 == NULL (empty job-queue vector cap-end)", P(self, 0x14) == 0);

		void *job = P(self, 0x04);
		check("+0x04 (mJob) non-NULL", job != 0);
		check("mJob->+0x40..+0x42 == 0xff (CRMJob ctor sentinel triple)",
		      U8(job, 0x40) == 0xff && U8(job, 0x41) == 0xff && U8(job, 0x42) == 0xff);
		check("mJob->+0x44 == -1 (CRMJob ctor)", I32(job, 0x44) == -1);
		check("mJob->+0x48 == 0 (CRMJob ctor)", I32(job, 0x48) == 0);
		check("mJob->+0x50 == 1 (CRMJob ctor)", I32(job, 0x50) == 1);

		printf("[2] vtable slot [1] (D0) identity\n");
		check("PTR__CJobStack_08e88608[1] == CJobStack_DeletingDtor",
		      PTR__CJobStack_08e88608[1] == (void *)CJobStack_DeletingDtor);

		printf("[3] CJobStack_DeletingDtor() direct call\n");
		CJobStack_DeletingDtor(self);
		check("did not crash (owned CRMJob + CJobStack block freed)", true);
	}

	printf("[4] Full opaque-dispatch round trip (mirrors "
	       "CRMApiInstance::PostKernelDestructor's own self+0x24 dispatch)\n");
	{
		void *self = malloc(0x18);
		CJobStack_Construct(self);

		void *vtbl = P(self, 0x00);
		OpaqueDtorFn fn = *(OpaqueDtorFn *)((char *)vtbl + 4);
		fn(self);
		check("opaque vtbl+4 dispatch reached CJobStack_DeletingDtor without crashing", true);
	}

	if (g_fail == 0)
		printf("\nall checks passed\n");
	else
		printf("\n%d check(s) FAILED\n", g_fail);

	return g_fail == 0 ? 0 : 1;
}
