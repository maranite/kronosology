/*
 * module_manager.h  -  CModuleManager, the module lifecycle driver (Stage 4).
 *
 * CKernel::CKernel() hand-builds one of these as a raw 0x44-byte blob (g_poModuleManager,
 * ckernel.cpp) rather than placement-constructing it through a real CModuleManager
 * ctor -- see ckernel.cpp's own header comment. Because of that, every method here is
 * implemented against raw `(char*)this` offset arithmetic (matching the real
 * decompile's own style) rather than through typed member declarations -- declaring
 * typed members here would misleadingly imply a construction contract this class
 * never actually goes through in this reconstruction.
 *
 * Real layout, confirmed by ckernel.cpp's own construction code AND independently by
 * every offset Setup()/Config()/AdjustTaskMask()/Start() touch (they agree exactly):
 *   +0x00        mBusy         "currently processing" scratch, always written 0
 *   +0x04..+0x1c mModules      embedded COmegaPtrArray -> TNamedPtrArray<CModule>
 *                               (this class's own +0xc/+0x14 land at absolute +0x10/
 *                               +0x18 here, which is exactly what Setup()/Config()/
 *                               AdjustTaskMask()/Start() read as "module count"/
 *                               "module array")
 *   +0x1c..+0x34 mConstructors embedded COmegaPtrArray -> TNamedPtrArray<CModuleConstructor>
 *   +0x34        mCurModule    "current module" scratch Setup()/Config()/... all set
 *                               per-iteration (real purpose: presumably lets a crash
 *                               handler report which module was mid-lifecycle-transition)
 *   +0x38        mPhase        1=Setup running, 2=Config running, 3=AdjustTaskMask
 *                               running (its own real disassembly never actually sets
 *                               this to a distinct value, unlike the other 3 -- see
 *                               module_manager.cpp), reset to 0 when each phase finishes
 *   +0x3c        mStarted      set 1 by Start() at the very end; read back by
 *                               AddModule()/EnableUpdate() as the gate on whether to
 *                               notify the host of a post-start topology change
 *   +0x40        mTopologyChanged  checked by Start()'s own final WriteMessageToHost(3, 8)
 *                               call; real setter confirmed (2026-07-25) to be
 *                               EnableUpdate(int) (unconditional, every call), NOT
 *                               AddModule() -- AddModule() only ever reads it despite
 *                               being the more obvious "topology changed" candidate
 *
 * mConstructors (+0x1c..+0x34) is now populated by a real, reconstructed pair of
 * methods -- AddConstructor()/RemoveConstructor() (Stage 6 breadth sweep, 2026-07-25,
 * following up on CConfigManager::CreateUserModules()/CreateFMDrivers(),
 * config_manager.cpp) -- confirming this genuinely IS a second, distinct "module
 * factory" registry, not the same array as mModules. Its own count/array fields land
 * at absolute +0x28/+0x30 (mConstructors' own relative +0xc/+0x14, same embedding-
 * offset derivation as mModules' own +0x10/+0x18), which is exactly what
 * CreateUserModules() reads directly (`g_poModuleManager+0x28`/`+0x30`) and what
 * AddConstructor()/RemoveConstructor() operate on via `this+0x28`/`this+0x30`. Real
 * boot-path producer: mains.cpp's RegisterModuleDescriptor() (all 15 descriptor-shaped
 * MMainXxx wrappers), via CSysApiInstance::AddConstructor() forwarding through Api's
 * own vtable slot +0x40 -- see sysapi_instance.h and omega_vtables.cpp for the real
 * vtable-slot fix this required (that slot was a dead EvaVTableStub no-op before this
 * batch, so mConstructors, like mModules before batch 3, stayed permanently empty on
 * every previously-traced boot path).
 */

#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

class CModule;
class CModuleConstructor;

class CModuleManager {
public:
	/* .text+0x0805fca0, 510 bytes. Calls each not-yet-setup (state < 1) module's
	 * vtable slot +8 ("Setup" virtual), then marks it state=1.
	 */
	void Setup();

	/* .text+0x0805feb0, 505 bytes. Same shape as Setup(), vtable slot +0xc
	 * ("Config" virtual), state gate < 2 -> 2.
	 */
	void Config();

	/* .text+0x080600c0, 620 bytes. Calls CModule::AdjustTaskMask() directly (a named
	 * method, not a vtable slot) on each state < 3 module, then marks state=3. Also
	 * brackets the whole pass in CSysApiInstance::EnableMultiTask(0)/EnableMultiTask
	 * (saved) -- real multitasking-disable-during-reconfigure guard.
	 */
	void AdjustTaskMask();

	/* .text+0x08060350, 845 bytes. Same shape as AdjustTaskMask() but dispatches
	 * vtable slot +0x10 ("Start" virtual) under the same EnableMultiTask bracket per
	 * module (not once for the whole pass -- real, slightly wasteful-looking
	 * per-iteration bracket, preserved as found), state gate < 4 -> 4. Notifies the
	 * host (WriteMessageToHost(3, 8)) at the end if mTopologyChanged is set.
	 */
	void Start();

	/* .text+0x0805efa0, 869 bytes -- Tier A (Stage 6 breadth sweep, 2026-07-25; was a
	 * Tier-B link-stub through batch 2). Real body: linear by-name scan over mModules
	 * for an existing module sharing the new module's mName (real code runs the same
	 * scan twice in a row -- see module_manager.cpp for why that's collapsed to one);
	 * if found, RemoveAtIndex()s the old entry first (using mModules' own mUnknown04
	 * "own/free-on-remove" flag as RemoveAtIndex's callDtorCallback argument -- a
	 * genuine re-registration-by-name mechanism, not dead code), then always
	 * COmegaPtrArray::Add()s the new module, clears mBusy, and -- if mStarted and
	 * mTopologyChanged are both set -- notifies the host (WriteMessageToHost(3, 8)).
	 * Real signature takes CModule& (reference); a pointer is used here to keep the
	 * call sites simple, semantically identical for a non-null argument. This is the
	 * function that was leaving mModules permanently empty for every real boot-path
	 * caller (mains.cpp's 8 MMainXxx registration shims, via
	 * CSysApiInstance::AddModule()) -- same shape bug as batch 1's
	 * CLevelManagerArray::Add()/Find().
	 */
	void AddModule(CModule *module);

	/* .text+0x08061ca0, 74 bytes -- Tier A (Stage 6 breadth sweep, 2026-07-25).
	 * Confirmed real setter of mTopologyChanged (unconditionally sets it to 1 --
	 * previously flagged above as "not traced"); if enable != 0, also clears mBusy
	 * and, if mStarted is set, notifies the host (WriteMessageToHost(3, 8)). Real
	 * boot-path caller: ckernel.cpp's InitSystemLayer() calls EnableUpdate(1)
	 * directly after Start().
	 */
	void EnableUpdate(int enable);

	/* .text+0x0805f660, 812 bytes -- Tier A (Stage 6 breadth sweep, 2026-07-25).
	 * Operates on mConstructors (+0x1c..+0x34), exact same shape as AddModule()'s own
	 * operations on mModules: linear by-name scan (real code runs it twice in a row,
	 * same license as AddModule()'s own collapse -- see module_manager.cpp) for an
	 * existing constructor sharing the new one's name (CModuleConstructor's own +4
	 * field); if found, RemoveAtIndex()s the old entry first (mConstructors' own
	 * mUnknown04 "own/free-on-remove" flag at absolute +0x20 as the callDtorCallback
	 * argument), then always COmegaPtrArray::Add()s the new constructor. Real
	 * signature takes CModuleConstructor& (reference); a pointer is used here for the
	 * same reason as AddModule(). Real caller: CSysApiInstance::AddConstructor()
	 * (sysapi_instance.h), itself the real target of Api's vtable slot +0x40 --
	 * see that header's own writeup.
	 */
	void AddConstructor(CModuleConstructor *ctor);

	/* .text+0x0805f990, 766 bytes -- Tier A (Stage 6 breadth sweep, 2026-07-25). Same
	 * by-name-scan-twice shape as AddConstructor() (real body: one scan to confirm a
	 * match exists, a second identical scan to re-derive the index for
	 * RemoveAtIndex() -- collapsed to a single scan here, same license), RemoveAtIndex()s
	 * the match if found, otherwise a no-op. No confirmed real caller on this pass's
	 * own traced boot path (declared/reconstructed for structural completeness
	 * alongside AddConstructor(), same as COmegaPtrArray's own RemoveAll()/
	 * SetAtIndex() precedent of "real method, not currently exercised").
	 */
	void RemoveConstructor(CModuleConstructor *ctor);
};

/* Real raw-blob instance CKernel::CKernel() builds -- see ckernel.cpp. Declared here
 * (rather than kept static inside ckernel.cpp) now that CModuleManager's own methods
 * need to reach it too (CSysApiInstance::AddModule's real forwarding target).
 */
extern void *g_poModuleManager;

#endif /* MODULE_MANAGER_H */
