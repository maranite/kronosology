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

/*
 * Local minimal CSTGCPUInfo stand-in, same ODR-avoidance technique as
 * CSTGHeapManager above: only the leading 3 confirmed real fields
 * (oa_setup_global_resources.h's own full declaration) are needed here,
 * `field8` (cyclesPerTick, a float) at its confirmed real `+0x8` offset.
 * `sInstance`'s storage is defined once, in engine_startup_bits.cpp.
 */
struct CSTGCPUInfo {
	static CSTGCPUInfo *sInstance;
	unsigned int _cpuCount;	/* +0x0, unused here */
	unsigned int _khz;	/* +0x4, unused here */
	float field8;		/* +0x8, cyclesPerTick */
};

/*
 * Generic vtable dispatch for the not-yet-named MIDI in/out port classes
 * (CSTGMidiInPort/CSTGMidiOutPort) -- matches this project's established
 * `CallVtableSlot`-style treatment (oa_engine_init.h) for vtabled classes
 * whose own layout isn't independently reconstructed. Slot 0 is a bool
 * "query" method (confirmed via disassembly: `test al,al` on the return
 * value gates the slot-1 call); slot 1 hands the port a region pointer.
 */
static bool PortQuery(void *port)
{
	typedef bool (*Fn)(void *);
	void **vtable = *(void ***)port;
	return ((Fn)vtable[0])(port);
}
static void PortRegister(void *port, void *region)
{
	typedef void (*Fn)(void *, void *);
	void **vtable = *(void ***)port;
	((Fn)vtable[1])(port, region);
}

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
 * Initialize() (sec 10.230/MASTER_REFERENCE, `.text+0xf4f60`, 790 bytes)
 * -- see this method's own declaration comment in oa_engine.h for the
 * full summary. Root-caused via full `objdump -dr` disassembly of
 * OA_real.ko: this method was previously a no-op stub (bar2_stubs.cpp),
 * which is the ENTIRE root cause of the `CSTGMidiQueueWriter::Write()`
 * ringCtl-NULL crash (sec 10.230) -- engine_init.cpp's own confirmed
 * struct-init block explicitly zeroes `+0x208`/`+0x20c` right before
 * calling this method, exactly as the real binary does, trusting this
 * method to overwrite them for real; call ORDER was never the problem
 * (`setup_global_resources.cpp` calls `engine->Initialize()` -- which
 * calls this -- strictly before `global->Initialize()`, confirmed by
 * direct inspection of that file's own real call sequence).
 *
 * Five embedded CSTGMidiQueue rings, confirmed sizes/labels (extracted
 * directly from `.rodata.str1.1`):
 *   +0xc   0x1000 (4096) byte ring, format 0, "STG MIDI Out"
 *   +0x70  0x400  (1024) byte ring, format 0, "KG Regular MIDI Out"
 *   +0xd4  0x80   (128)  byte ring, format 0, "KG Real Time MIDI Out"
 *   +0x140 0x200  (512)  byte ring, format 1, "STG->KG"
 *   +0x1a4 0x100  (256)  byte ring, format 1, "KG->STG"
 * `format`'s own enum meaning (0 vs 1) isn't independently determined
 * beyond these two observed values.
 *
 * The embedded CSTGMidiQueueWriter sub-objects (oa_global.h) at +0x138/
 * +0x208 get their own `ringCtl`/`bufBase` fields populated here by
 * resolving the FIRST ("STG MIDI Out") and FOURTH ("STG->KG") rings'
 * own alloc handles through the same LocalHeapRegion() idiom already
 * established by NotifyNKS4TestMode() above -- `+0x208` is the exact
 * field CSTGGlobal::SubmitPerfChangeRequest()'s call chain dereferences.
 *
 * The 8-port (4 in + 4 out) registration loop and the final CPU-speed-
 * scaled timing-constant block are both confirmed real and reproduced
 * faithfully, even though `sMidiInPorts`/`sMidiOutPorts` are confirmed
 * all-NULL at every point in this project's own current boot-reachable
 * call graph (no reconstructed caller of RegisterMidiInPort/
 * RegisterMidiOutPort exists yet) -- the loop is therefore provably
 * dead code for now, not exercised beyond its own null checks.
 */
void CSTGMidiPortManager::Initialize()
{
	unsigned char *self = (unsigned char *)this;

	CSTGMidiQueue *qStgOut  = (CSTGMidiQueue *)(self + 0xc);
	CSTGMidiQueue *qKgReg   = (CSTGMidiQueue *)(self + 0x70);
	CSTGMidiQueue *qKgRt    = (CSTGMidiQueue *)(self + 0xd4);
	CSTGMidiQueue *qStgToKg = (CSTGMidiQueue *)(self + 0x140);
	CSTGMidiQueue *qKgToStg = (CSTGMidiQueue *)(self + 0x1a4);

	qStgOut->Initialize(0, 0x1000);
	qKgReg->Initialize(0, 0x400);
	qKgRt->Initialize(0, 0x80);

	/* +0x138/+0x13c: CSTGMidiQueueWriter wrapping the "STG MIDI Out"
	 * ring -- ringCtl = &qStgOut, bufBase = LocalHeapRegion(qStgOut's
	 * own alloc handle, stored at qStgOut+0x0 by Initialize() above). */
	unsigned int stgOutHandle = *(unsigned int *)((unsigned char *)qStgOut + 0x0);
	*(unsigned int *)(self + 0x138) = (unsigned int)(unsigned long)qStgOut;
	*(unsigned int *)(self + 0x13c) = (unsigned int)(unsigned long)LocalHeapRegion(stgOutHandle);

	qStgOut->SetDesc("STG MIDI Out");
	qKgReg->SetDesc("KG Regular MIDI Out");
	qKgRt->SetDesc("KG Real Time MIDI Out");

	qStgToKg->Initialize(1, 0x200);
	qKgToStg->Initialize(1, 0x100);

	/* +0x208/+0x20c: CSTGMidiQueueWriter wrapping the "STG->KG" ring --
	 * THE crash-fix field: CSTGGlobal::SubmitPerfChangeRequest()'s call
	 * chain reads *(CSTGMidiPortManager::sInstance+0x208) and
	 * dereferences it. */
	unsigned int stgToKgHandle = *(unsigned int *)((unsigned char *)qStgToKg + 0x0);
	*(unsigned int *)(self + 0x208) = (unsigned int)(unsigned long)qStgToKg;
	*(unsigned int *)(self + 0x20c) = (unsigned int)(unsigned long)LocalHeapRegion(stgToKgHandle);

	qStgToKg->SetDesc("STG->KG");
	qKgToStg->SetDesc("KG->STG");

	/* 8-port registration loop -- see this function's own header
	 * comment: confirmed dead code right now (all 8 port pointers are
	 * NULL this early in boot), reproduced faithfully rather than
	 * skipped. `region` is the same heap "test mode" region
	 * NotifyNKS4TestMode() resolves, holding 8 dedicated CSTGMidiQueue-
	 * sized slots (4 in + 4 out, 0x64-byte stride, confirmed matching
	 * NotifyNKS4TestMode()'s own first 4 offsets). */
	unsigned int testModeSlot = *(unsigned int *)(LocalHeapBase() + 8);
	unsigned char *region = (unsigned char *)LocalHeapRegion(testModeSlot);

	static const unsigned int kInOffsets[4]  = { 0x0, 0x64, 0xc8, 0x12c };
	static const unsigned int kOutOffsets[4] = { 0x190, 0x1f4, 0x258, 0x2bc };

	for (int i = 0; i < 4; i++) {
		void *inPort = sMidiInPorts[i];
		if (inPort != 0 && PortQuery(inPort))
			PortRegister(inPort, region + kInOffsets[i]);

		void *outPort = sMidiOutPorts[i];
		if (outPort != 0 && PortQuery(outPort))
			PortRegister(outPort, region + kOutOffsets[i]);
	}

	/* CPU-speed-scaled timing constants (confirmed real `.rodata.cst8`
	 * immediates 0.04/0.2) -- plausibly active-sensing-monitor timeout
	 * thresholds in CPU-cycle units; meaning beyond the literal values
	 * not independently determined. CSTGCPUInfo::sInstance is confirmed
	 * already constructed by this point (setup_global_resources's own
	 * Step 1, strictly before `engine->Initialize()` -- which calls this
	 * method -- runs). */
	float cyclesPerTick = CSTGCPUInfo::sInstance->field8;
	*(int *)(self + 0x4) = (int)(0.04f * cyclesPerTick);
	*(int *)(self + 0x8) = (int)(0.2f * cyclesPerTick);
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
