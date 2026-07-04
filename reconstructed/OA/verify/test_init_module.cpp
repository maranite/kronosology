// SPDX-License-Identifier: GPL-2.0
/*
 * test_init_module.cpp  -  host-side known-answer tests for init_module()
 * (see include/oa_init.h / src/init/init_module.cpp).
 *
 * Mocks every step function so the test controls exactly which step
 * succeeds/fails, and logs every call so the exact confirmed call order
 * and partial-unwind-cascade depth can be asserted precisely -- the same
 * call-log methodology already used throughout verify/test_managers.cpp
 * and verify/test_engine.cpp.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_init.h"

static char g_log[4096];
static void log_call(const char *name)
{
	strcat(g_log, name);
	strcat(g_log, ";");
}

static int g_fail;
static void check_eq_str(const char *label, const char *got, const char *want)
{
	if (strcmp(got, want) == 0) {
		printf("  ok    %s\n", label);
		return;
	}
	printf("  FAIL  %s\n    got:  %s\n    want: %s\n", label, got, want);
	g_fail++;
}
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-60s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-60s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* Controls which hard-fail step (if any) returns failure. 0 = all succeed. */
static int g_failAtStep;

extern "C" {

void oa_debug_marker(int) { } /* TEMPORARY, matches src/init/debug_marker.cpp */
void init_cpp_support(void) { log_call("init_cpp_support"); }
void cleanup_cpp_support(void) { log_call("cleanup_cpp_support"); }

static unsigned char sFakeTask[0x100];
static unsigned long sOriginalMask = 0xdeadbeef;
void *stg_get_current_task(void)
{
	*(unsigned long *)(sFakeTask + 0xbc) = sOriginalMask;
	return sFakeTask;
}

bool cpu_features_ok(void) { log_call("cpu_features_ok"); return g_failAtStep != 2; }

unsigned long stg_cpumask_of_cpu(unsigned int cpu)
{
	char buf[40];
	sprintf(buf, "stg_cpumask_of_cpu(%u)", cpu);
	log_call(buf);
	return 0x1;
}
static unsigned long sLastAffinityMask = 0;
int stg_set_cpus_allowed(void *, unsigned long mask)
{
	sLastAffinityMask = mask;
	log_call("stg_set_cpus_allowed");
	return 0;
}

void *CSTGFile_Open(const char *, int) { log_call("CSTGFile_Open"); return 0; /* file missing -> step 4 body skipped */ }
unsigned int CSTGFile_GetFileSize(void *) { return 0; }
int  CSTGFile_Read(void *, void *, unsigned int) { return 0; }
int  CSTGFile_Close(void *) { log_call("CSTGFile_Close"); return 0; }
int  kill_proc_info(int, void *, int) { log_call("kill_proc_info"); return 0; }
int  sscanf(const char *, const char *, ...) { return 0; }

int InitializeSTGHeap(void) { log_call("InitializeSTGHeap"); return g_failAtStep == 5 ? -1 : 0; }
void CleanupSharedHeap(void) { log_call("CleanupSharedHeap"); }

int InitSharedMemProcInterface(void) { log_call("InitSharedMemProcInterface"); return g_failAtStep == 6 ? -1 : 0; }
void CleanupSharedMemProcInterface(void) { log_call("CleanupSharedMemProcInterface"); }

int InitPcmModProcInterface(void) { log_call("InitPcmModProcInterface"); return g_failAtStep == 7 ? -1 : 0; }
void CleanupPcmModProcInterface(void) { log_call("CleanupPcmModProcInterface"); }

int setup_global_resources(int) { log_call("setup_global_resources"); return g_failAtStep == 8 ? -1 : 0; }
void cleanup_global_resources(void) { log_call("cleanup_global_resources"); }

int setup_stg_decrypt_daemons(void) { log_call("setup_stg_decrypt_daemons"); return g_failAtStep == 10 ? -1 : 0; }
void cleanup_stg_decrypt_daemons(void) { log_call("cleanup_stg_decrypt_daemons"); }

int load_global_resources(void) { log_call("load_global_resources"); return g_failAtStep == 11 ? -1 : 0; }

int setup_stg_daemons(void) { log_call("setup_stg_daemons"); return g_failAtStep == 12 ? -1 : 0; }
void cleanup_stg_daemons(void) { log_call("cleanup_stg_daemons"); }

/* INVERTED convention (nonzero = success), matching the real disassembly. */
int CSTGAudioManager_StartAudioEngine(void) { log_call("CSTGAudioManager_StartAudioEngine"); return g_failAtStep == 13 ? 0 : 1; }
void CSTGAudioManager_StopAudioEngine(void) { log_call("CSTGAudioManager_StopAudioEngine"); }
void CSTGAudioManager_EnableAudioManagerThread(void) { log_call("CSTGAudioManager_EnableAudioManagerThread"); }

int CSTGKeybedInterface_Startup(void) { log_call("CSTGKeybedInterface_Startup"); return g_failAtStep == 14 ? 0 : 1; }
void CSTGKeybedInterface_Cleanup(void) { log_call("CSTGKeybedInterface_Cleanup"); }

int CSTGDrumPadInterface_Initialize(void) { log_call("CSTGDrumPadInterface_Initialize"); return -1; /* soft: result ignored regardless */ }
void CSTGDrumPadInterface_Cleanup(void) { log_call("CSTGDrumPadInterface_Cleanup"); }

int stg_rtfifo_init(void) { log_call("stg_rtfifo_init"); return g_failAtStep == 16 ? -1 : 0; }
void stg_rtfifo_cleanup(void) { log_call("stg_rtfifo_cleanup"); }

void IncProgressBar(void) { log_call("IncProgressBar"); }
void SetInstalledOptions(int) { log_call("SetInstalledOptions"); }
void stg_log_startup_error(int) { log_call("stg_log_startup_error"); }

int gModuleParam10 = 0;
int gModuleParam14 = 0;
int gModuleParam18 = 0;

int printk(const char *, ...) { return 0; }
void rt_printk(const char *, ...) { }
void __const_udelay(unsigned long) { }
unsigned long long stg_rdtsc(void) { return 0x1122334455667788ULL; }

} /* extern "C" */

/* Real C++ linkage (no extern "C" in oa_atmel.h) -- must match, not be
 * declared extern "C" here, or the linker sees a different (mangled vs.
 * unmangled) symbol than init_module.cpp actually calls. */
int SetupAtmelForAuthorizations(void) { log_call("SetupAtmelForAuthorizations"); return g_failAtStep == 9 ? -1 : 0; }

int main(void)
{
	printf("init_module() known-answer test\n");
	printf("================================\n");

	printf("[1] All steps succeed: returns 0, full call order, CPU affinity\n"
	       "    pinned to CPU 0 then restored to the ORIGINAL mask\n");
	g_log[0] = 0;
	g_failAtStep = 0;
	sLastAffinityMask = 0;
	int rc = init_module();
	check_eq("init_module() returns 0 (success)", rc, 0);
	check_eq_str("full step call order",
		g_log,
		"init_cpp_support;cpu_features_ok;stg_cpumask_of_cpu(0);stg_set_cpus_allowed;"
		"CSTGFile_Open;"
		"InitializeSTGHeap;IncProgressBar;InitSharedMemProcInterface;"
		"InitPcmModProcInterface;setup_global_resources;IncProgressBar;"
		"SetupAtmelForAuthorizations;setup_stg_decrypt_daemons;load_global_resources;"
		"setup_stg_daemons;IncProgressBar;CSTGAudioManager_StartAudioEngine;"
		"CSTGKeybedInterface_Startup;CSTGDrumPadInterface_Initialize;stg_rtfifo_init;"
		"IncProgressBar;CSTGAudioManager_EnableAudioManagerThread;stg_set_cpus_allowed;");
	check_eq("final stg_set_cpus_allowed call restored the ORIGINAL mask (0xdeadbeef)",
		 (long)sLastAffinityMask, (long)0xdeadbeef);

	printf("[2] Failure at step 5 (InitializeSTGHeap, the EARLIEST hard-fail gate):\n"
	       "    returns -1; the heap itself never got created, so CleanupSharedHeap\n"
	       "    is correctly SKIPPED (confirmed via disassembly: this jumps straight\n"
	       "    to the affinity-restore + cpp_support tail, not through fail_heap)\n");
	g_log[0] = 0;
	g_failAtStep = 5;
	rc = init_module();
	check_eq("init_module() returns -1 (failure)", rc, -1);
	check_eq_str("shallowest unwind: no cleanup calls at all, straight to restore+cpp_support",
		g_log,
		"init_cpp_support;cpu_features_ok;stg_cpumask_of_cpu(0);stg_set_cpus_allowed;"
		"CSTGFile_Open;"
		"InitializeSTGHeap;stg_log_startup_error;"
		"stg_set_cpus_allowed;cleanup_cpp_support;");

	printf("[3] Failure at step 9 (SetupAtmelForAuthorizations, a MIDDLE hard-fail\n"
	       "    gate): unwinds cleanup_global_resources onward, NOT the daemon/audio\n"
	       "    cleanups that never ran\n");
	g_log[0] = 0;
	g_failAtStep = 9;
	rc = init_module();
	check_eq("init_module() returns -1 (failure)", rc, -1);
	check_eq_str("mid-depth unwind: cleanup_global_resources -> ... -> CleanupSharedHeap",
		g_log,
		"init_cpp_support;cpu_features_ok;stg_cpumask_of_cpu(0);stg_set_cpus_allowed;"
		"CSTGFile_Open;"
		"InitializeSTGHeap;IncProgressBar;InitSharedMemProcInterface;"
		"InitPcmModProcInterface;setup_global_resources;IncProgressBar;"
		"SetupAtmelForAuthorizations;stg_log_startup_error;"
		"cleanup_global_resources;CleanupPcmModProcInterface;"
		"CleanupSharedMemProcInterface;CleanupSharedHeap;"
		"stg_set_cpus_allowed;cleanup_cpp_support;");

	printf("[4] Failure at step 16 (stg_rtfifo_init, the DEEPEST hard-fail gate):\n"
	       "    the drum pad + keybed get cleaned up FIRST (confirmed real order),\n"
	       "    then the full cascade down to CleanupSharedHeap\n");
	g_log[0] = 0;
	g_failAtStep = 16;
	rc = init_module();
	check_eq("init_module() returns -1 (failure)", rc, -1);
	check_eq_str("deepest unwind: DrumPad/Keybed cleanup first, then the full cascade",
		g_log,
		"init_cpp_support;cpu_features_ok;stg_cpumask_of_cpu(0);stg_set_cpus_allowed;"
		"CSTGFile_Open;"
		"InitializeSTGHeap;IncProgressBar;InitSharedMemProcInterface;"
		"InitPcmModProcInterface;setup_global_resources;IncProgressBar;"
		"SetupAtmelForAuthorizations;setup_stg_decrypt_daemons;load_global_resources;"
		"setup_stg_daemons;IncProgressBar;CSTGAudioManager_StartAudioEngine;"
		"CSTGKeybedInterface_Startup;CSTGDrumPadInterface_Initialize;stg_rtfifo_init;"
		"stg_log_startup_error;CSTGDrumPadInterface_Cleanup;CSTGKeybedInterface_Cleanup;"
		"CSTGAudioManager_StopAudioEngine;cleanup_stg_daemons;cleanup_global_resources;"
		"CleanupPcmModProcInterface;CleanupSharedMemProcInterface;CleanupSharedHeap;"
		"stg_set_cpus_allowed;cleanup_cpp_support;");

	printf("[5] Failure at step 2 (cpu_features_ok): fails BEFORE CPU pinning even\n"
	       "    happens -- confirmed no stg_set_cpus_allowed call at all on this path\n");
	g_log[0] = 0;
	g_failAtStep = 2;
	rc = init_module();
	check_eq("init_module() returns -1 (failure)", rc, -1);
	check_eq_str("earliest possible failure: no pinning, no restore, no subsystem setup",
		g_log,
		"init_cpp_support;cpu_features_ok;stg_log_startup_error;"
		"cleanup_cpp_support;");

	printf("================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
