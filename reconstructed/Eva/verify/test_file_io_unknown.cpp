/*
 * test_file_io_unknown.cpp  -  host-side known-answer test for CFileIoUnknown
 * (src/init/file_io_unknown.cpp, 2026-07-27 storage-cluster follow-up).
 *
 * Confirms the 3 trivial overrides' real sentinels (get_iotype()==1,
 * fmount()==-1, funmount()==0 -- notably NOT CFileIoBase's -1+assert default),
 * that getmediainfo() always returns 0 regardless of its stand-in dependencies'
 * (zero) output, and that both format() overloads really do tail-call through to
 * a dispatchable driver object rather than crashing or short-circuiting -- since
 * the stand-in CFilesys::get_fileioptr() replacement returns a plain CFileIoBase
 * instance, the observable result is CFileIoBase's own documented format()
 * sentinel (-1), which is exactly the right generic "unhandled" behavior for an
 * unresolved driver lookup.
 */

#include <cstdio>

#include "file_io_unknown.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CFileIoUnknown known-answer test\n");
	printf("=================================\n");

	CFileIoUnknown io;

	check("get_iotype() == 1", io.get_iotype() == 1);
	check("fmount() == -1", io.fmount(kDeviceId_Placeholder) == -1);
	check("funmount() == 0 (success, not CFileIoBase's -1)", io.funmount(kDeviceId_Placeholder) == 0);
	check("getmediainfo() == 0", io.getmediainfo(kDeviceId_Placeholder, 0) == 0);
	check("format(2-arg) == -1 (forwarded through stub driver)",
	      io.format(kDeviceId_Placeholder, 0) == -1);
	check("format(3-arg) == -1 (forwarded through stub driver)",
	      io.format(kDeviceId_Placeholder, 0, kFatType_Placeholder) == -1);

	/* Base-class methods CFileIoUnknown doesn't override still resolve to
	 * CFileIoBase's own stub bodies (no vtable slot patched for them) --
	 * confirms the derived class's vtable really does inherit the rest.
	 */
	check("inherited fopen() == -1", io.fopen("a", "r") == -1);
	check("inherited chdir() == -1", io.chdir("x") == -1);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
