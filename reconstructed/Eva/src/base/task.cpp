/*
 * task.cpp  -  see include/task.h.
 *
 * CTask::CTask(...) transcribed from CTask@0807ee80.c (330 bytes). Tier A.
 * CTask::~CTask() transcribed from CTask@0807e350.c (800 bytes, D1). Tier A.
 * CTask::SetMask(int) transcribed from CTask@0807e840.c (40 bytes). Tier A.
 * CTask::RegisterIfc(CIfcUnknown*) transcribed directly from `objdump -dr` of
 * .text+0x0807ec90 (472 bytes) -- now Tier A. See task.h header comment for the full
 * disassembly trace.
 * TVector_SRegisteredIfc_MakeCapacity() (below) transcribed from `objdump -dr` of
 * .text+0x08182220 (539 bytes,
 * _ZN7TVectorIN5CTask14SRegisteredIfcELi1EE12MakeCapacityEj) -- the first real
 * TVector<T,1>::MakeCapacity() transcription in this project. See task.h header
 * comment.
 *
 * Exception-unwind paths the real ctor's/dtor's own try/catch regions cover (malloc/
 * COmegaPtrArray-ctor failure cleanup, and the dtor's own 2 real
 * TNamedPtrArray<COutLink>::~TNamedPtrArray() unwind-path calls) are omitted, same
 * "happy path only" license already used for every other ctor/dtor reconstructed in
 * this project (e.g. CTaskBuffer, CModule -- neither models its own unwind path
 * either).
 */

#include "task.h"
#include "module.h"
#include "limiter_man.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "system_api.h"

#include <cstdlib>
#include <cstring>
#include <new>

/* Real module-scope global (mains.cpp). Same CallVSlot-at-+0x3c idiom
 * CModule::CModule() already uses (module.cpp).
 */
extern CSystemApi *Api;

/* Test/stub-only placeholder ctor -- see task.h header comment. Not ground truth. */
CTask::CTask()
{
	memset(this, 0, sizeof(*this));
}

CTask::CTask(const CModule &owner, const char *name, int level, int scheduleFlag,
             unsigned short lastArg)
{
	mVtbl = (void *)PTR__CNamedObjectBase_08e81378;
	mName = 0;

	size_t len = strlen(name);
	char *dup = (char *)malloc(len + 1);
	mName = dup;
	strcpy(dup, name);

	mVtbl = (void *)PTR__CTask_08e82128;
	mIfcThunk = (void *)&EvaDataPlaceholder_08e82144;

	new (mOutLinks) COmegaPtrArray();
	*(void **)mOutLinks = (void *)PTR__TNamedPtrArray_08e82198;

	new (mIfcArray) COmegaPtrArray(1, 0, 0);
	mOwnerModule = &owner;
	*(void **)mIfcArray = (void *)PTR__TNamedPtrArray_08e82198;
	mLevel = level;
	mLastArg = lastArg;

	*(void **)mRegisteredIfcs = (void *)PTR__TVector_08e82188;
	*(void **)(mRegisteredIfcs + 4) = 0;
	*(void **)(mRegisteredIfcs + 8) = 0;
	*(void **)(mRegisteredIfcs + 0xc) = 0;

	new (mLimiterMan) CLimiterMan(this);

	/* Real: base mask value keyed off scheduleFlag (0 / nonzero-not-2 / ==2), THEN
	 * possibly bumped by +2 (bit 0x02 -- the exact bit CModule::AdjustTaskMask()
	 * clears, module.cpp) if the owning module hasn't finished starting yet -- see
	 * task.h's header comment for the full cross-confirming writeup.
	 */
	mPeriod = 1;
	mCountdown = 1;
	mMask = 0x04;
	unsigned char altMask = 0x06;
	if (scheduleFlag != 0) {
		mMask = 0x0c;
		altMask = 0x0e;
		if (scheduleFlag == 2) {
			mMask = 0x0d;
			altMask = 0x0f;
		}
	}
	/* Real: `*(int *)(param_1 + 0x24)` -- CModule's own private mState field, read
	 * by raw offset the same way this whole project treats CModule/CTask as opaque
	 * blobs across class boundaries (module.h/module_manager.h/level_manager_array.h).
	 */
	int ownerState = *(const int *)(reinterpret_cast<const unsigned char *>(&owner) + 0x24);
	if (ownerState < 4)
		mMask = altMask;

	typedef int (*Fn)(void *);
	void *apiVtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)apiVtbl + 0x3c);
	mScopeId = fn(Api);

	RegisterIfc(reinterpret_cast<CIfcUnknown *>(mLimiterMan));
}

void CTask::Add(COutLink *link)
{
	/* Real: `mOutLinks.Add(link)` (already real, omega_ptr_array.h) then a genuine
	 * tail-jmp into `(*Api)[0x12c/4]` with `(Api, mOwnerModule)` -- the exact SAME
	 * `NotifyModuleFn` call `CModule::Add(CTask*)` makes through this identical
	 * slot (module.cpp) -- see task.h header comment for the full disassembly
	 * trace that established this.
	 */
	((COmegaPtrArray *)mOutLinks)->Add(link);

	typedef void (*NotifyModuleFn)(void *, const CModule *);
	void *apiVtbl = *(void **)Api;
	NotifyModuleFn notifyModule = *(NotifyModuleFn *)((char *)apiVtbl + 0x12c);
	notifyModule(Api, mOwnerModule);
}

/* TVector<CTask::SRegisteredIfc,1>::MakeCapacity(unsigned int), .text+0x08182220,
 * 539 bytes -- see task.h header comment for the full algorithm writeup. `tvec` is
 * mRegisteredIfcs' own raw byte buffer (same "operate by fixed offset on an opaque
 * unsigned char[]" convention as every other embedded array in this class):
 *   tvec+0x0  vtbl (untouched here)
 *   tvec+0x4  mBegin
 *   tvec+0x8  mEnd
 *   tvec+0xc  mCap
 * `SRegisteredIfc` is a real 12-byte (3-dword) POD element, confirmed by this
 * function's own element-size arithmetic (see below).
 */
static void TVector_SRegisteredIfc_MakeCapacity(unsigned char *tvec, unsigned int n)
{
	const unsigned int kElemSize = 12;

	unsigned char *begin = *(unsigned char **)(tvec + 4);
	unsigned char *cap   = *(unsigned char **)(tvec + 0xc);
	unsigned int curCapElems = (unsigned int)(cap - begin) / kElemSize;
	if (n <= curCapElems)
		return;

	unsigned int newCapElems = 10;
	if (n > 10) {
		do {
			newCapElems *= 2;
		} while (n > newCapElems);
	}
	unsigned int newBytes = newCapElems * kElemSize;

	/* Real: HAL_DisableInterrupts()/HAL_EnableInterrupts() bracket both malloc and
	 * free below -- not modeled, same established convention as every other HAL_*
	 * bracket in this project (e.g. out_link.cpp, circ_byte_buffer.cpp).
	 */
	void *newBlock = malloc(newBytes);

	unsigned char *oldBegin = *(unsigned char **)(tvec + 4);
	unsigned char *oldEnd   = *(unsigned char **)(tvec + 8);
	unsigned int copyBytes = 0;
	if (oldEnd != oldBegin) {
		copyBytes = (unsigned int)(oldEnd - oldBegin);
		/* Real: GCC's own Duff's-device-unrolled (x4, mod-4 prologue switch)
		 * 12-byte element copy loop -- collapsed to memcpy, identical result
		 * (SRegisteredIfc is trivially-copyable POD, no side effects to preserve).
		 */
		memcpy(newBlock, oldBegin, copyBytes);
	}

	*(unsigned char **)(tvec + 8) = (unsigned char *)newBlock + copyBytes;
	free(oldBegin);
	*(unsigned char **)(tvec + 4) = (unsigned char *)newBlock;
	*(unsigned char **)(tvec + 0xc) = (unsigned char *)newBlock + newBytes;
}

void CTask::RegisterIfc(CIfcUnknown *ifc)
{
	/* Real body -- see task.h header comment for the full disassembly trace. */
	typedef void *(*IfcKeyFn)(CIfcUnknown *);
	void *ifcVtbl = *(void **)ifc;
	IfcKeyFn getKey = *(IfcKeyFn *)((char *)ifcVtbl + 8);

	/* Called unconditionally, even if mRegisteredIfcs is empty -- ground truth does
	 * this before the empty check, reproduced faithfully. */
	void *key = getKey(ifc);

	unsigned char *begin = *(unsigned char **)(mRegisteredIfcs + 4);
	unsigned char *end   = *(unsigned char **)(mRegisteredIfcs + 8);

	unsigned char *found = 0;
	for (unsigned char *p = begin; p != end; p += 12) {
		if (*(void **)p == key) {
			found = p;
			break;
		}
	}

	if (found) {
		/* Real: two soft, non-enforcing Api diagnostic calls (vtbl+0x90 with the
		 * literal string "CTask::RegisterIfc - Error: multiple registration for
		 * same interface", then vtbl+0x94 with this project's standard soft-assert
		 * shape -- "Assertion failed in module %s, line %i.\n" / "Task.cpp" / 179)
		 * -- omitted, same established non-enforcing-diagnostic convention as every
		 * other Api+0x90/+0x94 call site in this project (ev_buffers_pool.h/
		 * client_comm_server.h/chunk_man.h/etc.). Real behavior after logging:
		 * return immediately, array untouched.
		 */
		return;
	}

	/* Not found -- append. Real: ifc->vtbl[2](ifc) called a SECOND time here (a
	 * fresh call, not the cached `key` above) -- reproduced as-is. */
	void *key2 = getKey(ifc);

	unsigned char *cap = *(unsigned char **)(mRegisteredIfcs + 0xc);
	if (end == cap) {
		unsigned int usedElems = (unsigned int)(end - begin) / 12;
		TVector_SRegisteredIfc_MakeCapacity(mRegisteredIfcs, usedElems + 1);
		end = *(unsigned char **)(mRegisteredIfcs + 8);
	}

	*(void **)end       = key2;
	*(void **)(end + 4) = ifc;
	*(void **)(end + 8) = 0;
	*(unsigned char **)(mRegisteredIfcs + 8) = end + 12;
}

void CTask::SetMask(int mask)
{
	if (mask == 0)
		mMask &= ~0x01;
	else
		mMask |= 0x01;
}

CTask::~CTask()
{
	mVtbl = (void *)PTR__CTask_08e82128;

	/* (1) Entry notification to Api -- vtbl slot+0x140, arg = this. See
	 * system_api.h.
	 */
	typedef void (*NotifyTaskFn)(void *, CTask *);
	void *apiVtbl = *(void **)Api;
	NotifyTaskFn notifyDestroy = *(NotifyTaskFn *)((char *)apiVtbl + 0x140);
	notifyDestroy(Api, this);

	/* (2) Fully drain mOutLinks front-to-back. Real: GCC's own 8-way Duff's-device
	 * unrolling of this exact loop -- collapsed here, same license as
	 * omega_ptr_array.cpp's own 5 methods.
	 */
	typedef void (*NotifyOutLinkFn)(void *, void *);
	NotifyOutLinkFn notifyOutLink = *(NotifyOutLinkFn *)((char *)apiVtbl + 0x58);
	COmegaPtrArray *outLinks = reinterpret_cast<COmegaPtrArray *>(mOutLinks);
	while (*(int *)(mOutLinks + 0xc) != 0) {
		void *first = *(void **)(*(void **)(mOutLinks + 0x14));
		notifyOutLink(Api, first);
		outLinks->RemoveAtIndex(0, 1);
	}

	/* (3) Destroy the embedded CLimiterMan sub-object. */
	reinterpret_cast<CLimiterMan *>(mLimiterMan)->~CLimiterMan();

	/* (4) Inlined ~TVector<CTask::SRegisteredIfc,1>(): free the backing array if
	 * non-null. Real, separate symbol only actually called from the unwind path
	 * (not modeled) -- ground truth inlines its trivial body here instead.
	 */
	*(void **)mRegisteredIfcs = (void *)PTR__TVector_08e82188;
	void *regBegin = *(void **)(mRegisteredIfcs + 4);
	if (regBegin)
		free(regBegin);

	/* (5) Destroy mIfcArray then mOutLinks -- each via the shared
	 * TNamedPtrArray<COutLink> identity, COmegaPtrArray::Destroy(), then downgrade
	 * to the base COmegaPtrArray identity. Real, separate
	 * TNamedPtrArray<COutLink>::~TNamedPtrArray() symbol likewise only called from
	 * the unwind path -- inlined here in the normal path, same as (4).
	 */
	*(void **)mIfcArray = (void *)PTR__TNamedPtrArray_08e82198;
	reinterpret_cast<COmegaPtrArray *>(mIfcArray)->Destroy();
	*(void **)mIfcArray = (void *)PTR__COmegaPtrArray_08e80be0;

	*(void **)mOutLinks = (void *)PTR__TNamedPtrArray_08e82198;
	outLinks->Destroy();
	*(void **)mOutLinks = (void *)PTR__COmegaPtrArray_08e80be0;

	/* (6) CTask's own +0x08 field (mIfcThunk) -- a secondary, this-adjusted vtable
	 * slot, not generic opaque data (see task.h/omega_vtables.h). Final identity
	 * install before peeling to the base classes.
	 */
	mIfcThunk = (void *)PTR__CMessageInput_08e80c68;

	/* (7) CNamedObjectBase's own inlined dtor body: install its vtable, free mName
	 * if non-null.
	 */
	mVtbl = (void *)PTR__CNamedObjectBase_08e81378;
	if (mName)
		free(mName);

	/* (8) Ultimate root base identity -- CObjectBase, RTTI-only, 0 own virtual
	 * function slots. Final act before returning.
	 */
	mVtbl = (void *)PTR__CObjectBase_08e79d68;
}
