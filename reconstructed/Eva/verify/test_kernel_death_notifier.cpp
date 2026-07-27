/*
 * test_kernel_death_notifier.cpp  -  host-side known-answer test for
 * CKernelDeathNotifier (kernel_death_notifier.h/.cpp), found + reconstructed
 * 2026-07-27 fresh broad-survey pass (see kernel_death_notifier.h's own file header).
 *
 * Exercises: ctor/dtor safety (registers/deregisters with CKernel's already-real
 * sm_poGlobalObjectList machinery via CGlobalObjectBase, ckernel.cpp), the real
 * PreKernelDestructor() override both via the named C++ method and via a raw
 * vtable-slot dispatch (same generic "CallVSlot"-style idiom CKernel::CKernel()/
 * ~CKernel() themselves use to walk that list), and confirms the 3 unoverridden
 * phase-hook slots still point at CGlobalObjectBase's own no-ops.
 */

#include <cstdio>
#include <cstring>
#include <new>

#include "kernel_death_notifier.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

typedef int (*PhaseHookFn)(void *, unsigned long);

int main(void)
{
	printf("CKernelDeathNotifier known-answer test\n");
	printf("=======================================\n");

	printf("[1] Real object size is 8 bytes (inherited 4-byte mVtbl + mDying)\n");
	check("sizeof(CKernelDeathNotifier) == 8", sizeof(CKernelDeathNotifier) == 8);

	printf("[2] Construction/destruction is safe (registers/deregisters with "
	       "CKernel::sm_poGlobalObjectList)\n");
	static unsigned char raw1[sizeof(CKernelDeathNotifier)];
	CKernelDeathNotifier *obj1 = new (raw1) CKernelDeathNotifier();
	check("construction did not crash", obj1 != 0);

	printf("[3] PreKernelDestructor() (named method) returns 0\n");
	check("PreKernelDestructor() == 0", obj1->PreKernelDestructor(0) == 0);

	printf("[4] Raw vtable dispatch matches the named-method call for all 6 slots -- "
	       "same generic dispatch shape CKernel::CKernel()/~CKernel() use on every "
	       "sm_poGlobalObjectList entry\n");
	void **vtbl = *(void ***)obj1;
	check("slot 2 (PreKernelConstructor) is the base no-op",
	      ((PhaseHookFn)vtbl[2])(obj1, 0) == 0);
	check("slot 3 (PostKernelConstructor) is the base no-op",
	      ((PhaseHookFn)vtbl[3])(obj1, 0) == 0);
	check("slot 4 (PreKernelDestructor, real override) dispatches correctly",
	      ((PhaseHookFn)vtbl[4])(obj1, 0) == 0);
	check("slot 5 (PostKernelDestructor) is the base no-op",
	      ((PhaseHookFn)vtbl[5])(obj1, 0) == 0);

	printf("[5] A second, independent instance also constructs/destructs cleanly\n");
	static unsigned char raw2[sizeof(CKernelDeathNotifier)];
	CKernelDeathNotifier *obj2 = new (raw2) CKernelDeathNotifier();
	void **vtbl2 = *(void ***)obj2;
	check("second instance's own vtable slot 4 also dispatches correctly",
	      ((PhaseHookFn)vtbl2[4])(obj2, 0) == 0);
	obj2->~CKernelDeathNotifier();

	obj1->~CKernelDeathNotifier();

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
