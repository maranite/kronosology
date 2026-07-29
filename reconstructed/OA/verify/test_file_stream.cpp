// SPDX-License-Identifier: GPL-2.0
/*
 * test_file_stream.cpp  -  host-side known-answer test for CFileStream
 * (see include/oa_file_stream.h for the full derivation). Mocks the
 * already-reconstructed CSTGFile_* primitives directly (file_io.cpp is
 * NOT linked here) so CFileStream's own error-flag/field logic can be
 * asserted in isolation.
 */

#include <cstdio>
#include <cstring>

#include "oa_file_stream.h"

static int g_fail;
static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) { printf("  ok    %-52s 0x%lx\n", label, got); return; }
	printf("  FAIL  %-52s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

/* ---- CSTGFile_* mocks ---- */
static void *g_openReturn;
static const char *g_openPath;
static int g_openMode;
static int g_openCalls;
extern "C" void *CSTGFile_Open(const char *path, int mode)
{
	g_openCalls++; g_openPath = path; g_openMode = mode;
	return g_openReturn;
}

static int g_closeCalls;
static void *g_lastClosed;
extern "C" int CSTGFile_Close(void *handle)
{
	g_closeCalls++; g_lastClosed = handle;
	return 0;
}

static int g_seekCalls;
static void *g_seekHandle;
static int g_seekOffset;
static int g_seekWhence;
static int g_seekReturn;
extern "C" int CSTGFile_Seek(void *handle, int offset, int whence)
{
	g_seekCalls++; g_seekHandle = handle; g_seekOffset = offset; g_seekWhence = whence;
	return g_seekReturn;
}

static unsigned int g_fileSizeReturn;
extern "C" unsigned int CSTGFile_GetFileSize(void *)
{
	return g_fileSizeReturn;
}

static int g_readCalls;
static void *g_readBuf;
static unsigned int g_readSize;
static int g_readReturn;
extern "C" int CSTGFile_Read(void *, void *buf, unsigned int size)
{
	g_readCalls++; g_readBuf = buf; g_readSize = size;
	return g_readReturn;
}

static int g_writeCalls;
static const void *g_writeBuf;
static unsigned int g_writeSize;
static int g_writeReturn;
extern "C" int CSTGFile_Write(void *, const void *buf, unsigned int count)
{
	g_writeCalls++; g_writeBuf = buf; g_writeSize = count;
	return g_writeReturn;
}

static int g_existsCalls;
static int g_existsReturn;
extern "C" int CSTGFile_FileExists(const char *)
{
	g_existsCalls++;
	return g_existsReturn;
}

static int g_getPosCalls;
static int g_getPosReturn;
extern "C" int CSTGFile_GetPosition(void *)
{
	g_getPosCalls++;
	return g_getPosReturn;
}

static int g_isAtEndCalls;
static bool g_isAtEndReturn;
extern "C" bool CSTGFile_IsAtEnd(void *)
{
	g_isAtEndCalls++;
	return g_isAtEndReturn;
}

static int g_flushCalls;
extern "C" void CSTGFile_Flush(void *)
{
	g_flushCalls++;
}

int main(void)
{
	printf("CFileStream known-answer test\n");
	printf("==============================\n");

	printf("[1] ctor: open success/failure -> error flag\n");
	{
		g_openReturn = (void *)0x1000; g_openCalls = 0;
		CFileStream ok("path.bin", 3, 0);
		check_eq("open() called with path/mode forwarded", g_openCalls, 1);
		check_eq("path forwarded verbatim", (unsigned long)(strcmp(g_openPath, "path.bin") == 0), 1);
		check_eq("mode forwarded verbatim", (unsigned long)g_openMode, 3);
		/* ChangeSize is a pure error-flag accessor (real body ignores
		 * its own arg -- see file_stream.cpp comment) -- use it to
		 * observe the ctor's success/failure outcome. */
		check_eq("open success -> errorFlag 0", ok.ChangeSize(0), 0);

		g_openReturn = 0;
		CFileStream fail("nope.bin", 0, 0);
		check_eq("open failure -> errorFlag 7", fail.ChangeSize(0), 7);
	}

	printf("[2] dtor: closes an open handle exactly once, guards NULL\n");
	{
		g_openReturn = (void *)0x2000; g_closeCalls = 0; g_lastClosed = 0;
		{
			CFileStream f("x", 0, 0);
			(void)f;
		}
		check_eq("dtor closed the real handle", g_closeCalls, 1);
		check_eq("...closed the handle the ctor opened", (unsigned long)g_lastClosed, 0x2000);

		g_openReturn = 0; g_closeCalls = 0;
		{
			CFileStream f("x", 0, 0); /* open fails -> mFileHandle stays 0 */
			(void)f;
		}
		check_eq("dtor does NOT close on a failed-open handle", g_closeCalls, 0);
	}

	printf("[3] Write/Read: error-flag gate, count-mismatch sets error\n");
	{
		g_openReturn = (void *)0x3000;
		CFileStream f("x", 3, 0);
		unsigned char buf[8] = {0};

		g_writeReturn = 8; g_writeCalls = 0;
		f.Write(buf, 8);
		check_eq("full write -> no error, dispatched once", (unsigned long)f.ChangeSize(0), 0);
		check_eq("...write called once", g_writeCalls, 1);

		g_writeReturn = 3; /* short write */
		f.Write(buf, 8);
		check_eq("short write -> errorFlag set to 1", f.ChangeSize(0), 1);

		/* once errored, Write is a no-op (gated on mErrorFlag==0) */
		g_writeCalls = 0;
		f.Write(buf, 8);
		check_eq("Write no-ops once errored", g_writeCalls, 0);
	}
	{
		g_openReturn = (void *)0x3100;
		CFileStream f("x", 1, 0);
		unsigned char buf[8];

		g_readReturn = 8; g_readCalls = 0;
		f.Read(buf, 8);
		check_eq("full read -> no error", (unsigned long)f.ChangeSize(0), 0);
		check_eq("...read called once", g_readCalls, 1);

		g_readReturn = 2; /* short read */
		f.Read(buf, 8);
		check_eq("short read -> errorFlag set to 1", f.ChangeSize(0), 1);
	}

	printf("[4] GetSize: only writes outSize when no prior error\n");
	{
		g_openReturn = (void *)0x4000;
		CFileStream f("x", 1, 0);
		g_fileSizeReturn = 0x9999;
		unsigned int size = 0;
		check_eq("GetSize returns 0 (no prior error)", (unsigned long)f.GetSize(size), 0);
		check_eq("...outSize written", size, 0x9999);

		/* force an error via a short write -- Write()'s own errorFlag
		 * is a plain (len != written) boolean, 0/1, NOT the `7`
		 * sentinel ctor/seek failures use (confirmed real asymmetry,
		 * transcribed verbatim in file_stream.cpp's own Write()) --
		 * then confirm GetSize returns that code WITHOUT touching
		 * outSize. */
		g_writeReturn = 1;
		unsigned char b[4];
		f.Write(b, 4);
		size = 0x1234;
		check_eq("GetSize returns errorFlag once errored", (unsigned long)f.GetSize(size), 1);
		check_eq("...outSize left untouched on error", size, 0x1234);
	}

	printf("[5] SetPositionEnd/Relative/Position: whence mapping + seek-failure -> error 7\n");
	{
		g_openReturn = (void *)0x5000;
		CFileStream f("x", 1, 0);

		g_seekReturn = 0; g_seekCalls = 0;
		check_eq("SetPositionEnd ok", (unsigned long)f.SetPositionEnd(), 0);
		check_eq("...whence == SEEK_END(2)", (unsigned long)g_seekWhence, 2);
		check_eq("...offset == 0", (unsigned long)g_seekOffset, 0);

		check_eq("SetPositionRelative(5) ok", (unsigned long)f.SetPositionRelative(5), 0);
		check_eq("...whence == SEEK_CUR(1)", (unsigned long)g_seekWhence, 1);
		check_eq("...offset == 5", (unsigned long)g_seekOffset, 5);

		check_eq("SetPosition(9) ok", (unsigned long)f.SetPosition(9), 0);
		check_eq("...whence == SEEK_SET(0)", (unsigned long)g_seekWhence, 0);
		check_eq("...offset == 9", (unsigned long)g_seekOffset, 9);
	}
	{
		g_openReturn = (void *)0x5100;
		CFileStream f("x", 1, 0);
		g_seekReturn = -1; /* seek failure */
		check_eq("failed seek -> errorFlag 7", (unsigned long)f.SetPosition(1), 7);
		check_eq("...subsequent SetPositionEnd short-circuits to same error",
			 (unsigned long)f.SetPositionEnd(), 7);
	}

	printf("[6] GetPosition/IsAtEnd/Flush: gated on errorFlag, forward mocked results\n");
	{
		g_openReturn = (void *)0x6000;
		CFileStream f("x", 1, 0);

		g_getPosReturn = 0x42; g_getPosCalls = 0;
		unsigned int pos = 0;
		check_eq("GetPosition ok", (unsigned long)f.GetPosition(pos), 0);
		check_eq("...outPos forwarded", pos, 0x42);
		check_eq("...dispatched once", g_getPosCalls, 1);

		g_getPosReturn = -1;
		check_eq("negative GetPosition result -> errorFlag 7",
			 (unsigned long)f.GetPosition(pos), 7);
	}
	{
		g_openReturn = (void *)0x6100;
		CFileStream f("x", 1, 0);
		g_isAtEndReturn = true; g_isAtEndCalls = 0;
		check_eq("IsAtEnd forwards mock true", (unsigned long)f.IsAtEnd(), 1);
		g_isAtEndReturn = false;
		check_eq("IsAtEnd forwards mock false", (unsigned long)f.IsAtEnd(), 0);
		check_eq("...dispatched twice total", g_isAtEndCalls, 2);
	}
	{
		g_openReturn = (void *)0x6200;
		CFileStream f("x", 1, 0);
		g_flushCalls = 0;
		check_eq("Flush ok, dispatches once", (unsigned long)f.Flush(), 0);
		check_eq("...dispatched once", g_flushCalls, 1);
	}

	printf("[7] Exists/Copy: static wrappers, no `this`\n");
	{
		g_existsReturn = 1;
		check_eq("Exists() true forwards mock", (unsigned long)CFileStream::Exists("a"), 1);
		g_existsReturn = 0;
		check_eq("Exists() false forwards mock", (unsigned long)CFileStream::Exists("a"), 0);
	}
	{
		/* Copy(dst, src): confirmed real param order -- param_1 is
		 * opened WRITE/CREATE/TRUNC (dst), param_2 READ-ONLY (src). */
		g_openReturn = (void *)0x7000; /* both opens "succeed" via this single mock */
		g_fileSizeReturn = 0; /* zero-byte source -> success without any read/write */
		g_readCalls = 0; g_writeCalls = 0; g_closeCalls = 0;
		check_eq("Copy of an empty file succeeds (err=0)",
			 (unsigned long)CFileStream::Copy("dst", "src"), 0);
		check_eq("...no read/write for a zero-byte source", g_readCalls + g_writeCalls, 0);
		check_eq("...both handles closed", g_closeCalls, 2);
	}
	{
		g_openReturn = 0; /* source open fails */
		g_closeCalls = 0;
		check_eq("Copy fails cleanly when source can't open",
			 (unsigned long)CFileStream::Copy("dst", "src"), 7);
		check_eq("...nothing to close", g_closeCalls, 0);
	}

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
