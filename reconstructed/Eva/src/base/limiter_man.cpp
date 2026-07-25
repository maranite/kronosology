/*
 * limiter_man.cpp  -  see include/limiter_man.h.
 *
 * Transcribed from CLimiterMan@0807bd10.c (46 bytes). Tier A.
 */

#include "limiter_man.h"
#include "omega_vtables.h"

CLimiterMan::CLimiterMan(CTask *owner)
{
	mVtbl = (void *)PTR__CLimiterMan_08e81ee8;
	mOwnerTask = owner;
	mVtbl2 = (void *)PTR__TVector_08e81f78;
	mBegin = 0;
	mEnd = 0;
	mCap = 0;
}
