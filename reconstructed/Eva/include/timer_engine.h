/*
 * timer_engine.h  -  CWheelsContainer, CExternalClock, CInternalClock, CTimerEngine.
 * Stage 6 breadth sweep, 2026-07-25 follow-up to the small-derived-module batch
 * (see seq_timer.h): unblocks `CSeqTimer::Setup()`'s own `CTimerEngine` dependency,
 * previously a Tier-B stub.
 *
 * GROUND TRUTH: `CTimerEngine::CTimerEngine(CModule const&)` (.text+0x0816bf40, 181
 * bytes) chains `CTask::CTask(owner, "Engine", 2, 1, 0x804b)` (already confirmed real
 * args, unchanged from the earlier stub), installs its own vtable + opaque +0x08
 * identity, sets an inferred int field at +0x7c (CTask's own real size boundary) to 1,
 * then default-constructs THREE embedded sub-objects in place: `CWheelsContainer`
 * (+0x80, 0x28 bytes), `CExternalClock` (+0xa8, 0x54 bytes), `CInternalClock` (+0xfc,
 * 0x18 bytes) -- each offset/size pair independently cross-checked against the NEXT
 * field/object the ctor touches (every boundary lines up exactly, including the
 * final one: 0x114 + 0x14 = 0x128, matching `CSeqTimer::Setup()`'s own
 * `malloc(0x128)` call size for the whole object -- confirms `sizeof(CTimerEngine)
 * == 0x128` exactly). After the 3 sub-objects, the ctor installs two trivial
 * "vtable-pointer-only" embedded interface slots at +0x11c (`PTR__CSyncRXInterface`)
 * and +0x120 (`PTR__CSyncTXInterface`), plus 2 small pointer/int bookkeeping fields
 * (+0x114/+0x118 self-pointers to the 2 interface slots, +0x124 a trailing int = 0)
 * -- these are NOT separately-constructed objects in the decompile (no ctor call),
 * just a raw vtable-pointer store each, matching the "install a vtable pointer with
 * no backing method-table dispatch on this pass's traced boot path" convention
 * already used for `CTask`'s own opaque +0x08 identity field.
 *
 * `CWheelsContainer`/`CExternalClock`/`CInternalClock` are NOT the deep ~24-method
 * sequencer/wheel engine this batch originally flagged as out of scope -- only their
 * ctor/dtor pairs are reconstructed here (everything CTimerEngine's own ctor/dtor
 * need); their further real methods (`CreateWheel`/`StartWheel`/`SetSyncInput`/
 * `RunVirtualTime`/... per `nm -C`) are genuinely deeper sequencer-engine logic, still
 * correctly deferred -- nothing on this project's traced boot path calls them.
 *
 * `CWheelsContainer` (.text+0x0816ddd0, 74 bytes ctor / 0x0816de20, 56 bytes dtor):
 * a fixed 8-slot pointer array (`mWheels[8]`, all null-initialized) + a capacity
 * constant (8) and a count (0) -- confirmed by the dtor's own loop, which iterates
 * `mCapacity` (re-read from `this+0` on every pass, not cached -- preserved verbatim)
 * times over `this+4+i*4`, virtual-dispatching slot+4 on each non-null entry. Since
 * nothing on this pass's traced path ever populates `mWheels` (no `CreateWheel`
 * caller reconstructed), the dtor's own virtual-dispatch branch is dead code here,
 * faithfully preserved but never actually exercised.
 *
 * `CExternalClock`/`CInternalClock` (.text+0x0816d070/0x0816ec60 ctors, 118/77 bytes;
 * .text+0x08195d30-40/0x08195e30-40 dtors, both a trivial vtable-pointer reset to a
 * SHARED `PTR__CClockBase_08e891e8` -- confirmed by a direct .rodata byte read:
 * `CClockBase`'s own real vtable is 6 slots, 2 real dtor slots + 4 identical
 * `__cxa_pure_virtual`-shaped PLT-stub entries (`0x0804c6ac`, repeated 4x) --
 * `CExternalClock`/`CInternalClock` each override those same 4 slots with their own
 * real methods, confirmed by their own vtables also being 6 slots long). Both derive
 * from an empty `CClockBase` (no data fields of its own -- confirmed since both
 * derived ctors' first field write lands at +4, immediately after the vtable
 * pointer) -- modeled here as standalone classes with a manual vtable-pointer-reset
 * dtor rather than a real shared C++ base, matching the project's "don't declare
 * genuine C++ `virtual`, install pointers by hand" convention.
 *
 * `CExternalClock::CExternalClock()`: vtable install, +4 (int) = 0, a virtual call
 * through Api's own vtable slot +0x9c (a real dispatch, same slot the CInternalClock
 * ctor also uses -- an "Api::GetDefaultXxx()"-shaped accessor, target not further
 * decoded) whose result is broadcast into 16 identical dwords at +0x10..+0x4c (a
 * fixed-size history/ring-buffer array, all seeded to the same starting value), then
 * +0x50 (int) = 0x10 (16, the array's own element count), +0x08 (u16) = 0, +0x0a
 * (u16) = 0x6666 (a fixed-point fraction, ~0.4 in Q16 terms -- plausible smoothing/
 * filter coefficient, not further decoded).
 *
 * `CInternalClock::CInternalClock()`: vtable install, +4 (int) = 1, +0x10 (int) = 0,
 * +0x14 (int) = 500000 (microseconds -- the standard default MIDI tempo, 120 BPM =
 * 500000us/quarter-note, matching this project's own BPM/MPQN default elsewhere),
 * the same Api vtable-slot-+0x9c call as CExternalClock stored at +8, and +0x0c (int)
 * = `(int)(0xbb800000 / (unsigned long long)500000) << 3` (a 64-bit division per
 * ground truth, evaluating to 50328/0xc498 given the default +0x14 -- exact
 * derivation/meaning of the 0xbb800000 constant not decoded, transcribed verbatim).
 */

#ifndef TIMER_ENGINE_H
#define TIMER_ENGINE_H

#include "task.h"

class CWheelsContainer {
public:
	/* .text+0x0816ddd0, 74 bytes. */
	CWheelsContainer();

	/* .text+0x0816de20, 56 bytes. See file header -- dead code on this pass's
	 * traced path (mWheels is always all-null), preserved faithfully.
	 */
	~CWheelsContainer();

private:
	int   mCapacity;     /* +0x00, ctor sets 8 */
	void *mWheels[8];     /* +0x04..+0x20 */
	int   mCount;         /* +0x24, ctor sets 0 */

	friend struct TimerEngineTestHooks;
};

class CExternalClock {
public:
	/* .text+0x0816d070, 118 bytes. See file header. */
	CExternalClock();

	/* .text+0x08195d30, 11 bytes (non-deleting dtor). Trivial vtable-pointer
	 * reset to the shared CClockBase vtable -- see file header.
	 */
	~CExternalClock();

private:
	void        *mVtbl;           /* +0x00 */
	int          mUnknown04;      /* +0x04, ctor sets 0 */
	unsigned short mSmoothingNum; /* +0x08, ctor sets 0, inferred name */
	unsigned short mSmoothingDen; /* +0x0a, ctor sets 0x6666, inferred name */
	int          mUnknown0c;      /* +0x0c, ctor sets 0 */
	int          mHistory[16];    /* +0x10..+0x4c, all seeded to Api's +0x9c result */
	int          mHistoryCount;   /* +0x50, ctor sets 0x10 */

	friend struct TimerEngineTestHooks;
};

class CInternalClock {
public:
	/* .text+0x0816ec60, 77 bytes. See file header. */
	CInternalClock();

	/* .text+0x08195e30, 11 bytes (non-deleting dtor). Trivial vtable-pointer
	 * reset to the shared CClockBase vtable -- see file header.
	 */
	~CInternalClock();

private:
	void *mVtbl;      /* +0x00 */
	int mUnknown04;   /* +0x04, ctor sets 1 */
	int mUnknown08;   /* +0x08, ctor sets Api's own +0x9c vtable-call result */
	int mUnknown0c;   /* +0x0c, ctor sets (int)(0xbb800000/(u64)mPeriodUs) << 3 */
	int mUnknown10;   /* +0x10, ctor sets 0 */
	int mPeriodUs;    /* +0x14, ctor sets 500000 (default MIDI tempo period) */

	friend struct TimerEngineTestHooks;
};

/* Tier B until this batch, now real -- see file header for the full field-offset
 * accounting (every boundary independently cross-checked, ending at the real
 * malloc(0x128) size CSeqTimer::Setup() already uses).
 */
class CTimerEngine : public CTask {
public:
	/* .text+0x0816bf40, 181 bytes. Real body -- see file header. */
	explicit CTimerEngine(const CModule &owner);

	/* .text+0x0816be90, 108 bytes (D0 deleting dtor's own non-deleting half);
	 * .text+0x0816be00, 90 bytes is the plain D2 non-deleting dtor this
	 * mirrors. Real body -- see file header (manual vtable pokes for the 2
	 * embedded clock sub-objects, then CWheelsContainer's own dtor, then the
	 * base CTask dtor via normal C++ member/base teardown).
	 */
	~CTimerEngine();

private:
	int              mUnknown7c;    /* +0x7c, ctor sets 1 */
	CWheelsContainer mWheels;       /* +0x80 */
	CExternalClock   mExtClock;     /* +0xa8 */
	CInternalClock   mIntClock;     /* +0xfc */
	void            *mRXInterfacePtr; /* +0x114, self-pointer to mRXVtbl's address */
	void            *mTXInterfacePtr; /* +0x118, self-pointer to mTXVtbl's address */
	void            *mRXVtbl;       /* +0x11c, PTR__CSyncRXInterface, vtable-ptr only */
	void            *mTXVtbl;       /* +0x120, PTR__CSyncTXInterface, vtable-ptr only */
	int              mUnknown124;   /* +0x124, ctor sets 0 */

	friend struct TimerEngineTestHooks;
};

#endif /* TIMER_ENGINE_H */
