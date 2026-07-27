/*
 * res_family.h  -  CResFamily, construction/destruction-only reconstruction.
 *
 * FOUND 2026-07-27 during a fresh broad-survey pass (see kernel_death_notifier.h's own
 * file header for the full survey method/results). `nm -C`/symbols.csv show
 * CResFamily : public CGlobalObjectBase with real PostKernelConstructor/
 * PostKernelDestructor overrides -- a class this project had never touched at all
 * (CConfigManager::CreateResourceFamilies(), config_manager.cpp, is the only prior
 * mention, and it stays a genuine Tier-B stub: its own real body depends on the
 * un-reconstructed 247-method CZ string-set container, out of scope). CResFamily
 * ITSELF is a separate, narrower, genuinely tractable slice: 32 instances
 * (`g_atResFamilies`, .bss) are constructed by a real, always-run static initializer
 * regardless of whether CreateResourceFamilies() ever runs or does anything -- same
 * "size is not depth" / narrow-scope precedent already used for CJobStack
 * (job_stack.h) and CRTRouterApiInstance's own phase hooks (mains.cpp WORKAROUND #4).
 *
 * REAL CONSTRUCTION (CResFamily::CResFamily(), .text+0x08063480, 100 bytes -- all
 * scalar field inits, no malloc, no other class dependency):
 *   global.constructors.keyed.to.g_atResFamilies@08069b90.c (194 bytes) calls this
 *   ctor 32 times in a row (8-way GCC-unrolled loop, collapsed to a plain loop here,
 *   same license as every other Duff's-device-style collapse in this project) over a
 *   flat array with a confirmed 0x48-byte (72-byte) per-object stride -- matches
 *   sizeof(CResFamily) exactly (CGlobalObjectBase's 4-byte mVtbl + this class's own
 *   fields, highest offset the ctor itself writes is +0x30, remainder is
 *   zero-initialized .bss, faithful either way).
 *
 * REAL TEARDOWN: ~CResFamily() D1 (.text+0x0817ce10, 23 bytes, re-stores this class's
 * own vtable then implicitly calls CGlobalObjectBase::~CGlobalObjectBase()) / D0
 * (.text+0x0817cfb0, 47 bytes, same + free(this) -- never exercised by any
 * reconstructed caller since g_atResFamilies is a static array, not heap-allocated;
 * installed for vtable shape only, same convention as CGlobalObjectBase's/
 * CNotifyList's own D0 treatment). The array's own batch-teardown helper
 * (`__tcf_0`, registered via `__cxa_atexit` in the same static initializer above) is
 * not itself modeled -- normal C++ static-array teardown on process exit, out of
 * scope for this project the same way __dso_handle/other libc-runtime plumbing always
 * has been.
 *
 * REAL PHASE-HOOK OVERRIDES (the actual reason this class matters to
 * CKernel::CKernel()'s/~CKernel()'s already-real sm_poGlobalObjectList walk,
 * ckernel.cpp):
 *   PostKernelConstructor (.text+0x080633a0, 82 bytes): heap-allocates a
 *     COmegaPtrArray(growBy=4, initialCapacity=4, ownFlag=1) (malloc(0x18) +
 *     placement-construct, already-reconstructed class, omega_ptr_array.h) into
 *     self+0x8, then overwrites ITS vtable slot with this specific TPtrArray<T>
 *     specialization's own vtable (PTR__TPtrArray_08e80f88) -- same
 *     base-construct-then-vtable-swap idiom as every COmegaPtrArray-derived flavor in
 *     this project. The specific T this array holds isn't confirmed (nothing in this
 *     reconstruction's own scope ever reads self+8 back -- every real consumer,
 *     AddGroupElem/GetGroupCount/etc, is one of CResFamily's own out-of-scope deep
 *     methods) -- PTR__TPtrArray_08e80f88 is declared as an opaque placeholder, same
 *     "installed but never dispatched, so a scalar 0 is safe" convention already used
 *     for PTR__CRMApi_08e88de8 (mains.cpp).
 *   PostKernelDestructor (.text+0x08063300, 82 bytes): generic opaque-vtable-slot+4
 *     "deleting dtor" dispatch on self+4 and self+8 if non-null (same idiom as
 *     CRMApiInstance_PostKernelDestructor, mains.cpp), then a plain free() on self+0x10
 *     (a 3rd, separately-owned raw buffer this project's own scope never allocates --
 *     stays NULL/no-op given nothing in scope ever sets it, faithfully preserved
 *     dead branch, not "fixed away").
 *
 * Deep per-field semantics (what self+0x10/+0x14/+0x18/+0x1c/+0x20/+0x24/+0x28/+0x2c/
 * +0x30 actually MEAN -- resource slot bookkeeping of some kind, given the method
 * names GetFreeRes/GetResFreePosZ/AddGroupElem/GetGroupCount/IsLoadedResListFull) are
 * NOT reconstructed -- every real consumer of those fields is one of CResFamily's own
 * 13 out-of-scope business-logic methods (SetName/SetSize/Clear/GetFreeRes/
 * GetResFreePosZ/SetLoadType/GetLoadedElemPos/GetLoadedElemCount/GetLoadedElem/
 * RemoveLoadedElem/AppendLoadedElem/IsLoadedResListFull x2/GetGroupCount/GetCountZ/
 * AddGroupElem), same CZ-container-adjacent "genuinely deep, real project-scope
 * boundary" verdict CreateResourceFamilies() itself already carries -- declared here
 * as an opaque byte buffer past CGlobalObjectBase's own 4-byte mVtbl, matching
 * CModuleManager's/CJobStack's own raw-offset-access convention for classes whose
 * full field semantics aren't modeled.
 */

#ifndef RES_FAMILY_H
#define RES_FAMILY_H

#include "global_object_base.h"

class CResFamily : public CGlobalObjectBase {
public:
	/* .text+0x08063480, 100 bytes. */
	CResFamily();

	/* .text+0x0817ce10 (D1) / .text+0x0817cfb0 (D0, shape-only -- see file header). */
	~CResFamily();

	/* .text+0x080633a0, 82 bytes. */
	int PostKernelConstructor(unsigned long);

	/* .text+0x08063300, 82 bytes. */
	int PostKernelDestructor(unsigned long);

private:
	/* Opaque past the inherited 4-byte mVtbl -- see file header "deep per-field
	 * semantics" note. Sized to the real 0x48-byte object (confirmed by
	 * g_atResFamilies' own array stride).
	 */
	unsigned char mOpaque[0x48 - sizeof(void *)];
};

/* Real global array, .bss -- 32 instances (confirmed by the static initializer's own
 * unrolled-loop trip count), each 0x48 bytes apart. See file header.
 */
#define RES_FAMILY_COUNT 32
extern CResFamily g_atResFamilies[RES_FAMILY_COUNT];

#endif /* RES_FAMILY_H */
