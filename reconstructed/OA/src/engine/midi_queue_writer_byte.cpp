// SPDX-License-Identifier: GPL-2.0
/*
 * midi_queue_writer_byte.cpp  -  CSTGMidiQueueWriter::Write(unsigned char).
 *
 * Deliberately its own translation unit, separate from midi_queue_writer.cpp
 * (which owns the already-real 3-arg `Write(const unsigned char*, unsigned
 * int, bool)` and has its own load-bearing mocks elsewhere -- see that
 * file's header comment) and separate from midi_port_manager.cpp /
 * midi_in_port.cpp (both independently under concurrent edit by other work
 * in this same reconstruction pass) -- a single-method file has the
 * smallest possible collision surface.
 *
 * Ground truth: raw objdump -d -M intel of OA_real.ko's
 * `.text._ZN19CSTGMidiQueueWriter5WriteEh` COMDAT section (72 bytes),
 * confirmed real (defined, not imported) via readelf -sW (WEAK, section-
 * relative value 0, real FUNC entry) and its own single relocation
 * (readelf -Wr on `.rel.text._ZN19CSTGMidiQueueWriter5WriteEh`) resolving
 * to `_ZNK13CSTGMidiQueue19GetNumWritableBytesEv` -- the SAME query the
 * 3-arg Write() effectively duplicates inline. Confirmed regparm(3):
 * EAX=this, DL=byte (single incoming argument, only the low byte used).
 */

#include "oa_global.h"
#include "oa_engine_init.h"	/* CSTGMidiQueue::GetNumWritableBytes() */

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}

void CSTGMidiQueueWriter::Write(unsigned char byte)
{
	unsigned char *self = (unsigned char *)this;
	CSTGMidiQueue *ringCtl = (CSTGMidiQueue *)FromU32(*(unsigned int *)self);

	if (ringCtl->GetNumWritableBytes() == 0)
		return;

	unsigned char *ctl = (unsigned char *)ringCtl;
	unsigned int writeCursor = *(unsigned int *)(ctl + 0xc);
	unsigned int mask = *(unsigned int *)(ctl + 0x8);
	unsigned int wrappedPos = writeCursor & mask;

	unsigned char *bufBase = FromU32(*(unsigned int *)(self + 0x4));
	bufBase[wrappedPos] = byte;

	*(unsigned int *)(ctl + 0xc) = writeCursor + 1;
}
