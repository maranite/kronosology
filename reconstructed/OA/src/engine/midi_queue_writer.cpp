// SPDX-License-Identifier: GPL-2.0
/*
 * midi_queue_writer.cpp  -  CSTGMidiQueueWriter::Write() (sec 10.83).
 *
 * Deliberately a SEPARATE translation unit from global.cpp: this
 * symbol's mock in test_global.cpp is load-bearing for roughly a dozen
 * other UpdateXXX assertions there (checking "was a MIDI message
 * sent", not this method's own internals), so the real body lives
 * here instead -- matching this project's own established per-unit
 * file convention (e.g. midi_dispatcher.cpp, heap_manager.cpp) -- with
 * its own dedicated host KAT (verify/test_midi_queue_writer.cpp)
 * exercising the real ring-buffer logic directly.
 *
 * CSTGMidiQueue::GetNumWritableBytes() (sec 10.150) shares the SAME
 * ringCtl memory this file's own Write() operates on, but deliberately
 * lives in its own SEPARATE file, midi_queue.cpp, NOT here: it needs to
 * be linked into test_global.cpp (its own mock footprint there was tiny
 * enough to promote), but Write() must NOT be (test_global.cpp keeps
 * its own Write() mock, load-bearing for ~10 other assertions) -- one
 * file per real/mocked split, matching this project's per-unit
 * convention rather than one file trying to serve both link needs at
 * once.
 */

#include "oa_global.h"

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}

/*
 * CSTGMidiQueueWriter::Write(const unsigned char*, unsigned int, bool)
 * (.text+0x401a0 in OA_real.ko) -- confirmed real: a multi-reader
 * lock-free ring buffer writer, drop-on-full (never blocks; silently
 * discards the whole write if there isn't room for it). See
 * oa_global.h for the confirmed ringCtl/buffer field layout.
 *
 * Computes the WORST-CASE backlog across every active reader (the
 * maximum of writeCursor - readerCursor[i]), then free space =
 * (capacityMask+1) - worstBacklog; if `length` doesn't fit, the whole
 * call is a no-op (the real disassembly's own early-exit branch). The
 * `flag` (bool) parameter is confirmed genuinely UNUSED by this
 * function's own body -- not a transcription omission, the real
 * disassembly never references the incoming 4th stack argument at all.
 */
void CSTGMidiQueueWriter::Write(const unsigned char *data, unsigned int length, bool)
{
	unsigned char *self = (unsigned char *)this;
	unsigned char *ringCtl = FromU32(*(unsigned int *)self);

	unsigned int writeCursor = *(unsigned int *)(ringCtl + 0xc);
	unsigned int readerCount = ringCtl[0x20];
	unsigned int worstBacklog = 0;
	for (unsigned int i = 0; i < readerCount; i++) {
		unsigned int readerPos = *(unsigned int *)(ringCtl + 0x10 + i * 4);
		unsigned int backlog = writeCursor - readerPos;
		if (worstBacklog < backlog)
			worstBacklog = backlog;
	}

	unsigned int mask = *(unsigned int *)(ringCtl + 0x8);
	unsigned int freeSpace = (mask + 1) - worstBacklog;
	if (length > freeSpace)
		return;

	unsigned char *bufBase = FromU32(*(unsigned int *)(self + 0x4));
	unsigned int wrappedPos = writeCursor & mask;
	unsigned int untilWrap = (mask + 1) - wrappedPos;
	unsigned int firstChunk = (length <= untilWrap) ? length : untilWrap;
	unsigned int secondChunk = length - firstChunk;

	/* No <cstring>/memcpy -- this project's own established convention
	 * (sec 10.56) avoids it on the freestanding -m32 Kbuild target. */
	for (unsigned int i = 0; i < firstChunk; i++)
		(bufBase + wrappedPos)[i] = data[i];
	for (unsigned int i = 0; i < secondChunk; i++)
		bufBase[i] = data[firstChunk + i];

	*(unsigned int *)(ringCtl + 0xc) = writeCursor + length;
}
