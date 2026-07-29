/*
 * test_filesys.cpp  -  host-side known-answer test for CFilesys's 9
 * real methods landed in round 46 (solo, 2026-07-29). See
 * include/filesys.h for the full derivation.
 */
#include <cstdio>
#include <cstring>
#include "filesys.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

struct FilesysTestHooks {
	static void SetDriverPtrs(CFilesys &f, void *p1, void *p2, void *p3, void *p4, void *p5,
				   void *p6, void *defaultDriver)
	{
		unsigned char *base = reinterpret_cast<unsigned char *>(&f);
		*reinterpret_cast<void **>(base + 0x04) = p1;
		*reinterpret_cast<void **>(base + 0x10) = p2;
		*reinterpret_cast<void **>(base + 0x14) = p3;
		*reinterpret_cast<void **>(base + 0x0c) = p4;
		*reinterpret_cast<void **>(base + 0x08) = p5;
		*reinterpret_cast<void **>(base + 0x18) = p6;
		*reinterpret_cast<void **>(base + 0x1c) = defaultDriver;
	}
	static void SetDeviceDriver(CFilesys &f, int idx, void *p)
	{
		unsigned char *base = reinterpret_cast<unsigned char *>(&f);
		reinterpret_cast<void **>(base + 0x20)[idx] = p;
	}
};

int main()
{
	static unsigned char storage[sizeof(CFilesys)];
	memset(storage, 0, sizeof(storage));
	CFilesys &f = *reinterpret_cast<CFilesys *>(storage);

	/* [1] eventhandling/startup: no-op, reached here */
	CFilesys::eventhandling();
	CFilesys::startup();
	check("eventhandling/startup: no-op, reached here", true);

	/* [2] run(): never called (infinite loop) -- just confirm it's
	 * real, correctly-typed, linkable code by taking its address. */
	void (*runFn)() = &CFilesys::run;
	check("run: address is non-null (real code, deliberately not invoked)", runFn != 0);

	/* [3] new_fptr: ignores 1st param, returns 2nd + 0x4c */
	check("new_fptr(anything, 0x100) == 0x14c", f.new_fptr(0x99, 0x100) == 0x14c);
	check("new_fptr(anything, 0) == 0x4c", f.new_fptr(0, 0) == 0x4c);

	/* [4] get_fileioptr(EFileIOType): 6-case switch + default */
	void *p1 = (void *)0x1001, *p2 = (void *)0x1002, *p3 = (void *)0x1003;
	void *p4 = (void *)0x1004, *p5 = (void *)0x1005, *p6 = (void *)0x1006;
	void *pDefault = (void *)0xdead;
	FilesysTestHooks::SetDriverPtrs(f, p1, p2, p3, p4, p5, p6, pDefault);
	check("get_fileioptr(1) == mFileIoPtr1", f.get_fileioptr(1u) == p1);
	check("get_fileioptr(2) == mFileIoPtr2", f.get_fileioptr(2u) == p2);
	check("get_fileioptr(3) == mFileIoPtr3", f.get_fileioptr(3u) == p3);
	check("get_fileioptr(4) == mFileIoPtr4", f.get_fileioptr(4u) == p4);
	check("get_fileioptr(5) == mFileIoPtr5", f.get_fileioptr(5u) == p5);
	check("get_fileioptr(6) == mFileIoPtr6", f.get_fileioptr(6u) == p6);
	check("get_fileioptr(0) [default] == mDefaultDriver", f.get_fileioptr(0u) == pDefault);
	check("get_fileioptr(99) [default] == mDefaultDriver", f.get_fileioptr(99u) == pDefault);

	/* [5] get_fileioptr(char const*, EDevice_Id*): drive-letter routing */
	void *pDevA = (void *)0x2000, *pDevC = (void *)0x2002;
	FilesysTestHooks::SetDeviceDriver(f, 0, pDevA);
	FilesysTestHooks::SetDeviceDriver(f, 2, pDevC);
	int devId = -1;
	check("get_fileioptr(\"A:\\\\foo\", &id): routes to device 0",
	      f.get_fileioptr("A:\\foo", &devId) == pDevA && devId == 0);
	check("get_fileioptr(\"C:\\\\bar\", &id): routes to device 2",
	      f.get_fileioptr("C:\\bar", &devId) == pDevC && devId == 2);
	check("get_fileioptr(\"nodrive\", &id): no ':' at [1] -> default, id=10",
	      f.get_fileioptr("nodrive", &devId) == pDefault && devId == 10);
	check("get_fileioptr(path, nullptr): outDeviceId may be null, no crash",
	      f.get_fileioptr("A:\\foo", 0) == pDevA);

	/* [6] setbuf/CheckError: shared static scratch buffer */
	int scratch[8];
	memset(scratch, 0xcc, sizeof(scratch));
	CFilesys::setbuf(7, scratch);
	check("setbuf: scratch[2] (pending-error flag) cleared to 0", scratch[2] == 0);

	f.CheckError(0); /* first zero-error call: latches mStickyErrorFlag, buf[2]=1, buf[0]=0 */
	check("CheckError(0), first call: buf[2] latched to 1", scratch[2] == 1);
	check("CheckError(0), first call: buf[0] forced to 0", scratch[0] == 0);

	scratch[0] = 0x77;
	f.CheckError(0); /* mStickyErrorFlag already set: falls through to buf[0]=errCode */
	check("CheckError(0), 2nd call (already latched): buf[0] = errCode (0)", scratch[0] == 0);

	f.CheckError(42);
	check("CheckError(42): buf[0] = 42 (errCode != 0 path)", scratch[0] == 42);

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
