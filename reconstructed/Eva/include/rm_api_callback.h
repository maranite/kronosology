/*
 * rm_api_callback.h  -  CRMApiCallBack, the polymorphic completion-callback base
 * class `CResMan`/`CJobStack`'s `LoadRes`/`SetRes`/`LoadFile`/`Delete` family
 * (resman.h, not yet reconstructed) is declared to take a pointer to
 * (symbols.csv: `_ZN7CResMan6LoadResEhhhiP14CRMApiCallBack` etc.). Eva "size is
 * not depth" re-check batch, 2026-07-26 -- pulled in as the 3rd real base class
 * of `CBatchDiskMainTask` (batch_disk_main_task.h), confirmed via that ctor's
 * own vtable-pointer store at this+0x80 and matching `_ZThn128_` dtor thunks.
 *
 * GROUND TRUTH: all 5 named virtual hooks (.text+0x0818f1f0..0x0818f230,
 * `OnSetRes`/`OnLoadRes`/`OnLoad`/`OnSave`/`OnDelete`) are confirmed
 * BYTE-IDENTICAL empty bodies -- literally a single `ret` instruction each, no
 * other code, at the base-class level. `CRMApiCallBack`'s own ctor is itself
 * trivial enough that GCC inlined it at every construction site seen so far
 * (a single `mov [this], &vtable` store, no separate `call` instruction) --
 * modeled here as a normal default ctor with the same effect.
 *
 * REAL LAYOUT (from `~CRMApiCallBack()`, .text+0x0818fa20/0x0818fa80):
 *   +0x00  vtbl
 *   +0x04  mJob  CRMJob* (rm_job.h) -- owned; dtor calls `~CRMJob()` + `free()`
 *          (HAL_DisableInterrupts/EnableInterrupts-wrapped, this project's
 *          usual raw-malloc'd-object idiom) if non-null.
 * Total 8 bytes -- confirmed by `CBatchDiskMainTask`'s own ctor storing its
 * `CRMJob*` directly at this+4 relative to the CRMApiCallBack subobject
 * (absolute this+0x84, subobject starts at this+0x80).
 *
 * Real vtable slot layout for THIS class as a `CBatchDiskMainTask` base
 * (confirmed via direct `.rodata` reads at 0x08eabefc, the group-3
 * this-adjusting thunk table symbols.csv shows immediately follows the
 * CEditable-thunk group): 7 slots -- {~CRMApiCallBack (D1), ~CRMApiCallBack
 * (D0), OnSetRes, OnLoadRes, OnLoad, OnSave, OnDelete} -- matching this
 * class's own 7 named methods exactly. `CBatchDiskMainTask`'s own vtable
 * arrays (omega_vtables.h) install this group `EvaVTableStub`-backed, which
 * is not a compromise here -- ground truth's own real bodies for all 5 OnXxx
 * slots ARE the empty no-op `EvaVTableStub` already provides, confirmed
 * byte-for-byte, not merely assumed safe.
 */

#ifndef RM_API_CALLBACK_H
#define RM_API_CALLBACK_H

class CRMJob;

class CRMApiCallBack {
public:
	CRMApiCallBack() : mVtbl(0), mJob(0) {}

	/* .text+0x0818fa20/0x0818fa80. Real body -- see header comment. */
	~CRMApiCallBack();

	/* All 5 confirmed byte-identical empty bodies in ground truth (base
	 * class level -- see header comment). Not dispatched through a real
	 * vtable by any reconstructed caller; kept as plain methods matching
	 * this project's established "void *mVtbl field, not a real C++
	 * virtual" convention (out_link.h's COutLink, e.g.).
	 */
	void OnSetRes(int result) { (void)result; }
	void OnLoadRes(int result) { (void)result; }
	void OnLoad(int result) { (void)result; }
	void OnSave(int result) { (void)result; }
	void OnDelete(int result) { (void)result; }

protected:
	void   *mVtbl;
	CRMJob *mJob; /* +0x04 */
};

#endif /* RM_API_CALLBACK_H */
