// SPDX-License-Identifier: GPL-2.0
/*
 * test_memorymod_procfileop.cpp  -  host-side known-answer test for
 * MemoryModProcFileOp_{open,close,lseek,mmap,ioctl,write,read} (see
 * ../include/oa_memorymod_procfileop.h / ../src/init/memorymod_procfileop.cpp).
 *
 * Links stgheap_init.cpp too (memorymod_procfileop.cpp calls its
 * accessors for sIORemapBase/sPhysicalHeapBase/sHeapSize), so this test
 * also mocks stgheap_init.cpp's own real-kernel dependencies (same mocks
 * as test_stgheap_init.cpp) and runs a real InitializeSTGHeap() once at
 * startup to populate that state, exactly as it would be during a real
 * insmod sequence.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_memorymod_procfileop.h"
#include "oa_stgheap_init.h"

static int g_fail;
static void check_eq(const char *label, long long got, long long want)
{
	if (got == want) {
		printf("  ok    %-40s 0x%llx\n", label, (unsigned long long)got);
		return;
	}
	printf("  FAIL  %-40s got=0x%llx want=0x%llx\n", label, (unsigned long long)got, (unsigned long long)want);
	g_fail++;
}
extern "C" {

/* ---- stgheap_init.cpp's own real-kernel mocks (mirrors test_stgheap_init.cpp) ---- */
unsigned long orig_mem_size[2] = { 0x00800000UL, 0 };
unsigned long high_memory = 0x40400000UL;
unsigned long __FIXADDR_TOP = 0xFFFFF000UL;
struct resource iomem_resource;

static void *sMappedRegion;
void *ioremap_cache(unsigned long offset, unsigned long size)
{
	(void)offset;
	sMappedRegion = malloc(size);
	memset(sMappedRegion, 0, size);
	return sMappedRegion;
}
void iounmap(void *addr) { free(addr); }

unsigned long CSTGHeapManager_Initialize(unsigned long base, unsigned long size)
{
	(void)size;
	return base + 0x40;
}
unsigned long CSTGHeapManager_GetHeapSize(void) { return 0x1000; }	/* 4KB mock heap */

/* ---- memorymod_procfileop.cpp's own real-kernel mocks ---- */
static char g_printkBuf[256];
int printk(const char *fmt, ...)
{
	(void)fmt;
	g_printkBuf[0] = 1;	/* just record "was called" */
	return 0;
}

static void *g_copyFromTo, *g_copyFromFrom; static unsigned long g_copyFromN;
unsigned long copy_from_user(void *to, const void *from, unsigned long n)
{
	g_copyFromTo = to; g_copyFromFrom = (void *)from; g_copyFromN = n;
	memcpy(to, from, n);
	return 0;
}

static void *g_copyToTo, *g_copyToFrom; static unsigned long g_copyToN;
unsigned long copy_to_user(void *to, const void *from, unsigned long n)
{
	g_copyToTo = to; g_copyToFrom = (void *)from; g_copyToN = n;
	memcpy(to, from, n);
	return 0;
}

static void *g_remapVma; static unsigned long g_remapAddr, g_remapPfn, g_remapSize, g_remapProt;
static int g_remapCalls, g_remapForceFail;
int remap_pfn_range(void *vma, unsigned long addr, unsigned long pfn, unsigned long size, unsigned long prot)
{
	g_remapCalls++;
	g_remapVma = vma; g_remapAddr = addr; g_remapPfn = pfn; g_remapSize = size; g_remapProt = prot;
	return g_remapForceFail ? -1 : 0;
}

static unsigned int g_lastHandleOffsetArg, g_lastHandleSizeArg;
unsigned int CSTGHeapManager_GetHandleOffset(unsigned int handle) { g_lastHandleOffsetArg = handle; return 0x100; }
unsigned int CSTGHeapManager_GetHandleSize(unsigned int handle) { g_lastHandleSizeArg = handle; return 0x40; }

static unsigned long g_lastAllocSize;
unsigned int CSTGHeapManager_Alloc(unsigned long size) { g_lastAllocSize = size; return 7; }

static unsigned int g_lastFreeHandle, g_lastFreeSize;
void CSTGHeapManager_Free(unsigned int handle, unsigned int size) { g_lastFreeHandle = handle; g_lastFreeSize = size; }

static unsigned int g_lastResizeHandle, g_lastResizeOld, g_lastResizeNew;
unsigned int CSTGHeapManager_Resize(unsigned int handle, unsigned int oldSize, unsigned int newSize)
{
	g_lastResizeHandle = handle; g_lastResizeOld = oldSize; g_lastResizeNew = newSize;
	return 0x99;
}

static int g_defragCalls;
void CSTGHeapManager_Defragment(void) { g_defragCalls++; }

unsigned int CSTGHeapManager_GetMovableHeapSize(void) { return 0x2222; }
unsigned int CSTGHeapManager_GetHeapFreeSize(void) { return 0x3333; }

static unsigned int g_lastReservedSize;
int CSTGHeapManager_SetReservedSize(unsigned int size) { g_lastReservedSize = size; return 1; }

} /* extern "C" */

int main(void)
{
	printf("MemoryModProcFileOp known-answer test\n");
	printf("======================================\n");

	/* Bring up real STG-heap state (sIORemapBase/sPhysicalHeapBase/sHeapSize)
	 * via the real InitializeSTGHeap(), same minimal single-node resource
	 * tree as test_stgheap_init.cpp. */
	struct resource node0;
	node0.start = 0x00400000UL;
	node0.end = 0x00400000UL + 0x1000UL;
	node0.name = "test"; node0.flags = 0; node0.parent = 0; node0.sibling = 0; node0.child = 0;
	iomem_resource.start = 0; iomem_resource.end = 0xFFFFFFFFUL; iomem_resource.name = "iomem";
	iomem_resource.flags = 0; iomem_resource.parent = 0; iomem_resource.sibling = 0;
	iomem_resource.child = &node0;
	check_eq("InitializeSTGHeap rc", InitializeSTGHeap(), 0);

	unsigned char fakeFile[64];
	memset(fakeFile, 0xCC, sizeof(fakeFile));

	printf("[1] open() zeroes f_pos (+0x24/+0x28)\n");
	{
		check_eq("open rc", MemoryModProcFileOp_open(0, fakeFile), 0);
		check_eq("f_pos lo", *(unsigned int *)(fakeFile + 0x24), 0);
		check_eq("f_pos hi", *(unsigned int *)(fakeFile + 0x28), 0);
	}

	printf("[2] close() is a real no-op returning 0\n");
	{
		check_eq("close rc", MemoryModProcFileOp_close(0, fakeFile), 0);
	}

	printf("[3] lseek SEEK_SET (origin=0) within bounds\n");
	{
		long long rc = MemoryModProcFileOp_lseek(fakeFile, 0x10, 0, 0);
		check_eq("lseek rc", rc, 0x10);
		check_eq("f_pos lo after", *(unsigned int *)(fakeFile + 0x24), 0x10);
	}

	printf("[4] lseek SEEK_CUR (origin=1) adds to current pos\n");
	{
		long long rc = MemoryModProcFileOp_lseek(fakeFile, 0x20, 0, 1);
		check_eq("lseek rc", rc, 0x30);	/* 0x10 + 0x20 */
		check_eq("f_pos lo after", *(unsigned int *)(fakeFile + 0x24), 0x30);
	}

	printf("[5] lseek SEEK_END (origin=2) adds sHeapSize (mocked 0x1000)\n");
	{
		long long rc = MemoryModProcFileOp_lseek(fakeFile, 0, 0, 2);
		check_eq("lseek rc", rc, 0x1000);
	}

	printf("[6] lseek invalid origin -> -EINVAL (64-bit sign-extended)\n");
	{
		long long rc = MemoryModProcFileOp_lseek(fakeFile, 0, 0, 5);
		check_eq("lseek rc (invalid origin)", rc, (long long)0xffffffffffffffeaLL);
	}

	printf("[7] lseek out-of-bounds (past sHeapSize) -> error sentinel, f_pos left errored\n");
	{
		long long rc = MemoryModProcFileOp_lseek(fakeFile, 0x2000, 0, 0);	/* > 0x1000 heap */
		check_eq("lseek rc (out of bounds)", rc, (long long)0xffffffffffffffeaLL);
	}

	printf("[8] mmap: vm_flags OR'd, remap_pfn_range gets vm_start/pfn/size/prot\n");
	{
		unsigned char vma[0x50];
		memset(vma, 0, sizeof(vma));
		/* vm_start/vm_end/vm_page_prot are 4-byte fields on the real
		 * x86-32 target -- write them as `unsigned int`, matching the
		 * reconstruction's own (bug-fixed) read width. */
		*(unsigned int *)(vma + 4) = 0x08000000U;	/* vm_start */
		*(unsigned int *)(vma + 8) = 0x08000000U + 0x1000U;	/* vm_end */
		*(unsigned int *)(vma + 0x10) = 0x63;	/* vm_page_prot (arbitrary) */
		*(unsigned int *)(vma + 0x14) = 0x1;	/* pre-existing vm_flags bit */
		*(int *)(vma + 0x44) = 2;	/* vm_pgoff = 2 */

		g_remapCalls = 0; g_remapForceFail = 0;
		int rc = MemoryModProcFileOp_mmap(0, vma);
		check_eq("mmap rc", rc, 0);
		check_eq("remap_pfn_range call count", g_remapCalls, 1);
		check_eq("vm_flags OR'd with 0x86000", *(unsigned int *)(vma + 0x14), 0x1 | 0x86000);
		check_eq("remap addr == vm_start", (long long)g_remapAddr, 0x08000000LL);
		check_eq("remap size == vm_end-vm_start", (long long)g_remapSize, 0x1000LL);
		check_eq("remap prot == vm_page_prot", (long long)g_remapProt, 0x63LL);
		unsigned long expectedPhys = 2UL * 0x1000UL + stgheap_get_physical_heap_base();
		check_eq("remap pfn == (2*PAGE+physHeapBase)>>12", (long long)g_remapPfn, (long long)(expectedPhys >> 12));

		g_remapForceFail = 1;
		int rc2 = MemoryModProcFileOp_mmap(0, vma);
		check_eq("mmap failure path -> -EAGAIN", rc2, -11);
	}

	printf("[9] ioctl dispatch: each real case reaches its own CSTGHeapManager_* callee\n");
	{
		check_eq("ioctl 0x64 -> GetHandleOffset(arg)", MemoryModProcFileOp_ioctl(0, 0, 0x64, 42), 0x100);
		check_eq("  arg forwarded", g_lastHandleOffsetArg, 42);

		check_eq("ioctl 0x65 -> GetHandleSize(arg)", MemoryModProcFileOp_ioctl(0, 0, 0x65, 42), 0x40);
		check_eq("  arg forwarded", g_lastHandleSizeArg, 42);

		check_eq("ioctl 0x66 -> Alloc(arg)", MemoryModProcFileOp_ioctl(0, 0, 0x66, 0x800), 7);
		check_eq("  size forwarded", g_lastAllocSize, 0x800);

		unsigned int freeArgs[2] = { 7, 0x800 };
		check_eq("ioctl 0x67 -> Free(*arg,arg[1]), returns 0", MemoryModProcFileOp_ioctl(0, 0, 0x67, (unsigned long)freeArgs), 0);
		check_eq("  handle forwarded", g_lastFreeHandle, 7);
		check_eq("  size forwarded", g_lastFreeSize, 0x800);

		unsigned int resizeArgs[3] = { 7, 0x800, 0x1000 };
		check_eq("ioctl 0x68 -> Resize(*arg,arg[1],arg[2])", MemoryModProcFileOp_ioctl(0, 0, 0x68, (unsigned long)resizeArgs), 0x99);
		check_eq("  handle/old/new forwarded", g_lastResizeHandle == 7 && g_lastResizeOld == 0x800 && g_lastResizeNew == 0x1000, true);

		g_defragCalls = 0;
		check_eq("ioctl 0x69 -> Defragment(), returns 0", MemoryModProcFileOp_ioctl(0, 0, 0x69, 0), 0);
		check_eq("  Defragment called once", g_defragCalls, 1);

		check_eq("ioctl 0x6a -> GetHeapFreeSize()", MemoryModProcFileOp_ioctl(0, 0, 0x6a, 0), 0x3333);
		check_eq("ioctl 0x6b -> GetMovableHeapSize()", MemoryModProcFileOp_ioctl(0, 0, 0x6b, 0), 0x2222);

		check_eq("ioctl 0x6c -> SetReservedSize(arg)", MemoryModProcFileOp_ioctl(0, 0, 0x6c, 0x555), 1);
		check_eq("  size forwarded", g_lastReservedSize, 0x555);

		check_eq("ioctl unknown cmd -> -EINVAL", MemoryModProcFileOp_ioctl(0, 0, 0xdead, 0), -22);
	}

	printf("[10] write: bounds-checked copy_from_user into the ioremap'd heap at f_pos, pos advances\n");
	{
		MemoryModProcFileOp_open(0, fakeFile);	/* reset f_pos to 0 */
		char srcBuf[8] = "ABCDEFG";
		/* Real convention: the VFS's own ppos IS &file->f_pos -- pos and
		 * the bounds check's own file+0x24/+0x28 read are the SAME
		 * memory, not two independent values. */
		long long *pos = (long long *)(fakeFile + 0x24);

		long rc = MemoryModProcFileOp_write(fakeFile, srcBuf, 7, pos);
		check_eq("write rc == count", rc, 7);
		check_eq("copy_from_user n", (long long)g_copyFromN, 7);
		check_eq("copy_from_user from == buf", g_copyFromFrom == (void *)srcBuf, true);
		check_eq("copy_from_user to == ioremapBase+0 (f_pos was 0)",
			 (long long)(unsigned long)g_copyFromTo, (long long)stgheap_get_ioremap_base());
		check_eq("*pos advanced by count", *pos, 7);
		check_eq("heap bytes actually written", memcmp(sMappedRegion, "ABCDEFG", 7) == 0, true);

		/* Out-of-bounds write: seek to a VALID position near the mocked
		 * 0x1000 heap's end (0xFF8), then attempt a write whose COUNT
		 * pushes f_pos+count past sHeapSize (0xFF8+0x10=0x1008>0x1000)
		 * -- the real, reachable OOB path (a direct seek past sHeapSize
		 * is itself rejected by lseek's own bounds check, see test [7]). */
		MemoryModProcFileOp_lseek(fakeFile, 0xFF8, 0, 0);
		g_copyFromN = 0;
		long rc2 = MemoryModProcFileOp_write(fakeFile, srcBuf, 0x10, pos);
		check_eq("write out-of-bounds rc", rc2, (long)0xffffffea);
		check_eq("no copy_from_user on OOB", (long long)g_copyFromN, 0);
	}

	printf("[11] read: bounds-checked copy_to_user from the ioremap'd heap at f_pos, pos advances\n");
	{
		MemoryModProcFileOp_open(0, fakeFile);	/* reset f_pos to 0 */
		char dstBuf[8]; memset(dstBuf, 0, sizeof(dstBuf));
		long long *pos = (long long *)(fakeFile + 0x24);

		long rc = MemoryModProcFileOp_read(fakeFile, dstBuf, 7, pos);
		check_eq("read rc == count", rc, 7);
		check_eq("copy_to_user n", (long long)g_copyToN, 7);
		check_eq("copy_to_user to == buf", g_copyToTo == (void *)dstBuf, true);
		check_eq("copy_to_user from == ioremapBase+0", (long long)(unsigned long)g_copyToFrom, (long long)stgheap_get_ioremap_base());
		check_eq("*pos advanced by count", *pos, 7);
		check_eq("dstBuf got the heap bytes written in test [10]", memcmp(dstBuf, "ABCDEFG", 7) == 0, true);

		MemoryModProcFileOp_lseek(fakeFile, 0xFF8, 0, 0);
		g_copyToN = 0;
		long rc2 = MemoryModProcFileOp_read(fakeFile, dstBuf, 0x10, pos);
		check_eq("read out-of-bounds rc", rc2, (long)0xffffffea);
		check_eq("no copy_to_user on OOB", (long long)g_copyToN, 0);
	}

	CleanupSharedHeap();
	printf("\n%s (%d failure%s)\n", g_fail ? "FAIL" : "PASS", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
