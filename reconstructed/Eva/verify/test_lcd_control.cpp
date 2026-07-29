/*
 * test_lcd_control.cpp  -  host-side known-answer test for the 7 confirmed
 * genuinely-empty USTGAPILCDControl::SetXxx() methods (round 59, solo).
 * LoadStoredSettings() itself is pre-existing code from an earlier round and
 * is not covered here.
 */

#include <cstdio>
#include "lcd_control.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("USTGAPILCDControl confirmed-empty SetXxx() methods\n");
	printf("===================================================\n");

	check("SetBlackLevel() -> false", USTGAPILCDControl::SetBlackLevel(3, 200) == false);
	check("SetAllRGBLevels() -> false", USTGAPILCDControl::SetAllRGBLevels() == false);
	check("SetRGBLevel() -> false", USTGAPILCDControl::SetRGBLevel(1, 2, -5) == false);
	check("SetContrast() -> false", USTGAPILCDControl::SetContrast(0.75f) == false);
	check("SetColorTemp() -> false", USTGAPILCDControl::SetColorTemp(6500) == false);
	check("SetSpreadSpectrumClock() -> false", USTGAPILCDControl::SetSpreadSpectrumClock(1, 0) == false);
	check("SetPadDrive2() -> false", USTGAPILCDControl::SetPadDrive2(4, 5) == false);

	printf("===================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
