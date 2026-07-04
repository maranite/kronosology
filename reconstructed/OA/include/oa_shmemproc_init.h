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
 * attempted this pass. A minimal placeholder (all handlers null) is
 * used here instead; `create_proc_entry`'s own contract doesn't require
 * a fully-populated fops for module load to proceed.
 */

#ifndef OA_SHMEMPROC_INIT_H
#define OA_SHMEMPROC_INIT_H

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

int InitSharedMemProcInterface(void);
void CleanupSharedMemProcInterface(void);

} /* extern "C" */

#endif /* OA_SHMEMPROC_INIT_H */
