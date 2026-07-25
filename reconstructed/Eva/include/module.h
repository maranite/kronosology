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
 * CModule::AdjustTaskMask() (.text+0x0807c640, 458 bytes) is a Tier-B link-stub here --
 * real per-module task-mask recompute, genuinely out of scope for this pass (needs a
 * real CTask-instantiation path, which nothing in this reconstruction's own call graph
 * exercises -- see task_buffer.h / level_manager_array.h's CLevelManager::RunLevel()
 * comment for the CTask layout this would need).
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

class CModule {
public:
	CModule(const char *name);

	void AdjustTaskMask(); /* Tier-B link-stub, see header comment */

private:
	void *mVtbl;
	char *mName;
	char  mTasks[0x18];
	int   mUnknown20;
	int   mState;
	int   mScopeId;
};

#endif /* MODULE_H */
