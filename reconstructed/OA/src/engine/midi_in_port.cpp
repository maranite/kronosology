// SPDX-License-Identifier: GPL-2.0
/*
 * midi_in_port.cpp  -  CSTGMidiInPortGeneric::Receive() a.k.a. the project
 * brief's "MidiInPortGeneric7Receive" (a fragment of its own real mangled
 * name), plus CSTGMidiPortManager::RegisterMidiInPort().
 *
 * Ground truth for BOTH functions is the raw disassembly of OA_real.ko
 * (MD5 955636c2b11a70a1dbecefaaa7bd4f80), not the Ghidra decompile of
 * Receive() (`/home/share/Decomp/oa_export/functions/Receive@00107a10.c`,
 * Ghidra addresses = real ELF address + 0x10000): that decompile is
 * internally inconsistent about several of this function's tail calls
 * (stack-spilled `in_stack_ffffffXX` placeholders where the optimizer
 * reused registers across near-duplicate basic blocks) and, cross-checked
 * against the real disassembly below, actively mis-attributes one call as
 * `CSTGMidiInPort::ReceiveSysEx` when it is really a
 * `CSTGMidiQueueWriter::Write()` buffer flush. This file was written
 * directly from:
 *   objdump -d -M intel --start-address=0xf7a10 --stop-address=0xf7d70 OA.ko
 *   readelf -Wr OA.ko   (every relocation touching that byte range)
 *   objdump -d -M intel -j .text._ZN19CSTGMidiQueueWriter5WriteEh OA.ko
 *
 * Symbols/addresses:
 *   _ZN21CSTGMidiInPortGeneric7ReceiveEPKhj       .text+0xf7a10  862 bytes
 *   _ZN19CSTGMidiPortManager18RegisterMidiInPortEP14CSTGMidiInPort
 *                                                  .text+0xf4f40   12 bytes
 * Both confirmed regparm(3). See oa_engine.h's CSTGMidiInPort/
 * CSTGMidiInPortGeneric class comment for the full confirmed field-layout
 * writeup this translation relies on.
 *
 * Two confirmed-real oddities preserved verbatim (project convention:
 * preserve real bugs/anomalies, don't paper over them):
 *
 *  1. The gate `if (*CSTGMidiPortManager::sInstance != 0)` that the
 *     (misleading) Ghidra decompile shows is NOT a null-pointer check on
 *     the singleton. The real instruction sequence is:
 *         mov edx, &CSTGMidiPortManager::sInstance   ; ADDRESS of the slot
 *         cmp byte ptr [edx], 0                       ; its own LOW BYTE
 *     i.e. it tests the raw low byte of the pointer variable's own
 *     storage, not `*sInstance`. Per oa_engine.h's own exhaustive
 *     analysis, nothing in OA.ko ever writes `sInstance`, so this byte is
 *     always 0 in an OA.ko-alone build -- making the entire
 *     "STG-generic-processing" body of Receive() provably dead code here,
 *     UNLESS some companion module (candidate already named in
 *     oa_engine.h: STGEnabler.ko) writes to that storage at runtime.
 *
 *  2. `this->fieldAt(0x2e4) = *(unsigned int*)((char*)CSTGGlobal::
 *     sInstance + 0x29c9fa8)`. Confirmed via readelf -Wr that NO
 *     relocation touches this instruction's displacement bytes -- 0x29c9fa8
 *     (~44MB) is a literal compile-time immediate, not a link-time symbol
 *     reference. CSTGGlobal's own confirmed size is a few KB, so this is
 *     not a plausible struct member offset. Reproduced verbatim as a
 *     genuinely unresolved real anomaly (likely reads unmapped/unrelated
 *     memory on real hardware) rather than "corrected" to something more
 *     plausible.
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"	/* CSTGMidiQueue::GetNumWritableBytes() */

/*
 * CSTGMidiPortManager::RegisterMidiInPort(CSTGMidiInPort*) -- confirmed
 * real 1-line body (.text+0xf4f40, 12 bytes): stores `port` into
 * `sMidiInPorts`, indexed by the port's OWN `portType` byte (+0x25),
 * which the port's constructor has already set before calling this.
 * `sMidiInPorts` stays `void*[4]` (its existing, already-linked
 * declaration/definition in engine.cpp) rather than being retyped to
 * `CSTGMidiInPort*[4]` here -- a pure-addition, zero-collision-risk
 * choice given other concurrent work in this tree also touches
 * MIDI-out-adjacent code sharing that same array's declaration site.
 */
void CSTGMidiPortManager::RegisterMidiInPort(CSTGMidiInPort *port)
{
	sMidiInPorts[(unsigned char)port->portType] = port;
}

static CSTGMidiQueueWriter *WriterAt(CSTGMidiInPort *p, unsigned int offset)
{
	return (CSTGMidiQueueWriter *)((unsigned char *)p + offset);
}

enum {
	W_PRIMARY  = 0xf0,	/* SysEx-scratch-flush / new-status-byte tail target */
	W_REALTIME = 0xf8,	/* realtime-byte ring / "system common" tail target */
	W_KG       = 0x100,	/* KG-engine passthrough, gated by flags & 1 */
};

/* 64-bit cycle counter for the realtime-byte ring's per-entry timestamp.
 * Kept local (not oa_internal.h's `rdtsc()`) to avoid pulling in that
 * header's own placement-new operator overloads into this TU, matching
 * this project's established per-file "re-derive the tiny primitive
 * locally" convention (see midi_port_manager.cpp's own CSTGHeapManager/
 * CSTGCPUInfo stand-ins for the same reasoning). */
static inline unsigned long long ReadTSC(void)
{
	unsigned int lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)hi << 32) | lo;
}

/*
 * Inlined single-byte push into the realtime-message ring (.text+0xf7acb
 * onward) -- confirmed NOT a call to CSTGMidiQueueWriter::Write(unsigned
 * char); it is hand-inlined in Receive() itself, sharing only the
 * GetNumWritableBytes() free-space query.
 *
 * The `slotBase + 0xc/+0x10/+0x14` addressing below is reproduced
 * EXACTLY as the real code computes it: `slotBase = this + 0x140 +
 * (idx&7)*0xc`, then the byte lands at `slotBase+0xc` and the 8-byte
 * timestamp at `slotBase+0x10..0x17` -- i.e. each logical ring entry's
 * real storage is offset by a full stride (0xc bytes) from the slot base
 * the index arithmetic computes for it. This leaves the first 0xc bytes
 * of the +0x140 region (this project's own `_unrecovered108` gap
 * territory at that point) permanently unused and the last touched byte
 * at index 7 landing at +0x1ab, one byte short of the +0x1ac index
 * counter -- consistent, not a transcription slip, and preserved
 * verbatim rather than "fixed" to a cleaner off-by-zero model.
 */
static void PushRealtimeByte(CSTGMidiInPort *p, unsigned char b)
{
	unsigned char *self = (unsigned char *)p;
	unsigned int idx = *(unsigned int *)(self + 0x1ac) & 7;
	unsigned char *slotBase = self + 0x140 + idx * 0xc;
	unsigned long long tsc = ReadTSC();

	*(unsigned int *)(slotBase + 0x10) = (unsigned int)tsc;
	*(unsigned int *)(slotBase + 0x14) = (unsigned int)(tsc >> 32);
	*(unsigned char *)(slotBase + 0xc) = b;
	*(unsigned int *)(self + 0x1ac) += 1;

	if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] != 0)
		return;

	CSTGMidiQueue *q = (CSTGMidiQueue *)(*(unsigned int *)(self + 0xf8));
	if (q->GetNumWritableBytes() == 0)
		return;

	unsigned char *ctl = (unsigned char *)q;
	unsigned char *bufBase = (unsigned char *)(*(unsigned int *)(self + 0xfc));
	unsigned int wrappedPos = (*(unsigned int *)(ctl + 0xc)) & (*(unsigned int *)(ctl + 0x8));

	bufBase[wrappedPos] = b;
	*(unsigned int *)(ctl + 0xc) += 1;
}

/*
 * state==2 ("locally buffering") flush (.text+0xf7c67 / f7ce3, both
 * real call sites confirmed bit-identical): append the 0xF7 (EOX)
 * terminator to the local scratch buffer, then -- if the message
 * processor isn't busy -- flush the whole scratch buffer to the PRIMARY
 * writer (+0xf0). Confirmed real target is +0xf0 at BOTH call sites,
 * even though the two callers differ on which writer the OUTER tail flush
 * (FlushSysExStateThenForward's own final Write()) uses.
 */
static void FlushLocalScratch(CSTGMidiInPort *p)
{
	unsigned char *self = (unsigned char *)p;
	unsigned char count = self[0x24];

	self[4 + count] = 0xf7;
	unsigned char newCount = (unsigned char)(count + 1);
	self[0x24] = newCount;

	if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] == 0)
		WriterAt(p, W_PRIMARY)->Write(self + 4, newCount, false);
}

/*
 * state==3 ("streaming") flush (.text+0xf7c2e / f7caa, both real call
 * sites confirmed bit-identical): flush a single 0xF7 to the primary
 * writer (gated by streamFlagPrimary AND the message-processor-busy
 * check) and/or a single 0xF7 to the KG writer (gated by streamFlagKG
 * AND flags&1 -- NOT re-checking the message-processor gate, a real,
 * confirmed asymmetry with the primary-writer arm).
 */
static void FlushStreamingState(CSTGMidiInPort *p)
{
	unsigned char *self = (unsigned char *)p;

	if (self[0x2e1] != 0) {
		if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] == 0)
			WriterAt(p, W_PRIMARY)->Write((unsigned char)0xf7);
	}
	if (self[0x2e2] != 0 && (self[0x26] & 0x1) != 0)
		WriterAt(p, W_KG)->Write((unsigned char)0xf7);
}

/*
 * Shared "abort in-progress SysEx state, then forward the whole incoming
 * buffer" tail (.text+0xf7b69 / f7bf9, both real call sites confirmed
 * bit-identical EXCEPT for which writer gets the final whole-buffer
 * flush): the "system common" (0xF1-0xF6) caller flushes to the REALTIME
 * writer (+0xf8); the "new channel status byte" (0x80-0xF0) caller
 * flushes to the PRIMARY writer (+0xf0). Double-checked against
 * relocations (f7b95's `lea eax,[esi+0xf8]` vs f7c25's `lea
 * eax,[esi+0xf0]`) -- a real asymmetry, not a transcription error.
 */
static void FlushSysExStateThenForward(CSTGMidiInPort *p, const unsigned char *data,
					unsigned int len, unsigned int tailWriterOffset)
{
	unsigned char *self = (unsigned char *)p;

	if (self[0x2e0] == 2)
		FlushLocalScratch(p);
	else if (self[0x2e0] == 3)
		FlushStreamingState(p);

	self[0x2e0] = 0;
	self[0x24] = 0;

	if (((unsigned char *)CSTGMessageProcessor::sInstance)[0x48] != 0)
		return;

	WriterAt(p, tailWriterOffset)->Write(data, len, false);
}

/*
 * CSTGMidiInPortGeneric::Receive(const unsigned char*, unsigned int) --
 * see this file's own header comment for full ground-truthing.
 */
void CSTGMidiInPortGeneric::Receive(const unsigned char *data, unsigned int len)
{
	unsigned char *self = (unsigned char *)this;

	if (len == 0)
		return;

	unsigned char flags = self[0x26];
	if ((flags & 0x2) == 0)
		return;

	/* Oddity #1 -- see file header comment. */
	if (*(volatile unsigned char *)&CSTGMidiPortManager::sInstance == 0)
		return;

	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	if (g[0x6ac] != 0) {
		if ((flags & 0x1) != 0)
			WriterAt(this, W_KG)->Write(data, len, false);
		return;
	}

	/* Oddity #2 -- see file header comment. */
	*(unsigned int *)(self + 0x2e4) = *(unsigned int *)(g + 0x29c9fa8);

	unsigned char b0 = data[0];

	if (b0 == 0xf7 || b0 == 0xf0) {
		ReceiveSysEx(data, len);
		return;
	}

	if (b0 > 0xf7) {
		/* 0xF8-0xFF: realtime bytes + 0xFE active-sensing marker. */
		if (b0 > 0xfc) {
			if (b0 == 0xfe)
				self[0x2e3] = 1;
			return;			/* 0xFD/0xFF: unhandled */
		}
		if (b0 < 0xfa && b0 != 0xf8)
			return;			/* 0xF9: undefined, ignored */
		PushRealtimeByte(this, b0);
		return;
	}

	if (b0 > 0xf0) {
		/* 0xF1-0xF6: system common (non-exclusive). */
		FlushSysExStateThenForward(this, data, len, W_REALTIME);
		return;
	}

	if ((signed char)b0 < 0) {
		/* 0x80-0xEF: new channel voice/mode status byte. */
		FlushSysExStateThenForward(this, data, len, W_PRIMARY);
		return;
	}

	/* 0x00-0x7F: plain data byte, only meaningful mid-SysEx. */
	if (self[0x2e0] == 0)
		return;
	ReceiveSysEx(data, len);
}

/*
 * Flat, plain-C-callable alias under the EXACT name/signature the
 * project brief requested: `MidiInPortGeneric7Receive(void*, const
 * uint8_t*, uint32_t)`, regparm(3) EAX/EDX/ECX = port_obj/bytes/len.
 * This is a trivial tail call, not a separate real OA.ko symbol -- the
 * real symbol is the C++ method above (`_ZN21CSTGMidiInPortGeneric7Rec
 * eiveEPKhj`). Provided so downstream kernel modules (e.g.
 * KronosScreenRemoteDaemon/midi_module/midi_bridge.c's `receive_fn_t`,
 * which resolves this exact function by address and calls it with this
 * exact argument shape) have a concrete, addressable symbol to call
 * through, matching how they already treat this function on real
 * hardware.
 */
extern "C" __attribute__((regparm(3)))
void MidiInPortGeneric7Receive(void *port_obj, const unsigned char *bytes, unsigned int len)
{
	((CSTGMidiInPortGeneric *)port_obj)->Receive(bytes, len);
}
