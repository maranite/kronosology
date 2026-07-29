/*
 * lcd_control.h  -  USTGAPILCDControl::LoadStoredSettings (Stage 1 boot path),
 * 7 further confirmed genuinely-empty methods (round 59), plus
 * SetBacklightBrightness/ResetToInit/SaveCurrentSettings (round 60, solo,
 * 2026-07-29).
 *
 * Pulls LCD/front-panel settings out of the shared memory USTGUserAPI::Connect()
 * mapped and forwards one STGMessage back to OA.ko. The individual field
 * offsets (+0xc4..+0xe4 into mFrontPanelStatusAddress) are faithfully
 * reproduced but not semantically decoded (contrast/backlight/calibration --
 * not determined) -- not needed for the boot-path milestone.
 *
 * BUG FIX (round 60): a PRE-EXISTING bug from before this session's own
 * work -- `LoadStoredSettings()` had a bogus extra read from
 * `mFrontPanelStatusAddress+0xe0` that does NOT exist anywhere in the real
 * ground-truth decompile, and silently discarded `sCurrentSettings`
 * (frontPanel+0xc4) into a dead local instead of persisting it as the real,
 * NAMED global ground truth actually has (`sCurrentSettings` is a genuine
 * Ghidra-recovered symbol, not a placeholder `DAT_xxxxxxxx`). This was
 * invisible until this round because no KAT test previously exercised
 * `LoadStoredSettings()`'s own field mapping at all -- caught only because
 * `SetBacklightBrightness`/`ResetToInit`/`SaveCurrentSettings` (this round)
 * all read/write `sCurrentSettings` directly and require it to be real,
 * persistent state. Fixed: `sStoredSettings` shrunk from 9 to 7 real
 * elements (dropping the bogus +0xe0 read and the phantom 9th field),
 * `sCurrentSettings` promoted to its own persistent global, and
 * `sUnknownFc` added (frontPanel+0xe0's own backing store -- confirmed real
 * via `SaveCurrentSettings`'s own read of it, but genuinely never written
 * by `LoadStoredSettings`, a real quirk in ground truth itself, not a
 * further bug in this reconstruction).
 *
 * The 7 SetXxx() methods below (SetBlackLevel..SetPadDrive2) are ALL
 * confirmed genuinely empty in the real binary (each a literal 3-byte `mov
 * eax,0; ret`, ignoring every one of their own declared parameters) --
 * this Kronos build's LCD-control command path is a real, but partially
 * stubbed-out, no-op surface for THOSE 7. SetBacklightBrightness/
 * ResetToInit/SaveCurrentSettings below are genuinely real (not stubs).
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

/* Forward-declared with only the one real method SaveCurrentSettings()
 * below tail-calls -- the class itself (a dozen-plus-method calibration
 * facade) is genuinely out of scope. Minimal linkage-only counting stub,
 * same established pattern as sysex_msg_task_base.cpp's own
 * CSysExApiInstance::EventToMessage/MessageToEvent.
 */
class USTGAPICalibration {
public:
	static void SaveCurrentCalibrationToDisk();
};

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

	/* .text+0x08e1dc70, 68 bytes (round 60, solo). Sets sCurrentSettings'
	 * own byte 3 to `level`, then forwards {type=0x10,subtype=1,
	 * field8=0,field12=0x1a} via SendSTGMessageWithSource() -- the SAME
	 * message shape LoadStoredSettings() already sends.
	 */
	static void SetBacklightBrightness(unsigned char level);

	/* .text+0x08e1dd00, 81 bytes. Same shape as SetBacklightBrightness
	 * but hardcodes sCurrentSettings' byte 3 to 0x3f AND sUnknownFc to
	 * the float bit-pattern 0x3f800000 (1.0f) -- both real, transcribed
	 * exactly. Always returns true.
	 */
	static bool ResetToInit();

	/* .text+0x08e1dd60, 126 bytes. Writes sCurrentSettings + all 7
	 * sStoredSettings elements + sUnknownFc back out to
	 * mFrontPanelStatusAddress (the exact inverse of LoadStoredSettings'
	 * own read order), then tail-calls the already-real
	 * USTGAPICalibration::SaveCurrentCalibrationToDisk().
	 */
	static void SaveCurrentSettings();
};

#endif /* LCD_CONTROL_H */
