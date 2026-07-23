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
 * index-by-index against each real decompile while writing this. AddModule()/
 * EnableUpdate() are Tier-B link-stubs (see module_manager.h).
 */

#include "module_manager.h"
#include "module.h"
#include "sysapi_instance.h"

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

void CModuleManager::AddModule(CModule * /*module*/)
{
	/* Tier-B link-stub -- .text+0x0805efa0, 869 bytes. See module_manager.h. */
}

void CModuleManager::EnableUpdate(int /*enable*/)
{
	/* Tier-B link-stub -- .text+0x08061ca0. See module_manager.h. */
}
