/*
 * test_file_io_cdda.cpp  -  host-side known-answer test for CFileIoCdda
 * (src/init/file_io_cdda.cpp, 2026-07-28 storage-cluster follow-up --
 * fresh re-survey of the 2 previously "less tractable" siblings).
 *
 * Confirms the trivial sentinel overrides (including the genuinely
 * DIFFERENT chdir()==0/dir()==0 sentinels, not CFileIoBase's own -1), the
 * cdda_* forwarding methods' set_error()-on-failure behavior against the
 * (all-failing) stand-in library, and format()'s tail-call-through-vtable
 * shape landing on CFileIoBase's own documented -1 sentinel (same
 * convention as test_file_io_unknown.cpp's own format() checks).
 */

#include <cstdio>

#include "file_io_cdda.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CFileIoCdda known-answer test\n");
	printf("==============================\n");

	CFileIoCdda io;

	check("get_iotype() == 2", io.get_iotype() == 2);
	check("fflush() == -1", io.fflush(0) == -1);
	check("resize() == -1", io.resize(0, 0) == -1);
	check("chdir() == 0 (genuine non-CFileIoBase sentinel)", io.chdir("x") == 0);
	unsigned long cont = 0;
	check("dir() == 0 (always reports no entry)", io.dir("x", 0, cont, 0) == 0);
	check("rename() == -1", io.rename("a", "b") == -1);
	check("remove() == -1", io.remove("x") == -1);
	check("mkdir() == -1", io.mkdir("x") == -1);
	check("rmdir() == -1", io.rmdir("x") == -1);

	check("totalfreeclus() == 0 (freebytes/2352, stub freebytes==0)",
	      io.totalfreeclus(kDeviceId_Placeholder) == 0);
	check("freebytes() == 0", io.freebytes(kDeviceId_Placeholder) == 0);
	check("getmediainfo() == 0", io.getmediainfo(kDeviceId_Placeholder, 0) == 0);
	char wdBuf[256];
	check("getwd() unconditional success (no failure path)",
	      io.getwd(kDeviceId_Placeholder, wdBuf) == reinterpret_cast<int>(wdBuf));
	check("format() == -1 (forwarded through stub driver)",
	      io.format(kDeviceId_Placeholder, 0) == -1);
	check("funmount() == 0 (always succeeds)", io.funmount(kDeviceId_Placeholder) == 0);

	check("getemphasized() == -1 (stub cdda_isemphasized fails)",
	      io.getemphasized(0, 0) == -1);
	unsigned long idxlen;
	check("getidxlen() == -1 (stub fails)",
	      io.getidxlen(kDeviceId_Placeholder, 0, 0, 0, &idxlen) == -1);
	check("gettrklen() == -1 (stub fails)",
	      io.gettrklen(kDeviceId_Placeholder, 0, &idxlen) == -1);
	unsigned char idxOut;
	check("getmaxidx() == -1 (stub fails)",
	      io.getmaxidx(kDeviceId_Placeholder, 0, &idxOut) == -1);
	check("getmaxtrk() == -1 (stub fails)",
	      io.getmaxtrk(kDeviceId_Placeholder, &idxOut) == -1);
	check("writesetup() == -1 (stub fails)",
	      io.writesetup(kDeviceId_Placeholder, 0) == -1);
	check("stopscan() == -1 (stub fails)", io.stopscan(kDeviceId_Placeholder) == -1);
	check("rewscan() == -1 (stub fails)", io.rewscan(kDeviceId_Placeholder, 0, 0, 0, 0) == -1);
	check("ffscan() == -1 (stub fails)", io.ffscan(kDeviceId_Placeholder, 0, 0, 0, 0) == -1);
	check("resume() == -1 (stub fails)", io.resume(kDeviceId_Placeholder) == -1);
	check("pause() == -1 (stub fails)", io.pause(kDeviceId_Placeholder) == -1);
	check("stop() == -1 (stub fails)", io.stop(kDeviceId_Placeholder) == -1);
	check("play() == -1 (stub fails)", io.play(kDeviceId_Placeholder, 0, 0, 0, 0) == -1);

	check("fseek() == 0 (stub cdda_lseek returns 0, not negative)", io.fseek(0, 0, 0) == 0);
	check("fwrite() == 0 (stub cdda_write fails)", io.fwrite("x", 1, 1, 0) == 0);
	char buf[4];
	check("fread() == 0 (stub cdda_read fails)", io.fread(buf, 1, 1, 0) == 0);
	check("fclose() == 0 (stub cdda_close reports success)", io.fclose(0) == 0);

	int rc = io.fopen("x", "z");
	check("fopen() < 0 (stub cdda_open fails after passing writable gate)", rc < 0);

	check("fmount() == -1 (stub getdevinfo never reports type 7)",
	      io.fmount(kDeviceId_Placeholder, kMountIoType_Placeholder, 0) == -1);
	check("settestmode() == -1 (stub cdda_writesetup fails)",
	      io.settestmode(kDeviceId_Placeholder, 0) == -1);
	check("finalize() == -1 (stub cdda_writesetup fails, set_error path)",
	      io.finalize(kDeviceId_Placeholder) == -1);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
