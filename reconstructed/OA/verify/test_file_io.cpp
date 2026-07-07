// SPDX-License-Identifier: GPL-2.0
/*
 * test_file_io.cpp  -  host-side known-answer tests for the CSTGFile_*
 * VFS wrappers reconstructed in src/init/file_io.cpp.
 *
 * Sec 10.180 (sections [1]-[4] below): CSTGFile_Open, CSTGFile_Close,
 * CSTGFile_Seek, CSTGFile_GetFileSize.
 *
 * Sec 10.181 (sections [5]-[9]): CSTGFile_Read, CSTGFile_Write,
 * CSTGFile_FileExists, CSTGFile_FreeReadBuffer,
 * CSTGFile_ReadFileIntoNewBuffer -- plus a safe host mock of the
 * stg_set_fs()/stg_restore_fs() opaque set_fs helper pair (the real
 * bodies, in src/stub/bar2_stubs_c.cpp, do genuine `esp & 0xffffe000`
 * thread_info-locating inline asm that is NOT safe to execute against a
 * live host stack -- same treatment as verify/test_init_module.cpp's own
 * stg_get_current_task() mock).
 *
 * Links only src/init/file_io.cpp and mocks the kernel entry points
 * (filp_open/filp_close/generic_file_llseek/vmalloc/vfree) plus a fake
 * file_operations read/write pair, recording arguments so the wrappers'
 * flag-mapping, null-guarding, IS_ERR handling, f_mode verification,
 * EOF-clamping, set_fs bracketing and struct-offset traversal can each be
 * asserted exactly. Fake struct file/dentry/inode/file_operations are
 * byte buffers laid out to the confirmed 2.6.32/x86-32 offsets the
 * wrappers read.
 *
 * g_inode/g_dentry/g_fops are mmap32()'d (own-test-bug fix, sec 10.181):
 * file_io.cpp now reads dentry/d_inode/f_op as packed 32-bit values via
 * FromU32() (see that file's own header note on why), so these targets'
 * addresses must round-trip losslessly through a 32-bit truncation --
 * a plain static array's address is NOT guaranteed to fit in 32 bits on
 * a PIE-linked 64-bit host (confirmed: this crashed on the very first
 * run, corrupting the dentry pointer, before this fix).
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>

static void *mmap32(unsigned long size)
{
	return mmap(0, size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
}

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) { printf("  ok    %-52s 0x%lx\n", label, got); return; }
	printf("  FAIL  %-52s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

/* ---- the wrappers under test ---- */
extern "C" void          *CSTGFile_Open(const char *path, int mode);
extern "C" int             CSTGFile_Close(void *handle);
extern "C" int             CSTGFile_Seek(void *handle, int offset, int whence);
extern "C" unsigned int    CSTGFile_GetFileSize(void *handle);
extern "C" int             CSTGFile_Read(void *handle, void *buf, unsigned int size);
extern "C" int             CSTGFile_Write(void *handle, const void *buf, unsigned int count);
extern "C" int             CSTGFile_FileExists(const char *path);
extern "C" void            CSTGFile_FreeReadBuffer(unsigned char *buf);
extern "C" unsigned char  *CSTGFile_ReadFileIntoNewBuffer(const char *path, unsigned int *outLen);

/* ---- kernel VFS mocks ---- */
static void       *g_filpOpenReturn;
static const char *g_filpOpenPath;
static int         g_filpOpenFlags;
static int         g_filpOpenMode;
extern "C" void *filp_open(const char *filename, int flags, int mode)
{
	g_filpOpenPath = filename; g_filpOpenFlags = flags; g_filpOpenMode = mode;
	return g_filpOpenReturn;
}

static int   g_filpCloseCalls;
static void *g_lastClosed;
extern "C" int filp_close(void *filp, void *id)
{
	g_filpCloseCalls++; g_lastClosed = filp; (void)id; return 0;
}

static int       g_llseekCalls;
static void     *g_llseekFile;
static long long g_llseekOffset;
static int       g_llseekWhence;
static long long g_llseekReturn;
extern "C" long long generic_file_llseek(void *file, long long offset, int whence)
{
	g_llseekCalls++; g_llseekFile = file; g_llseekOffset = offset; g_llseekWhence = whence;
	return g_llseekReturn;
}

/* ---- vmalloc/vfree mocks (ReadFileIntoNewBuffer) ---- */
static int           g_vmallocCalls;
static unsigned long  g_lastVmallocSize;
static bool           g_forceVmallocFail;
static int            g_vfreeCalls;
static void          *g_lastVfreePtr;
extern "C" void *vmalloc(unsigned long size)
{
	g_vmallocCalls++; g_lastVmallocSize = size;
	if (g_forceVmallocFail)
		return 0;
	return malloc(size);
}
extern "C" void vfree(void *addr)
{
	g_vfreeCalls++; g_lastVfreePtr = addr;
	free(addr);
}

/*
 * ---- stg_set_fs/stg_restore_fs mock (opaque set_fs pair) ----
 * The real bodies do genuine `esp & 0xffffe000` thread_info-locating
 * inline asm -- not safe to execute against a live host stack. This
 * mock instead tracks a single fake "current addr_limit" global so the
 * bracketing (set to KERNEL_DS before dispatch, restored to the saved
 * old value after) can be asserted exactly, including sampling the
 * fake limit from INSIDE the mock read/write dispatch below.
 */
static unsigned long g_fakeFsLimit = 0x12345678UL; /* arbitrary non-KERNEL_DS "old" value */
static int           g_setFsCalls;
static unsigned long  g_lastSetFsArg;
static int            g_restoreFsCalls;
static unsigned long  g_lastRestoreFsArg;
extern "C" unsigned long stg_set_fs(unsigned long newLimit)
{
	g_setFsCalls++; g_lastSetFsArg = newLimit;
	unsigned long old = g_fakeFsLimit;
	g_fakeFsLimit = newLimit;
	return old;
}
extern "C" void stg_restore_fs(unsigned long oldLimit)
{
	g_restoreFsCalls++; g_lastRestoreFsArg = oldLimit;
	g_fakeFsLimit = oldLimit;
}

/* ---- fake file_operations read/write dispatch (CSTGFile_Read/Write/
 * ReadFileIntoNewBuffer all call through file->f_op->read/write) ---- */
static int        g_readCalls;
static void       *g_readFile;
static void       *g_readBuf;
static unsigned int g_readCount;
static long long   *g_readPpos;
static int          g_readReturn;
static unsigned long g_fsLimitDuringRead;
extern "C" int mock_read(void *file, void *buf, unsigned int count, long long *ppos)
{
	g_readCalls++; g_readFile = file; g_readBuf = buf; g_readCount = count; g_readPpos = ppos;
	g_fsLimitDuringRead = g_fakeFsLimit;
	return g_readReturn;
}

static int        g_writeCalls;
static void       *g_writeFile;
static const void *g_writeBuf;
static unsigned int g_writeCount;
static long long   *g_writePpos;
static int          g_writeReturn;
static unsigned long g_fsLimitDuringWrite;
extern "C" int mock_write(void *file, const void *buf, unsigned int count, long long *ppos)
{
	g_writeCalls++; g_writeFile = file; g_writeBuf = buf; g_writeCount = count; g_writePpos = ppos;
	g_fsLimitDuringWrite = g_fakeFsLimit;
	return g_writeReturn;
}

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

/*
 * ---- fake VFS structs, laid out to the offsets the wrappers read ----
 * g_inode/g_dentry/g_fops are mmap32()'d (see file header comment) since
 * their addresses get stored as packed-32-bit values via ToU32(), read
 * back by file_io.cpp's own FromU32(). g_filp stays a plain static array
 * -- its OWN address is only ever used directly as an opaque host
 * handle, never reconstituted from a packed 32-bit field.
 */
static unsigned char *g_inode;
static unsigned char *g_dentry;
static unsigned char  g_filp[64];
/*
 * g_fops: the real file_operations struct has read/write 4 bytes apart
 * (+0x8/+0xc, matching the real 32-bit target's 4-byte pointers) -- but
 * a HOST pointer is 8 bytes, so storing two adjacent native `void*`
 * mocks at those exact offsets would overlap by 4 bytes and corrupt
 * whichever is written first (own-test-harness bug, caught before ever
 * running this: an 8-byte store at +0x8 spans bytes [0x8,0x10), a second
 * 8-byte store at +0xc spans [0xc,0x14) -- these overlap in [0xc,0x10)).
 * Since no single CSTGFile_Read/Write call ever dereferences BOTH slots
 * at once (Read only touches +0x8, Write only touches +0xc), the fix is
 * to never populate both slots in the same make_filp() call -- wire
 * only the ONE the upcoming test actually needs via wire_read_fop()/
 * wire_write_fop() below, leaving the other slot's bytes as whatever
 * zero/garbage make_filp()'s memset left (never dereferenced). (The
 * read/write function pointers themselves stay native 8-byte host
 * pointers -- they must be real, directly callable addresses, unlike
 * dentry/d_inode/f_op which are only ever read back as data.)
 */
static unsigned char *g_fops;
static void *make_filp(unsigned int fmode, unsigned int i_size)
{
	if (!g_inode) g_inode = (unsigned char *)mmap32(128);
	if (!g_dentry) g_dentry = (unsigned char *)mmap32(64);
	if (!g_fops) g_fops = (unsigned char *)mmap32(32);
	memset(g_inode, 0, 128);
	memset(g_dentry, 0, 64);
	memset(g_filp, 0, sizeof g_filp);
	memset(g_fops, 0, 32);
	*(unsigned int *)(g_inode + 0x40) = i_size;             /* inode.i_size (low) */
	*(unsigned int *)(g_dentry + 0x10) = ToU32(g_inode);    /* dentry.d_inode     */
	*(unsigned int *)(g_filp + 0x0c) = ToU32(g_dentry);     /* file.f_path.dentry */
	*(unsigned int *)(g_filp + 0x20) = fmode;               /* file.f_mode        */
	*(unsigned int *)(g_filp + 0x10) = ToU32(g_fops);       /* file.f_op          */
	return g_filp;
}
static void wire_read_fop(void *filp)
{
	unsigned char *fops = (unsigned char *)(unsigned long)(*(unsigned int *)((unsigned char *)filp + 0x10));
	*(void **)(fops + 0x8) = (void *)&mock_read;   /* file_operations.read */
}
static void wire_write_fop(void *filp)
{
	unsigned char *fops = (unsigned char *)(unsigned long)(*(unsigned int *)((unsigned char *)filp + 0x10));
	*(void **)(fops + 0xc) = (void *)&mock_write;  /* file_operations.write */
}
/* Sets file.f_pos (loff_t, +0x24 lo/+0x28 hi) and inode.i_size's high
 * dword (+0x44) on an already-made_filp() handle -- used by the
 * EOF-clamp tests, which need i_size/f_pos beyond what make_filp()'s
 * plain 32-bit i_size argument can express. */
static void set_pos_and_isize_hi(void *filp, unsigned int fposLo, unsigned int fposHi, unsigned int isizeHi)
{
	unsigned char *f = (unsigned char *)filp;
	*(unsigned int *)(f + 0x24) = fposLo;
	*(unsigned int *)(f + 0x28) = fposHi;
	unsigned char *dentry = (unsigned char *)(unsigned long)(*(unsigned int *)(f + 0x0c));
	unsigned char *inode  = (unsigned char *)(unsigned long)(*(unsigned int *)(dentry + 0x10));
	*(unsigned int *)(inode + 0x44) = isizeHi;
}

int main(void)
{
	printf("CSTGFile_* VFS-wrapper known-answer test\n");
	printf("========================================\n");

	printf("[1] CSTGFile_GetFileSize: file->dentry->inode->i_size traversal\n");
	check_eq("NULL handle -> 0", CSTGFile_GetFileSize(0), 0);
	{
		void *f = make_filp(0x3, 0x123456);
		check_eq("i_size 0x123456 read through 3 hops",
			 CSTGFile_GetFileSize(f), 0x123456);
		f = make_filp(0x3, 0);
		check_eq("i_size 0 read cleanly", CSTGFile_GetFileSize(f), 0);
	}

	printf("[2] CSTGFile_Close: filp_close(filp, NULL), guarded on NULL\n");
	g_filpCloseCalls = 0;
	check_eq("CSTGFile_Close(NULL) returns 0", CSTGFile_Close(0), 0);
	check_eq("...and does NOT call filp_close", g_filpCloseCalls, 0);
	{
		void *f = make_filp(0x3, 0);
		check_eq("CSTGFile_Close(filp) returns 0", CSTGFile_Close(f), 0);
		check_eq("...calls filp_close exactly once", g_filpCloseCalls, 1);
		check_eq("...on the right handle", (unsigned long)(g_lastClosed == f), 1);
	}

	printf("[3] CSTGFile_Seek: generic_file_llseek, offset sign-extended\n");
	g_llseekCalls = 0;
	check_eq("CSTGFile_Seek(NULL,..) returns 0", CSTGFile_Seek(0, 5, 1), 0);
	check_eq("...does NOT call generic_file_llseek", g_llseekCalls, 0);
	{
		void *f = make_filp(0x3, 0);
		g_llseekReturn = 0x00000000AABBCCDDULL;  /* low dword returned */
		check_eq("returns low 32 bits of llseek result",
			 (unsigned int)CSTGFile_Seek(f, 16, 0), 0xAABBCCDD);
		check_eq("passed the handle through", (unsigned long)(g_llseekFile == f), 1);
		check_eq("whence forwarded verbatim", (unsigned long)g_llseekWhence, 0);
		check_eq("positive offset forwarded", (unsigned long)g_llseekOffset, 16);
		/* negative offset must sign-extend to a negative loff_t */
		CSTGFile_Seek(f, -1, 2);
		check_eq("offset -1 sign-extends to 0xffffffffffffffff",
			 (unsigned long)g_llseekOffset, (unsigned long)-1LL);
	}

	printf("[4] CSTGFile_Open: flag mapping, IS_ERR, f_mode check\n");
	/* NULL from filp_open -> NULL */
	g_filpOpenReturn = 0;
	check_eq("filp_open()==NULL -> Open returns NULL",
		 (unsigned long)CSTGFile_Open("p", 0), 0);
	/* ERR_PTR from filp_open -> NULL (IS_ERR) */
	g_filpOpenReturn = (void *)-13;   /* -EACCES-shaped error pointer */
	check_eq("filp_open() ERR_PTR -> Open returns NULL",
		 (unsigned long)CSTGFile_Open("p", 0), 0);

	/* valid filp whose f_mode has the required bit -> returned as-is;
	 * verify the mode->flags mapping for every table entry. */
	{
		struct { int mode; int flags; unsigned int fmode; } cases[] = {
			{0, 0x000, 0x1}, {1, 0x800, 0x1}, {2, 0x041, 0x2},
			{3, 0x241, 0x2}, {4, 0x042, 0x1}, {7, 0x000, 0x1}, /* mode>4 -> flags 0, bit READ */
		};
		for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
			void *f = make_filp(cases[i].fmode, 0);
			g_filpOpenReturn = f;
			void *r = CSTGFile_Open("path", cases[i].mode);
			char lbl[80];
			snprintf(lbl, sizeof lbl, "mode %d -> flags 0x%x", cases[i].mode, cases[i].flags);
			check_eq(lbl, (unsigned long)g_filpOpenFlags, (unsigned long)cases[i].flags);
			snprintf(lbl, sizeof lbl, "mode %d -> valid filp returned", cases[i].mode);
			check_eq(lbl, (unsigned long)(r == f), 1);
		}
		check_eq("filp_open always called with mode 0644", (unsigned long)g_filpOpenMode, 0x1a0);
	}

	/* valid filp lacking the required f_mode bit -> closed, NULL returned */
	{
		void *f = make_filp(0x0 /* neither READ nor WRITE */, 0);
		g_filpOpenReturn = f;
		g_filpCloseCalls = 0;
		void *r = CSTGFile_Open("path", 0);   /* mode 0 requires bit 1 */
		check_eq("f_mode missing required bit -> Open returns NULL",
			 (unsigned long)r, 0);
		check_eq("...and the opened file is closed", g_filpCloseCalls, 1);
	}

	printf("[5] CSTGFile_Read: NULL guard, set_fs bracketing, EOF clamp, error clamp\n");
	check_eq("CSTGFile_Read(NULL,..) returns 0", CSTGFile_Read(0, 0, 100), 0);
	{
		unsigned char buf[16];
		void *f = make_filp(0x3, 0x100 /* i_size = 256 */);
		wire_read_fop(f);
		set_pos_and_isize_hi(f, 0, 0, 0);

		g_readCalls = 0; g_setFsCalls = 0; g_restoreFsCalls = 0;
		g_readReturn = 10;
		int r = CSTGFile_Read(f, buf, 10);
		check_eq("no-clamp read: return value == bytes read", r, 10);
		check_eq("...f_op->read called exactly once", g_readCalls, 1);
		check_eq("...count NOT clamped (10 < i_size)", g_readCount, 10);
		check_eq("...file arg forwarded", (unsigned long)(g_readFile == f), 1);
		check_eq("...buf arg forwarded", (unsigned long)(g_readBuf == buf), 1);
		check_eq("...ppos == &file->f_pos", (unsigned long)((unsigned char *)g_readPpos == (unsigned char *)f + 0x24), 1);
		check_eq("...set_fs(KERNEL_DS) called once before dispatch", g_setFsCalls, 1);
		check_eq("...new limit was KERNEL_DS (0xffffffff)", g_lastSetFsArg, 0xffffffffUL);
		check_eq("...KERNEL_DS was active DURING the read call", g_fsLimitDuringRead, 0xffffffffUL);
		check_eq("...restore_fs called once after dispatch", g_restoreFsCalls, 1);
		check_eq("...restored to the pre-call fake limit", g_lastRestoreFsArg, 0x12345678UL);
		check_eq("...fake limit restored after the call", g_fakeFsLimit, 0x12345678UL);

		/* request runs past EOF (fpos=0, i_size=0x100, size=0x200) -> count clamped to i_size-fpos */
		g_readReturn = 0x100; /* mock "reads" the clamped count fully */
		r = CSTGFile_Read(f, buf, 0x200);
		check_eq("EOF clamp: count clamped to i_size-fpos", g_readCount, 0x100);
		check_eq("EOF clamp: return value unaffected (own-fastpath quirk)", r, 0x100);

		/* exact-EOF request (size == i_size-fpos) is NOT clamped (sum==isize, not >) */
		g_readReturn = 0x100;
		r = CSTGFile_Read(f, buf, 0x100);
		check_eq("exact-to-EOF request not clamped", g_readCount, 0x100);

		/* negative (error) return from f_op->read is clamped to 0 */
		g_readReturn = -5;
		r = CSTGFile_Read(f, buf, 10);
		check_eq("negative f_op->read result clamped to 0", r, 0);

		/* nonzero fpos narrows the clamp further */
		set_pos_and_isize_hi(f, 0x80 /* fpos=128 */, 0, 0);
		g_readReturn = 0x80;
		r = CSTGFile_Read(f, buf, 0x200);
		check_eq("EOF clamp honors nonzero f_pos (0x100-0x80=0x80)", g_readCount, 0x80);
	}

	printf("[6] CSTGFile_Write: NO null guard (confirmed real asymmetry), set_fs bracketing\n");
	{
		void *f = make_filp(0x3, 0);
		wire_write_fop(f);
		g_writeCalls = 0; g_setFsCalls = 0; g_restoreFsCalls = 0;
		g_writeReturn = 7;
		const char msg[] = "hi";
		int r = CSTGFile_Write(f, msg, 2);
		check_eq("write result forwarded", r, 7);
		check_eq("f_op->write called exactly once", g_writeCalls, 1);
		check_eq("count forwarded unclamped (Write never EOF-clamps)", g_writeCount, 2);
		check_eq("buf arg forwarded", (unsigned long)(g_writeBuf == msg), 1);
		check_eq("ppos == &file->f_pos", (unsigned long)((unsigned char *)g_writePpos == (unsigned char *)f + 0x24), 1);
		check_eq("set_fs(KERNEL_DS) called once", g_setFsCalls, 1);
		check_eq("KERNEL_DS active during the write call", g_fsLimitDuringWrite, 0xffffffffUL);
		check_eq("restore_fs called once", g_restoreFsCalls, 1);

		/* negative (error) return clamped to 0 */
		g_writeReturn = -1;
		r = CSTGFile_Write(f, msg, 2);
		check_eq("negative f_op->write result clamped to 0", r, 0);
	}

	printf("[7] CSTGFile_FileExists: filp_open + f_mode gate, always closed\n");
	{
		/* NULL from filp_open -> 0 */
		g_filpOpenReturn = 0;
		check_eq("filp_open()==NULL -> 0", CSTGFile_FileExists("p"), 0);
		/* ERR_PTR -> 0 */
		g_filpOpenReturn = (void *)-2;
		check_eq("filp_open() ERR_PTR -> 0", CSTGFile_FileExists("p"), 0);
		/* valid filp, FMODE_READ set -> 1, closed */
		void *f = make_filp(0x1, 0);
		g_filpOpenReturn = f;
		g_filpCloseCalls = 0;
		check_eq("valid filp with FMODE_READ -> 1", CSTGFile_FileExists("p"), 1);
		check_eq("...and closed", g_filpCloseCalls, 1);
		/* valid filp, FMODE_READ clear -> 0, still closed */
		f = make_filp(0x2 /* FMODE_WRITE only */, 0);
		g_filpOpenReturn = f;
		g_filpCloseCalls = 0;
		check_eq("valid filp without FMODE_READ -> 0", CSTGFile_FileExists("p"), 0);
		check_eq("...and still closed", g_filpCloseCalls, 1);
	}

	printf("[8] CSTGFile_FreeReadBuffer: plain vfree() forward\n");
	{
		void *p = malloc(4);
		g_vfreeCalls = 0; g_lastVfreePtr = 0;
		CSTGFile_FreeReadBuffer((unsigned char *)p);
		check_eq("vfree called exactly once", g_vfreeCalls, 1);
		check_eq("...on the right pointer", (unsigned long)(g_lastVfreePtr == p), 1);
	}

	printf("[9] CSTGFile_ReadFileIntoNewBuffer: open+size+alloc+read+NUL-terminate+close\n");
	{
		/* open failure -> NULL, outLen untouched by filp_open failure itself */
		g_filpOpenReturn = 0;
		unsigned int outLen = 0xdeadbeef;
		check_eq("filp_open()==NULL -> NULL",
			 (unsigned long)CSTGFile_ReadFileIntoNewBuffer("p", &outLen), 0);

		/* zero-size file -> NULL, but outLen IS written (0), confirmed quirk */
		void *f = make_filp(0x1, 0);
		g_filpOpenReturn = f;
		g_filpCloseCalls = 0;
		outLen = 0xdeadbeef;
		check_eq("zero-size file -> NULL",
			 (unsigned long)CSTGFile_ReadFileIntoNewBuffer("p", &outLen), 0);
		check_eq("...outLen still written (0)", outLen, 0);
		check_eq("...filp closed", g_filpCloseCalls, 1);

		/* vmalloc failure -> NULL, outLen already holds the real file size (quirk) */
		f = make_filp(0x1, 12);
		g_filpOpenReturn = f;
		g_filpCloseCalls = 0;
		g_forceVmallocFail = true;
		outLen = 0;
		check_eq("vmalloc failure -> NULL",
			 (unsigned long)CSTGFile_ReadFileIntoNewBuffer("p", &outLen), 0);
		check_eq("...outLen holds real file size despite the NULL return (confirmed quirk)", outLen, 12);
		check_eq("...filp closed", g_filpCloseCalls, 1);
		g_forceVmallocFail = false;

		/* short read (result != fileSize) -> NULL, buffer vfree'd, outLen still holds fileSize */
		f = make_filp(0x1, 12);
		wire_read_fop(f);
		g_filpOpenReturn = f;
		g_filpCloseCalls = 0;
		g_vfreeCalls = 0;
		g_readReturn = 5; /* short of the requested/expected 12 */
		outLen = 0;
		check_eq("short read -> NULL",
			 (unsigned long)CSTGFile_ReadFileIntoNewBuffer("p", &outLen), 0);
		check_eq("...outLen still holds real file size", outLen, 12);
		check_eq("...buffer vfree'd on short-read failure", g_vfreeCalls, 1);
		check_eq("...filp closed", g_filpCloseCalls, 1);

		/* full success: exact read, buffer NUL-terminated at byte [fileSize] */
		f = make_filp(0x1, 5);
		wire_read_fop(f);
		g_filpOpenReturn = f;
		g_filpCloseCalls = 0;
		g_readReturn = 5;
		outLen = 0;
		unsigned char *buf = CSTGFile_ReadFileIntoNewBuffer("p", &outLen);
		check_eq("full success -> non-NULL buffer", (unsigned long)(buf != 0), 1);
		check_eq("...outLen == file size", outLen, 5);
		if (buf) {
			check_eq("...NUL-terminated at buf[fileSize]", buf[5], 0);
			check_eq("...vmalloc size == fileSize+1", g_lastVmallocSize, 6);
		}
		check_eq("...filp closed on success too", g_filpCloseCalls, 1);
		CSTGFile_FreeReadBuffer(buf);
	}

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
