/*
 * omega_vtables.h  -  real PTR__ClassName_<addr> vtable-slot arrays for classes this
 * project hasn't reconstructed the methods of, but whose objects get manually
 * vtable-swapped into existence throughout ckernel.cpp/scheduler.cpp/module_manager.cpp/
 * mains.cpp (the "base-construct then overwrite this+0" idiom -- see
 * omega_ptr_array.h's header comment for why).
 *
 * Real slot counts recovered directly from symbols.csv's own vtable/typeinfo layout
 * (Decomp/EVA_Decomp/eva_export/symbols.csv, the 08e80a80..08e814a0 cluster where GCC
 * laid out every Itanium-ABI vtable back-to-back, immediately followed by every
 * typeinfo object back-to-back) -- for each class, slot count = (next "vtable"-labeled
 * symbol's address - this class's own PTR_~ClassName address) / 4, i.e. the real
 * distance to the next class's vtable header or (for the last class in a contiguous
 * run) to the first typeinfo object, both being real, load-bearing boundaries in the
 * actual binary:
 *
 *   CHostInterfaceBase  08e80b68 -> 08e80bc0 (TPtrArray<CGlobalObjectBase>)  = 22 slots
 *   CHostInterface      08e80b08 -> 08e80b60 (CHostInterfaceBase)            = 22 slots
 *   COmegaPtrArray      08e80be0 -> 08e80bf0 (TNamedPtrArray<CModuleConstructor>) = 4 slots
 *   TNamedPtrArray<CModuleConstructor> 08e80bf8 -> 08e80c08 (TNamedPtrArray<CModule>) = 4 slots
 *   TNamedPtrArray<CModule>            08e80c10 -> 08e80c20 (CLevelManagerArray) = 4 slots
 *   TPtrArray<CLevelManager>           08e80c40 -> 08e80c50 (TVector<...>)   = 4 slots
 *   TVector<CTimerObject*,1>           08e80c58 -> 08e80c60 (CMessageInput)  = 2 slots
 *   CDummyMsgInput      08e80c80 -> 08e80c8c (start of typeinfo cluster)     = 3 slots
 *   CNamedObjectBase    08e81378 -> 08e81380 (start of typeinfo cluster)     = 2 slots
 *   CTracer             08e81468 -> 08e81474 (start of typeinfo cluster)    = 3 slots
 *   CLevelManagerArray  08e80c28 -> 08e80c38 (TPtrArray<CLevelManager>)     = 4 slots
 *   CLevelManager       08e80e50 -> 08e80ea0 (TNamedPtrArray<CTask>)        = 20 slots
 *   TNamedPtrArray<CTask> 08e80ea8 -> 08e80eb4 (start of typeinfo cluster)  = 3 slots
 *
 * Added in the Api/SysApiInstance pass (2026-07-23), same methodology, plus one direct
 * raw-byte read (08e81008+0xa4, confirmed == CSysApiInstance::RegisterApi's own real
 * .text address 0806bab0 -- see sysapi_instance.h) that pins down CSysApiInstance's own
 * vtable identity (it's the one Api's runtime type actually installs), not just its slot
 * count:
 *
 *   TPtrArray<CGlobalObjectBase> 08e80bc8 -> 08e80bd8 (COmegaPtrArray)      = 4 slots
 *     (this is sm_poGlobalObjectList's own real vtable -- CKernel::AddGlobalObject
 *     installs it, see global_object_base.h/ckernel.cpp; never dispatched through by
 *     any code on this pass's own traced boot path, same "install-only" status as
 *     everything else in this file)
 *   CSysApiInstance     08e81008 -> 08e81180 (typeinfo-name, no further vtable
 *     symbol in this run) = 94 slots -- Api's own real installed vtable once
 *     SysApiInstance's static constructor runs (sysapi_instance.cpp). Genuinely
 *     dispatched through by Mains()'s existing 17-member MMainXxx family (mains.cpp,
 *     CallVSlot at +0x40/+0xa0/+0xb4) -- unlike the opaque single-symbol vtables
 *     mains.cpp installs locally for classes nothing dispatches through, this one
 *     needs the real slot count so those raw offset reads stay in-bounds. +0xa4 is
 *     confirmed (not just "shape-sized") to be RegisterApi -- mains.cpp's own 8-member
 *     MMainXxx(void) family (ckernel.cpp's InitSystemLayer()) calls it by name
 *     directly rather than through this array, now that the identity is known.
 *   TNamedPtrArray<CDriverBase>    08e811a8 -> 08e811b8 (next vtable header) = 4 slots
 *   TNamedPtrArray<CApiDescriptor> 08e811c0 -> 08e811e0 (CSystemApi)         = 8 slots
 *     (SysApiInstance's own 2 embedded COmegaPtrArray sub-objects install these --
 *     see sysapi_instance.h's corrected +4/+0x1c field mapping. Install-only, like
 *     the TPtrArray<CGlobalObjectBase> entry above.)
 *
 * Every slot points at the same no-op stub (EvaVTableStub, cdecl, zero declared
 * parameters) -- safe under the real cdecl calling convention regardless of how many
 * args/regs a caller's own Fn-typedef pushes, since cdecl callees never pop caller-
 * pushed arguments themselves. This is "shape faithful" (real slot COUNT, matching the
 * real binary's own vtable size) but explicitly NOT "behavior faithful" -- every slot
 * is a safe no-op, not the real virtual method. Anything that ever actually dispatches
 * through one of these at runtime silently does nothing rather than reading garbage
 * memory or crashing; still not something to rely on for correctness.
 */

#ifndef OMEGA_VTABLES_H
#define OMEGA_VTABLES_H

extern "C" {
extern void *PTR__CHostInterfaceBase_08e80b68[22];
extern void *PTR__CHostInterface_08e80b08[22];
extern void *PTR__COmegaPtrArray_08e80be0[4];
extern void *PTR__TNamedPtrArray_08e80bf8[4];
extern void *PTR__TNamedPtrArray_08e80c10[4];
extern void *PTR__TPtrArray_08e80c40[4];
extern void *PTR__TVector_08e80c58[2];
extern void *PTR__CDummyMsgInput_08e80c80[3];
extern void *PTR__CNamedObjectBase_08e81378[2];
extern void *PTR__CTracer_08e81468[3];
extern void *PTR__CLevelManagerArray_08e80c28[4];
extern void *PTR__CLevelManager_08e80e50[20];
extern void *PTR__TNamedPtrArray_08e80ea8[3];
extern void *PTR__TPtrArray_08e80bc8[4];
extern void *PTR__CSysApiInstance_08e81008[94];
extern void *PTR__TNamedPtrArray_08e811a8[4];
extern void *PTR__TNamedPtrArray_08e811c0[8];
}

#endif /* OMEGA_VTABLES_H */
