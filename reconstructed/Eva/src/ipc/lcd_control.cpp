/*
 * lcd_control.cpp  -  see include/lcd_control.h.
 *
 * LoadStoredSettings() (.text+0x08e1dde0, 176 bytes) transcribed from
 * Decomp/EVA_Decomp/eva_export/functions/LoadStoredSettings@08e1dde0.c;
 * FIXED round 60 (solo) -- see lcd_control.h's own header comment for the
 * pre-existing bogus-+0xe0-read / discarded-sCurrentSettings bug this
 * replaced.
 *
 * The fields read out of mFrontPanelStatusAddress are copied into module-scope
 * globals whose real names are mostly not recovered (Ghidra emitted them as bare
 * DAT_xxxxxxxx addresses with no symbol, except `sCurrentSettings` which IS a
 * real recovered symbol) -- kept as a small local array indexed the same way the
 * real disassembly does, rather than inventing named globals with no evidence behind
 * the names.
 */

#include "lcd_control.h"
#include "ustg_user_api.h"

/* STGMessage's real layout is Stage 2 (see ustg_user_api.h). LoadStoredSettings only
 * ever constructs one, with these 4 fields set before the call:
 *   u16 type    = 0x10
 *   u16 subtype = 1
 *   u32 field8  = 0
 *   u32 field12 = 0x1a
 * matching the real local_1c/local_1a/local_18/local_14/local_10 layout in the
 * decompile. A minimal local shape sufficient to reproduce that call, not a claim
 * about STGMessage's full real size.
 */
struct STGMessageLocalShape {
	unsigned short type;
	unsigned short subtype;
	unsigned int field8;
	unsigned int field12;
};

/* sStoredSettings[7] maps directly to ground truth's own DAT_093033e4/e8/ec/
 * f0/f4/f8/09303400, in that exact order -- 7 real globals, NOT 9 (round-60
 * fix, see lcd_control.h's own header comment). sCurrentSettings and
 * sUnknownFc are the 2 further real fields, each promoted to its own
 * persistent global since SetBacklightBrightness/ResetToInit/
 * SaveCurrentSettings (round 60) all read/write them directly.
 */
static unsigned int sStoredSettings[7];
static unsigned int sCurrentSettings;
static unsigned int sUnknownFc;

bool USTGAPILCDControl::LoadStoredSettings()
{
	unsigned char *base = (unsigned char *)USTGUserAPI::mFrontPanelStatusAddress;

	sStoredSettings[0] = *(unsigned int *)(base + 0xc8);
	sCurrentSettings = *(unsigned int *)(base + 0xc4);
	sStoredSettings[1] = *(unsigned int *)(base + 0xcc);
	sStoredSettings[2] = *(unsigned int *)(base + 0xd0);
	sStoredSettings[3] = *(unsigned int *)(base + 0xd4);
	sStoredSettings[4] = *(unsigned int *)(base + 0xd8);
	sStoredSettings[5] = *(unsigned int *)(base + 0xdc);
	/* local_10 = sCurrentSettings >> 0x18 in the real disassembly -- computed but
	 * never observed used before the function returns; preserved as a no-op read
	 * rather than dropped silently.
	 */
	(void)(sCurrentSettings >> 0x18);
	sStoredSettings[6] = *(unsigned int *)(base + 0xe4);

	STGMessageLocalShape msg;
	msg.type = 0x10;
	msg.subtype = 1;
	msg.field8 = 0;
	msg.field12 = 0x1a;

	USTGUserAPI::SendSTGMessageWithSource((const STGMessage *)&msg);
	return true;
}

/* Round 59 (solo): 7 confirmed genuinely-empty methods -- each a literal
 * 3-byte `mov eax,0; ret` in the real binary, ignoring every declared
 * parameter. See lcd_control.h's own header comment.
 */
bool USTGAPILCDControl::SetBlackLevel(eLCDColorSelect, unsigned char)
{
	return false;
}

bool USTGAPILCDControl::SetAllRGBLevels()
{
	return false;
}

bool USTGAPILCDControl::SetRGBLevel(eLCDColorSelect, eLCDColorSelect, short)
{
	return false;
}

bool USTGAPILCDControl::SetContrast(float)
{
	return false;
}

bool USTGAPILCDControl::SetColorTemp(eLCDColorTemp)
{
	return false;
}

bool USTGAPILCDControl::SetSpreadSpectrumClock(unsigned char, unsigned char)
{
	return false;
}

bool USTGAPILCDControl::SetPadDrive2(unsigned char, unsigned char)
{
	return false;
}

/* Round 60 (solo). */

void USTGAPILCDControl::SetBacklightBrightness(unsigned char level)
{
	((unsigned char *)&sCurrentSettings)[3] = level;

	STGMessageLocalShape msg;
	msg.type = 0x10;
	msg.subtype = 1;
	msg.field8 = 0;
	msg.field12 = 0x1a;
	USTGUserAPI::SendSTGMessageWithSource((const STGMessage *)&msg);
}

bool USTGAPILCDControl::ResetToInit()
{
	sUnknownFc = 0x3f800000; /* 1.0f, confirmed real hardcoded bit pattern */
	((unsigned char *)&sCurrentSettings)[3] = 0x3f;

	STGMessageLocalShape msg;
	msg.type = 0x10;
	msg.subtype = 1;
	msg.field8 = 0;
	msg.field12 = 0x1a;
	USTGUserAPI::SendSTGMessageWithSource((const STGMessage *)&msg);
	return true;
}

void USTGAPILCDControl::SaveCurrentSettings()
{
	unsigned char *base = (unsigned char *)USTGUserAPI::mFrontPanelStatusAddress;

	*(unsigned int *)(base + 0xc4) = sCurrentSettings;
	*(unsigned int *)(base + 0xc8) = sStoredSettings[0];
	*(unsigned int *)(base + 0xcc) = sStoredSettings[1];
	*(unsigned int *)(base + 0xd0) = sStoredSettings[2];
	*(unsigned int *)(base + 0xd4) = sStoredSettings[3];
	*(unsigned int *)(base + 0xd8) = sStoredSettings[4];
	*(unsigned int *)(base + 0xdc) = sStoredSettings[5];
	*(unsigned int *)(base + 0xe0) = sUnknownFc;
	*(unsigned int *)(base + 0xe4) = sStoredSettings[6];

	USTGAPICalibration::SaveCurrentCalibrationToDisk();
}

/* Minimal linkage-only counting stub -- SaveCurrentSettings() above is the
 * first real caller of this symbol anywhere in this project (the class
 * itself, a dozen-plus-method calibration facade, is genuinely out of
 * scope). Same established pattern as sysex_msg_task_base.cpp's own
 * CSysExApiInstance::EventToMessage/MessageToEvent.
 */
int g_lcdTestSaveCalibrationCalls = 0;
void USTGAPICalibration::SaveCurrentCalibrationToDisk()
{
	g_lcdTestSaveCalibrationCalls++;
}
