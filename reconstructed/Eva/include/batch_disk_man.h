/*
 * batch_disk_man.h  -  CBatchDiskMan, the per-module class behind "BatchDiskManClass"
 * (mains.cpp's `MMainBatchDiskMan()`/config_info.cpp's `s_atCreateInfo` row 0:
 * {"BatchDiskManClass", "BatchDiskMan",
 * "PRELOAD=EditResources.Unlocalized;EditResources.Localized.ENG"}).
 *
 * Dispatched as a direct follow-up to the CPanel/CEditor unlock batches
 * ([[eva_cpanel_unlock_2026-07-26]], [[eva_ceditor_vtable_dispatch_fixed_2026-07-26]]):
 * the EXACT same "CModuleConstructor factory currently routed through mains.cpp's
 * shared `ModuleFactoryCreateStub` (returns NULL), real per-module class not yet
 * reconstructed" situation, now applied to `CBatchDiskMan`/`CBatchDiskManConstructor`.
 * A prior mop-up pass ([[eva_mopup_sweep_2026-07-26_negative]]) re-verified
 * `CBatchDiskMan`'s OWN top-level methods really are small/mechanical (matching an
 * even earlier assessment), but found `CBatchDiskMan::Setup()` constructs 2 brand-new
 * classes (`CBatchDiskMainTask`, `CEditTask`) and correctly deferred the whole class
 * rather than build a false "unlock" that silently no-ops. This batch does the real
 * unlock: `CBatchDiskMan` itself, `CEditTask` (edit_task.h, fully real), and
 * `CBatchDiskMainTask` (batch_disk_main_task.h). UPDATE (Eva "size is not depth"
 * re-check batch, 2026-07-26): `CBatchDiskMainTask`'s own ctor is now ALSO fully
 * real -- only its embedded `CZ` member (edit_task.h's own already-established
 * project-wide out-of-scope container, config_manager.h's `CreateResourceFamilies()`)
 * stays opaque; the ctor's OWN logic around it (CTask/CEditable/CRMApiCallBack base
 * construction, heap CRMJob/COutLinkMulti, embedded CDirEntry, field inits) turned
 * out to be equally mechanical, not the "genuinely deep, CZ-scale" dependency cluster
 * the original unlock batch assumed -- see batch_disk_main_task.h's own header
 * comment for the full re-derivation. `PrepareGroupsForPreload()`/`PreloadDir()`/
 * `PreloadGroup()`/`AddItemToPreload()`/`Exec(CMessage&)` remain genuinely deferred
 * (the real CZ/CRMJob-driven business logic itself).
 *
 * REAL CLASS SHAPE (CBatchDiskMan : public CModule, public CEditServer -- the SAME
 * CModule+CEditServer shell family es_common.h's own header comment already surveyed
 * for the 10 CESxxx classes, confirmed via `nm -C`/`objdump -dr -M intel`,
 * CBatchDiskMan@082436e0.c/~CBatchDiskMan@082433c0.c,082434c0.c/Setup@082435d0.c/
 * Config@08243330.c/Start@08243320.c/IsBusy@082437c0.c/IsPreloadRunning@082437e0.c):
 *   +0x00..0x2c   CModule base subobject (module.h)
 *   +0x2c..0x40064 CEditServer base subobject (edit_server.h, 0x40038 bytes) -- same
 *                  multiply-inherited-at-fixed-offset composition CESCommon already
 *                  established (es_common.h)
 *   +0x40064  mMainTask  CBatchDiskMainTask* (batch_disk_main_task.h)
 *   +0x40068  mParam     CParameterString* -- HEAP-ALLOCATED (not embedded, unlike
 *                        CPanel's own `mParam`), constructed from the ctor's own 2nd
 *                        argument ONLY IF that argument is non-null (real, confirmed
 *                        `if (param2 != 0) { ... }` branch in the ctor's own
 *                        disassembly), and destroyed+freed again inside `Setup()`
 *                        itself once consumed -- a genuinely transient field, never
 *                        alive past `Setup()` returning.
 *   +0x4006c  mEditTask  CEditTask* (edit_task.h)
 * Real total size 0x40070 (262256 bytes), confirmed directly from
 * `CBatchDiskManConstructor::Create`'s own real `malloc(0x40070)` call
 * (.text+0x08243d80) -- matches 0x40064 (CModule+CEditServer) + 3*4 (the 3 pointer
 * members above) exactly, no gap.
 *
 * `CBatchDiskMan::CBatchDiskMan(const char *name, const char *param2)`
 * (.text+0x082436e0, 208 bytes): `CModule::CModule(this, name)`,
 * `CEditServer::CEditServer(this+0x2c, name)`, install CBatchDiskMan's own real
 * combined vtables (`this+0`=0x08eac048, `this+0x2c`=0x08eac06c, confirmed via direct
 * `.rodata` dword reads), then conditionally `malloc(0xc)` +
 * `CParameterString::CParameterString(&mParam, param2)` if `param2 != 0`, else
 * `mParam = 0`.
 *
 * `CBatchDiskMan::Setup()` (.text+0x082435d0, 209 bytes): if `mParam != 0`,
 * `preloadList = mParam->GetParamStr("PRELOAD")` (key immediate confirmed at
 * 0x08eabe85) else `preloadList = 0`; `malloc(sizeof(CBatchDiskMainTask real =
 * 0x160))` + `CBatchDiskMainTask::CBatchDiskMainTask(*this, preloadList)`,
 * `mMainTask = raw`, `CModule::Add(mMainTask)`; `malloc(0x88)` +
 * `CEditTask::CEditTask(*this)`, `mEditTask = raw`, `CModule::Add(mEditTask)`; if
 * `mParam != 0`, `mParam->~CParameterString(); free(mParam);`; `mParam = 0`;
 * `return 0`. This is the exact call `CBatchDiskMainTask`/`CEditTask`'s own header
 * comments described in advance; now implemented for real (modulo
 * `CBatchDiskMainTask`'s own Tier-B substitute body, see that file).
 *
 * `CBatchDiskMan::Config()` (.text+0x08243330, 137 bytes): real, dispatches through
 * a global registration call at `Api+0x44` (`ds:0x930a1f4` = `Api`, sysapi_instance.cpp)
 * with 6 real args transcribed directly from the disassembly: `(this->mName,
 * mEditTask->mName, mEditTask->GetOutLinkName(), this->mName again,
 * mMainTask->mName, 0)` -- own name passed twice (arg1 and arg4), preserved verbatim
 * rather than simplified since the slot's own semantics are undecoded. A real, but
 * previously-project-wide-out-of-scope global-Api-vtable-slot-0x44 dispatch. Kept as
 * a direct call through that same slot (matching this project's `CallVSlot`-style raw
 * vtable dispatch idiom used throughout ckernel.cpp/module_manager.cpp) rather than
 * inventing a named wrapper -- no other reconstructed code currently names this slot
 * (`Api+0x44`), and this pass doesn't have independent grounds to name it here either;
 * unconditional `return 0`.
 *
 * `CBatchDiskMan::Start()` (.text+0x08243320, 6 bytes): real, literal `return 0;` --
 * no other body (same "genuinely empty, not a stub" status as `CPanel::Start()`).
 *
 * `CBatchDiskMan::IsBusy() const`/`IsPreloadRunning() const` (.text+0x082437c0/
 * 0x082437e0, 22/22 bytes): real, literal `return mMainTask->IsBusy();`/
 * `return mMainTask->IsPreloadRunning();` -- direct forwards, no logic of their own
 * (ground truth is a raw tail `jmp` into `CBatchDiskMainTask`'s own same-named
 * method).
 *
 * `CBatchDiskMan::~CBatchDiskMan()` (.text+0x082433c0, 240 bytes): re-installs both
 * vtable groups, destroys+frees `mParam` if still non-null (dead code on every real
 * path since `Setup()` already nulls it -- preserved for fidelity, matches ground
 * truth exactly), `CEditServer::~CEditServer()` (already real, edit_server.h), base
 * `CModule` teardown not reconstructed (no `~CModule()` exists in this project --
 * same known gap as `CESCommon`/`CPanel`/`CEditor`, not new here).
 */

#ifndef BATCH_DISK_MAN_H
#define BATCH_DISK_MAN_H

#include "module.h"
#include "edit_server.h"
#include "parameter_string.h"
#include "batch_disk_main_task.h"
#include "edit_task.h"

class CBatchDiskMan : public CModule, public CEditServer {
public:
	CBatchDiskMan(const char *name, const char *param2);
	~CBatchDiskMan();

	int Setup();
	int Config();
	int Start();

	bool IsBusy() const;
	bool IsPreloadRunning() const;

private:
	CBatchDiskMainTask *mMainTask;
	CParameterString    *mParam;
	CEditTask           *mEditTask;

	/* Friend accessor for verify/test_batch_disk_man.cpp -- same "friend pokes
	 * private state" convention as panel.h's own PanelTestHooks.
	 */
	friend struct BatchDiskManTestHooks;
};

#endif /* BATCH_DISK_MAN_H */
