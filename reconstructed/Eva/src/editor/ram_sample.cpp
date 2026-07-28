/* ram_sample.cpp - see include/ram_sample.h for full ground-truth provenance. */

#include "ram_sample.h"

/* ==================== CRamSample ==================== */

/* .text+0x08427db0 -- real body is a bare `ret`, no field initialization at
 * all. mFS/mName etc. start uninitialized in ground truth; left default-
 * initialized here too rather than fabricating a zero-init ground truth
 * never performs. */
CRamSample::CRamSample()
{
}

/* .text+0x08427dc0 -- bare `ret`. */
CRamSample::~CRamSample()
{
}

unsigned long CRamSample::GetFS() const { return mFS; }
void CRamSample::SetFS(unsigned long v) { mFS = v; }

int CRamSample::GetLoopTune() const { return mLoopTune; }
void CRamSample::SetLoopTune(int v) { mLoopTune = (unsigned char)v; }

bool CRamSample::IsNotUse2ndStart() const { return (mFlags & 0x04) != 0; }
void CRamSample::SetNotUse2ndStart(int v)
{
	if (v)
		mFlags |= 0x04;
	else
		mFlags &= ~0x04;
}

bool CRamSample::IsOneShot() const { return (mFlags & 0x80) != 0; }

int CRamSample::IsPlus12dB() const { return mFlags & 0x08; }
void CRamSample::SetPlus12dB(int v)
{
	if (v)
		mFlags |= 0x08;
	else
		mFlags &= ~0x08;
}

int CRamSample::IsReverse() const { return mFlags & 0x20; }

unsigned CRamSample::GetFlag() const { return mFlags; }
void CRamSample::SetFlag(unsigned v) { mFlags = (unsigned char)v; }

int CRamSample::GetBank() const { return 0; }
void CRamSample::SetBank(int) { /* real ground-truth no-op, see file header */ }

unsigned long CRamSample::GetStartAddress(int flag) const
{
	if (flag && !(mFlags & 0x04))
		return m2ndStartAddress;
	return mStartAddress;
}
unsigned long CRamSample::GetStartAddress() const { return mStartAddress; }
void CRamSample::SetStartAddress(unsigned long v) { mStartAddress = v; }

unsigned long CRamSample::Get2ndStartAddress() const { return m2ndStartAddress; }
void CRamSample::Set2ndStartAddress(unsigned long v) { m2ndStartAddress = v; }

unsigned long CRamSample::GetLoopStartAddress() const { return mLoopStartAddress; }
void CRamSample::SetLoopStartAddress(unsigned long v) { mLoopStartAddress = v; }

unsigned long CRamSample::GetEndAddress() const { return mEndAddress; }
void CRamSample::SetEndAddress(unsigned long v) { mEndAddress = v; }

unsigned long CRamSample::GetTopAddress() const { return mTopAddress; }
void CRamSample::SetTopAddress(unsigned long v) { mTopAddress = v; }

unsigned long CRamSample::GetNumOfByte() const { return mNumOfByte; }
void CRamSample::SetNumOfByte(unsigned long v) { mNumOfByte = v; }

char *CRamSample::GetName() { return mName; }

/* .text+0x08427fc0, 0x2d bytes. `unusedIndex` is a genuine dead parameter --
 * ground truth never reads that stack slot, see file header. */
void CRamSample::Initialize(int /*unusedIndex*/, unsigned long startAddr, unsigned long numBytes)
{
	mStartAddress = startAddr;
	m2ndStartAddress = startAddr;
	mLoopStartAddress = startAddr;
	mEndAddress = startAddr + numBytes - 1;
	mTopAddress = startAddr * 2;
	mNumOfByte = numBytes * 2;
	mFlags = 0xd2;
}

/* ==================== CMultiSample ==================== */

/* .text+0x084280f0 / 0x08428100 -- both bare `ret`, no field init. */
CMultiSample::CMultiSample()
{
}
CMultiSample::~CMultiSample()
{
}

unsigned CMultiSample::GetTopOfRelative() const { return mTopOfRelative; }
void CMultiSample::SetTopOfRelative(int v)
{
	/* ground truth reads only the low byte of `v`, see file header */
	mTopOfRelative = (unsigned char)v;
}

unsigned CMultiSample::GetNumOfRelative() const { return mNumOfRelative; }
void CMultiSample::SetNumOfRelative(int v) { mNumOfRelative = (unsigned char)v; }

bool CMultiSample::IsNotUse2ndStart() const { return (mFlags & 0x01) != 0; }
void CMultiSample::SetFlag(int v) { mFlags = (unsigned char)v; }

char *CMultiSample::GetName() { return mName; }

/* ==================== CRamSampleRelative ==================== */

/* .text+0x08428180 -- zeroes the hidden tail dword only, see file header. */
CRamSampleRelative::CRamSampleRelative() : mReserved0xc(0)
{
}

/* .text+0x08428190 -- bare `ret`. */
CRamSampleRelative::~CRamSampleRelative()
{
}

unsigned CRamSampleRelative::GetSampleNumber() const { return mSampleNumber; }
void CRamSampleRelative::SetSampleNumber(int v) { mSampleNumber = (unsigned short)v; }

unsigned CRamSampleRelative::GetSampleBank() const { return mSampleBank; }
void CRamSampleRelative::SetSampleBank(int v) { mSampleBank = (unsigned char)v; }

unsigned CRamSampleRelative::GetTopKey() const { return mTopKey; }
void CRamSampleRelative::SetTopKey(int v) { mTopKey = (unsigned char)v; }

unsigned CRamSampleRelative::GetOriginalKey() const { return mOriginalKey; }
void CRamSampleRelative::SetOriginalKey(int v) { mOriginalKey = (unsigned char)v; }

unsigned CRamSampleRelative::GetTune() const { return mTune; }
void CRamSampleRelative::SetTune(int v) { mTune = (unsigned char)v; }

unsigned CRamSampleRelative::GetLevel() const { return mLevel; }
void CRamSampleRelative::SetLevel(int v) { mLevel = (unsigned char)v; }

/* aliased with Get/SetPan() -- both pairs touch the exact same byte, see
 * file header. */
unsigned CRamSampleRelative::GetTranspose() const { return (unsigned char)mTransposeOrPan; }
void CRamSampleRelative::SetTranspose(int v) { mTransposeOrPan = (signed char)v; }

int CRamSampleRelative::GetPan() const { return mTransposeOrPan; }
void CRamSampleRelative::SetPan(int v) { mTransposeOrPan = (signed char)v; }

void CRamSampleRelative::SetupAsSkipped() { mSampleNumber = 0xffff; }
bool CRamSampleRelative::IsSkipped() const { return mSampleNumber == 0xffff; }

int CRamSampleRelative::GetCutoff() const { return mCutoff; }
void CRamSampleRelative::SetCutoff(int v) { mCutoff = (signed char)v; }

int CRamSampleRelative::GetResonance() const { return mResonance; }
void CRamSampleRelative::SetResonance(int v) { mResonance = (signed char)v; }

int CRamSampleRelative::GetAttack() const { return mAttack; }
void CRamSampleRelative::SetAttack(int v) { mAttack = (signed char)v; }

int CRamSampleRelative::GetDecay() const { return mDecay; }
void CRamSampleRelative::SetDecay(int v) { mDecay = (signed char)v; }
