/*
 * res_family.cpp  -  see include/res_family.h.
 */

#include "res_family.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

namespace {
typedef void (*OpaqueDeletingDtorFn)(void *);

inline void CallOpaqueDeletingDtor(void *obj)
{
	void *vtbl = *(void **)obj;
	OpaqueDeletingDtorFn fn = *(OpaqueDeletingDtorFn *)((char *)vtbl + 4);
	fn(obj);
}

extern "C" {
void CResFamily_Dtor(void *self)
{
	((CResFamily *)self)->~CResFamily();
}

void CResFamily_DeletingDtor(void *self)
{
	((CResFamily *)self)->~CResFamily();
	free(self);
}

int CResFamily_PostKernelConstructorVSlot(void *self, unsigned long flags)
{
	return ((CResFamily *)self)->PostKernelConstructor(flags);
}

int CResFamily_PostKernelDestructorVSlot(void *self, unsigned long flags)
{
	return ((CResFamily *)self)->PostKernelDestructor(flags);
}
} // extern "C"
} // namespace

/* This specific TPtrArray<T> specialization's own vtable, installed by
 * PostKernelConstructor() over the generic COmegaPtrArray it heap-allocates -- see
 * header comment. Element type not confirmed, but UNLIKE PTR__CRMApi_08e88de8's
 * transient vtable (mains.cpp, genuinely never survives to be dispatched through),
 * this one DOES get dispatched for real: PostKernelDestructor()'s own real body
 * (self+8 != NULL branch) calls through vtbl+4 (the D0 deleting-dtor slot) on every
 * teardown. 4-slot EvaVTableStub-backed placeholder -- real slot COUNT matches the
 * shape every other TPtrArray<T> flavor in this project uses (PTR__TPtrArray_08e80bc8/
 * 08e80c40, omega_vtables.h/.cpp), safe-no-op behavior per that header's own
 * "shape faithful, not behavior faithful" convention.
 */
extern "C" void *PTR__TPtrArray_08e80f88[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* Real 6-slot vtable, confirmed by direct raw-byte read of the real Eva binary at
 * .rodata+0x08e80f48. Slots 3/5 (PostKernelConstructor/PostKernelDestructor) are
 * real overrides; slots 2/4 (PreKernelConstructor/PreKernelDestructor) stay
 * CGlobalObjectBase's own no-ops.
 */
extern "C" void *PTR__CResFamily_08e80f48[6] = {
	(void *)CResFamily_Dtor,
	(void *)CResFamily_DeletingDtor,
	(void *)CGlobalObjectBase_PreKernelConstructor,
	(void *)CResFamily_PostKernelConstructorVSlot,
	(void *)CGlobalObjectBase_PreKernelDestructor,
	(void *)CResFamily_PostKernelDestructorVSlot,
};

CResFamily::CResFamily() : CGlobalObjectBase()
{
	char *self = (char *)this;

	mVtbl = &PTR__CResFamily_08e80f48[0];

	*(unsigned char *)(self + 0x30) = 0;
	*(int *)(self + 0x18) = 0;
	*(int *)(self + 0x1c) = 0;
	*(int *)(self + 0x10) = 0;
	*(int *)(self + 0x14) = 0;
	*(unsigned char *)(self + 0x20) = 0xff;
	*(int *)(self + 0x24) = 1;
	*(int *)(self + 4) = 0;
	*(int *)(self + 8) = 0;
	*(int *)(self + 0x28) = 1;
	*(int *)(self + 0x2c) = 1;
}

CResFamily::~CResFamily()
{
	/* Real: redundantly reasserts this class's own vtable before the implicit
	 * CGlobalObjectBase::~CGlobalObjectBase() call downgrades it. */
	mVtbl = &PTR__CResFamily_08e80f48[0];
}

int CResFamily::PostKernelConstructor(unsigned long)
{
	char *self = (char *)this;

	void *raw = malloc(0x18);
	COmegaPtrArray *arr = new (raw) COmegaPtrArray(4, 4, 1);
	*(void **)raw = &PTR__TPtrArray_08e80f88[0];
	*(COmegaPtrArray **)(self + 8) = arr;

	return 0;
}

int CResFamily::PostKernelDestructor(unsigned long)
{
	char *self = (char *)this;
	void *p;

	p = *(void **)(self + 4);
	if (p != 0)
		CallOpaqueDeletingDtor(p);
	*(int *)(self + 4) = 0;

	p = *(void **)(self + 8);
	if (p != 0)
		CallOpaqueDeletingDtor(p);
	*(int *)(self + 8) = 0;

	void *ptr = *(void **)(self + 0x10);
	free(ptr); /* real: free(NULL) is a safe no-op -- self+0x10 is never set by
	            * anything in this reconstruction's own scope, faithfully preserved
	            * dead branch. */

	return 0;
}

/* Real global array, .bss+g_atResFamilies -- 32 instances, real static initializer
 * (global.constructors.keyed.to.g_atResFamilies@08069b90.c). See header comment.
 */
CResFamily g_atResFamilies[RES_FAMILY_COUNT];
