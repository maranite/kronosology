// SPDX-License-Identifier: GPL-2.0
/*
 * midi_dispatcher.cpp  -  CSTGMidiDispatcher::CSTGMidiDispatcher()/
 * Initialize(). See oa_engine_init.h's own file comment for
 * CSTGMidiQueue's discovery.
 *
 * Ground-truthed via readelf+objdump (`-j .text`) against OA_real.ko:
 *   CSTGMidiDispatcher::CSTGMidiDispatcher()  .text+0xd9670, 490 bytes
 *   CSTGMidiDispatcher::Initialize()          .text+0xd9860, 116 bytes
 *
 * The constructor is almost entirely zeroing -- every touched byte is
 * confirmed real (individually, not estimated), but several small
 * gaps (e.g. +0xc..+0x1f, +0x2/+0x3 of several 4-byte groups, +0xa3)
 * are confirmed NEVER written by this constructor -- preserved
 * verbatim as real gaps rather than "cleaned up" by also zeroing
 * them. The one non-zero literal (+0xa2 = 1) is a confirmed real
 * immediate.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"

/* Local minimal stand-in matching oa_types.h's own convention (same
 * mangled `sInstance` symbol, defined once elsewhere in this project's
 * heap-manager work) -- avoids pulling in oa_types.h/oa_heapmanager.h
 * here, matching this project's established ODR-avoidance pattern. */
struct CSTGHeapManager { static char *sInstance; };

/* CSTGMidiDispatcher::sInstance is already defined in engine_init.cpp
 * (sec 10.58, same treatment as CSTGWaveSeqManager::sInstance,
 * sec 10.62) -- NOT redefined here. */

CSTGMidiDispatcher::CSTGMidiDispatcher()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);

	*(unsigned int *)(p + 0x4) = 0;
	*(unsigned int *)(p + 0x8) = 0;
	/* +0xc..+0x1f: confirmed real gap, never written by this ctor. */

	for (int i = 0; i < 8; i++) {
		p[0x60 + i * 4] = 0;
		p[0x61 + i * 4] = 0;
		/* +2/+3 of each group: confirmed real gap. */
	}
	for (int i = 0; i < 8; i++) {
		p[0x80 + i * 4] = 0;
		p[0x81 + i * 4] = 0;
	}

	sInstance = this;

	p[0xa0] = 0;
	p[0xa1] = 0;
	p[0xa2] = 1;
	/* +0xa3: confirmed real gap. */
	*(unsigned int *)(p + 0xa4) = 0;

	p[0x1] = 0;
	p[0x0] = 0;
	for (int i = 0; i < 16; i++) {
		p[0x30 + i] = 0;
		p[0x50 + i] = 0;
	}
	for (int i = 0; i < 16; i++) {
		p[0x20 + i] = 0;
		p[0x40 + i] = 0;
	}
}

/* Host/target pointer-width fix (this project's established pattern):
 * the real target stores +0x4/+0x8 as plain 32-bit pointer fields, but
 * a native host pointer is 8 bytes on a 64-bit build. */
static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

void CSTGMidiDispatcher::Initialize()
{
	unsigned char *p = reinterpret_cast<unsigned char *>(this);
	unsigned char *portMgr = reinterpret_cast<unsigned char *>(CSTGMidiPortManager::sInstance);
	unsigned char *heap = reinterpret_cast<unsigned char *>(CSTGHeapManager::sInstance);

	/* Real confirmed field: the ADDRESS of CSTGMidiPortManager's own
	 * +0x1a4 slot field (a "confirmed 0xffffffff sentinel" per
	 * engine_init.cpp's own already-reconstructed struct-init block,
	 * sec 10.58 -- meaning the bounds check below will normally take
	 * the "invalid slot" path on this project's own reconstructed
	 * boot path, leaving +0x8 at 0). */
	unsigned char *slotFieldAddr = portMgr + 0x1a4;
	*(unsigned int *)(p + 0x4) = ToU32(slotFieldAddr);

	unsigned int slot = *(unsigned int *)slotFieldAddr;
	unsigned int resolved = 0;
	if (slot <= 0x1869f) {
		/* Real confirmed heap-slot resolution: `heap + 0x18 +
		 * slot*0x14` (the sentinel-relative addressing -- slot 0
		 * is the sentinel itself), reading that entry's own +0xc
		 * "offset" field then adding heapBase. This independently
		 * RECONFIRMS (rather than contradicts) sec 10.60's own
		 * heap_manager.cpp implementation and oa_heap_region()'s
		 * already-established `heap+0x24+slot*0x14` formula --
		 * `0x18 + 0xc == 0x24` exactly. The "open discrepancy"
		 * flagged in sec 10.60 is hereby RESOLVED, not just
		 * observed a third time: it was a mistaken worry in this
		 * project's own earlier derivation, not a real bug -- see
		 * this pass's own MASTER_REFERENCE.md correction. */
		unsigned char *entry = heap + 0x18 + slot * 0x14;
		if (entry != 0) {
			unsigned int offset = *(unsigned int *)(entry + 0xc);
			unsigned int heapBase = *(unsigned int *)(heap + 0x1e8498);
			resolved = offset + heapBase;
		}
	}
	*(unsigned int *)(p + 0x8) = resolved;

	unsigned char readerId = ((CSTGMidiQueue *)slotFieldAddr)->AllocReader();
	p[0xc] = readerId;
	p[0xd] = 0;
}

/*
 * HandleController(const unsigned char*, int, int) (sec 10.139): see
 * oa_engine_init.h for the full confirmed shape.
 */
void CSTGMidiDispatcher::HandleController(const unsigned char *bytes, int source, int target)
{
	unsigned char channel = (unsigned char)(bytes[0] & 0xf);
	HandleController(channel, bytes[1], bytes[2], source, target);
}
