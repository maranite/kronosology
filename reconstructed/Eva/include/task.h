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
 *                         begin/end/cap zeroed -- grown by RegisterIfc() (now Tier A,
 *                         see below)
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
 * `RegisterIfc(CIfcUnknown*)` (.text+0x0807ec90, 472 bytes) is NOW Tier A (previously
 * deferred as Tier B pending a real `TVector<T,1>::MakeCapacity()` transcription --
 * done this pass, see below). Real body, transcribed directly from `objdump -dr`
 * (Ghidra's own decompile of this one is a GCC Duff's-device/aliasing-check tangle
 * that resists direct reading -- worked from raw disassembly instead, same as
 * `CTask::Add()` before it):
 *
 *   1. Calls `ifc`'s own vtable+8 (`ifc->vtbl[2](ifc)`, cdecl, this-as-first-arg --
 *      presumably a `GetInterfaceId()`/`QueryInterface`-shaped method; `CIfcUnknown`
 *      itself stays opaque, not reconstructed, same as everywhere else it appears in
 *      this project) to compute a dedup key. Called UNCONDITIONALLY, even when
 *      mRegisteredIfcs is empty -- reproduced faithfully (not cached across the two
 *      call sites below) in case the real method has side effects.
 *   2. Linear scan over mRegisteredIfcs comparing the key against each element's own
 *      first dword. Real scan is GCC's own 8-way-unrolled Duff's device (a mod-8
 *      prologue switch computed via the same magic-multiply-divide-by-3 trick
 *      MakeCapacity() uses below, since element count here is always an exact multiple
 *      of 3 dwords) -- collapses to a plain linear `for` loop with an identical result.
 *   3. If found: two Api diagnostic calls (`vtbl+0x90` with the literal string
 *      `"CTask::RegisterIfc - Error: multiple registration for same interface"`, then
 *      `vtbl+0x94` with this project's standard soft-assert shape --
 *      `"Assertion failed in module %s, line %i.\n"` / `"Task.cpp"` / `179` --
 *      confirmed by reading both rodata strings directly). Both are the SAME
 *      non-enforcing, return-value-discarded diagnostic convention already
 *      established at every other Api+0x90/+0x94 call site in this project
 *      (`ev_buffers_pool.h`/`client_comm_server.h`/`chunk_man.h`/etc.) -- omitted from
 *      the reconstructed body for that reason, not guessed. Real behavior after
 *      logging: return immediately, array untouched (a duplicate registration is
 *      logged, not rejected or merged).
 *   4. If not found: calls `ifc->vtbl[2](ifc)` a SECOND time (a fresh call, not the
 *      cached key from step 1 -- ground truth genuinely does this, reproduced as-is),
 *      builds a 3-dword `{key, ifc, 0}` element, grows via `MakeCapacity(usedElems+1)`
 *      if `mEnd == mCap` (i.e. exactly full), then appends and advances `mEnd`.
 *
 * This establishes `CTask::SRegisteredIfc`'s real layout for the first time: 3 dwords
 * (12 bytes) -- `{void *mKey, CIfcUnknown *mIfc, void *mUnused}` (the third dword is
 * always written 0 and never read back by anything reconstructed here). Confirmed two
 * independent ways: MakeCapacity()'s own element-count arithmetic (divides byte counts
 * by 3 dwords, exactly, with no remainder, for every real capacity value exercised),
 * and this function's own element-copy stride (`lea edx,[edx+0xc]` per scan step,
 * 3 dword writes per appended element).
 *
 * `TVector<CTask::SRegisteredIfc,1>::MakeCapacity(unsigned int)` (.text+0x08182220,
 * 539 bytes, mangled `_ZN7TVectorIN5CTask14SRegisteredIfcELi1EE12MakeCapacityEj`) is
 * ALSO now Tier A -- the FIRST full transcription of a `TVector<T,1>::MakeCapacity()`
 * anywhere in this project (ckernel.h's own longstanding "unreconstructed template
 * base" note predates this). Implemented as a private static helper in task.cpp
 * (`TVector_SRegisteredIfc_MakeCapacity`), not a real C++ template -- matching this
 * project's established convention of manually transcribing each real instantiation
 * rather than writing a generic template the real compiler never actually emitted
 * this way (every sibling `TVector<T,1>::MakeCapacity()` in `manifest/eva_functions.csv`
 * -- `CRTRouterApi::SConnection`/`CPool::SPool` at the same 539-byte size, several
 * others at 506/542/556/etc. for differently-sized `T` -- is its own separate real
 * symbol in ground truth, not a shared template instantiation folded by the linker;
 * this project's OWN reconstruction of any of those would likewise be its own separate
 * transcription if/when pursued, same as every other class in this codebase).
 * Real algorithm, confirmed by direct `objdump -dr`:
 *   - `this` (a raw `unsigned char[0x10]` buffer, same convention as mRegisteredIfcs
 *     itself) and `n` (requested minimum element count) both arrive as plain stack
 *     args (GNU cdecl, `this` as an ordinary first parameter -- NOT MSVC thiscall,
 *     despite `manifest/eva_functions.csv`'s own `__thiscall` label for the row,
 *     which is just that CSV's generic "non-static member function" tag).
 *   - Current capacity in elements = `(mCap - mBegin) / 12` (a magic-multiply
 *     divide-by-3-of-dwords trick, exact because capacity is always an integral
 *     multiple of 3 dwords). If `n <= current capacity`, return immediately (no-op).
 *   - Otherwise: minimum capacity is 10 elements; if `n > 10`, double repeatedly
 *     (10, 20, 40, 80, ... ) until `>= n` -- GCC unrolls this 8-doublings-per-block
 *     with a loop-back for very large `n`, collapses to a plain `do {} while` here.
 *   - `malloc()`s the new block (real: `HAL_DisableInterrupts()`/
 *     `HAL_EnableInterrupts()`-bracketed, same as `malloc`/`free` everywhere else in
 *     this project -- not modeled, per this project's established convention), copies
 *     `[mBegin, mEnd)` into it (real: another GCC Duff's-device unrolled-by-4 copy
 *     loop with its own mod-4 prologue switch, byte-identical result to a plain
 *     `memcpy` since `SRegisteredIfc` is trivially-copyable POD -- collapsed here),
 *     `free()`s the old block, and installs the new `mBegin`/`mEnd`/`mCap`.
 *
 * `CIfcUnknown` itself (RegisterIfc's argument type) stays not reconstructed --
 * genuinely out of scope (only its vtable+8 call shape is exercised here, same as
 * `CLimiterMan`'s own base-class relationship to it, limiter_man.h).
 *
 * `SetMask(EMask)` (.text+0x0807e840, 40 bytes) IS reconstructed (Stage 6 SetMask/
 * ~CTask batch, 2026-07-25 -- CSysExMsgTaskBase's own real dependency, see
 * sysex_msg_task_base.h). Trivial: reads/writes bit 0x01 of mMask (+0x4c) -- the
 * OTHER low mask bit from `CModule::AdjustTaskMask()`'s own bit 0x02
 * (module.h/module.cpp), confirmed by `CLevelManager::RunLevel()`'s own "either of
 * its low 2 bits set = masked" check (level_manager_array.h). `EMask` itself isn't
 * individually named in the decompile (only 0/nonzero are exercised: 0 clears the
 * bit, nonzero sets it) -- opaque `int`, same convention as `ETaskLevel`/
 * `EScheduleFlag` above.
 *
 * `~CTask()` (.text+0x0807e350, 800 bytes, the D1 complete-object destructor) IS ALSO
 * reconstructed this same batch -- real, widely-exercised in ground truth (every
 * derived CTask-family class's own destructor calls it as a base dtor; ~87 direct
 * call sites found via a full `objdump -dr` sweep, e.g. CDumpTask/CSysExTaskBase/
 * CChkBaseTask/CRTRouter and every Peg-adjacent CTask-derived class), even though
 * nothing in THIS reconstruction's own wired call graph destroys a CTask yet -- same
 * "ground-truth-reachable is the bar, not this reconstruction's own partial call
 * graph" precedent this file's own CTask::CTask() note already established. Real
 * body, in order: (1) a virtual notification to Api (vtbl slot+0x140,
 * system_api.h) with `this`; (2) fully drains mOutLinks front-to-back -- for each
 * element, ANOTHER Api notification (vtbl slot+0x58, system_api.h) with the element,
 * then `COmegaPtrArray::RemoveAtIndex(0, true)` (already reconstructed,
 * omega_ptr_array.h) -- GCC's real 8-way Duff's-device unrolling collapsed to a plain
 * `while` loop, same license as every other unrolled loop in this project; (3)
 * `CLimiterMan::~CLimiterMan()` on the embedded mLimiterMan (now reconstructed,
 * limiter_man.h); (4) inlines `~TVector<SRegisteredIfc,1>()`'s own trivial body
 * (free mRegisteredIfcs' backing array if non-null -- confirmed inlined here rather
 * than called, since `TVector<CTask::SRegisteredIfc,1>::~TVector()` is a real,
 * separate, weak (COMDAT) symbol per `nm -C`, only actually CALLED from this
 * function's own (not modeled -- see below) exception-unwind path); (5) destroys
 * mIfcArray then mOutLinks, in that order, each via the SAME "install the shared
 * TNamedPtrArray<COutLink> vtable identity (0x8e82198, matching the ctor's own
 * install), call the already-reconstructed `COmegaPtrArray::Destroy()`, then
 * downgrade to the base COmegaPtrArray identity (0x8e80be0)" sequence -- confirmed
 * inlining `~TNamedPtrArray<COutLink>()`'s own body the same way, for the same
 * reason (its real, separate, weak symbol is likewise only called from the unwind
 * path); (6) installs CTask's own +0x08 field (mIfcThunk) to a real identity
 * (0x8e80c68, confirmed via nm to be `vtable for CMessageInput`+8 -- see
 * omega_vtables.h) -- CMessageInput itself is not reconstructed (separate,
 * unrelated subsystem), so this stays an opaque final-value install like the ctor's
 * own +0x08 write; (7) installs CNamedObjectBase's own vtable (0x8e81378) and frees
 * mName if non-null (CNamedObjectBase's own inlined dtor body -- confirmed against
 * `CNamedObjectBase::~CNamedObjectBase()`'s own standalone disassembly, byte-for-byte
 * identical shape, called from many OTHER classes' dtors instead of being inlined
 * there -- GCC apparently inlines it here specifically); (8) installs the ultimate
 * base identity CObjectBase (0x8e79d68, confirmed via nm/typeinfo-string
 * "11CObjectBase" -- omega_vtables.h) as the final act before returning.
 *
 * Exception-unwind paths (the real function's own `_Unwind_Resume`/
 * `__cxa_call_unexpected` cleanup landing pads, and the 2 real calls to
 * `TNamedPtrArray<COutLink>::~TNamedPtrArray()` they contain) are NOT modeled -- same
 * "happy path only" license already used for the ctor (task.cpp) and every other
 * reconstructed ctor/dtor in this project.
 *
 * Every raw vtable-slot dispatch this destructor performs (the two Api notifications,
 * CLimiterMan's own per-element release-through-the-element's-own-vtable call, and
 * every COmegaPtrArray-family slot+8 "free element" callback) resolves to
 * `EvaVTableStub` in this reconstruction's own placeholder tables (omega_vtables.cpp)
 * -- functionally inert here (confirmed: none of these arrays hold live elements in
 * any KAT this batch wrote, since nothing populates mLimiterMan's own TVector --
 * `CLimiterMan::RegisterLimiter()` is a real ground-truth method, not reconstructed,
 * out of scope; `mOutLinks` CAN now be populated, see `Add(COutLink*)` below), but the
 * vtbl-swap sequence itself is transcribed byte-order-faithfully regardless, matching
 * this project's general standard for every other reconstructed ctor/dtor.
 *
 * `CTask::Add(COutLink*)` (.text+0x0807e870, 59 bytes) is Tier A this pass (Eva
 * CSysExMsgClientOutLink follow-up, 2026-07-25) -- real body confirmed via direct
 * `objdump -dr` reading (Ghidra's own decompile mis-resolved the tail call as a
 * zero-argument indirect call, "could not recover jumptable"): `mOutLinks.Add(link)`
 * (COmegaPtrArray::Add, already real, omega_ptr_array.h) followed by a TAIL JUMP
 * (`jmp`, not `call`+`ret`) into `(*Api)[0x12c/4]`, overwriting this function's own
 * incoming argument stack slots with `(Api, mOwnerModule)` first -- i.e. the exact SAME
 * `NotifyModuleFn(void*, CModule*)` call `CModule::Add(CTask*)` already makes through
 * this identical slot (module.cpp), just triggered from the COutLink-registration path
 * instead of the CTask-registration path. Confirms system_api.h's own "+0x12c: real
 * meaning not decoded" note is a single, consistent call site shape used from at least
 * two different real callers now.
 *
 * **CPoller: reassessed 2026-07-26, now real -- see `include/poller.h`.** Its own ctor
 * (`CPoller::CPoller(CModule const&, char const*)`, .text+0x089ef740, 1933 bytes) DOES
 * call `CTask::CTask()` too (a second real, confirmed caller, alongside
 * CPanelIfcTask's). The ctor body is ~1900 bytes of straight-line Duff's-device-
 * unrolled 0xffffffff-fill over 2 large (0x40+0x80 dword) fixed handle tables, plus a
 * call through Api vtable slot +0xac (a named-resource lookup) -- both fully
 * mechanical once `CTask::SetMask(int)` (this same batch, above) was real; a fresh
 * `objdump -dr` re-check found NO further blocker. CPoller's own ctor/dtor/3 const
 * accessors, plus its nested `CIfcClient` class and that class's own
 * `TVector<CIfcClient*,1>::MakeCapacity()`, are now Tier A -- see poller.h for the
 * full derivation, the real `CPanel::Setup()` reachability finding, and the list of
 * CPoller's remaining ~20 deferred methods (size/CMessage-prerequisite reasons only,
 * not toolkit depth).
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
class COutLink;

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

	/* .text+0x0807e350, 800 bytes (D1 complete-object destructor). See header
	 * comment.
	 */
	~CTask();

	/* .text+0x0807e840, 40 bytes (symbols.csv: _ZN5CTask7SetMaskENS_5EMaskE). See
	 * header comment. `mask` is the real EMask argument, opaque int (only 0/
	 * nonzero are exercised).
	 */
	void SetMask(int mask);

	/* .text+0x0807ec90, 472 bytes. Tier A -- see header comment / .cpp. */
	void RegisterIfc(CIfcUnknown *ifc);

	/* .text+0x0807e870, 59 bytes. Tier A -- see header comment / .cpp. Real
	 * return value is whatever the tail-called Api notification itself returns
	 * (a genuine `jmp`, not `call`+`ret` -- see header comment); this function's
	 * committed signature is `void` since its own real caller
	 * (`CSysExMsgTaskBase::CSysExMsgTaskBase()`, sysex_msg_task_base.cpp) discards
	 * it as a bare statement, same "eax not part of the real contract" category
	 * as several other methods in this project (e.g. `CClientCommServer::TXData()`).
	 */
	void Add(COutLink *link);

protected:
	/* Test/stub-only placeholder (Stage 6 CEditor batch, 2026-07-25) -- zero-
	 * initializes every field, does NOT correspond to any real ground-truth
	 * CTask construction (every real CTask is always built via the 5-argument
	 * ctor above). Exists solely so a Tier-B stub CTask-derived class (e.g.
	 * `CEditor::CPanelIfcTask`'s own pre-existing default-constructible test
	 * compat, panel_ifc_task.h) can stay default-constructible without
	 * fabricating a fake CModule/name/level just to satisfy the real ctor's
	 * signature. Never invoked by any reconstructed real code path.
	 */
	CTask();

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
