/*
 * dir_entry.cpp  -  see include/dir_entry.h.
 */

#include "dir_entry.h"
#include "omega_vtables.h"

CDirEntry::CDirEntry()
	: mVtbl((void *)PTR__CDirEntry_08e81908),
	  mShortName(1), mShortExt(1), mLongName(1), mLongExt(1),
	  mUnknown44(0), mUnknown48(0), mUnknown4c(0),
	  mUnknown50(0), mUnknown51(0), mUnknown52(0), mUnknown53(0),
	  mUnknown54(0x13), mUnknown55(0x50), mUnknown56(0x01), mUnknown57(0x01),
	  mUnknown58(0), mUnknown5c(1), mUnknown60(0), mUnknown64(0)
{
}

CDirEntry::~CDirEntry()
{
	/* Real dtor: 4x conditional operator delete[] on the CZ members' own
	 * internal heap buffers -- CZ's own opaque dtor is a real no-op (see
	 * cz_util.h), so there is nothing further to release here.
	 */
}

/* .text+0x080723b0/0x080723e0. See header comment. */
const char *CDirEntry::GetName() const
{
	typedef int (*Fn)(const CDirEntry *);
	Fn fn = *(Fn *)((const char *)mVtbl + 0x8);
	bool useLong = fn(this) != 0;
	uint32_t raw = useLong ? mLongName.RawPtrField() : mShortName.RawPtrField();
	return (const char *)(uintptr_t)raw;
}

const char *CDirEntry::GetExt() const
{
	typedef int (*Fn)(const CDirEntry *);
	Fn fn = *(Fn *)((const char *)mVtbl + 0x8);
	bool useLong = fn(this) != 0;
	uint32_t raw = useLong ? mLongExt.RawPtrField() : mShortExt.RawPtrField();
	return (const char *)(uintptr_t)raw;
}
