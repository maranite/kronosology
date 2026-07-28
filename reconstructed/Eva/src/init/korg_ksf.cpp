/*
 * korg_ksf.cpp  -  CKorgKsf partial reconstruction. See include/korg_ksf.h
 * for full ground-truth provenance and the deferred-method list.
 */

#include "korg_ksf.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

void CKorgKsf::CSampleChunk::GetName(char *dest, unsigned int /*maxLen*/) const
{
	strncpy(dest, mName, 0x11);
	dest[0x10] = 0;
}

void CKorgKsf::CSampleChunk::SetName(const char *name)
{
	strncpy(mName, name, 0x10);
}

unsigned int CKorgKsf::CSampleChunk::GetStartOffsetSamples(unsigned int idx) const
{
	if (idx > 1)
		return 0;
	return mStartOffsetSamples[idx];
}

void CKorgKsf::CSampleChunk::SetStartOffsetSamples(unsigned int idx, unsigned int value)
{
	if (idx > 1)
		return;
	mStartOffsetSamples[idx] = value;
}

void CKorgKsf::CSampleFileNameChunk::GetSampleFileName(char *dest, unsigned int /*maxLen*/) const
{
	strncpy(dest, mSampleFileName, 0xd);
	dest[0xc] = 0;
}

void CKorgKsf::CSampleFileNameChunk::SetSampleFileName(const char *name)
{
	if (!name) {
		mSampleFileName[0] = 0;
		return;
	}
	strncpy(mSampleFileName, name, 0xc);
}

void CKorgKsf::CSampleDataChunk::SetOneShot(bool oneShot)
{
	if (oneShot)
		mFlags |= 0x80;
	else
		mFlags &= 0x7f;
}

CKorgKsf::CKorgKsf(const char *name, const char * /*displayName*/, unsigned int field16a,
                    KorgType type, bool field154)
	: CKorgRiff(name, ".KSF")
	, mField16a(field16a)
	, mType(type)
	, mField154(field154)
	, mBuffer(0)
	, mDataSize(0)
{
	/* Real ground truth also builds a type-suffixed short display-name
	 * copy from `displayName` into an undocumented field this pass's own
	 * methods never read -- not reproduced, see file header.
	 */
}

CKorgKsf::~CKorgKsf()
{
}

const char *CKorgKsf::TypeString(KorgType type)
{
	switch (type) {
	case Mono:  return "Mono";
	case Left:  return "Left";
	case Right: return "Right";
	default:    return "Unknown";
	}
}

bool CKorgKsf::IsBigEndian() const
{
	return true;
}

void CKorgKsf::MakeSampleFileName(unsigned int a, unsigned int /*b*/, unsigned int c,
                                   char *dest, unsigned int /*maxLen*/)
{
	sprintf(dest, "MS%03u%03u", a, c + 1);
	CKorgFile::MakeFileName(dest, 0xd, ".KSF");
}

void CKorgKsf::SetSampleDataSize(unsigned int size, bool alloc)
{
	if (mBuffer) {
		free(mBuffer);
		mBuffer = 0;
	}

	mDataSize = size;

	if (size != 0 && alloc)
		mBuffer = malloc(size);
}
