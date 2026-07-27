/*
 * kernel_death_notifier.cpp  -  see include/kernel_death_notifier.h.
 */

#include "kernel_death_notifier.h"

#include <cstdlib>

namespace {
extern "C" {
void CKernelDeathNotifier_Dtor(void *self)
{
	((CKernelDeathNotifier *)self)->~CKernelDeathNotifier();
}

void CKernelDeathNotifier_DeletingDtor(void *self)
{
	((CKernelDeathNotifier *)self)->~CKernelDeathNotifier();
	free(self);
}

int CKernelDeathNotifier_PreKernelDestructorVSlot(void *self, unsigned long flags)
{
	return ((CKernelDeathNotifier *)self)->PreKernelDestructor(flags);
}
} // extern "C"
} // namespace

/* Real 6-slot vtable, confirmed by direct raw-byte read of the real Eva binary at
 * .rodata+0x08e80e08 (2-word ABI header skipped, same convention as every other
 * CGlobalObjectBase-derived vtable in this project). Only slot 4
 * (PreKernelDestructor) is overridden; the other 3 phase hooks are the unmodified
 * CGlobalObjectBase no-ops.
 */
extern "C" void *PTR__CKernelDeathNotifier_08e80e08[6] = {
	(void *)CKernelDeathNotifier_Dtor,
	(void *)CKernelDeathNotifier_DeletingDtor,
	(void *)CGlobalObjectBase_PreKernelConstructor,
	(void *)CGlobalObjectBase_PostKernelConstructor,
	(void *)CKernelDeathNotifier_PreKernelDestructorVSlot,
	(void *)CGlobalObjectBase_PostKernelDestructor,
};

CKernelDeathNotifier::CKernelDeathNotifier() : CGlobalObjectBase()
{
	mVtbl = &PTR__CKernelDeathNotifier_08e80e08[0];
	mDying = 0;
}

CKernelDeathNotifier::~CKernelDeathNotifier()
{
	/* Real: redundantly reasserts this class's own vtable before the implicit
	 * CGlobalObjectBase::~CGlobalObjectBase() call downgrades it -- same no-op
	 * re-store already seen in CNotifyList::~CNotifyList()/CResFamily::~CResFamily().
	 */
	mVtbl = &PTR__CKernelDeathNotifier_08e80e08[0];
}

int CKernelDeathNotifier::PreKernelDestructor(unsigned long)
{
	mDying = 1;
	return 0;
}

/* The real global object -- see header comment for why this is a genuine C++ static
 * rather than the raw-byte-buffer idiom used elsewhere. Registers itself into
 * CKernel::sm_poGlobalObjectList via the base ctor (global_object_base.cpp), same as
 * every other CGlobalObjectBase-derived global in this project; CKernel::~CKernel()'s
 * already-real teardown walk (ckernel.cpp) now genuinely dispatches
 * PreKernelDestructor() on this object too.
 */
static CKernelDeathNotifier g_oKernelDeathNotifier;
