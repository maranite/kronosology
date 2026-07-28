/*
 * korg_kmp.cpp  -  CKorgKmp partial reconstruction. See include/korg_kmp.h
 * for full ground-truth provenance and the deferred-method list.
 */

#include "korg_kmp.h"

#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

void CKorgKmp::CMultisampleChunk::GetName(char *dest, unsigned int /*maxLen*/) const
{
	strncpy(dest, mName, 0x11);
	dest[0x10] = 0;
}

void CKorgKmp::CMultisampleChunk::SetName(const char *name)
{
	strncpy(mName, name, 0x10);
}

void CKorgKmp::CMultisampleRelativeChunk::GetName(char *dest, unsigned int /*maxLen*/) const
{
	strncpy(dest, mName, 0xd);
	dest[0xc] = 0;
}

void CKorgKmp::CMultisampleRelativeChunk::SetName(const char *name)
{
	strncpy(mName, name, 0xc);
}

CKorgKmp::CKorgKmp(const char *name, const char *displayName, unsigned int field13a,
                    KorgType type, unsigned int field150, unsigned int field154,
                    unsigned int field158, unsigned int field15c,
                    unsigned char field160, unsigned char field161)
	: CKorgRiff(name, ".KMP")
	, mFlagA(0)
	, mFlagB(1)
	, mField13a(field13a)
	, mType(type)
	, mField144(0)
	, mField148(0)
	, mField14c(0)
	, mField150(field150)
	, mField154(field154)
	, mField158(field158)
	, mField15c(field15c)
	, mField160(field160)
	, mField161(field161)
{
	mDisplayName[0] = 0;

	if (displayName) {
		switch (type) {
		case Left:
			CKorgFile::MakeNameLeft(displayName, mDisplayName, 0x18);
			break;
		case Right:
			CKorgFile::MakeNameRight(displayName, mDisplayName, 0x18);
			break;
		case Mono:
		default:
			CKorgFile::MakeName(displayName, mDisplayName, 0x18);
			break;
		}
	}
}

CKorgKmp::~CKorgKmp()
{
}

const char *CKorgKmp::TypeString(KorgType type)
{
	switch (type) {
	case Mono:  return "Mono";
	case Left:  return "Left";
	case Right: return "Right";
	default:    return "Unknown";
	}
}

bool CKorgKmp::IsBigEndian() const
{
	return true;
}

void CKorgKmp::MakeFolder()
{
	char buf[0x100];
	CKorgFile::GetFolder(buf, sizeof(buf));
	mkdir(buf, 0777);
}

void CKorgKmp::GetName(char *dest, unsigned int /*maxLen*/) const
{
	strncpy(dest, mDisplayName, 0x19);
	dest[0x18] = 0;
}

bool CKorgKmp::IsStereoCounterpart(const CKorgKmp *other) const
{
	if (mType == Mono || other->mType == Mono)
		return false;

	char a[0x19];
	strncpy(a, mDisplayName, 0x19);
	char *dashA = strrchr(a, '-');
	if (dashA)
		*dashA = 0;

	char b[0x19];
	strncpy(b, other->mDisplayName, 0x19);
	char *dashB = strrchr(b, '-');
	if (dashB)
		*dashB = 0;

	return strcmp(a, b) == 0;
}

bool CKorgKmp::CanAddSample(unsigned char low, unsigned char high) const
{
	if (mRelativeChunks.empty())
		return true;

	std::list<CMultisampleRelativeChunk *>::const_iterator it2 = mRelativeChunks.begin();
	std::list<CMultisampleRelative3Chunk *>::const_iterator it3 = mRelative3Chunks.begin();

	for (; it2 != mRelativeChunks.end(); ++it2, ++it3) {
		const CMultisampleRelativeChunk *rec2 = *it2;
		const CMultisampleRelative3Chunk *rec3 = *it3;

		if (rec3->mUnknownHigh <= high) {
			if (rec2->mUnknownLow >= low)
				return false;
		}
	}

	return true;
}
