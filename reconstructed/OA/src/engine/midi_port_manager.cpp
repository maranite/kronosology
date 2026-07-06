// SPDX-License-Identifier: GPL-2.0
/*
 * midi_port_manager.cpp  -  CSTGMidiPortManager::WriteSTGMidiOutQueue()/
 * NotifyNKS4TestMode() (batch 12).
 *
 * Deliberately a SEPARATE translation unit from global.cpp: both
 * symbols already have their own load-bearing call-counting mocks in
 * test_global.cpp (~30 assertions across that file per WriteSTGMidiOutQueue's
 * own long-standing header comment in oa_engine.h, sec 10.145) plus
 * trivial link-satisfying mocks in test_engine.cpp/test_global_ctor.cpp
 * -- none of the three link this file, so all of their existing mocks
 * are completely untouched. Matches this project's own established
 * "give it its own TU" technique, the SAME one that already let
 * CSTGMidiQueueWriter::Write() itself (sec 10.83) get a real body
 * without disturbing test_global.cpp's own counters.
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"	/* for CSTGMidiQueue::Reset() */

/*
 * Deliberately NOT `#include "oa_heap.h"` here: oa_heap.h transitively
 * pulls in oa_types.h's minimal `struct CSTGGlobal { static char
 * *sInstance; };`, which directly conflicts with oa_global.h's fuller
 * `class CSTGGlobal` (a DIFFERENT type for the same static member) --
 * the exact pre-existing ODR hazard oa_setup_global_resources.h's own
 * header comment already documents and routes around. This file needs
 * the real `CSTGGlobal`/`CSTGMidiPortManager`/`CSTGMidiQueueWriter`/
 * `CSTGMidiQueue` (from oa_global.h/oa_engine.h/oa_engine_init.h) AND
 * `CSTGHeapManager::sInstance`, so it re-derives `oa_heap_base()`/
 * `oa_heap_region()`'s own formulas locally instead, matching
 * setup_global_resources.cpp's own established precedent for the same
 * conflict (and midi_dispatcher.cpp's own local minimal
 * `CSTGHeapManager` stand-in) -- same real storage, defined once in
 * heap_manager.cpp, not redefined here.
 */
struct CSTGHeapManager { static char *sInstance; };

static char *LocalHeapBase()
{
	char *heap = CSTGHeapManager::sInstance;
	if (heap == (char *)(long)-44)		/* 0xFFFFFFD4 sentinel: heap not yet up */
		return 0;
	return (char *)(*(unsigned int *)(heap + 0x38) + *(unsigned int *)(heap + 0x1e8498));
}

static char *LocalHeapRegion(unsigned int slot)
{
	char *heap = CSTGHeapManager::sInstance;
	if (slot >= 100000)
		return 0;
	return (char *)(*(unsigned int *)(heap + 0x24 + slot * 0x14) +
			*(unsigned int *)(heap + 0x1e8498));
}

/*
 * WriteSTGMidiOutQueue(const unsigned char*, unsigned int) (sec
 * 10.73/10.145, `.text+0xf57d0`, 53 bytes) confirmed via disassembly,
 * already fully documented in oa_engine.h's own class comment: no-op if
 * `CSTGGlobal::sInstance->fieldAt(0x6ac)` (a confirmed real gate byte,
 * see global.cpp's own UpdateSPDIFSampleRate comment for its OTHER
 * confirmed use) is nonzero; otherwise forwards to the already-real
 * `CSTGMidiQueueWriter::Write(data, length, false)` on the embedded
 * `CSTGMidiQueueWriter` at `this->fieldAt(0x138)`. `data`/`length` pass
 * through untouched from this method's own regparm(3) edx/ecx -- no
 * separate C++ parameter renaming needed, they map directly.
 */
void CSTGMidiPortManager::WriteSTGMidiOutQueue(const unsigned char *data, unsigned int length)
{
	unsigned char *g = (unsigned char *)CSTGGlobal::sInstance;

	if (g[0x6ac] != 0)
		return;

	CSTGMidiQueueWriter *writer = (CSTGMidiQueueWriter *)((unsigned char *)this + 0x138);
	writer->Write(data, length, false);
}

/*
 * NotifyNKS4TestMode() (sec 10.73, `.text+0xf5390`, 115 bytes) confirmed
 * via disassembly: resolves a heap-managed "test mode" region through
 * the SAME two idioms already recovered in oa_heap.h (re-derived
 * locally above as `LocalHeapBase()`/`LocalHeapRegion()` rather than
 * including oa_heap.h directly -- see this file's own top-of-file note
 * on the ODR conflict that rules that out) --
 *   heapBase = LocalHeapBase()                    ; 0 if heap not up yet
 *   slot     = *(unsigned int*)(heapBase + 8)     ; a slot number stored
 *                                                  ; INSIDE the heap base
 *                                                  ; region itself
 *   region   = LocalHeapRegion(slot)              ; 0 if slot >= 100000
 * then resets FOUR embedded CSTGMidiQueue objects inside that region, at
 * a confirmed real `0x64`-byte stride (`+0x0`/`+0x64`/`+0xc8`/`+0x12c`).
 * This REFINES oa_engine.h's own older "indirect calls through fields of
 * a not-yet-identified structure" description (written before
 * CSTGMidiQueue existed in this project, sec 10.63/10.82/10.150): all
 * four calls are in fact DIRECT relocated calls to the real, already-
 * tiny `CSTGMidiQueue::Reset()` (batch 12, midi_queue.cpp), not indirect
 * vtable dispatch -- exactly the "check whether the blocking dependency
 * is itself small" pattern that unblocked this one.
 *
 * If `region` ends up 0 (heap not up, or slot out of range), the real
 * code still unconditionally calls `Reset()` on addresses 0x0/0x64/0xc8/
 * 0x12c -- a genuine near-NULL-dereference crash risk on real hardware,
 * faithfully preserved here (not guarded), matching this project's
 * "preserve real bugs, don't add error handling" rule. Not exercised by
 * this pass's own KAT with region==0 for that reason (would crash the
 * host test process too) -- only the "heap is up, slot in range" path
 * is driven there.
 */
void CSTGMidiPortManager::NotifyNKS4TestMode()
{
	char *heapBase = LocalHeapBase();
	unsigned int slot = *(unsigned int *)(heapBase + 8);
	unsigned char *region = (unsigned char *)LocalHeapRegion(slot);

	((CSTGMidiQueue *)(region + 0x0))->Reset();
	((CSTGMidiQueue *)(region + 0x64))->Reset();
	((CSTGMidiQueue *)(region + 0xc8))->Reset();
	((CSTGMidiQueue *)(region + 0x12c))->Reset();
}
