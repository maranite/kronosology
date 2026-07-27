/*
 * test_file_io_base.cpp  -  host-side known-answer test for CFileIoBase
 * (src/init/file_io_base.cpp, 2026-07-27 storage-cluster batch).
 *
 * CFileIoBase is a pure stub interface layer -- every method is an unconditional,
 * argument-independent return of a fixed sentinel (see file_io_base.h's own header
 * comment for the full breakdown). This test exercises a representative sample of
 * both the "heavy" (Api+0x94 assert-report call first) and "light" (immediate
 * return, no assert call) method families, confirms the documented sentinel for
 * each, and confirms the one 64-bit-returning method (freebytes()) really does
 * return a full 0 in both halves of EDX:EAX, not just EAX.
 *
 * Real `Api` global (mains.cpp) is linked in via $(OBJ) same as every other verify
 * test -- CFileIoBase's own assert-report call just needs *Api dereferenceable to
 * fetch its (EvaVTableStub-backed) vtable, never fatal.
 */

#include <cstdio>

#include "file_io_base.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CFileIoBase known-answer test\n");
	printf("==============================\n");

	CFileIoBase io;

	/* "light" family -- no assert call, immediate sentinel. */
	check("get_iotype() == 0", io.get_iotype() == 0);
	check("format(3-arg) == -1", io.format(kDeviceId_Placeholder, 0, kFatType_Placeholder) == -1);
	check("freebytes() == 0 (full 64-bit)", io.freebytes(kDeviceId_Placeholder) == 0ULL);
	check("chdir() == -1", io.chdir("x") == -1);
	check("getwd() == 0", io.getwd(kDeviceId_Placeholder, 0) == 0);
	check("getmediainfo() == -1", io.getmediainfo(kDeviceId_Placeholder, 0) == -1);
	check("stop() == -1", io.stop(kDeviceId_Placeholder) == -1);
	check("getcurpos() == 0", io.getcurpos(kDeviceId_Placeholder, 0, 0, 0, 0, 0) == 0);
	check("getmaxtrk() == -1", io.getmaxtrk(kDeviceId_Placeholder, 0) == -1);

	/* "heavy" family -- Api+0x94 assert-report call first, then sentinel. */
	check("fmount(1-arg) == -1", io.fmount(kDeviceId_Placeholder) == -1);
	check("fmount(3-arg) == -1", io.fmount(kDeviceId_Placeholder, kMountIoType_Placeholder, 0) == -1);
	check("funmount() == -1", io.funmount(kDeviceId_Placeholder) == -1);
	check("fopen() == -1", io.fopen("a", "r") == -1);
	check("fread() == 0", io.fread(0, 1, 1, 0) == 0u);
	check("fwrite() == 0", io.fwrite(0, 1, 1, 0) == 0u);
	check("ftell() == -1", io.ftell(0) == -1);
	check("totalfreeclus() == 0", io.totalfreeclus(kDeviceId_Placeholder) == 0ul);
	check("rename() == -1", io.rename("a", "b") == -1);
	check("finalize() == -1", io.finalize(kDeviceId_Placeholder) == -1);
	check("fdummywrite() == 0", io.fdummywrite(1, 1, 0) == 0u);
	check("getmaxclusterno() == 0", io.getmaxclusterno(kDeviceId_Placeholder) == 0ul);
	check("chmod() == -1", io.chmod("a", 0) == -1);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
