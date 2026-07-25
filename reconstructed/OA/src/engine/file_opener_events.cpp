// SPDX-License-Identifier: GPL-2.0
/*
 * file_opener_events.cpp  -  CSTGFileOpener::AddPlaybackEvent(CSTGAudioEvent*,
 * unsigned int)/AddRecordEvent(CSTGAudioEvent*, unsigned int) (batch 51),
 * plus CSTGFileOpener::Initialize() (batch 63) and the `sEventListMap`
 * global it populates.
 *
 * Deliberately its own dedicated TU, separate from managers.cpp (which owns
 * CSTGFileOpener's ctor/sInstance) -- matches this project's established
 * per-concern-file convention (e.g. playback_buffer_events.cpp vs.
 * managers.cpp for CSTGPlaybackBuffer). This file DOES now define its own
 * global storage (`sEventListMap`, added batch 63) -- confirmed via a
 * project-wide grep that no other TU defines it, so linking this file
 * alongside managers.cpp remains safe.
 *
 * Ground-truthed via objdump -dr against
 * /home/share/Decomp/OA.ko_Decomp/OA.ko:
 *   CSTGFileOpener::AddPlaybackEvent(CSTGAudioEvent*, unsigned int)  .text+0x11a4f0, 111B
 *   CSTGFileOpener::AddRecordEvent(CSTGAudioEvent*, unsigned int)    .text+0x11a570, 103B
 *
 * Both are self-contained (zero relocations in either body -- confirmed via
 * `readelf -r`, no external/internal calls at all) plain ring-buffer
 * producer bookkeeping, regparm(3): eax=this, edx=event, ecx=index.
 *
 * Real per-object layout confirmed (see oa_engine.h's own CSTGFileOpener
 * class comment for the full derivation, including how this resolves the
 * ctor's previously-unexplained "32 identical 16-byte slots" span):
 *   this+index*16           the "playback" lane for `index`       (4-field
 *                            ring: base ptr/write idx/read idx/capacity,
 *                            16 bytes: +0x0/+0x4/+0x8/+0xc)
 *   this+index*16+0x100      the "record" lane for the SAME `index` --
 *                            0x100 further into the identical 32-slot span,
 *                            not a separately allocated region
 *   this+0x200                a FIXED fallback lane (same 4-field ring
 *                            shape), used by BOTH methods whenever their
 *                            own per-index lane is full. Confirmed via
 *                            address arithmetic to be exactly slot 31 of
 *                            the ctor's own 32-slot span (`+0x10 + 31*0x10
 *                            == +0x200`), not a separate structure.
 *
 * Per-lane logic (identical for the normal lane and the fallback lane,
 * confirmed instruction-for-instruction):
 *   nextWrite = (writeIdx + 1) % capacity
 *   if (nextWrite == readIdx)      // lane full
 *       <write into the FIXED fallback lane instead, unconditionally,
 *        with NO fullness check of its own on the fallback lane -- a real,
 *        faithfully-preserved silent-overwrite-on-double-overflow quirk>
 *   else
 *       base[writeIdx] = event; writeIdx = (writeIdx + 1) % capacity
 *
 * `index` is used purely as a lane selector (`this + index*16[+0x100]`) --
 * no bounds check against the 32-slot span exists in either method; an
 * out-of-range `index` would read/write past CSTGFileOpener's own 544-byte
 * object, exactly like the real target. Neither method dereferences
 * `event` itself (stored opaquely as a raw pointer) -- the `CSTGAudioEvent*`
 * parameter type matches the real mangled signature exactly, even though
 * every confirmed real caller so far (`CSTGHDRManager::
 * ProcessPlaybackCommands()`, hdr_playback_commands.cpp) actually passes a
 * `CSTGPlaybackEvent*` reinterpreted to this type -- safe, since the value
 * is never read through either type here.
 */

#include "oa_engine.h"
#include "oa_bank_memory.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

/* Shared per-lane enqueue logic, factored out since both methods' own real
 * bodies are byte-for-byte identical past their own lane-address
 * computation -- matches this project's established "identical instruction
 * sequence -> shared static helper" technique (sec 10.167). */
static void FileOpenerEnqueue(unsigned char *fileOpener, unsigned char *lane, CSTGAudioEvent *event)
{
	unsigned int writeIdx = *(unsigned int *)(lane + 0x4);
	unsigned int capacity = *(unsigned int *)(lane + 0xc);
	unsigned int nextWrite = (writeIdx + 1) % capacity;

	if (nextWrite == *(unsigned int *)(lane + 0x8)) {
		/* Lane full -- fall back to the FIXED lane at +0x200, no
		 * fullness check of its own (faithful quirk, see file header). */
		unsigned char *fallback = fileOpener + 0x200;
		unsigned int fbWriteIdx = *(unsigned int *)(fallback + 0x4);
		unsigned char *fbBase = FromU32(*(unsigned int *)(fallback + 0x0));

		*(unsigned int *)(fbBase + fbWriteIdx * 4) = ToU32(event);

		unsigned int fbNext = (fbWriteIdx + 1) % *(unsigned int *)(fallback + 0xc);
		*(unsigned int *)(fallback + 0x4) = fbNext;
		return;
	}

	unsigned char *base = FromU32(*(unsigned int *)(lane + 0x0));
	*(unsigned int *)(base + writeIdx * 4) = ToU32(event);
	*(unsigned int *)(lane + 0x4) = nextWrite;
}

void CSTGFileOpener::AddPlaybackEvent(CSTGAudioEvent *event, unsigned int index)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *lane = self + index * 0x10;
	FileOpenerEnqueue(self, lane, event);
}

void CSTGFileOpener::AddRecordEvent(CSTGAudioEvent *event, unsigned int index)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *lane = self + index * 0x10 + 0x100;
	FileOpenerEnqueue(self, lane, event);
}

/* Global 33-entry table of pointers to CSTGFileOpener's own event-lane ring
 * structs, populated below. Confirmed real symbol name `sEventListMap` via
 * `objdump -dr` relocations (R_386_32 against every map-write instruction
 * in Initialize()) -- see oa_engine.h for the full derivation. */
unsigned char *sEventListMap[33];

/*
 * CSTGFileOpener::Initialize() (batch 63, `.text+0x119f90`, 1364 bytes).
 * Ground-truthed via `objdump -dr`: fully mechanical, no branches, no
 * conditionals -- a straight-line sequence of `CSTGBankMemory::
 * AllocAligned` calls populating this object's 34 total ring-shaped
 * regions (see oa_engine.h's class comment for the complete field-by-field
 * derivation). Reconstructed here as a loop over the 32 regular "A"/"B"
 * lanes plus two tail blocks for the fallback lane and the object's own
 * command ring, rather than 34 unrolled call sites -- confirmed
 * behaviorally equivalent (no field is read before being written anywhere
 * in the real function, so the real code's interleaved instruction order,
 * a compiler-scheduling artifact, carries no observable semantics beyond
 * what's captured here).
 *
 * Lane shape (all 34 regions, 16 bytes each): +0x0 base ptr (zeroed by the
 * ctor, set here), +0x4 write idx / +0x8 read idx (both left zeroed by the
 * ctor, untouched here), +0xc capacity (left untouched by the ctor, set
 * here to `byteSize/4` for every lane except the object's own command ring
 * at +0x210, which uses `byteSize/8` -- matching that ring's own confirmed
 * 8-byte `status:1 byte, ptr:4 bytes` record shape, ProcessCommands()).
 */
void CSTGFileOpener::Initialize()
{
	unsigned char *self = (unsigned char *)this;

	/* 16 "A" lanes (this+0x00..0xf0, sEventListMap[0..15]) paired with 16
	 * "B" lanes (this+0x100..0x1f0, sEventListMap[16..31]) -- same 0x324-
	 * byte (804-byte, 201x 4-byte-element) AllocAligned size throughout. */
	for (unsigned int i = 0; i < 16; i++) {
		unsigned char *aLane = self + i * 0x10;
		unsigned char *bLane = self + 0x100 + i * 0x10;

		*(unsigned int *)(aLane + 0xc) = 0xc9;	/* 0x324/4 */
		*(unsigned int *)(aLane + 0x0) = ToU32(CSTGBankMemory::AllocAligned(0x324, 0x10));

		*(unsigned int *)(bLane + 0xc) = 0xc9;	/* 0x324/4 */
		*(unsigned int *)(bLane + 0x0) = ToU32(CSTGBankMemory::AllocAligned(0x324, 0x10));

		sEventListMap[i]      = aLane;
		sEventListMap[16 + i] = bLane;
	}

	/* 33rd lane: the fixed fallback lane at +0x200 (already documented by
	 * AddPlaybackEvent()/AddRecordEvent() above), same 4-byte-element
	 * shape, a bigger 0xd24-byte (3364-byte, 841x) allocation. */
	unsigned char *fallback = self + 0x200;
	*(unsigned int *)(fallback + 0xc) = 0x349;	/* 0xd24/4 */
	*(unsigned int *)(fallback + 0x0) = ToU32(CSTGBankMemory::AllocAligned(0xd24, 0x10));
	sEventListMap[32] = fallback;

	/* The object's own ProcessCommands() ring at +0x210 -- NOT part of
	 * sEventListMap (different element size, a genuine command queue, not
	 * an "event list"). 0x8348-byte (33608-byte) allocation, 8-byte
	 * elements -> capacity 0x1069 (4201). */
	unsigned char *ownRing = self + 0x210;
	*(unsigned int *)(ownRing + 0xc) = 0x1069;	/* 0x8348/8 */
	*(unsigned int *)(ownRing + 0x0) = ToU32(CSTGBankMemory::AllocAligned(0x8348, 0x10));
}

/*
 * CSTGFileOpener::ProcessCommands() (`.text+0x11a870`, 201 bytes,
 * 2026-07-25). Ground-truthed via `objdump -dr`. Was long documented in
 * bar2_stubs.cpp as blocked by "an unrecovered vtable or pointer-to-
 * member-function table" (part of a 5-sibling cluster, sec 10.153
 * onward) -- fresh disassembly this batch found that characterization
 * doesn't hold here: it's a plain fixed-slot vtable dispatch on an
 * untyped payload object, the same idiom already established for
 * `CSTGEffectRackVars::UpdateDModRoutings()` (`oa_global.h`,
 * `global.cpp`) -- not a real per-command function-pointer table. (Its
 * siblings `CSTGHDRFileReader`/`CSTGStreamingFileReader::
 * ProcessCommands()` genuinely DO dispatch through such a table --
 * `TSTGArrayManager<T>::sInstance->indexArray` -- and remain blocked;
 * see bar2_stubs.cpp.)
 *
 * Drains this object's own command ring at `+0x210` (`Initialize()`'s
 * own `ownRing` above): `+0x214` write idx (never written here, some
 * other not-yet-reconstructed producer owns it), `+0x218` read idx
 * (advanced here), `+0x21c` capacity. 8-byte records: 1-byte tag `+0x0`,
 * 4-byte payload pointer `+0x4` (matches the `status:1 byte, ptr:4 bytes`
 * shape already documented above).
 *
 * tag==0: `payload->fieldAt(0xc) = 2`, raw vtable dispatch (slot 2,
 * `call [vtbl+0x8]`).
 * tag==1: `payload->fieldAt(0xc) = 4`, `fieldAt(0x10) = 1`, raw vtable
 * dispatch (slot 4, `call [vtbl+0x10]`).
 * tag==2: `payload->fieldAt(0xc) = 3`, then pushes `{payload, 0}`
 * (8-byte record) onto `CSTGFileCloser::sInstance`'s own FIRST embedded
 * ring at `+0x00`/`+0x04`/`+0x08`/`+0x0c` -- the SAME target/shape
 * `CSTGSamplingDaemon::ProcessCommands()`'s own tag==0 push and
 * `CSTGHDRFileWriter::ProcessCommands()`'s own tag==0/2 pushes use
 * (managers.cpp) -- no vtable call.
 * Any other tag: real, faithfully-preserved no-op -- entry still
 * consumed, no further action.
 */
void CSTGFileOpener::ProcessCommands()
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *ring = self + 0x210;
	unsigned int writeIdx = *(unsigned int *)(ring + 0x4);
	unsigned int readIdx = *(unsigned int *)(ring + 0x8);

	while (writeIdx != readIdx) {
		unsigned int capacity = *(unsigned int *)(ring + 0xc);
		unsigned int nextIdx = (readIdx + 1) % capacity;
		unsigned char *entry = FromU32(*(unsigned int *)(ring + 0x0)) + readIdx * 8;

		*(unsigned int *)(ring + 0x8) = nextIdx;

		unsigned char tag = entry[0];
		unsigned char *payload = FromU32(*(unsigned int *)(entry + 4));

		if (tag == 0) {
			*(unsigned int *)(payload + 0xc) = 2;
			typedef void (*VtableSlot2Fn)(void *);
			void **vtable = *(void ***)payload;
			((VtableSlot2Fn)vtable[2])(payload);
		} else if (tag == 1) {
			*(unsigned int *)(payload + 0xc) = 4;
			*(unsigned int *)(payload + 0x10) = 1;
			typedef void (*VtableSlot4Fn)(void *);
			void **vtable = *(void ***)payload;
			((VtableSlot4Fn)vtable[4])(payload);
		} else if (tag == 2) {
			*(unsigned int *)(payload + 0xc) = 3;

			unsigned char *fc = (unsigned char *)CSTGFileCloser::sInstance;
			unsigned int fcCursor = *(unsigned int *)(fc + 0x4);
			unsigned char *fcEntry = FromU32(*(unsigned int *)(fc + 0x0)) + fcCursor * 8;
			*(unsigned int *)(fcEntry + 0) = ToU32(payload);
			*(unsigned int *)(fcEntry + 4) = 0;
			*(unsigned int *)(fc + 0x4) = (fcCursor + 1) % *(unsigned int *)(fc + 0xc);
		}

		writeIdx = *(unsigned int *)(ring + 0x4);
		readIdx = *(unsigned int *)(ring + 0x8);
	}
}
