// SPDX-License-Identifier: GPL-2.0
/*
 * test_pcmmodproc_init.cpp  -  host-side known-answer test for
 * InitPcmModProcInterface()/CleanupPcmModProcInterface() (see
 * ../include/oa_cmd_proc.h / ../src/auth/oa_cmd_proc.cpp).
 *
 * These two were ALREADY fully reconstructed from Stage 1 (init_module
 * step 7 turned out to be exactly this, confirmed via the real proc
 * entry name ".oacmd" extracted from .rodata -- see oa_init.h's own
 * updated comment). What was actually missing was a host KAT
 * specifically for the Init/Cleanup pair (oa_cmd_proc.cpp's own
 * open/close/read/write state machine already has broader coverage via
 * test_init_module.cpp's mocks, but the proc_dir_entry registration
 * itself had never been independently exercised) -- and a real,
 * pre-existing linkage bug this pass found and fixed: oa_cmd_proc.h was
 * missing an `extern "C"` wrapper, so these two functions were compiled
 * under mangled C++ names that never matched oa_init.h's own (correct)
 * extern "C" declarations, leaving both permanently unresolved in a
 * real Kbuild build despite oa_cmd_proc.cpp already being reconstructed
 * and already part of OA-objs.
 */

#include <cstdio>
#include <cstring>
#include <cstdint>
#include "oa_cmd_proc.h"

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) {
		printf("  ok    %-40s %ld\n", label, got);
		return;
	}
	printf("  FAIL  %-40s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}
static void check_str(const char *label, const char *got, const char *want)
{
	if (got && strcmp(got, want) == 0) {
		printf("  ok    %-40s %s\n", label, got);
		return;
	}
	printf("  FAIL  %-40s got=%s want=%s\n", label, got ? got : "(null)", want);
	g_fail++;
}

extern "C" {

/* Mirrors oa_cmd_proc.cpp's own real proc_dir_entry field offsets
 * (+0x10 uid, +0x14 gid, +0x24 proc_fops), which it accesses via raw
 * pointer arithmetic rather than a typed struct (Stage 1's own
 * convention, predating this project's later offsetof-probe method).
 * Sized generously past +0x24 to safely hold a native pointer write
 * there regardless of host pointer width (8 bytes on this 64-bit host,
 * vs. 4 on the real 32-bit target). */
static unsigned char g_entry[0x40];
static char g_createdName[64];
static unsigned int g_createdMode;
static void *g_createdParent;
static int g_createCalls;
static int g_returnNull;

void *create_proc_entry(const char *name, unsigned int mode, void *parent)
{
	g_createCalls++;
	strncpy(g_createdName, name, sizeof(g_createdName) - 1);
	g_createdMode = mode;
	g_createdParent = parent;
	if (g_returnNull)
		return 0;
	memset(g_entry, 0, sizeof(g_entry));
	return g_entry;
}

static char g_removedName[64];
static void *g_removedParent;
static int g_removeCalls;
void remove_proc_entry(const char *name, void *parent)
{
	g_removeCalls++;
	strncpy(g_removedName, name, sizeof(g_removedName) - 1);
	g_removedParent = parent;
}

void mutex_lock(void *) {}
void mutex_unlock(void *) {}
void *PcmModuleMutex;

unsigned long copy_to_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
unsigned long copy_from_user(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}

int sOACmdStatus;
int sOACmdResult;

} /* extern "C" */

/*
 * Stage 1's own ProcessOACmd -- not under test here (InitPcmModProcInterface/
 * CleanupPcmModProcInterface never call it), a trivial stub suffices.
 * Deliberately declared OUTSIDE extern "C": process_oacmd.h (like
 * oa_cmd_proc.h before this pass's fix) has no extern "C" wrapper of its
 * own, so oa_cmd_proc.cpp's call to ProcessOACmd expects the real C++
 * mangled name -- matching that here, rather than introducing the same
 * kind of linkage mismatch this pass just fixed elsewhere. ProcessOACmd
 * itself is only ever called internally between process_oacmd.cpp and
 * oa_cmd_proc.cpp in the real reconstruction, so this mismatch (mangled
 * here, unmangled in the real binary) doesn't affect insmod success --
 * flagged here rather than silently worked around, but not fixed in
 * this pass since it's outside this task's scope.
 */
int ProcessOACmd(const char *cmd, int *result) { (void)cmd; (void)result; return -1; }

int main(void)
{
	printf("[1] InitPcmModProcInterface success path:\n");
	g_returnNull = 0;
	int rc = InitPcmModProcInterface();
	check_eq("return value", rc, 0);
	check_str("create_proc_entry name", g_createdName, ".oacmd");
	check_eq("create_proc_entry mode (0600 octal)", g_createdMode, 0600);
	check_eq("create_proc_entry parent (NULL)", (long)(intptr_t)g_createdParent, 0);
	check_eq("create_proc_entry call count", g_createCalls, 1);
	check_eq("entry->uid (+0x10)", *(int *)(g_entry + 0x10), 500);
	check_eq("entry->gid (+0x14)", *(int *)(g_entry + 0x14), 500);
	check_eq("entry->proc_fops (+0x24) set (non-null)",
		 *(void **)(g_entry + 0x24) != 0, 1);

	printf("\n[2] InitPcmModProcInterface failure path (create_proc_entry returns NULL):\n");
	g_returnNull = 1;
	rc = InitPcmModProcInterface();
	check_eq("return value on failure", rc, -1);

	printf("\n[3] CleanupPcmModProcInterface:\n");
	CleanupPcmModProcInterface();
	check_str("remove_proc_entry name", g_removedName, ".oacmd");
	check_eq("remove_proc_entry parent (NULL)", (long)(intptr_t)g_removedParent, 0);
	check_eq("remove_proc_entry call count", g_removeCalls, 1);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
