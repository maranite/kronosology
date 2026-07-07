// SPDX-License-Identifier: GPL-2.0
/*
 * test_file_io.cpp  -  host-side known-answer tests for the four
 * CSTGFile_* VFS wrappers reconstructed in src/init/file_io.cpp
 * (sec 10.180): CSTGFile_Open, CSTGFile_Close, CSTGFile_Seek,
 * CSTGFile_GetFileSize.
 *
 * Links only src/init/file_io.cpp and mocks the three kernel entry
 * points (filp_open/filp_close/generic_file_llseek), recording their
 * arguments so the wrappers' flag-mapping, null-guarding, IS_ERR
 * handling, f_mode verification and struct-offset traversal can each be
 * asserted exactly. Fake struct file/dentry/inode are byte buffers laid
 * out to the confirmed 2.6.32/x86-32 offsets the wrappers read.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) { printf("  ok    %-52s 0x%lx\n", label, got); return; }
	printf("  FAIL  %-52s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

/* ---- the wrappers under test ---- */
extern "C" void        *CSTGFile_Open(const char *path, int mode);
extern "C" int          CSTGFile_Close(void *handle);
extern "C" int          CSTGFile_Seek(void *handle, int offset, int whence);
extern "C" unsigned int CSTGFile_GetFileSize(void *handle);

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

/* ---- fake VFS structs, laid out to the offsets the wrappers read ---- */
static unsigned char g_inode[128];
static unsigned char g_dentry[64];
static unsigned char g_filp[64];
static void *make_filp(unsigned int fmode, unsigned int i_size)
{
	memset(g_inode, 0, sizeof g_inode);
	memset(g_dentry, 0, sizeof g_dentry);
	memset(g_filp, 0, sizeof g_filp);
	*(unsigned int *)(g_inode + 0x40) = i_size;             /* inode.i_size (low) */
	*(unsigned char **)(g_dentry + 0x10) = g_inode;         /* dentry.d_inode     */
	*(unsigned char **)(g_filp + 0x0c) = g_dentry;          /* file.f_path.dentry */
	*(unsigned int *)(g_filp + 0x20) = fmode;               /* file.f_mode        */
	return g_filp;
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

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
