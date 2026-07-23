/*
 * sysapi_instance.cpp  -  see include/sysapi_instance.h.
 *
 * Cleanup()/AddModule() transcribed from Cleanup@0806ca50.c (497 bytes) and
 * AddModule@0806b550.c (22 bytes). EnableMultiTask()/WriteMessageToHost()/RegisterApi()
 * are Tier-B link-stubs (see sysapi_instance.h).
 *
 * ConstructSysApiInstance() (new, 2026-07-23) transcribes
 * global.constructors.keyed.to.SysApiInstance@0806cc50.c (123 bytes) -- the real static
 * constructor that sets `Api = SysApiInstance;` before main(), fixing the
 * MMainEditMan() NULL-Api crash. See global_object_base.h for the full mechanism.
 */

#include "sysapi_instance.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "module_manager.h"
#include "global_object_base.h"
#include "system_api.h"

#include <new>

unsigned char SysApiInstance[0x34] = {};

/* Real global, mains.cpp's own primary singleton (its canonical definition lives
 * there -- see ckernel.h's own module.cpp precedent for this same extern-in-a-.cpp
 * pattern). SysApiInstance's own static constructor below is the one that actually
 * writes it.
 */
extern CSystemApi *Api;

/* Real global paired with Api (adjacent in .data -- Api@0930a1f4, this@0930a1f8),
 * same "name string right after the pointer" pattern as mains.cpp's own DAT_0930aae8
 * ("EditApi") etc. Not read by any reconstructed code, kept for shape-fidelity.
 */
extern "C" const char *_DAT_0930a1f8 = 0;

/* .text+0x0806cc50, 123 bytes (global.constructors.keyed.to.SysApiInstance@0806cc50.c).
 * Real sequence: base-construct CGlobalObjectBase (registers into
 * sm_poGlobalObjectList, see global_object_base.h/ckernel.cpp), install
 * CSysApiInstance's own vtable, placement-construct the 2 embedded COmegaPtrArray
 * sub-objects and vtable-swap each to its own real TNamedPtrArray<T> flavor (see this
 * header's own corrected +4/+0x1c field-mapping comment), then `Api = SysApiInstance;`.
 * The real __cxa_atexit(CSysApiInstance::~CSysApiInstance, ...) registration is not
 * modeled -- this pass's own traced boot path never calls exit() normally, and
 * CSysApiInstance has no reconstructed destructor of its own to register (Cleanup() is
 * a distinct, explicitly-invoked method, not a C++ destructor).
 */
__attribute__((constructor))
static void ConstructSysApiInstance()
{
	new (SysApiInstance) CGlobalObjectBase();
	*(void **)SysApiInstance = &PTR__CSysApiInstance_08e81008[0];

	new (SysApiInstance + 4) COmegaPtrArray();
	*(void **)(SysApiInstance + 4) = &PTR__TNamedPtrArray_08e811c0[0];

	new (SysApiInstance + 0x1c) COmegaPtrArray();
	*(void **)(SysApiInstance + 0x1c) = &PTR__TNamedPtrArray_08e811a8[0];

	Api = (CSystemApi *)SysApiInstance;
	_DAT_0930a1f8 = "SysApi";
}

namespace {
typedef void (*UninitFn)(void *);

inline void CallUninit(void *obj)
{
	void *vtbl = *(void **)obj;
	UninitFn fn = *(UninitFn *)((char *)vtbl + 0x1c);
	fn(obj);
}
} // namespace

void CSysApiInstance::Cleanup()
{
	char *self = (char *)this;

	COmegaPtrArray *drivers = (COmegaPtrArray *)(self + 4);
	while (*(int *)(self + 0x10) != 0) {
		int count = *(int *)(self + 0x10);
		void *elem = *(void **)(*(int *)(self + 0x18) + (count - 1) * 4);
		void *sub = *(void **)((char *)elem + 8);
		CallUninit(sub);

		int callDtor = *(int *)(self + 8);
		unsigned idx = drivers->FindIndex(elem);
		drivers->RemoveAtIndex(idx, callDtor);
	}

	COmegaPtrArray *apis = (COmegaPtrArray *)(self + 0x1c);
	drivers->Shrink();

	while (*(int *)(self + 0x28) != 0) {
		int count = *(int *)(self + 0x28);
		void *elem = *(void **)(*(int *)(self + 0x30) + (count - 1) * 4);
		int callDtor = *(int *)(self + 0x20);
		unsigned idx = apis->FindIndex(elem);
		apis->RemoveAtIndex(idx, callDtor);
	}
	apis->Shrink();
}

int CSysApiInstance::EnableMultiTask(int /*enable*/)
{
	return 0; /* Tier-B link-stub. See sysapi_instance.h. */
}

void CSysApiInstance::WriteMessageToHost(int /*a*/, int /*b*/)
{
	/* Tier-B link-stub. */
}

void CSysApiInstance::AddModule(CModule *module)
{
	((CModuleManager *)g_poModuleManager)->AddModule(module);
}

void CSysApiInstance::RegisterApi(const char * /*name*/, CApiBase * /*api*/)
{
	/* Tier-B link-stub -- .text+0x0806bab0, 1099 bytes. See sysapi_instance.h. */
}
