/*
 * global_object_base.cpp  -  see include/global_object_base.h.
 */

#include "global_object_base.h"
#include "ckernel.h"

#include <cstdlib>

extern "C" {

/* D1 (complete-object) destructor thunk -- forwards to the real C++ destructor body. */
static void CGlobalObjectBase_Dtor(void *self)
{
	((CGlobalObjectBase *)self)->~CGlobalObjectBase();
}

/* D0 (deleting) destructor thunk -- real body additionally frees `this` (matches
 * ~CGlobalObjectBase@08063290's own trailing free(this)). Never exercised by any
 * reconstructed caller (nothing `delete`s a CGlobalObjectBase*) -- installed for shape
 * only.
 */
static void CGlobalObjectBase_DeletingDtor(void *self)
{
	((CGlobalObjectBase *)self)->~CGlobalObjectBase();
	free(self);
}

/* 4 real "phase hook" no-ops -- PreKernelConstructor/PostKernelConstructor/
 * PreKernelDestructor/PostKernelDestructor, all literally `return 0` in the real
 * binary (.text+0x0804cc10/0x0804cc20/0x0804cc30/0x0804cc40, 3 bytes each) and
 * confirmed unoverridden by every XxxApiInstance-derived class this project touches
 * (see header comment). Real signature takes an `unsigned long` param Ghidra couldn't
 * further resolve (no caller in this pass ever dispatches through these 4 slots with a
 * meaningful argument either).
 */
static int CGlobalObjectBase_PreKernelConstructor(unsigned long) { return 0; }
static int CGlobalObjectBase_PostKernelConstructor(unsigned long) { return 0; }
static int CGlobalObjectBase_PreKernelDestructor(unsigned long) { return 0; }
static int CGlobalObjectBase_PostKernelDestructor(unsigned long) { return 0; }

} // extern "C"

/* Real 6-slot vtable, confirmed by direct raw-byte read of the real Eva binary at
 * .rodata+0x08e80f08 (see header comment) -- every slot real, not a stub. */
void *PTR__CGlobalObjectBase_08e80f08[6] = {
	(void *)CGlobalObjectBase_Dtor,
	(void *)CGlobalObjectBase_DeletingDtor,
	(void *)CGlobalObjectBase_PreKernelConstructor,
	(void *)CGlobalObjectBase_PostKernelConstructor,
	(void *)CGlobalObjectBase_PreKernelDestructor,
	(void *)CGlobalObjectBase_PostKernelDestructor,
};

CGlobalObjectBase::CGlobalObjectBase()
{
	mVtbl = &PTR__CGlobalObjectBase_08e80f08[0];
	CKernel::AddGlobalObject(this);
}

CGlobalObjectBase::~CGlobalObjectBase()
{
	mVtbl = &PTR__CGlobalObjectBase_08e80f08[0];
	CKernel::RemoveGlobalObject(this);
}
