/*
 * slot_pool.cpp  -  see include/slot_pool.h.
 */

#include "slot_pool.h"
#include "omega_vtables.h"

#include <cstdlib>

/* 2-slot (D1/D0) install-only placeholder -- see slot_pool.h's own CSlotStateFree note.
 * Real vtable base is 0x8e89660; +8 (0x8e89668) is where the real ground truth's own D1
 * dtor slot lives -- kept as the literal constant here for address fidelity even though
 * this reconstruction's own array is a fresh (not byte-identical) EvaVTableStub-backed
 * table, same convention as PTR__TPtrArray_08e80f88 (res_family.cpp).
 */
extern "C" void *PTR__CSlotStateFree_08e89668[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

CSlotStateFree::CSlotStateFree() : mVtbl(&PTR__CSlotStateFree_08e89668[0])
{
}

/* Real global, ds:0x0931b220 -- see slot_pool.h. A genuine C++ static object; the real
 * toolchain's own static-init machinery produces the equivalent of ground truth's merged
 * _GLOBAL__I_ function, same precedent as CKernelDeathNotifier's own g_oKernelDeathNotifier
 * (kernel_death_notifier.cpp).
 */
CSlotStateFree g_oSlotStateFreeInstance;

extern "C" {
void CSlotPool_Dtor(void *self)
{
	((CSlotPool *)self)->~CSlotPool();
}

void CSlotPool_DeletingDtor(void *self)
{
	((CSlotPool *)self)->~CSlotPool();
	free(self);
}

int CSlotPool_PreKernelConstructorVSlot(void *self, unsigned long flags)
{
	return ((CSlotPool *)self)->PreKernelConstructor(flags);
}

int CSlotPool_PostKernelDestructorVSlot(void *self, unsigned long flags)
{
	return ((CSlotPool *)self)->PostKernelDestructor(flags);
}
} // extern "C"

/* Real 6-slot vtable, confirmed by direct raw-byte read of the real Eva binary at
 * .rodata+0x08e89268 (vtable base 0x8e89260, +8 skips the Itanium ABI offset-to-top/RTTI
 * pair). Slots 3 (PreKernelConstructor) and 5 (PostKernelDestructor) are real overrides;
 * slots 2/4 (PostKernelConstructor/PreKernelDestructor) stay CGlobalObjectBase's own
 * no-ops -- symbols.csv shows no CSlotPool override for either.
 */
extern "C" void *PTR__CSlotPool_08e89268[6] = {
	(void *)CSlotPool_Dtor,
	(void *)CSlotPool_DeletingDtor,
	(void *)CSlotPool_PreKernelConstructorVSlot,
	(void *)CGlobalObjectBase_PostKernelConstructor,
	(void *)CGlobalObjectBase_PreKernelDestructor,
	(void *)CSlotPool_PostKernelDestructorVSlot,
};

CSlotPool::CSlotPool(unsigned capacity) : CGlobalObjectBase()
{
	mVtbl = &PTR__CSlotPool_08e89268[0];
	mCapacity = capacity;
	mArray = 0;
	mFreeListHead = 0;
}

CSlotPool::~CSlotPool()
{
	mVtbl = &PTR__CSlotPool_08e89268[0];
}

int CSlotPool::PreKernelConstructor(unsigned long)
{
	SSlot *array = new SSlot[mCapacity];

	mArray = array;
	mFreeListHead = array;

	for (unsigned i = 0; i < mCapacity; ++i) {
		array[i].mState = &g_oSlotStateFreeInstance;
		array[i].mIndex = (unsigned short)i;
		array[i].mNext = (i + 1 < mCapacity) ? &array[i + 1] : 0;
	}

	return 0;
}

int CSlotPool::PostKernelDestructor(unsigned long)
{
	if (mArray != 0) {
		delete[] mArray;
		mArray = 0;
	}
	return 0;
}
