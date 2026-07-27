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
 *
 * DEFENSIVE NULL-GUARD (2026-07-27, integration-boot regression, ROOT
 * CAUSE NOW FOUND AND FIXED): this loop (below) was genuinely dead code
 * until the same-day fix that made `ConstructKorgUsbMidiPorts()` run for
 * real during `init_module()` (see that function's own comment,
 * midi_korgusb_port.cpp) -- before that, `sMidiInPorts[]`/
 * `sMidiOutPorts[]` were always all-NULL and this dispatch never
 * actually ran. Now live for the first time, it hit TWO separate real
 * crashes on a live kronos_vm boot: (1) a literal null FUNCTION POINTER
 * in `CSTGMidiInPortKorgUsb`'s still-placeholder vtable (fixed
 * separately, see that array's own comment) and (2) this port's OWN
 * vtable POINTER FIELD reading back NULL for `CSTGMidiOutPortKorgUsb`
 * instances.
 *
 * (2) is now root-caused: confirmed via direct `objdump -dr -M intel`
 * against ground truth OA.ko that BOTH `CSTGMidiOutPort::CSTGMidiOutPort()`
 * (`.text+0xf8270`) and `CSTGMidiOutPortKorgUsb::CSTGMidiOutPortKorgUsb()`
 * (`.text+0x340650`) write this field as their own first/last act
 * respectively (`mov [this],0x8` + an `R_386_32` relocation to
 * `_ZTV15CSTGMidiOutPort`/`_ZTV22CSTGMidiOutPortKorgUsb`) -- this
 * project's own hand-modeled, non-`virtual` `vtable` field (see
 * oa_engine_init.h's class comment for why real C++ `virtual` was
 * reverted) simply never had an equivalent assignment anywhere in either
 * reconstructed ctor. Fixed in `midi_korgusb_port.cpp`: a real
 * `_ZTV22CSTGMidiOutPortKorgUsb[9]` array (every slot a genuine,
 * already-reconstructed method -- no stubs needed) is now written into
 * `outPort+0x00` immediately after `Construct()`'s placement-new,
 * matching the InPort side's own established precedent 2 lines up in
 * that same file. `CSTGMidiOutPortKorgUsb` is the ONLY currently-live
 * `CSTGMidiOutPort`-family construction site in this project (the
 * physical-DIN `CSTGMidiOutPortSerial` class has no construction site at
 * all yet) -- so this fully resolves the crash for every port this loop
 * can currently reach.
 *
 * This guard is kept anyway as cheap defense-in-depth, NOT because a
 * correctly-constructed instance is ever genuinely supposed to have a
 * null `vtable` field (ground truth's own ctors write it unconditionally
 * as literally their first act) -- this exact "reconstructed ctor
 * transcribes every field write except the vtable pointer" mistake has
 * now recurred twice in this project (this bug, and `CSTGDrumPadClient`'s
 * `.init_array` vtable, `87e446d`), so a future incomplete
 * `CSTGMidiOutPortSerial` (or similar) construction reintroducing it
 * fails safe instead of Oopsing.
 */
static bool PortQuery(void *port)
{
	typedef bool (*Fn)(void *);
	void **vtable = *(void ***)port;
	if (!vtable)
		return false;
	return ((Fn)vtable[0])(port);
}
static void PortRegister(void *port, void *region)
{
	typedef void (*Fn)(void *, void *);
	void **vtable = *(void ***)port;
	if (!vtable)
		return;
	((Fn)vtable[1])(port, region);
}

/* WORKAROUND (2026-07-24): these used to re-derive oa_heap_base()/
 * oa_heap_region()'s own raw-offset formula locally (matching
 * setup_global_resources.cpp's own pre-fix precedent) -- but a live
 * kronos_vm boot proved that formula's underlying data (heapBase and
 * each handle-table entry's own `offset` field) is not reliably
 * readable by any caller other than CSTGHeapManager::Initialize()/
 * Alloc() themselves. See heap_manager.cpp's own file comment for the
 * full investigation. Now reads the same captured-value snapshots
 * setup_global_resources.cpp uses instead of re-deriving the broken
 * formula independently. */
extern "C" unsigned long CSTGHeapManager_GetCapturedHeapBase(void);
extern "C" unsigned int CSTGHeapManager_GetCapturedOffset(unsigned int slot);

static char *LocalHeapBase()
{
	char *heap = CSTGHeapManager::sInstance;
	if (heap == (char *)(long)-44)		/* 0xFFFFFFD4 sentinel: heap not yet up */
		return 0;
	/* Slot 1 (the very first CSTGHeapManager::Alloc() call of the whole
	 * boot) is what oa_heap_base()'s own `*(heap+0x38)` term structurally
	 * resolves to -- see setup_global_resources.cpp's own local_heap_base()
	 * comment for the derivation. */
	return (char *)(CSTGHeapManager_GetCapturedOffset(1) +
			(unsigned int)CSTGHeapManager_GetCapturedHeapBase());
}

static char *LocalHeapRegion(unsigned int slot)
{
	if (slot >= 100000)
		return 0;
	return (char *)(CSTGHeapManager_GetCapturedOffset(slot) +
			(unsigned int)CSTGHeapManager_GetCapturedHeapBase());
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

/*
 * ~CSTGMidiPortManager() (batch 57, .text+0xf5280, 264 bytes) -- confirmed
 * real, disassembly-derived. Genuinely reached: engine.cpp's own
 * `CSTGEngine::~CSTGEngine()` calls it directly
 * (`if (sInstance) sInstance->~CSTGMidiPortManager();`), not merely a
 * dead-code destructor. Operates ENTIRELY on the two static port-table
 * arrays (matches this class's own oa_engine.h header comment: no
 * per-instance state at all) -- confirmed via the real disassembly's own
 * absolute `mov eax,ds:CONST`-style addressing of `sMidiInPorts`/
 * `sMidiOutPorts`, never `[this+OFFSET]`.
 *
 * For each of the 4 slots, IN-PORT THEN OUT-PORT (confirmed real
 * interleave order from the disassembly -- NOT "all 4 in-ports then all
 * 4 out-ports"): if the slot is non-NULL AND its own "active" flag bit
 * (bit1) is set, dispatches through vtable slot 2 on that port object --
 * one slot further than PortQuery()/PortRegister() above, presumably each
 * port's own virtual destructor (not independently confirmed beyond the
 * call shape itself). The in-port check uses CSTGMidiInPort's own
 * confirmed `flags` field (+0x26, oa_engine.h); the out-port check is
 * this project's first confirmed real use of CSTGMidiOutPort's own +0x5
 * byte (previously unconfirmed padding there -- now named `flags`,
 * oa_engine_init.h, see that struct's own updated comment). Finally
 * zeroes `sInstance` unconditionally, real or not.
 */
static void PortDestroy(void *port)
{
	typedef void (*Fn)(void *);
	void **vtable = *(void ***)port;
	((Fn)vtable[2])(port);
}

CSTGMidiPortManager::~CSTGMidiPortManager()
{
	for (int i = 0; i < 4; i++) {
		CSTGMidiInPort *inPort = (CSTGMidiInPort *)sMidiInPorts[i];
		if (inPort != 0 && (inPort->flags & 0x2))
			PortDestroy(inPort);

		CSTGMidiOutPort *outPort = (CSTGMidiOutPort *)sMidiOutPorts[i];
		if (outPort != 0 && (outPort->flags & 0x2))
			PortDestroy(outPort);
	}

	sInstance = 0;
}
