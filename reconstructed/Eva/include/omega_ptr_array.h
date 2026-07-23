/*
 * omega_ptr_array.h  -  COmegaPtrArray, the flat-void*-array container underlying
 * CKernel's own sm_poGlobalObjectList and every TNamedPtrArray<T>/TPtrArray<T>
 * "derived by vtable swap" instance seen throughout ckernel.cpp/scheduler.cpp/
 * module_manager.cpp/mains.cpp (Stage 4).
 *
 * Real layout confirmed from COmegaPtrArray@080a6be0.c (default ctor) and the 3 real
 * methods below -- 0x18 bytes (6 dwords):
 *   +0x00  vtbl        raw vtable-pointer slot. NOT a real C++ vtable in this
 *                       reconstruction (COmegaPtrArray declares no virtual methods) --
 *                       every real derived flavor (TNamedPtrArray<CModule>,
 *                       TNamedPtrArray<CModuleConstructor>, TPtrArray<CLevelManager>,
 *                       ...) is produced by base-constructing a COmegaPtrArray then
 *                       manually overwriting this raw field with the derived class's
 *                       own PTR__ClassName_<addr> vtable -- same manual-vtable-swap
 *                       idiom already established in ckernel.cpp/mains.cpp. Declaring
 *                       real C++ virtual methods here would fight that pattern, so this
 *                       stays a plain void* and Destroy()/RemoveAtIndex() dispatch
 *                       through it by hand (CallVSlot-style), matching the original
 *                       disassembly's own `(**(code**)(*this+8))(this, elem)` idiom.
 *   +0x04  mUnknown04   ctor sets 1; never read back by any of the 5 reconstructed
 *                       methods. Possibly an "owns/auto-deletes elements" flag or a
 *                       fixed elemSize-in-words -- not confirmed either way.
 *   +0x08  mCapacity    allocated slot count (elements, not bytes)
 *   +0x0c  mCount       used slot count
 *   +0x10  mGrowBy      ctor sets 1; growth increment -- not exercised by any of the
 *                       5 reconstructed methods (Add()/SetAtIndex() would need it,
 *                       neither is reconstructed)
 *   +0x14  mArray       flat malloc'd void* array, NULL when empty
 *
 * 6 of COmegaPtrArray's real methods are reconstructed here (both ctors, Add, Destroy,
 * FindIndex, RemoveAtIndex, Shrink) -- all self-contained, no further Stage 4+
 * dependencies beyond the generic vtable-slot-8 "free element" callback dispatch (which
 * every real caller so far either never triggers with this pass's data, or is itself a
 * documented no-op stub -- see omega_vtables.cpp). RemoveAll()/SetAtIndex() are real
 * COmegaPtrArray methods too (symbols.csv: 080a7080/080a72f0) but nothing on any traced
 * call path invokes them -- not reconstructed, out of scope for this pass.
 *
 * Add() (.text+0x080a6da0, 343 bytes) was added in the Api/SysApiInstance pass
 * (2026-07-23) -- it's CKernel::AddGlobalObject()'s own real dependency (see
 * global_object_base.h): sm_poGlobalObjectList is a COmegaPtrArray built via the
 * 3-int constructor below (growBy=1, initialCapacity=0, ownFlag=0), and every
 * XxxApiInstance-style global registers itself into it via this method, through
 * CGlobalObjectBase's own constructor.
 */

#ifndef OMEGA_PTR_ARRAY_H
#define OMEGA_PTR_ARRAY_H

class COmegaPtrArray {
public:
	COmegaPtrArray();

	/* .text+0x080a6c10, 113 bytes -- the second real COmegaPtrArray constructor
	 * overload (symbols.csv: _ZN14COmegaPtrArrayC1Eiii). Real param order confirmed
	 * from CKernel::AddGlobalObject's own call site (`COmegaPtrArray(this,1,0,0)`)
	 * matched against each field's write: param 1 -> mGrowBy (+0x10), param 2 ->
	 * mCapacity (+0x08, also the initial malloc(param2*4) size when nonzero), param 3
	 * -> mUnknown04 (+0x04).
	 */
	COmegaPtrArray(int growBy, int initialCapacity, int ownFlag);

	/* .text+0x080a6da0, 343 bytes (symbols.csv: _ZN14COmegaPtrArray3AddEPKv). Real
	 * body grows the backing array by mGrowBy (copying the old contents across, GCC's
	 * usual 8-way-unrolled copy loop here too -- collapsed to a plain loop, same
	 * license as the other 5 methods) whenever mCount reaches mCapacity; returns
	 * INT_MAX (0x7fffffff, matching FindIndex's own "not found" sentinel) if mGrowBy
	 * is 0 (growth disabled) or the capacity arithmetic would overflow. Otherwise
	 * returns the index the element was stored at (mCount before the increment).
	 */
	int Add(const void *item);

	/* .text+0x080a6ca0, 224 bytes. Pops+frees every live element via the vtable-slot-8
	 * callback (only if mUnknown04 != 0, which the ctor always sets), then frees the
	 * backing array.
	 */
	void Destroy();

	/* .text+0x080a7200, 227 bytes. const in the real binary
	 * (_ZNK14COmegaPtrArray9FindIndexEPKv) -- linear scan, returns 0x7fffffff (INT_MAX)
	 * if not found, matching the real sentinel (not -1).
	 */
	unsigned FindIndex(const void *item) const;

	/* .text+0x080a6f20, 331 bytes. Real return value (bool: index valid) is discarded
	 * by every reconstructed caller so far; kept void here matching the existing
	 * ckernel.cpp call-contract this class was first declared under (Stage 3).
	 * callDtorCallback selects whether the vtable-slot-8 "free element" callback fires
	 * for the removed element before it's shifted out.
	 */
	void RemoveAtIndex(unsigned index, int callDtorCallback);

	/* .text+0x080a7310, 356 bytes. Real "shrink to fit": reallocates the backing array
	 * down to mCount slots (or frees it entirely if mCount == 0). Only does anything
	 * when mCount < mCapacity.
	 */
	void Shrink();

private:
	void  *mVtbl;
	int    mUnknown04;
	int    mCapacity;
	int    mCount;
	int    mGrowBy;
	void **mArray;
};

#endif /* OMEGA_PTR_ARRAY_H */
