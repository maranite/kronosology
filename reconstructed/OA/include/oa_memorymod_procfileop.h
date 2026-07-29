// SPDX-License-Identifier: GPL-2.0
/*
 * oa_memorymod_procfileop.h  -  MemoryModProcFileOp_{open,close,lseek,mmap,
 * ioctl,write,read}: the real `struct file_operations` handlers behind
 * `MemoryModProcFileOps` (`.data+0x6da8a0`), OA.ko's own zero-copy window
 * into the STG heap.
 *
 * FOUND 2026-07-29 (round 46, solo), while pulling this cluster from
 * /home/share/Decomp/oa_export's own per-function decompiles
 * (MemoryModProcFileOp_*@0x000197d0..0x00019b40): `InitSharedMemProcInterface`
 * (`.text+0x9b60`, already reconstructed in oa_shmemproc_init.h/
 * shmemproc_init.cpp) wires `create_proc_entry(".shm", ...)`'s `proc_fops`
 * to a real named global, `MemoryModProcFileOps` -- NOT the invented
 * per-mode-page virtual stand-in `s_shmProcFops` that file's own header
 * comment already flags as a "documented software substitute", added
 * before this cluster's real handlers had been located. See that file's
 * header comment for why the stand-in is currently load-bearing (a real
 * kronos_vm boot-test dependency) and this round does NOT swap it in --
 * see this header's own CROSS-REFERENCE note below.
 *
 * REAL PROTOCOL (confirmed via ground-truth decompile, cross-checked
 * against `CSTGHandle::Access()`'s own already-reconstructed real call
 * sequence in `reconstructed/Eva/src/ipc/stg_handle.cpp` -- see
 * shmemproc_init.cpp's own header comment for that exact 4-call
 * sequence): `mode` in Eva's calling convention is a literal
 * `CSTGHeapManager` HANDLE NUMBER, not an enum of fixed shared-memory
 * kinds.
 *   - `ioctl(fd, 0x64, mode)`  -> `CSTGHeapManager_GetHandleOffset(mode)`
 *     ("byte offset", Eva's own comment)
 *   - `ioctl(fd, 0x65, mode)`  -> `CSTGHeapManager_GetHandleSize(mode)`
 *     ("nonzero size" gate, Eva's own comment)
 *   - `mmap(..., fd, pageOff)` -> `MemoryModProcFileOp_mmap`, which
 *     computes a PHYSICAL address (`vm_pgoff*PAGE_SIZE + sPhysicalHeapBase`,
 *     see oa_stgheap_init.h's own NAMING FIX note for `sPhysicalHeapBase`)
 *     and `remap_pfn_range()`s it directly into the caller's address
 *     space -- genuine zero-copy shared memory between OA.ko's kernel-side
 *     STG heap and Eva's userspace mapping of the SAME physical page, not
 *     a vmalloc'd private copy.
 *   - `ioctl(fd, 0x66..0x6c, ...)` -> the remaining CSTGHeapManager_*
 *     alloc/free/resize/defragment/reserve family (see
 *     `oa_heapmanager.h`'s own already-reconstructed `CSTGHeapManager`
 *     class for the 2 methods that ARE modeled; the 6 C-linkage
 *     `CSTGHeapManager_*` wrappers this ioctl dispatches to are NOT yet
 *     reconstructed -- declared here as genuinely unresolved externs,
 *     this project's established treatment for a confirmed-real,
 *     not-yet-bodied callee, matching `oa_heapmanager.h`'s own
 *     `CSTGHeapManager_Initialize`/`GetHeapSize` precedent).
 *
 * CROSS-REFERENCE (NOT swapped in this round): `InitSharedMemProcInterface`
 * currently wires the INVENTED `s_shmProcFops` stand-in, not this file's
 * real `MemoryModProcFileOps`. Swapping requires the 6 CSTGHeapManager_*
 * callees above AND re-verifying kronos_vm boot (the stand-in's own
 * header comment documents it as load-bearing for that exact scenario) --
 * left for a dedicated future round rather than risking an unverified
 * regression in this batch. See `HARDWARE_REVIEW_LOG.md`'s own entry for
 * this round.
 *
 * `sHeapSize`/`sPhysicalHeapBase`: the SAME real `.bss` globals
 * `stgheap_init.cpp`'s `InitializeSTGHeap()` already writes (confirmed
 * via matching `.bss` addresses in `/home/share/Decomp/oa_export/
 * symbols.csv`: `sHeapSize`@0x6f58b0, `sPhysicalHeapBase`@0x6f58ac) --
 * reused here via that file's own accessors
 * (`stgheap_get_heap_size()`/`stgheap_get_physical_heap_base()`) rather
 * than re-declaring a second, ODR-conflicting static of the same name.
 *
 * `struct file`'s `+0x24/+0x28` f_pos fields match `file_io.cpp`'s own
 * already-confirmed offsets exactly (reused, not re-derived).
 */

#ifndef OA_MEMORYMOD_PROCFILEOP_H
#define OA_MEMORYMOD_PROCFILEOP_H

extern "C" {

/* Real, exported Linux 2.6.32 symbols this cluster calls (confirmed via
 * /home/share/Decomp/oa_export/symbols.csv IMPORTED labels at the exact
 * relocation targets: printk@0xd2b01c, copy_from_user@0xd2b0b4,
 * remap_pfn_range@0xd2b0f0, copy_to_user@0xd2b1dc). */
int printk(const char *fmt, ...) __attribute__((regparm(0)));
unsigned long copy_from_user(void *to, const void *from, unsigned long n);
unsigned long copy_to_user(void *to, const void *from, unsigned long n);
int remap_pfn_range(void *vma, unsigned long addr, unsigned long pfn,
		     unsigned long size, unsigned long prot);

/* The 6 CSTGHeapManager_* wrappers MemoryModProcFileOp_ioctl dispatches
 * to, confirmed real via /home/share/Decomp/oa_export (0x3ef20..0x3f150)
 * but NOT yet reconstructed -- genuinely unresolved externs, same
 * treatment oa_heapmanager.h already established for
 * CSTGHeapManager_Initialize/GetHeapSize. */
unsigned int CSTGHeapManager_GetHandleOffset(unsigned int handle);
unsigned int CSTGHeapManager_GetHandleSize(unsigned int handle);
unsigned int CSTGHeapManager_Alloc(unsigned long size);
void CSTGHeapManager_Free(unsigned int handle, unsigned int size);
unsigned int CSTGHeapManager_Resize(unsigned int handle, unsigned int oldSize, unsigned int newSize);
void CSTGHeapManager_Defragment(void);
unsigned int CSTGHeapManager_GetMovableHeapSize(void);
unsigned int CSTGHeapManager_GetHeapFreeSize(void);
int CSTGHeapManager_SetReservedSize(unsigned int size);

int MemoryModProcFileOp_open(void *inode, void *file);
int MemoryModProcFileOp_close(void *inode, void *file);
long long MemoryModProcFileOp_lseek(void *file, unsigned int offsetLo, int offsetHi, int origin);
int MemoryModProcFileOp_mmap(void *file, void *vma);
int MemoryModProcFileOp_ioctl(void *inode, void *file, unsigned int cmd, unsigned long arg);
long MemoryModProcFileOp_write(void *file, const char *buf, unsigned long count, long long *pos);
long MemoryModProcFileOp_read(void *file, char *buf, unsigned long count, long long *pos);

} /* extern "C" */

#endif /* OA_MEMORYMOD_PROCFILEOP_H */
