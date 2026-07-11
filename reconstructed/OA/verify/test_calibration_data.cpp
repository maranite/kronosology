// SPDX-License-Identifier: GPL-2.0
/*
 * test_calibration_data.cpp  -  host-side known-answer test for
 * SCalibrationData_LoadCalibrationFile() (src/init/calibration_data.cpp,
 * batch 38).
 *
 * Links src/init/calibration_data.cpp directly. Mocks CSTGFile_Open/
 * Read/Close with a small scripted in-memory "file" so the real
 * checksum logic (byte-sum of the first 0xfc bytes vs. a trailing
 * 4-byte stored checksum) is genuinely exercised end to end, not
 * skipped like test_setup_global_resources.cpp's own deliberately
 * simple `return 0` mock (which stays as-is -- see that file's own
 * comment).
 */

#include <cstdio>
#include <cstring>

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ---- scripted fake file ---- */
static int g_openCalls, g_closeCalls;
static int g_openShouldFail;
static unsigned char g_fileBytes[0x100];
static unsigned int g_fileLen;
static unsigned int g_readPos;
static int g_forceShortRead; /* if set, next Read() returns 1 byte less than requested */

extern "C" void *CSTGFile_Open(const char *path, int mode)
{
	g_openCalls++;
	if (g_openShouldFail)
		return 0;
	if (strcmp(path, "/korg/rw/Calibration/Calibration.img") != 0)
		return 0;
	if (mode != 0 /* O_RDONLY */)
		return 0;
	g_readPos = 0;
	return (void *)0x1; /* any non-NULL handle */
}

extern "C" int CSTGFile_Read(void *handle, void *buf, unsigned int size)
{
	if (handle != (void *)0x1)
		return 0;
	unsigned int avail = (g_readPos < g_fileLen) ? (g_fileLen - g_readPos) : 0;
	unsigned int n = (size < avail) ? size : avail;
	if (g_forceShortRead && n > 0) {
		n--;
		g_forceShortRead = 0;
	}
	memcpy(buf, g_fileBytes + g_readPos, n);
	g_readPos += n;
	return (int)n;
}

extern "C" int CSTGFile_Close(void *handle)
{
	if (handle == (void *)0x1)
		g_closeCalls++;
	return 0;
}

extern "C" char SCalibrationData_LoadCalibrationFile(unsigned char *panel);

static void resetMocks()
{
	g_openCalls = g_closeCalls = 0;
	g_openShouldFail = 0;
	g_forceShortRead = 0;
}

int main()
{
	unsigned char panel[0x200];

	printf("[1] Open failure returns false, no Read/Close calls\n");
	resetMocks();
	g_openShouldFail = 1;
	memset(panel, 0xcc, sizeof(panel));
	char rc = SCalibrationData_LoadCalibrationFile(panel);
	check_eq("Open call count", g_openCalls, 1);
	check_eq("returns false", rc, 0);
	check_eq("Close never called", g_closeCalls, 0);

	printf("[2] short main read (< 0xfc bytes) closes and returns false\n");
	resetMocks();
	memset(g_fileBytes, 0x11, sizeof(g_fileBytes));
	g_fileLen = 0xfc - 1; /* one byte short */
	rc = SCalibrationData_LoadCalibrationFile(panel);
	check_eq("returns false on short main read", rc, 0);
	check_eq("Close called on failure path", g_closeCalls, 1);

	printf("[3] short checksum read (< 4 bytes) closes and returns false\n");
	resetMocks();
	memset(g_fileBytes, 0x22, 0xfc);
	/* Only 3 checksum bytes follow the 0xfc data bytes. */
	g_fileLen = 0xfc + 3;
	rc = SCalibrationData_LoadCalibrationFile(panel);
	check_eq("returns false on short checksum read", rc, 0);
	check_eq("Close called on failure path", g_closeCalls, 1);

	printf("[4] matching checksum -> true, panel[0..0xfb] filled from the file\n");
	resetMocks();
	unsigned int sum = 0;
	for (unsigned int i = 0; i < 0xfc; i++) {
		g_fileBytes[i] = (unsigned char)(i * 7 + 3);
		sum += g_fileBytes[i];
	}
	memcpy(g_fileBytes + 0xfc, &sum, 4);
	g_fileLen = 0xfc + 4;
	memset(panel, 0xcc, sizeof(panel));
	rc = SCalibrationData_LoadCalibrationFile(panel);
	check_eq("returns true on matching checksum", rc, 1);
	check_eq("Close called on success path too", g_closeCalls, 1);
	int panelMatches = 1;
	for (unsigned int i = 0; i < 0xfc; i++)
		if (panel[i] != g_fileBytes[i])
			panelMatches = 0;
	check_eq("panel[0..0xfb] filled from file", panelMatches, 1);

	printf("[5] mismatching checksum -> false, even though both reads succeeded\n");
	resetMocks();
	unsigned int wrongSum = sum + 1;
	memcpy(g_fileBytes + 0xfc, &wrongSum, 4);
	rc = SCalibrationData_LoadCalibrationFile(panel);
	check_eq("returns false on checksum mismatch", rc, 0);
	check_eq("Close still called", g_closeCalls, 1);

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
