/*
 * lcd_control.h  -  USTGAPILCDControl::LoadStoredSettings (Stage 1 boot path).
 *
 * Pulls LCD/front-panel settings out of the shared memory USTGUserAPI::Connect()
 * mapped and forwards one STGMessage back to OA.ko. The 9 individual field offsets
 * (+0xc8..+0xe4 into mFrontPanelStatusAddress) are faithfully reproduced but not
 * semantically decoded (contrast/backlight/calibration -- not determined) -- not
 * needed for the boot-path milestone.
 */

#ifndef LCD_CONTROL_H
#define LCD_CONTROL_H

class USTGAPILCDControl {
public:
	static bool LoadStoredSettings();
};

#endif /* LCD_CONTROL_H */
