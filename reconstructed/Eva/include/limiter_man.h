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
 * Only the constructor is reconstructed here -- ~CLimiterMan() (.text+0x0807bbc0, a
 * real per-element vtable-slot+4 "release" walk over the TVector plus a free() of its
 * backing array) is NOT reconstructed: nothing in this reconstruction's own call graph
 * ever invokes ~CTask() (no destruction path traced from any boot-path caller), so
 * there is nothing to exercise it against yet -- same "construct, don't destruct"
 * scope already established for every other embedded sub-object in this project
 * (COmegaPtrArray's own RemoveAll()/SetAtIndex(), CTaskBuffer's producer side, etc.).
 * Not declared as a real C++ base/member of CTask for the same reason CModule's own
 * mTasks stays a raw byte buffer (module.h) -- CTask doesn't declare a dtor either, so
 * a real C++ member here would silently generate one Ghidra's own binary never had a
 * caller for. CTask constructs this via placement-new into a raw byte buffer instead
 * (task.cpp), matching the project's established manual-sub-object-construction idiom.
 */

#ifndef LIMITER_MAN_H
#define LIMITER_MAN_H

class CTask;

class CLimiterMan {
public:
	/* .text+0x0807bd10, 46 bytes (symbols.csv: _ZN11CLimiterManC1ER5CTask). */
	CLimiterMan(CTask *owner);

private:
	void *mVtbl;
	CTask *mOwnerTask;
	void  *mVtbl2;
	void  *mBegin;
	void  *mEnd;
	void  *mCap;
};

#endif /* LIMITER_MAN_H */
