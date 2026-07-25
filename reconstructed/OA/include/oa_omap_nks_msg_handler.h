// SPDX-License-Identifier: GPL-2.0
#ifndef OA_OMAP_NKS_MSG_HANDLER_H
#define OA_OMAP_NKS_MSG_HANDLER_H

/*
 * oa_omap_nks_msg_handler.h  -  CSTGOmapNKSMsgHandler::ProcessNextNKSEvent(),
 * the real USB-NKS4-panel event pump that HandleSwitchEvent/HandleRotary/
 * HandleTouchPanel/HandleAnalogController (front_panel_handlers.cpp,
 * batch 63) already anticipated but never had a reconstructed caller for.
 *
 * Found via the same class-level `nm -C` sweep that turned up
 * CSTGFrontPanelMsgHandler this pass -- there is exactly ONE real method
 * on this class, confirmed previously unclaimed (`grep -rl
 * CSTGOmapNKSMsgHandler reconstructed/OA/` before this pass only hit
 * doc-comment mentions in oa_setup_global_resources.h/front_panel_
 * handlers.cpp noting it as "not reconstructed in this pass").
 *
 * Ground truth: `CSTGOmapNKSMsgHandler::ProcessNextNKSEvent()`, OA_real.ko
 * `.text+0x2073d0`..`.text+0x20774a` (~890 bytes, nm addresses against
 * `/home/share/Decomp/OA.ko_Decomp/OA.ko`). Transcribed from a full
 * `objdump -dr -M intel` disassembly (not decompile text).
 *
 * Shape: polls one 4-byte event packet per call via the confirmed-real-
 * but-external `OmapNKS4InputFifo_ReadCommand()` (companion
 * OmapNKS4Module.ko import, own body out of scope -- same treatment as
 * the already-established `OmapNKS4OutputFifo_WriteCommand`), then
 * dispatches on the packet's 4th byte (`type`) to one of the ALREADY-
 * REAL `CSTGFrontPanel::Handle*` methods, or builds a small
 * `PushUnsolicitedMessage()` packet directly for a handful of "raw
 * report" event types. Returns `true` if an event was consumed (matches
 * every code path except "no event pending"), `false` only when
 * `OmapNKS4InputFifo_ReadCommand()` itself reports nothing available.
 *
 * Also lazily initializes a SECOND, front-panel-specific
 * `CSTGKeybedKeyDebounceFilter` instance on first call (confirmed via
 * relocation: `_ZN27CSTGKeybedKeyDebounceFilter10InitializeEv`, the
 * EXACT SAME mangled real symbol keybed_debounce.cpp already
 * reconstructed as the free function `CSTGKeybedKeyDebounceFilter_
 * Initialize(unsigned char *filter)` -- called here with a DIFFERENT
 * static storage blob (`.bss+0x2367e0`) than the keybed's own embedded
 * sub-object at `CSTGKeybedInterface::sInstance + KEYBED_OFF_DEBOUNCE_
 * FILTER`; ground truth genuinely has two independent debounce-filter
 * instances, one per input subsystem).
 *
 * Also toggles a SPDIF-clock-error status bit
 * (`STGAPIFrontPanelStatus::sInstance+0x1090`, bit `0x4`) off the
 * confirmed-real-but-external `COmapNKS4Driver_GetSPDIFClockError()`
 * (companion-module import, same "confirmed real export, own body out
 * of scope" treatment as `COmapNKS4Driver_GetTestMode`/
 * `_StartScanning`) on EVERY call, independent of the event dispatch
 * below it.
 *
 * Event-type dispatch (packet byte 3, `type`):
 *   0x00 -> further decode on byte 2 (`b2`):
 *     b2 < 0                     : ignored (no-op, returns true)
 *     (b2 & 0xf0) == 0x30 or 0x40: switch/button event -- in test mode
 *       (`COmapNKS4Driver_GetTestMode()`), builds a raw-capture
 *       PushUnsolicitedMessage (subtype 0x07); otherwise forwards to
 *       `CSTGFrontPanel::HandleSwitchEvent(code, pressed)` (pressed =
 *       masked==0x30).
 *     (b2 & 0xf0) == 0x10        : forwards to `CSTGFrontPanel::
 *       HandleTouchPanel(eventType=b2&0xf, coord=(b1<<8)|b0)`.
 *     else                       : ignored (no-op).
 *   0x01: if (b2&0xf0)==0x50, forwards to `CSTGFrontPanel::
 *     HandleRotary(delta=(b1<<8)|b0)`; else ignored.
 *   0x03: forwards to `CSTGFrontPanel::HandleAnalogController` via the
 *     already-real `ShortInvertNkS4AnalogValue()` byte-pair converter.
 *   0x08: unconditional 16-byte PushUnsolicitedMessage (subtype 0x2a,
 *     value 0) -- real semantics of this fixed "no-payload" report not
 *     independently confirmed (plausibly a keybed-ready/connect ping).
 *   0x1f: builds a 20-byte PushUnsolicitedMessage (subtype 0x2b,
 *     value=b2&0x7f, extra=b1&1) -- touch-panel presence/status change.
 *   0x61: raw-analog TEST-MODE capture -- stashes (deviceCode=b2&0x3f,
 *     rawValue=(b0<<8)|b1) into static state; if deviceCode==7,
 *     immediately runs it through the newly-reconstructed
 *     `ShortInvertNkS4RawAnalogValue()` and overwrites the stashed raw
 *     value with the converted one. Never itself sends a message.
 *   0x62: if the 0x61 capture above ran AND its deviceCode still
 *     matches this packet's `b2&0x3f`, builds a 24-byte
 *     PushUnsolicitedMessage (subtype 0x12, deviceCode, value=the
 *     stashed/converted raw value, scanCode=((b0<<8)|b1 as signed16)>>3)
 *     -- same byte shape as `CSTGCalibrationMsgHandler`'s own
 *     `STGCalibrationUnsolMsg` (oa_calibration.h), kept as an
 *     independent local struct per this project's own established
 *     precedent for byte-identical-but-conceptually-distinct reply
 *     shapes. Always resets the "capture pending" flag afterward
 *     (one-shot: a 0x62 with no prior matching 0x61 just resets and
 *     sends nothing).
 *   anything else (0x02,0x04-0x07,0x09-0x1e,0x20-0x60 minus 0x1f/0x61/
 *     0x62/anything > 0x62 besides those): ignored, returns true.
 */

#include "oa_setup_global_resources.h"	/* CSTGFrontPanel, STGAPIFrontPanelStatus */
#include "oa_keybed_init.h"		/* CSTGKeybedKeyDebounceFilter_Initialize */

extern "C" {
/* Companion OmapNKS4Module.ko imports -- confirmed real via `nm -C`
 * showing both as undefined ("U") in OA.ko, own bodies out of scope
 * (same treatment as OmapNKS4OutputFifo_WriteCommand/
 * COmapNKS4Driver_GetTestMode/_StartScanning elsewhere in this
 * project). `OmapNKS4InputFifo_ReadCommand` fills a 4-byte packet
 * (byte0, byte1, byte2, type) and returns nonzero iff an event was
 * available. */
int OmapNKS4InputFifo_ReadCommand(void *buf);
int COmapNKS4Driver_GetSPDIFClockError(void);
void PushUnsolicitedMessage(void *msg);
}

/* ShortInvertNkS4RawAnalogValue(ushort val, ushort *outShifted,
 * ushort *outInverted) (.text+0x2077c0, 39 bytes, confirmed via
 * objdump -dr) -- plain regparm(3) free function, sibling of the
 * already-real `ShortInvertNkS4AnalogValue` (front_panel_handlers.cpp):
 * EAX=val, EDX=&outShifted, ECX=&outInverted.
 *   *outShifted = val >> 3;
 *   *outInverted = (val == 0x200) ? 0x200 : (0x3ff - val);
 * NOTE the output-pointer roles are NOT symmetric with the sibling
 * function's own (outHi,outLo) naming/order -- confirmed via this
 * function's own disassembly, not assumed from the sibling. */
extern "C" void ShortInvertNkS4RawAnalogValue(unsigned short val,
					       unsigned short *outShifted,
					       unsigned short *outInverted);

/* Generic small PushUnsolicitedMessage packet family this handler
 * builds -- same "size/flags/reserved/subtype[/payload]" shape already
 * established project-wide (STGControlReplyMsg/STGCalibrationUnsolMsg),
 * kept local to this cluster per this project's own precedent for
 * byte-identical-but-conceptually-distinct reply shapes. `flags` is
 * always the literal 1 in every observed call site here (unlike some
 * sibling structs' "_pad2 never written" convention -- this one IS
 * written). */
struct STGNKSUnsolMsg16 {
	unsigned short size;	/* +0x0, always 0x10 */
	unsigned short flags;	/* +0x2, always 1 */
	unsigned int reserved;	/* +0x4, always 0 */
	unsigned int subtype;	/* +0x8 */
	unsigned int value;	/* +0xc */
};

struct STGNKSUnsolMsg20 {
	unsigned short size;	/* +0x0, always 0x14 */
	unsigned short flags;	/* +0x2, always 1 */
	unsigned int reserved;	/* +0x4, always 0 */
	unsigned int subtype;	/* +0x8 */
	unsigned int value;	/* +0xc */
	unsigned int extra;	/* +0x10 */
};

struct STGNKSUnsolMsg24 {
	unsigned short size;	/* +0x0, always 0x18 */
	unsigned short flags;	/* +0x2, always 1 */
	unsigned int reserved;	/* +0x4, always 0 */
	unsigned int subtype;	/* +0x8, always 0x12 */
	int deviceCode;		/* +0xc, sign-extended from a byte */
	int value;		/* +0x10, sign-extended from a word */
	int scanCode;		/* +0x14, signed (arithmetic shift) */
};

class CSTGOmapNKSMsgHandler {
public:
	bool ProcessNextNKSEvent();
};

#endif
