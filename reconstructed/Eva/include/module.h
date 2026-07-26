/*
 * module.h  -  CModule, the common base every "MMainXxx(void)" system-layer module
 * (CEditMan, CViewBase, CSeqTimer, CFileMan, CChunkMan, CDumpManMod, CResMan, ...)
 * vtable-swaps itself into after this base ctor runs (Stage 4).
 *
 * Real layout confirmed from CModule@0807c330.c (the only reconstructed method):
 *   +0x00  vtbl        raw vtable-pointer slot -- installed as CNamedObjectBase's base
 *                       vtable first, then immediately overwritten with the real
 *                       derived module's own PTR__CXxx_<addr> vtable by every MMainXxx
 *                       caller (mains.cpp) -- same manual-vtable-swap idiom as
 *                       everywhere else in this project. Plain void*, not a real C++
 *                       vtable, same reasoning as omega_ptr_array.h's mVtbl.
 *   +0x04  mName        malloc'd copy of the name string passed to the ctor
 *   +0x08  mTasks       embedded COmegaPtrArray (0x18 bytes), vtable-swapped to
 *                       TNamedPtrArray<CTask> by the ctor
 *   +0x20  mUnknown20   ctor zeroes; not read back by any reconstructed method
 *   +0x24  mState       CModuleManager::Setup()/Config()/AdjustTaskMask()/Start()'s own
 *                       lifecycle-stage gate: 0=constructed, 1=setup done, 2=configured,
 *                       3=task-mask adjusted, 4=started. CModuleManager accesses this by
 *                       raw offset (module manager's own methods treat modules as
 *                       opaque `int*` blobs, matching the real decompile -- see
 *                       module_manager.cpp), not through a CModule member, so this field
 *                       has no public accessor here.
 *   +0x28  mScopeId     result of a virtual call through Api's own vtable slot +0x3c at
 *                       construction time -- meaning not decoded (some kind of
 *                       CSystemApi-assigned scope/task-level id), stored but never read
 *                       back by any reconstructed method
 *
 * Real total base size is at least 0x2c (44) bytes; every real derived module malloc's
 * more than that (0x30/0x34/0x2c/0xa5c/0x21a0 depending on the module) for its own
 * extra fields -- none of those derived fields are reconstructed (see mains.cpp).
 *
 * CModule::AdjustTaskMask() (.text+0x0807c640, 458 bytes) is Tier A (Stage 6 breadth
 * sweep, 2026-07-25 -- upgraded from a Tier-B link-stub). Real body: a compiler-
 * unrolled (8x, Duff's-device-shaped) reverse walk over mTasks (this class's own
 * embedded COmegaPtrArray, count/array at the usual relative +0xc/+0x14, landing at
 * this class's absolute +0x14/+0x1c since mTasks itself starts at +0x08 -- same
 * derivation module_manager.h's header comment already used for CModuleManager's own
 * embedded arrays), clearing bit 0x02 of each task's own mask/flags byte at +0x4c.
 * That is the SAME byte/bit `CLevelManager::RunLevel()` (level_manager_array.h /
 * scheduler.cpp) reads as one of its own "masked, don't run" gate bits -- confirming
 * AdjustTaskMask()'s real purpose: re-enable (un-mask) every one of this module's own
 * tasks for scheduling. Genuinely boot-path-reachable now: called by
 * `CModuleManager::AdjustTaskMask()` (module_manager.cpp) once per registered module,
 * from `CKernel::InitSystemLayer()`/`InitUserLayer()` (ckernel.cpp) -- dead in a prior
 * batch (mModules was permanently empty before `CModuleManager::AddModule()` itself
 * went Tier A, Stage 6 batch 3), now genuinely exercised. See module.cpp for the real
 * body and a preserved-but-dead near-NULL-deref quirk in the original disassembly.
 *
 * CModule::Add(CTask*) (.text+0x0807c410, 91 bytes) is Tier A (Stage 6 breadth sweep,
 * 2026-07-25 -- CTask::CTask() reconstruction batch). Real, DEFINITIVELY-boot-path-
 * reachable caller: `CEditor::Setup()` (.text+0x08249b60) calls it once per
 * freshly-constructed CTask-derived member (CMainTask, CPanelIfcTask,
 * CChunkServerTask, ...), and `CEditor::Setup()` itself is dispatched by
 * `CModuleManager::Setup()`'s own already-real per-module vtable+8 call
 * (module_manager.cpp), from `CKernel::InitSystemLayer()` (ckernel.cpp) -- the SAME
 * already-real boot-path spine `AddModule()`/`AdjustTaskMask()` sit on. This is the
 * genuine `mTasks`-populating method neither Stage 6 batch 2 nor batch 5 knew existed
 * when they wrote "nothing... constructs a CTask, so mTasks stays permanently empty" /
 * "no AddTask()-shaped method exists" -- both corrected, see task.h's own header
 * comment for the full writeup. `CEditor`/`CPoller` themselves (the only two real
 * callers of `CTask::CTask()`, task.h) remain unreconstructed (genuinely deep Peg/UI
 * depth), so mTasks still stays empty on THIS reconstruction's own currently-wired
 * boot path -- but the claim that no such mechanism exists in ground truth is false.
 *
 * CModule's own real vtable (PTR__CModule_08e81fe8) is a ground-truth-counted 7-slot
 * array (omega_vtables.h/.cpp, Stage 6, 2026-07-25): dtor pair (0/4), Setup(+8),
 * Config(+0xc), Start(+0x10) -- all 3 dispatched by name in module_manager.cpp -- plus
 * 2 further real named methods this pass didn't individually trace (`Destroy`,
 * `GetErrorMsg`) that exactly account for the remaining 2 slots. Never actually
 * dispatched through by any reconstructed code: every real MMainXxx(void) caller
 * (mains.cpp) overwrites this field with the derived module's own vtable immediately
 * after construction. CModuleManager::Setup/Config/Start() DO now iterate a genuinely
 * populated mModules (CModuleManager::AddModule() upgraded to Tier A, 2026-07-25 --
 * see module_manager.h) -- but every Setup/Config/Start dispatch still lands on
 * CModule's own base vtable slots transcribed above, since the derived module's real
 * vtable-swap target objects (CEditMan, CViewBase, CSeqTimer, ...) are themselves out
 * of scope for this pass.
 */

#ifndef MODULE_H
#define MODULE_H

class CTask;

class CModule {
public:
	CModule(const char *name);

	void AdjustTaskMask(); /* Tier A, see header comment + module.cpp */

	/* .text+0x0807c410, 91 bytes. Tier A, see header comment + module.cpp. Appends
	 * `task` to mTasks and fires 2 Api notifications (system_api.h: +0x134/+0x12c);
	 * real return value is just `task` echoed back (matches every real caller
	 * ignoring it, e.g. CEditor::Setup()'s own `CModule::Add(this, task)` call whose
	 * result is discarded).
	 */
	CTask *Add(CTask *task);

private:
	void *mVtbl;
	char *mName;
	char  mTasks[0x18];
	int   mUnknown20;
	int   mState;
	int   mScopeId;

	/* Friend accessor for verify/test_module_adjust_task_mask.cpp -- same
	 * extraction pattern already used by comm_driver.h/level_manager_array.h.
	 */
	friend struct ModuleTestHooks;

	/* CEditor (editor.h) is the one confirmed real caller whose OWN ctor installs
	 * its derived vtable inline (`*(void***)this = &PTR__CEditor_...`), unlike the
	 * MMainXxx-shim family above which vtable-swaps externally in mains.cpp -- needs
	 * direct write access to mVtbl for that, matching ground truth's own inline
	 * self-install shape (CEditor@08249cd0.c).
	 */
	friend class CEditor;

	/* CPanel (panel.h) installs its own derived vtable inline the same way, Eva
	 * Stage 6 CPanel unlock batch, 2026-07-26.
	 */
	friend class CPanel;
};

#endif /* MODULE_H */
