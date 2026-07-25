// SPDX-License-Identifier: GPL-2.0
/*
 * midi_usb_accessory_port.cpp -- the generic-USB-MIDI-class accessory
 * hierarchy's small "activate/deactivate plumbing" methods (survey
 * candidate 3's "hierarchy 2", genuinely distinct from the KorgUsb
 * transport in midi_korgusb_port.cpp). See agent-memory
 * oa_hdrfilereader_processcommands_and_usbmidi_reassessment.md item 3
 * for the full reassessment this file picks up from: that session
 * inventoried this whole neighborhood and identified these specific
 * methods as genuinely sound and tractable but did not commit them
 * (pure time budget). This file closes that out.
 *
 * Ground truth, full `objdump -dr -M intel` transcription against
 * `/home/share/Decomp/OA.ko_Decomp/OA.ko`:
 *   CSTGUSBMidiAccessoryMidiInPort::Deactivate()            .text+0xfa7f0  36B
 *   CSTGUSBMidiAccessoryMidiInPort::Activate(CSTGMidiQueue*) .text+0xfa820  25B
 *   CSTGUSBMidiAccessoryMidiInPort::ShouldActivate() const  (own comdat)     6B
 *   CSTGMidiOutPortUSB::Activate(CSTGMidiQueue*)            (own comdat)    15B
 *   CUSBMidiAccessory_DrumPadClient::ReceiveNotification(...) (own comdat)   1B
 * See oa_engine.h (`CSTGUSBMidiAccessoryMidiInPort`,
 * `CUSBMidiAccessory_DrumPadClient`) and oa_engine_init.h
 * (`CSTGMidiOutPortUSB`) for the full class-layout derivation -- kept
 * there, not here, matching this project's established
 * class-comment-lives-with-the-class convention.
 *
 * Deliberately NOT in this file (still correctly blocked, see the
 * reassessment memory cited above for why each is a different KIND of
 * blocker, not just size):
 *   - CSTGMidiOutPortUSB::CanSendRealTime()/CanSendRegular()/
 *     SendRealTime()/SendSingleByte() -- bottom out in a genuinely
 *     UNRESOLVED __cxa_pure_virtual vtable slot in ground truth itself.
 *   - CSTGDrumPadClient::CanReceiveTriggerEvent()/ReceiveTriggerEvent()
 *     -- real fields accessed via addresses relocated against
 *     CSTGDrumPadInterface::sInstance+N, genuine linker-adjacency
 *     aliasing that can't be faithfully reproduced in a clean-room
 *     rebuild.
 *   - CMidiInClient::Receive() and the sUSBMidiAccessoryMidiInPort
 *     singleton's own construction -- needs its own dedicated decision
 *     on how to represent that singleton (opaque placeholder vs. a real
 *     ctor reconstruction of the `_GLOBAL__I_` trampoline), a separate
 *     scoped task.
 *   - CSTGMidiInPortUSB::ReceivePacket()/CSTGMidiOutPortUSB::
 *     ProcessRegularMessage()/CSTGDrumPadClient::ReceiveNotification()
 *     -- the pre-existing "big 3", unchanged, still disproportionate.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"

/*
 * USBMidiAccessory_SetMidiInClient(void*) -- CONFIRMED genuinely new
 * real external companion-module symbol (`U` in ground truth, same
 * family as the already-real `USBMidiAccessory_SetDrumPadClient`,
 * drumpad_init.cpp). Single-arg: the pointer goes in EAX under this
 * project's -mregparm=3 kernel build (ccflags-y, Makefile) -- no
 * explicit `regparm` attribute needed, matching every other single/
 * multi-arg extern "C" companion-module declaration in this codebase
 * (e.g. midi_korgusb_port.cpp's `KorgUsbMidi*` family).
 */
extern "C" void USBMidiAccessory_SetMidiInClient(void *client);

/*
 * sMidiInClient -- CONFIRMED real singleton (`.bss+0x1056e8`, 4 bytes:
 * a bare `CMidiInClient` instance -- vtable pointer only, no other
 * fields). Its own type's real behavior (`CMidiInClient::Receive()`) is
 * explicitly out of scope this pass (see header comment above); modeled
 * here as an opaque 4-byte placeholder purely so `Activate()`/
 * `Deactivate()` can take/pass its address, matching this project's
 * established "opaque slot for a not-yet-reconstructed singleton"
 * convention (e.g. `CSTGMidiPortManager::sMidiInPorts`, oa_engine.h;
 * `gDrumPadReceiveQueue`, drumpad_init.cpp).
 */
static unsigned char sMidiInClient[4];

/* ---------------------------------------------------------------------
 * CSTGUSBMidiAccessoryMidiInPort
 * ------------------------------------------------------------------- */

/* Activate(CSTGMidiQueue*) -- CONFIRMED real: base CSTGMidiInPort::
 * Activate() FIRST, THEN registers &sMidiInClient with the companion
 * module. */
void CSTGUSBMidiAccessoryMidiInPort::Activate(CSTGMidiQueue *q)
{
	((CSTGMidiInPort *)this)->Activate(q);
	USBMidiAccessory_SetMidiInClient(sMidiInClient);
}

/* Deactivate() -- CONFIRMED real: unregisters the companion module
 * (NULL) FIRST -- note the reversed order vs. Activate(), confirmed by
 * disassembly, not a copy-paste of Activate's own order -- THEN the
 * base CSTGMidiInPort::Deactivate(). */
void CSTGUSBMidiAccessoryMidiInPort::Deactivate()
{
	USBMidiAccessory_SetMidiInClient(0);
	((CSTGMidiInPort *)this)->Deactivate();
}

/* ---------------------------------------------------------------------
 * CSTGMidiOutPortUSB
 * ------------------------------------------------------------------- */

/* Activate(CSTGMidiQueue*) -- CONFIRMED real: a trivial forward to the
 * base CSTGMidiOutPort::Activate(), no other work. */
void CSTGMidiOutPortUSB::Activate(CSTGMidiQueue *q3)
{
	((CSTGMidiOutPort *)this)->Activate(q3);
}
