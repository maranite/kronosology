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
 * object). Only KorgUsbMidiOutput's argument shape was disassembly-
 * confirmed in this pass (three args under -mregparm=3: a port index,
 * a data pointer, a length) -- the rest use the same shape by
 * inference, not independently confirmed.
 */
int  KorgUsbMidiInitialize(void);
int  KorgUsbMidiInitialized(void);
int  KorgUsbMidiDone(void);
int  KorgUsbMidiOutput(int port, const void *data, unsigned int length);
int  KorgUsbMidiOutputCanSend(int port);
int  KorgUsbRealtimeMidiOutput(int port, const void *data, unsigned int length);
int  KorgUsbRealtimeMidiOutputCanSend(int port);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* KORGUSBAUDIO_STUB_H */
