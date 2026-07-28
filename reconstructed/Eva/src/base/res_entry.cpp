/*
 * res_entry.cpp  -  see include/res_entry.h.
 *
 * Transcribed from CResInfo/CResEntry/CResEntryEx bodies at .text+0x081507e0..
 * 0x081512c0.
 */

#include "res_entry.h"
#include "system_api.h"

#include <cstring>

extern CSystemApi *Api; /* mains.cpp */

namespace {

/* Real Api+0x94 soft-assert-report call -- same slot/shape already established
 * in tempo.cpp/config_manager.cpp/partition_table.cpp/etc. Real calls here (not
 * dropped): "ResInfo.cpp" line 0x21 (ctor-from-buffer, Deserialize) or 0x33
 * (Serialize), confirmed by direct .rodata string read.
 */
inline void ApiAssert(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}

} // namespace

/* ---- CResInfo ---- */

CResInfo::CResInfo()
{
	std::memset(mResName, 0, sizeof(mResName));
	mTail[0] = mTail[1] = mTail[2] = mTail[3] = mTail[4] = 0xff;
	mTail[5] = 0;
}

CResInfo::CResInfo(const unsigned char *src)
{
	/* Real: NULL soft-asserts (ResInfo.cpp:0x21) then falls through into the
	 * same copy anyway -- see file header. Not exercised with an actual NULL
	 * by this reconstruction's own KAT (would reproduce ground truth's own
	 * post-assert NULL dereference).
	 */
	if (src == 0)
		ApiAssert("ResInfo.cpp", 0x21);

	std::memcpy(mResName, src, sizeof(mResName));
	std::memcpy(mTail, src + 0x12, sizeof(mTail));
}

void CResInfo::Deserialize(const unsigned char *src)
{
	if (src == 0)
		ApiAssert("ResInfo.cpp", 0x21);

	std::memcpy(mResName, src, sizeof(mResName));
	std::memcpy(mTail, src + 0x12, sizeof(mTail));
}

void CResInfo::Serialize(unsigned char *dst) const
{
	if (dst == 0)
		ApiAssert("ResInfo.cpp", 0x33);

	std::memcpy(dst, mResName, sizeof(mResName));
	std::memcpy(dst + 0x12, mTail, sizeof(mTail));
}

unsigned CResInfo::SizeOf()
{
	return 0x18;
}

void CResInfo::SetResName(const char *name)
{
	strncpy(mResName, name, 0x11);
	mResName[0x11] = 0;
}

void CResInfo::ResetResName()
{
	std::memset(mResName, 0, sizeof(mResName));
}

/* ---- CResEntry ---- */

CResEntry::CResEntry(STriplet id, const char *name, unsigned char a, unsigned char b, int pos, int size)
	: mIndex(0xffff), mPos(pos), mSize(size), mInfo()
{
	mInfo.SetResName(name);
	mInfo.mTail[0] = id.b0;
	mInfo.mTail[1] = id.b1;
	mInfo.mTail[2] = id.b2;
	mInfo.mTail[3] = a;
	mInfo.mTail[4] = b;
}

CResEntry::CResEntry(STriplet id, const char *name, unsigned short index, unsigned char a, unsigned char b)
	: mIndex(index), mPos(-1), mSize(-1), mInfo()
{
	mInfo.SetResName(name);
	mInfo.mTail[0] = id.b0;
	mInfo.mTail[1] = id.b1;
	mInfo.mTail[2] = id.b2;
	mInfo.mTail[3] = a;
	mInfo.mTail[4] = b;
}

CResEntry::CResEntry(const CResEntry &other)
	: mIndex(other.mIndex), mPos(other.mPos), mSize(other.mSize), mInfo()
{
	mInfo.SetResName(other.mInfo.mResName);
	mInfo.mTail[0] = other.mInfo.mTail[0];
	mInfo.mTail[1] = other.mInfo.mTail[1];
	mInfo.mTail[2] = other.mInfo.mTail[2];
	mInfo.mTail[3] = other.mInfo.mTail[3];
	mInfo.mTail[4] = other.mInfo.mTail[4];
}

CResEntry &CResEntry::operator=(const CResEntry &other)
{
	if (this != &other)
		Copy(other);
	return *this;
}

void CResEntry::Reset()
{
	mIndex = 0xffff;
	mPos = -1;
	mSize = -1;
	mInfo.ResetResName();
	mInfo.mTail[0] = mInfo.mTail[1] = mInfo.mTail[2] = mInfo.mTail[3] = mInfo.mTail[4] = 0xff;
}

void CResEntry::Copy(const CResEntry &other)
{
	mIndex = other.mIndex;
	mPos = other.mPos;
	mSize = other.mSize;
	mInfo.SetResName(other.mInfo.mResName);
	mInfo.mTail[0] = other.mInfo.mTail[0];
	mInfo.mTail[1] = other.mInfo.mTail[1];
	mInfo.mTail[2] = other.mInfo.mTail[2];
	mInfo.mTail[3] = other.mInfo.mTail[3];
	mInfo.mTail[4] = other.mInfo.mTail[4];
}

/* ---- CResEntryEx ---- */

CResEntryEx::CResEntryEx(STriplet id, const char *name, unsigned char a, unsigned char b, int pos, int size,
                          unsigned extra)
	: CResEntry(id, name, a, b, pos, size), mExtra(extra)
{
}

CResEntryEx::CResEntryEx(STriplet id, const char *name, unsigned char a, unsigned char b, int pos, int size)
	: CResEntry(id, name, a, b, pos, size), mExtra(0)
{
}

CResEntryEx::CResEntryEx(STriplet id, const char *name, unsigned short index, unsigned char a, unsigned char b,
                          unsigned extra)
	: CResEntry(id, name, index, a, b), mExtra(extra)
{
}

CResEntryEx::CResEntryEx(STriplet id, const char *name, unsigned short index, unsigned char a, unsigned char b)
	: CResEntry(id, name, index, a, b), mExtra(0)
{
}

CResEntryEx::CResEntryEx(const CResEntryEx &other)
	: CResEntry(), mExtra(0)
{
	Copy(other);
	mExtra = other.mExtra;
}

CResEntryEx::CResEntryEx(const CResEntry &other)
	: CResEntry(), mExtra(0)
{
	Copy(other);
	mExtra = 0;
}

CResEntryEx &CResEntryEx::operator=(const CResEntryEx &other)
{
	if (this != &other) {
		Copy(other);
		mExtra = other.mExtra;
	}
	return *this;
}

CResEntryEx &CResEntryEx::operator=(const CResEntry &other)
{
	if (this != &other) {
		Copy(other);
		mExtra = 0;
	}
	return *this;
}

void CResEntryEx::CopyEx(const CResEntryEx &other)
{
	Copy(other);
	mExtra = other.mExtra;
}

void CResEntryEx::CopyEx(const CResEntry &other)
{
	Copy(other);
	mExtra = 0;
}

void CResEntryEx::Reset()
{
	CResEntry::Reset();
	mExtra = 0;
}
