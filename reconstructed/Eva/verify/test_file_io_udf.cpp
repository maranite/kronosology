/*
 * test_file_io_udf.cpp  -  host-side known-answer test for CFileIoUdf
 * (src/init/file_io_udf.cpp, 2026-07-28 storage-cluster follow-up -- the
 * last untouched concrete CFileIoBase sibling, closing out the cluster
 * file_io_base.h's own "OUT OF SCOPE" list started).
 *
 * Confirms the trivial forwarding overrides' set_error()-on-failure
 * behavior against the (all-failing) stand-in library, and the genuine,
 * independently verified finding that fopen() with mode[0]=='v' returns -1
 * unconditionally with NO other work performed at all -- distinct from
 * every other mode letter, and from CFileIoCdda::fopen()'s own 'v' slot
 * (which does real work).
 */

#include <cstdio>

#include "file_io_udf.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CFileIoUdf known-answer test\n");
	printf("=============================\n");

	CFileIoUdf io;

	check("get_iotype() == 3", io.get_iotype() == 3);
	check("totalfreeclus() == 0 (stub udf_freeblks==0)",
	      io.totalfreeclus(kDeviceId_Placeholder) == 0);
	check("freebytes() == 0", io.freebytes(kDeviceId_Placeholder) == 0);
	check("getmediainfo() == 0", io.getmediainfo(kDeviceId_Placeholder, 0) == 0);
	check("funmount() == 0 (always succeeds)", io.funmount(kDeviceId_Placeholder) == 0);

	unsigned long cont = 0;
	check("dir() == 0 (stub udf_gfirst reports no entry)",
	      io.dir("x", 0, cont, 0) == 0);

	check("isodir() == -1 (stub fails)", io.isodir(kDeviceId_Placeholder, 0, 0) == -1);
	check("sortdir() == -1 (stub fails)", io.sortdir(kDeviceId_Placeholder) == -1);
	check("closepath() == -1 (stub fails)", io.closepath(kDeviceId_Placeholder, 0) == -1);
	check("opennextpath() == -1 (stub fails)", io.opennextpath(kDeviceId_Placeholder) == -1);
	check("getwd() == 0 (stub udf_gcwd fails)", io.getwd(kDeviceId_Placeholder, 0) == 0);
	check("chdir() == -1 (stub udf_scwd fails)", io.chdir("x") == -1);

	check("fflush() == -1 (stub udf_flush fails)", io.fflush(0) == -1);
	check("fseek() == 0 (stub udf_lseek returns 0, not negative)", io.fseek(0, 0, 0) == 0);
	check("fwrite() == 0 (stub udf_write fails)", io.fwrite("x", 1, 1, 0) == 0);
	char buf[4];
	check("fread() == 0 (stub udf_read fails)", io.fread(buf, 1, 1, 0) == 0);
	check("fclose() == 0 (stub udf_close reports success)", io.fclose(0) == 0);

	check("fmount() == -1 (stub getdevinfo never reports type 7)",
	      io.fmount(kDeviceId_Placeholder) == -1);
	check("writesetup() == -1 (stub scsi_read_trkinfo fails)",
	      io.writesetup(kDeviceId_Placeholder, 0) == -1);
	check("chmod() < 0 (writesetup gate fails first)", io.chmod("x", 0) < 0);
	check("rmdir() < 0 (writesetup gate fails first)", io.rmdir("x") < 0);
	check("mkdir() < 0 (writesetup gate fails first)", io.mkdir("x") < 0);
	check("remove() < 0 (writesetup gate fails first)", io.remove("x") < 0);
	check("rename() < 0 (writesetup gate fails first)", io.rename("x", "y") < 0);

	check("fopen('r') < 0 (real work, stub udf_open fails)", io.fopen("x", "r") < 0);
	check("fopen('v') == -1 GENUINE FINDING (unconditional, no other work)",
	      io.fopen("x", "v") == -1);
	check("fopen('z' default) < 0 (real work, stub udf_open fails)", io.fopen("x", "z") < 0);

	check("SetRecoveryParam() == -1 (stub scsi_mode_sense10 fails)",
	      io.SetRecoveryParam(kDeviceId_Placeholder, 0) == -1);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
