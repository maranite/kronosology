/*
 * test_lcd_control.cpp  -  host-side known-answer test for the 7 confirmed
 * genuinely-empty USTGAPILCDControl::SetXxx() methods (round 59, solo) plus
 * LoadStoredSettings()'s round-60 bug fix and the 3 new real methods
 * SetBacklightBrightness/ResetToInit/SaveCurrentSettings (round 60, solo).
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "lcd_control.h"
#include "ustg_user_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Friend accessor -- same shape as test_ustg_api_wrappers.cpp's own (a fresh
 * definition here since each verify/ binary is compiled independently).
 */
struct UstgUserApiTestHooks {
	static void SetUser2Rt(int fd) { USTGUserAPI::m_activeUser2rtFD = fd; }
};

extern int g_lcdTestSaveCalibrationCalls;

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

	printf("\nLoadStoredSettings()/SetBacklightBrightness()/ResetToInit()/SaveCurrentSettings() (round 60)\n");
	printf("===================================================\n");
	{
		int user2rt[2];
		pipe(user2rt);
		UstgUserApiTestHooks::SetUser2Rt(user2rt[1]);

		unsigned char frontPanel[0x100];
		memset(frontPanel, 0, sizeof(frontPanel));
		*(unsigned int *)(frontPanel + 0xc8) = 0x11111111;
		*(unsigned int *)(frontPanel + 0xc4) = 0x22222222; /* sCurrentSettings source */
		*(unsigned int *)(frontPanel + 0xcc) = 0x33333333;
		*(unsigned int *)(frontPanel + 0xd0) = 0x44444444;
		*(unsigned int *)(frontPanel + 0xd4) = 0x55555555;
		*(unsigned int *)(frontPanel + 0xd8) = 0x66666666;
		*(unsigned int *)(frontPanel + 0xdc) = 0x77777777;
		*(unsigned int *)(frontPanel + 0xe0) = 0xDEADBEEF; /* must NOT be read by LoadStoredSettings */
		*(unsigned int *)(frontPanel + 0xe4) = 0x88888888;
		USTGUserAPI::mFrontPanelStatusAddress = frontPanel;

		bool loaded = USTGAPILCDControl::LoadStoredSettings();
		check("LoadStoredSettings() -> true", loaded == true);
		char buf[64];
		read(user2rt[0], buf, sizeof(buf)); /* drain the STGMessage LoadStoredSettings sent */

		USTGAPILCDControl::SetBacklightBrightness(0x77);
		read(user2rt[0], buf, sizeof(buf));
		unsigned char *cur = (unsigned char *)USTGUserAPI::mFrontPanelStatusAddress; /* dummy use to silence warnings */
		(void)cur;

		USTGAPILCDControl::SaveCurrentSettings();
		check("SaveCurrentSettings(): frontPanel+0xc4 byte 3 == 0x77 (from SetBacklightBrightness)",
		      frontPanel[0xc4 + 3] == 0x77);
		check("SaveCurrentSettings(): frontPanel+0xc8 round-trips == 0x11111111",
		      *(unsigned int *)(frontPanel + 0xc8) == 0x11111111u);
		check("SaveCurrentSettings(): frontPanel+0xcc round-trips == 0x33333333",
		      *(unsigned int *)(frontPanel + 0xcc) == 0x33333333u);
		check("SaveCurrentSettings(): frontPanel+0xd0 round-trips == 0x44444444",
		      *(unsigned int *)(frontPanel + 0xd0) == 0x44444444u);
		check("SaveCurrentSettings(): frontPanel+0xd4 round-trips == 0x55555555",
		      *(unsigned int *)(frontPanel + 0xd4) == 0x55555555u);
		check("SaveCurrentSettings(): frontPanel+0xd8 round-trips == 0x66666666",
		      *(unsigned int *)(frontPanel + 0xd8) == 0x66666666u);
		check("SaveCurrentSettings(): frontPanel+0xdc round-trips == 0x77777777",
		      *(unsigned int *)(frontPanel + 0xdc) == 0x77777777u);
		check("SaveCurrentSettings(): frontPanel+0xe0 == 0 (sUnknownFc, never set by LoadStoredSettings -- real quirk)",
		      *(unsigned int *)(frontPanel + 0xe0) == 0u);
		check("SaveCurrentSettings(): frontPanel+0xe4 round-trips == 0x88888888",
		      *(unsigned int *)(frontPanel + 0xe4) == 0x88888888u);
		check("SaveCurrentSettings(): tail-calls USTGAPICalibration::SaveCurrentCalibrationToDisk()",
		      g_lcdTestSaveCalibrationCalls == 1);

		bool reset = USTGAPILCDControl::ResetToInit();
		read(user2rt[0], buf, sizeof(buf));
		check("ResetToInit() -> true", reset == true);
		USTGAPILCDControl::SaveCurrentSettings();
		check("ResetToInit(): frontPanel+0xc4 byte 3 == 0x3f", frontPanel[0xc4 + 3] == 0x3f);
		check("ResetToInit(): frontPanel+0xe0 == 0x3f800000 (1.0f)",
		      *(unsigned int *)(frontPanel + 0xe0) == 0x3f800000u);
		check("SaveCurrentCalibrationToDisk() called again (now 2 total)",
		      g_lcdTestSaveCalibrationCalls == 2);
	}

	printf("===================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
