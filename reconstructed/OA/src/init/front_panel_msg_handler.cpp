// SPDX-License-Identifier: GPL-2.0
/*
 * front_panel_msg_handler.cpp  -  CSTGFrontPanelMsgHandler, the
 * MIDI-sourced front-panel LED/beep control message dispatch table.
 *
 * See include/oa_front_panel_msg_handler.h for the full ground-truth
 * provenance and the "install vs dispatch" vtable note.
 */

#include "oa_front_panel_msg_handler.h"

/* Non-virtual member-function-pointer -> raw void* type pun. Same
 * rationale as control_msg_handler.cpp's own AsRawFn (2-word Itanium
 * ABI pointer-to-member-function, only the leading plain-address word
 * is copied out; install-only, nothing dispatches through sMsgHandler
 * yet). Local `static` copy, not shared across translation units. */
template <typename T>
static void *AsRawFn(T memberFnPtr)
{
	void *raw;
	__builtin_memcpy(&raw, &memberFnPtr, sizeof(raw));
	return raw;
}

CSTGFrontPanelMsgHandler *CSTGFrontPanelMsgHandler::sInstance;
STGFrontPanelMsgHandlerEntry CSTGFrontPanelMsgHandler::sMsgHandler[5];

/*
 * Real vtable data (24 bytes / 4 slots, confirmed via `readelf -sW`
 * against ground truth's own `_ZTV24CSTGFrontPanelMsgHandler`).
 * Zero-filled placeholder, matching this project's established
 * "install vs dispatch" rule -- nothing in this project dispatches
 * through it.
 *
 * FIXED (2026-07-27): the ctor previously stored a bare literal
 * `nullptr` here instead of this real symbol. Ground truth's own ctor
 * (`.text+0xe9f80`) does `mov DWORD PTR [eax],0x8` with a real
 * `R_386_32 _ZTV24CSTGFrontPanelMsgHandler` relocation -- confirmed via
 * `objdump -dr` against OA.ko_Decomp/OA.ko -- so a bare null was a
 * genuine value mismatch ("install vs dispatch" means nothing reads it
 * back through a vtable slot, not that the field itself should be left
 * null).
 */
extern "C" unsigned char _ZTV24CSTGFrontPanelMsgHandler[24] = { 0 };

/* ------------------------------------------------------------------ */
/* All 5 methods ignore their own `this` (the CSTGFrontPanelMsgHandler
 * instance) -- confirmed real: EAX is clobbered by the very first
 * instruction in each (loading CSTGFrontPanel::sInstance) and never
 * read again. `source` (eSTGMidiSource) is likewise unused by every
 * method, matching this project's established convention for this
 * not-independently-defined enum (see oa_control_msg_handler.h). */

void CSTGFrontPanelMsgHandler::SetLED(const STGMsgDataOneParam *param, int)
{
	CSTGFrontPanel::sInstance->SetLED(param->value);
}

void CSTGFrontPanelMsgHandler::ResetLED(const STGMsgDataOneParam *param, int)
{
	CSTGFrontPanel::sInstance->ResetLED(param->value);
}

void CSTGFrontPanelMsgHandler::SetLEDBlinking(const STGMsgDataOneParam *param, int)
{
	CSTGFrontPanel::sInstance->SetLEDBlinking(param->value);
}

void CSTGFrontPanelMsgHandler::SetLED16Bits(const STGMsgDataOneParam *param, int)
{
	CSTGFrontPanel::sInstance->SetLED16Bits(param->value);
}

void CSTGFrontPanelMsgHandler::Beep(const void *, int)
{
	CSTGFrontPanel::sInstance->Beep();
}

/* ------------------------------------------------------------------ */
/* Constructor: installs the vtable pointer, the &sMsgHandler table
 * pointer, the 0x05 reply tag, and all 5 {fn,ctx=NULL} table entries.
 * Order confirmed via the real constructor's own relocation sequence:
 * SetLED, SetLEDBlinking, ResetLED, SetLED16Bits, Beep. */
CSTGFrontPanelMsgHandler::CSTGFrontPanelMsgHandler()
{
	_vtablePtr = _ZTV24CSTGFrontPanelMsgHandler + 8;	/* real value, install-only, see header comment */
	_msgHandlerTable = &sMsgHandler;
	_replyTag = 0x05;

	sInstance = this;

	sMsgHandler[0].fn = reinterpret_cast<void (*)()>(
		AsRawFn(&CSTGFrontPanelMsgHandler::SetLED));
	sMsgHandler[0].ctx = nullptr;

	sMsgHandler[1].fn = reinterpret_cast<void (*)()>(
		AsRawFn(&CSTGFrontPanelMsgHandler::SetLEDBlinking));
	sMsgHandler[1].ctx = nullptr;

	sMsgHandler[2].fn = reinterpret_cast<void (*)()>(
		AsRawFn(&CSTGFrontPanelMsgHandler::ResetLED));
	sMsgHandler[2].ctx = nullptr;

	sMsgHandler[3].fn = reinterpret_cast<void (*)()>(
		AsRawFn(&CSTGFrontPanelMsgHandler::SetLED16Bits));
	sMsgHandler[3].ctx = nullptr;

	sMsgHandler[4].fn = reinterpret_cast<void (*)()>(
		AsRawFn(&CSTGFrontPanelMsgHandler::Beep));
	sMsgHandler[4].ctx = nullptr;
}

CSTGFrontPanelMsgHandler::~CSTGFrontPanelMsgHandler()
{
	/* volatile: see CSTGControlMsgHandler's identical dtor note. */
	*(void * volatile *)&_vtablePtr = _ZTV18CSTGMessageHandler + 8;
}
