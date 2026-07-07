// SPDX-License-Identifier: GPL-2.0
/*
 * test_startup_helpers.cpp  -  host-side known-answer tests for the
 * init_module()-call-chain helpers reconstructed in
 * src/init/startup_helpers.cpp:
 *   sec 10.179: init_cpp_support(), GetInstalledRAM(), IncProgressBar(),
 *     SetInstalledOptions(int).
 *   sec 10.182 (batch 34): stg_is_linux_context(),
 *     stg_log_startup_error(const char *).
 *
 * Links only src/init/startup_helpers.cpp. Provides the link-time
 * dependencies that file references but does not define itself:
 *   - STGAPIFrontPanelStatus::sInstance storage (every other panel-
 *     touching test defines its own copy identically -- see
 *     test_engine_startup_bits2.cpp:83 etc.);
 *   - a COmapNKS4_IncProgressBar() mock (the real symbol is external,
 *     from OmapNKS4Module.ko) that counts its invocations;
 *   - rt_whoami() (RTAI, external in the real .ko) returning a fake
 *     RT_TASK whose priority word at +0x1c we control, so
 *     stg_is_linux_context() is genuinely EXECUTED on the host rather
 *     than mocked away;
 *   - CSTGFile_Open/Write/Close mocks (real bodies live in file_io.cpp,
 *     not linked here) that record their arguments so
 *     stg_log_startup_error()'s open/write/close transcript can be
 *     asserted directly.
 * gInstalledRAM's storage comes from startup_helpers.cpp itself.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_setup_global_resources.h"   /* STGAPIFrontPanelStatus, GetInstalledRAM, IncProgressBar */
#include "oa_init.h"                      /* init_cpp_support, SetInstalledOptions */

static int g_fail;

static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) {
		printf("  ok    %-52s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-52s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

/* ---- link-time dependencies ---- */
unsigned char *STGAPIFrontPanelStatus::sInstance;

static int g_progressTicks;
extern "C" void COmapNKS4_IncProgressBar(void) { g_progressTicks++; }

/* gInstalledRAM is defined in startup_helpers.cpp; drive it from here. */
extern "C" unsigned int gInstalledRAM;

/* ---- rt_whoami(): fake RT_TASK whose priority word (+0x1c) we set ---- */
static unsigned char g_rtTask[0x40];
static void set_rt_priority(unsigned int prio) {
	*(unsigned int *)(g_rtTask + 0x1c) = prio;
}
extern "C" void *rt_whoami(void) { return g_rtTask; }

/* ---- CSTGFile_Open/Write/Close mocks: record the transcript ---- */
static int   g_openCalls, g_writeCalls, g_closeCalls;
static char  g_openPath[128];
static int   g_openMode;
static void *g_openReturn;                 /* what Open hands back */
static void *g_writeHandle, *g_closeHandle;
static char  g_writeBuf[128];
static unsigned int g_writeCount;

static void reset_file_mock(void *openReturn) {
	g_openCalls = g_writeCalls = g_closeCalls = 0;
	g_openPath[0] = 0; g_openMode = -1;
	g_openReturn = openReturn;
	g_writeHandle = g_closeHandle = (void *)0xdead;
	g_writeBuf[0] = 0; g_writeCount = 0xffffffffu;
}
extern "C" void *CSTGFile_Open(const char *path, int mode) {
	g_openCalls++;
	if (path) { strncpy(g_openPath, path, sizeof(g_openPath) - 1); g_openPath[sizeof(g_openPath)-1]=0; }
	g_openMode = mode;
	return g_openReturn;
}
extern "C" int CSTGFile_Write(void *handle, const void *buf, unsigned int count) {
	g_writeCalls++;
	g_writeHandle = handle;
	g_writeCount = count;
	unsigned int n = count < sizeof(g_writeBuf) - 1 ? count : sizeof(g_writeBuf) - 1;
	memcpy(g_writeBuf, buf, n); g_writeBuf[n] = 0;
	return (int)count;
}
extern "C" int CSTGFile_Close(void *handle) {
	g_closeCalls++;
	g_closeHandle = handle;
	return 0;
}

int main(void)
{
	printf("init_module startup-helpers known-answer test\n");
	printf("=============================================\n");

	printf("[1] init_cpp_support() is a confirmed no-op (bare ret)\n");
	init_cpp_support();          /* must not crash / must do nothing */
	printf("  ok    init_cpp_support() returned cleanly\n");

	printf("[2] GetInstalledRAM() returns the .bss+0x20 global verbatim\n");
	gInstalledRAM = 0;
	check_eq("GetInstalledRAM() when global==0", GetInstalledRAM(), 0);
	gInstalledRAM = 0x40000000;  /* 1 GiB in bytes */
	check_eq("GetInstalledRAM() when global==1GiB", GetInstalledRAM(), 0x40000000);
	gInstalledRAM = 0x12345678;
	check_eq("GetInstalledRAM() passes value through unmodified",
		 GetInstalledRAM(), 0x12345678);

	printf("[3] IncProgressBar() forwards to COmapNKS4_IncProgressBar()\n");
	g_progressTicks = 0;
	IncProgressBar();
	check_eq("one IncProgressBar() -> one external tick", g_progressTicks, 1);
	IncProgressBar();
	IncProgressBar();
	check_eq("three total IncProgressBar() calls -> three ticks",
		 g_progressTicks, 3);

	printf("[4] SetInstalledOptions(): OR low byte into sInstance+0x1090\n");
	{
		unsigned char *panel = (unsigned char *)calloc(1, STGAPI_FRONTPANEL_SIZE);
		STGAPIFrontPanelStatus::sInstance = panel;

		SetInstalledOptions(0x20);
		check_eq("first SetInstalledOptions(0x20) sets bit 0x20",
			 panel[STGAPI_OFF_INSTALLED_OPTIONS], 0x20);

		SetInstalledOptions(0x10);
		check_eq("SetInstalledOptions(0x10) ORs in -> 0x30",
			 panel[STGAPI_OFF_INSTALLED_OPTIONS], 0x30);

		SetInstalledOptions(0x20);  /* already set -> stays 0x30 */
		check_eq("re-OR of an already-set bit is idempotent",
			 panel[STGAPI_OFF_INSTALLED_OPTIONS], 0x30);

		/* Only the LOW BYTE of the argument participates, and only the
		 * single target byte is touched -- the neighbouring 0x1091
		 * (STGAPI_OFF_CAL_MARKER) must remain untouched. */
		panel[STGAPI_OFF_INSTALLED_OPTIONS] = 0;
		SetInstalledOptions(0x1FF);
		check_eq("SetInstalledOptions(0x1FF) uses low byte only -> 0xFF",
			 panel[STGAPI_OFF_INSTALLED_OPTIONS], 0xFF);
		check_eq("adjacent byte 0x1091 untouched by the byte-wide OR",
			 panel[STGAPI_OFF_CAL_MARKER], 0x00);

		free(panel);
	}

	printf("[5] SetInstalledOptions() tolerates a NULL sInstance\n");
	STGAPIFrontPanelStatus::sInstance = 0;
	SetInstalledOptions(0x20);   /* guarded -> must not dereference NULL */
	printf("  ok    SetInstalledOptions() with NULL sInstance did not crash\n");

	printf("[6] stg_is_linux_context(): rt_whoami()->[0x1c]==0x7fffffff\n");
	set_rt_priority(0x7fffffff);
	check_eq("priority == RT_SCHED_LINUX_PRIORITY -> true", stg_is_linux_context(), 1);
	set_rt_priority(0);
	check_eq("priority == 0 (hard-RT) -> false", stg_is_linux_context(), 0);
	set_rt_priority(0x7ffffffe);
	check_eq("priority one below sentinel -> false", stg_is_linux_context(), 0);
	set_rt_priority(0x80000000);
	check_eq("priority with top bit set -> false", stg_is_linux_context(), 0);

	printf("[7] stg_log_startup_error(): guarded open+write+close transcript\n");
	/* (a) non-Linux context: nothing happens at all (open never called). */
	set_rt_priority(0);              /* not Linux context */
	reset_file_mock((void *)0x1000); /* Open would succeed if reached */
	stg_log_startup_error("cpu cap");
	check_eq("non-Linux context -> CSTGFile_Open NOT called", g_openCalls, 0);
	check_eq("non-Linux context -> no write", g_writeCalls, 0);
	check_eq("non-Linux context -> no close", g_closeCalls, 0);

	/* (b) Linux context but Open fails (returns NULL): open attempted
	 * once with the exact path+mode, then a silent no-op (no write, no
	 * close of a null handle). */
	set_rt_priority(0x7fffffff);
	reset_file_mock((void *)0);      /* Open fails */
	stg_log_startup_error("memory error");
	check_eq("Linux ctx -> CSTGFile_Open called once", g_openCalls, 1);
	check_eq("...with mode 3", g_openMode, 3);
	printf("  %s  open path == \"/tmp/startupErrorLog\" (%s)\n",
	       strcmp(g_openPath, "/tmp/startupErrorLog") == 0 ? "ok  " : "FAIL",
	       g_openPath);
	if (strcmp(g_openPath, "/tmp/startupErrorLog") != 0) g_fail++;
	check_eq("open-fail -> no write", g_writeCalls, 0);
	check_eq("open-fail -> no close of a null handle", g_closeCalls, 0);

	/* (c) Linux context, Open succeeds: open+write(strlen)+close, in
	 * order, threading the SAME handle through write and close. */
	set_rt_priority(0x7fffffff);
	reset_file_mock((void *)0xBEEF);
	stg_log_startup_error("audio threads");
	check_eq("success -> Open called once", g_openCalls, 1);
	check_eq("success -> Write called once", g_writeCalls, 1);
	check_eq("success -> Close called once", g_closeCalls, 1);
	check_eq("Write got the Open handle", (unsigned long)g_writeHandle, 0xBEEF);
	check_eq("Write count == strlen(msg)", g_writeCount, strlen("audio threads"));
	printf("  %s  write buffer == the message (%s)\n",
	       strcmp(g_writeBuf, "audio threads") == 0 ? "ok  " : "FAIL", g_writeBuf);
	if (strcmp(g_writeBuf, "audio threads") != 0) g_fail++;
	check_eq("Close got the same handle", (unsigned long)g_closeHandle, 0xBEEF);

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
