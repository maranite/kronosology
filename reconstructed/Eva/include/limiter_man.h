/*
 * limiter_man.h  -  CLimiterMan, a tiny sub-object every CTask embeds at CTask+0x60
 * (Stage 6 breadth sweep, 2026-07-25 -- CTask::CTask() reconstruction batch).
 *
 * Real layout confirmed from CLimiterMan@0807bd10.c (the only reconstructed method) and
 * cross-confirmed by CTask's own ctor byte offsets (task.h): 0x18 bytes --
 *   +0x00  vtbl        PTR__CLimiterMan_08e81ee8 (omega_vtables.h, 4 slots), installed
 *                       by this ctor
 *   +0x04  mOwnerTask  back-pointer to the owning CTask (the ctor's own param_1),
 *                       never read back by any reconstructed method
 *   +0x08  vtbl2       PTR__TVector_08e81f78 (omega_vtables.h, 2 slots) -- an embedded
 *                       TVector<CLimiterBase*,1>, installed by this ctor
 *   +0x0c  mBegin      TVector begin pointer, ctor zeroes
 *   +0x10  mEnd        TVector end pointer, ctor zeroes
 *   +0x14  mCap        TVector capacity-end pointer, ctor zeroes
 *
 * ~CLimiterMan() (.text+0x0807bbc0, 97 bytes, the D1 complete-object destructor) is
 * NOW reconstructed too (Stage 6 SetMask/~CTask batch, 2026-07-25 -- CTask::~CTask()'s
 * own real caller, task.cpp, needed it). Real body: installs CLimiterMan's own vtable,
 * walks the embedded TVector<CLimiterBase*,1> range [mBegin, mEnd), and for each
 * non-null element calls a virtual method THROUGH THE ELEMENT'S OWN vtable at slot+4
 * (`(*(code**)(*elem+4))(elem)`, presumably a `Release()`-shaped method on a
 * not-reconstructed `CLimiterBase` -- opaque, same license as every other "virtual
 * call through an unreconstructed class's own vtable" already in this project),
 * reinstalls the TVector's own vtable (0x8e81f78, same value, matching the
 * "re-assert own identity right before the inherited-from-COmegaPtrArray-shaped-cleanup
 * one" idiom every other destructor in this project reconstructs the same way), frees
 * the backing array, then installs the FINAL identity 0x8e81d80 -- confirmed via nm to
 * be `vtable for CIfcUnknown` + 8, i.e. CLimiterMan's own further base is CIfcUnknown
 * (omega_vtables.h), matching CTask::CTask()'s own
 * `RegisterIfc(reinterpret_cast<CIfcUnknown*>(mLimiterMan))` call byte-for-byte.
 *
 * Still NOT declared as a real C++ base/member of CTask for the same reason CModule's
 * own mTasks stays a raw byte buffer (module.h) -- CTask constructs AND destroys this
 * via placement-new/explicit dtor call into a raw byte buffer instead (task.cpp),
 * matching the project's established manual-sub-object-construction idiom.
 */

#ifndef LIMITER_MAN_H
#define LIMITER_MAN_H

class CTask;

class CLimiterMan {
public:
	/* .text+0x0807bd10, 46 bytes (symbols.csv: _ZN11CLimiterManC1ER5CTask). */
	CLimiterMan(CTask *owner);

	/* .text+0x0807bbc0, 97 bytes (D1 complete-object destructor). See header
	 * comment. mBegin==mEnd always in this reconstruction (nothing calls
	 * RegisterLimiter(), not reconstructed -- out of scope), so the per-element
	 * release loop never actually fires here; only the free()+vtbl-teardown tail
	 * is exercised.
	 */
	~CLimiterMan();

private:
	void *mVtbl;
	CTask *mOwnerTask;
	void  *mVtbl2;
	void  *mBegin;
	void  *mEnd;
	void  *mCap;
};

#endif /* LIMITER_MAN_H */
