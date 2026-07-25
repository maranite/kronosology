// SPDX-License-Identifier: GPL-2.0
#ifndef OA_FRONT_PANEL_MSG_HANDLER_H
#define OA_FRONT_PANEL_MSG_HANDLER_H

/*
 * oa_front_panel_msg_handler.h  -  CSTGFrontPanelMsgHandler: the
 * MIDI-sourced front-panel LED/beep control message dispatch table
 * (SetLED/ResetLED/SetLEDBlinking/SetLED16Bits/Beep).
 *
 * Found via a broad `nm -C OA.ko` class-level sweep for hardware-
 * sounding classes wholly absent from `reconstructed/OA/` (same
 * methodology that turned up CSTGControlMsgHandler the prior session --
 * see oa_control_msg_handler.h's own header comment). Confirmed
 * previously 100% unclaimed via `grep -rl CSTGFrontPanelMsgHandler
 * reconstructed/OA/` (zero hits before this pass) -- distinct from, and
 * much smaller than, `CSTGControlMsgHandler` (a different class, no
 * relation beyond both being `*MsgHandler` dispatch-table installers
 * that happen to touch front-panel state).
 *
 * Ground truth: `CSTGFrontPanelMsgHandler::*`, OA_real.ko
 * `.text+0xe9ee0`..`.text+0xe9fff` (nm addresses against
 * `/home/share/Decomp/OA.ko_Decomp/OA.ko`; add the project's documented
 * `+0x10000` for the Ghidra-export `oa_export/functions/` filenames).
 * Every body transcribed from a raw `objdump -dr -M intel` disassembly.
 *
 * All 5 real methods are pure one-call forwards to the ALREADY-real
 * `CSTGFrontPanel::SetLED`/`ResetLED`/`SetLEDBlinking` (front_panel_
 * handlers.cpp, established prior session) plus two NEWLY-added
 * `CSTGFrontPanel` methods this same pass, `SetLED16Bits`/`Beep`
 * (oa_setup_global_resources.h/front_panel_handlers.cpp) -- both were
 * confirmed real via relocation from THIS handler but had never been
 * reconstructed themselves. Every wrapper method here ignores its own
 * `this` (the CSTGFrontPanelMsgHandler instance) and dispatches through
 * `CSTGFrontPanel::sInstance` instead -- same "this unused, real state
 * off a different singleton" pattern established for HandleSwitchEvent
 * et al.
 *
 * Not a real C++-virtual dispatch target in this project: same
 * "install vs dispatch" treatment as `CSTGControlMsgHandler`/
 * `CSTGCalibrationMsgHandler` -- the real object installs a vtable
 * pointer at construction, but nothing in this project dispatches
 * through it, so it's modeled as a raw untyped pointer field.
 *
 * `sMsgHandler` (`.bss`, 5 entries, confirmed real via the
 * constructor's own 10-relocation table-populate sequence) is the SAME
 * `{fn,ctx}` PAIR shape as `CSTGCalibrationMsgHandler::sMsgHandler`
 * (`ctx` always NULL here, confirmed: every second dword the
 * constructor writes is a plain `0`, not a relocation) -- NOT the flat
 * raw-pointer shape `CSTGControlMsgHandler::sMsgHandler` uses. Order
 * confirmed via the constructor's own relocation sequence: SetLED,
 * SetLEDBlinking, ResetLED, SetLED16Bits, Beep.
 */

#include "oa_control_msg_handler.h"	/* STGMsgDataOneParam, CSTGFrontPanel */

struct STGFrontPanelMsgHandlerEntry {
	void (*fn)();
	void *ctx;
};

class CSTGFrontPanelMsgHandler {
public:
	static CSTGFrontPanelMsgHandler *sInstance;
	static STGFrontPanelMsgHandlerEntry sMsgHandler[5];

	void *_vtablePtr;		/* +0x0, install-only placeholder, see header comment */
	void *_msgHandlerTable;	/* +0x4, always &sMsgHandler */
	unsigned char _replyTag;	/* +0x8, default 0x05 */

	CSTGFrontPanelMsgHandler();

	void SetLED(const STGMsgDataOneParam *param, int source);
	void ResetLED(const STGMsgDataOneParam *param, int source);
	void SetLEDBlinking(const STGMsgDataOneParam *param, int source);
	void SetLED16Bits(const STGMsgDataOneParam *param, int source);
	/* Ignores `param` entirely -- confirmed, own body never touches EDX. */
	void Beep(const void *param, int source);
};

#endif
