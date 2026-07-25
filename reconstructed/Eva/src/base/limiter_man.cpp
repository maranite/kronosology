/*
 * limiter_man.cpp  -  see include/limiter_man.h.
 *
 * Transcribed from CLimiterMan@0807bd10.c (46 bytes) and CLimiterMan@0807bbc0.c (97
 * bytes, the D1 destructor). Tier A.
 */

#include "limiter_man.h"
#include "omega_vtables.h"

#include <cstdlib>

CLimiterMan::CLimiterMan(CTask *owner)
{
	mVtbl = (void *)PTR__CLimiterMan_08e81ee8;
	mOwnerTask = owner;
	mVtbl2 = (void *)PTR__TVector_08e81f78;
	mBegin = 0;
	mEnd = 0;
	mCap = 0;
}

CLimiterMan::~CLimiterMan()
{
	mVtbl = (void *)PTR__CLimiterMan_08e81ee8;

	/* Real: virtual call THROUGH EACH ELEMENT'S OWN vtable at slot+4 (index 1),
	 * i.e. `elem->vtbl[1](elem)` -- not a call through CLimiterMan's own vtable,
	 * and not COmegaPtrArray's generic slot+8 idiom (this array isn't a
	 * COmegaPtrArray -- see header comment). See limiter_man.h for what this
	 * models (a not-reconstructed CLimiterBase::Release()-shaped method).
	 */
	typedef void (*ReleaseFn)(void *);
	for (void **p = (void **)mBegin; p != (void **)mEnd; p++) {
		void *elem = *p;
		if (elem) {
			void *elemVtbl = *(void **)elem;
			ReleaseFn release = *(ReleaseFn *)((char *)elemVtbl + 4);
			release(elem);
		}
	}

	mVtbl2 = (void *)PTR__TVector_08e81f78;
	if (mBegin)
		free(mBegin);

	mVtbl = (void *)PTR__CIfcUnknown_08e81d80;
}
