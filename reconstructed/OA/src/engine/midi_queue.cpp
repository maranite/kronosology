// SPDX-License-Identifier: GPL-2.0
/*
 * midi_queue.cpp  -  CSTGMidiQueue::GetNumWritableBytes() const (sec
 * 10.150, `.text+0x400a0`, 84 bytes).
 *
 * Shares the SAME ringCtl memory as CSTGMidiQueueWriter::Write()
 * (midi_queue_writer.cpp), but deliberately lives in its OWN separate
 * translation unit: this one IS linked into test_global.cpp (its own
 * mock footprint there was tiny -- the old mock's return value was
 * never actually varied away from its 0 default anywhere in that
 * file), while Write() must NOT be (test_global.cpp keeps its own
 * Write() mock, load-bearing for ~10 other assertions) -- see
 * midi_queue_writer.cpp's own header comment for the full reasoning.
 *
 * `this` IS the ringCtl block directly (no extra `self+0x0`
 * indirection like Write()'s own wrapper object -- confirmed via
 * CSTGGlobal::SubmitPerfChangeRequest's own call site, which
 * dereferences `CSTGMidiPortManager::sInstance + 0x208` ONE level
 * before calling this). See oa_global.h's `CSTGMidiQueueWriter` comment
 * for the confirmed shared ringCtl field layout (`+0x8` capacity mask,
 * `+0xc` write cursor, `+0x10+i*4` reader i's cursor, `+0x20` active
 * reader count).
 *
 * Algebraically identical to Write()'s own "free space" computation,
 * just returning it directly instead of using it to gate a copy:
 * `(mask+1) - max_i(writeCursor - readerCursor[i])` for `i` in
 * `[0, readerCount)`.
 */

#include "oa_global.h"
#include "oa_engine_init.h"	/* for CSTGMidiQueue */

unsigned int CSTGMidiQueue::GetNumWritableBytes() const
{
	const unsigned char *ringCtl = (const unsigned char *)this;

	unsigned int writeCursor = *(const unsigned int *)(ringCtl + 0xc);
	unsigned int readerCount = ringCtl[0x20];
	unsigned int worstBacklog = 0;
	for (unsigned int i = 0; i < readerCount; i++) {
		unsigned int readerPos = *(const unsigned int *)(ringCtl + 0x10 + i * 4);
		unsigned int backlog = writeCursor - readerPos;
		if (worstBacklog < backlog)
			worstBacklog = backlog;
	}

	unsigned int mask = *(const unsigned int *)(ringCtl + 0x8);
	return (mask + 1) - worstBacklog;
}

/*
 * Reset() (batch 12, `.text+0x40060`, 36 bytes) confirmed real via full
 * disassembly: exactly 5 dword stores, no branches --
 *   movl $0,0xc(%eax)   ; writeCursor = 0
 *   movl $0,0x10(%eax)  ; readerCursor[0] = 0
 *   movl $0,0x14(%eax)  ; readerCursor[1] = 0
 *   movl $0,0x18(%eax)  ; readerCursor[2] = 0
 *   movl $0,0x1c(%eax)  ; readerCursor[3] = 0
 * `+0x8` (capacity mask) and `+0x20` (active reader count) are
 * confirmed NOT touched -- a real, harmless gap: this rewinds an
 * already-`Initialize()`'d ring back to empty, it doesn't reconfigure
 * it. `this` IS the ringCtl block directly, same as
 * GetNumWritableBytes() above (no `+0x0` wrapper indirection).
 */
void CSTGMidiQueue::Reset()
{
	unsigned char *ringCtl = (unsigned char *)this;

	*(unsigned int *)(ringCtl + 0xc) = 0;
	*(unsigned int *)(ringCtl + 0x10) = 0;
	*(unsigned int *)(ringCtl + 0x14) = 0;
	*(unsigned int *)(ringCtl + 0x18) = 0;
	*(unsigned int *)(ringCtl + 0x1c) = 0;
}
