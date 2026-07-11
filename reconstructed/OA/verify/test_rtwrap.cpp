// SPDX-License-Identifier: GPL-2.0
/*
 * test_rtwrap.cpp  -  host-side known-answer test for the `rtwrap_*`
 * RTAI wrapper layer (src/init/rtwrap.cpp, batch 37).
 *
 * Links src/init/rtwrap.cpp directly (a brand-new TU no other verify/
 * file links, so this test's own local mocks of rt_whoami / rt_sem_* /
 * rt_task_* / rt_*_irq / rtheap_free / rtai_global_heap can't collide
 * with anything else -- these real RTAI primitives are not implemented
 * anywhere in this reconstruction, on real hardware they come from
 * rtai_hal.ko/rtai_sched.ko/rtai_lxrt.ko at insmod time).
 */

#include <cstdio>
#include <cstdint>
#include <cstring>

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}
static void check_ptr(const char *label, const void *got, const void *want)
{
	if (got == want) { printf("  ok    %-50s %p\n", label, got); return; }
	printf("  FAIL  %-50s got=%p want=%p\n", label, got, want);
	g_fail++;
}

/* ---- Recording mocks for every real RTAI primitive rtwrap.cpp forwards to ---- */
extern "C" {

unsigned char rtai_global_heap[1];

static int g_semWaitIfReturn;
static int g_rtWhoamiCalls;
static void *g_rtWhoamiReturn;
static int g_taskDeleteCalls;
static void *g_lastTaskDeleted;
static int g_taskSuspendCalls;
static void *g_lastTaskSuspended;
static int g_semWaitCalls;
static void *g_lastSemWaited;
static int g_semWaitIfCalls;
static void *g_lastSemWaitedIf;
static int g_semSignalCalls;
static void *g_lastSemSignaled;
static int g_semDeleteCalls;
static void *g_lastSemDeleted;
static int g_typedSemInitCalls;
static void *g_lastSemInit;
static int g_lastSemInitValue;
static int g_lastSemInitType;
static int g_setRunnableCalls;
static void *g_lastRunnableTask;
static unsigned int g_lastRunnableCpu;
static int g_clearDebugTrapsCalls;
static void *g_lastClearDebugTrapsTask;
static int g_shutdownIrqCalls, g_releaseIrqCalls, g_assignIrqCalls, g_startupIrqCalls;
static unsigned int g_lastShutdownIrq, g_lastReleaseIrq, g_lastAssignIrq, g_lastAssignCpu, g_lastStartupIrq;
static int g_rtheapFreeCalls;
static void *g_lastRtheapFreeHeap;
static void *g_lastRtheapFreePtr;

void *rt_whoami(void) { g_rtWhoamiCalls++; return g_rtWhoamiReturn; }
int rt_task_delete(void *task) { g_taskDeleteCalls++; g_lastTaskDeleted = task; return 0; }
int rt_task_suspend(void *task) { g_taskSuspendCalls++; g_lastTaskSuspended = task; return 0; }
int rt_sem_wait(void *sem) { g_semWaitCalls++; g_lastSemWaited = sem; return 1; }
int rt_sem_wait_if(void *sem) { g_semWaitIfCalls++; g_lastSemWaitedIf = sem; return g_semWaitIfReturn; }
int rt_sem_signal(void *sem) { g_semSignalCalls++; g_lastSemSignaled = sem; return 0; }
int rt_sem_delete(void *sem) { g_semDeleteCalls++; g_lastSemDeleted = sem; return 0; }
int rt_typed_sem_init(void *sem, int value, int type)
{
	g_typedSemInitCalls++;
	g_lastSemInit = sem;
	g_lastSemInitValue = value;
	g_lastSemInitType = type;
	return 0;
}
void rt_set_runnable_on_cpuid(void *task, unsigned int cpuId)
{
	g_setRunnableCalls++;
	g_lastRunnableTask = task;
	g_lastRunnableCpu = cpuId;
}
void clear_debug_traps_in_rt_task(void *task)
{
	g_clearDebugTrapsCalls++;
	g_lastClearDebugTrapsTask = task;
}
int rt_shutdown_irq(unsigned int irq) { g_shutdownIrqCalls++; g_lastShutdownIrq = irq; return 0; }
int rt_release_irq(unsigned int irq) { g_releaseIrqCalls++; g_lastReleaseIrq = irq; return 0; }
int rt_assign_irq_to_cpu(unsigned int irq, unsigned int cpu)
{
	g_assignIrqCalls++;
	g_lastAssignIrq = irq;
	g_lastAssignCpu = cpu;
	return 0;
}
int rt_startup_irq(unsigned int irq) { g_startupIrqCalls++; g_lastStartupIrq = irq; return 0; }
void rtheap_free(void *heap, void *ptr)
{
	g_rtheapFreeCalls++;
	g_lastRtheapFreeHeap = heap;
	g_lastRtheapFreePtr = ptr;
}

/* Declarations of the functions under test (matching rtwrap.cpp exactly). */
void rtwrap_shutdown_irq(unsigned int irq);
void rtwrap_release_irq(unsigned int irq);
void rtwrap_assign_irq_to_cpu(unsigned int irq, unsigned int cpu);
void rtwrap_startup_irq(unsigned int irq);
void rtwrap_set_runnable_on_cpuid(void *taskHandle, unsigned int cpuId);
void rtwrap_clear_debug_traps_in_rt_task(void *taskHandle);
void rtwrap_free(void *ptr);
void rtwrap_pthread_mutex_init(void *mutex, void *attr);
void rtwrap_pthread_mutex_destroy(void *mutex);
void rtwrap_pthread_mutex_lock(void *mutex);
void rtwrap_pthread_mutex_unlock(void *mutex);
void rtwrap_pthread_mutexattr_init(void *attr);
void rtwrap_pthread_mutexattr_settype(void *attr, int type);
void rtwrap_pthread_mutexattr_destroy(void *attr);
void rtwrap_pthread_cond_init(void *cond, void *attr);
void rtwrap_pthread_attr_init(void *attr);
void rtwrap_pthread_attr_setrtpriority(void *attr, int priority);
void rtwrap_pthread_attr_setstacksize(void *attr, unsigned int stackSize);
void rtwrap_pthread_attr_destroy(void *attr);
void *rtwrap_whoami(void);
void rtwrap_task_suspend(void *task);
void rtwrap_pthread_cancel(void *taskHandle);

} /* extern "C" */

int main(void)
{
	printf("rtwrap_* known-answer test\n");
	printf("===========================\n");

	unsigned char dummyMutex, dummyCond;

	printf("\n[1] rtwrap_free -> rtheap_free(&rtai_global_heap, ptr)\n");
	{
		rtwrap_free(&dummyMutex);
		check_eq("rtheap_free called once", g_rtheapFreeCalls, 1);
		check_ptr("heap == &rtai_global_heap", g_lastRtheapFreeHeap, &rtai_global_heap);
		check_ptr("ptr == argument", g_lastRtheapFreePtr, &dummyMutex);
	}

	printf("\n[2] rtwrap_pthread_mutex_init mode derivation\n");
	{
		rtwrap_pthread_mutex_init(&dummyMutex, 0);
		check_eq("attr==NULL -> mode 0", g_lastSemInitValue, 0);
		check_eq("type arg always 3", g_lastSemInitType, 3);
		check_ptr("sem == mutex ptr", g_lastSemInit, &dummyMutex);

		unsigned int attrNormal = 1;   /* bit0 set */
		rtwrap_pthread_mutex_init(&dummyMutex, &attrNormal);
		check_eq("bit0(normal) set -> mode 0", g_lastSemInitValue, 0);

		unsigned int attrRecursive = 4;  /* bit2 set, bit0/1 clear */
		rtwrap_pthread_mutex_init(&dummyMutex, &attrRecursive);
		check_eq("bit2(recursive) set -> mode 1", g_lastSemInitValue, 1);

		unsigned int attrErrorcheck = 2;  /* bit1 set, bit0/2 clear */
		rtwrap_pthread_mutex_init(&dummyMutex, &attrErrorcheck);
		check_eq("bit1(errorcheck) set -> mode -1", g_lastSemInitValue, -1);
	}

	printf("\n[3] rtwrap_pthread_mutex_destroy: rt_sem_wait_if gates signal+delete\n");
	{
		g_semSignalCalls = g_semDeleteCalls = 0;
		g_semWaitIfReturn = 0;   /* >= 0 -> proceed */
		rtwrap_pthread_mutex_destroy(&dummyMutex);
		check_eq("wait_if>=0: sem_signal called", g_semSignalCalls, 1);
		check_eq("wait_if>=0: sem_delete called", g_semDeleteCalls, 1);

		g_semSignalCalls = g_semDeleteCalls = 0;
		g_semWaitIfReturn = -1;  /* < 0 -> skip both */
		rtwrap_pthread_mutex_destroy(&dummyMutex);
		check_eq("wait_if<0: sem_signal NOT called", g_semSignalCalls, 0);
		check_eq("wait_if<0: sem_delete NOT called", g_semDeleteCalls, 0);
	}

	printf("\n[4] rtwrap_pthread_mutex_lock/unlock -- direct rt_sem_wait/signal forward\n");
	{
		rtwrap_pthread_mutex_lock(&dummyMutex);
		check_eq("rt_sem_wait called once", g_semWaitCalls, 1);
		check_ptr("...on the mutex ptr", g_lastSemWaited, &dummyMutex);

		rtwrap_pthread_mutex_unlock(&dummyMutex);
		/* [3]'s own last sub-case reset g_semSignalCalls to 0 right before
		 * running its wait_if<0 (skip) branch, so this call is the only
		 * one contributing to the counter at this point. */
		check_eq("rt_sem_signal called", g_semSignalCalls, 1);
		check_ptr("...on the mutex ptr", g_lastSemSignaled, &dummyMutex);
	}

	printf("\n[5] rtwrap_pthread_mutexattr_init -> *attr = 1\n");
	{
		unsigned int attr = 0xcccccccc;
		rtwrap_pthread_mutexattr_init(&attr);
		check_eq("attr == 1", attr, 1);
	}

	printf("\n[6] rtwrap_pthread_mutexattr_settype: (flags&~7)|bit, high bits preserved\n");
	{
		unsigned int attr = 0x18 | 0x7;  /* high marker bits 0x18 + garbage low 3 bits */
		rtwrap_pthread_mutexattr_settype(&attr, 0);
		check_eq("type0 -> (attr&~7)|1, high bits kept", attr, 0x18 | 1);

		attr = 0x18 | 0x7;
		rtwrap_pthread_mutexattr_settype(&attr, 1);
		check_eq("type1 -> (attr&~7)|4 (recursive)", attr, 0x18 | 4);

		attr = 0x18 | 0x7;
		rtwrap_pthread_mutexattr_settype(&attr, 2);
		check_eq("type2 -> (attr&~7)|2 (errorcheck)", attr, 0x18 | 2);
	}

	printf("\n[7] rtwrap_pthread_mutexattr_destroy -- confirmed true no-op\n");
	{
		unsigned int attr = 0x42;
		rtwrap_pthread_mutexattr_destroy(&attr);
		check_eq("attr untouched", attr, 0x42);
	}

	printf("\n[8] rtwrap_pthread_cond_init -- attr NEVER dereferenced, always (cond,0,1)\n");
	{
		g_typedSemInitCalls = 0;
		rtwrap_pthread_cond_init(&dummyCond, 0);
		check_eq("sem==cond", (long)(g_lastSemInit == &dummyCond), 1);
		check_eq("value==0", g_lastSemInitValue, 0);
		check_eq("type==1", g_lastSemInitType, 1);

		/* Pass a garbage (unreadable-if-dereferenced-wrong) attr pointer --
		 * if the real function ever dereferenced it this would still be
		 * safe on host (valid stack address), but the call-order/argument
		 * check above already proves attr's VALUE never reaches
		 * rt_typed_sem_init, matching the real "attr ignored" quirk. */
		unsigned int notReallyAnAttr = 0xdeadbeef;
		rtwrap_pthread_cond_init(&dummyCond, &notReallyAnAttr);
		check_eq("still value==0 regardless of attr", g_lastSemInitValue, 0);
		check_eq("still type==1 regardless of attr", g_lastSemInitType, 1);
	}

	printf("\n[9] rtwrap_pthread_attr_init -- four fixed dwords\n");
	{
		unsigned int attr[4] = { 0xcccccccc, 0xcccccccc, 0xcccccccc, 0xcccccccc };
		rtwrap_pthread_attr_init(attr);
		check_eq("attr[0] == 0x2000", attr[0], 0x2000);
		check_eq("attr[1] == 1", attr[1], 1);
		check_eq("attr[2] == 0xf4240", attr[2], 0xf4240);
		check_eq("attr[3] == 1", attr[3], 1);
	}

	printf("\n[10] rtwrap_pthread_attr_setrtpriority -- valid [1..140], attr[1] always 1\n");
	{
		unsigned int attr[4] = { 0, 0xcccccccc, 0, 0xcccccccc };
		rtwrap_pthread_attr_setrtpriority(attr, 1);
		check_eq("priority=1: attr[1]=1", attr[1], 1);
		check_eq("priority=1: attr[3]=140-1=139", attr[3], 139);

		attr[3] = 0xcccccccc;
		rtwrap_pthread_attr_setrtpriority(attr, 140);
		check_eq("priority=140: attr[3]=0", attr[3], 0);

		attr[3] = 0xcccccccc;
		rtwrap_pthread_attr_setrtpriority(attr, 0);
		check_eq("priority=0 (invalid): attr[3] untouched", attr[3], 0xcccccccc);

		attr[3] = 0xcccccccc;
		rtwrap_pthread_attr_setrtpriority(attr, 141);
		check_eq("priority=141 (invalid): attr[3] untouched", attr[3], 0xcccccccc);
	}

	printf("\n[11] rtwrap_pthread_attr_setstacksize -- overwrites attr[0] (real quirk)\n");
	{
		unsigned int attr[4] = { 0x2000, 1, 0xf4240, 1 };
		rtwrap_pthread_attr_setstacksize(attr, 0x5000);
		check_eq("attr[0] overwritten with stack size", attr[0], 0x5000);
		check_eq("attr[1] untouched", attr[1], 1);
	}

	printf("\n[12] rtwrap_pthread_attr_destroy -- confirmed true no-op\n");
	{
		unsigned int attr[4] = { 1, 2, 3, 4 };
		rtwrap_pthread_attr_destroy(attr);
		check_eq("attr untouched", (long)(attr[0] == 1 && attr[1] == 2 && attr[2] == 3 && attr[3] == 4), 1);
	}

	printf("\n[13] rtwrap_set_runnable_on_cpuid / rtwrap_clear_debug_traps_in_rt_task\n");
	{
		unsigned char task;
		rtwrap_set_runnable_on_cpuid(&task, 2);
		check_eq("rt_set_runnable_on_cpuid called", g_setRunnableCalls, 1);
		check_ptr("...task", g_lastRunnableTask, &task);
		check_eq("...cpuId==2", g_lastRunnableCpu, 2);

		rtwrap_clear_debug_traps_in_rt_task(&task);
		check_eq("clear_debug_traps_in_rt_task called", g_clearDebugTrapsCalls, 1);
		check_ptr("...task", g_lastClearDebugTrapsTask, &task);
	}

	printf("\n[14] rtwrap_whoami / rtwrap_task_suspend\n");
	{
		unsigned char fakeTask;
		g_rtWhoamiReturn = &fakeTask;
		void *me = rtwrap_whoami();
		check_eq("rt_whoami called once", g_rtWhoamiCalls, 1);
		check_ptr("returns rt_whoami's own return value", me, &fakeTask);

		rtwrap_task_suspend(me);
		check_eq("rt_task_suspend called once", g_taskSuspendCalls, 1);
		check_ptr("...with the SAME task handle (self-suspend idiom)", g_lastTaskSuspended, &fakeTask);
	}

	printf("\n[15] rtwrap_pthread_cancel -- explicit handle vs. NULL->rt_whoami() self-cancel\n");
	{
		/* +0x5b8 raw-alloc-pointer field, per rtwrap_pthread_create's own
		 * confirmed real layout -- allocate a buffer at least that big
		 * (0x600, matching the real rtheap_alloc request size) plus room
		 * for an 8-byte host pointer read at the tail of that offset. */
		unsigned char taskBuf[0x600];
		memset(taskBuf, 0xcc, sizeof(taskBuf));
		unsigned char rawAllocMarker;
		*(void **)(taskBuf + 0x5b8) = &rawAllocMarker;

		g_taskDeleteCalls = g_rtheapFreeCalls = g_rtWhoamiCalls = 0;
		rtwrap_pthread_cancel(taskBuf);
		check_eq("explicit handle: rt_whoami NOT called", g_rtWhoamiCalls, 0);
		check_eq("explicit handle: rt_task_delete called once", g_taskDeleteCalls, 1);
		check_ptr("...on the given handle", g_lastTaskDeleted, taskBuf);
		check_eq("rtheap_free called once", g_rtheapFreeCalls, 1);
		check_ptr("...heap == &rtai_global_heap", g_lastRtheapFreeHeap, &rtai_global_heap);
		check_ptr("...ptr == the +0x5b8 marker", g_lastRtheapFreePtr, &rawAllocMarker);

		unsigned char selfTaskBuf[0x600];
		memset(selfTaskBuf, 0xcc, sizeof(selfTaskBuf));
		unsigned char selfRawAllocMarker;
		*(void **)(selfTaskBuf + 0x5b8) = &selfRawAllocMarker;
		g_rtWhoamiReturn = selfTaskBuf;

		g_taskDeleteCalls = g_rtheapFreeCalls = g_rtWhoamiCalls = 0;
		rtwrap_pthread_cancel(0);
		check_eq("NULL handle: rt_whoami called once", g_rtWhoamiCalls, 1);
		check_eq("NULL handle: rt_task_delete called once", g_taskDeleteCalls, 1);
		check_ptr("...on rt_whoami()'s own return", g_lastTaskDeleted, selfTaskBuf);
		check_ptr("...ptr == the self task's +0x5b8 marker", g_lastRtheapFreePtr, &selfRawAllocMarker);
	}

	printf("\n[16] irq quartet -- direct forwards\n");
	{
		rtwrap_shutdown_irq(5);
		check_eq("rt_shutdown_irq(5)", g_lastShutdownIrq, 5);
		rtwrap_release_irq(6);
		check_eq("rt_release_irq(6)", g_lastReleaseIrq, 6);
		rtwrap_assign_irq_to_cpu(7, 1);
		check_eq("rt_assign_irq_to_cpu(7,_)", g_lastAssignIrq, 7);
		check_eq("rt_assign_irq_to_cpu(_,1)", g_lastAssignCpu, 1);
		rtwrap_startup_irq(8);
		check_eq("rt_startup_irq(8)", g_lastStartupIrq, 8);
	}

	printf("\n=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
