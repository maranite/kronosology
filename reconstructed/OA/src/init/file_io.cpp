// SPDX-License-Identifier: GPL-2.0
/*
 * file_io.cpp  -  the CSTGFile_* kernel-VFS wrapper primitives, promoted
 * from their inert placeholders in src/stub/bar2_stubs_c.cpp to faithful
 * reconstructions from the real OA_322.ko disassembly (sec 10.180).
 *
 * This batch does the four that DON'T manipulate the per-CPU thread_info
 * addr_limit (set_fs): CSTGFile_Open, CSTGFile_Close, CSTGFile_Seek,
 * CSTGFile_GetFileSize. The set_fs + f_op->read/write dispatch pair
 * (CSTGFile_Read/CSTGFile_Write) is deferred to its own batch -- its
 * `esp & ~0x1fff` thread_info trick is not host-executable (it would
 * clobber the host stack) and needs an opaque set_fs helper, matching
 * init_module.cpp's own treatment of the raw `current` idiom.
 *
 * These are thin wrappers over the Linux 2.6.32 VFS. The `struct file`
 * handle is treated as an opaque `void *` throughout (the project's
 * already-established handle-based CSTGFile_* ABI). Kernel struct field
 * accesses are raw byte-offset loads transcribed verbatim from the
 * disassembly -- the confirmed 2.6.32/x86-32 offsets:
 *     file  +0x0c = f_path.dentry
 *     file  +0x10 = f_op
 *     file  +0x20 = f_mode          (fmode_t / u32)
 *     dentry+0x10 = d_inode
 *     inode +0x40 = i_size          (loff_t; low dword returned)
 * On the real -m32 build these are 4-byte pointer / 4-byte scalar loads
 * exactly as the kernel lays them out; the host KAT builds fake structs
 * with a matching byte layout so the identical source is testable there.
 *
 * The three kernel entry points (filp_open/filp_close/generic_file_llseek)
 * are ordinary regparm(3) kernel functions (NOT asmlinkage -- unlike the
 * printk family, see init_module.cpp's regparm(0) note), so a plain
 * extern "C" declaration under this file's -mregparm=3 default matches
 * their real ABI. All three are undefined (`U`) in the real OA.ko as
 * well, resolved at insmod time -- promoting these raises the
 * reconstruction's unresolved count by three (33 -> 36), all legitimate.
 */

/* ---- kernel VFS entry points (regparm(3), resolved at insmod) ---- */
extern "C" void     *filp_open(const char *filename, int flags, int mode);
extern "C" int       filp_close(void *filp, void *id);
extern "C" long long generic_file_llseek(void *file, long long offset, int whence);

/*
 * IS_ERR: the real object code is `cmp $0xfffff000,%eax; ja`. Writing
 * the threshold as (unsigned long)-4096 is the kernel's own
 * IS_ERR_VALUE idiom and is width-correct on BOTH targets: on -m32 it is
 * 0xfffff000 (bit-exact with the disassembly), and on the 64-bit host
 * KAT it is 0xfffffffffffff000, so real high host heap pointers are
 * correctly NOT treated as error pointers while small negative ERR_PTR
 * values still are.
 */
static inline int stg_is_err_ptr(void *p)
{
	return (unsigned long)p > (unsigned long)-4096;
}

extern "C" void *CSTGFile_Open(const char *path, int mode)
{
	/* mode -> open flags + required f_mode bit. The five-entry table
	 * is the real .rodata table at .rodata+0xaef4 (verified verbatim);
	 * the f_mode bit is `(flags & 1) ? 2 : 1` (FMODE_WRITE : FMODE_READ),
	 * exactly the disassembly's sbb/add dance. */
	int flags;
	unsigned int requiredFMode;
	if ((unsigned int)mode > 4) {
		flags = 0;
		requiredFMode = 1;
	} else {
		static const int kOpenFlagTable[5] = {
			0x000,   /* mode 0: O_RDONLY                       */
			0x800,   /* mode 1: O_RDONLY | O_NONBLOCK          */
			0x041,   /* mode 2: O_WRONLY | O_CREAT             */
			0x241,   /* mode 3: O_WRONLY | O_CREAT | O_TRUNC   */
			0x042,   /* mode 4: O_RDWR   | O_CREAT             */
		};
		flags = kOpenFlagTable[mode];
		requiredFMode = (flags & 1) ? 2u : 1u;
	}

	void *filp = filp_open(path, flags, 0x1a0 /* 0644 */);
	if (filp == 0)
		return 0;
	if (stg_is_err_ptr(filp))
		return 0;

	/* Verify the file actually opened with the expected access; if not,
	 * close it and report failure. */
	unsigned int fmode = *(unsigned int *)((unsigned char *)filp + 0x20);
	if ((fmode & requiredFMode) != 0)
		return filp;

	filp_close(filp, 0);
	return 0;
}

extern "C" int CSTGFile_Close(void *handle)
{
	if (handle != 0)
		filp_close(handle, 0);
	return 0;
}

extern "C" int CSTGFile_Seek(void *handle, int offset, int whence)
{
	if (handle == 0)
		return 0;
	/* offset is a signed 32-bit value sign-extended to the 64-bit
	 * loff_t argument (the `sar $0x1f` in the disassembly). */
	return (int)generic_file_llseek(handle, (long long)offset, whence);
}

extern "C" unsigned int CSTGFile_GetFileSize(void *handle)
{
	if (handle == 0)
		return 0;
	/* file->f_path.dentry->d_inode->i_size (low dword). */
	unsigned char *dentry = *(unsigned char **)((unsigned char *)handle + 0x0c);
	unsigned char *inode  = *(unsigned char **)(dentry + 0x10);
	return *(unsigned int *)(inode + 0x40);
}
