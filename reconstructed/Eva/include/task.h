/*
 * task.h  -  CTask, the real, ground-truth-reachable class `CModule`'s own `mTasks`
 * array (module.h), `CLevelManager::RunLevel()` (level_manager_array.h), and
 * `CModule::AdjustTaskMask()` (module.h/module.cpp) were all ALREADY documented as
 * expecting -- reconstructed for real in the Stage 6 breadth-sweep pass (2026-07-25,
 * this batch), correcting two prior batches' own "no caller anywhere in this
 * reconstruction's call graph" verdict (Stage 6 batch 2's task_buffer.h note and batch
 * 5's module.h note, both now stale -- see those files' updated comments and the
 * agent-memory entries this batch corrects).
 *
 * GROUND TRUTH: `CTask::CTask()` (.text+0x0807ee80) genuinely IS called, from
 * `CEditor::CPanelIfcTask::CPanelIfcTask()` (.text+0x0824b7e0) and from
 * `CPoller::CPoller(CModule const&, char const*)` (.text+0x089ef740) -- both confirmed
 * via direct `objdump -dr` call-site inspection this batch, not inferred. Neither
 * caller is itself reconstructed here (both are deep, out-of-scope Peg/UI-adjacent
 * classes -- see the bottom of this comment), so THIS reconstruction's own currently-
 * wired call graph still does not construct a CTask on any executed path (no live
 * kronos_vm behavior change from this batch). What changed is the completeness bar:
 * per PLAN.md's own verification methodology (layer A/B faithfulness against ground
 * truth, not against "does this reconstruction's own partial call graph reach it
 * today"), a function real and reachable in the ACTUAL BINARY is a legitimate
 * reconstruction target regardless of whether every one of its own callers has been
 * reconstructed yet -- the same bar `CModuleManager::AddModule()` met before
 * `mains.cpp` populated `mModules` (Stage 6 batch 3's own "populate-the-container
 * exposes a downstream stub" finding, module_manager.h).
 *
 * A SECOND, more directly load-bearing discovery this batch: `CModule::Add(CTask*)`
 * (.text+0x0807c410, module.h/module.cpp) -- the actual `mTasks`-populating method
 * neither prior batch knew existed -- is called from `CEditor::Setup()`
 * (.text+0x08249b60), itself dispatched by `CModuleManager::Setup()`'s already-real
 * per-module vtable+8 dispatch (module_manager.cpp), from `CKernel::InitSystemLayer()`
 * (ckernel.cpp) -- i.e. `CModule::Add(CTask*)` sits on the SAME already-real boot-path
 * spine `CModuleManager::AddModule()`/`Setup()`/`AdjustTaskMask()` already occupy. This
 * DEFINITIVELY disproves both prior batches' claim that "nothing... constructs a CTask,
 * so mTasks stays permanently empty" / "no AddTask()-shaped method exists" -- one does,
 * and it is itself boot-path-reachable in ground truth. The only remaining gap between
 * "ground-truth reachable" and "this reconstruction's own live call graph reaches it"
 * is `CEditor`/`CPoller` themselves (and their constructors) staying unreconstructed --
 * both genuinely deep Peg/UI-toolkit-adjacent classes, correctly out of scope (see
 * below), not a further hidden dead-code claim.
 *
 * Real layout (0x7c bytes), confirmed from CTask@0807ee80.c and cross-checked against
 * every known consumer (module.h's AdjustTaskMask(), level_manager_array.h's
 * RunLevel()):
 *   +0x00  vtbl          CNamedObjectBase's base vtable first, then CTask's own
 *                         (PTR__CTask_08e82128, 7 slots, omega_vtables.h) once the name
 *                         copy below succeeds -- same "vtable installed AFTER the part
 *                         that can fail" idiom CModule::CModule() also uses.
 *   +0x04  mName          malloc'd copy of the name argument
 *   +0x08  mIfcThunk      opaque pointer (real: &DAT_08e82144, meaning not decoded --
 *                         see omega_vtables.h). Stored, never read back by any
 *                         reconstructed code.
 *   +0x0c  mOutLinks      embedded COmegaPtrArray (0x18 bytes), default-constructed,
 *                         vtable-swapped to TNamedPtrArray<COutLink>
 *                         (PTR__TNamedPtrArray_08e82198, 3 slots)
 *   +0x24  mIfcArray      embedded COmegaPtrArray(growBy=1,cap=0,own=0), vtable-swapped
 *                         to the SAME TNamedPtrArray<COutLink> vtable as mOutLinks
 *   +0x3c  mOwnerModule   the ctor's own CModule& argument, stored as a raw pointer
 *   +0x40  mLevel         the ctor's own ETaskLevel argument, stored verbatim
 *   +0x44  mLastArg       the ctor's own trailing `unsigned short` argument, stored
 *                         verbatim (real meaning not decoded -- every real caller so
 *                         far passes the same literal 0x804b)
 *   +0x48  mScopeId       result of a virtual call through Api's own vtable slot +0x3c
 *                         at construction time -- the exact SAME call CModule::CModule()
 *                         makes (module.cpp), same undecoded meaning
 *   +0x4c  mMask          the schedule/mask flags byte -- see below, the field
 *                         CLevelManager::RunLevel() and CModule::AdjustTaskMask() both
 *                         already read/write
 *   +0x50  mRegisteredIfcs TVector<CTask::SRegisteredIfc,1> (vtbl/begin/end/cap, 0x10
 *                         bytes), vtable-swapped to PTR__TVector_08e82188 (2 slots),
 *                         begin/end/cap zeroed -- grown by RegisterIfc() (Tier B, see
 *                         below)
 *   +0x60  mLimiterMan    embedded CLimiterMan (limiter_man.h, 0x18 bytes),
 *                         placement-constructed with `this` as owner
 *   +0x78  mPeriod        unsigned short, ctor sets 1 -- CLevelManager::RunLevel()'s
 *                         own reload value on countdown expiry
 *   +0x7a  mCountdown     unsigned short, ctor sets 1 -- CLevelManager::RunLevel()'s
 *                         own per-tick decrement target
 *
 * **Real, cross-confirming finding worth its own paragraph**: the ctor's own mMask
 * computation is genuinely two-tiered, and the SECOND tier sets exactly the bit
 * `CModule::AdjustTaskMask()` (module.h/module.cpp) clears. Base value comes from the
 * EScheduleFlag argument (0 -> 0x04, nonzero-and-not-2 -> 0x0c, ==2 -> 0x0d) -- but if
 * the owning module's own +0x24 lifecycle field (CModule's `mState`, module.h) is < 4
 * (i.e. the module has not yet reached CModuleManager's "started" stage), the ctor
 * instead uses 0x06/0x0e/0x0f -- each exactly 2 (bit 0x02) more than the corresponding
 * base value. Bit 0x02 is the SAME bit `CModule::AdjustTaskMask()` unconditionally
 * clears on every one of a module's own tasks, and one of the two bits
 * `CLevelManager::RunLevel()` checks before running a task at all. Read together: a
 * task constructed before its owning module finishes starting comes into existence
 * PRE-MASKED, and `CModuleManager::AdjustTaskMask()`'s own per-module pass (called once
 * `CKernel::InitSystemLayer()`/`InitUserLayer()` advances every module past the
 * "started" gate) is what un-masks it for scheduling -- a genuine, deliberate
 * two-phase task-activation design, not an accident this reconstruction stumbled into.
 * This closes the loop batch 5's own AdjustTaskMask() writeup left open ("re-enable
 * un-mask every one of a module's own tasks" -- now confirmed to mean exactly this).
 *
 * `RegisterIfc(CIfcUnknown*)` (.text+0x0807ec90, 472 bytes) is Tier B: real signature,
 * empty body. Real behavior is a linear dedup scan over mRegisteredIfcs (keyed by a
 * virtual call through the passed interface's own vtable+8, presumably a
 * GetInterfaceId()-shaped method) followed by a TVector::MakeCapacity()-driven
 * push_back -- genuinely deep (MakeCapacity alone is 539 bytes, a TVector<T,1> growth
 * routine this project has never generalized -- ckernel.h's own note that
 * `TVector<?>` stays an "unreconstructed template base" throughout this project
 * applies here too) and CIfcUnknown itself (the interface RegisterIfc's argument is
 * typed as) is not reconstructed. Left Tier B rather than fabricating a growth routine
 * this project has consistently deferred everywhere else it appears.
 *
 * `~CTask()` and `CLimiterMan::~CLimiterMan()` are NOT reconstructed -- nothing in this
 * reconstruction's own call graph ever destroys a CTask (see limiter_man.h). Same
 * "construct, don't destruct" scope as everywhere else sub-object construction has
 * been added without a matching destruction path.
 *
 * **CPoller surveyed, NOT pursued this batch** (batch 6 flagged it as "genuinely
 * tempting... directly in the CModule/CTask family", deliberately deferred for
 * territory reasons -- now checked with this territory owned): its own ctor
 * (`CPoller::CPoller(CModule const&, char const*)`, .text+0x089ef740, 1933 bytes) DOES
 * call `CTask::CTask()` too (a second real, confirmed caller, alongside
 * CPanelIfcTask's), but the ctor body itself is ~1900 bytes of straight-line
 * Duff's-device-unrolled 0xffffffff-fill over 2 large (0x40+0x80 dword) fixed handle
 * tables, plus a real dependency on a NOT-yet-reconstructed `CTask::SetMask(int)`
 * method and Api vtable slot +0xac (a named-resource lookup). Deeper than any single
 * Tier-A candidate reconstructed this batch and pulling in a brand-new CTask method --
 * correctly deferred, same "several-hundred-plus-bytes, pulls in further subsystems"
 * Tier-B-worthy shape as CSysApiInstance::RegisterApi/CModuleManager::AddModule's own
 * precedent, not a hidden dead-code claim.
 *
 * `CEditor::CPanelIfcTask`'s own ctor remains correctly out of scope too (see
 * panel_ifc_task.h's updated note) -- its post-CTask::CTask() tail does real multiple-
 * inheritance vtable-adjustment-thunk work (a second malloc'd sub-object,
 * `COutLinkMono`, installed via a `this+8`-adjusted secondary vtable pointer) that is
 * Peg/UI-editor-toolkit depth, not CModule/CTask/CLevelManager/CPoller family depth.
 */

#ifndef TASK_H
#define TASK_H

class CModule;
class CIfcUnknown;

class CTask {
public:
	/* .text+0x0807ee80, 330 bytes (symbols.csv:
	 * _ZN5CTaskC1ERK7CModulePKc10ETaskLevelNS_13EScheduleFlagEt). `level` is the real
	 * ETaskLevel enum (not reconstructed elsewhere in this project either -- passed
	 * through opaque as `int`, matching CLevelManagerArray::Find()'s own convention,
	 * level_manager_array.h) and `scheduleFlag` the real CTask::EScheduleFlag enum
	 * (opaque `int` here for the same reason -- only its 0/nonzero/==2 cases are
	 * exercised by the real ctor body, see header comment).
	 */
	CTask(const CModule &owner, const char *name, int level, int scheduleFlag,
	      unsigned short lastArg);

	/* .text+0x0807ec90, 472 bytes. Tier B -- see header comment. */
	void RegisterIfc(CIfcUnknown *ifc);

private:
	void          *mVtbl;
	char          *mName;
	void          *mIfcThunk;
	unsigned char  mOutLinks[0x18];
	unsigned char  mIfcArray[0x18];
	const CModule *mOwnerModule;
	int            mLevel;
	unsigned short mLastArg;
	unsigned short mPad46; /* not confirmed real; keeps mScopeId dword-aligned */
	int            mScopeId;
	unsigned char  mMask;
	unsigned char  mPad4d[3]; /* not confirmed real */
	unsigned char  mRegisteredIfcs[0x10];
	unsigned char  mLimiterMan[0x18];
	unsigned short mPeriod;
	unsigned short mCountdown;

	/* Friend accessor for verify/test_task.cpp -- see that file for the actual
	 * struct definition, same convention as comm_driver.h's CommDriverTestHooks.
	 */
	friend struct TaskTestHooks;
};

#endif /* TASK_H */
