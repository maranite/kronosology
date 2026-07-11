// SPDX-License-Identifier: GPL-2.0
/*
 * drumpad_init.cpp  -  CSTGDrumPadInterface_Initialize()/_Cleanup():
 * init_module step 15 (soft) / its own cleanup path.
 *
 * Ground truth: `CSTGDrumPadInterface_Initialize` (.text+0x33ce60, 20
 * bytes) and `CSTGDrumPadInterface_Cleanup` (.text+0x33cea0, 17 bytes)
 * are REAL standalone C-linkage symbols in OA_real.ko -- and each is
 * BYTE-IDENTICAL to its own mangled-C++ sibling
 * (`CSTGDrumPadInterface::Initialize()`/`::Cleanup()`, .text+0x33ce40/
 * 0x33ce80): the compiler emitted two separate entry points for the
 * same body (one C-linkage, one mangled), unlike the
 * SCalibrationData/CSTGKeybedKeyDebounceFilter case (see
 * calibration_data.cpp/keybed_debounce.cpp) where only the mangled
 * form exists. Neither function touches CSTGDrumPadInterface's own
 * fields at all -- both just register/unregister a receive-queue
 * pointer with the (separately provided) USB MIDI accessory driver:
 *
 *   Initialize(): `mov eax,&gDrumPadReceiveQueue; call
 *     USBMidiAccessory_SetDrumPadClient; <epilogue>; ret` -- the call's
 *     own return value is passed straight through as this function's
 *     return value (no `mov eax,...` after the call), matching the
 *     `int` return this project's header already declares.
 *   Cleanup(): `xor eax,eax; call USBMidiAccessory_SetDrumPadClient;
 *     <epilogue>; ret` -- unregisters with a NULL queue pointer, `void`
 *     return (matches the header; the call's own return value is
 *     simply discarded since the caller never reads EAX here).
 *
 * `USBMidiAccessory_SetDrumPadClient` is confirmed `U` (genuinely
 * external, resolved by whatever module owns the USB MIDI accessory
 * driver -- KorgUsbMidi* / USBMidiAccessory_SetMidiInClient live in the
 * same `U` family in ground truth) -- declared here, not defined; a
 * real external dependency, not something for this project to
 * substitute.
 *
 * `gDrumPadReceiveQueue`'s real ground-truth address (0x26d38c) sits
 * immediately after CSTGDrumPadInterface::sInstance's own confirmed
 * 0x4c-byte object (0x26d340+0x4c=0x26d38c) -- almost certainly a
 * private receive-event-queue buffer declared right after the
 * singleton in the real source, NOT part of sInstance itself. This
 * project does not model CSTGDrumPadInterface as a class at all (its
 * own receive-side methods -- ReceiveTriggerEvent/
 * DrainReceiveEventQueue/GetTriggerEvent/ResetAllPads/etc -- are not
 * reconstructed), so the queue's real size is unconfirmed; modeled as
 * an opaque placeholder since only its ADDRESS is ever used by either
 * function in this file, never its contents.
 */

#include "oa_init.h"

extern "C" int USBMidiAccessory_SetDrumPadClient(void *queue);

static unsigned char gDrumPadReceiveQueue[4]; /* opaque placeholder -- real size unconfirmed, see above */

int CSTGDrumPadInterface_Initialize(void)
{
	return USBMidiAccessory_SetDrumPadClient(gDrumPadReceiveQueue);
}

void CSTGDrumPadInterface_Cleanup(void)
{
	USBMidiAccessory_SetDrumPadClient(0);
}
