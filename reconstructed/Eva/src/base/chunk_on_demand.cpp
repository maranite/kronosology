/*
 * chunk_on_demand.cpp  -  see include/chunk_on_demand.h.
 */

#include "chunk_on_demand.h"
#include "omega_vtables.h"

CChunkOnDemand::CChunkOnDemand()
	: mSlots(2, 4, 1)
{
	/* Manual vtable-swap idiom (mSlots is a sibling member, not a base, so its
	 * `protected` mVtbl isn't reachable by name here -- same raw-pointer-punning
	 * idiom used throughout this project, e.g. mains.cpp's `*(void**)module = ...`).
	 */
	*reinterpret_cast<void **>(&mSlots) = (void *)PTR__TPtrArray_08e885a0;

	mUnknown00 = 0;
	mUnknown1c = 0;
}
