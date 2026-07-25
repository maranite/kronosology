// SPDX-License-Identifier: GPL-2.0
/*
 * midi_in_port_serial.cpp  -  CSTGMidiInPortSerial::ReceiveByte()/
 * ReceiveBytes()/CheckForCompleteMessage() -- the physical (5-pin DIN)
 * hardware MIDI-IN UART byte parser.
 *
 * This is a genuinely different subsystem from CSTGMidiInPortGeneric::
 * Receive() (midi_in_port.cpp): Generic's Receive(data,len) takes an
 * ALREADY-FRAMED complete MIDI event per call (one call = one message);
 * Serial's ReceiveByte()/ReceiveBytes() take RAW, unframed bytes straight
 * off a UART interrupt/DMA and must reconstruct message boundaries
 * themselves (running status, partial accumulation, SysEx start/end
 * detection) one byte at a time. Deliberately kept a SEPARATE TU from
 * midi_in_port.cpp per this project's established per-symbol-cluster
 * convention, even though the two files share several small primitives
 * (ReadTSC/WriterAt/the realtime-timestamp-ring math) that are
 * re-derived locally here rather than imported, matching e.g.
 * oa_keybed_interface_batch64's own "re-derive the tiny primitive
 * locally" precedent.
 *
 * Ground truth (OA.ko_Decomp/OA.ko, all three confirmed regparm(3)):
 *   _ZN20CSTGMidiInPortSerial23CheckForCompleteMessageEv
 *                                                  .text+0xf6ac0  197 bytes
 *   _ZN20CSTGMidiInPortSerial11ReceiveByteEh       .text+0xf6b90 1140 bytes
 *   _ZN20CSTGMidiInPortSerial12ReceiveBytesEPKhh   .text+0xf7010 1261 bytes
 * Both ReceiveByte/ReceiveBytes were disassembled independently in full
 * (objdump -dr -M intel) -- ReceiveBytes is confirmed to be its own
 * separately-compiled per-byte loop sharing the identical per-byte
 * dispatch structure, not a call-through wrapper around ReceiveByte.
 * This file expresses that shared structure once, as the static
 * `ReceiveByteCore()` helper below, called from both real methods --
 * this changes nothing observable (same memory writes/Write() calls in
 * the same order) versus the real binary's independent inlined copies.
 *
 * New real-hardware discovery this batch: a per-object realtime-message
 * TIMESTAMP RING, confirmed via the 0xF8/0xFA/0xFB/0xFC (MIDI
 * Clock/Start/Continue/Stop) dispatch path in both ReceiveByte and
 * ReceiveBytes (byte-identical address arithmetic in both):
 *   idx       = *(u32*)(this+0x1ac) & 7          -- monotonic counter, mod 8
 *   slotBase  = this + 0x140 + idx*0xc
 *   [slotBase+0xc]        = the realtime status byte (1 byte)
 *   [slotBase+0x10..0x13] = rdtsc low 32 bits
 *   [slotBase+0x14..0x17] = rdtsc high 32 bits
 *   *(u32*)(this+0x1ac)  += 1
 * i.e. each logical ring entry's OWN 12-byte storage actually starts at
 * (this+0x140 + idx*0xc + 0xc) = (this+0x14c + idx*0xc); the leading
 * 0xc-byte offset in the index arithmetic is a real, confirmed-not-a-
 * transcription-error quirk (also indepedently confirmed against the
 * exact same address math in CSTGMidiInPortGeneric::Receive()'s own
 * PushRealtimeByte(), midi_in_port.cpp -- both port types operate on
 * the SAME CSTGMidiInPort object layout, confirming this ring is a
 * base-class field, not something Serial/Generic each have their own
 * copy of). This carves real names out of oa_engine.h's previous
 * `_unrecovered108[0x1d8]` blob for +0x14c..+0x1ac (8 x 12-byte
 * entries) and +0x1ac (the index) -- done via raw offsets here rather
 * than a header field-list edit, matching midi_in_port.cpp's own
 * existing convention for the identical region.
 *
 * `USTGMidiUtils::kChannelMsgLen[7]`/`kSystemMsgLen[8]` (real symbols,
 * `.rodata` @ VA 0x47684/0x4768b, section-relative in this unlinked
 * .ko) are standard MIDI message-length tables, dumped directly from
 * the binary:
 *   kChannelMsgLen = {3,3,3,3,2,2,3}   -- indexed by (status>>4)&7:
 *       0x8 NoteOff=3, 0x9 NoteOn=3, 0xA PolyAT=3, 0xB CC=3,
 *       0xC ProgChange=2, 0xD ChanAT=2, 0xE PitchBend=3
 *   kSystemMsgLen  = {0,2,3,2,0,0,1,1} -- indexed by (status-0xF0):
 *       F0 SysEx(n/a)=0, F1 MTCQtrFrame=2, F2 SongPos=3, F3 SongSelect=2,
 *       F4/F5 undefined=0, F6 TuneRequest=1, F7 EOX=1
 * Kept as file-local `static const` arrays (not exported under their
 * real mangled names) since nothing else in this project references
 * them yet.
 *
 * `StartSysEx()`/`ReceiveSysExData(unsigned char)` (called for the 0xF0
 * SysEx-start byte and for in-SysEx data bytes respectively) are
 * confirmed real CSTGMidiInPort base-class methods but are NOT
 * reconstructed by this batch -- deliberately deferred no-op stubs in
 * bar2_stubs.cpp (own substantial state machine, disproportionate to
 * this batch's UART-parser scope; see oa_engine.h's class comment).
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"	/* CSTGMidiQueueWriter::Write(), CSTGMidiQueue::GetNumWritableBytes() */

namespace {

/* Real .rodata tables -- see file header comment for the dump. */
const unsigned char kChannelMsgLen[7] = { 3, 3, 3, 3, 2, 2, 3 };
const unsigned char kSystemMsgLen[8]  = { 0, 2, 3, 2, 0, 0, 1, 1 };

enum {
	W_PRIMARY  = 0xf0,
	W_REALTIME = 0xf8,
	W_KG       = 0x100,
};

inline CSTGMidiQueueWriter *WriterAt(unsigned char *self, unsigned int off)
{
	return (CSTGMidiQueueWriter *)(self + off);
}

inline bool MessageProcessorNotBusy()
{
	return ((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] == 0;
}

inline unsigned long long ReadTSC(void)
{
	unsigned int lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)hi << 32) | lo;
}

/* Realtime-message timestamp ring push -- see file header comment. */
void PushRealtimeByte(unsigned char *self, unsigned char b)
{
	unsigned int idx = *(unsigned int *)(self + 0x1ac) & 7;
	unsigned char *slotBase = self + 0x140 + idx * 0xc;
	unsigned long long tsc = ReadTSC();

	*(unsigned int *)(slotBase + 0x10) = (unsigned int)tsc;
	*(unsigned int *)(slotBase + 0x14) = (unsigned int)(tsc >> 32);
	*(unsigned char *)(slotBase + 0xc) = b;
	*(unsigned int *)(self + 0x1ac) += 1;

	if (!MessageProcessorNotBusy())
		return;

	CSTGMidiQueue *q = (CSTGMidiQueue *)(*(unsigned int *)(self + 0xf8));
	if (q->GetNumWritableBytes() == 0)
		return;

	unsigned char *ctl = (unsigned char *)q;
	unsigned char *bufBase = (unsigned char *)(*(unsigned int *)(self + 0xfc));
	unsigned int wrapped = (*(unsigned int *)(ctl + 0xc)) & (*(unsigned int *)(ctl + 0x8));

	bufBase[wrapped] = b;
	*(unsigned int *)(ctl + 0xc) += 1;
}

/* 0xF8-0xFF dispatch: F8/FA/FB/FC -> timestamp ring; F9/FD/FF -> ignored;
 * FE -> activeSensingSeen flag only (no ring entry). */
void HandleRealtimeStatus(unsigned char *self, unsigned char b)
{
	if (b > 0xfc) {
		if (b == 0xfe)
			self[0x2e3] = 1;	/* activeSensingSeen */
		return;
	}
	if (b < 0xfa && b != 0xf8)
		return;				/* 0xF9: undefined, ignored */
	PushRealtimeByte(self, b);
}

/*
 * CheckForCompleteMessage() core -- table-driven completion check +
 * flush + running-status-aware reset. See file header comment for the
 * confirmed table values/flush targets.
 */
void CheckForCompleteMessageImpl(unsigned char *self)
{
	unsigned char count = self[0x24];
	if (count == 0)
		return;

	unsigned char status = self[4];
	unsigned int expected;

	if (status <= 0xef)
		expected = kChannelMsgLen[(status >> 4) & 7];
	else if (status <= 0xf7)
		expected = kSystemMsgLen[status - 0xf0];
	else
		expected = 1;	/* defensive/unreachable in real disasm, preserved */

	if (count != expected)
		return;

	if (status <= 0xef) {
		if (MessageProcessorNotBusy())
			WriterAt(self, W_PRIMARY)->Write(self + 4, count, false);
		self[0x24] = 1;		/* keep status byte -- running status */
	} else {
		if (MessageProcessorNotBusy())
			WriterAt(self, W_REALTIME)->Write(self + 4, count, false);
		self[0x24] = 0;		/* system-common: no running status */
	}
}

/* state==2 ("locally buffering") abort: append EOX, conditionally flush
 * to PRIMARY. Does NOT itself reset sysExState/count -- callers do that
 * (either via EstablishNewStatus() or the explicit bare-0xF7 reset),
 * matching the two genuinely different real post-abort tails below. */
void AbortLocalScratchFlush(unsigned char *self)
{
	unsigned char count = self[0x24];

	self[4 + count] = 0xf7;
	unsigned char newCount = (unsigned char)(count + 1);
	self[0x24] = newCount;

	if (MessageProcessorNotBusy())
		WriterAt(self, W_PRIMARY)->Write(self + 4, newCount, false);
}

/* state==3 ("streaming") abort: conditionally flush a lone 0xF7 to
 * PRIMARY (gated by streamFlagPrimary + busy-check) and/or KG (gated by
 * streamFlagKG + flags&1, NOT re-checking busy -- a real, confirmed
 * asymmetry, matches CSTGMidiInPortGeneric::FlushStreamingState()). */
void AbortStreamingFlush(unsigned char *self)
{
	if (self[0x2e1] != 0) {
		if (MessageProcessorNotBusy())
			WriterAt(self, W_PRIMARY)->Write((unsigned char)0xf7);
	}
	if (self[0x2e2] != 0 && (self[0x26] & 0x1) != 0)
		WriterAt(self, W_KG)->Write((unsigned char)0xf7);
}

/* Establish `status` as the start of a new message (running-status
 * reset) and immediately check completion -- covers single-status-byte
 * messages like 0xF6 Tune Request completing with no data bytes. */
void EstablishNewStatus(unsigned char *self, unsigned char status)
{
	self[0x2e0] = 0;
	self[4] = status;
	self[0x24] = 1;
	CheckForCompleteMessageImpl(self);
}

/*
 * Shared per-byte dispatch, used identically by ReceiveByte() and
 * ReceiveBytes()'s loop -- see file header comment for why this is
 * faithful to two independently-compiled real functions.
 */
void ReceiveByteCore(unsigned char *self, unsigned char b)
{
	if ((signed char)b < 0) {
		/* Status byte, 0x80-0xFF. */
		if (b > 0xf7) {
			HandleRealtimeStatus(self, b);
			return;
		}
		if (b == 0xf0) {
			((CSTGMidiInPort *)self)->StartSysEx();
			return;
		}
		unsigned char state = self[0x2e0];
		if (b == 0xf7) {
			/* Bare EOX with no (or an aborted) active SysEx: abort
			 * whatever's pending, then just reset -- NOT established
			 * as a new status (confirmed distinct real tail from the
			 * general new-status case below). */
			if (state == 2)
				AbortLocalScratchFlush(self);
			else if (state == 3)
				AbortStreamingFlush(self);
			self[0x2e0] = 0;
			self[0x24] = 0;
			return;
		}
		/* Genuine new channel/system-common status byte: abort any
		 * pending SysEx, then establish this byte as the new message. */
		if (state == 2)
			AbortLocalScratchFlush(self);
		else if (state == 3)
			AbortStreamingFlush(self);
		EstablishNewStatus(self, b);
		return;
	}

	/* Data byte, 0x00-0x7F. */
	if (self[0x2e0] != 0) {
		((CSTGMidiInPort *)self)->ReceiveSysExData(b);
		return;
	}
	unsigned char count = self[0x24];
	if (count == 0)
		return;			/* stray data byte, no active message: drop */

	self[4 + count] = b;
	unsigned char newCount = (unsigned char)(count + 1);
	self[0x24] = newCount;
	if (newCount == 0)
		return;			/* 8-bit wraparound edge case, preserved verbatim */

	CheckForCompleteMessageImpl(self);
}

} // namespace

void CSTGMidiInPortSerial::CheckForCompleteMessage()
{
	CheckForCompleteMessageImpl((unsigned char *)this);
}

/*
 * ReceiveByte(unsigned char) -- gates identical to
 * CSTGMidiInPortGeneric::Receive()'s own Oddity #1/#2 (see
 * midi_in_port.cpp's header comment): flags&2 (port active), the raw
 * low-byte-of-the-sInstance-slot test (NOT a null check), then the
 * KG-bulk-bypass gate on CSTGGlobal::sInstance[0x6ac].
 */
void CSTGMidiInPortSerial::ReceiveByte(unsigned char b)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char flags = self[0x26];

	if ((flags & 0x2) == 0)
		return;
	if (*(volatile unsigned char *)&CSTGMidiPortManager::sInstance == 0)
		return;

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	if (g[0x6ac] != 0) {
		if ((flags & 0x1) != 0)
			WriterAt(self, W_KG)->Write(&b, 1, false);
		return;
	}

	/* Oddity #2, same as Generic -- see midi_in_port.cpp. */
	*(unsigned int *)(self + 0x2e4) = *(unsigned int *)(g + 0x29c9fa8);

	ReceiveByteCore(self, b);
}

/*
 * ReceiveBytes(const unsigned char*, unsigned char) -- same gates as
 * ReceiveByte(), evaluated ONCE for the whole call. Confirmed real
 * asymmetry vs ReceiveByte(): in the KG-bulk-bypass branch, Oddity #2 is
 * NOT written (the real code returns before reaching that instruction);
 * it's written unconditionally on the normal path, even for len==0.
 */
void CSTGMidiInPortSerial::ReceiveBytes(const unsigned char *data, unsigned char len8)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char flags = self[0x26];

	if ((flags & 0x2) == 0)
		return;
	if (*(volatile unsigned char *)&CSTGMidiPortManager::sInstance == 0)
		return;

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;
	unsigned int len = len8;

	if (g[0x6ac] != 0) {
		if ((flags & 0x1) != 0)
			WriterAt(self, W_KG)->Write(data, len, false);
		return;
	}

	*(unsigned int *)(self + 0x2e4) = *(unsigned int *)(g + 0x29c9fa8);
	if (len == 0)
		return;

	for (unsigned int i = 0; i < len; i++)
		ReceiveByteCore(self, data[i]);
}

/*
 * CSTGMidiInPort::CSTGMidiInPort(int portType, unsigned int flagsInit)
 * -- CONFIRMED real (`_ZN14CSTGMidiInPortC2E12eSTGMidiPortj`,
 * `.text+0xf59a0`, 144 bytes, regparm(3): this=EAX, portType=EDX,
 * flagsInit=ECX), full `objdump -dr` transcription. Added THIS BATCH
 * (candidate-3/KorgUsb) as a genuine new dependency:
 * `CKorgUsbAudioDriverMidiPorts`'s own ctor (midi_korgusb_port.cpp)
 * calls this SAME real function directly to construct its 2 embedded
 * `CSTGMidiInPortKorgUsb` sub-objects -- it cannot be left an
 * undefined/deferred extern the way e.g. `ReceiveSysEx()` is, because
 * unlike those, this ctor runs unconditionally on the module's own
 * live global-constructor path, not a dead/hardware-gated branch.
 *
 * Real body, in order: `portType` stored at +0x25; `flags` (+0x26)
 * read-modify-written, replacing only bit0 with `flagsInit & 1` (bit1
 * and the other 6 bits are left whatever they already were in memory --
 * this is placement-into-existing-storage, NOT a zero-init, matching
 * `CSTGMidiOutPort`'s own base ctor's identical "only touch what I own"
 * pattern); vtable set to `&_ZTV14CSTGMidiInPort + 8`;
 * `sysExScratchLen` (+0x24) cleared; `+0x28`/`+0x8c` set to `-1`
 * (sentinel, exact meaning not independently determined -- both fall
 * inside this class's own already-documented `_unrecovered27`/
 * `_unrecovered108` gaps); the 3 embedded `CSTGMidiQueueWriter` pairs
 * (+0xf0/+0xf4, +0xf8/+0xfc, +0x100/+0x104) zeroed; `sysExState` (+0x2e0)
 * cleared; finally calls the already-real
 * `CSTGMidiPortManager::RegisterMidiInPort(this)`.
 *
 * DELIBERATELY NOT reproduced: two more real writes inside this same
 * ctor body, `*(byte*)(this+0x148) = 1` and a vtable-pointer write at
 * `this+0x108` (`&_ZTV20CSTGExtMIDIClockSync + 8`) -- the latter proves
 * `CSTGMidiInPort` embeds a THIRD clock-sync-family object (parallel to
 * `CSTGMIDIClockSync`'s own use of the already-reconstructed
 * `CSTGIntMIDIClockSync`, oa_engine_init.h) somewhere in the existing,
 * already-documented `_unrecovered108[0x1d8]` gap -- a genuinely new
 * discovery, but reconstructing a whole THIRD MIDI-clock-sync class
 * hierarchy (own vtable, own methods, its own ctor call chain) is a
 * disproportionate expansion for this batch, matching this project's
 * established "confirmed real, deliberately deferred, characterized for
 * a future session" convention. Nothing this batch's own KATs touch
 * depends on either omitted write.
 */
CSTGMidiInPort::CSTGMidiInPort(int portType, unsigned int flagsInit)
{
	unsigned char *self = (unsigned char *)this;

	self[0x25] = (unsigned char)portType;
	self[0x26] = (unsigned char)((self[0x26] & 0xfe) | (flagsInit & 1));
	*(unsigned int *)(self + 0x00) = 0; /* real: &_ZTV14CSTGMidiInPort + 8, own vtable not yet reconstructed as a real class hierarchy -- see class comment */
	self[0x24] = 0;
	*(unsigned int *)(self + 0x28) = 0xffffffffu;
	*(unsigned int *)(self + 0x8c) = 0xffffffffu;
	*(unsigned int *)(self + 0xf0) = 0;
	*(unsigned int *)(self + 0xf4) = 0;
	*(unsigned int *)(self + 0xf8) = 0;
	*(unsigned int *)(self + 0xfc) = 0;
	*(unsigned int *)(self + 0x100) = 0;
	*(unsigned int *)(self + 0x104) = 0;
	self[0x2e0] = 0;

	CSTGMidiPortManager::RegisterMidiInPort(this);
}
