// SPDX-License-Identifier: GPL-2.0
/*
 * omap_nks_msg_handler.cpp  -  CSTGOmapNKSMsgHandler::ProcessNextNKSEvent()
 * and its sibling free function ShortInvertNkS4RawAnalogValue.
 *
 * See include/oa_omap_nks_msg_handler.h for the full ground-truth
 * provenance and event-dispatch table.
 */

#include "oa_omap_nks_msg_handler.h"

/* ShortInvertNkS4RawAnalogValue -- see header comment. */
void ShortInvertNkS4RawAnalogValue(unsigned short val, unsigned short *outShifted,
				    unsigned short *outInverted)
{
	*outShifted = (unsigned short)(val >> 3);
	*outInverted = (val == 0x200) ? (unsigned short)0x200 : (unsigned short)(0x3ff - val);
}

/* Lazy-init guard for the front-panel-specific debounce filter instance
 * (.bss+0x2371f4 in ground truth) and its own static storage
 * (.bss+0x2367e0 in ground truth -- 128 * 0x14-byte records + a 4-byte
 * trailing field, see keybed_debounce.cpp's own CSTGKeybedKeyDebounceFilter_
 * Initialize() comment for the full record shape). Genuinely SEPARATE
 * from CSTGKeybedInterface's own embedded debounce filter. */
static bool sDebounceFilterInitialized;
static unsigned char sFrontPanelDebounceFilter[128 * 0x14 + 0x14];

/* Raw-analog test-mode capture state (.bss+0x2371f6/0x2371f8/0x2371f9
 * in ground truth). Written by type 0x61, consumed/reset by type 0x62. */
static short sNks4TestRawData;
static signed char sNks4TestRawDataController;
static bool sNks4TestRawDataReceived;

bool CSTGOmapNKSMsgHandler::ProcessNextNKSEvent()
{
	if (!sDebounceFilterInitialized) {
		CSTGKeybedKeyDebounceFilter_Initialize(sFrontPanelDebounceFilter);
		sDebounceFilterInitialized = true;
	}

	unsigned char *status = (unsigned char *)STGAPIFrontPanelStatus::sInstance;
	if (COmapNKS4Driver_GetSPDIFClockError())
		status[0x1090] |= 0x04;
	else
		status[0x1090] &= ~0x04u;

	unsigned char pkt[4] = { 0, 0, 0, 0 };
	if (!OmapNKS4InputFifo_ReadCommand(pkt))
		return false;

	unsigned char b0 = pkt[0], b1 = pkt[1], b2 = pkt[2], type = pkt[3];

	if (type == 0) {
		if ((signed char)b2 < 0)
			return true;

		unsigned int masked = b2 & 0xf0u;
		if (masked == 0x30 || masked == 0x40) {
			bool pressed = (masked == 0x30);
			unsigned int code = b1 & 0x7fu;

			if (COmapNKS4Driver_GetTestMode()) {
				STGNKSUnsolMsg20 msg;
				msg.size = 0x14;
				msg.flags = 1;
				msg.reserved = 0;
				msg.subtype = 0x7;
				msg.value = code;
				msg.extra = pressed ? 0x7fu : 0u;
				PushUnsolicitedMessage(&msg);
			} else {
				CSTGFrontPanel::sInstance->HandleSwitchEvent(code, pressed);
			}
			return true;
		}
		if (masked == 0x10) {
			unsigned int eventType = b2 & 0xfu;
			int coord = (int)(((unsigned int)b1 << 8) | b0);
			CSTGFrontPanel::sInstance->HandleTouchPanel(eventType, coord);
			return true;
		}
		return true;
	}

	if (type == 1) {
		if ((b2 & 0xf0u) != 0x50u)
			return true;
		int delta = (int)(((unsigned int)b1 << 8) | b0);
		CSTGFrontPanel::sInstance->HandleRotary(delta);
		return true;
	}

	if (type == 3) {
		unsigned short outShifted = 0, outInverted = 0;
		ShortInvertNkS4AnalogValue(b0, b1, &outShifted, &outInverted);
		unsigned int deviceCode = b2 & 0x3fu;
		unsigned char param2 = (unsigned char)outShifted;
		unsigned short param3 = outInverted;
		CSTGFrontPanel::sInstance->HandleAnalogController(deviceCode, param2, param3);
		return true;
	}

	if (type == 8) {
		STGNKSUnsolMsg16 msg;
		msg.size = 0x10;
		msg.flags = 1;
		msg.reserved = 0;
		msg.subtype = 0x2a;
		msg.value = 0;
		PushUnsolicitedMessage(&msg);
		return true;
	}

	if (type == 0x1f) {
		STGNKSUnsolMsg20 msg;
		msg.size = 0x14;
		msg.flags = 1;
		msg.reserved = 0;
		msg.subtype = 0x2b;
		msg.value = b2 & 0x7fu;
		msg.extra = b1 & 0x1u;
		PushUnsolicitedMessage(&msg);
		return true;
	}

	if (type == 0x61) {
		unsigned int deviceCode = b2 & 0x3fu;
		short raw = (short)(((unsigned int)b0 << 8) | b1);
		sNks4TestRawDataReceived = true;
		sNks4TestRawDataController = (signed char)deviceCode;
		sNks4TestRawData = raw;

		if (deviceCode == 7) {
			unsigned short outShifted = 0, outInverted = 0;
			ShortInvertNkS4RawAnalogValue((unsigned short)raw, &outShifted, &outInverted);
			sNks4TestRawData = (short)outInverted;
		}
		return true;
	}

	if (type == 0x62) {
		if (sNks4TestRawDataReceived) {
			unsigned int deviceCode = b2 & 0x3fu;
			if ((int)sNks4TestRawDataController == (int)deviceCode) {
				short combined = (short)(((unsigned int)b0 << 8) | b1);
				STGNKSUnsolMsg24 msg;
				msg.size = 0x18;
				msg.flags = 1;
				msg.reserved = 0;
				msg.subtype = 0x12;
				msg.deviceCode = sNks4TestRawDataController;
				msg.value = sNks4TestRawData;
				msg.scanCode = combined >> 3;
				PushUnsolicitedMessage(&msg);
			}
		}
		sNks4TestRawDataReceived = false;
		return true;
	}

	return true;
}
