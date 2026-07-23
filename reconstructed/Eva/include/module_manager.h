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
 *   +0x3c        mStarted      set 1 by Start() at the very end, never read back here
 *   +0x40        mTopologyChanged  checked (never written) by Start()'s own final
 *                               WriteMessageToHost(3, 8) call -- real setter (probably
 *                               AddModule()/RemoveModule()) not traced
 */

#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

class CModule;

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

	/* .text+0x0805efa0, 869 bytes -- Tier-B link-stub, not reconstructed (real module
	 * registration: array insert + task-level wiring, genuinely out of scope for this
	 * pass). Real signature takes CModule& (reference); a pointer is used here to
	 * keep the call sites simple, semantically identical for a non-null argument.
	 */
	void AddModule(CModule *module);

	/* .text+0x08061ca0, not measured -- Tier-B link-stub, not reconstructed. */
	void EnableUpdate(int enable);
};

/* Real raw-blob instance CKernel::CKernel() builds -- see ckernel.cpp. Declared here
 * (rather than kept static inside ckernel.cpp) now that CModuleManager's own methods
 * need to reach it too (CSysApiInstance::AddModule's real forwarding target).
 */
extern void *g_poModuleManager;

#endif /* MODULE_MANAGER_H */
