/*
 * test_file_io_iso9660.cpp  -  host-side known-answer test for CFileIoIso9660
 * (src/init/file_io_iso9660.cpp, 2026-07-28 storage-cluster batch: Akai/Dos/
 * Iso9660 concrete CFileIoBase overrides).
 *
 * Every out-of-scope dependency (CDDriverIO, the embedded cd_ ISO9660 library,
 * CMediaInfo, CFilesys::get_fileioptr, CFilePath) is an inert stand-in with a
 * fixed return value (see file_io_iso9660.cpp's own per-function comments) --
 * this test confirms the real, self-contained parts: sentinel/EFileIOType
 * constants, the "0 == failure" (cd_gcwd/cd_scwd/cd_dskopen) vs "negative ==
 * failure" (cd_lseek/cd_open) vs "== -1 failure" (cd_read) per-call-site
 * conventions, set_error()'s own bitmask-based (not table-based) raw-code
 * translation, and fmount()'s own genuine "always returns -1" finding.
 */

#include <cstdio>

#include "file_io_iso9660.h"
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
	printf("CFileIoIso9660 known-answer test\n");
	printf("=================================\n");

	CFileIoIso9660 io;

	check("get_iotype() == 4", io.get_iotype() == 4);
	check("freebytes() == 0 (full 64-bit)", io.freebytes(kDeviceId_Placeholder) == 0ULL);
	check("getmediainfo() == 0", io.getmediainfo(kDeviceId_Placeholder, 0) == 0);
	check("format() == -1 (tail-call through stub driver's inherited format(3-arg))",
	      io.format(kDeviceId_Placeholder, 0) == -1);
	check("ftell() == 0", io.ftell(1) == 0);
	check("fclose() == 0", io.fclose(1) == 0);
	check("funmount() == 0", io.funmount(kDeviceId_Placeholder) == 0);

	char buf[16];
	check("fread() == 0 elements (cd_read stand-in returns -1)",
	      io.fread(buf, 1, sizeof(buf), 1) == 0);
	check("getwd() == 0 (cd_gcwd stand-in reports failure)",
	      io.getwd(kDeviceId_Placeholder, buf) == 0);
	check("fseek() == 0 (cd_lseek stand-in returns 0, not negative)",
	      io.fseek(1, 0, 0) == 0);
	check("chdir() == -1 (cd_scwd stand-in reports failure)", io.chdir("\\DIR\\FILE.TXT") == -1);
	check("fopen() == -1 (cd_open stand-in returns -1)", io.fopen("\\DIR\\FILE.TXT", "r") == -1);

	/* fmount() -- genuine "always returns -1" finding, even before the
	 * getdevinfo()+0x1==7 gate is exercised (the stand-in reports a
	 * non-SCSI-7 device by default).
	 */
	check("fmount() == -1 (getdevinfo stand-in reports non-SCSI-7 device)",
	      io.fmount(kDeviceId_Placeholder) == -1);

	/* dir() -- cd_gfirst stand-in reports no entries. */
	unsigned long cont = 0;
	check("dir() == 0 (cd_gfirst stand-in reports no entries)",
	      io.dir("\\*.*", 1, cont, 0) == 0);
	check("dir() left cont == 0", cont == 0);

	/* set_error()'s own bitmask translation (raw codes 0..0xa only; NOT a
	 * 44-entry table like CFileIoAkai/CFileIoDos's own). Run last, same
	 * reasoning as the other two drivers' own tests.
	 */
	g_theFilesys->lastError = 0;
	cd_errno = 3; /* bit 0x8 & 0x428 != 0 -> field 1 */
	io.set_error();
	check("set_error() raw=3 -> lastError=1 (bit 0x428 mask)", g_theFilesys->lastError == 1);

	g_theFilesys->lastError = 0;
	cd_errno = 0; /* bit 0x1 & 0x3c3 != 0 -> field 3 */
	io.set_error();
	check("set_error() raw=0 -> lastError=3 (bit 0x3c3 mask)", g_theFilesys->lastError == 3);

	g_theFilesys->lastError = 0;
	cd_errno = 10; /* bit 0x400 & 0x428 != 0 -> field 1 */
	io.set_error();
	check("set_error() raw=0xa -> lastError=1 (bit 0x428 mask)", g_theFilesys->lastError == 1);

	g_theFilesys->lastError = 5; /* already pending -> short-circuit */
	cd_errno = 3;
	io.set_error();
	check("set_error() is a no-op once lastError is already pending",
	      g_theFilesys->lastError == 5);
	g_theFilesys->lastError = 0;

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
