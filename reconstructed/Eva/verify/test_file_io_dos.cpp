/*
 * test_file_io_dos.cpp  -  host-side known-answer test for CFileIoDos
 * (src/init/file_io_dos.cpp, 2026-07-28 storage-cluster batch: Akai/Dos/Iso9660
 * concrete CFileIoBase overrides). format(EDevice_Id, int, EFatType) is
 * deferred (DECOMPILE_ERRORS.md) and has no vtable slot on this class -- not
 * exercised here.
 *
 * Every out-of-scope dependency (CDDriverIO, the embedded pc_ / po_ DOS/FAT
 * library) is an inert stand-in with a fixed return value (see
 * file_io_dos.cpp's own per-function comments) -- this test confirms the real,
 * self-contained parts: sentinel/EFileIOType constants, the per-call-site
 * success/failure conventions (several genuinely differ from CFileIoAkai's own
 * equivalents -- see file_io_dos.cpp), and set_error()'s own 44-entry table
 * (confirmed to differ from CFileIoAkai's at raw code 24, and to never log an
 * Api assert, unlike Akai's).
 */

#include <cstdio>
#include <cstring>

#include "file_io_dos.h"
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
	printf("CFileIoDos known-answer test\n");
	printf("=============================\n");

	CFileIoDos io;

	check("get_iotype() == 6", io.get_iotype() == 6);
	check("getmaxclusterno() == 0", io.getmaxclusterno(kDeviceId_Placeholder) == 0);
	check("totalfreeclus() == 0", io.totalfreeclus(kDeviceId_Placeholder) == 0);
	check("freebytes() == 0 (full 64-bit)", io.freebytes(kDeviceId_Placeholder) == 0ULL);
	check("ftell() == 0", io.ftell(1) == 0);
	check("getmediainfo() == 0", io.getmediainfo(kDeviceId_Placeholder, 0) == 0);

	unsigned char lbaBlob[0x18];
	std::memset(lbaBlob, 0, sizeof(lbaBlob));
	check("getfilelbaarray() == -1 (po_makelbaarray stand-in reports failure)",
	      io.getfilelbaarray(kDeviceId_Placeholder, 0, reinterpret_cast<CFileLbaArray *>(lbaBlob)) == -1);

	/* "0 == failure, nonzero == success" convention (pc_rmdir/mkdir/unlink/
	 * mv/pwd/set_cwd/dskfree/resize/flush/close stand-ins default to
	 * success -- resize()/fflush() default to FAILURE, since their own
	 * stand-ins return 0).
	 */
	check("rmdir() == 0", io.rmdir("SUBDIR") == 0);
	check("mkdir() == 0", io.mkdir("SUBDIR") == 0);
	check("remove() == 0", io.remove("FILE.TXT") == 0);
	check("rename() == 0", io.rename("A.TXT", "B.TXT") == 0);
	char buf[16];
	check("getwd() returns buf on success", io.getwd(kDeviceId_Placeholder, buf) == (int)(long)buf);
	check("chdir() == 0", io.chdir("SUBDIR") == 0);
	check("resize() == -1 (po_resize stand-in defaults to failure)", io.resize(1, 100) == -1);
	check("fflush() == -1 (po_flush stand-in defaults to failure)", io.fflush(1) == -1);
	check("funmount() == 0 (pc_dskfree stand-in defaults to success)",
	      io.funmount(kDeviceId_Placeholder) == 0);

	/* "negative == failure" (po_lseek) and "== -1 failure" (po_read/write/
	 * dummywrite/open) conventions.
	 */
	check("fseek() == 0 (po_lseek stand-in returns 0, not negative)", io.fseek(1, 0, 0) == 0);
	check("fread() == 0 elements (po_read stand-in returns -1)",
	      io.fread(buf, 1, sizeof(buf), 1) == 0);
	check("fopen() == -1 (po_open stand-in returns -1)", io.fopen("A.TXT", "r") == -1);

	/* "nonzero == failure" (po_close). */
	check("fclose() == 0 (po_close stand-in returns 0)", io.fclose(1) == 0);

	/* fmount() -- capacity stand-in reports 0, failing before po_dskinit is
	 * ever reached.
	 */
	check("fmount() == -1 (read_capacity stand-in reports 0 capacity)",
	      io.fmount(kDeviceId_Placeholder) == -1);

	/* dir() -- pc_gfirst stand-in reports no entries. */
	unsigned long cont = 0;
	check("dir() == 0 (pc_gfirst stand-in reports no entries)",
	      io.dir("*.*", 1, cont, 0) == 0);
	check("dir() left cont == 0", cont == 0);

	/* optimizemedium() -- pc_optimizemedium stand-in reports failure, raw
	 * fs_user code 0 (reset by the method itself) isn't one of the two
	 * special-cased codes.
	 */
	g_theFilesys->lastError = 0;
	check("optimizemedium() == -1 (pc_optimizemedium stand-in reports failure)",
	      io.optimizemedium(kDeviceId_Placeholder, 10, 0, 1) == -1);

	/* scandisk() -- pc_scandisk stand-in reports failure -> set_error(). */
	g_theFilesys->lastError = 0;
	g_rawFsErrorStorage = 0;
	unsigned long c = 0, d = 0;
	check("scandisk() == 0", io.scandisk(kDeviceId_Placeholder, 0, 0, &c, &d) == 0);

	/* fdummywrite()'s own method-specific fallback: set_error() no-ops on
	 * raw code 0, then fdummywrite() itself force-sets lastError=2.
	 */
	g_theFilesys->lastError = 0;
	g_rawFsErrorStorage = 0;
	check("fdummywrite() == 0 elements (po_dummy_write stand-in returns -1)",
	      io.fdummywrite(1, sizeof(buf), 1) == 0);
	check("fdummywrite() force-sets lastError=2 when set_error() left it 0",
	      g_theFilesys->lastError == 2);

	/* fwrite()'s own short-circuit: skips set_error() entirely when
	 * lastError is already 2 (distinct from set_error()'s own generic
	 * "!=0" pending check).
	 */
	g_theFilesys->lastError = 2;
	g_rawFsErrorStorage = 1; /* would map to 1 if set_error() ran */
	check("fwrite() == 0 elements (po_write stand-in returns -1)",
	      io.fwrite(buf, 1, sizeof(buf), 1) == 0);
	check("fwrite() skipped set_error() (lastError still 2, not overwritten to 1)",
	      g_theFilesys->lastError == 2);

	/* set_error()'s own 44-entry raw-code translation table -- no Api-assert
	 * log path exists for Dos (confirmed via disassembly), unlike Akai's.
	 * Run last, same reasoning as test_file_io_akai.cpp.
	 */
	g_theFilesys->lastError = 0;
	g_rawFsErrorStorage = 0; /* raw 0 -> kNoop, no-op */
	io.set_error();
	check("set_error() raw=0 is a no-op", g_theFilesys->lastError == 0);

	g_rawFsErrorStorage = 24; /* Dos-specific: raw 24 -> field 3 (Akai leaves this raw code unmapped/log) */
	io.set_error();
	check("set_error() raw=24 -> lastError=3 (Dos-specific mapping)",
	      g_theFilesys->lastError == 3);

	g_theFilesys->lastError = 0;
	g_rawFsErrorStorage = 0x2c; /* out of range (> 0x2b) -> silently ignored, no log */
	io.set_error();
	check("set_error() raw=0x2c (out of range) is a silent no-op",
	      g_theFilesys->lastError == 0);

	g_theFilesys->lastError = 7; /* already pending -> short-circuit, no overwrite */
	g_rawFsErrorStorage = 1;
	io.set_error();
	check("set_error() is a no-op once lastError is already pending",
	      g_theFilesys->lastError == 7);
	g_theFilesys->lastError = 0;

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
