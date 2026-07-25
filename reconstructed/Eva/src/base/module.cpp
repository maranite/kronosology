/*
 * module.cpp  -  see include/module.h.
 *
 * CModule::CModule(const char*) transcribed from CModule@0807c330.c (156 bytes).
 * CModule::AdjustTaskMask() (.text+0x0807c640, 458 bytes) transcribed from
 * AdjustTaskMask@0807c640.c -- Tier A, Stage 6 breadth sweep 2026-07-25, see
 * module.h's header comment for the full behavior writeup.
 *
 * CModule's own real vtable (PTR__CModule_08e81fe8, 7 ground-truth-counted slots) now
 * lives in omega_vtables.h/omega_vtables.cpp, matching this project's established
 * per-class vtable-sizing convention (Stage 6, 2026-07-25) -- previously a bare
 * `void* = 0` here, upgraded now that its real slot count is confirmed. Still never
 * dispatched through by any reconstructed code -- see omega_vtables.cpp's own comment
 * on this array for why.
 */

#include "module.h"
#include "task.h"
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
	/* Real body -- see module.h's header comment. Reverse walk over mTasks
	 * (this class's own absolute +0x14 = count, +0x1c = flat void* array),
	 * clearing bit 0x02 of each task's own +0x4c mask/flags byte -- the un-mask
	 * half of the same gate CLevelManager::RunLevel() checks (scheduler.cpp).
	 *
	 * The real disassembly re-reads mTasks' own count field on every iteration
	 * rather than hoisting it once, and treats an out-of-range index as a NULL
	 * task pointer -- then unconditionally dereferences it anyway (a literal
	 * near-NULL write to fixed address 0x4c in the real binary). Preserved
	 * faithfully: nothing in this reconstruction's own call graph mutates
	 * mTasks during this call (no reconstructed AddTask()/RemoveTask() exists),
	 * so the re-read always matches the original count and this branch is
	 * never actually taken here -- same "faithful but currently-dead defensive
	 * branch" shape as CModuleManager's own mPhase-never-set finding
	 * (module_manager.h) and CTaskBuffer::SendBuffer()'s always-empty mHead
	 * walk (task_buffer.h). The 8x loop unrolling itself is collapsed to a
	 * plain reverse loop, same license used throughout this project.
	 */
	unsigned char *self = (unsigned char *)this;

	int count = *(int *)(self + 0x14);
	for (int i = count - 1; i >= 0; --i) {
		int cur = *(int *)(self + 0x14);
		unsigned char *task = 0;
		if (i < cur)
			task = *(unsigned char **)(*(int *)(self + 0x1c) + i * 4);
		*(task + 0x4c) &= 0xfd;
	}
}

CTask *CModule::Add(CTask *task)
{
	/* Real body -- see module.h's header comment. `mTasks` is treated by raw offset
	 * (this+8), same convention AdjustTaskMask() above already uses, so this works
	 * correctly against either a real CModule or a raw offset-matched test buffer.
	 */
	((COmegaPtrArray *)((unsigned char *)this + 8))->Add(task);

	/* Real: two Api vtable-slot notifications, both call-contract-only (system_api.h)
	 * -- neither slot's real behavior is decoded, both discard any return value, same
	 * CallVSlot idiom used throughout this project for undecoded CSystemApi slots.
	 */
	typedef void (*NotifyTaskFn)(void *, CTask *);
	typedef void (*NotifyModuleFn)(void *, CModule *);
	void *apiVtbl = *(void **)Api;
	NotifyTaskFn notifyTask = *(NotifyTaskFn *)((char *)apiVtbl + 0x134);
	notifyTask(Api, task);
	NotifyModuleFn notifyModule = *(NotifyModuleFn *)((char *)apiVtbl + 0x12c);
	notifyModule(Api, this);

	return task;
}
