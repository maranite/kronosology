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
#include <sys/mman.h>

/* This project's established convention (sec 10.156/10.181): any test
 * object whose address gets round-tripped through a packed 32-bit
 * field (ToU32/FromU32 in rtwrap.cpp, batch 39) must live in the low
 * 4GB, or the round trip silently truncates on this 64-bit host. */
static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

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
static int g_rtheapAllocCalls;
static void *g_lastRtheapAllocHeap;
static unsigned int g_lastRtheapAllocSize;
static int g_lastRtheapAllocFlags;
static void *g_rtheapAllocReturn;
static int g_taskInitCalls;
static void *g_lastTaskInitTask;
static void *g_lastTaskInitEntry;
static long g_lastTaskInitData;
static unsigned int g_lastTaskInitStackSize;
static unsigned int g_lastTaskInitPriority;
static int g_lastTaskInitUsesFpu;
static void *g_lastTaskInitSignal;
static int g_taskInitReturn;
static int g_taskResumeCalls;
static void *g_lastTaskResumed;

/*
 * Batch 39 calling-convention fix: `rt_task_suspend`/`rt_sem_wait`/
 * `rt_sem_wait_if`/`rt_sem_signal`/`rt_sem_delete`/`rt_typed_sem_init`/
 * `rt_set_runnable_on_cpuid` are confirmed STACK-passed (regparm(0)) in
 * ground truth, not this file's own default `-mregparm=3` -- see
 * rtwrap.cpp's own header comment for the full derivation. These mock
 * DEFINITIONS must carry the SAME attribute as rtwrap.cpp's own extern
 * declarations of them, or the two TUs would disagree on which
 * registers/stack slots carry the arguments (a real ABI mismatch, even
 * though both live in this same host binary).
 */
void *rt_whoami(void) { g_rtWhoamiCalls++; return g_rtWhoamiReturn; }
int rt_task_delete(void *task) { g_taskDeleteCalls++; g_lastTaskDeleted = task; return 0; }
int rt_task_suspend(void *task) __attribute__((regparm(0)));
int rt_task_suspend(void *task) { g_taskSuspendCalls++; g_lastTaskSuspended = task; return 0; }
int rt_task_resume(void *task) __attribute__((regparm(0)));
int rt_task_resume(void *task) { g_taskResumeCalls++; g_lastTaskResumed = task; return 0; }
int rt_sem_wait(void *sem) __attribute__((regparm(0)));
int rt_sem_wait(void *sem) { g_semWaitCalls++; g_lastSemWaited = sem; return 1; }
int rt_sem_wait_if(void *sem) __attribute__((regparm(0)));
int rt_sem_wait_if(void *sem) { g_semWaitIfCalls++; g_lastSemWaitedIf = sem; return g_semWaitIfReturn; }
int rt_sem_signal(void *sem) __attribute__((regparm(0)));
int rt_sem_signal(void *sem) { g_semSignalCalls++; g_lastSemSignaled = sem; return 0; }
int rt_sem_delete(void *sem) __attribute__((regparm(0)));
int rt_sem_delete(void *sem) { g_semDeleteCalls++; g_lastSemDeleted = sem; return 0; }
int rt_typed_sem_init(void *sem, int value, int type) __attribute__((regparm(0)));
int rt_typed_sem_init(void *sem, int value, int type)
{
	g_typedSemInitCalls++;
	g_lastSemInit = sem;
	g_lastSemInitValue = value;
	g_lastSemInitType = type;
	return 0;
}
void rt_set_runnable_on_cpuid(void *task, unsigned int cpuId) __attribute__((regparm(0)));
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
/* rtheap_alloc/rt_task_init are both confirmed register-passed
 * (regparm(3), this file's own default) -- no attribute override. */
void *rtheap_alloc(void *heap, unsigned int size, int flags)
{
	g_rtheapAllocCalls++;
	g_lastRtheapAllocHeap = heap;
	g_lastRtheapAllocSize = size;
	g_lastRtheapAllocFlags = flags;
	return g_rtheapAllocReturn;
}
int rt_task_init(void *task, void (*entry)(long), long data,
		  unsigned int stackSize, unsigned int priority,
		  int usesFpu, void (*signalFn)(void))
{
	g_taskInitCalls++;
	g_lastTaskInitTask = task;
	g_lastTaskInitEntry = (void *)entry;
	g_lastTaskInitData = data;
	g_lastTaskInitStackSize = stackSize;
	g_lastTaskInitPriority = priority;
	g_lastTaskInitUsesFpu = usesFpu;
	g_lastTaskInitSignal = (void *)signalFn;
	return g_taskInitReturn;
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
void *rtwrap_pthread_create(void *out, void *attr, void *(*start)(void *), void *arg);

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
		 * for the packed-32-bit pointer read at the tail of that offset.
		 * The marker itself must live in the low 4GB (mmap32) since
		 * rtwrap_pthread_cancel now reconstitutes it via FromU32 --
		 * a plain stack address would silently truncate on this 64-bit
		 * host (batch 39's own ToU32/FromU32 fix, see rtwrap.cpp). */
		unsigned char taskBuf[0x600];
		memset(taskBuf, 0xcc, sizeof(taskBuf));
		unsigned char *rawAllocMarker = (unsigned char *)mmap32(1);
		*(unsigned int *)(taskBuf + 0x5b8) = (unsigned int)(unsigned long)rawAllocMarker;

		g_taskDeleteCalls = g_rtheapFreeCalls = g_rtWhoamiCalls = 0;
		rtwrap_pthread_cancel(taskBuf);
		check_eq("explicit handle: rt_whoami NOT called", g_rtWhoamiCalls, 0);
		check_eq("explicit handle: rt_task_delete called once", g_taskDeleteCalls, 1);
		check_ptr("...on the given handle", g_lastTaskDeleted, taskBuf);
		check_eq("rtheap_free called once", g_rtheapFreeCalls, 1);
		check_ptr("...heap == &rtai_global_heap", g_lastRtheapFreeHeap, &rtai_global_heap);
		check_ptr("...ptr == the +0x5b8 marker", g_lastRtheapFreePtr, rawAllocMarker);

		unsigned char selfTaskBuf[0x600];
		memset(selfTaskBuf, 0xcc, sizeof(selfTaskBuf));
		unsigned char *selfRawAllocMarker = (unsigned char *)mmap32(1);
		*(unsigned int *)(selfTaskBuf + 0x5b8) = (unsigned int)(unsigned long)selfRawAllocMarker;
		g_rtWhoamiReturn = selfTaskBuf;

		g_taskDeleteCalls = g_rtheapFreeCalls = g_rtWhoamiCalls = 0;
		rtwrap_pthread_cancel(0);
		check_eq("NULL handle: rt_whoami called once", g_rtWhoamiCalls, 1);
		check_eq("NULL handle: rt_task_delete called once", g_taskDeleteCalls, 1);
		check_ptr("...on rt_whoami()'s own return", g_lastTaskDeleted, selfTaskBuf);
		check_ptr("...ptr == the self task's +0x5b8 marker", g_lastRtheapFreePtr, selfRawAllocMarker);
	}

	printf("\n[16] rtwrap_pthread_create -- alloc failure\n");
	{
		g_rtheapAllocCalls = g_taskInitCalls = 0;
		g_rtheapAllocReturn = 0;
		void *out = (void *)0xdeadbeef;
		void *fakeStart = (void *)0x1;
		void *rc = rtwrap_pthread_create(&out, 0, (void *(*)(void *))fakeStart, (void *)0x2);
		check_eq("rtheap_alloc called once", g_rtheapAllocCalls, 1);
		check_ptr("...heap == &rtai_global_heap", g_lastRtheapAllocHeap, &rtai_global_heap);
		check_eq("...size == 0x600", (long)g_lastRtheapAllocSize, 0x600);
		check_eq("...flags == 0", g_lastRtheapAllocFlags, 0);
		check_eq("rt_task_init NOT called", g_taskInitCalls, 0);
		check_eq("returns -12 (ENOMEM-like sentinel)", (long)(intptr_t)rc, -12);
		check_ptr("*out left untouched", out, (void *)0xdeadbeef);
	}

	printf("\n[17] rtwrap_pthread_create -- rt_task_init failure frees the block\n");
	{
		unsigned char raw[0x700];
		memset(raw, 0xcc, sizeof(raw));
		g_rtheapAllocReturn = raw;
		g_taskInitReturn = -5;
		g_rtheapFreeCalls = g_typedSemInitCalls = g_taskResumeCalls = 0;
		void *out = (void *)0xdeadbeef;
		void *rc = rtwrap_pthread_create(&out, 0, 0, 0);
		check_eq("rt_task_init called once", g_taskInitCalls, 1);
		check_eq("rtheap_free called once", g_rtheapFreeCalls, 1);
		check_ptr("...frees the RAW alloc pointer, not the aligned task", g_lastRtheapFreePtr, raw);
		check_eq("rt_typed_sem_init NOT called", g_typedSemInitCalls, 0);
		check_eq("rt_task_resume NOT called", g_taskResumeCalls, 0);
		check_eq("returns rt_task_init's own error code", (long)(intptr_t)rc, -5);
		check_ptr("*out left untouched", out, (void *)0xdeadbeef);
	}

	printf("\n[18] rtwrap_pthread_create -- full success, attr==NULL uses confirmed defaults\n");
	{
		unsigned char raw[0x700];
		memset(raw, 0xcc, sizeof(raw));
		unsigned char *task = (unsigned char *)(((unsigned long)raw + 0x40) & ~0x3ful);
		g_rtheapAllocReturn = raw;
		g_taskInitReturn = 0;
		g_rtheapFreeCalls = g_typedSemInitCalls = g_taskResumeCalls = g_taskInitCalls = 0;
		void *out = 0;
		void *fakeStart = (void *)0x11111111;
		void *rc = rtwrap_pthread_create(&out, 0, (void *(*)(void *))fakeStart, (void *)0x22222222);

		check_eq("returns 0 on success", (long)(intptr_t)rc, 0);
		check_ptr("*out == the aligned task pointer", out, task);
		/* +0x5b0/+0x5b4/+0x5b8 are packed 32-bit fields only 4 bytes
		 * apart (batch 39's ToU32/FromU32 fix, see rtwrap.cpp) -- compare
		 * the SAME truncated 32-bit value on both sides rather than
		 * round-tripping a full pointer (which would only be lossless if
		 * `raw` itself were guaranteed to live in the low 4GB). */
		check_eq("task+0x5b8 == raw alloc pointer (32-bit packed)",
			 (long)(unsigned int)*(unsigned int *)(task + 0x5b8),
			 (long)(unsigned int)(unsigned long)raw);
		check_eq("task+0x5b0 == start routine (32-bit packed)",
			 (long)(unsigned int)*(unsigned int *)(task + 0x5b0),
			 (long)(unsigned int)(unsigned long)fakeStart);
		check_eq("task+0x5b4 == arg (32-bit packed)",
			 (long)(unsigned int)*(unsigned int *)(task + 0x5b4), 0x22222222);
		check_eq("task+0x8 == 0", *(int *)(task + 0x8), 0);
		check_ptr("rt_task_init: task == aligned pointer", g_lastTaskInitTask, task);
		/*
		 * sec 10.235: RTWRAP_THREAD_TRAMPOLINE is no longer a raw
		 * ground-truth literal address (0x118e80) -- that address is
		 * never valid in THIS project's own freshly-linked OA.ko, and
		 * RTAIVirtualDriver.ko's own real rt_task_init()/
		 * rt_task_resume() genuinely jump straight to whatever entry
		 * pointer they're given (confirmed via a live-boot Oops,
		 * EIP==CR2==0x118e80, the first time this project's own
		 * real-time thread creation actually ran end to end). Same
		 * "call the captured function pointer back by hand" technique
		 * as test_daemon_lifecycle.cpp's own sec 10.234 fix: confirm
		 * it's non-NULL (a real, valid function, not a bare literal
		 * address) and that calling it with the real `(long)task`
		 * argument rt_task_init would actually pass does not crash. */
		check_eq("rt_task_init: entry is a real function pointer (not the old 0x118e80 literal)",
			 (long)(intptr_t)(g_lastTaskInitEntry != (void *)0x118e80), 1);
		check_eq("rt_task_init: entry is non-NULL",
			 (long)(intptr_t)(g_lastTaskInitEntry != 0), 1);
		((void (*)(long))g_lastTaskInitEntry)((long)(intptr_t)task);
		printf("  ok    %-58s\n", "dispatched the captured trampoline by hand without crashing");
		check_eq("rt_task_init: data == (long)task", g_lastTaskInitData, (long)(intptr_t)task);
		check_eq("rt_task_init: default stackSize == 0x2000", (long)g_lastTaskInitStackSize, 0x2000);
		check_eq("rt_task_init: default priority == 0x3fffffff", (long)g_lastTaskInitPriority, 0x3fffffff);
		check_eq("rt_task_init: usesFpu == 1 (hardcoded)", g_lastTaskInitUsesFpu, 1);
		check_ptr("rt_task_init: signal == NULL (hardcoded)", g_lastTaskInitSignal, (void *)0);
		check_ptr("rt_typed_sem_init: sem == task+0x580", g_lastSemInit, task + 0x580);
		check_eq("rt_typed_sem_init: value == 0", g_lastSemInitValue, 0);
		check_eq("rt_typed_sem_init: type == 5", g_lastSemInitType, 5);
		check_ptr("rt_task_resume called with task", g_lastTaskResumed, task);
		check_eq("rtheap_free NOT called (success path)", g_rtheapFreeCalls, 0);
	}

	printf("\n[19] rtwrap_pthread_create -- attr!=NULL overrides stackSize(+0x0)/priority(+0xc)\n");
	{
		unsigned char raw[0x700];
		memset(raw, 0xcc, sizeof(raw));
		g_rtheapAllocReturn = raw;
		g_taskInitReturn = 0;
		unsigned int attr[4] = { 0x4000, 0, 0, 99 };
		void *out = 0;
		rtwrap_pthread_create(&out, attr, 0, 0);
		check_eq("rt_task_init: stackSize from attr+0x0", (long)g_lastTaskInitStackSize, 0x4000);
		check_eq("rt_task_init: priority from attr+0xc", (long)g_lastTaskInitPriority, 99);
	}

	printf("\n[20] irq quartet -- direct forwards\n");
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
