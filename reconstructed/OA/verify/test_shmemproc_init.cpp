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
#include <cstdlib>
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

/* ---- vmalloc_user/vfree/remap_vmalloc_range mocks (shm_* handlers) ----
 * Same mocking convention as test_file_io.cpp's vmalloc/vfree stubs:
 * plain host malloc/free stand-ins, real kernel semantics (zeroing,
 * VM_USERMAP tagging, actual page remapping) aren't reproducible on the
 * host and aren't needed to exercise this file's own control flow. */
static int g_vmallocUserCalls;
static unsigned long g_lastVmallocUserSize;
static int g_vfreeCalls;
static int g_remapCalls;
static void *g_lastRemapVma;
static void *g_lastRemapAddr;
static unsigned long g_lastRemapPgoff;
static int g_remapReturn;

void *vmalloc_user(unsigned long size)
{
	g_vmallocUserCalls++;
	g_lastVmallocUserSize = size;
	void *p = malloc(size);
	if (p)
		memset(p, 0, size);
	return p;
}

void vfree(void *addr)
{
	g_vfreeCalls++;
	free(addr);
}

int remap_vmalloc_range(void *vma, void *addr, unsigned long pgoff)
{
	g_remapCalls++;
	g_lastRemapVma = vma;
	g_lastRemapAddr = addr;
	g_lastRemapPgoff = pgoff;
	return g_remapReturn;
}

} /* extern "C" */

/*
 * Minimal host-side stand-in for the one struct vm_area_struct field the
 * mmap handler reads (vm_pgoff at the confirmed real offset 0x44 ON THE
 * REAL 32-BIT KERNEL TARGET). Deliberately a flat byte buffer read/
 * written via the SAME raw `(char*)vma + VM_PGOFF_OFFSET` pointer
 * arithmetic shm_mmap() itself uses, rather than a named trailing
 * struct field -- a named `unsigned long vm_pgoff` field here would let
 * the HOST compiler (native 64-bit, unlike the real -m32 kernel target)
 * insert its own alignment padding before an 8-byte-aligned `unsigned
 * long` field, silently shifting it off the intended offset 0x44 and
 * making this test read/write the wrong bytes. Sized generously past
 * VM_PGOFF_OFFSET + sizeof(unsigned long) for both 4- and 8-byte hosts.
 */
struct fake_vma {
	unsigned char raw[128];
};
static void fake_vma_set_pgoff(fake_vma *vma, unsigned long value)
{
	*(unsigned long *)(vma->raw + VM_PGOFF_OFFSET) = value;
}

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

	printf("\n[3] shm_open: always succeeds\n");
	check_eq("shm_open returns 0", shm_open(0, 0), 0);

	printf("\n[4] shm_ioctl SHM_IOC_GET_OFFSET:\n");
	check_eq("mode=1 -> offset = 1*SHM_PAGE_SIZE",
		 shm_ioctl(0, SHM_IOC_GET_OFFSET, 1), (long)SHM_PAGE_SIZE);
	check_eq("mode=0 -> offset 0",
		 shm_ioctl(0, SHM_IOC_GET_OFFSET, 0), 0);
	check_eq("mode=-1 -> -EINVAL",
		 shm_ioctl(0, SHM_IOC_GET_OFFSET, (unsigned long)-1), -22);
	check_eq("mode=SHM_MAX_ENTRIES (out of range) -> -EINVAL",
		 shm_ioctl(0, SHM_IOC_GET_OFFSET, SHM_MAX_ENTRIES), -22);

	printf("\n[5] shm_ioctl SHM_IOC_GET_SIZE (lazily allocates):\n");
	g_vmallocUserCalls = 0;
	check_eq("mode=1 -> size SHM_PAGE_SIZE",
		 shm_ioctl(0, SHM_IOC_GET_SIZE, 1), (long)SHM_PAGE_SIZE);
	check_eq("...vmalloc_user() called exactly once (first touch)", g_vmallocUserCalls, 1);
	check_eq("...vmalloc_user() size == SHM_PAGE_SIZE",
		 (long)g_lastVmallocUserSize, (long)SHM_PAGE_SIZE);
	check_eq("mode=1 again -> still SHM_PAGE_SIZE (cached)",
		 shm_ioctl(0, SHM_IOC_GET_SIZE, 1), (long)SHM_PAGE_SIZE);
	check_eq("...no second vmalloc_user() call (lazy-alloc-once)", g_vmallocUserCalls, 1);
	check_eq("mode=SHM_MAX_ENTRIES (out of range) -> size 0, not an error",
		 shm_ioctl(0, SHM_IOC_GET_SIZE, SHM_MAX_ENTRIES), 0);

	printf("\n[6] shm_ioctl unknown cmd -> -ENOTTY:\n");
	check_eq("unknown cmd -> -25", shm_ioctl(0, 12345, 1), -25);

	printf("\n[7] shm_mmap: reads vm_pgoff, remaps the matching entry:\n");
	{
		fake_vma vma;
		memset(&vma, 0, sizeof(vma));
		fake_vma_set_pgoff(&vma, 1);	/* mode 1, already allocated by test [5] */
		g_remapCalls = 0;
		g_remapReturn = 0;
		int mrc = shm_mmap(0, &vma);
		check_eq("shm_mmap(mode=1) return value", mrc, 0);
		check_eq("...remap_vmalloc_range() called once", g_remapCalls, 1);
		check_eq("...remap_vmalloc_range() pgoff arg == 0", (long)g_lastRemapPgoff, 0);
		check_eq("...remap_vmalloc_range() vma arg == &vma",
			 (long)(intptr_t)g_lastRemapVma, (long)(intptr_t)&vma);
	}
	{
		fake_vma vma;
		memset(&vma, 0, sizeof(vma));
		fake_vma_set_pgoff(&vma, 5);	/* mode 5, first touch via mmap directly (no prior ioctl) */
		g_remapCalls = 0;
		g_vmallocUserCalls = 0;
		int mrc = shm_mmap(0, &vma);
		check_eq("shm_mmap(mode=5, first touch) return value", mrc, 0);
		check_eq("...lazily allocated via vmalloc_user()", g_vmallocUserCalls, 1);
		check_eq("...remap_vmalloc_range() called once", g_remapCalls, 1);
	}
	{
		fake_vma vma;
		memset(&vma, 0, sizeof(vma));
		fake_vma_set_pgoff(&vma, (unsigned long)-1);	/* out of range */
		int mrc = shm_mmap(0, &vma);
		check_eq("shm_mmap(mode=-1) -> -EINVAL", mrc, -22);
	}

	printf("\n[8] CleanupSharedMemProcInterface: removes proc entry, frees every allocated entry:\n");
	g_vfreeCalls = 0;
	CleanupSharedMemProcInterface();
	check_str("remove_proc_entry name", g_removedName, ".shm");
	check_eq("remove_proc_entry parent (NULL)", (long)(intptr_t)g_removedParent, 0);
	check_eq("remove_proc_entry call count", g_removeCalls, 1);
	/* modes 0, 1, 5 were allocated above (mode 0's offset-only ioctl call
	 * never touched shm_ensure_entry, so only 1 and 5 actually allocated). */
	check_eq("vfree() called once per allocated entry (modes 1, 5)", g_vfreeCalls, 2);

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
