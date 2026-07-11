// SPDX-License-Identifier: GPL-2.0
/*
 * test_cpu_affinity.cpp  -  host-side known-answer test for
 * CSTGThread::CreateRealTimeWithCPUAffinity() (see
 * ../include/oa_cpu_affinity.h / ../src/init/cpu_affinity.cpp).
 *
 * Mocks every real RTAI-wrapping helper this method calls, exercising:
 *   [1] the full success path (attr setup, thread creation, debug
 *       traps, CPU pinning, confirmed call ORDER and exact arguments).
 *   [2] rtwrap_pthread_create() itself fails -> returns 0 immediately,
 *       no debug-trap/CPU-pinning calls at all.
 *   [3] thread creation succeeds but debug-trap installation fails ->
 *       tears the thread back down (clear traps + cancel) and returns 0.
 *
 * POLARITY FIX (batch 39): `rtwrap_pthread_create`'s real ground-truth
 * return convention is 0 = success / nonzero = failure (confirmed from
 * BOTH its own real body, src/init/rtwrap.cpp, and its real caller's
 * own disassembly, `.text+0x40a30` -- see cpu_affinity.cpp's header
 * comment for the full derivation). This mock previously returned 0 for
 * "failure" and an opaque nonzero value for "success" -- exactly
 * backwards -- which went undetected because `CreateRealTimeWithCPU
 * Affinity`'s own check was ALSO inverted at the time (two matching
 * bugs canceling out). Both are now fixed together.
 */

#include <cstdio>
#include <cstdint>
#include "oa_cpu_affinity.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-50s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

extern "C" {

static unsigned int g_attrSize = 40; /* an arbitrary but plausible real size */
unsigned int get_sizeof_rtwrap_pthread_attr(void) { return g_attrSize; }

static int g_attrInitCalls;
static void *g_lastAttrInit;
void rtwrap_pthread_attr_init(void *attr) { g_attrInitCalls++; g_lastAttrInit = attr; }

static int g_lastPriority;
void rtwrap_pthread_attr_setrtpriority(void *, int priority) { g_lastPriority = priority; }

static unsigned int g_lastStackSize;
void rtwrap_pthread_attr_setstacksize(void *, unsigned int stackSize) { g_lastStackSize = stackSize; }

static int g_createCalls;
static void *g_lastCreateThis, *g_lastCreateAttr, *g_lastCreateArg;
static void *g_lastEntryFn;
static void *g_createShouldFail; /* non-null "this" for which create fails */
void *rtwrap_pthread_create(void *this_, void *attr, void *(*entryFn)(void *), void *arg)
{
	g_createCalls++;
	g_lastCreateThis = this_;
	g_lastCreateAttr = attr;
	g_lastEntryFn = (void *)entryFn;
	g_lastCreateArg = arg;
	if (this_ == g_createShouldFail)
		return (void *)(long)-1; /* nonzero = failure (real polarity) */
	/* Confirmed real: the actual function populates the caller's own
	 * taskHandle field (+0x0) as a side effect -- CreateRealTimeWith
	 * CPUAffinity reads it back via `this->taskHandle` afterward
	 * rather than using this return value directly (matching the real
	 * disassembly's own `mov eax,[esi]` re-read). Simulated here since
	 * this mock stands in for that real function's own body. */
	((CSTGThread *)this_)->taskHandle = (void *)0x1234;
	return 0; /* 0 = success (real polarity) */
}

static int g_attrDestroyCalls;
void rtwrap_pthread_attr_destroy(void *) { g_attrDestroyCalls++; }

static int g_debugTrapsShouldFail;
static int g_setDebugTrapsCalls;
static void *g_lastSetDebugTrapsHandle;
int rtwrap_set_debug_traps_in_rt_task(void *taskHandle)
{
	g_setDebugTrapsCalls++;
	g_lastSetDebugTrapsHandle = taskHandle;
	return g_debugTrapsShouldFail ? -1 : 0;
}

static int g_setRunnableCalls;
static void *g_lastRunnableHandle;
static unsigned int g_lastRunnableCpu;
void rtwrap_set_runnable_on_cpuid(void *taskHandle, unsigned int cpuId)
{
	g_setRunnableCalls++;
	g_lastRunnableHandle = taskHandle;
	g_lastRunnableCpu = cpuId;
}

static int g_clearDebugTrapsCalls;
void rtwrap_clear_debug_traps_in_rt_task(void *) { g_clearDebugTrapsCalls++; }

static int g_cancelCalls;
void rtwrap_pthread_cancel(void *) { g_cancelCalls++; }

} /* extern "C" */

static void *fake_entry(void *) { return 0; }

static void reset(void)
{
	g_attrInitCalls = g_createCalls = g_attrDestroyCalls = 0;
	g_setDebugTrapsCalls = g_setRunnableCalls = 0;
	g_clearDebugTrapsCalls = g_cancelCalls = 0;
	g_debugTrapsShouldFail = 0;
	g_createShouldFail = 0;
}

int main(void)
{
	CSTGThread thread;
	thread.taskHandle = 0;
	thread.debugTrapsInstalled = 0;

	printf("[1] full success path:\n");
	reset();
	char rc = thread.CreateRealTimeWithCPUAffinity(fake_entry, 42, 3, (void *)0xbeef);
	check_eq("return value (success)", rc, 1);
	check_eq("attr_init called once", g_attrInitCalls, 1);
	check_eq("rtwrap_pthread_create called once", g_createCalls, 1);
	check_eq("create() this == &thread", (long)(g_lastCreateThis == &thread), 1);
	check_eq("create() attr matches attr_init's", (long)(g_lastCreateAttr == g_lastAttrInit), 1);
	check_eq("create() entryFn == fake_entry", (long)(g_lastEntryFn == (void *)&fake_entry), 1);
	check_eq("create() arg == 0xbeef", (long)(long)(intptr_t)g_lastCreateArg, 0xbeef);
	check_eq("priority == 42", g_lastPriority, 42);
	check_eq("stack size == 0x5000 (confirmed literal)", (long)g_lastStackSize, 0x5000);
	check_eq("attr_destroy called once (regardless of outcome)", g_attrDestroyCalls, 1);
	check_eq("debugTrapsInstalled flag set", thread.debugTrapsInstalled, 1);
	check_eq("set_debug_traps called with the real task handle", (long)(intptr_t)g_lastSetDebugTrapsHandle, 0x1234);
	check_eq("set_runnable_on_cpuid called once", g_setRunnableCalls, 1);
	check_eq("set_runnable cpuId == 3", g_lastRunnableCpu, 3);
	check_eq("no cleanup calls on success", g_clearDebugTrapsCalls + g_cancelCalls, 0);

	printf("\n[2] rtwrap_pthread_create() itself fails:\n");
	reset();
	thread.taskHandle = 0;
	thread.debugTrapsInstalled = 0;
	g_createShouldFail = &thread;
	rc = thread.CreateRealTimeWithCPUAffinity(fake_entry, 1, 0, 0);
	check_eq("return value (failure)", rc, 0);
	check_eq("attr_destroy still called", g_attrDestroyCalls, 1);
	check_eq("no debug-trap call at all", g_setDebugTrapsCalls, 0);
	check_eq("no CPU-pinning call at all", g_setRunnableCalls, 0);
	check_eq("debugTrapsInstalled left at 0", thread.debugTrapsInstalled, 0);

	printf("\n[3] thread created, but debug-trap installation fails -> torn down:\n");
	reset();
	thread.taskHandle = 0;
	thread.debugTrapsInstalled = 0;
	g_debugTrapsShouldFail = 1;
	rc = thread.CreateRealTimeWithCPUAffinity(fake_entry, 1, 0, 0);
	check_eq("return value (failure)", rc, 0);
	check_eq("set_debug_traps was attempted", g_setDebugTrapsCalls, 1);
	check_eq("no CPU-pinning call (never reached)", g_setRunnableCalls, 0);
	check_eq("clear_debug_traps called once (teardown)", g_clearDebugTrapsCalls, 1);
	check_eq("pthread_cancel called once (teardown)", g_cancelCalls, 1);
	check_eq("debugTrapsInstalled reset to 0", thread.debugTrapsInstalled, 0);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
