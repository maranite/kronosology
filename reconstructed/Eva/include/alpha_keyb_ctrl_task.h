/*
 * alpha_keyb_ctrl_task.h  -  CAlphaKeybCtrlTask, CAlphaKeybCtrl::Setup()'s own real
 * construction target (Eva CAlphaKeybCtrl/CAlphaKeybCtrlTask batch, 2026-07-26).
 *
 * Flagged (not executed) by an earlier same-day mop-up pass
 * ([[eva_mopup_sweep_2026-07-26_negative]]) purely on the ctor's raw size (4289
 * bytes, "an order of magnitude bigger than anything this project currently calls
 * tractable"). Re-investigated properly this batch: the ctor's real bulk is NOT
 * algorithmic depth -- it is GCC fully inlining the SAME ~60-line "build one
 * CKeyboardLayout" sequence 15 times (see keyboard_layout.h), a mechanical,
 * table-driven pattern once un-inlined back into a single small constructor called
 * 15 times with different data. The genuinely deep, out-of-scope piece turned out to
 * be much smaller and off to the side: a single `COutLinkIfcBase`/`COutLinkIfc<T>`/
 * `CMarshaller<T>` interface-link sub-object the ctor constructs once (see
 * "AlphaKeybCode interface link" below).
 *
 * REAL CLASS SHAPE (CAlphaKeybCtrlTask : public CTask, confirmed via `nm -C`/
 * `objdump -dr -M intel` against `/home/share/Decomp/EVA_Decomp/Eva`, matching
 * PLAN.md's own ground-truth path):
 *   +0x00..0x7c  CTask base subobject (task.h)
 *   +0x7c  mHidResource  void* -- a named-resource pointer fetched via
 *                        `Api->vtbl[0xac](Api, hidDrvName)` (the SAME named-resource
 *                        lookup call shape `CPoller::CPoller()` already established,
 *                        poller.h/`LookupResourceStub`), kept only if the resource's
 *                        own vtbl+0x10 slot reports type `10` AND a subsequent
 *                        vtbl+8 call (writing a byte out-param) succeeds. Always
 *                        stays NULL under this reconstruction's own `LookupResourceStub`
 *                        (returns NULL unconditionally, poller.cpp) -- same
 *                        "structurally faithful, quiescent under the current Api
 *                        stub" status as `CPoller`'s own analogous field. Real
 *                        vtable-slot shape (not individually named -- opaque,
 *                        raw-offset dispatch only, matching this project's
 *                        established "no real C++ virtual for ground-truth vtable
 *                        slots" rule): +0x08 (called with a byte out-param during
 *                        construction), +0x0c (dtor's own "release" call, arg 0),
 *                        +0x10 (type-id query), +0x1c (Exec()'s own notify call),
 *                        +0x20 (Exec()'s own "overcurrent iteration" query), +0x28
 *                        (Exec()'s own "has event" query).
 *   +0x80  mCodeIfc      void* -- the "AlphaKeybCode" interface-link sub-object.
 *                        See "AlphaKeybCode interface link" below.
 *   +0x84  mUnknown84    int, ctor sets 1. No consumer found anywhere in this
 *                        class's own reconstructed methods (Exec/Initialize/
 *                        SetCtrlCondition/ProcessEvent/dtor) -- real meaning not
 *                        decoded, faithfully preserved regardless.
 *   +0x88  mUnknown88    int, ctor sets 0xb (11). Same "no consumer found, real
 *                        meaning not decoded" status as mUnknown84.
 *   +0x8c  mUnknown8c    int, ctor zeroes (tail of ctor, after the layout-vector
 *                        setup). Same status.
 *   +0x90  mUnknown90    int, ctor zeroes (tail). Same status.
 *   +0x94  mUnknown94    int, ctor zeroes (tail). Same status.
 *   +0x98  mUnknown98    unsigned short, ctor zeroes (near the START of the ctor,
 *                        before the resource lookup). Same status.
 *   +0x9c  mUnknown9c    int, ctor zeroes (same early point as mUnknown98).
 *   +0xa0  mLayoutList      LayoutVector (0x10 bytes) -- real ground truth:
 *                        `TVector<CKeyboardLayout*,1>`. Holds the Default layout
 *                        always, plus (conditionally, see ctor) the 13 international
 *                        layouts.
 *   +0xb0  mAsciiLayoutList LayoutVector (0x10 bytes) -- holds Custom ASCII only,
 *                        unconditionally.
 * Real total size 0xc0 (192 bytes), confirmed directly from
 * `CAlphaKeybCtrl::Setup()`'s own `malloc(0xc0)` call site (alpha_keyb_ctrl.h).
 *
 * "AlphaKeybCode interface link" (mCodeIfc, +0x80): the ctor `malloc(0x50)`s a
 * `COutLinkIfcBase`-shaped object, base-constructs it as a real `COutLink`
 * (owner=*this, name="AlphaKeybCode", direction=eDirectionOut, mode=0x804b,
 * lastArg=1 -- out_link.h, reused directly, real code), then does 3 further raw
 * writes ground truth's own (un-reconstructed) `COutLinkIfcBase`/
 * `COutLinkIfc<IAlphaKeybCode>`/`CMarshaller<IAlphaKeybCode>` constructors perform:
 * a secondary "CMsgSender"-shaped thunk-vtable install at +0x34 (real target
 * `&DAT_08eabd70`, itself a small Itanium-ABI-shaped thunk table -- confirmed via a
 * direct `.rodata` read, not modeled), the ctor's own `interfaceId`/`unmarshallFn`
 * args at +0x38/+0x3c, a `CMarshaller<IAlphaKeybCode>` sub-object install at +0x48
 * (real target `&PTR__CMarshaller_08e89f18`, confirmed via its own adjacent
 * `.rodata` typeinfo-name string "11CMarshallerI14IAlphaKeybCodeE" to be the exact
 * specialization for this type, NOT a shared generic base as Ghidra's own symbol
 * name misleadingly suggests), and a self-referential back-pointer at +0x4c
 * (`this_01+0x4c = this_01+0x34`). Then `CTask::Add(this_01)` (already-real,
 * task.h) registers it into the base class's own `mOutLinks` array.
 *
 * DELIBERATELY NOT MODELED AS A REAL C++ CLASS -- genuinely disproportionate depth,
 * same bar as `CAlphaKeybIfcTask::ProcessCode()` (alpha_keyb_ifc_task.h) and
 * `CKeyboardLayoutManager::AddLayout()`/`GetLayout()` (locale_manager.h): the full
 * `COutLinkIfcBase`/`COutLinkIfc<T>`/`CMarshaller<T>` family is a real Itanium-ABI
 * multiple-inheritance-with-vtable-adjustment-thunk hierarchy (confirmed via direct
 * `.rodata` reads of the real thunk tables -- offset-to-top/typeinfo header words
 * before the real function-pointer array, this-adjusting `-0x34`/`-8` thunk dtor
 * variants in `functions.csv`), used by several OTHER un-reconstructed subsystems
 * too (`ILimiterNotify`, `IAlphaKeybEvent`, `IAlphaKeybCtrl` all have their own
 * `CMarshaller<T>`/`COutLinkIfc<T>` instantiations per `functions.csv` -- reconstructing
 * it properly means reconstructing a shared framework, not a one-off). Modeled here
 * purely as an opaque, malloc'd raw buffer a private helper function pokes at the
 * SAME byte offsets ground truth does, with placeholder (`EvaVTableStub`-backed,
 * install-only) vtables at the 2 identities that matter for THIS class's own call
 * shapes -- matching this project's "raw offset, not a real class" convention already
 * used for e.g. `CTask`'s own `mIfcThunk`/`mLimiterMan` sub-objects (task.h).
 *
 * `Exec()`, `Initialize()`, and `ProcessEvent()` (below) all reference mCodeIfc
 * through this raw-offset shape, reproducing ground truth's own real call sites
 * exactly (including `COutLinkIfcBase::GetDirectIfcPtr()`, fully reconstructed for
 * real as a free function since it only touches mCodeIfc's own self-contained
 * +0x40/+0x44 fields -- see .cpp). The one further dispatch `ProcessEvent()` makes
 * THROUGH mCodeIfc's own `CMarshaller<IAlphaKeybCode>` sub-object (its real target,
 * `CMarshaller<IAlphaKeybCode>::ProcessCode()`, itself forwards into the SAME
 * `COutLinkIfcBase::SendWithAnswer()`-family marshalling machinery that is out of
 * scope) resolves to the placeholder `EvaVTableStub` -- safe, since `ProcessEvent()`'s
 * own real body discards that call's return value unconditionally (confirmed via
 * direct disassembly reading, not assumed).
 */

#ifndef ALPHA_KEYB_CTRL_TASK_H
#define ALPHA_KEYB_CTRL_TASK_H

#include "task.h"

class CModule;
class CKeyboardLayout;

/* Minimal local shape for `IAlphaKeybEvent::SKeyboardEvt`, matching hid_driver.h's
 * own `AlphaKeybEvt` byte-for-byte (isKeyDown@0, unused8@4, keycode@0xc,
 * modifiers@0x10) -- `CAlphaKeybCtrlTask::ProcessEvent()`'s own real argument type,
 * confirmed to be the exact same struct `CHIDDriver::GetKeyboardEvent()` fills in
 * (hid_driver.h). Not #include-d from there directly to avoid a hw<->hw cross
 * dependency for a 20-byte POD shape; field order/offsets must stay in sync if
 * either is ever changed.
 */
struct SKeyboardEvt {
	unsigned int  isKeyDown;  /* +0x00 */
	unsigned int  unused8;    /* +0x04 */
	unsigned int  keycode;    /* +0x0c */
	unsigned char modifiers;  /* +0x10 */
};

class CAlphaKeybCtrlTask : public CTask {
public:
	/* .text+0x0823f2a0, 4289 bytes. See header comment -- mechanically table-
	 * driven, not algorithmically deep. `owner` is the real ctor's `CModule
	 * const&`; `hidDrvName` is `CAlphaKeybCtrl::Setup()`'s own
	 * `mParam.GetParamStr("HIDDRV")` result (may be NULL).
	 */
	CAlphaKeybCtrlTask(const CModule *owner, const char *hidDrvName);

	/* .text+0x0823e9d0, 1199 bytes (D1 complete-object destructor). Frees both
	 * LayoutVectors' own elements + backing arrays, releases mHidResource (if
	 * set) via its own vtbl+0xc, then falls through to `CTask::~CTask()`
	 * (already-real, task.h). mCodeIfc is NOT freed here -- ground truth's own
	 * real behavior: it was registered into the base class's `mOutLinks` array
	 * via `CTask::Add()`, and `CTask::~CTask()`'s own generic mOutLinks-drain
	 * loop notifies-and-removes each element WITHOUT calling its own dtor/delete
	 * (task.h's own documented gap, matching `out_link.h`'s "OutLinkTestHooks"
	 * precedent) -- a real, faithfully-preserved ground-truth leak, not an
	 * omission of this reconstruction's own.
	 */
	~CAlphaKeybCtrlTask();

	/* .text+0x0823e930, 152 bytes. Real: if mHidResource is NULL, `SetMask(1);
	 * return -1;` (always this branch under this reconstruction's own
	 * `LookupResourceStub`). Otherwise: query mHidResource's own vtbl+0x28 for
	 * "has event"; if a control-condition byte-set succeeds via vtbl+0x1c,
	 * self-dispatch `ProcessEvent()` through THIS object's own vtable slot 5
	 * (matching ground truth's real raw `(**(code**)(*this+0x14))(this,...)`
	 * call, reproduced via `PTR__CAlphaKeybCtrlTask_08eabcc8`, not a direct
	 * C++ call -- same convention `CHIDDriver::GetKeyboardEvent()` already
	 * established for its own self-dispatch through slot 5, hid_driver.h);
	 * then query vtbl+0x20 for an "overcurrent iteration" trace log.
	 */
	int Exec();

	/* .text+0x0823ef50, 83 bytes. Real: a single Api vtbl+0x44 diagnostic-
	 * registration call (`Api->vtbl[0x11](Api, mOwnerModule->GetName(),
	 * mName, mCodeIfc's own COutLink::GetName(), "Editor",
	 * "AlphaKeybIfcTask", 0)` -- return value discarded). Both pointers
	 * dereferenced are always valid (mOwnerModule set unconditionally by the
	 * base CTask ctor; mCodeIfc always constructed by this class's own ctor),
	 * so this is safe to transcribe literally even though the target Api slot
	 * is a placeholder.
	 */
	void Initialize();

	/* .text+0x0823efc0, 268 bytes. Real: a small static-local bitmask
	 * (`s_iStatusBits`) tracking which of 4 "sticky" keys (Alt/Shift/Ctrl/
	 * CapsLock-ish, keycodes 'X'/';'/'L'/'a') are currently latched, with
	 * slightly different toggle-vs-set semantics depending on whether `down`
	 * is true. Fully self-contained bit logic, no external dependency --
	 * transcribed byte-for-byte from the real switch/branch structure.
	 */
	static unsigned int SetCtrlCondition(unsigned char keycode, bool down);

	/* .text+0x0823f0f0, 374 bytes. Real: decodes `evt`'s own modifier byte into
	 * a locale-table row index, looks up `CLocaleManager::GetInstance()->
	 * GetKeyboardLayout(0x8409)` (ALWAYS the Default layout, a real,
	 * faithfully-preserved ground-truth literal -- not a bug, see
	 * keyboard_layout.h), and on success builds an `IAlphaKeybCode::
	 * SKeyboardCode` and dispatches it through mCodeIfc (see header comment).
	 * Under this reconstruction's own `CLocaleManager::GetKeyboardLayout()`
	 * stub (always returns NULL, locale_manager.h), always takes the real
	 * "lookup failed" branch (`return 1;`) -- structurally faithful,
	 * quiescent, same status as `Exec()`.
	 */
	int ProcessEvent(const SKeyboardEvt *evt);

private:
	/* Minimal, non-template stand-in for ground truth's real
	 * `TVector<CKeyboardLayout*,1>` (0x10 bytes: vtbl + begin/end/cap pointers,
	 * matching every other TVector-shaped field in this project, e.g. task.h's
	 * mRegisteredIfcs). Growth policy mirrors `TVector<CTask::SRegisteredIfc,1>::
	 * MakeCapacity()`'s own real algorithm (task.h): start at 10 elements,
	 * double while still short. `mVtbl` is left null (never dispatched through
	 * by any code in this class -- real ground-truth identity is
	 * `PTR__TVector_08eabd20`, install-only, not consumed by anything this
	 * class's own methods do).
	 */
	struct LayoutVector {
		void             *mVtbl;
		CKeyboardLayout **mBegin;
		CKeyboardLayout **mEnd;
		CKeyboardLayout **mCap;

		void Append(CKeyboardLayout *layout);
		void FreeAll(); /* frees every element + the backing array itself */
	};

	void          *mHidResource;   /* +0x7c */
	void          *mCodeIfc;       /* +0x80 */
	int            mUnknown84;     /* +0x84, ctor sets 1 */
	int            mUnknown88;     /* +0x88, ctor sets 0xb */
	int            mUnknown8c;     /* +0x8c, ctor zeroes */
	int            mUnknown90;     /* +0x90, ctor zeroes */
	int            mUnknown94;     /* +0x94, ctor zeroes */
	unsigned short mUnknown98;     /* +0x98, ctor zeroes */
	int            mUnknown9c;     /* +0x9c, ctor zeroes */
	LayoutVector   mLayoutList;      /* +0xa0 */
	LayoutVector   mAsciiLayoutList; /* +0xb0 */

	CAlphaKeybCtrlTask(const CAlphaKeybCtrlTask &);
	CAlphaKeybCtrlTask &operator=(const CAlphaKeybCtrlTask &);

	friend struct AlphaKeybCtrlTaskTestHooks;
};

#endif /* ALPHA_KEYB_CTRL_TASK_H */
