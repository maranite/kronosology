/*
 * lcd_control.h  -  USTGAPILCDControl::LoadStoredSettings (Stage 1 boot path),
 * plus 7 further confirmed genuinely-empty methods (round 59, solo, 2026-07-29).
 *
 * Pulls LCD/front-panel settings out of the shared memory USTGUserAPI::Connect()
 * mapped and forwards one STGMessage back to OA.ko. The 9 individual field offsets
 * (+0xc8..+0xe4 into mFrontPanelStatusAddress) are faithfully reproduced but not
 * semantically decoded (contrast/backlight/calibration -- not determined) -- not
 * needed for the boot-path milestone.
 *
 * The 7 SetXxx() methods below are ALL confirmed genuinely empty in the real
 * binary (each a literal 3-byte `mov eax,0; ret`, ignoring every one of their
 * own declared parameters) -- this Kronos build's LCD-control command path is
 * a real, but entirely stubbed-out, no-op surface. Same "harmless unused-
 * param mismatch on a confirmed-empty body" precedent as OA.ko's
 * CSTGLFOBase::AdvanceFadeEnv.
 */

#ifndef LCD_CONTROL_H
#define LCD_CONTROL_H

/* Opaque scalar stand-ins for 2 real enum types (symbols.csv gives only the
 * mangled type name, not the enumerator list) -- same "declared opaque,
 * byte-for-byte faithful" precedent as ustg_api_wrappers.h's own
 * eSTGMsgPerfType.
 */
typedef unsigned int eLCDColorSelect;
typedef unsigned int eLCDColorTemp;

class USTGAPILCDControl {
public:
	static bool LoadStoredSettings();

	/* .text+0x08e1dc40, 3 bytes. */
	static bool SetBlackLevel(eLCDColorSelect color, unsigned char level);
	/* .text+0x08e1dc50, 3 bytes. */
	static bool SetAllRGBLevels();
	/* .text+0x08e1dc60, 3 bytes. */
	static bool SetRGBLevel(eLCDColorSelect a, eLCDColorSelect b, short level);
	/* .text+0x08e1dcc0, 3 bytes. */
	static bool SetContrast(float contrast);
	/* .text+0x08e1dcd0, 3 bytes. */
	static bool SetColorTemp(eLCDColorTemp temp);
	/* .text+0x08e1dce0, 3 bytes. */
	static bool SetSpreadSpectrumClock(unsigned char a, unsigned char b);
	/* .text+0x08e1dcf0, 3 bytes. */
	static bool SetPadDrive2(unsigned char a, unsigned char b);
};

#endif /* LCD_CONTROL_H */
