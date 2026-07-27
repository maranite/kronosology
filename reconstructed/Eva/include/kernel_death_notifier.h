/*
 * kernel_death_notifier.h  -  CKernelDeathNotifier, the smallest CGlobalObjectBase
 * derived class in the binary.
 *
 * FOUND 2026-07-27 during a fresh broad-survey pass cross-referencing every real
 * PreKernelConstructor/PostKernelConstructor/PreKernelDestructor/PostKernelDestructor
 * override in the ground-truth binary against this project's own coverage (see
 * global_object_base.h's own "9 confirmed XxxApiInstance globals" list plus
 * CNotifyList/CEvBuffersPool/CRTRouterApiInstance/CRMApiInstance, already reconstructed
 * elsewhere). `nm -C`/symbols.csv turned up 4 more real overriding classes never
 * modeled at all: CResFamily (res_family.h, this same batch), CPool, CSlotPool
 * (both deferred -- see this batch's own status notes, they need a not-yet-modeled
 * TVector<T,N> template and CSlotStateFree singleton), and this class.
 *
 * CKernelDeathNotifier is a single real global object (`g_oKernelDeathNotifier`,
 * .bss), constructed by a real, always-run static initializer
 * (`global.constructors.keyed.to.g_oKernelDeathNotifier@0805e780.c`, 69 bytes,
 * textually adjacent to CKernel's own methods in ckernel.cpp's translation unit) --
 * confirmed via direct decompile:
 *
 *   void global_constructors_keyed_to_g_oKernelDeathNotifier(void)
 *   {
 *     CGlobalObjectBase::CGlobalObjectBase((CGlobalObjectBase*)&g_oKernelDeathNotifier);
 *     g_oKernelDeathNotifier._0_4_ = &PTR__CKernelDeathNotifier_08e80e08;
 *     g_oKernelDeathNotifier._4_4_ = 0;
 *     __cxa_atexit(CKernelDeathNotifier::~CKernelDeathNotifier, &g_oKernelDeathNotifier,
 *                  &__dso_handle);
 *   }
 *
 * -- i.e. exactly what a genuine `static CKernelDeathNotifier g_oKernelDeathNotifier;`
 * compiles to (no separate CKernelDeathNotifier::CKernelDeathNotifier() symbol exists
 * in the ground truth -- GCC inlined the whole 2-field ctor into this single call
 * site, same as it does for any trivial ctor with one call site). Modeled here as a
 * real C++ static object, same convention as CNotifyList's own `g_oNotifyList`
 * (edit_server.cpp) rather than the raw-byte-buffer-plus-manual-vtable-swap idiom used
 * for the 9 XxxApiInstance globals (whose owning classes aren't modeled at all) --
 * CKernelDeathNotifier's owning class IS fully modeled here, so the more faithful
 * genuine-object form applies.
 *
 * Real layout (2 fields, 8 bytes total: CGlobalObjectBase's own 4-byte mVtbl + 1 more):
 *   +0x00  mVtbl   inherited from CGlobalObjectBase, immediately overwritten with this
 *                  class's own vtable (PTR__CKernelDeathNotifier_08e80e08)
 *   +0x04  mDying  0 at construction; set to 1 by PreKernelDestructor() (the one real
 *                  override -- .text+0x0817cbe0, 14 bytes). Nothing in this
 *                  reconstruction reads it back -- CKernel::~CKernel()'s own
 *                  sm_poGlobalObjectList teardown walk dispatches PreKernelDestructor
 *                  on every registered object (ckernel.cpp, already real) but never
 *                  reads any object's own fields afterward, so this flag is real but
 *                  currently a pure "was the kernel already dying when I was torn
 *                  down" marker with no consumer -- same "real but not yet consumed
 *                  field" treatment as several other WORKAROUND-#4-era fields in this
 *                  project (mains.cpp).
 *
 * Only PreKernelDestructor is overridden -- PreKernelConstructor/PostKernelConstructor/
 * PostKernelDestructor stay CGlobalObjectBase's own no-ops (confirmed by the vtable's
 * own raw-byte-read shape below, same "which of the 4 phase hooks are real" spot-check
 * this project has done for every other CGlobalObjectBase-derived class it touches).
 */

#ifndef KERNEL_DEATH_NOTIFIER_H
#define KERNEL_DEATH_NOTIFIER_H

#include "global_object_base.h"

class CKernelDeathNotifier : public CGlobalObjectBase {
public:
	/* Inlined into the real static initializer -- see file header. No separate
	 * ground-truth symbol; modeled as an ordinary ctor. */
	CKernelDeathNotifier();

	/* .text+0x0817cbf0 (D1) / .text+0x0817cc10 (D0, `+ free(this)`, never exercised
	 * by any reconstructed caller -- installed for vtable shape only, matching
	 * CGlobalObjectBase's/CNotifyList's own D0 treatment). */
	~CKernelDeathNotifier();

	/* .text+0x0817cbe0, 14 bytes. */
	int PreKernelDestructor(unsigned long);

private:
	int mDying; /* +0x04 */
};

#endif /* KERNEL_DEATH_NOTIFIER_H */
