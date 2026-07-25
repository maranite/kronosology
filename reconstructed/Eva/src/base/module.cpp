/*
 * module.cpp  -  see include/module.h.
 *
 * CModule::CModule(const char*) transcribed from CModule@0807c330.c (156 bytes).
 * CModule::AdjustTaskMask() (.text+0x0807c640, 458 bytes) is a Tier-B link-stub, not
 * reconstructed -- see module.h.
 *
 * CModule's own real vtable (PTR__CModule_08e81fe8, 7 ground-truth-counted slots) now
 * lives in omega_vtables.h/omega_vtables.cpp, matching this project's established
 * per-class vtable-sizing convention (Stage 6, 2026-07-25) -- previously a bare
 * `void* = 0` here, upgraded now that its real slot count is confirmed. Still never
 * dispatched through by any reconstructed code -- see omega_vtables.cpp's own comment
 * on this array for why.
 */

#include "module.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "system_api.h"

#include <cstdlib>
#include <cstring>
#include <new>

/* Real module-scope global (mains.cpp's own definition); every real MMainXxx(void)
 * module ctor call happens with Api already set (MMainPanelDriver, Mains()'s first
 * call, always runs first on the real boot path -- see mains.cpp).
 */
extern CSystemApi *Api;

CModule::CModule(const char *name)
{
	mVtbl = (void *)PTR__CNamedObjectBase_08e81378;
	mName = 0;

	size_t len = strlen(name);
	char *dup = (char *)malloc(len + 1);
	mName = dup;
	strcpy(dup, name);

	mVtbl = (void *)PTR__CModule_08e81fe8;

	new (mTasks) COmegaPtrArray();
	*(void **)mTasks = (void *)PTR__TNamedPtrArray_08e80ea8;
	mUnknown20 = 0;
	mState = 0;

	/* Real: `(**(code**)(*Api + 0x3c))(Api)` -- vtable-slot dispatch on Api, same
	 * CallVSlot idiom used throughout the project for classes whose real vtable
	 * layout isn't reconstructed.
	 */
	typedef int (*Fn)(void *);
	void *apiVtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)apiVtbl + 0x3c);
	mScopeId = fn(Api);
}

void CModule::AdjustTaskMask()
{
	/* Tier-B link-stub -- .text+0x0807c640, 458 bytes. See module.h. */
}
