// SPDX-License-Identifier: GPL-2.0
/*
 * file_io.cpp  -  the CSTGFile_* kernel-VFS wrapper primitives, promoted
 * from their inert placeholders in src/stub/bar2_stubs_c.cpp to faithful
 * reconstructions from the real OA_322.ko disassembly (sec 10.180/10.181).
 *
 * Batch 32 (sec 10.180) did the four that DON'T manipulate the per-CPU
 * thread_info addr_limit (set_fs): CSTGFile_Open, CSTGFile_Close,
 * CSTGFile_Seek, CSTGFile_GetFileSize.
 *
 * This batch (sec 10.181) closes out the rest of the cluster:
 * CSTGFile_Read/CSTGFile_Write (the set_fs + f_op->read/write dispatch
 * pair), CSTGFile_FileExists, CSTGFile_FreeReadBuffer, and
 * CSTGFile_ReadFileIntoNewBuffer (already declared/called from
 * src/auth/parse_auth.cpp). The set_fs manipulation itself (real
 * disassembly: `mov %esp,%reg; and $0xffffe000,%reg; ...0x18(%reg)...`,
 * locating current_thread_info() via the esp&~(THREAD_SIZE-1) trick and
 * reading/writing its +0x18 addr_limit field) is NOT safely
 * host-executable -- it would read/write live host stack memory rather
 * than a kernel thread_info -- so it's represented via the opaque
 * stg_set_fs()/stg_restore_fs() helper pair (real bodies in
 * src/stub/bar2_stubs_c.cpp, alongside stg_get_current_task(), same
 * host/target divergence pattern already established there and in
 * init_module.cpp's own `current`-accessor treatment).
 *
 * These are thin wrappers over the Linux 2.6.32 VFS. The `struct file`
 * handle is treated as an opaque `void *` throughout (the project's
 * already-established handle-based CSTGFile_* ABI). Kernel struct field
 * accesses are raw byte-offset loads transcribed verbatim from the
 * disassembly -- the confirmed 2.6.32/x86-32 offsets:
 *     file  +0x0c = f_path.dentry
 *     file  +0x10 = f_op
 *     file  +0x20 = f_mode          (fmode_t / u32)
 *     file  +0x24 = f_pos           (loff_t, 8 bytes: +0x24 lo / +0x28 hi)
 *     dentry+0x10 = d_inode
 *     inode +0x40 = i_size          (loff_t, 8 bytes: +0x40 lo / +0x44 hi)
 *     file_operations +0x8  = read
 *     file_operations +0xc  = write
 * On the real -m32 build these are 4-byte pointer / 4-byte scalar loads
 * exactly as the kernel lays them out; the host KAT builds fake structs
 * with a matching byte layout so the identical source is testable there.
 *
 * NOTE (sec 10.181, own-bug fix): `f_path.dentry` (+0xc) and `f_op`
 * (+0x10) are only 4 bytes apart in the real (32-bit-pointer) struct --
 * on this 64-bit host a native `unsigned char **` read/write of either
 * spans 8 bytes and physically overlaps the other. Batch 32's own
 * dentry-only reads never hit this (nothing else was modeled at +0x10
 * yet); this batch's f_op need forced the issue, caught by a real host
 * KAT segfault (CSTGFile_GetFileSize crashing on a corrupted dentry
 * pointer), not by inspection. Fixed project-wide in this file by
 * reading every kernel-struct POINTER field (dentry, d_inode, f_op) as
 * an explicit 32-bit value and reconstituting via FromU32() -- this
 * project's established ToU32()/FromU32() convention (see e.g.
 * src/engine/playback_subrate.cpp), applied here to a field this code
 * only ever READS (never writes), hence only FromU32() is needed.
 * Identical behavior on the real -m32 target (pointers are already 4
 * bytes there) and immune to host 8-byte-pointer overlap regardless of
 * which adjacent fields a future batch adds. Plain scalar fields
 * (f_mode, f_pos, i_size) are unaffected -- they were never pointers.
 */
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }
/*
 * The kernel entry points (filp_open/filp_close/generic_file_llseek/
 * vmalloc/vfree, plus the f_op->read/write function-pointer dispatch)
 * are ordinary regparm(3) kernel functions (NOT asmlinkage -- unlike the
 * printk family, see init_module.cpp's regparm(0) note), so a plain
 * extern "C" declaration / plain function-pointer typedef under this
 * file's -mregparm=3 default matches their real ABI. All are undefined
 * (`U`) in the real OA.ko as well, resolved at insmod time -- this batch
 * adds vmalloc/vfree as two further legitimate new externs (36 -> 38).
 */

/* ---- kernel VFS entry points (regparm(3), resolved at insmod) ---- */
extern "C" void     *filp_open(const char *filename, int flags, int mode);
extern "C" int       filp_close(void *filp, void *id);
extern "C" long long generic_file_llseek(void *file, long long offset, int whence);
extern "C" void     *vmalloc(unsigned long size);
extern "C" void      vfree(void *addr);

/* ---- opaque set_fs helper pair (see file header comment above);
 * real bodies in src/stub/bar2_stubs_c.cpp ---- */
extern "C" unsigned long stg_set_fs(unsigned long newLimit);
extern "C" void          stg_restore_fs(unsigned long oldLimit);

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
	unsigned char *dentry = FromU32(*(unsigned int *)((unsigned char *)handle + 0x0c));
	unsigned char *inode  = FromU32(*(unsigned int *)(dentry + 0x10));
	return *(unsigned int *)(inode + 0x40);
}

/*
 * Shared EOF-clamp helper (sec 10.181), factored out of the two real
 * call sites that each embed this exact cascade verbatim in the
 * ground-truth disassembly (CSTGFile_Read and
 * CSTGFile_ReadFileIntoNewBuffer -- compiler-duplicated there, not
 * shared; sharing it here is a non-observable refactor since both
 * copies are instruction-for-instruction identical). Confirmed real
 * quirks preserved exactly, NOT "cleaned up":
 *   - the 64-bit `fpos + count` sum is compared against `isize` with a
 *     SIGNED 64-bit compare (loff_t is signed; matches the real
 *     high-dword-signed / low-dword-unsigned jge/jg/jbe cascade);
 *   - clamping triggers only on `sum > isize`, never on exact equality
 *     (reading exactly to EOF is not clamped);
 *   - the clamped replacement value is a PLAIN 32-bit `isizeLo - fposLo`
 *     subtract -- NOT a full 64-bit subtract -- so it silently
 *     wraps/misbehaves for files >4GB whose high dwords differ; the
 *     real object code does exactly this, transcribed verbatim rather
 *     than "fixed".
 */
static inline unsigned int stg_clamp_count_to_eof(unsigned int count,
						   unsigned int fposLo, unsigned int fposHi,
						   unsigned int isizeLo, unsigned int isizeHi)
{
	unsigned long long fpos  = ((unsigned long long)fposHi  << 32) | fposLo;
	unsigned long long isize = ((unsigned long long)isizeHi << 32) | isizeLo;
	unsigned long long sum   = fpos + (unsigned long long)count; /* count zero-extended, matching the real `xor edi,edi` + add/adc */

	if ((long long)sum > (long long)isize)
		count = isizeLo - fposLo;
	return count;
}

extern "C" int CSTGFile_Read(void *handle, void *buf, unsigned int size)
{
	if (handle == 0)
		return 0;

	unsigned char *file   = (unsigned char *)handle;
	unsigned char *dentry = FromU32(*(unsigned int *)(file + 0x0c));
	unsigned char *inode  = FromU32(*(unsigned int *)(dentry + 0x10));

	unsigned int fposLo  = *(unsigned int *)(file + 0x24);
	unsigned int fposHi  = *(unsigned int *)(file + 0x28);
	unsigned int isizeLo = *(unsigned int *)(inode + 0x40);
	unsigned int isizeHi = *(unsigned int *)(inode + 0x44);

	unsigned int count = stg_clamp_count_to_eof(size, fposLo, fposHi, isizeLo, isizeHi);

	/* f_op->read(file, buf, count, &f_pos); regparm(3): file=EAX,
	 * buf=EDX, count=ECX, &f_pos on the stack -- matches this file's
	 * default -mregparm=3, so a plain function-pointer typedef (no
	 * explicit attribute) is correct. The read/write function POINTERS
	 * themselves (unlike dentry/f_op above) are dereferenced as real
	 * CALLABLE host addresses, not reconstituted via FromU32 -- see the
	 * file_operations offset note in the header comment. */
	typedef int (*read_fn_t)(void *file, void *buf, unsigned int count, long long *ppos);
	unsigned char *fOp = FromU32(*(unsigned int *)(file + 0x10));
	read_fn_t readFn = *(read_fn_t *)(fOp + 0x8);

	unsigned long oldLimit = stg_set_fs(0xffffffffUL /* KERNEL_DS */);
	int r = readFn(handle, buf, count, (long long *)(file + 0x24));
	stg_restore_fs(oldLimit);

	/* Confirmed real: the disassembly's `cmp count,result; je <fastpath>`
	 * is a pure shortcut (both arms land on the same final value) --
	 * the actual returned value is simply "0 on error, else the raw
	 * result", regardless of whether it matched the requested count. */
	return (r < 0) ? 0 : r;
}

extern "C" int CSTGFile_Write(void *handle, const void *buf, unsigned int count)
{
	/* Confirmed real, a genuine asymmetry vs. CSTGFile_Read: there is
	 * NO handle==0 guard here at all -- the disassembly dereferences
	 * handle+0x10 unconditionally as its very first instruction.
	 * Preserved faithfully, not "fixed" to match Read's own NULL
	 * check. */
	unsigned char *file = (unsigned char *)handle;

	typedef int (*write_fn_t)(void *file, const void *buf, unsigned int count, long long *ppos);
	unsigned char *fOp = FromU32(*(unsigned int *)(file + 0x10));
	write_fn_t writeFn = *(write_fn_t *)(fOp + 0xc);

	unsigned long oldLimit = stg_set_fs(0xffffffffUL /* KERNEL_DS */);
	int r = writeFn(handle, buf, count, (long long *)(file + 0x24));
	stg_restore_fs(oldLimit);

	return (r < 0) ? 0 : r;
}

extern "C" int CSTGFile_FileExists(const char *path)
{
	void *filp = filp_open(path, 0 /* O_RDONLY */, 0x1a0 /* 0644 */);
	if (filp == 0)
		return 0;
	if (stg_is_err_ptr(filp))
		return 0;

	/* Confirmed real: requires the FMODE_READ bit, same gate as
	 * CSTGFile_Open's own `requiredFMode` check -- either way the filp
	 * is closed before returning. */
	unsigned int fmode = *(unsigned int *)((unsigned char *)filp + 0x20);
	if ((fmode & 1) == 0) {
		filp_close(filp, 0);
		return 0;
	}
	filp_close(filp, 0);
	return 1;
}

extern "C" void CSTGFile_FreeReadBuffer(unsigned char *buf)
{
	vfree(buf);
}

extern "C" unsigned char *CSTGFile_ReadFileIntoNewBuffer(const char *path, unsigned int *outLen)
{
	void *filp = filp_open(path, 0 /* O_RDONLY */, 0x1a0 /* 0644 */);
	if (filp == 0)
		return 0;
	if (stg_is_err_ptr(filp))
		return 0;

	unsigned char *file = (unsigned char *)filp;
	unsigned int fmode = *(unsigned int *)(file + 0x20);
	if ((fmode & 1) == 0) {
		filp_close(filp, 0);
		return 0;
	}

	unsigned char *dentry   = FromU32(*(unsigned int *)(file + 0x0c));
	unsigned char *inode    = FromU32(*(unsigned int *)(dentry + 0x10));
	unsigned int   fileSize = *(unsigned int *)(inode + 0x40); /* low dword of i_size */

	/* Confirmed real quirk: *outLen is written HERE, unconditionally,
	 * BEFORE any of the later failure checks (zero-size file, vmalloc
	 * failure, short read) -- on those failure paths outLen ends up
	 * holding the real file size even though the function returns
	 * NULL. Callers must key failure off the NULL return value, not
	 * off outLen -- preserved exactly, not "fixed" to zero outLen on
	 * every failure path. */
	*outLen = fileSize;

	if (fileSize == 0) {
		filp_close(filp, 0);
		return 0;
	}

	/* +1 for the null terminator this function always appends on
	 * success (confirmed: the final `movb $0,(%esi,%eax,1)` before the
	 * shared close+return tail). */
	unsigned char *newBuf = (unsigned char *)vmalloc(fileSize + 1);
	if (newBuf == 0) {
		filp_close(filp, 0);
		return 0;
	}

	unsigned int fposLo  = *(unsigned int *)(file + 0x24);
	unsigned int fposHi  = *(unsigned int *)(file + 0x28);
	unsigned int isizeLo = *(unsigned int *)(inode + 0x40);
	unsigned int isizeHi = *(unsigned int *)(inode + 0x44);
	unsigned int count = stg_clamp_count_to_eof(fileSize, fposLo, fposHi, isizeLo, isizeHi);

	typedef int (*read_fn_t)(void *file, void *buf, unsigned int count, long long *ppos);
	unsigned char *fOp = FromU32(*(unsigned int *)(file + 0x10));
	read_fn_t readFn = *(read_fn_t *)(fOp + 0x8);

	unsigned long oldLimit = stg_set_fs(0xffffffffUL /* KERNEL_DS */);
	int r = readFn(filp, newBuf, count, (long long *)(file + 0x24));
	stg_restore_fs(oldLimit);
	if (r < 0)
		r = 0;

	/* Confirmed real: unlike CSTGFile_Read's own redundant fast-path
	 * shortcut, THIS comparison is a genuine branch -- success requires
	 * the read to have returned EXACTLY the originally-stored full
	 * `fileSize` (not the possibly-clamped `count`); any short/partial
	 * read is a hard failure (buffer vfree'd, filp closed, NULL
	 * returned). */
	if ((unsigned int)r != fileSize) {
		vfree(newBuf);
		filp_close(filp, 0);
		return 0;
	}

	newBuf[r] = 0;
	filp_close(filp, 0);
	return newBuf;
}
