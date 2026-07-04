// SPDX-License-Identifier: GPL-2.0
/*
 * test_shmemproc_init.cpp  -  host-side known-answer test for
 * InitSharedMemProcInterface()/CleanupSharedMemProcInterface() (see
 * ../include/oa_shmemproc_init.h / ../src/init/shmemproc_init.cpp).
 *
 * Mocks create_proc_entry/remove_proc_entry, asserting the exact
 * confirmed name/mode/parent arguments and the entry's own field values
 * set on success.
 */

#include <cstdio>
#include <cstring>
#include <cstdint>
#include "oa_shmemproc_init.h"

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

static struct proc_dir_entry g_entry;
static char g_createdName[64];
static unsigned short g_createdMode;
static struct proc_dir_entry *g_createdParent;
static int g_createCalls;
static int g_returnNull;

struct proc_dir_entry *create_proc_entry(const char *name, unsigned short mode,
					  struct proc_dir_entry *parent)
{
	g_createCalls++;
	strncpy(g_createdName, name, sizeof(g_createdName) - 1);
	g_createdMode = mode;
	g_createdParent = parent;
	if (g_returnNull)
		return 0;
	memset(&g_entry, 0, sizeof(g_entry));
	return &g_entry;
}

static char g_removedName[64];
static struct proc_dir_entry *g_removedParent;
static int g_removeCalls;
void remove_proc_entry(const char *name, struct proc_dir_entry *parent)
{
	g_removeCalls++;
	strncpy(g_removedName, name, sizeof(g_removedName) - 1);
	g_removedParent = parent;
}

} /* extern "C" */

int main(void)
{
	printf("[1] InitSharedMemProcInterface success path:\n");
	g_returnNull = 0;
	int rc = InitSharedMemProcInterface();
	check_eq("return value", rc, 0);
	check_str("create_proc_entry name", g_createdName, ".shm");
	check_eq("create_proc_entry mode (0600 octal)", g_createdMode, 0600);
	check_eq("create_proc_entry parent (NULL)", (long)(intptr_t)g_createdParent, 0);
	check_eq("create_proc_entry call count", g_createCalls, 1);
	check_eq("entry->uid", (long)g_entry.uid, 500);
	check_eq("entry->gid", (long)g_entry.gid, 500);
	check_eq("entry->proc_fops set (non-null)", g_entry.proc_fops != 0, 1);

	printf("\n[2] InitSharedMemProcInterface failure path (create_proc_entry returns NULL):\n");
	g_returnNull = 1;
	rc = InitSharedMemProcInterface();
	check_eq("return value on failure", rc, -1);

	printf("\n[3] CleanupSharedMemProcInterface:\n");
	CleanupSharedMemProcInterface();
	check_str("remove_proc_entry name", g_removedName, ".shm");
	check_eq("remove_proc_entry parent (NULL)", (long)(intptr_t)g_removedParent, 0);
	check_eq("remove_proc_entry call count", g_removeCalls, 1);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
