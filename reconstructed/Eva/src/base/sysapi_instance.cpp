/*
 * sysapi_instance.cpp  -  see include/sysapi_instance.h.
 *
 * Cleanup()/AddModule() transcribed from Cleanup@0806ca50.c (497 bytes) and
 * AddModule@0806b550.c (22 bytes). EnableMultiTask()/WriteMessageToHost() -- promoted
 * Tier B -> Tier A 2026-07-26 (broad Tier-B recheck sweep), see sysapi_instance.h.
 * RegisterApi() -- Tier A, see below and the header's own file comment.
 *
 * ConstructSysApiInstance() (new, 2026-07-23) transcribes
 * global.constructors.keyed.to.SysApiInstance@0806cc50.c (123 bytes) -- the real static
 * constructor that sets `Api = SysApiInstance;` before main(), fixing the
 * MMainEditMan() NULL-Api crash. See global_object_base.h for the full mechanism.
 *
 * RegisterApi() (promoted to Tier A, Stage 6 breadth sweep, 2026-07-25) transcribed
 * from RegisterApi@0806bab0.c (1099 bytes) -- see sysapi_instance.h for the full
 * per-branch accounting.
 */

#include "sysapi_instance.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "module_manager.h"
#include "global_object_base.h"
#include "system_api.h"
#include "scheduler.h"
#include "ckernel.h"

#include <new>
#include <cstring>
#include <cstdlib>
#include <cstdio>

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

int CSysApiInstance::EnableMultiTask(int enable)
{
	/* Real: pure tail call into g_poScheduler->Enable(enable) -- see sysapi_instance.h. */
	return g_poScheduler->Enable(enable);
}

namespace {
/* Same "vtable dispatch through an opaque, unreconstructed singleton" idiom used
 * throughout this project (ckernel.cpp's CallVSlot1/2, dump_task.cpp's CallVSlot1) --
 * not shared across translation units by convention. Real slot takes ONE pointer
 * argument (the formatted message buffer), not (a,b) -- the two ints are sprintf'd
 * into that buffer by the caller, not passed through.
 */
typedef void (*HostMessageFn)(void *, const char *);

inline void CallHostWriteMessage(void *hostInterface, const char *msg)
{
	void *vtbl = *(void **)hostInterface;
	HostMessageFn fn = *(HostMessageFn *)((char *)vtbl + 0xc);
	fn(hostInterface, msg);
}
} // namespace

void CSysApiInstance::WriteMessageToHost(int a, int b)
{
	/* Real: sprintf(buf, "%d\x0c%d\r", a, b) into a 0x1c-byte stack buffer, then an
	 * unchecked vtable dispatch (slot +0xc, one pointer arg -- the buffer) through
	 * g_poHostInterface -- see sysapi_instance.h.
	 */
	char buf[0x1c];
	sprintf(buf, "%d\x0c%d\r", a, b);
	CallHostWriteMessage(g_poHostInterface, buf);
}

void CSysApiInstance::AddModule(CModule *module)
{
	((CModuleManager *)g_poModuleManager)->AddModule(module);
}

void CSysApiInstance::AddConstructor(CModuleConstructor *ctor)
{
	((CModuleManager *)g_poModuleManager)->AddConstructor(ctor);
}

namespace {
typedef void (*ApiWarn1Fn)(void *, const char *, const char *);

/* Real Api+0x90 "soft log" call, one %s argument -- same slot/shape already
 * established in config_manager.cpp's own ApiWarn1() (each translation unit keeps
 * its own tiny local copy, matching this project's established per-file convention
 * rather than a shared header for a 2-line helper).
 */
inline void ApiWarn1(const char *fmt, const char *arg)
{
	void *vtbl = *(void **)Api;
	ApiWarn1Fn fn = *(ApiWarn1Fn *)((char *)vtbl + 0x90);
	fn(Api, fmt, arg);
}
} // namespace

int CSysApiInstance::RegisterApi(const char *name, CApiBase *api)
{
	char *self = (char *)this;

	/* mApis embedded at +0x04 (see header's own corrected field-mapping comment):
	 * own +0xc/+0x14 land at absolute +0x10 (count) / +0x18 (array), own +4
	 * ("call dtor on remove" flag) lands at absolute +0x08.
	 */
	int count = *(int *)(self + 0x10);
	void **array = *(void ***)(self + 0x18);

	int foundIndex = -1;
	for (int i = 0; i < count; i++) {
		void **entry = (void **)array[i];
		if (strcmp(name, (const char *)entry[1]) == 0) {
			foundIndex = i;
			break;
		}
	}

	if (foundIndex >= 0) {
		void **entry = (void **)array[foundIndex];

		if (entry[2] == (void *)api) {
			ApiWarn1("API <%s> already registered!", name);
			return 1;
		}

		/* Replacing an already-registered name under a different API pointer --
		 * real, but never exercised by this pass's own 7 boot-path callers
		 * (each registers a distinct, never-repeated name). Removes the old
		 * descriptor (dispatching TNamedPtrArray<CApiDescriptor>'s own "delete
		 * element" callback if mApis' own +0x04 flag says to -- EvaVTableStub-
		 * backed, see omega_vtables.h), then falls through to build a fresh one
		 * below, exactly like the not-found case.
		 */
		ApiWarn1("Replacing API <%s>!", name);
		int callDtor = *(int *)(self + 0x08);
		COmegaPtrArray *apis = (COmegaPtrArray *)(self + 4);
		apis->RemoveAtIndex((unsigned)foundIndex, callDtor);
	}

	/* Build a fresh 3-word {vtbl, name, api} descriptor -- identical shape to
	 * mains.cpp's own RegisterModuleDescriptor() helper, just installing
	 * CApiDescriptor's own vtable (PTR__CApiDescriptor_08e81368) at the end
	 * instead of a per-module one.
	 */
	void **descriptor = (void **)malloc(0xc);
	descriptor[0] = PTR__CNamedObjectBase_08e81378;
	descriptor[1] = 0;

	char *nameBuf = (char *)malloc(strlen(name) + 1);
	strcpy(nameBuf, name);
	descriptor[1] = nameBuf;

	descriptor[2] = api;
	descriptor[0] = PTR__CApiDescriptor_08e81368;

	COmegaPtrArray *apis = (COmegaPtrArray *)(self + 4);
	apis->Add(descriptor);
	return 1;
}
