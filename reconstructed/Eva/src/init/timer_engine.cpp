/*
 * timer_engine.cpp  -  see include/timer_engine.h.
 *
 * Real vtable content confirmed by direct .rodata byte reads (not the naive
 * "next symbol" heuristic -- typeinfo/typeinfo-name objects for several classes are
 * interleaved between adjacent vtables in this region of .rodata, same trap already
 * documented for CApiDescriptor/CResMan):
 *   PTR__CTimerEngine_08e896c8      7 slots (standard CTask-derived shape, matches
 *                                   the DAT_08e896e4 opaque-secondary-vtable boundary
 *                                   already used throughout this project)
 *   PTR__CExternalClock_08e897a8    6 slots (2 real dtor + 4 real overrides)
 *   PTR__CInternalClock_08e89888    6 slots (2 real dtor + 4 real overrides)
 *   PTR__CClockBase_08e891e8        6 slots (2 real dtor + 4 IDENTICAL PLT-stub
 *                                   entries at 0x0804c6ac -- a real __cxa_pure_virtual
 *                                   shape; CExternalClock/CInternalClock each
 *                                   override those same 4 slots with real methods)
 *   PTR__CSyncRXInterface_08e89748  5 slots (2 real dtor + 3 real methods)
 *   PTR__CSyncTXInterface_08e89198  10 slots (4 real: 2 dtor + 2 methods; the
 *                                   remaining 6 are literal zero words in the raw
 *                                   .rodata bytes, not a repeated PLT-stub pattern
 *                                   like CClockBase's own pure-virtual slots -- exact
 *                                   cause not resolved (possibly an as-yet-unapplied
 *                                   relocation class this project hasn't otherwise
 *                                   hit), left as EvaVTableStub like the rest since
 *                                   nothing dispatches through this vtable on the
 *                                   traced boot path either way)
 * None of these 6 vtables are ever dispatched through by any reconstructed code on
 * this pass's traced boot path -- all EvaVTableStub-backed, install-only, same
 * status as every other never-dispatched vtable in this project.
 */

#include "timer_engine.h"
#include "omega_vtables.h"
#include "system_api.h"

extern CSystemApi *Api; /* mains.cpp */

namespace {

/* Real Api+0x9c vtable call both CExternalClock/CInternalClock's own ctors make --
 * a real dispatch, target/semantics not further decoded (an "Api::GetDefaultXxx()"
 * shaped accessor). EvaVTableStub-backed like every other unconfirmed Api slot.
 */
inline int ApiGetDefault9c()
{
	typedef int (*Fn)(void *);
	Fn fn = (Fn)(((void **)*(void **)Api)[0x9c / 4]);
	return fn(Api);
}

} /* namespace */

/* ===== CWheelsContainer ===== */

CWheelsContainer::CWheelsContainer()
	: mCapacity(8), mCount(0)
{
	for (int i = 0; i < 8; ++i)
		mWheels[i] = 0;
}

CWheelsContainer::~CWheelsContainer()
{
	/* Real body: re-reads mCapacity every iteration (not cached across the
	 * virtual call) -- preserved verbatim even though dead on this pass's
	 * traced path (mWheels is always all-null, so the vcall branch never
	 * actually fires here).
	 */
	unsigned int count = (unsigned int)mCapacity;
	for (unsigned int i = 0; i < count; ++i) {
		void *wheel = mWheels[i];
		if (wheel) {
			typedef void (*Fn)(void *);
			Fn fn = (Fn)(((void **)*(void **)wheel)[1]);
			fn(wheel);
			count = (unsigned int)mCapacity;
		}
	}
}

/* ===== CExternalClock ===== */

CExternalClock::CExternalClock()
	: mVtbl(0), mUnknown04(0), mSmoothingNum(0), mSmoothingDen(0x6666),
	  mUnknown0c(0), mHistoryCount(0x10)
{
	mVtbl = (void *)PTR__CExternalClock_08e897a8;

	int seed = ApiGetDefault9c();
	for (int i = 0; i < 16; ++i)
		mHistory[i] = seed;
}

CExternalClock::~CExternalClock()
{
	mVtbl = (void *)PTR__CClockBase_08e891e8;
}

/* ===== CInternalClock ===== */

CInternalClock::CInternalClock()
	: mVtbl(0), mUnknown04(1), mUnknown08(0), mUnknown0c(0), mUnknown10(0),
	  mPeriodUs(500000)
{
	mVtbl = (void *)PTR__CInternalClock_08e89888;

	mUnknown08 = ApiGetDefault9c();
	mUnknown0c = (int)((unsigned long long)0xbb800000ULL / (unsigned)mPeriodUs) << 3;
}

CInternalClock::~CInternalClock()
{
	mVtbl = (void *)PTR__CClockBase_08e891e8;
}

/* ===== CTimerEngine ===== */

CTimerEngine::CTimerEngine(const CModule &owner)
	: CTask(owner, "Engine", 2, 1, 0x804b), mUnknown7c(1),
	  mRXInterfacePtr(0), mTXInterfacePtr(0), mRXVtbl(0), mTXVtbl(0), mUnknown124(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CTimerEngine_08e896c8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		&EvaDataPlaceholder_08e896e4;

	/* mWheels/mExtClock/mIntClock already default-constructed by the member
	 * initializer list above (matches ground truth's own in-place placement
	 * ctor calls at +0x80/+0xa8/+0xfc, no malloc involved -- embedded
	 * sub-objects).
	 */

	mRXVtbl = (void *)PTR__CSyncRXInterface_08e89748;
	mTXInterfacePtr = &mTXVtbl;
	mTXVtbl = (void *)PTR__CSyncTXInterface_08e89198;
	mRXInterfacePtr = &mRXVtbl;
}

CTimerEngine::~CTimerEngine()
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CTimerEngine_08e896c8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		&EvaDataPlaceholder_08e896e4;
	mTXVtbl = (void *)PTR__CSyncTXInterface_08e89198;
	mRXVtbl = (void *)PTR__CSyncRXInterface_08e89748;
	/* mIntClock/mExtClock/mWheels/CTask base dtors all run automatically via
	 * normal C++ member/base teardown (reverse declaration order), matching
	 * ground truth's own explicit +0xfc/+0xa8 CClockBase vtable pokes (inlined
	 * into CExternalClock::~CExternalClock()/CInternalClock::~CInternalClock()
	 * -- see file header) then CWheelsContainer::~CWheelsContainer() then
	 * CTask::~CTask().
	 */
}

/* Real vtable definitions -- moved here from omega_vtables.cpp, same "define
 * locally where the real forwarders/owners live" precedent as chunk_man.cpp/
 * seq_timer.cpp.
 */
void *PTR__CTimerEngine_08e896c8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CExternalClock_08e897a8[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CInternalClock_08e89888[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CClockBase_08e891e8[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CSyncRXInterface_08e89748[5] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CSyncTXInterface_08e89198[10] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* Opaque data placeholder CTimerEngine's own ctor stores the ADDRESS of at +0x08
 * (mIfcThunk) -- never dereferenced by any reconstructed code, same treatment as
 * EvaDataPlaceholder_08e82144 (omega_vtables.cpp).
 */
int EvaDataPlaceholder_08e896e4;
