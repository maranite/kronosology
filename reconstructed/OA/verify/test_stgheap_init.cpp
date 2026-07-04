// SPDX-License-Identifier: GPL-2.0
/*
 * test_stgheap_init.cpp  -  host-side known-answer test for
 * InitializeSTGHeap() (see ../include/oa_stgheap_init.h /
 * ../src/init/stgheap_init.cpp).
 *
 * Mocks every real kernel primitive this function calls
 * (orig_mem_size/high_memory/__FIXADDR_TOP/iomem_resource/ioremap_cache/
 * printk) plus OA.ko's own CSTGHeapManager_Initialize/GetHeapSize, with a
 * single top-level iomem_resource child chosen so the search loop's
 * first (self-comparing) iteration already produces a huge gap and exits
 * immediately -- exercising the common real-world path, not the
 * multi-sibling search loop (which would need a second KAT to exercise
 * meaningfully; not attempted here since the loop body itself is a
 * direct, mechanical translation of the disassembly with nothing left
 * genuinely ambiguous to verify further). Deliberately small-magnitude
 * inputs (8MB/1GB-ish rather than realistic multi-GB values) so the
 * mocked `ioremap_cache` can actually `malloc()` the requested size for
 * the real memset-equivalent zero-fill to safely touch.
 *
 * Expected values below were computed independently in Python using the
 * exact same masks/shifts/constants confirmed via disassembly (see
 * MASTER_REFERENCE.md sec 10.45), not by re-deriving them from this same
 * C++ file -- an actual known-answer check, not a tautology.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_stgheap_init.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) {
		printf("  ok    %-32s 0x%08lx\n", label, got);
		return;
	}
	printf("  FAIL  %-32s got=0x%08lx want=0x%08lx\n", label, got, want);
	g_fail++;
}

extern "C" {

unsigned long orig_mem_size[2] = { 0x00800000UL, 0 };  /* 8MB */
unsigned long high_memory = 0x40400000UL;              /* 1GB + 4MB */
unsigned long __FIXADDR_TOP = 0xFFFFF000UL;
struct resource iomem_resource;

static void *sMappedRegion;
static unsigned long g_ioremapOffset, g_ioremapSize;
static int g_ioremapCalls;
void *ioremap_cache(unsigned long offset, unsigned long size)
{
	g_ioremapCalls++;
	g_ioremapOffset = offset;
	g_ioremapSize = size;
	sMappedRegion = malloc(size); /* real size, so the zero-fill is safe */
	return sMappedRegion;
}

static unsigned long g_heapInitBase, g_heapInitSize;
unsigned long CSTGHeapManager_Initialize(unsigned long base, unsigned long size)
{
	g_heapInitBase = base;
	g_heapInitSize = size;
	return base + 0x40; /* mock: real manager reserves a small header */
}

unsigned long CSTGHeapManager_GetHeapSize(void)
{
	return 0x1234; /* arbitrary, only checked for pass-through */
}

int printk(const char *fmt, ...) { (void)fmt; return 0; }

} /* extern "C" */

int main(void)
{
	/* Single top-level resource: the real search loop's first iteration
	 * compares the first child against ITSELF (see the function's own
	 * header comment on this), so its own start/end values don't matter
	 * much beyond being self-consistent -- placed right at
	 * sPhysicalBankStart so the `curEnd >= sPhysicalBankStart` guard
	 * passes. */
	struct resource node0;
	node0.start = 0x00400000UL;
	node0.end   = 0x00400000UL + 0x1000UL;
	node0.name = "test";
	node0.flags = 0;
	node0.parent = 0;
	node0.sibling = 0;
	node0.child = 0;
	iomem_resource.start = 0;
	iomem_resource.end = 0xFFFFFFFFUL;
	iomem_resource.name = "iomem";
	iomem_resource.flags = 0;
	iomem_resource.parent = 0;
	iomem_resource.sibling = 0;
	iomem_resource.child = &node0;

	int rc = InitializeSTGHeap();
	check_eq("return value", (unsigned long)rc, 0);

	check_eq("sInstalledRAM", stgheap_get_installed_ram(), 0x08000000UL);
	check_eq("ioremap_cache offset", g_ioremapOffset, 0x00401001UL);
	check_eq("ioremap_cache size", g_ioremapSize, 0x00400000UL);
	check_eq("ioremap_cache call count", (unsigned long)g_ioremapCalls, 1);
	check_eq("sPhysicalBankStart (final)", stgheap_get_physical_bank_start(), 0x00401001UL);
	check_eq("sIORemapBase", stgheap_get_ioremap_base(), (unsigned long)(void *)sMappedRegion);
	check_eq("CSTGHeapManager_Initialize base", g_heapInitBase, (unsigned long)(void *)sMappedRegion);
	check_eq("CSTGHeapManager_Initialize size", g_heapInitSize, 0x00400000UL);
	/* AlignedHeapBase = (mockHeapInitResult=mappedBase+0x40) - mappedBase
	 * + sPhysicalBankStart(final)=0x00401001 = 0x00401041 -- independent
	 * of the actual malloc'd address. */
	check_eq("AlignedHeapBase", stgheap_get_aligned_heap_base(), 0x00401041UL);
	check_eq("sHeapSize", stgheap_get_heap_size(), 0x1234UL);

	/* Confirm the mapped region really was zeroed (spot-check first/last
	 * dwords of the mocked allocation). */
	unsigned char *p = (unsigned char *)sMappedRegion;
	int zerofail = 0;
	for (unsigned long i = 0; i < g_ioremapSize; i++) {
		if (p[i] != 0) { zerofail = 1; break; }
	}
	if (zerofail) {
		printf("  FAIL  mapped region zero-filled\n");
		g_fail++;
	} else {
		printf("  ok    mapped region zero-filled\n");
	}

	printf(g_fail ? "\nRESULT: %d check(s) FAILED\n" : "\nRESULT: all checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
