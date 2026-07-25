// SPDX-License-Identifier: GPL-2.0
/*
 * midi_out_port_serial.cpp  -  CSTGMidiOutPort's remaining real methods
 * (Activate/Deactivate/BumpTimers/ProcessNormal/ProcessNKS4TestMode/
 * GenerateActiveSensing/ProcessRealTimeMessage/ReadNextMessage/ctor) +
 * CSTGMidiOutPortSerial (the physical 5-pin-DIN MIDI-OUT UART port,
 * output-side counterpart to CSTGMidiInPortSerial, midi_in_port_serial.cpp)
 * + the two small ring-buffer helper classes both of them lean on,
 * CSTGMidiQueueReader and CSTGMidiQueueMessageReader.
 *
 * Ground truth: full `objdump -dr -M intel` against
 * OA.ko_Decomp/OA.ko for every symbol below, cross-checked against the
 * project's own pre-existing Ghidra decompile export
 * (/home/share/Decomp/oa_export/functions/) where available. Addresses:
 *   CSTGMidiOutPort::CSTGMidiOutPort(eSTGMidiPort,uint)  .text+0xf8270   95B
 *   CSTGMidiOutPort::Activate(CSTGMidiQueue*)            .text+0xf7f10  365B
 *   CSTGMidiOutPort::Deactivate()                        .text+0xf7d70    5B
 *   CSTGMidiOutPort::BumpTimers()                        .text+0x5a6880  14B (comdat)
 *   CSTGMidiOutPort::ProcessNormal()                     .text+0xf8330  145B
 *   CSTGMidiOutPort::ProcessNKS4TestMode()                .text+0xf82d0   93B
 *   CSTGMidiOutPort::GenerateActiveSensing()              .text+0xf83d0   78B
 *   CSTGMidiOutPort::ProcessRealTimeMessage()             .text+0xf8430   97B
 *   CSTGMidiOutPort::ReadNextMessage(uchar*,uint)         .text+0xf84a0  191B
 *   CSTGMidiOutPortSerial::Activate(CSTGMidiQueue*)       .text+0xf8080   83B
 *   CSTGMidiOutPortSerial::ProcessRegularMessage()        .text+0xf80e0  378B
 *   CSTGMidiOutPortSerial::RefillMsgBuffer()              .text+0xf8570  324B
 *   CSTGMidiOutPortSerial::BumpTimers()                   .text+0x5a68b0  27B (comdat)
 *   CSTGMidiOutPortSerial::CanSendRealTime()const         .text+0x5a6890  15B (comdat)
 *   CSTGMidiOutPortSerial::CanSendRegular()const          .text+0x5a68a0  15B (comdat)
 *   CSTGMidiOutPortSerial::SendRealTime(uchar)            .text+0x5a68d0  18B (comdat)
 *   CSTGMidiOutPortSerial::SendSingleByte(uchar)          .text+0x5a68f0  18B (comdat)
 *   CSTGMidiQueueReader::Read(uchar*,uint)                .text+0x40100  152B
 *   CSTGMidiQueueMessageReader::ReadMessage(uchar*,uint)  .text+0x403d0  377B
 *   CSTGMidiQueueMessageReader::ReadSysEx(uchar*,uint,uint) .text+0x40250 ~380B
 * (comdat = own `.text._ZN...` linkonce section rather than the main
 * merged .text; still genuinely defined inside OA.ko itself, confirmed
 * via `readelf -sW` Ndx being a real section, not UND).
 *
 * KEY STRUCTURAL DISCOVERY this batch (see oa_engine_init.h's own
 * class comments for the full writeup): `CSTGMidiOutPort`'s q1/q2/q3
 * "queue slots" (Queue* / Buf* / ReaderIdx, +0x14/+0x20/+0x2c) are laid out
 * BYTE-IDENTICAL to `CSTGMidiQueueMessageReader` and are reinterpreted
 * in place as one -- `ReadNextMessage()`/`ProcessRegularMessage()`/
 * `RefillMsgBuffer()` all call `ReadMessage()` with `this` pointing
 * directly at `outPort + 0x14 + roundRobinIdx*0xc`, no separate object.
 * This also explains what looked like unconfirmed padding
 * (+0x1d/+0x29/+0x35): it's each slot's own embedded `inSysEx` byte.
 *
 * All 5 "hardware transmit" vtable slots this class ultimately bottoms
 * out at (`CanTransmitHardware()`/`TransmitHardwareByte()`, the 2
 * TRAILING slots in CSTGMidiOutPortSerial's own vtable) are STILL
 * `__cxa_pure_virtual` in the real binary -- no further-derived class
 * providing them exists anywhere in OA.ko. This UART transmit path is
 * therefore genuinely dead/unwired in this firmware image as shipped;
 * declared but deliberately left WITHOUT a definition in this file
 * (oa_engine_init.h), matching that real gap honestly.
 *
 * IMPORTANT reconstruction-only deviation from the real class layout:
 * `ProcessNormal()`/`ProcessNKS4TestMode()`/`GenerateActiveSensing()`/
 * `ProcessRealTimeMessage()`/`ReadNextMessage()` are real MEMBERS OF THE
 * BASE CLASS in the actual binary (dispatched to the derived override
 * of `CanSendRealTime()` etc via a real vtable at runtime). They are
 * declared+defined here as `CSTGMidiOutPortSerial::` methods instead
 * (calling `CanSendRealTime()` etc as ordinary same-class calls) --
 * genuine C++ `virtual` was tried first and reverted: it inserts a
 * compiler-generated vtable pointer that does not match this project's
 * single hand-modeled `vtable` field, silently shifting every field
 * offset below +0x40 by 4 bytes (caught via a live KAT segfault
 * dereferencing the wrong bytes as the q1/q2/q3 embedded
 * `CSTGMidiQueueMessageReader`). Since `CSTGMidiOutPortSerial` is the
 * only concrete class this project models in this hierarchy at all,
 * this produces byte-identical observable behavior with zero ABI risk
 * -- see oa_engine_init.h's own class comments for the full writeup.
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_heapmanager.h"

namespace {

/* Same real .rodata tables as midi_in_port_serial.cpp's own
 * `kChannelMsgLen`/`kSystemMsgLen` (identical values, independently
 * re-derived here from CSTGMidiQueueMessageReader::ReadMessage()'s own
 * disassembly -- Ghidra mislabeled the access as
 * `CSTG01WWaveshaperTables::k01WWaveshaperTable59[bVar5+0xf1b]`, a
 * symbol-attribution artifact from an adjacent .rodata blob; the actual
 * index formula, `status - 0xf0`, matches kSystemMsgLen[] exactly).
 * Duplicated locally rather than shared, matching this project's
 * established per-TU convention for small confirmed-identical tables. */
const unsigned char kChannelMsgLen[7] = { 3, 3, 3, 3, 2, 2, 3 };
const unsigned char kSystemMsgLen[8]  = { 0, 2, 3, 2, 0, 0, 1, 1 };

/*
 * resolve_heap_handle() -- the exact region-resolution formula
 * `CSTGMidiOutPort::Activate()`'s own real disassembly uses to turn a
 * `CSTGMidiQueue::allocHandle` into a live data-buffer pointer:
 * `heap+0x18+handle*0x14` (a pointer-nullity check only, never
 * dereferenced -- always true in practice) gates whether
 * `*(heap+0x24+handle*0x14) + heap->heapBase` is returned. ALGEBRAICALLY
 * IDENTICAL to `setup_global_resources.cpp`'s own independently-
 * confirmed `local_heap_region()` (see oa_heapmanager.h's file comment
 * for the full sentinel/slot-addressing derivation) -- re-derived
 * locally here via raw offsets (rather than calling that static
 * function, which lives in a different TU and additionally routes
 * through this project's own "captured value" workaround for an
 * unrelated, already-diagnosed live-boot bug that doesn't apply to this
 * fresh reconstruction) to stay a faithful, direct expression of THIS
 * function's own real body.
 */
unsigned char *resolve_heap_handle(unsigned int handle)
{
	unsigned char *heap = (unsigned char *)CSTGHeapManager::sInstance;
	if (handle >= 100000)
		return 0;
	unsigned int entryOff = handle * 0x14;
	if (heap + 0x18 + entryOff == 0) /* always-true pointer check, preserved for fidelity */
		return 0;
	unsigned int offset = *(unsigned int *)(heap + 0x24 + entryOff);
	unsigned int heapBase = *(unsigned int *)(heap + 0x1e8498);
	return (unsigned char *)(unsigned long)(offset + heapBase);
}

unsigned char *resolve_queue_buffer(CSTGMidiQueue *queue)
{
	unsigned int handle = *(unsigned int *)queue; /* CSTGMidiQueue::allocHandle, +0x0 */
	return resolve_heap_handle(handle);
}

/* Packed-32-bit pointer round-trip -- see oa_engine_init.h's own
 * comment on CSTGMidiOutPort's field types for why. */
unsigned int ToU32(const void *p) { return (unsigned int)(unsigned long)p; }
unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

/*
 * Shared round-robin/inSysEx-gated q1/q2/q3 poll used identically by
 * CSTGMidiOutPort::ReadNextMessage() and
 * CSTGMidiOutPortSerial::ProcessRegularMessage()/RefillMsgBuffer() --
 * expressed once here rather than duplicated 3x, matching this
 * project's established ReceiveByteCore()-style shared-helper
 * convention (midi_in_port_serial.cpp) for real, confirmed-identical
 * logic (independently disassembly-cross-checked against
 * CSTGMidiOutPortSerial::ProcessRegularMessage()'s own raw `lea eax,
 * [ebx+eax*4+0x14]` slot-address computation).
 *
 * If the CURRENT round-robin slot's embedded inSysEx flag is clear:
 * advance the index (0->1->2->0) and retry ReadMessage() up to 3 times
 * (once per q1/q2/q3 slot) until one returns nonzero. If set: poll the
 * SAME (unadvanced) slot once, without advancing (mid-SysEx).
 */
unsigned int PollNextRegularMessage(CSTGMidiOutPort *port, unsigned char *buf, unsigned int bufLen)
{
	unsigned char *self = (unsigned char *)port;
	unsigned char idx = port->roundRobinIdx;

	if (self[0x1d + idx * 0xc] != 0) {
		CSTGMidiQueueMessageReader *reader =
			(CSTGMidiQueueMessageReader *)(self + 0x14 + idx * 0xc);
		return reader->ReadMessage(buf, bufLen);
	}

	for (int attempt = 0; attempt < 3; attempt++) {
		idx = port->roundRobinIdx;
		port->roundRobinIdx = (idx < 2) ? (unsigned char)(idx + 1) : (unsigned char)0;
		CSTGMidiQueueMessageReader *reader =
			(CSTGMidiQueueMessageReader *)(self + 0x14 + port->roundRobinIdx * 0xc);
		unsigned int result = reader->ReadMessage(buf, bufLen);
		if (result != 0 || attempt == 2)
			return result;
	}
	return 0; /* unreachable */
}

} /* anonymous namespace */

int CSTGMidiOutPort::sActiveSensingTransmitPeriodTicks;
int CSTGMidiOutPortSerial::sRunningStatusTimeoutTicks;

/*
 * CSTGMidiOutPort(eSTGMidiPort portType, unsigned int flagsInit) --
 * CONFIRMED real, `.text+0xf8270`, 95 bytes, regparm(3): this=EAX,
 * portType=EDX, flagsInit=ECX. Sets the base vtable, `portIndex =
 * (unsigned char)portType`, `flags` bit0 from `flagsInit & 1`, zeroes
 * all 4 queue-slot Queue* / Buf* pointers (NOT the reader-index/inSysEx
 * bytes -- left uninitialized until Activate()), then calls the
 * already-real CSTGMidiPortManager::RegisterMidiOutPort(this).
 */
CSTGMidiOutPort::CSTGMidiOutPort(int portType, unsigned int flagsInit)
{
	portIndex = (signed char)portType;
	flags = (unsigned char)((flags & 0xfe) | (flagsInit & 1));

	q0Queue = 0; q0Buf = 0;
	q1Queue = 0; q1Buf = 0;
	q2Queue = 0; q2Buf = 0;
	q3Queue = 0; q3Buf = 0;

	CSTGMidiPortManager::RegisterMidiOutPort(this);
}

/*
 * Activate(CSTGMidiQueue *q3arg) -- CONFIRMED real, `.text+0xf7f10`,
 * 365 bytes. Takes exactly ONE argument (the per-port "bulk-dump"
 * queue) and unconditionally wires up ALL 4 slots every call: q0/q1/q2
 * always resolve to the 3 SAME CSTGMidiPortManager-embedded
 * CSTGMidiQueue objects (+0xd4/+0x0c/+0x70 respectively, confirmed via
 * `CSTGMidiQueue::Initialize()`'s own header comment listing exactly
 * these 5 embed offsets); q3 is the caller-supplied argument. Confirmed
 * relocations: CSTGMidiPortManager::sInstance (read once, reused),
 * CSTGHeapManager::sInstance (read 4x, once per slot's buffer
 * resolution), CSTGMidiQueue::AllocReader() (called 4x, once per slot).
 */
void CSTGMidiOutPort::Activate(CSTGMidiQueue *q3arg)
{
	unsigned char *mgr = (unsigned char *)CSTGMidiPortManager::sInstance;

	if (sActiveSensingTransmitPeriodTicks == 0) {
		float scale = CSTGAudioBusManager::sInstance->busGainScale;
		sActiveSensingTransmitPeriodTicks = (int)(0.25f * scale);
	}

	CSTGMidiQueue *q0 = (CSTGMidiQueue *)(mgr + 0xd4);
	q0Queue = ToU32(q0);
	q0Buf = ToU32(resolve_queue_buffer(q0));
	q0ReaderIdx = q0->AllocReader();

	CSTGMidiQueue *q1 = (CSTGMidiQueue *)(mgr + 0x0c);
	q1Queue = ToU32(q1);
	q1Buf = ToU32(resolve_queue_buffer(q1));
	q1ReaderIdx = q1->AllocReader();
	q1InSysEx = 0;

	CSTGMidiQueue *q2 = (CSTGMidiQueue *)(mgr + 0x70);
	q2Queue = ToU32(q2);
	q2Buf = ToU32(resolve_queue_buffer(q2));
	q2ReaderIdx = q2->AllocReader();
	q2InSysEx = 0;

	q3Queue = ToU32(q3arg);
	q3Buf = ToU32(resolve_queue_buffer(q3arg));
	q3ReaderIdx = q3arg->AllocReader();
	q3InSysEx = 0;

	roundRobinIdx = 0;
	activeSensingTimer = 0;
	flags |= 2; /* active/live */
}

/* Deactivate() -- CONFIRMED real, `.text+0xf7d70`, 5 bytes: clears
 * `flags` bit1. */
void CSTGMidiOutPort::Deactivate()
{
	flags &= 0xfd;
}

/* BumpTimers() -- CONFIRMED real, non-pure base body (comdat,
 * `.text+0x5a6880`, 14 bytes): decrements `activeSensingTimer` by 1
 * while nonzero. */
void CSTGMidiOutPort::BumpTimers()
{
	if (activeSensingTimer != 0)
		activeSensingTimer -= 1;
}

/*
 * ProcessNormal() -- CONFIRMED real, `.text+0xf8330`, 145 bytes (raw
 * `objdump -dr` fully cross-checked, including the byte-fetch the
 * initial Ghidra decompile dropped). If CanSendRealTime() is true AND
 * q0's ring has a pending byte: fetch+consume it, call
 * SendRealTime(byte), reload activeSensingTimer, return true. Else if
 * CanSendRegular() is true and ProcessRegularMessage() sent something:
 * same timer reload, return true. Else return CanSendRegular()'s own
 * raw byte result (matches the real disassembly's literal, not
 * necessarily zero-extended-above-bit-0, return value).
 */
int CSTGMidiOutPortSerial::ProcessNormal()
{
	if (CanSendRealTime()) {
		unsigned int slot = q0ReaderIdx + 4u;
		unsigned char *ringCtl = FromU32(q0Queue);
		unsigned int *cursorSlot = (unsigned int *)(ringCtl + slot * 4);
		unsigned int writeCursor = *(unsigned int *)(ringCtl + 0xc);

		if (*cursorSlot != writeCursor) {
			unsigned int mask = *(unsigned int *)(ringCtl + 8);
			unsigned char b = FromU32(q0Buf)[*cursorSlot & mask];
			*cursorSlot = *cursorSlot + 1;
			SendRealTime(b);
			activeSensingTimer = sActiveSensingTransmitPeriodTicks;
			return 1;
		}
	}

	if (CanSendRegular()) {
		if (ProcessRegularMessage()) {
			activeSensingTimer = sActiveSensingTransmitPeriodTicks;
			return 1;
		}
		return 0;
	}
	return 0;
}

/*
 * ProcessNKS4TestMode() -- CONFIRMED real, `.text+0xf82d0`, 93 bytes.
 * `while (CanSendRegular())` drains q3's ring one byte at a time via
 * SendSingleByte(byte), until either q3 is empty or CanSendRegular()
 * goes false.
 */
void CSTGMidiOutPortSerial::ProcessNKS4TestMode()
{
	while (CanSendRegular()) {
		unsigned int slot = q3ReaderIdx + 4u;
		unsigned char *ringCtl = FromU32(q3Queue);
		unsigned int *cursorSlot = (unsigned int *)(ringCtl + slot * 4);
		unsigned int writeCursor = *(unsigned int *)(ringCtl + 0xc);

		if (*cursorSlot == writeCursor)
			break;

		unsigned int mask = *(unsigned int *)(ringCtl + 8);
		unsigned char b = FromU32(q3Buf)[*cursorSlot & mask];
		*cursorSlot = *cursorSlot + 1;
		SendSingleByte(b);
	}
}

/*
 * GenerateActiveSensing() -- CONFIRMED real, `.text+0xf83d0`, 78 bytes.
 * If flags bit0 is set AND activeSensingTimer==0 AND CanSendRealTime():
 * sends the literal MIDI Active Sensing byte (0xFE) via
 * SendRealTime(0xFE) and reloads activeSensingTimer.
 */
void CSTGMidiOutPortSerial::GenerateActiveSensing()
{
	if ((flags & 1) == 0)
		return;
	if (activeSensingTimer != 0)
		return;
	if (!CanSendRealTime())
		return;

	SendRealTime(0xfe);
	activeSensingTimer = sActiveSensingTransmitPeriodTicks;
}

/*
 * ProcessRealTimeMessage() -- CONFIRMED real, `.text+0xf8430`, 97
 * bytes, returns bool. If q0's ring has a pending byte: fetch it,
 * advance q0's reader cursor, call SendRealTime(byte), return true;
 * else false. (ProcessNormal()'s own q0-draining block duplicates this
 * exact sequence inline rather than calling this method -- a real,
 * confirmed code duplication in the original binary.)
 */
bool CSTGMidiOutPortSerial::ProcessRealTimeMessage()
{
	unsigned int slot = q0ReaderIdx + 4u;
	unsigned char *ringCtl = FromU32(q0Queue);
	unsigned int *cursorSlot = (unsigned int *)(ringCtl + slot * 4);
	unsigned int writeCursor = *(unsigned int *)(ringCtl + 0xc);

	if (*cursorSlot == writeCursor)
		return false;

	unsigned int mask = *(unsigned int *)(ringCtl + 8);
	unsigned char b = FromU32(q0Buf)[*cursorSlot & mask];
	*cursorSlot = *cursorSlot + 1;
	SendRealTime(b);
	return true;
}

/*
 * ReadNextMessage(unsigned char*, unsigned int) -- CONFIRMED real,
 * `.text+0xf84a0`, 191 bytes. Thin wrapper around the shared
 * round-robin poll (see PollNextRegularMessage() above). CORRECTED
 * (candidate-3/KorgUsb batch): genuinely a member of the BASE class
 * `CSTGMidiOutPort` (matching its real mangled symbol
 * `_ZN15CSTGMidiOutPort15ReadNextMessageEPhj`), and its return value is
 * NOT discarded by the real disassembly -- see the class declaration's
 * own comment (oa_engine_init.h) for the full derivation and why the
 * prior "void, Serial-hosted, discarded" model was locally correct for
 * every caller that existed at the time but wrong in general.
 */
unsigned int CSTGMidiOutPort::ReadNextMessage(unsigned char *buf, unsigned int bufLen)
{
	return PollNextRegularMessage(this, buf, bufLen);
}

/*
 * CSTGMidiOutPortSerial::Activate(CSTGMidiQueue*) override -- CONFIRMED
 * real, `.text+0xf8080`, 83 bytes: calls the base Activate() first,
 * lazily initializes sRunningStatusTimeoutTicks (0.05f *
 * CSTGAudioBusManager::sInstance->busGainScale = 75 ticks @ 1500Hz),
 * then zeroes msgLen/state/lastStatus/runningStatusTimer.
 */
void CSTGMidiOutPortSerial::Activate(CSTGMidiQueue *q3arg)
{
	CSTGMidiOutPort::Activate(q3arg);

	if (sRunningStatusTimeoutTicks == 0) {
		float scale = CSTGAudioBusManager::sInstance->busGainScale;
		sRunningStatusTimeoutTicks = (int)(0.05f * scale);
	}

	msgLen = 0;
	state = 0;
	lastStatus = 0;
	runningStatusTimer = 0;
}

/*
 * ProcessRegularMessage() override -- CONFIRMED real, `.text+0xf80e0`,
 * 378 bytes, full `objdump -dr` transcription (the initial Ghidra
 * decompile dropped the transmitted-byte register in both branches;
 * confirmed via raw disassembly that BOTH the "pull a new message" and
 * "continue a multi-byte message" paths send exactly `msgBuf[newState-1]`
 * -- i.e. one byte of the buffered 1-3 byte message per call, tracked
 * via `state` as a byte cursor into `msgBuf`).
 *
 * If `msgLen == state` (fully sent the previous message, or none
 * buffered yet): poll a new message via the shared round-robin helper
 * into `msgBuf` (bufLen fixed at 3, confirmed real). If none available,
 * clear msgLen/state and return false. Otherwise: msgLen/state updated,
 * and if `msgBuf[0]` is a channel voice/mode status byte (0x80-0xEF)
 * matching `lastStatus` with `runningStatusTimer` still running,
 * running-status compression applies -- the status byte is NOT
 * (re)transmitted; instead `runningStatusTimer` is reloaded and, unless
 * `msgLen==1` (in which case the call ends here with no byte sent),
 * `state` jumps straight to 2 and `msgBuf[1]` is sent first. Otherwise
 * `lastStatus`/`runningStatusTimer` are updated normally (or cleared,
 * for non-channel status bytes) and `msgBuf[0]` (the status byte) is
 * sent, `state` becoming 1.
 *
 * If `msgLen != state` (still mid-message from a previous call):
 * `msgBuf[state]` is sent, `state` incremented by 1 -- no new message
 * pulled, no running-status bookkeeping touched.
 */
bool CSTGMidiOutPortSerial::ProcessRegularMessage()
{
	if (msgLen == state) {
		unsigned int result = PollNextRegularMessage(this, msgBuf, 3);
		msgLen = (unsigned char)result;
		state = 0;
		if (msgLen == 0)
			return false;

		unsigned char statusByte = msgBuf[0];
		if ((unsigned char)(statusByte + 0x80u) < 0x70) {
			/* channel voice/mode status byte (0x80..0xEF) */
			if (statusByte == lastStatus && runningStatusTimer != 0) {
				runningStatusTimer = sRunningStatusTimeoutTicks;
				if (msgLen == 1)
					return false;
				state = 2;
				TransmitHardwareByte(msgBuf[1]);
				return true;
			}
			lastStatus = statusByte;
			runningStatusTimer = sRunningStatusTimeoutTicks;
		} else {
			lastStatus = 0;
			runningStatusTimer = 0;
		}
		state = 1;
		TransmitHardwareByte(msgBuf[0]);
		return true;
	}

	unsigned char oldState = state;
	state = (unsigned char)(oldState + 1);
	TransmitHardwareByte(msgBuf[oldState]);
	return true;
}

/*
 * RefillMsgBuffer() -- CONFIRMED real, `.text+0xf8570`, 324 bytes. The
 * "prepare next message without sending" half of the same
 * running-status logic: pulls the next message via the same
 * round-robin ReadMessage() polling into `msgBuf` (bufLen=3), but only
 * updates lastStatus/runningStatusTimer/state bookkeeping -- never
 * calls TransmitHardwareByte() itself. `state` is left at 0 (send the
 * full message including its status byte next time) or 1 (running
 * status applies -- ProcessRegularMessage()'s own `msgLen!=state`
 * "continue" path will then send `msgBuf[1]` first, skipping the
 * status byte).
 */
void CSTGMidiOutPortSerial::RefillMsgBuffer()
{
	unsigned int result = PollNextRegularMessage(this, msgBuf, 3);
	msgLen = (unsigned char)result;
	state = 0;
	if (msgLen == 0)
		return;

	unsigned char statusByte = msgBuf[0];
	if ((unsigned char)(statusByte + 0x80u) >= 0x70) {
		/* not a channel voice/mode status byte */
		lastStatus = 0;
		runningStatusTimer = 0;
		return;
	}
	if (statusByte != lastStatus || runningStatusTimer == 0) {
		lastStatus = statusByte;
		runningStatusTimer = sRunningStatusTimeoutTicks;
		return;
	}
	state = 1;
	runningStatusTimer = sRunningStatusTimeoutTicks;
}

/* BumpTimers() override -- CONFIRMED real (comdat, `.text+0x5a68b0`,
 * 27 bytes): decrements BOTH the base's activeSensingTimer (+0x3c) AND
 * this class's own runningStatusTimer (+0x48). A separately-compiled
 * body, not a call-through to the base's own BumpTimers(). */
void CSTGMidiOutPortSerial::BumpTimers()
{
	if (activeSensingTimer != 0)
		activeSensingTimer -= 1;
	if (runningStatusTimer != 0)
		runningStatusTimer -= 1;
}

/* CanSendRealTime()/CanSendRegular() -- CONFIRMED real (comdat,
 * `.text+0x5a6890`/`0x5a68a0`, 15 bytes each, BYTE-IDENTICAL bodies):
 * both simply forward to the still-pure CanTransmitHardware() slot --
 * this class does not distinguish realtime vs. regular transmit
 * readiness at the hardware level. */
bool CSTGMidiOutPortSerial::CanSendRealTime() const
{
	return CanTransmitHardware();
}

bool CSTGMidiOutPortSerial::CanSendRegular() const
{
	return CanTransmitHardware();
}

/* SendRealTime()/SendSingleByte() -- CONFIRMED real (comdat,
 * `.text+0x5a68d0`/`0x5a68f0`, 18 bytes each, byte-identical bodies):
 * both simply forward to the still-pure TransmitHardwareByte() slot. */
void CSTGMidiOutPortSerial::SendRealTime(unsigned char byte)
{
	TransmitHardwareByte(byte);
}

void CSTGMidiOutPortSerial::SendSingleByte(unsigned char byte)
{
	TransmitHardwareByte(byte);
}

/*
 * CSTGMidiQueueReader::Read(unsigned char *dest, unsigned int count) --
 * CONFIRMED real, `.text+0x40100`, 152 bytes, regparm(3): this=EAX,
 * dest=EDX, count=ECX. Circular-buffer copy from the reader's own
 * cursor position, wrapping at `mask+1`, advancing the cursor by
 * `count` unconditionally (no clamping to available backlog -- callers
 * are expected to only request what's actually available, matching
 * every real call site in this file). Real disassembly uses an
 * unrolled word/halfword/byte copy loop; expressed here as a plain
 * byte loop instead (same observable memory effect), matching this
 * project's established no-memcpy, no-unrolling convention (see
 * midi_queue_writer.cpp's own Write() for the identical precedent on
 * the write side).
 */
void CSTGMidiQueueReader::Read(unsigned char *dest, unsigned int count)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *ringCtl = FromU32(*(unsigned int *)self);
	unsigned char *bufBase = FromU32(*(unsigned int *)(self + 4));
	unsigned int slot = self[8] + 4u;

	unsigned int mask = *(unsigned int *)(ringCtl + 8);
	unsigned int *cursorSlot = (unsigned int *)(ringCtl + slot * 4);
	unsigned int pos = *cursorSlot & mask;

	unsigned int untilWrap = (mask + 1) - pos;
	unsigned int firstChunk = (count < untilWrap) ? count : untilWrap;
	unsigned int secondChunk = count - firstChunk;

	for (unsigned int i = 0; i < firstChunk; i++)
		dest[i] = bufBase[pos + i];
	for (unsigned int i = 0; i < secondChunk; i++)
		dest[firstChunk + i] = bufBase[i];

	*cursorSlot = *cursorSlot + count;
}

/*
 * CSTGMidiQueueMessageReader::ReadMessage(unsigned char *buf, unsigned
 * int bufLen) -- CONFIRMED real, `.text+0x403d0`, 377 bytes. If the
 * ring is empty, returns 0. If `inSysEx` is set, forwards straight to
 * ReadSysEx(). Otherwise peeks the next ring byte: if it's a stray data
 * byte or a bare 0xF7 (EOX with no preceding SysEx-start seen by this
 * reader), resyncs forward until a real status byte is found (or the
 * ring runs dry, in which case the skipped bytes are consumed with no
 * message returned). Once a status byte is found: 0xF0 dispatches to
 * ReadSysEx(); otherwise the expected length comes from
 * kChannelMsgLen[]/kSystemMsgLen[] unless the queue's own `format`
 * field (ringCtl+0x4) is 1, in which case the length is always 5 --
 * then copies that many bytes out via CSTGMidiQueueReader::Read().
 */
unsigned int CSTGMidiQueueMessageReader::ReadMessage(unsigned char *buf, unsigned int bufLen)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *ringCtl = FromU32(*(unsigned int *)self);
	unsigned char *bufBase = FromU32(*(unsigned int *)(self + 4));
	unsigned int slot = self[8] + 4u;
	unsigned int *cursorSlot = (unsigned int *)(ringCtl + slot * 4);
	unsigned int mask = *(unsigned int *)(ringCtl + 8);

	if (*(unsigned int *)(ringCtl + 0xc) == *cursorSlot)
		return 0;

	if (self[9] != 0) /* inSysEx */
		return ReadSysEx(buf, 0, bufLen);

	unsigned char status = bufBase[*cursorSlot & mask];

	if ((signed char)status >= 0 || status == 0xf7) {
		/* stray data byte or bare EOX -- resync forward to the next
		 * real status byte, discarding skipped bytes. */
		unsigned int avail = *(unsigned int *)(ringCtl + 0xc) - *cursorSlot;
		unsigned int skip;

		if (avail < 2) {
			*cursorSlot = *cursorSlot + 1;
			return 0;
		}

		unsigned char next = bufBase[(*cursorSlot + 1) & mask];
		if ((signed char)next < 0 && next != 0xf7) {
			skip = 1;
			status = next;
		} else {
			skip = 1;
			for (;;) {
				skip++;
				if (avail <= skip) {
					*cursorSlot = *cursorSlot + skip;
					return 0;
				}
				status = bufBase[(*cursorSlot + skip) & mask];
				if ((signed char)status < 0 && status != 0xf7)
					break;
			}
		}
		*cursorSlot = *cursorSlot + skip;
		if (status == 0)
			return 0;
	}

	if (status == 0xf0)
		return ReadSysEx(buf, 0, bufLen);

	unsigned int msgLen = 5;
	if (*(int *)(ringCtl + 4) != 1) {
		msgLen = 1;
		if (status < 0xf8) {
			if (status < 0xf0)
				msgLen = kChannelMsgLen[(status >> 4) & 7];
			else
				msgLen = kSystemMsgLen[status - 0xf0];
		}
	}

	CSTGMidiQueueReader *reader = (CSTGMidiQueueReader *)self;
	reader->Read(buf, msgLen);
	return msgLen;
}

/*
 * CSTGMidiQueueMessageReader::ReadSysEx(unsigned char *buf, unsigned
 * int startPos, unsigned int bufLen) -- CONFIRMED real, `.text+0x40250`,
 * full `objdump -dr` instruction-level transcription (the initial
 * Ghidra decompile badly mis-attributed registers here -- this body
 * was built directly from the raw disassembly, not the decompile). The
 * REAL parameter order is (buf, startPos, bufLen) -- NOT (buf, bufLen,
 * alreadyWritten) as the mangled name's parameter ORDER might suggest;
 * confirmed via the calling convention (this=EAX, buf=EDX, startPos=
 * ECX -- used directly as the scan cursor, ECX being the 3rd regparm
 * register -- bufLen=stack, the 4th argument). Both real callers
 * (ReadMessage(), above) always pass startPos=0 -- this class has no
 * field to remember a resume position across calls, so `startPos`'s
 * real-world use in THIS binary is degenerate, though the parameter
 * genuinely exists in the ABI.
 *
 * Scans forward (from the ring's current, unmoved reader cursor) for a
 * status-shaped byte (bit7 set): if `format==1`, only 0xF7 counts as a
 * terminator; otherwise ANY status-shaped byte terminates the scan
 * (0xF7 = real EOX, anything else = SysEx aborted by a new status byte
 * -- both handled identically below). The scan is capped at
 * `min(availableRingBacklog, bufLen)`; if the cap is reached with no
 * terminator found:
 *   - if `bufLen > availableRingBacklog` (genuinely not enough ring
 *     data yet, not just a bufLen cap): returns 0, no copy, no state
 *     change.
 *   - otherwise: copies the full `bufLen` bytes anyway via
 *     CSTGMidiQueueReader::Read() and returns `bufLen`, WITHOUT setting
 *     `inSysEx` -- a real, confirmed quirk (not a translation error):
 *     if the caller's buffer is smaller than the actual pending SysEx
 *     run, SysEx continuity tracking is silently lost after exactly
 *     one such call (the next ReadMessage() call will treat whatever
 *     follows as a fresh resync scan, not a SysEx continuation).
 *     Reproduced faithfully.
 * If a terminator WAS found: the span up to and including it is copied
 * via Read(); if the terminator itself wasn't a genuine trailing 0xF7
 * already present in the copied bytes, one is appended. `inSysEx` is
 * then cleared (0=complete). If NO terminator was found but there WAS
 * ring backlog to copy (the two bullet points above), `inSysEx` is set
 * to 1 (continues on the next call) only in the "not enough ring data
 * yet" short-circuit path -- see the code below for the exact,
 * disassembly-confirmed shape of this asymmetry.
 */
unsigned int CSTGMidiQueueMessageReader::ReadSysEx(unsigned char *buf, unsigned int startPos, unsigned int bufLen)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *ringCtl = FromU32(*(unsigned int *)self);
	unsigned int slot = self[8] + 4u;
	unsigned int writeCursor = *(unsigned int *)(ringCtl + 0xc);
	unsigned int readerCursor0 = *(unsigned int *)(ringCtl + slot * 4);
	unsigned int avail = writeCursor - readerCursor0;
	unsigned int capped = (bufLen <= avail) ? bufLen : avail;

	unsigned char *bufBase = FromU32(*(unsigned int *)(self + 4));
	unsigned int mask = *(unsigned int *)(ringCtl + 8);

	unsigned int pos = startPos;
	unsigned int scanEnd;
	bool foundTerminator;

	if (*(int *)(ringCtl + 4) == 1) {
		/* format==1: only a literal 0xF7 counts as a terminator (no
		 * "aborted by a different status byte" concept for this
		 * format). */
		if (pos >= capped)
			goto not_found;
		for (;;) {
			unsigned int cursor = *(unsigned int *)(ringCtl + slot * 4);
			if (bufBase[(pos + cursor) & mask] == 0xf7) {
				scanEnd = pos + 1; /* includes the EOX byte itself */
				foundTerminator = true;
				goto transfer;
			}
			pos = pos + 1;
			if (!(capped > pos))
				goto not_found;
		}
	}

	/* format != 1: ANY status-shaped byte terminates the scan -- 0xF7
	 * is real EOX (included in the copied span, scanEnd=pos+1);
	 * anything else means SysEx was aborted by a new status byte
	 * (NOT included in the copied span, scanEnd=pos, possibly 0 if the
	 * very first scanned byte was already an aborting status byte --
	 * CONFIRMED via raw disassembly: this case skips straight to
	 * abort_check without calling Read() at all when scanEnd==0). */
	if (pos >= capped)
		goto not_found;
	for (;;) {
		unsigned int cursor = *(unsigned int *)(ringCtl + slot * 4);
		unsigned char b = bufBase[(pos + cursor) & mask];
		if ((signed char)b >= 0) {
			pos = pos + 1;
			if (capped <= pos)
				goto not_found;
			continue;
		}
		if (b == 0xf7) {
			scanEnd = pos + 1;
			foundTerminator = true;
			goto transfer;
		}
		scanEnd = pos;
		foundTerminator = true;
		goto abort_check;
	}

not_found:
	if (bufLen > avail)
		return 0; /* genuinely not enough ring data yet -- no copy, no state change */
	scanEnd = bufLen;
	foundTerminator = false;
	goto abort_check;

abort_check:
	if (scanEnd != 0)
		goto transfer;
	if (!foundTerminator)
		return 0; /* ring had nothing scannable at all */
	/* Aborted immediately at position 0: nothing to copy via Read(),
	 * but still synthesize a trailing EOX and close out the transfer
	 * (CONFIRMED: the real disassembly's append_check block always
	 * takes its "scanEnd==0" branch here, unconditionally writing
	 * buf[0]=0xF7). */
	buf[0] = 0xf7;
	self[9] = 0; /* inSysEx = 0 (complete) */
	return 1;

transfer:
	{
		CSTGMidiQueueReader *reader = (CSTGMidiQueueReader *)self;
		reader->Read(buf, scanEnd);
	}
	if (!foundTerminator)
		return scanEnd; /* not_found-with-enough-backlog quirk: no inSysEx update, see header comment */

	/* Dedupe: the EOX case already copied the terminator as the last
	 * byte of the span; only the "aborted by a different status byte"
	 * case needs a synthetic append. */
	if (buf[scanEnd - 1] != 0xf7)
		buf[scanEnd++] = 0xf7;
	self[9] = 0; /* inSysEx = 0 (complete) */
	return scanEnd;
}
