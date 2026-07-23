// SPDX-License-Identifier: GPL-2.0
/*
 * oa_shmemproc_init.h  -  InitSharedMemProcInterface()/
 * CleanupSharedMemProcInterface(): init_module step 6 (hard-fail).
 *
 * Ground-truthed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko (InitSharedMemProcInterface
 * `.text+0x9b60`, 61 bytes; CleanupSharedMemProcInterface `.text+0x9ba0`,
 * 22 bytes), then a full objdump disassembly + relocation trace.
 *
 * CORRECTED from an earlier guess in oa_init.h: the real proc entry name
 * (extracted directly from `.rodata.str1.1+0x1c8`, not inferred) is
 * `.shm` -- i.e. `/proc/.shm`, NOT `/proc/.oacmd_shmem`.
 *
 * What it does: `create_proc_entry(".shm", 0600, NULL)`, then on success
 * sets `uid=500`, `gid=500` (real `struct proc_dir_entry` field offsets
 * confirmed via a compile-time `offsetof` probe against this project's
 * own local kernel source tree, not guessed) and `proc_fops` to a real
 * static `struct file_operations` object embedded in OA.ko's own
 * `.data` section.
 *
 * NOT independently resolved in this pass: that `file_operations`
 * object's own handler function pointers. This binary is compiled with
 * one section per function/data object (16025 total ELF sections), so
 * the specific `.data` sub-section this relocation targets isn't the
 * same as any other `.data+0x440` reference elsewhere in the binary --
 * resolving which of the many sub-sections it actually is wasn't
 * attempted this pass. Real OA.ko's actual handler behavior for
 * `/proc/.shm` (what "mode" really means beyond an array index, what its
 * real offset/size scheme is, whether it backs onto a fixed shared
 * memory region set up elsewhere in `init_module` rather than allocating
 * lazily, etc) remains UNRECOVERED -- nothing below claims otherwise.
 *
 * VIRTUAL STAND-IN (added to unblock Eva VM boot-testing, not a ground-
 * truth recovery): `create_proc_entry`'s own contract doesn't require a
 * populated fops for module load to succeed, so the all-null placeholder
 * above was sufficient for `insmod` -- but it left `/proc/.shm` with no
 * open/ioctl/mmap handlers at all, and Eva's real (decompiler-confirmed,
 * unmodified) `USTGUserAPI::Connect()` dereferences the result of
 * `CSTGHandle::Access()` with no NULL check, so a handler-less
 * `/proc/.shm` makes Eva segfault immediately on `ioctl()` returning
 * -ENOTTY (confirmed live in kronos_vm, see reconstructed/Eva's own VM
 * boot-test notes). `s_shmProcFops` below is now wired to a real,
 * minimal `struct file_operations` (open/unlocked_ioctl/mmap) that
 * services `CSTGHandle::Access()`'s exact two-ioctl-then-mmap sequence
 * (`reconstructed/Eva/src/ipc/stg_handle.cpp`) well enough for that call
 * to succeed and hand back a real, usable, zero-filled page -- same
 * "documented software stand-in for an unrecoverable/inaccessible real
 * dependency" policy already used by `AT88VirtualChip.ko`/
 * `OmapNKS4VirtualDriver.ko`/`RTAIVirtualDriver.ko` elsewhere in this
 * project, not a claim about recovering the real protocol. Specifically
 * NOT guaranteed:
 *   - "mode" values other than 1 (the only one Eva's reconstructed call
 *     sites are observed to use) are untested; this stand-in treats mode
 *     as a plain 0..(SHM_MAX_ENTRIES-1) table index, which may or may not
 *     match whatever the real "mode" semantics are.
 *   - The byte offsets/sizes `ioctl(fd, 100/0x65, mode)` return are
 *     INTERNAL to this virtual implementation only -- meaningful within
 *     one boot of this module, not physical-hardware-matching, and not
 *     interoperable with any other implementation's own offset scheme.
 *   - Each entry is a single, separately `vmalloc_user()`'d zeroed page
 *     (4096 bytes) -- big enough for the one confirmed real consumer
 *     (`USTGAPILCDControl::LoadStoredSettings()` reads up to `+0xe4`,
 *     `reconstructed/Eva/src/ipc/lcd_control.cpp`), not a claim that the
 *     real shared region is this size, this shape, or this granularity.
 */

#ifndef OA_SHMEMPROC_INIT_H
#define OA_SHMEMPROC_INIT_H

/*
 * VIRTUAL STAND-IN table size: a small, fixed number of lazily-backed
 * mmap'able pages, indexed by the "mode" ioctl arg. 64 is arbitrary
 * headroom over the single confirmed-used mode (1) -- not a recovered
 * real limit.
 */
#define SHM_MAX_ENTRIES 64
#define SHM_PAGE_SIZE   4096

/*
 * `ioctl(fd, 100, mode)` / `ioctl(fd, 0x65, mode)` request codes, per
 * `CSTGHandle::Access()`'s own literal immediates (stg_handle.cpp) --
 * "presumably get offset / get size" per that file's own header comment;
 * not independently confirmed against any real OA.ko ioctl table (this
 * stand-in defines its own handling for them, see above).
 */
#define SHM_IOC_GET_OFFSET 100
#define SHM_IOC_GET_SIZE   0x65

/*
 * struct vm_area_struct's vm_pgoff field offset on this exact target
 * (x86-32, non-PAE, CONFIG_NUMA off), confirmed via a compile-time
 * offsetof probe against /home/build/linux-kronos's own headers (same
 * technique already used for struct proc_dir_entry's uid/gid, see
 * above) -- see shmemproc_init.cpp's own comment for the exact probe
 * method. Not the whole struct: vm_area_struct's rb_node/prio_tree_node/
 * list_head union members aren't worth hand-modeling for the one field
 * shm_mmap() actually reads.
 */
#define VM_PGOFF_OFFSET 0x44
/* vm_start, confirmed offset 4 by the same probe -- not currently read
 * by any handler below, kept only because it fell out of the same probe
 * run and may be useful if a future mode needs vm_start/vm_end. */
#define VM_START_OFFSET 4

extern "C" {

/* Minimal, real Linux 2.6.32 x86-32 field layout (confirmed via a
 * compile-time `offsetof` probe against this project's own kernel
 * source tree, `include/linux/proc_fs.h`) -- only the fields this
 * function actually touches. */
struct proc_dir_entry {
	unsigned int low_ino;
	unsigned short namelen;
	const char *name;
	unsigned short mode;
	unsigned short nlink;
	unsigned int uid;
	unsigned int gid;
	unsigned long long size;
	const void *proc_iops;
	const void *proc_fops;
};

struct proc_dir_entry *create_proc_entry(const char *name, unsigned short mode,
					  struct proc_dir_entry *parent);
void remove_proc_entry(const char *name, struct proc_dir_entry *parent);

/*
 * Real, exported Linux 2.6.32 vmalloc-family symbols (mm/vmalloc.c,
 * both plain EXPORT_SYMBOL, confirmed against this project's own
 * /home/build/linux-kronos tree). `vmalloc_user()` zeroes the returned
 * region and tags it VM_USERMAP so `remap_vmalloc_range()` will map it;
 * `vfree()` is its release counterpart. `struct vm_area_struct *` is
 * kept opaque here (declared as `void *` in this freestanding
 * reconstruction, same treatment as every other real-kernel-struct
 * pointer this project passes through without modeling -- see
 * oa_stgheap_init.h's `struct resource` for the alternative, "model only
 * the touched fields" approach used where fields actually need reading;
 * shm_fops.cpp needs exactly one field, vm_pgoff, read via a raw offset
 * instead, see its own comment).
 */
void *vmalloc_user(unsigned long size);
void  vfree(void *addr);
int   remap_vmalloc_range(void *vma, void *addr, unsigned long pgoff);

/*
 * VIRTUAL STAND-IN /proc/.shm fops handlers (shmemproc_init.cpp). Not
 * static -- exposed here, same as oa_cmd_proc.h's oa_cmd_open/read/
 * write/close, so the host verify test can drive each one directly
 * rather than only through the full open()/ioctl()/mmap() syscall path.
 */
int  shm_open(void *inode, void *file);
long shm_ioctl(void *file, unsigned int cmd, unsigned long arg);
int  shm_mmap(void *file, void *vma);

int InitSharedMemProcInterface(void);
void CleanupSharedMemProcInterface(void);

} /* extern "C" */

#endif /* OA_SHMEMPROC_INIT_H */
