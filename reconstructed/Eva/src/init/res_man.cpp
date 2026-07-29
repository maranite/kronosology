/*
 * res_man.cpp  -  see include/res_man.h.
 */

#include "res_man.h"
#include "omega_vtables.h"
#include "res_family.h"
#include "system_api.h"

#include <cstdlib>
#include <new>

extern CSystemApi *Api; /* mains.cpp */
extern void HAL_DisableInterrupts();
extern void HAL_EnableInterrupts();

namespace {

/* Real Api+0x94 soft-assert-report call -- same slot/shape as res_entry.cpp's
 * own ApiAssert(); real calls here are diagnostic-only, the code always falls
 * through and completes the operation regardless (see each caller's own
 * comment in res_man.h).
 */
inline void ApiAssert(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}

} // namespace

/* Real class-static global (symbols.csv: CResMan::SysName, 4 bytes, a `const
 * char*`) -- same "opaque, content not decoded" treatment as file_man.cpp's own
 * CFileMan_SysName / mains.cpp's CEditMan_SysName and siblings.
 */
extern "C" const char *CResMan_SysName = "ResMan";

CResMan::CResMan()
	: CModule(CResMan_SysName)
	, mResults(2, 0, 1)
{
	/* Transient CRMApiCallBack-interface identity -- overwritten below once this
	 * class' own final vtable is installed (matches ground truth's own two-step
	 * write exactly).
	 */
	mCallbackVtbl = (void *)PTR__CRMApiCallBack_08e886e8;

	/* CRMJob -- real placement-construction (Eva "size is not depth" re-check
	 * batch, 2026-07-26; see res_man.h's own updated header comment). Real
	 * HAL_DisableInterrupts()/HAL_EnableInterrupts() brackets dropped, same
	 * established convention as every other kernel-side critical section in
	 * this userspace reconstruction -- see e.g. scheduler.cpp.
	 */
	mJob = new (malloc(0x54)) CRMJob();

	/* Manual vtable-swap idiom: install this class' own real (final) vtable now
	 * that CModule's base ctor has finished. The CRMApiCallBack-interface identity
	 * above is ALSO finalized here -- it physically lives in the same combined
	 * 21-element array (index 14 onward, a genuine secondary Itanium-ABI vtable,
	 * NOT a plain continuation of the primary one) rather than being a
	 * separately-declared array; see omega_vtables.h's own byte-verified
	 * derivation.
	 */
	*reinterpret_cast<void **>(this) = (void *)PTR__CResMan_08e88b08;
	mCallbackVtbl = (void *)&PTR__CResMan_08e88b08[14];

	*reinterpret_cast<int *>(mUnknown34 + 0x00) = -1;      /* absolute +0x34 */
	*reinterpret_cast<int *>(mUnknown34 + 0x04) = 0;       /* +0x38 */
	*reinterpret_cast<int *>(mUnknown34 + 0x08) = 0;       /* +0x3c */
	*reinterpret_cast<int *>(mUnknown34 + 0x0c) = 0;       /* +0x40 */
	*reinterpret_cast<int *>(mUnknown34 + 0x10) = 0;       /* +0x44 */
	*reinterpret_cast<int *>(mUnknown34 + 0x20) = 0;       /* +0x54 */
	*reinterpret_cast<int *>(mUnknown34 + 0x24) = 0;       /* +0x58 */
	*reinterpret_cast<int *>(mUnknown34 + 0x14) = 0;       /* +0x48 */
	*reinterpret_cast<int *>(mUnknown34 + 0x18) = 0;       /* +0x4c */
	*reinterpret_cast<int *>(mUnknown34 + 0x28) = 0;       /* +0x5c */
	*reinterpret_cast<int *>(mUnknown34 + 0x30) = 0;       /* +0x64 (this+100) */
	*reinterpret_cast<int *>(mUnknown34 + 0x38) = 0;       /* +0x6c */
	*reinterpret_cast<int *>(mUnknown34 + 0x3c) = 0;       /* +0x70 */
	mUnknown34[0x34] = (unsigned char)0xff;                /* +0x68 */
	mUnknown34[0x35] = (unsigned char)0xff;                /* +0x69 */
	mUnknown34[0x36] = (unsigned char)0xff;                /* +0x6a */

	mResultCount = 0;

	/* mResults already 3-int-constructed (member init list); real ctor installs
	 * its own vtable-swap target then immediately clears it (redundant -- see
	 * omega_ptr_array.h's own RemoveAll() header comment).
	 */
	*reinterpret_cast<void **>(&mResults) = (void *)PTR__TPtrArray_08e88bb8;
	mResults.RemoveAll(1);

	/* mChunks[257] already default-constructed (member init, implicit, same
	 * sequential order as ground truth's own 1-then-256-in-a-loop construction).
	 */

	/* 10x TVector<CResEntryEx,1> installs, absolute offsets 0x20b4..0x2198 (this
	 * class' own trailing region, mTail, starts at absolute 0x20b0) -- see
	 * res_man.h's own header comment on why this stays a flat byte buffer instead
	 * of a fixed-stride sub-object array.
	 */
	for (int i = 0; i < 10; i++) {
		unsigned char *slot = mTail + 4 + i * 0x18;
		*reinterpret_cast<void **>(slot) = (void *)PTR__TVector_08e88ba8;
		*reinterpret_cast<int *>(slot + 4) = 0;
		*reinterpret_cast<int *>(slot + 8) = 0;
		*reinterpret_cast<int *>(slot + 0xc) = 0;
	}
}

void CResMan::Start()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

/* Round 55 batch -- see res_man.h. Neither of these touches `this`. */

int CResMan::IsAutoUnloadEnabled(unsigned char family) const
{
	return *reinterpret_cast<int *>(
		reinterpret_cast<unsigned char *>(&g_atResFamilies[family]) + 0x28);
}

void CResMan::SetLoadRes(unsigned char family, int value) const
{
	if (family > 0x1f)
		ApiAssert("ResMan.cpp", 0xc7a);
	*reinterpret_cast<int *>(
		reinterpret_cast<unsigned char *>(&g_atResFamilies[family]) + 0x2c) = value;
}

bool CResMan::TestAndSetBusy(CRMApiCallBack *owner)
{
	if (owner == 0)
		ApiAssert("ResMan.cpp", 0xc38);

	HAL_DisableInterrupts();
	unsigned char *slot = mUnknown34 + 0x1c; /* absolute this+0x50 */
	CRMApiCallBack *cur = *reinterpret_cast<CRMApiCallBack **>(slot);
	if (cur == 0)
		*reinterpret_cast<CRMApiCallBack **>(slot) = owner;
	HAL_EnableInterrupts();
	return cur == 0;
}

void CResMan::UnPrepareDestMap(STriplet **map) const
{
	STriplet *ptr = *map;
	if (ptr == 0) {
		ApiAssert("ResMan.cpp", 0x949);
		ptr = *map;
	}
	if (ptr != 0) {
		HAL_DisableInterrupts();
		free(ptr);
		HAL_EnableInterrupts();
	}
	*map = 0;
}

extern "C" void CResManStartVSlot(void *obj)
{
	static_cast<CResMan *>(obj)->Start();
}

/* Real vtable definition -- 21 elements (omega_vtables.h: 12 real primary slots +
 * 2 non-callable secondary-header words + 7 real secondary slots, direct
 * .rodata-byte-read-confirmed, NOT a plain 21-slot dispatch array -- see that
 * header comment for the full derivation). Slot 4 (Start) wired to the real
 * forwarder above; slots 2/3 (Setup/Config) and 7-11 (OnSave/OnDelete/OnLoad/
 * OnSetRes/OnLoadRes) are real methods too but genuinely out of scope for this
 * ctor-focused pass, stay EvaVTableStub; indices 12/13 are the secondary header's
 * own offset-to-top/typeinfo words (not function pointers, never dispatched
 * through -- declared `0`, not EvaVTableStub, so nothing mistakes them for a
 * callable slot); indices 14-20 (the real secondary vtable) are install-only.
 */
void *PTR__CResMan_08e88b08[21] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,                              /* 0-1  dtor pair */
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)CResManStartVSlot,   /* 2-4  Setup/Config/Start */
	(void *)EvaVTableStub, (void *)EvaVTableStub,                              /* 5-6  Destroy/GetErrorMsg */
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,       /* 7-9  OnSave/OnDelete/OnLoad */
	(void *)EvaVTableStub, (void *)EvaVTableStub,                              /* 10-11 OnSetRes/OnLoadRes */
	0, 0,                                                                       /* 12-13 secondary header, not callable */
	(void *)EvaVTableStub, (void *)EvaVTableStub,                              /* 14-15 secondary dtor pair */
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,       /* 16-18 secondary OnSetRes/OnLoadRes/OnLoad */
	(void *)EvaVTableStub, (void *)EvaVTableStub,                              /* 19-20 secondary OnSave/OnDelete */
};
