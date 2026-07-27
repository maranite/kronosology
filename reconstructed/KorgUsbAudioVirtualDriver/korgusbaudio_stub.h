// SPDX-License-Identifier: GPL-2.0
/*
 * korgusbaudio_stub.h  -  shared state/declarations for the KorgUsbAudio*
 * stub. See README.md for the full design rationale.
 *
 * Freestanding, host-testable core (no kernel headers) -- module_main.cpp
 * is the separate, kernel-only file that wires this into a real .ko via
 * EXPORT_SYMBOL, matching AT88VirtualChip's own split.
 */

#ifndef KORGUSBAUDIO_STUB_H
#define KORGUSBAUDIO_STUB_H

/*
 * A small ring buffer standing in for the real USB audio codec's DMA
 * buffers. Real KorgUsbAudioDriver.ko uses module-global static state
 * (confirmed via its own disassembly -- e.g. KorgUsbAudioOutput() at
 * .text+0x310 computes `ds:0x2544 + ds:0x254c * ds:0x25a0`, a classic
 * base+stride*index ring-buffer-slot accessor, no arguments); this struct
 * is the reconstruction's equivalent, sized generously rather than
 * matched byte-for-byte to the real driver's undetermined buffer size
 * (irrelevant for a stub that never touches real hardware).
 */
struct KorgUsbAudioStubState {
	int  initialized;
	int  started;
	unsigned int outputIndex;
	unsigned int inputIndex;
	/* A tiny dummy sample buffer -- real callers (OA.ko's audio tick
	 * routines) read/write through the pointer KorgUsbAudioOutput()/
	 * KorgUsbAudioInput() return; since no real codec exists, this is
	 * just scratch memory so those reads/writes don't fault. */
	unsigned char outputBuf[256];
	unsigned char inputBuf[256];
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Confirmed via direct disassembly of the REAL KorgUsbAudioDriver.ko
 * binary (ARCHIVE/Ignored/DecryptedImages/MOD_Extracted/
 * KorgUsbAudioDriver.ko) -- every one of these takes NO ARGUMENTS
 * (operates on the driver's own static state) and returns an int status/
 * bool code or a void* buffer pointer:
 *   KorgUsbAudioStart        .text+0x230  (49B)  -- 0=success, else a status code
 *   KorgUsbAudioInitialized  .text+0x350  (8B)   -- bool flag read
 *   KorgUsbAudioDone         .text+0x630  (128B) -- status code, checks 3 flags
 *   KorgUsbAudioOutput       .text+0x310  (21B)  -- base+stride*index pointer, no args
 *   KorgUsbAudioInput        .text+0x2d0  (21B)  -- same shape as Output
 *   KorgUsbAudioOutputDone   .text+0x330  (28B)  -- void, advances/wraps a ring index
 *   KorgUsbAudioInputStarving .text+0x270 (43B)  -- bool, ring-fullness check
 * KorgUsbAudioInitialize (.text+0xf70, 495B) is too large to fully trace
 * in this pass; its callers (confirmed via OA.ko's own
 * CSTGAudioDriverInterfaceKorgUsb::Initialize(), which calls it but
 * discards the return value entirely) don't depend on any particular
 * return value, so `int` is used defensively without asserting the
 * real meaning of nonzero results.
 */
int   KorgUsbAudioInitialize(void);
int   KorgUsbAudioInitialized(void);
int   KorgUsbAudioStart(void);
int   KorgUsbAudioDone(void);
void *KorgUsbAudioOutput(void);
void *KorgUsbAudioInput(void);
void  KorgUsbAudioOutputDone(void);
int   KorgUsbAudioInputStarving(void);

/*
 * NOT individually disassembled in this pass -- inferred by consistent
 * naming/shape symmetry with their sibling functions above (same
 * no-argument, int/void-returning convention), not independently
 * confirmed. Flagged here rather than silently treated as equally solid.
 */
void  KorgUsbAudioInputDone(void);
int   KorgUsbAudioOutputStarving(void);
const char *KorgUsbAudioErrorString(int code);
int   KorgUsbAudioFormatSize(int format);
const char *KorgUsbAudioFormatString(int format);
void  KorgUsbAudioPrintIndices(void);

/*
 * KorgUsbMidi (etc.) & KorgUsbRealtimeMidiOutput (etc.) family -- same real binary,
 * combined audio+MIDI driver (confirmed via readelf -sW on the real
 * KorgUsbAudioDriver.ko: both families are exported from the same
 * object).
 *
 * UPDATE (OA.ko reconstruction, reconstructed/OA/src/engine/
 * midi_korgusb_port.cpp): full `objdump -dr` of OA.ko's own real call
 * sites (`CKorgUsbAudioDriverMidiPorts::CMidiPortPair::Connect()`/
 * `Disconnect()`, `CSTGMidiOutPortKorgUsb`'s ctor) proves this file's
 * PRIOR signatures here were a real ABI mismatch, not just an
 * unconfirmed guess: `KorgUsbMidiInitialize`/`Initialized`/`Done` all
 * take a real `int idx` argument (`Initialize` additionally takes 2
 * `unsigned int` buffer-size args + a `void *userdata`, regparm(3) + 1
 * stack arg), and `KorgUsbMidiOutput`/`KorgUsbRealtimeMidiOutput`
 * return `void` (not `int`) and take `unsigned char *` (not
 * `const void *`). Fixed here to match OA.ko's own confirmed real
 * calling convention -- see MASTER_REFERENCE.md/HARDWARE_REVIEW_LOG.md
 * for the cross-reference; this is the actual real ABI, not inference.
 */
int  KorgUsbMidiInitialize(int idx, unsigned int bufSizeA, unsigned int bufSizeB, void *userdata);
int  KorgUsbMidiInitialized(int idx);
int  KorgUsbMidiDone(int idx);
void KorgUsbMidiOutput(int port, unsigned char *data, unsigned int length);
int  KorgUsbMidiOutputCanSend(int port);
void KorgUsbRealtimeMidiOutput(int port, unsigned char *data, unsigned int length);
int  KorgUsbRealtimeMidiOutputCanSend(int port);

/*
 * ADDED (2026-07-11): closes a gap found in a live boot test (kronosvm) --
 * `insmod OA.ko` failed symbol resolution on 12 unknown symbols, one of
 * which was this one, called by OA.ko's own `CSTGDrumPadInterface_
 * Initialize()`/`_Cleanup()` (reconstructed/OA/src/init/drumpad_init.cpp,
 * init_module step 15, a SOFT gate -- return value unchecked by its
 * caller).
 *
 * IMPORTANT DISCREPANCY, left visible rather than silently papered over:
 * this project's own earlier `readelf -sW` survey of the REAL
 * KorgUsbAudioDriver.ko (see MASTER_REFERENCE.md, the KorgUsbAudioDriver.ko
 * exported-surface investigation) explicitly confirmed
 * `USBMidiAccessory_SetDrumPadClient`/`SetMidiInClient` are ABSENT from
 * that binary's real exports -- on REAL hardware this symbol is resolved
 * by a separate `USBMidiAccessory.ko` module, never reconstructed/stubbed
 * anywhere in this project. There is currently no dedicated virtual
 * stand-in for that module, so this symbol is added HERE purely as a
 * pragmatic VM-boot-test convenience (this .ko is already in every test's
 * load order, and the two modules cover closely related USB-MIDI-ish
 * ground) -- NOT a claim that real KorgUsbAudioDriver.ko exports it. A
 * future batch adding a real `USBMidiAccessory.ko` stand-in should move
 * this declaration/definition/export there instead.
 *
 * Real signature confirmed via OA.ko's own drumpad_init.cpp disassembly
 * comment: takes one `void *` (a receive-event-queue pointer to register,
 * or NULL to unregister), returns int (real body's own return value is
 * passed straight through by `CSTGDrumPadInterface_Initialize()`, so any
 * non-crashing value is safe here since that call site's own result is
 * itself soft/unchecked further up).
 */
int  USBMidiAccessory_SetDrumPadClient(void *queue);

/*
 * ADDED (2026-07-27): closes the exact same class of gap as
 * `USBMidiAccessory_SetDrumPadClient` above, for its sibling. Found
 * during an OA.ko dynamic-sweep insmod test: `midi_usb_accessory_port.cpp`
 * (reconstructed/OA, `CSTGUSBMidiAccessoryMidiInPort::Activate()`/
 * `Deactivate()`) calls this real companion-module symbol, but unlike
 * `SetDrumPadClient` it was never given the matching pragmatic VM
 * stand-in here when that file was added -- a pure oversight, not a new
 * design decision. SAME "IMPORTANT DISCREPANCY" caveat as
 * `SetDrumPadClient` applies verbatim: this project's own
 * `readelf -sW` survey of the REAL KorgUsbAudioDriver.ko confirmed
 * `USBMidiAccessory_Set{DrumPad,MidiIn}Client` are BOTH absent from that
 * binary's real exports -- on real hardware both are resolved by the
 * separate, never-reconstructed `USBMidiAccessory.ko`. Added here purely
 * as a pragmatic VM-boot-test convenience (this .ko is already in every
 * test's load order); a future real `USBMidiAccessory.ko` stand-in should
 * move both declarations/definitions/exports there instead.
 *
 * Real signature confirmed via OA.ko's own midi_usb_accessory_port.cpp
 * disassembly comment: single `void *` arg (a `CMidiInClient*` to
 * register, or NULL to unregister), VOID return (unlike SetDrumPadClient,
 * which returns int) -- confirmed by the real call sites never using a
 * result.
 */
void USBMidiAccessory_SetMidiInClient(void *client);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* KORGUSBAUDIO_STUB_H */
