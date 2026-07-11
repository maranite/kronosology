// SPDX-License-Identifier: GPL-2.0
/*
 * file_opener_events.cpp  -  CSTGFileOpener::AddPlaybackEvent(CSTGAudioEvent*,
 * unsigned int)/AddRecordEvent(CSTGAudioEvent*, unsigned int) (batch 51).
 *
 * Deliberately its own dedicated TU, separate from managers.cpp (which owns
 * CSTGFileOpener's ctor/sInstance) -- matches this project's established
 * per-concern-file convention (e.g. playback_buffer_events.cpp vs.
 * managers.cpp for CSTGPlaybackBuffer). No new global storage is defined
 * here, only methods, so linking this file alongside managers.cpp (which
 * DOES define `CSTGFileOpener::sInstance`) is safe wherever both are needed
 * -- confirmed via a project-wide grep (no verify/ .cpp file references
 * AddPlaybackEvent/AddRecordEvent before this batch, so there is no stale
 * mock to remove).
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
