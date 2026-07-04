// SPDX-License-Identifier: GPL-2.0
/*
 * oa_bank_memory.h  -  CSTGBankMemory: a static bump/arena allocator used
 * throughout OA.ko's synthesis engine for bank-associated allocations.
 * Stage 2 shared utility (PLAN.md).
 *
 * Ground-truthed by direct disassembly -- all three methods are tiny
 * (31/6/37 bytes) and fully confirmed, no simplification needed:
 *   CSTGBankMemory::Initialize(unsigned char*, unsigned long)   .text+0x232b0
 *   CSTGBankMemory::SetTotalBytesToManage(unsigned long)        .text+0x232d0
 *   CSTGBankMemory::AllocAligned(unsigned int, unsigned int)    .text+0x232e0
 *
 * All three methods are genuinely STATIC (no `this` at all -- EAX in
 * AllocAligned is confirmed to be its first real argument, `size`, under
 * -mregparm=3), backed by three static globals, all real confirmed symbols:
 *   sOurMemoryBase             unsigned char*
 *   sCurrentAllocationOffset   unsigned long
 *   sTotalMemoryAvailable      unsigned long
 *
 * Initialize(base, size): 16-byte-aligns `base` UP and `size` DOWN, resets
 * the allocation offset to 0.
 *
 * SetTotalBytesToManage(size): overwrites sTotalMemoryAvailable directly,
 * with NO re-alignment -- used to adjust the manageable size after
 * Initialize (confirmed: a separate, later call site, not part of
 * Initialize's own sequence).
 *
 * AllocAligned(size, alignment): a straightforward bump allocator --
 *   ptr = align_up(sOurMemoryBase + sCurrentAllocationOffset, alignment)
 *   sCurrentAllocationOffset = (ptr - sOurMemoryBase) + size
 *   return ptr
 * CONFIRMED: there is NO bounds/overflow check against sTotalMemoryAvailable
 * anywhere in AllocAligned itself -- callers are trusted not to overrun the
 * pool. sTotalMemoryAvailable is read elsewhere purely for diagnostics: a
 * separate, much larger function, `CSTGMultisampleBank::
 * GetTestBankMemoryUsage(bool)` (.text+0x33f70, 923 bytes), reads both
 * sCurrentAllocationOffset and sTotalMemoryAvailable together -- almost
 * certainly to report a used/total utilization statistic. Not reconstructed
 * here; out of scope for the allocator itself.
 */

#ifndef OA_BANK_MEMORY_H
#define OA_BANK_MEMORY_H

struct CSTGBankMemory {
	static void Initialize(unsigned char *base, unsigned long size);
	static void SetTotalBytesToManage(unsigned long size);
	static unsigned char *AllocAligned(unsigned int size, unsigned int alignment);

	static unsigned char *sOurMemoryBase;
	static unsigned long  sCurrentAllocationOffset;
	static unsigned long  sTotalMemoryAvailable;
};

#endif /* OA_BANK_MEMORY_H */
