/*
 * lcd_control.cpp  -  see include/lcd_control.h.
 *
 * LoadStoredSettings() (.text+0x08e1dde0, 176 bytes) transcribed from
 * Decomp/EVA_Decomp/eva_export/functions/LoadStoredSettings@08e1dde0.c.
 *
 * The 9 fields read out of mFrontPanelStatusAddress are copied into module-scope
 * globals whose real names are not recovered (Ghidra emitted them as bare DAT_xxxxxxxx
 * addresses with no symbol) -- kept as a small local array indexed the same way the
 * real disassembly does, rather than inventing 9 named globals with no evidence behind
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

static unsigned int sStoredSettings[9];

bool USTGAPILCDControl::LoadStoredSettings()
{
	unsigned char *base = (unsigned char *)USTGUserAPI::mFrontPanelStatusAddress;

	sStoredSettings[0] = *(unsigned int *)(base + 0xc8);
	unsigned int currentSettings = *(unsigned int *)(base + 0xc4);
	sStoredSettings[1] = *(unsigned int *)(base + 0xcc);
	sStoredSettings[2] = *(unsigned int *)(base + 0xd0);
	sStoredSettings[3] = *(unsigned int *)(base + 0xd4);
	sStoredSettings[4] = *(unsigned int *)(base + 0xd8);
	sStoredSettings[5] = *(unsigned int *)(base + 0xdc);
	sStoredSettings[6] = *(unsigned int *)(base + 0xe0);
	/* local_10 = sCurrentSettings >> 0x18 in the real disassembly -- computed but
	 * never observed used before the function returns; preserved as a no-op read
	 * rather than dropped silently.
	 */
	(void)(currentSettings >> 0x18);
	sStoredSettings[7] = *(unsigned int *)(base + 0xe4);

	STGMessageLocalShape msg;
	msg.type = 0x10;
	msg.subtype = 1;
	msg.field8 = 0;
	msg.field12 = 0x1a;

	USTGUserAPI::SendSTGMessageWithSource((const STGMessage *)&msg);
	return true;
}
