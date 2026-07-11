// SPDX-License-Identifier: GPL-2.0
/*
 * test_daemon_lifecycle.cpp  -  host-side known-answer test for the STG
 * daemon kernel-thread lifecycle (src/init/daemon_lifecycle.cpp, batch
 * 40): SetupDaemon/SetupDecryptDaemon, setup_stg_daemons/
 * cleanup_stg_daemons, setup_stg_decrypt_daemons/cleanup_stg_decrypt_daemons.
 *
 * Links src/init/daemon_lifecycle.cpp directly (the only verify/ file
 * that does -- test_init_module.cpp mocks the 4 public entry points at
 * its own isolated level and never links this file, test_stg_daemons.cpp
 * links stg_daemons.cpp, a sibling TU, not this one -- no collisions).
 * Mocks the 9 real kernel/RTAI externs this file forwards to.
 */

#include <cstdio>
#include <cstring>
#include "oa_daemons.h"

/* gStgDaemons' real storage lives in src/init/stg_daemons.cpp (sec
 * 10.183), a sibling TU this isolated test does NOT link (matching
 * test_rtwrap.cpp/test_cpu_affinity.cpp's own established "give this
 * extern its own local storage" precedent) -- gStgDecryptDaemons'
 * storage lives in daemon_lifecycle.cpp itself, which IS linked here. */
STGDaemonWatch gStgDaemons[STG_DAEMON_COUNT];

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-58s %ld\n", label, got); return; }
	printf("  FAIL  %-58s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}
/* NOTE: gStgDaemons[i].name/gStgDecryptDaemons[i].name are packed 32-bit
 * copies of a `const char *` (this project's established ToU32/FromU32
 * convention, host is 64-bit, real target is 32-bit). This test's own
 * host process is a plain PIE executable (HOST_CXXFLAGS has no
 * -fno-pie) -- string-literal addresses can legitimately land above
 * 4GB, so a name field is NEVER round-tripped back through FromU32() and
 * dereferenced here (that would be the sec 10.181/10.190 truncation
 * gotcha). Identity is instead confirmed via the caller-supplied
 * ownerTag/p4/p5/index values, which are plain integers with no such
 * risk; "name" is only ever checked for non-zero-ness. */

/* ---- recording mocks ------------------------------------------------ */
extern "C" {

static int g_printkCalls;
void rt_printk(const char *, ...) { g_printkCalls++; }

static int g_jiffiesCalls;
unsigned long msecs_to_jiffies(unsigned int msecs) { g_jiffiesCalls++; return (unsigned long)msecs * 100; }

static int g_initWqCalls;
void __init_waitqueue_head(void *, void *) { g_initWqCalls++; }

static int g_srqCalls;
static unsigned int g_lastSrqLabel;
static void *g_lastSrqHandler;
static int g_srqFailAtCall;   /* 1-based; 0 = never fail */
static int g_srqReturnValue = 1;
int rt_request_srq(unsigned int label, void (*handler)(void), void *rt_handler)
{
	g_srqCalls++;
	g_lastSrqLabel = label;
	g_lastSrqHandler = (void *)handler;
	(void)rt_handler;
	if (g_srqFailAtCall && g_srqCalls == g_srqFailAtCall) return -1;
	return g_srqReturnValue;
}

static int g_freeSrqCalls;
static unsigned int g_lastFreedSrq;
void rt_free_srq(unsigned int srq) { g_freeSrqCalls++; g_lastFreedSrq = srq; }

static int g_kthreadCalls;
static int g_kthreadFailAtCall;
static long g_kthreadReturnValue = 100;
static void *g_lastKthreadArg;
static unsigned long g_lastKthreadFlags;
long kernel_thread(int (*)(void *), void *arg, unsigned long flags)
{
	g_kthreadCalls++;
	g_lastKthreadArg = arg;
	g_lastKthreadFlags = flags;
	if (g_kthreadFailAtCall && g_kthreadCalls == g_kthreadFailAtCall) return -1;
	return g_kthreadReturnValue + g_kthreadCalls;
}

static int g_waitCompletionCalls;
static void *g_lastWaitCompletion;
void wait_for_completion(void *completion) { g_waitCompletionCalls++; g_lastWaitCompletion = completion; }

static int g_waitCompletionTimeoutCalls;
static void *g_lastWaitCompletionTimeout;
static unsigned long g_lastTimeoutValue;
long wait_for_completion_timeout(void *completion, unsigned long timeout)
{
	g_waitCompletionTimeoutCalls++;
	g_lastWaitCompletionTimeout = completion;
	g_lastTimeoutValue = timeout;
	return 1;
}

static int g_wakeUpCalls;
static void *g_lastWakeQueue;
static unsigned int g_lastWakeMode;
static int g_lastWakeExclusive;
void __wake_up(void *q, unsigned int mode, int nr_exclusive, void *key)
{
	g_wakeUpCalls++;
	g_lastWakeQueue = q;
	g_lastWakeMode = mode;
	g_lastWakeExclusive = nr_exclusive;
	(void)key;
}

} /* extern "C" */

static void reset_mocks(void)
{
	g_printkCalls = 0;
	g_jiffiesCalls = 0;
	g_initWqCalls = 0;
	g_srqCalls = 0; g_lastSrqLabel = 0; g_lastSrqHandler = 0;
	g_srqFailAtCall = 0; g_srqReturnValue = 1;
	g_freeSrqCalls = 0; g_lastFreedSrq = 0;
	g_kthreadCalls = 0; g_kthreadFailAtCall = 0; g_kthreadReturnValue = 100;
	g_lastKthreadArg = 0; g_lastKthreadFlags = 0;
	g_waitCompletionCalls = 0; g_lastWaitCompletion = 0;
	g_waitCompletionTimeoutCalls = 0; g_lastWaitCompletionTimeout = 0; g_lastTimeoutValue = 0;
	g_wakeUpCalls = 0; g_lastWakeQueue = 0; g_lastWakeMode = 0; g_lastWakeExclusive = 0;
	memset(gStgDaemons, 0, sizeof(gStgDaemons));
	memset(gStgDecryptDaemons, 0, sizeof(gStgDecryptDaemons));
}

int main(void)
{
	printf("daemon kernel-thread lifecycle known-answer test\n");
	printf("=================================================\n");

	printf("[1] setup_stg_daemons(): all 7 succeed\n");
	reset_mocks();
	int rc = setup_stg_daemons();
	check_eq("return 0 (success)", rc, 0);
	check_eq("rt_request_srq called 7 times", g_srqCalls, 7);
	check_eq("kernel_thread called 7 times", g_kthreadCalls, 7);
	check_eq("wait_for_completion called 7 times", g_waitCompletionCalls, 7);
	check_eq("__init_waitqueue_head called 21 times (3 per daemon)", g_initWqCalls, 21);
	check_eq("cleanup NOT invoked (no rt_free_srq calls)", g_freeSrqCalls, 0);

	/* Ground-truth index mapping: OAStreamingReader->[2], OAFileOpener->[0],
	 * OAFileReader->[1], OAFileWriter->[3], OAFileCloser->[4],
	 * OASampling->[5], OACDAudio->[6] (sec 10.190/batch40's own real-base
	 * derivation from setup_stg_daemons' disassembly). */
	check_eq("gStgDaemons[2].name is non-zero (OAStreamingReader)", gStgDaemons[2].name != 0, 1);
	check_eq("gStgDaemons[2].p4", gStgDaemons[2].p4, 2);
	check_eq("gStgDaemons[2].p5", gStgDaemons[2].p5, 3);
	check_eq("gStgDaemons[2].running", gStgDaemons[2].running, 1);
	check_eq("gStgDaemons[2].jiffiesTimeout == msecs_to_jiffies(2)", gStgDaemons[2].jiffiesTimeout, 200);
	check_eq("gStgDaemons[2].lastTick == 0", gStgDaemons[2].lastTick, 0);
	check_eq("gStgDaemons[2].timeout == 0x32 (confirmed real constant)", gStgDaemons[2].timeout, 0x32);
	check_eq("gStgDaemons[2].field_20 == 0", gStgDaemons[2].field_20, 0);
	check_eq("gStgDaemons[2].srq == mocked rt_request_srq() return", gStgDaemons[2].srq, 1);
	check_eq("gStgDaemons[2].status == kernel_thread's pid", gStgDaemons[2].status, 101);

	check_eq("gStgDaemons[0].name is non-zero (OAFileOpener)", gStgDaemons[0].name != 0, 1);
	check_eq("gStgDaemons[0].p4/p5 == 1/0", gStgDaemons[0].p4 * 10 + gStgDaemons[0].p5, 10);
	check_eq("gStgDaemons[6].name is OACDAudio (last daemon, ownerTag 0x43444175)",
		  gStgDaemons[6].name != 0, 1);
	check_eq("last daemon's rt_request_srq ownerTag == 0x43444175 (OACDAudio)",
		  (long)g_lastSrqLabel, 0x43444175);

	printf("\n[2] setup_stg_daemons(): 3rd SetupDaemon call fails (rt_request_srq) -> short-circuit + auto-cleanup\n");
	reset_mocks();
	g_srqFailAtCall = 3;   /* 3rd call in real ground-truth ORDER: StreamingReader, FileOpener, FileReader(fails) */
	rc = setup_stg_daemons();
	check_eq("return -1 (failure)", rc, -1);
	check_eq("rt_request_srq attempted exactly 3 times (short-circuit &&)", g_srqCalls, 3);
	check_eq("kernel_thread attempted only for the 2 that passed the srq gate", g_kthreadCalls, 2);
	check_eq("daemons 4-7 (FileWriter/Closer/Sampling/CDAudio) never attempted",
		 gStgDaemons[3].name == 0 && gStgDaemons[4].name == 0 &&
		 gStgDaemons[5].name == 0 && gStgDaemons[6].name == 0, 1);
	check_eq("cleanup_stg_daemons() was invoked automatically (frees the 2 real srqs obtained)",
		 g_freeSrqCalls, 2);

	printf("\n[3] cleanup_stg_daemons(): standalone, mixed running/srq state across 7 entries\n");
	reset_mocks();
	for (int i = 0; i < STG_DAEMON_COUNT; i++) {
		gStgDaemons[i].srq = (unsigned int)-1;
		gStgDaemons[i].running = 0;
	}
	gStgDaemons[1].srq = 55;           /* has a real srq, not running */
	gStgDaemons[4].running = 1;        /* running, no srq */
	gStgDaemons[6].srq = 77; gStgDaemons[6].running = 1;   /* both */
	cleanup_stg_daemons();
	check_eq("rt_free_srq called exactly twice (entries 1 and 6)", g_freeSrqCalls, 2);
	check_eq("__wake_up called exactly twice (entries 4 and 6)", g_wakeUpCalls, 2);
	check_eq("wait_for_completion_timeout called exactly twice", g_waitCompletionTimeoutCalls, 2);
	check_eq("timeout value passed == 0x7d0 (2000 jiffies, confirmed real constant)",
		 (long)g_lastTimeoutValue, 0x7d0);
	check_eq("__wake_up mode == 3 (confirmed real constant)", (long)g_lastWakeMode, 3);
	check_eq("__wake_up nr_exclusive == 1 (confirmed real constant)", g_lastWakeExclusive, 1);
	check_eq("entry[1].srq reset to -1 after free", (long)(int)gStgDaemons[1].srq, -1);
	check_eq("entry[6].running reset to 0", gStgDaemons[6].running, 0);
	check_eq("untouched entry[0] still srq==-1, running==0",
		 (int)gStgDaemons[0].srq == -1 && gStgDaemons[0].running == 0, 1);

	printf("\n[4] setup_stg_decrypt_daemons(): all 4 succeed\n");
	reset_mocks();
	rc = setup_stg_decrypt_daemons();
	check_eq("return 0 (success)", rc, 0);
	check_eq("rt_request_srq NEVER called (decrypt daemons register no SRQ)", g_srqCalls, 0);
	check_eq("kernel_thread called 4 times", g_kthreadCalls, 4);
	check_eq("wait_for_completion called 4 times", g_waitCompletionCalls, 4);
	check_eq("__init_waitqueue_head called 16 times (4 completions per daemon)", g_initWqCalls, 16);
	check_eq("gStgDecryptDaemons[0].name is non-zero (Decrypt0)", gStgDecryptDaemons[0].name != 0, 1);
	check_eq("gStgDecryptDaemons[0].index == 0", gStgDecryptDaemons[0].index, 0);
	check_eq("gStgDecryptDaemons[0].typeTag == 2", gStgDecryptDaemons[0].typeTag, 2);
	check_eq("gStgDecryptDaemons[0].waitTimeout == 0x32", gStgDecryptDaemons[0].waitTimeout, 0x32);
	check_eq("gStgDecryptDaemons[0].jiffiesTimeout == msecs_to_jiffies(2)",
		 gStgDecryptDaemons[0].jiffiesTimeout, 200);
	check_eq("gStgDecryptDaemons[3].jiffiesTimeout == msecs_to_jiffies(4) (Decrypt3's own timeoutMs=4)",
		 gStgDecryptDaemons[3].jiffiesTimeout, 400);
	check_eq("gStgDecryptDaemons[0].reserved24 == -1 (set by setup_stg_decrypt_daemons itself)",
		 (long)gStgDecryptDaemons[0].reserved24, -1);
	check_eq("gStgDecryptDaemons[0].running == 1", gStgDecryptDaemons[0].running, 1);
	check_eq("gStgDecryptDaemons[0].status == kernel_thread's pid", gStgDecryptDaemons[0].status, 101);

	printf("\n[5] setup_stg_decrypt_daemons(): 2nd daemon's kernel_thread fails -> auto-cleanup, status=-1\n");
	reset_mocks();
	g_kthreadFailAtCall = 2;
	rc = setup_stg_decrypt_daemons();
	check_eq("return -1 (failure)", rc, -1);
	check_eq("kernel_thread attempted exactly 2 times (short-circuit &&)", g_kthreadCalls, 2);
	check_eq("Decrypt2/Decrypt3 never attempted",
		 gStgDecryptDaemons[2].name == 0 && gStgDecryptDaemons[3].name == 0, 1);
	check_eq("failing entry's status == -1 (kernel_thread failure sentinel)",
		 (long)gStgDecryptDaemons[1].status, -1);
	check_eq("failing entry's running reset to 0", gStgDecryptDaemons[1].running, 0);
	check_eq("rt_printk logged (failure message)", g_printkCalls >= 1, 1);
	check_eq("cleanup_stg_decrypt_daemons() invoked automatically (wakes the 1 real running entry)",
		 g_wakeUpCalls, 1);

	printf("\n[6] cleanup_stg_decrypt_daemons(): standalone\n");
	reset_mocks();
	gStgDecryptDaemons[0].running = 1;
	gStgDecryptDaemons[2].running = 1;
	cleanup_stg_decrypt_daemons();
	check_eq("__wake_up called exactly twice (entries 0 and 2)", g_wakeUpCalls, 2);
	check_eq("wait_for_completion_timeout called exactly twice", g_waitCompletionTimeoutCalls, 2);
	check_eq("both running flags cleared",
		 gStgDecryptDaemons[0].running == 0 && gStgDecryptDaemons[2].running == 0, 1);
	check_eq("untouched entries[1]/[3] still running==0",
		 gStgDecryptDaemons[1].running == 0 && gStgDecryptDaemons[3].running == 0, 1);

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
