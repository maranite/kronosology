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
extern "C" void SCalibrationData_InitAll(unsigned char *panel);

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

	printf("[6] SCalibrationData_InitAll() -- spot-check field groups + bounds\n");
	memset(panel, 0xcc, sizeof(panel));
	SCalibrationData_InitAll(panel);
	/* generic curve table (0x00-0x1f), overridden tail at 0x04/0x05 */
	check_eq("curve[0x00]", panel[0x00], 0x03);
	check_eq("curve[0x01]", panel[0x01], 0x74);
	check_eq("curve[0x1e]", panel[0x1e], 0x06);
	check_eq("curve[0x1f]", panel[0x1f], 0x5b);
	check_eq("final override [0x04]", panel[0x04], 0x01);
	check_eq("final override [0x05]", panel[0x05], 0x02);
	/* JoystickX */
	check_eq("JSX xMin [0x20]", *(short *)(panel + 0x20), 0x0075);
	check_eq("JSX xMax [0x26]", *(short *)(panel + 0x26), 0x0387);
	{
		int bits;
		memcpy(&bits, panel + 0x28, 4);
		check_eq("JSX scaleLo [0x28] (bit pattern)", bits, (int)0x3fb48a3a);
	}
	/* JoystickY */
	check_eq("JSY yMin [0x34]", *(short *)(panel + 0x34), 0x00a0);
	/* Ribbon (default/non-drum branch) */
	check_eq("Ribbon xMinRaw [0x48]", *(short *)(panel + 0x48), 0x0050);
	check_eq("Ribbon extra [0x98]", *(unsigned short *)(panel + 0x98), 0x0190);
	/* Vector joystick shares one float between X and Y */
	{
		int bx, by;
		memcpy(&bx, panel + 0x64, 4);
		memcpy(&by, panel + 0x78, 4);
		check_eq("VectorJS scaleLoX==scaleLoY [0x64 vs 0x78]", bx, by);
	}
	/* Half-damper */
	check_eq("Damper xMin [0x84]", *(short *)(panel + 0x84), 0x01fe);
	/* Touch screen */
	check_eq("TouchScreen [0x9c]", *(unsigned short *)(panel + 0x9c), 0x0008);
	check_eq("TouchScreen [0xb6]", *(unsigned short *)(panel + 0xb6), 0x000a);
	/* LCD control: default gain 1.0f + trailing enable flags */
	check_eq("LCD gain [0xc8] (bit pattern)", *(int *)(panel + 0xc8), (int)0x3f800000);
	check_eq("LCD range [0xce]", *(unsigned short *)(panel + 0xce), 0xffff);
	check_eq("LCD enable [0xe6]", panel[0xe6], 0x01);
	check_eq("LCD enable [0xe7]", panel[0xe7], 0x01);
	/* Aftertouch */
	check_eq("Aftertouch xMin [0xe8]", *(short *)(panel + 0xe8), 0x00d4);
	check_eq("Aftertouch xMax [0xee]", *(short *)(panel + 0xee), 0x029c);
	/* Highest write is the dword at 0xf4 (0xf4-0xf7) -- confirm 0xf8
	 * onward (the trailing bytes of the same 0xfc-byte blob, plus the
	 * canary well past it) are untouched. */
	check_eq("canary [0xf8] untouched", panel[0xf8], 0xcc);
	check_eq("canary [0xff] untouched", panel[0xff], 0xcc);

	if (g_fail) {
		printf("FAILED: %d check(s)\n", g_fail);
		return 1;
	}
	printf("All checks passed.\n");
	return 0;
}
