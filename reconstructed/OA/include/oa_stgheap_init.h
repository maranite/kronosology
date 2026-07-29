// SPDX-License-Identifier: GPL-2.0
/*
 * oa_stgheap_init.h  -  InitializeSTGHeap()/CleanupSharedHeap(): the real
 * init_module step 5 (hard-fail).
 *
 * Ground-truthed offset: `.text+0x9bc0`, 640 bytes exactly (real symbol
 * `InitializeSTGHeap`, confirmed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko, then a full objdump
 * disassembly + relocation resolution -- not Ghidra, which times out
 * (600s) trying to load this whole 14MB/22,195-function binary; see
 * MASTER_REFERENCE.md sec 10.17's own note on this same fallback).
 *
 * What it does: finds a free region of physical/MMIO address space large
 * enough to serve as OA.ko's own "STG heap" (a chunk of `ioremap_cache`'d
 * memory that `CSTGHeapManager` then carves up for engine-global state --
 * see oa_heap.h's own accessors into that region), maps it, zeroes it,
 * and hands it to `CSTGHeapManager_Initialize`. The five diagnostic
 * `printk` calls' exact format strings were extracted directly from
 * `.rodata.str1.4` (not guessed) and independently confirm every named
 * local below (`sInstalledRAM`, `sPhysicalBankStart`, `physMemSize`,
 * `sIORemapBase`, `AlignedHeapBase`) matches the real source's own names.
 *
 * NAMING FIX (round 46, 2026-07-29): ground truth has TWO distinct real
 * `.bss` globals here, confirmed via `/home/share/Decomp/oa_export`'s own
 * symbols.csv -- `sAlignedHeapBase` (0x6f58a8, the RAW
 * `CSTGHeapManager_Initialize()` return value) and `sPhysicalHeapBase`
 * (0x6f58ac, `sAlignedHeapBase - sIORemapBase + sPhysicalBankStart`). The
 * final printk's format string literally says "AlignedHeapBase is
 * 0x%lx" but its own real argument list passes `sPhysicalHeapBase`, not
 * `sAlignedHeapBase` -- confirmed directly in ground truth's own
 * decompile (`InitializeSTGHeap@00019bc0.c`), not a Ghidra artifact. An
 * earlier pass of this file took that printk string at face value and
 * named ITS OWN single local `sAlignedHeapBase` while actually computing
 * (and storing) the `sPhysicalHeapBase` formula into it -- numerically
 * correct (this project's `stgheap_get_aligned_heap_base()` always
 * returned the right VALUE, since it's the only one of the two globals
 * this function's own call graph ever consumed downstream, by
 * `MemoryModProcFileOp_mmap`), but mismatched against ground truth's
 * real variable name, and ground truth's real `sAlignedHeapBase` (the
 * un-adjusted raw return value) was never modeled under any name at all.
 * Fixed here: the renamed accessor below now matches ground truth, and a
 * NEW accessor exposes the previously-unmodeled raw value.
 *
 * NOTE: `CSTGHeapManager_Initialize`/`CSTGHeapManager_GetHeapSize` are
 * confirmed via relocation to be plain, UNMANGLED C-linkage symbols --
 * NOT methods of the `CSTGHeapManager` C++ class already forward-declared
 * in oa_types.h (whose only known member, `sInstance`, is a distinct,
 * separately-confirmed symbol, `_ZN15CSTGHeapManager9sInstanceE`).
 * Presumably a C-linkage wrapper around the real class's constructor/
 * accessor, matching this project's established `ClassName_MethodName`
 * pattern seen throughout init_module's other steps (`CSTGKeybedInterface_
 * Startup`, `CSTGAudioManager_StartAudioEngine`, etc.) -- not yet
 * independently confirmed which, since neither symbol name itself
 * resolves to a class in this binary's own symbol table beyond that.
 */

#ifndef OA_STGHEAP_INIT_H
#define OA_STGHEAP_INIT_H

extern "C" {

/*
 * Real kernel globals this function reads directly (all confirmed via
 * `.rel.text` relocations against this exact function, not assumed).
 *
 * `orig_mem_size`: the disassembly does `cmp DWORD PTR [orig_mem_size+4],
 * 0` -- a genuine +4-byte-offset read past the symbol's own address, per
 * the relocation's addend. Modeled as a 2-element array rather than
 * asserting it's really a 64-bit value in the original source (not
 * independently confirmed which); index 0 is the normal low-32-bit
 * physical memory size this function actually uses, index 1 is that
 * confirmed extra guard read.
 */
extern unsigned long orig_mem_size[2];
extern unsigned long high_memory;
extern unsigned long __FIXADDR_TOP;

/* struct resource -- real Linux 2.6.32 x86-32 kernel layout (confirmed
 * by the disassembly's own field offsets: end@+0x4, sibling@+0x14,
 * child@+0x18, matching the real kernel header exactly). Only the
 * fields this function actually touches are declared. */
struct resource {
	unsigned long start;
	unsigned long end;
	const char *name;
	unsigned long flags;
	struct resource *parent;
	struct resource *sibling;
	struct resource *child;
};
extern struct resource iomem_resource;

void *ioremap_cache(unsigned long offset, unsigned long size);
void iounmap(void *addr);

/* OA.ko's own heap-manager entry points -- see this header's own note
 * above on the naming/class-membership caveat. Not yet reconstructed;
 * confirmed real via relocation, same treatment as every other
 * not-yet-implemented step in oa_init.h. */
unsigned long CSTGHeapManager_Initialize(unsigned long base, unsigned long size);
unsigned long CSTGHeapManager_GetHeapSize(void);

int InitializeSTGHeap(void);
void CleanupSharedHeap(void);

/* Test/inspection accessors for the confirmed-real .bss statics this
 * function computes (not present in the real binary as named exports --
 * added here purely so a host KAT can observe the result). */
unsigned long stgheap_get_installed_ram(void);
unsigned long stgheap_get_physical_bank_start(void);
unsigned long stgheap_get_ioremap_base(void);
/* Ground truth's raw CSTGHeapManager_Initialize() return value (0x6f58a8). */
unsigned long stgheap_get_aligned_heap_base(void);
/* Ground truth's sAlignedHeapBase-adjusted-for-physical-offset (0x6f58ac) --
 * what the final printk's "AlignedHeapBase" string actually prints despite
 * its own name (see this header's NAMING FIX note above). Consumed by
 * MemoryModProcFileOp_mmap (memorymod_procfileop.cpp). */
unsigned long stgheap_get_physical_heap_base(void);
unsigned long stgheap_get_heap_size(void);

} /* extern "C" */

#endif /* OA_STGHEAP_INIT_H */
