// SPDX-License-Identifier: GPL-2.0
/*
 * memorymod_procfileop.cpp  -  MemoryModProcFileOp_{open,close,lseek,mmap,
 * ioctl,write,read}. See oa_memorymod_procfileop.h for the full
 * ground-truthing/protocol writeup and the CROSS-REFERENCE note on why
 * this does NOT (yet) get wired into InitSharedMemProcInterface().
 *
 * `struct file`'s +0x24/+0x28 f_pos fields: same confirmed offsets as
 * file_io.cpp's own (loff_t, 8 bytes: +0x24 lo / +0x28 hi).
 *
 * INFERRED (not directly decompiled): MemoryModProcFileOp_write/read's
 * own copy_from_user()/copy_to_user() calls and MemoryModProcFileOp_mmap's
 * own remap_pfn_range() call are each decompiled with FEWER visible
 * arguments than their real prototypes take (Ghidra failed to recover the
 * register-passed args under this -mregparm=3 build -- confirmed by
 * comparing against each function's own real, well-known Linux 2.6.32
 * signature). The MISSING args are filled in here from that real kernel
 * API contract, not fabricated: remap_pfn_range's vma/addr/pfn (addr =
 * vma's own vm_start, pfn = the confirmed byte-address formula >>
 * PAGE_SHIFT); copy_from_user/copy_to_user's kernel-side `to`/`from`
 * pointer (sIORemapBase + current f_pos -- the SAME ioremap'd kernel VA
 * stgheap_init.cpp's InitializeSTGHeap() already established for this
 * exact physical region, reused via stgheap_get_ioremap_base()). Every
 * OTHER piece of control flow below (the 64-bit-safe bounds check, the
 * manual 64-bit pos += count via CARRY-preserving two-word arithmetic,
 * the exact ioctl case dispatch including which cases dereference `arg`
 * as a pointer vs. use it as a raw integer) is a direct, literal
 * transcription of the real decompile -- not simplified or "cleaned up".
 */

#include "oa_memorymod_procfileop.h"
#include "oa_stgheap_init.h"

#define PAGE_SHIFT 12
#define PAGE_SIZE 0x1000UL

/* Real CARRY4-equivalent: does a+b overflow a 32-bit unsigned add? */
static inline unsigned int add_carry(unsigned int a, unsigned int b)
{
	return (a + b) < a ? 1u : 0u;
}

int MemoryModProcFileOp_open(void *inode, void *file)
{
	(void)inode;
	unsigned char *f = (unsigned char *)file;
	*(unsigned int *)(f + 0x24) = 0;
	*(unsigned int *)(f + 0x28) = 0;
	return 0;
}

int MemoryModProcFileOp_close(void *inode, void *file)
{
	(void)inode; (void)file;
	return 0;
}

long long MemoryModProcFileOp_lseek(void *file, unsigned int offsetLo, int offsetHi, int origin)
{
	unsigned char *f = (unsigned char *)file;
	unsigned int posLo = offsetLo;
	int posHi = offsetHi;

	if (origin == 1) {
		/* SEEK_CUR: add the file's own current position. */
		unsigned int carry = add_carry(posLo, *(unsigned int *)(f + 0x24));
		posLo = posLo + *(unsigned int *)(f + 0x24);
		posHi = posHi + *(int *)(f + 0x28) + (int)carry;
	} else if (origin == 2) {
		/* SEEK_END: add sHeapSize. */
		unsigned long heapSize = stgheap_get_heap_size();
		unsigned int carry = add_carry(posLo, (unsigned int)heapSize);
		posLo = posLo + (unsigned int)heapSize;
		posHi = posHi + (int)carry;
	} else if (origin != 0) {
		return (long long)0xffffffffffffffeaLL;	/* -EINVAL */
	}

	if (posHi < 0 || (posHi < 1 && posLo <= stgheap_get_heap_size())) {
		*(unsigned int *)(f + 0x24) = posLo;
		*(int *)(f + 0x28) = posHi;
	} else {
		printk("MemoryModProcFileOp_lseek: EINVAL ADDRESS %llu\n",
		       (unsigned long long)posLo | ((unsigned long long)(unsigned int)posHi << 32));
		posLo = 0xffffffea;
		posHi = -1;
	}

	return ((long long)(unsigned int)posHi << 32) | (unsigned int)posLo;
}

int MemoryModProcFileOp_mmap(void *file, void *vma)
{
	(void)file;
	unsigned char *v = (unsigned char *)vma;

	/* vm_flags |= VM_IO|VM_RESERVED|VM_DONTEXPAND (literal 0x86000). */
	*(unsigned int *)(v + 0x14) |= 0x86000;

	unsigned long physAddr = (unsigned long)(*(int *)(v + 0x44)) * PAGE_SIZE +
				  stgheap_get_physical_heap_base();

	/* vm_start/vm_end/vm_page_prot are 4-byte fields on the real x86-32
	 * target (same struct vm_area_struct this file's own header already
	 * confirmed vm_flags/vm_pgoff's offsets against) -- read as
	 * `unsigned int`, NOT `unsigned long` (8 bytes on a 64-bit host),
	 * matching file_io.cpp's own already-established host/target
	 * pointer-width caveat for exactly this class of bug. Caught by this
	 * file's own host KAT (test_memorymod_procfileop.cpp), not by
	 * inspection. */
	unsigned long vmStart = *(unsigned int *)(v + 4);
	unsigned long vmEnd = *(unsigned int *)(v + 8);
	unsigned long vmPageProt = *(unsigned int *)(v + 0x10);

	int rc = remap_pfn_range(vma, vmStart, physAddr >> PAGE_SHIFT,
				  vmEnd - vmStart, vmPageProt);
	if (rc == 0)
		return 0;

	printk("MemoryModProcFileOp_mmap: remap failed for address %lx\n", physAddr);
	return -11;	/* -EAGAIN */
}

int MemoryModProcFileOp_ioctl(void *inode, void *file, unsigned int cmd, unsigned long arg)
{
	(void)inode; (void)file;
	unsigned int *argp = (unsigned int *)arg;

	switch (cmd) {
	case 0x64:
		return (int)CSTGHeapManager_GetHandleOffset((unsigned int)arg);
	case 0x65:
		return (int)CSTGHeapManager_GetHandleSize((unsigned int)arg);
	case 0x66:
		return (int)CSTGHeapManager_Alloc(arg);
	case 0x67:
		CSTGHeapManager_Free(argp[0], argp[1]);
		return 0;
	case 0x68:
		return (int)CSTGHeapManager_Resize(argp[0], argp[1], argp[2]);
	case 0x69:
		CSTGHeapManager_Defragment();
		return 0;
	case 0x6a:
		return (int)CSTGHeapManager_GetHeapFreeSize();
	case 0x6b:
		return (int)CSTGHeapManager_GetMovableHeapSize();
	case 0x6c:
		return CSTGHeapManager_SetReservedSize((unsigned int)arg);
	default:
		return -22;	/* -EINVAL */
	}
}

long MemoryModProcFileOp_write(void *file, const char *buf, unsigned long count, long long *pos)
{
	unsigned char *f = (unsigned char *)file;
	unsigned int fPosLo = *(unsigned int *)(f + 0x24);
	int fPosHi = *(int *)(f + 0x28);
	int sumHi = fPosHi + (int)add_carry((unsigned int)count, fPosLo);

	if (sumHi >= 1 || (sumHi >= 0 && count + fPosLo > stgheap_get_heap_size()))
		return (long)0xffffffea;	/* -EINVAL */

	void *to = (void *)(stgheap_get_ioremap_base() + fPosLo);
	copy_from_user(to, buf, count);

	unsigned int *posWords = (unsigned int *)pos;
	unsigned int oldLo = posWords[0];
	posWords[0] = oldLo + (unsigned int)count;
	posWords[1] = posWords[1] + add_carry(oldLo, (unsigned int)count);

	return (long)count;
}

long MemoryModProcFileOp_read(void *file, char *buf, unsigned long count, long long *pos)
{
	unsigned char *f = (unsigned char *)file;
	unsigned int fPosLo = *(unsigned int *)(f + 0x24);
	int fPosHi = *(int *)(f + 0x28);
	int sumHi = fPosHi + (int)add_carry(fPosLo, (unsigned int)count);

	if (sumHi >= 1 || (sumHi >= 0 && fPosLo + count > stgheap_get_heap_size()))
		return (long)0xffffffea;	/* -EINVAL */

	void *from = (void *)(stgheap_get_ioremap_base() + fPosLo);
	copy_to_user(buf, from, count);

	unsigned int *posWords = (unsigned int *)pos;
	unsigned int oldLo = posWords[0];
	posWords[0] = oldLo + (unsigned int)count;
	posWords[1] = posWords[1] + add_carry(oldLo, (unsigned int)count);

	return (long)count;
}

/*
 * `MemoryModProcFileOps` (`.data+0x6da8a0`): the real, unmodified Linux
 * 2.6.32 x86-32 `struct file_operations` layout -- same field-offset
 * convention already confirmed twice in this project (oa_cmd_proc.cpp's
 * `oa_cmd_fops`, +0x08 read/+0x0c write/+0x30 open/+0x38 release; this
 * struct additionally uses +0x04 llseek, +0x20 (classic) ioctl, +0x2c
 * mmap -- the ioctl handler's own regparm(3) register usage (cmd in ECX,
 * arg on the stack) matches the CLASSIC `ioctl(inode,file,cmd,arg)`
 * 4-arg prototype at +0x20, NOT `unlocked_ioctl` at +0x24, which would
 * put cmd in EDX instead). NOT currently wired into any real
 * proc_dir_entry by this project -- see this file's own header CROSS-
 * REFERENCE note.
 */
struct memorymod_file_operations {
	void *owner;
	long long (*llseek)(void *file, unsigned int offsetLo, int offsetHi, int origin);
	long (*read)(void *file, char *buf, unsigned long count, long long *pos);
	long (*write)(void *file, const char *buf, unsigned long count, long long *pos);
	void *aio_read, *aio_write, *readdir, *poll;
	int (*ioctl)(void *inode, void *file, unsigned int cmd, unsigned long arg);
	void *unlocked_ioctl, *compat_ioctl;
	int (*mmap)(void *file, void *vma);
	int (*open)(void *inode, void *file);
	void *flush;
	int (*release)(void *inode, void *file);
};

struct memorymod_file_operations MemoryModProcFileOps = {
	/* owner           */ 0,
	/* llseek          */ MemoryModProcFileOp_lseek,
	/* read            */ MemoryModProcFileOp_read,
	/* write           */ MemoryModProcFileOp_write,
	/* aio_read..poll  */ 0, 0, 0, 0,
	/* ioctl           */ MemoryModProcFileOp_ioctl,
	/* unlocked_ioctl  */ 0,
	/* compat_ioctl    */ 0,
	/* mmap            */ MemoryModProcFileOp_mmap,
	/* open            */ MemoryModProcFileOp_open,
	/* flush           */ 0,
	/* release         */ MemoryModProcFileOp_close,
};
