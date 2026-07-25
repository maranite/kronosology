/*
 * module_manager.cpp  -  see include/module_manager.h.
 *
 * Setup()/Config()/AdjustTaskMask()/Start() transcribed from:
 *   CModuleManager::Setup()          .text+0x0805fca0, 510 bytes
 *   CModuleManager::Config()         .text+0x0805feb0, 505 bytes
 *   CModuleManager::AdjustTaskMask() .text+0x080600c0, 620 bytes
 *   CModuleManager::Start()          .text+0x08060350, 845 bytes
 *
 * All 4 real bodies are the same GCC 4-way-unrolled "walk the module array, run a
 * per-module lifecycle transition if its state hasn't reached this phase yet" loop,
 * collapsed to a single clean for-loop per method (same license as
 * omega_ptr_array.cpp's collapse of its own 3 unrolled walks) -- verified
 * index-by-index against each real decompile while writing this.
 *
 * AddModule()/EnableUpdate() upgraded from Tier-B to Tier A (Stage 6 breadth sweep,
 * 2026-07-25) -- both have real boot-path callers (mains.cpp's 8 MMainXxx registration
 * shims call AddModule() via CSysApiInstance::AddModule(); ckernel.cpp's
 * InitSystemLayer() calls EnableUpdate(1) directly), so mModules/mCurModule/
 * mTopologyChanged were staying permanently at their construction-time zero values --
 * the exact same "real caller, dead Tier-B stub" shape batch 1 found in
 * CLevelManagerArray::Add()/Find(). See module_manager.h for the transcribed layout
 * detail this reconstruction confirms/corrects.
 *
 *   CModuleManager::AddModule()      .text+0x0805efa0, 869 bytes
 *   CModuleManager::EnableUpdate()   .text+0x08061ca0, 74 bytes
 */

#include "module_manager.h"
#include "module.h"
#include "omega_ptr_array.h"
#include "sysapi_instance.h"

#include <cstring>

void *g_poModuleManager = 0;

namespace {
typedef void (*VCallFn)(void *);

inline void CallVSlot(void *obj, int byteOffset)
{
	void *vtbl = *(void **)obj;
	VCallFn fn = *(VCallFn *)((char *)vtbl + byteOffset);
	fn(obj);
}
} // namespace

void CModuleManager::Setup()
{
	char *self = (char *)this;
	int count = *(int *)(self + 0x10);
	*(int *)(self + 0x38) = 1;

	for (int i = 0; i < count; i++) {
		void *module = ((void **)*(int *)(self + 0x18))[i];
		*(void **)(self + 0x34) = module;
		if (*(int *)((char *)module + 0x24) < 1) {
			CallVSlot(module, 8);
			*(int *)((char *)module + 0x24) = 1;
		}
	}

	*(void **)(self + 0x34) = 0;
	*(int *)(self + 0x38) = 0;
}

void CModuleManager::Config()
{
	char *self = (char *)this;
	int count = *(int *)(self + 0x10);
	*(int *)(self + 0x38) = 2;

	for (int i = 0; i < count; i++) {
		void *module = ((void **)*(int *)(self + 0x18))[i];
		*(void **)(self + 0x34) = module;
		if (*(int *)((char *)module + 0x24) < 2) {
			CallVSlot(module, 0xc);
			*(int *)((char *)module + 0x24) = 2;
		}
	}

	*(void **)(self + 0x34) = 0;
	*(int *)(self + 0x38) = 0;
}

void CModuleManager::AdjustTaskMask()
{
	char *self = (char *)this;
	int count = *(int *)(self + 0x10);
	*(int *)(self + 0x38) = 1; /* real code writes 1 here too, not 3 -- see header comment */

	int saved = ((CSysApiInstance *)SysApiInstance)->EnableMultiTask(0);

	for (int i = 0; i < count; i++) {
		CModule *module = (CModule *)((void **)*(int *)(self + 0x18))[i];
		*(void **)(self + 0x34) = module;
		if (*(int *)((char *)module + 0x24) < 3) {
			module->AdjustTaskMask();
			*(int *)((char *)module + 0x24) = 3;
		}
	}

	((CSysApiInstance *)SysApiInstance)->EnableMultiTask(saved);
	*(void **)(self + 0x34) = 0;
	*(int *)(self + 0x38) = 0;
}

void CModuleManager::Start()
{
	char *self = (char *)this;
	int count = *(int *)(self + 0x10);
	*(int *)(self + 0x38) = 3;

	for (int i = 0; i < count; i++) {
		void *module = ((void **)*(int *)(self + 0x18))[i];
		*(void **)(self + 0x34) = module;
		if (*(int *)((char *)module + 0x24) < 4) {
			/* Real code brackets each individual Start() dispatch in its own
			 * EnableMultiTask(0)/EnableMultiTask(saved) pair, not once for the
			 * whole loop -- preserved as found, not "optimized".
			 */
			int saved = ((CSysApiInstance *)SysApiInstance)->EnableMultiTask(0);
			CallVSlot(module, 0x10);
			*(int *)((char *)module + 0x24) = 4;
			((CSysApiInstance *)SysApiInstance)->EnableMultiTask(saved);
		}
	}

	*(void **)(self + 0x34) = 0;
	*(int *)(self + 0x38) = 0;
	*(int *)(self + 0x3c) = 1;
	*(int *)self = 0;

	if (*(int *)(self + 0x40) != 0)
		((CSysApiInstance *)SysApiInstance)->WriteMessageToHost(3, 8);
}

void CModuleManager::AddModule(CModule *module)
{
	char *self = (char *)this;
	COmegaPtrArray *modules = (COmegaPtrArray *)(self + 4);

	int count = *(int *)(self + 0x10);
	void **array = *(void ***)(self + 0x18);
	const char *newName = *(char **)((char *)module + 4); /* CModule::mName */

	/* Real code performs this identical by-name linear scan TWICE in a row -- once
	 * as an existence check (falls straight through to LAB_0805f149/"just append"
	 * if no match), once more immediately after to re-derive the index actually
	 * passed to RemoveAtIndex(). Nothing mutates mModules between the two scans in
	 * the real body, so they provably always agree; collapsed to a single scan here
	 * (same license as omega_ptr_array.cpp's Duff's-device collapses). The real
	 * "found but slot is NULL" guard (LAB_0805f1b0's first check) is preserved
	 * faithfully even though it's structurally unreachable given a match requires
	 * dereferencing that same slot -- kept as found rather than silently dropped.
	 */
	int foundIndex = -1;
	for (int i = 0; i < count; i++) {
		const char *existingName = *(char **)((char *)array[i] + 4);
		if (strcmp(newName, existingName) == 0) {
			foundIndex = i;
			break;
		}
	}

	if (foundIndex >= 0 && array[foundIndex] != 0) {
		int ownFlag = *(int *)(self + 8); /* mModules.mUnknown04, absolute +0x08 */
		modules->RemoveAtIndex((unsigned)foundIndex, ownFlag);
	}

	modules->Add(module);
	*(int *)self = 0; /* mBusy = 0 */

	if (*(int *)(self + 0x3c) != 0 && *(int *)(self + 0x40) != 0) {
		((CSysApiInstance *)SysApiInstance)->WriteMessageToHost(3, 8);
	}
}

void CModuleManager::EnableUpdate(int enable)
{
	char *self = (char *)this;

	*(int *)(self + 0x40) = 1; /* mTopologyChanged = 1, unconditional -- real setter,
	                             * confirmed here (module_manager.h previously flagged
	                             * this as "not traced"). */

	if (enable != 0) {
		*(int *)self = 0; /* mBusy = 0 */
		if (*(int *)(self + 0x3c) != 0) {
			((CSysApiInstance *)SysApiInstance)->WriteMessageToHost(3, 8);
		}
	}
}
