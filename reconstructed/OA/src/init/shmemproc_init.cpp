// SPDX-License-Identifier: GPL-2.0
/*
 * shmemproc_init.cpp  -  InitSharedMemProcInterface()/
 * CleanupSharedMemProcInterface(): init_module step 6 (hard-fail). See
 * oa_shmemproc_init.h for the ground-truthing details.
 *
 * Faithful reconstruction from a full objdump disassembly of the real
 * `InitSharedMemProcInterface` (`.text+0x9b60`, 61 bytes) and
 * `CleanupSharedMemProcInterface` (`.text+0x9ba0`, 22 bytes) in
 * OA_real.ko -- both small enough to trace completely, no ambiguity
 * left in the control flow itself (unlike InitializeSTGHeap's own
 * address-space search loop).
 */

#include "oa_shmemproc_init.h"

/*
 * ---------------------------------------------------------------------
 * VIRTUAL STAND-IN: /proc/.shm open/ioctl/mmap handlers.
 *
 * Everything below this comment down to s_shmProcFops is NOT a ground-
 * truth recovery of real OA.ko's `/proc/.shm` protocol -- it is a
 * documented software substitute added to unblock Eva VM boot-testing,
 * per this project's established "virtual stand-in for an unrecoverable
 * environment dependency" policy (AT88VirtualChip.ko/
 * OmapNKS4VirtualDriver.ko/RTAIVirtualDriver.ko). See oa_shmemproc_init.h's
 * header comment for the full rationale and the guarantees this
 * deliberately does NOT provide.
 *
 * Services exactly the sequence CSTGHandle::Access() performs
 * (reconstructed/Eva/src/ipc/stg_handle.cpp):
 *   open("/proc/.shm", O_RDWR)
 *   ioctl(fd, 100, mode)   -> byte offset, used (after &0xfffff000) as
 *                             the mmap() file offset
 *   ioctl(fd, 0x65, mode)  -> nonzero size -> caller mmap()s
 *   mmap(..., fd, pageOff)
 *
 * Offset scheme: offset = mode * SHM_PAGE_SIZE, so mode is recoverable
 * from vm_pgoff (offset >> PAGE_SHIFT) alone inside the mmap handler --
 * no per-fd state needs to be threaded through. Each mode gets its own
 * lazily vmalloc_user()'d, zeroed page, shared by every process that
 * maps it (real shared-memory semantics, even though the specific
 * offset/size numbers are this stand-in's own invention).
 */

/*
 * struct vm_area_struct's vm_pgoff field offset on this exact target
 * (x86-32, non-PAE, CONFIG_NUMA off -- confirmed: neither
 * CONFIG_X86_PAE nor CONFIG_NUMA is set in /home/build/linux-kronos's
 * own .config). NOT hand-derived from the header's field list (that
 * struct embeds an rb_node/prio_tree_node/list_head union whose exact
 * sizes aren't worth hand-modeling) -- confirmed via the same
 * compile-time offsetof-probe technique this project already used for
 * struct proc_dir_entry's uid/gid fields (see oa_shmemproc_init.h):
 * `char probe[offsetof(struct vm_area_struct, vm_pgoff)];` compiled
 * against the real /home/build/linux-kronos headers via Kbuild, then
 * `nm -S` read the resulting symbol's size back out. Result: 0x44 (68).
 * Both offsets are `#define`d in oa_shmemproc_init.h
 * (VM_PGOFF_OFFSET/VM_START_OFFSET) so the host verify test can build an
 * equivalent fake_vma layout.
 */

static void *s_shmEntries[SHM_MAX_ENTRIES];

/*
 * Lazily vmalloc_user() the page backing `mode`, if not already done.
 * Returns NULL for an out-of-range mode or on allocation failure.
 *
 * KNOWN SIMPLIFICATION (documented, not hidden): no locking around the
 * check-then-allocate below. Real concurrent first-touch of the same
 * mode from two threads could both vmalloc_user() and leak one buffer
 * (whichever loses the race gets its return value silently overwritten
 * in s_shmEntries[mode]). Accepted here because every confirmed real
 * caller (CSTGHandle::Access(), via USTGUserAPI::Connect()) makes both
 * ioctls and the mmap from a single thread at startup before any
 * sharing begins -- a real production driver would use a spinlock/mutex
 * here, this stand-in does not.
 */
static void *shm_ensure_entry(int mode)
{
	if (mode < 0 || mode >= SHM_MAX_ENTRIES)
		return 0;
	if (!s_shmEntries[mode])
		s_shmEntries[mode] = vmalloc_user(SHM_PAGE_SIZE);
	return s_shmEntries[mode];
}

int shm_open(void *inode, void *file)
{
	(void)inode; (void)file;
	return 0;
}

/*
 * unlocked_ioctl(struct file *, unsigned int cmd, unsigned long arg) --
 * `arg` is the plain integer `mode` value CSTGHandle::Access() passes,
 * not a userspace pointer, so no copy_from_user/copy_to_user is needed.
 */
long shm_ioctl(void *file, unsigned int cmd, unsigned long arg)
{
	(void)file;
	int mode = (int)(long)arg;

	switch (cmd) {
	case SHM_IOC_GET_OFFSET:
		if (mode < 0 || mode >= SHM_MAX_ENTRIES)
			return -22;	/* -EINVAL */
		return (long)mode * SHM_PAGE_SIZE;

	case SHM_IOC_GET_SIZE:
		/* Real hardware/graceful-absence case: an out-of-range mode
		 * gets size 0, matching CSTGHandle::Access()'s own "size==0
		 * -> return NULL, no error" path rather than forcing it down
		 * the mmap-with-garbage-args branch a negative errno would. */
		if (!shm_ensure_entry(mode))
			return 0;
		return SHM_PAGE_SIZE;

	default:
		return -25;	/* -ENOTTY: matches real VFS behavior for an
				 * fops with no handler for this cmd, kept
				 * explicit here for clarity. */
	}
}

/*
 * mmap(struct file *, struct vm_area_struct *vma). `vma` is kept
 * untyped (matches this header's `void *` treatment, see
 * oa_shmemproc_init.h) -- the one field actually needed, vm_pgoff, is
 * read via the confirmed raw offset above rather than a hand-modeled
 * struct.
 */
int shm_mmap(void *file, void *vma)
{
	(void)file;
	unsigned char *v = (unsigned char *)vma;
	unsigned long pgoff = *(unsigned long *)(v + VM_PGOFF_OFFSET);
	int mode = (int)pgoff;

	void *entry = shm_ensure_entry(mode);
	if (!entry)
		return -22;	/* -EINVAL: bad mode or allocation failure */

	/* pgoff=0: our per-mode entry is exactly one page, mapped from its
	 * own start -- CSTGHandle::Access()'s own offset math already
	 * reduced everything to a whole-page mmap() offset before calling
	 * in (pageOff = offset & 0xfffff000, and our offsets are already
	 * page-aligned by construction: mode * SHM_PAGE_SIZE). */
	return remap_vmalloc_range(vma, entry, 0);
}

/*
 * Real, unmodified Linux 2.6.32 x86-32 `struct file_operations` layout
 * -- same convention/field order as oa_cmd_proc.cpp's
 * `oa_file_operations` (see that file's own header comment, which
 * confirms +0x08 read/+0x0c write/+0x30 open/+0x38 release against the
 * real oa_cmd_fops relocations). This struct's non-null fields are
 * +0x24 unlocked_ioctl, +0x2c mmap, +0x30 open -- not independently
 * confirmed via relocation the way oa_cmd_fops's fields were (there is
 * no real fops blob to check against here, see this file's own VIRTUAL
 * STAND-IN note above), just the correct real 2.6.32 struct offsets for
 * those field names.
 */
struct shm_file_operations {
	void *owner;
	void *llseek;
	void *read;
	void *write;
	void *aio_read, *aio_write, *readdir, *poll;
	void *ioctl;
	long (*unlocked_ioctl)(void *file, unsigned int cmd, unsigned long arg);
	void *compat_ioctl;
	int (*mmap)(void *file, void *vma);
	int (*open)(void *inode, void *file);
	void *flush;
	void *release;
};

static struct shm_file_operations s_shmFileOps = {
	/* owner..poll     */ 0,0,0,0,0,0,0,0,
	/* ioctl           */ 0,
	/* unlocked_ioctl  */ shm_ioctl,
	/* compat_ioctl    */ 0,
	/* mmap            */ shm_mmap,
	/* open            */ shm_open,
	/* flush           */ 0,
	/* release         */ 0,
};

/*
 * `entry->proc_fops` takes a direct pointer to the real `struct
 * file_operations`-shaped object -- unlike oa_cmd_proc.cpp's
 * `InitPcmModProcInterface` (which pokes proc_fops via a raw
 * `(char*)entry + 0x24` offset because ITS local proc_dir_entry mirror
 * doesn't name that field), oa_shmemproc_init.h's proc_dir_entry
 * already declares `proc_fops` by name, so a plain field assignment is
 * used here.
 *
 * CORRECTED (found while wiring this up): the original all-null
 * placeholder assigned `s_shmProcFops` (the bare 16-void*-slot array)
 * itself as proc_fops, which only ever worked because every slot was
 * null -- the VFS would have read the array's own storage AS the
 * `struct file_operations` layout, not as a pointer to one. Now that
 * real handlers exist, proc_fops must point directly at the
 * `shm_file_operations` object below.
 */
static const void *s_shmProcFops = &s_shmFileOps;

int InitSharedMemProcInterface(void)
{
	/* Real name extracted directly from `.rodata.str1.1+0x1c8`:
	 * ".shm" -- so this creates `/proc/.shm`. Mode 0600 (owner
	 * read/write only, confirmed literal 0x180). */
	struct proc_dir_entry *entry = create_proc_entry(".shm", 0600, 0);
	if (!entry)
		return -1;

	entry->proc_fops = s_shmProcFops;
	entry->uid = 500;
	entry->gid = 500;
	return 0;
}

/*
 * VIRTUAL STAND-IN cleanup: free every lazily-vmalloc_user()'d page
 * this pass allocated. Not confirmed real (the original 22-byte
 * CleanupSharedMemProcInterface just does the remove_proc_entry() call
 * below) -- added so repeated module load/unload cycles (e.g. during
 * VM boot-test iteration) don't leak a page per mode touched.
 */
void CleanupSharedMemProcInterface(void)
{
	remove_proc_entry(".shm", 0);

	for (int i = 0; i < SHM_MAX_ENTRIES; i++) {
		if (s_shmEntries[i]) {
			vfree(s_shmEntries[i]);
			s_shmEntries[i] = 0;
		}
	}
}
