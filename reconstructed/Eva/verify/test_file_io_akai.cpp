/*
 * test_file_io_akai.cpp  -  host-side known-answer test for CFileIoAkai
 * (src/init/file_io_akai.cpp, 2026-07-28 storage-cluster batch: Akai/Dos/Iso9660
 * concrete CFileIoBase overrides).
 *
 * Every out-of-scope dependency (CDDriverIO, the embedded aki_ / akiutil_ Akai
 * library, CMediaInfo, CFilesys::get_fileioptr) is an inert stand-in with a fixed
 * return value (see file_io_akai.cpp's own per-function comments) -- this test
 * confirms the real, self-contained parts: sentinel/EFileIOType constants, the
 * "0 == failure" vs "negative == failure" vs "-1 == failure" per-call-site
 * conventions each override forwards through to set_error(), and set_error()'s
 * own 44-entry raw-error-code translation table (both the direct-mapping and the
 * "silently ignored" cases -- the Api-assert-log path is exercised implicitly by
 * every set_error()-calling override below, same as test_file_io_base.cpp's own
 * "heavy" family, relying on mains.o's real Api initialization via $(OBJ)).
 */

#include <cstdio>

#include "file_io_akai.h"
#include "file_io_driver_common.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CFileIoAkai known-answer test\n");
	printf("==============================\n");

	CFileIoAkai io;

	check("get_iotype() == 5", io.get_iotype() == 5);
	check("freebytes() == 0 (full 64-bit)", io.freebytes(kDeviceId_Placeholder) == 0ULL);
	check("getmediainfo() == 0", io.getmediainfo(kDeviceId_Placeholder, 0) == 0);
	check("format() == -1 (tail-call through stub driver's inherited format(3-arg))",
	      io.format(kDeviceId_Placeholder, 0) == -1);
	check("ftell() == 0", io.ftell(1) == 0);

	/* "0 == failure" convention (akiutil_pwd/set_cwd/dskfree stand-ins default
	 * to 0/1 as documented in file_io_akai.cpp -- getwd()'s stand-in defaults
	 * to success, the other two default to failure).
	 */
	char buf[16];
	check("getwd() returns buf on success", io.getwd(kDeviceId_Placeholder, buf) == (int)(long)buf);
	check("chdir() == -1 (akiutil_set_cwd stand-in defaults to failure)", io.chdir("VOL") == -1);
	check("funmount() == -1 (alowl_dskfree stand-in defaults to failure)",
	      io.funmount(kDeviceId_Placeholder) == -1);

	/* "negative == failure" (aki_lseek/aki_open) and "== -1 failure" (aki_read)
	 * conventions.
	 */
	check("fseek() == 0 (aki_lseek stand-in returns 0, not negative)",
	      io.fseek(1, 0, 0) == 0);
	check("fread() == 0 elements (aki_read stand-in returns -1)",
	      io.fread(buf, 1, sizeof(buf), 1) == 0);
	check("fopen() == -1 (aki_open stand-in returns -1)", io.fopen("A.WAV", "r") == -1);

	/* "nonzero == failure" (aki_close). */
	check("fclose() == 0 (aki_close stand-in returns 0)", io.fclose(1) == 0);

	/* fmount() -- capacity stand-in returns 0, which fails the ">0" sector
	 * probe before akiutil_dskinit is ever reached.
	 */
	check("fmount() == -1 (read_capacity stand-in reports 0 capacity)",
	      io.fmount(kDeviceId_Placeholder) == -1);

	/* dir() -- aki_gfirst stand-in reports no entries. */
	unsigned long cont = 0;
	check("dir() == 0 (aki_gfirst stand-in reports no entries)",
	      io.dir("*.*", 0, cont, 0) == 0);
	check("dir() left cont == 0", cont == 0);

	/* set_error()'s own 44-entry raw-code translation table -- exercised
	 * directly via the shared fs_user/theFilesys globals (file_io_driver_common.h).
	 * Run last: several mappings deliberately leave theFilesys->lastError
	 * non-zero, which would short-circuit any subsequent set_error()-calling
	 * override above.
	 */
	g_theFilesys->lastError = 0;
	g_rawFsErrorStorage = 0; /* raw 0 -> kNoop, no-op */
	io.set_error();
	check("set_error() raw=0 is a no-op", g_theFilesys->lastError == 0);

	g_rawFsErrorStorage = 1; /* raw 1 -> mapped 1, direct */
	io.set_error();
	check("set_error() raw=1 -> lastError=1", g_theFilesys->lastError == 1);

	g_theFilesys->lastError = 0;
	g_rawFsErrorStorage = 0x2b; /* raw 0x2b (last in-range slot) -> mapped 0xb */
	io.set_error();
	check("set_error() raw=0x2b -> lastError=0xb", g_theFilesys->lastError == 0xb);

	g_theFilesys->lastError = 5; /* already pending -> short-circuit, no overwrite */
	g_rawFsErrorStorage = 1;
	io.set_error();
	check("set_error() is a no-op once lastError is already pending",
	      g_theFilesys->lastError == 5);
	g_theFilesys->lastError = 0;

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
