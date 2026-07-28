/*
 * chunk_family.cpp  -  see include/chunk_family.h.
 *
 * Real per-class file-name literals confirmed via `objdump -s -j .rodata`:
 *   CChunkBase's inline-in-header asserts -> "Chunk.h"       (.rodata+0x8e7fa81)
 *   CChunkBase/CChunk out-of-line asserts -> "FileChunk.cpp" (.rodata+0x8e7fba3)
 *   CChunkInfoItem/CChunkInfoList asserts -> "ChunkInfo.cpp" (.rodata+0x8e7faef)
 * Shared format string "Assertion failed in module %s, line %i.\n"
 * (.rodata+0x8e7bef8) -- same string already established project-wide
 * (partition_table.cpp/stream_family.cpp/etc).
 */

#include "chunk_family.h"
#include "system_api.h"

extern CSystemApi *Api; /* mains.cpp */

namespace {

inline void ApiAssertImpl(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}

} // namespace

void CChunkBase::ApiAssertChunkH(int line) { ApiAssertImpl("Chunk.h", line); }
void CChunkBase::ApiAssertFileChunkCpp(int line) { ApiAssertImpl("FileChunk.cpp", line); }

static void ApiAssertChunkInfoCpp(int line) { ApiAssertImpl("ChunkInfo.cpp", line); }

/* ---------------------------------------------------------------------- *
 * CChunkBase
 * ---------------------------------------------------------------------- */

CChunkBase::CChunkBase(const SChkHeader &hdr)
	: mBasePos(0), mRelSonNestLev(0), mStatus(eClosed), mType(0), mSubtype(0),
	  mId(0), mFlags(0), mDeclaredLen(0), mParent(0), mOpenChild(0),
	  mAbsSonNumber(0)
{
	/* Real body: reserved-bits check on hdr.flags (bit0 | bit6 | bit7 must all
	 * be clear for a "normal" header). On failure: soft-assert (FileChunk.cpp
	 * 0x27=39), then unconditionally uses the SAME hdr data anyway (a real,
	 * disassembly-confirmed "log but continue" dead-code-landmine retry --
	 * nothing changes hdr between the assert and the field copy that follows).
	 */
	if ((hdr.flags & 0x41) != 0 || (hdr.flags & 0x80) != 0)
		ApiAssertFileChunkCpp(0x27);

	mType = hdr.type;
	mSubtype = hdr.subtype;
	mId = hdr.id;
	mFlags = hdr.flags;
	mDeclaredLen = hdr.length;
}

CChunkBase::~CChunkBase()
{
}

bool CChunkBase::Init()
{
	if (!mParent) {
		/* Real dead-code landmine: soft-assert (Chunk.h 0xbb=187) then retry
		 * using the SAME (still NULL) mParent -- no real caller hits this.
		 */
		ApiAssertChunkH(0xbb);
		return false;
	}
	mBasePos = (unsigned long)mParent->Tell();
	return !mParent->HasIoError();
}

bool CChunkBase::PostClose()
{
	mStatus = eClosed;
	return true;
}

bool CChunkBase::Close()
{
	if (mOpenChild)
		ApiAssertFileChunkCpp(0x6f);

	unsigned long pos = (unsigned long)mParent->Tell();
	if (pos < mBasePos) {
		/* Real dead-code landmine (FileChunk.cpp 0x72=114): nothing between
		 * the assert and the re-read can change mParent's position.
		 */
		ApiAssertFileChunkCpp(0x72);
		pos = (unsigned long)mParent->Tell();
	}

	if (mStatus == eWrite) {
		if (OnWriteLenAndFlags(mBasePos - 5, pos - mBasePos, mDeclaredLen, mFlags))
			mDeclaredLen = pos - mBasePos;
	} else if (mStatus != eClosed && mStatus != eError) {
		/* mStatus == eRead: skip any unread trailing payload so the next
		 * sibling can be read from the correct position.
		 */
		mParent->Seek((long)(mBasePos + mDeclaredLen), CStream::eSeekSet);
	}

	return !mParent->HasIoError();
}

void CChunkBase::OnChildDestroy()
{
	if (!mOpenChild)
		return;
	mOpenChild->mStatus = eError;
	if (!mOpenChild) {
		ApiAssertChunkH(0x109);
	} else {
		mOpenChild->PreClose();
	}
	if (!mOpenChild) {
		ApiAssertChunkH(0x10f);
	} else {
		mOpenChild->Close();
	}
	if (!mOpenChild) {
		ApiAssertChunkH(0x115);
	} else {
		mOpenChild->PostClose();
	}
	mOpenChild = 0;
}

bool CChunkBase::GetAllInfo()
{
	if (mOpenChild)
		ApiAssertChunkH(0x3a);
	return false;
}

/* Re-verified 2026-07-28 via a fresh, careful re-read of the raw disassembly
 * against the independently vtable-dumped byte-offset table (see this file's
 * own commit) after an earlier draft mis-attributed several of the indirect
 * calls below. Confirmed call targets, by real vtable byte offset:
 *   +0x40 sub->GetFather() (must be NULL going in, FileChunk.cpp 0x16f=367)
 *   +0x44 sub->SetFather(this)
 *   +0x2c this->GetAbsSonNumber() -> fed into sub->SetRankNumber()
 *   +0x48 sub->SetRankNumber(absNum)
 *   +0x0c this->GetRelSonNestLev() -> stored into sub->mRelSonNestLev directly
 * The "must be a root/wildcard identity" check (FileChunk.cpp 0x16c=364) is on
 * THIS CONTAINER's OWN identity (`0x10/0x11/0x12(%ebx)`, not the sub's) --
 * only evaluated when the incoming sub itself carries a non-zero kind (`sub->
 * mFlags & 0x6 != 0`, i.e. sub is itself some container/root kind, not a
 * plain leaf): requires this->MatchesTypeOrWildcard(4, 0x10) OR
 * (this->mFlags & 0x6) in {4, 6} -- the two kind values this batch's own
 * AddSubChunk()/GetNextSubChunk() never construct (0=CChunk, 2=CChunkBlock),
 * strongly suggesting 4/6 belong to the deferred CChunkRootWithSeek(WithCRC)
 * classes' own root-of-tree identity.
 */
bool CChunkBase::LinkSubChunk(CChunk *&sub)
{
	/* NOTE on polarity: this bit-0x8 check requires the CONTAINER's OWN flag
	 * to be CLEAR to proceed (SET -> error) -- the OPPOSITE polarity from
	 * every Get()/Put()/Read()/Write()/Skip()/operator>>/operator<< check on
	 * this same bit position (which require it SET to proceed). Confirmed by
	 * direct re-read of the real disassembly (`test $0x8,%dl; je <continue>`
	 * here vs `testb $0x8,...; je <error>` there) -- not a transcription
	 * error. Real semantic reconciliation not established: transcribed
	 * exactly as observed rather than "corrected" to match the other
	 * methods' polarity.
	 */
	if ((mFlags & 0x8) != 0) {
		mStatus = eError;
		return false;
	}
	if (mStatus == eError)
		return false;
	if (mStatus == eClosed)
		return false;

	if (!sub) {
		ApiAssertChunkH(0x11b);
		return false;
	}

	if ((sub->mFlags & 0x6) != 0) {
		bool wildcard = MatchesTypeOrWildcard(4, 0x10);
		if (!wildcard && (mFlags & 0x6) != 4 && (mFlags & 0x6) != 6)
			ApiAssertFileChunkCpp(0x16c);
	}

	if (sub->GetFather())
		ApiAssertFileChunkCpp(0x16f);

	mAbsSonNumber += 1;
	if (mOpenChild)
		ApiAssertFileChunkCpp(0x174);
	mOpenChild = sub;

	sub->SetFather(this);

	unsigned int absNum = GetAbsSonNumber();
	sub->SetRankNumber(absNum);

	sub->mRelSonNestLev = (unsigned char)GetRelSonNestLev();

	if (sub->mParent)
		ApiAssertFileChunkCpp(0x17c);

	if (!mParent)
		ApiAssertChunkH(0xc2);
	sub->mParent = mParent;

	if (sub->mStatus != eClosed)
		ApiAssertFileChunkCpp(0x17f);
	sub->mStatus = mStatus;

	return true;
}

/* Re-verified 2026-07-28 alongside LinkSubChunk() (see its own header
 * comment): an earlier draft incorrectly omitted the real, disassembly-
 * confirmed `sub->Init()` call this method makes AFTER a successful
 * WriteHeader() (vtable offset 0x30) -- without it, `sub->GetBasePos()`
 * (mBasePos) is never snapshotted and every later Read/Write/Get/Put on the
 * new sub-chunk clamps against a bogus 0 baseline. On Init() failure, `sub`
 * is deleted and cleared, matching the real cleanup path (WriteHeader
 * failure shares the exact same cleanup code in ground truth).
 */
bool CChunkBase::AddSubChunk(CChunk *&sub, SIdVRF id)
{
	SChkHeader hdr;
	hdr.type = id.type;
	hdr.subtype = id.subtype;
	hdr.id = id.id;
	hdr.flags = id.flags;
	hdr.length = 0;

	if ((id.flags & 0x6) == 2)
		sub = new CChunkBlock(hdr);
	else
		sub = new CChunk(hdr);

	bool ok = LinkSubChunk(sub);
	if (!ok) {
		delete sub;
		sub = 0;
		return false;
	}

	if (mStatus == eWrite) {
		bool wrote = sub->WriteHeader(hdr);
		if (!wrote) {
			delete sub;
			sub = 0;
			return false;
		}
		bool inited = sub->Init();
		if (!inited) {
			delete sub;
			sub = 0;
			return false;
		}
	}
	return true;
}

/* Re-verified 2026-07-28 (see AddSubChunk()'s own note): the real tail calls
 * `sub->Init()` (vtable offset 0x30) after a successful LinkSubChunk() --
 * NOT `sub->GetAllInfo()` as an earlier draft had it (that confusion likely
 * came from CChunkBlock::Init()'s OWN read-path, which DOES call GetAllInfo()-
 * adjacent logic, but at a different call site). GetNextSubChunk()'s own
 * return value IS `sub->Init()`'s result; on failure `sub` is deleted and
 * cleared via the SAME cleanup code the LinkSubChunk()-failure path uses.
 */
bool CChunkBase::GetNextSubChunk(CChunk *&sub)
{
	sub = 0;
	if (mStatus == eError)
		return false;
	if (mStatus != eRead) {
		ApiAssertFileChunkCpp(0xd3);
		return false;
	}

	unsigned long pos = (unsigned long)mParent->Tell();
	unsigned long remaining = (mBasePos + mDeclaredLen > pos) ? (mBasePos + mDeclaredLen - pos) : 0;
	if (remaining <= 7)
		return false;

	SChkHeader hdr;
	if (!ReadHeader(hdr))
		return false;

	if ((hdr.flags & 0x6) == 2)
		sub = new CChunkBlock(hdr);
	else
		sub = new CChunk(hdr);

	bool ok = LinkSubChunk(sub);
	if (!ok) {
		if (sub) {
			delete sub;
			sub = 0;
		}
		return false;
	}

	if (mStatus != eRead)
		ApiAssertFileChunkCpp(0x126);

	bool inited = sub->Init();
	if (!inited) {
		delete sub;
		sub = 0;
		return false;
	}
	return true;
}

bool CChunkBase::CloseSubChunk(CChunk *&sub)
{
	CChunkBase *s = sub;
	if (s != mOpenChild)
		ApiAssertFileChunkCpp(0x55);
	if (!mOpenChild)
		return false;

	bool preOk = mOpenChild->PreClose();
	if (!preOk)
		mOpenChild->mStatus = eError;

	bool closeOk = mOpenChild->Close();
	mOpenChild->PostClose();
	mOpenChild = 0;
	sub = 0;
	return preOk && closeOk;
}

int CChunkBase::SetStatus()
{
	if (!mParent) {
		ApiAssertChunkH(0xbb);
		return mStatus;
	}
	/* Real ground truth reads mParent's own status-ish field through the SAME
	 * this-adjusted secondary view used elsewhere. Modeled here via
	 * mParent's own Tell()-derived state is not applicable -- mParent is a
	 * plain CStream, which has no "status" concept of its own in this
	 * project's model. This method has no reconstructed caller in this batch
	 * (deferred territory: CBackupChunk/CChunkRootWithSeek); kept as a
	 * documented but unexercised passthrough returning mStatus unchanged.
	 */
	return mStatus;
}

bool CChunkBase::ReadHeader(SChkHeader &out)
{
	if (!mParent) {
		ApiAssertChunkH(0xbb);
		return false;
	}
	unsigned char buf[8];
	mParent->Read(buf, 8);
	out.type = buf[0];
	out.subtype = buf[1];
	out.id = buf[2];
	out.flags = buf[3];
	out.length = ((unsigned long)buf[4] << 24) | ((unsigned long)buf[5] << 16) |
	             ((unsigned long)buf[6] << 8) | (unsigned long)buf[7];

	if (mParent->GetLastOpLen() != 8 || mParent->HasIoError()) {
		mStatus = eError;
		return false;
	}
	if ((out.flags & 0x41) != 0 || (out.flags & 0x80) != 0)
		return false;
	return true;
}

bool CChunkBase::WriteHeader(const SChkHeader &hdr)
{
	if ((hdr.flags & 0x41) != 0 || (hdr.flags & 0x80) != 0) {
		ApiAssertChunkH(0xbb);
		mStatus = eError;
		return false;
	}
	if (!mParent) {
		ApiAssertChunkH(0xbb);
		return false;
	}
	/* Real body accumulates mLastOpLen across ALL 5 individual Write() calls
	 * (4x single-byte -- type/subtype/id/flags -- + 1x 4-byte length) and
	 * requires the TOTAL to be exactly 8 -- not just the last call's own
	 * count (a genuine bug in an earlier draft of this method: both the
	 * missing 4th single-byte `flags` write AND the "check only the last
	 * call" mistake were caught by this file's own round-trip KAT test).
	 */
	unsigned long total = 0;
	unsigned char b;
	b = hdr.type;
	mParent->Write(&b, 1);
	total += mParent->GetLastOpLen();
	b = hdr.subtype;
	mParent->Write(&b, 1);
	total += mParent->GetLastOpLen();
	b = hdr.id;
	mParent->Write(&b, 1);
	total += mParent->GetLastOpLen();
	b = hdr.flags;
	mParent->Write(&b, 1);
	total += mParent->GetLastOpLen();

	unsigned char lenBytes[4];
	lenBytes[0] = (unsigned char)(hdr.length >> 24);
	lenBytes[1] = (unsigned char)(hdr.length >> 16);
	lenBytes[2] = (unsigned char)(hdr.length >> 8);
	lenBytes[3] = (unsigned char)hdr.length;
	mParent->Write(lenBytes, 4);
	total += mParent->GetLastOpLen();

	if (total != 8) {
		mStatus = eError;
		return false;
	}
	return !mParent->HasIoError();
}

unsigned int CChunkBase::ReadBinary(void *buf, unsigned int n)
{
	/* Real gate: eWrite alone is rejected (soft-assert, FileChunk.cpp
	 * 0xe2=226); eRead/eClosed/eError all fall through to the same clamped
	 * read below (status is re-validated by the caller's own Get()/Read()
	 * wrapper before reaching here in every real call site).
	 */
	if (mStatus == eWrite)
		ApiAssertFileChunkCpp(0xe2);
	if (!mParent) {
		ApiAssertChunkH(0xbb);
		return 0;
	}

	unsigned long remaining = (mBasePos + mDeclaredLen) - (unsigned long)mParent->Tell();
	unsigned int want = (n <= remaining) ? n : (unsigned int)remaining;

	mParent->Read(buf, want);
	if (mParent->GetLastOpLen() == want && mParent->HasIoError())
		mStatus = eError;
	return want;
}

void CChunkBase::WriteBinary(const void *buf, unsigned int n)
{
	if (mStatus != eWrite) {
		ApiAssertChunkH(0xbb);
	}
	if (!mParent) {
		ApiAssertChunkH(0xbb);
		return;
	}
	mParent->Write(buf, n);
	if (mParent->HasIoError() && mParent->GetLastOpLen() != n)
		mStatus = eError;
}

/* ---------------------------------------------------------------------- *
 * CChunkInfoItem
 * ---------------------------------------------------------------------- */

CChunkInfoItem::CChunkInfoItem(unsigned char pathDepth, unsigned char a,
                                unsigned char b, unsigned char c,
                                unsigned char d, const char *name)
	: mReserved1(0), mPathDepth(pathDepth), mA(a), mB(b), mC(c), mD(d),
	  mName(0), mData(0), mNext(0), mPathRemaining(pathDepth)
{
	mData = pathDepth ? new unsigned char[pathDepth] : 0;

	unsigned int nameLen;
	if (name) {
		nameLen = (unsigned int)strlen(name) + 1;
		mName = new char[nameLen];
		strncpy(mName, name, nameLen);
	} else {
		nameLen = 1;
		mName = new char[1];
		mName[0] = '\0';
	}

	mTotalLen = (unsigned char)(6 + pathDepth + nameLen);
}

CChunkInfoItem::CChunkInfoItem()
	: mReserved1(0), mPathDepth(0), mA(0), mB(0), mC(0), mD(0), mName(0),
	  mData(0), mNext(0), mPathRemaining(0)
{
	mName = new char[1];
	mName[0] = '\0';
	mTotalLen = 7;
}

CChunkInfoItem::~CChunkInfoItem()
{
	delete[] mData;
	delete[] mName;
}

bool CChunkInfoItem::SetRankNum(unsigned char r)
{
	if (mPathDepth == 0 || mPathRemaining == 0)
		return false;
	mPathRemaining -= 1;
	mData[mPathRemaining] = r;
	return true;
}

void CChunkInfoItem::Serialize(CChunk *out)
{
	if (!out) {
		ApiAssertChunkInfoCpp(0x5d);
		return;
	}
	out->Put(mTotalLen);
	out->Put(mReserved1);
	out->Put(mPathDepth);
	out->Write(mData, mPathDepth);
	out->Put(mA);
	out->Put(mB);
	out->Put(mC);
	out->Put(mD);

	unsigned int nameLen = (unsigned int)mTotalLen - (unsigned int)mPathDepth - 6;
	unsigned int realNameLen = (unsigned int)strlen(mName) + 1;
	if (nameLen != realNameLen)
		ApiAssertChunkInfoCpp(0x6a);
	out->Write(mName, nameLen);
}

void CChunkInfoItem::DeSerialize(CChunk *in)
{
	/* DeSerialize()'s own real disassembly was not independently traced to
	 * the same instruction-by-instruction depth as Serialize() (its exact
	 * mirror-image field order is the natural, low-risk assumption given
	 * every other pair in this project follows that convention) -- guard
	 * kept defensive rather than asserting a specific unconfirmed line.
	 */
	if (!in)
		return;
	unsigned char total, reserved1, pathDepth, a, b, c, d;
	in->Get(total);
	in->Get(reserved1);
	in->Get(pathDepth);

	delete[] mData;
	mData = pathDepth ? new unsigned char[pathDepth] : 0;
	if (pathDepth)
		in->Read(mData, pathDepth);

	in->Get(a);
	in->Get(b);
	in->Get(c);
	in->Get(d);

	unsigned int nameLen = (unsigned int)total - (unsigned int)pathDepth - 6;
	delete[] mName;
	mName = nameLen ? new char[nameLen] : new char[1];
	if (nameLen) {
		in->Read(mName, nameLen);
		mName[nameLen - 1] = '\0';
	} else {
		mName[0] = '\0';
	}

	mTotalLen = total;
	mReserved1 = reserved1;
	mPathDepth = pathDepth;
	mA = a;
	mB = b;
	mC = c;
	mD = d;
	mPathRemaining = pathDepth;
}

/* ---------------------------------------------------------------------- *
 * CChunkInfoList
 * ---------------------------------------------------------------------- */

CChunkInfoList::~CChunkInfoList()
{
	DestroyAllItem();
}

void CChunkInfoList::DestroyAllItem()
{
	CChunkInfoItem *cur = mHead;
	while (cur) {
		CChunkInfoItem *next = cur->GetNext();
		delete cur;
		cur = next;
	}
	mHead = 0;
}

void CChunkInfoList::Serialize(CChunk *out)
{
	for (CChunkInfoItem *cur = mHead; cur; cur = cur->GetNext())
		cur->Serialize(out);
}

void CChunkInfoList::DeSerialize(CChunk *in)
{
	if (mHead)
		ApiAssertChunkInfoCpp(0xad);

	/* Real body: repeatedly DeSerialize()s a fresh item and Add()s it while
	 * `in` still has meaningfully more bytes remaining in its own declared
	 * region (uses CChunkBase::GetRemainingBytes(), chunk_family.h's own
	 * small helper exposing the SAME position/length computation ReadHeader()/
	 * GetNextSubChunk() already do internally, rather than duplicating it a
	 * third time here).
	 */
	while (in->GetRemainingBytes() > 0) {
		CChunkInfoItem *item = new CChunkInfoItem();
		item->DeSerialize(in);
		Add(item);
	}
}

bool CChunkInfoList::Add(CChunkInfoItem *item)
{
	if (!mHead) {
		mHead = item;
		return true;
	}
	CChunkInfoItem *cur = mHead;
	for (;;) {
		if (cur->GetDedupKey() == item->GetDedupKey() &&
		    strcmp(cur->GetName(), item->GetName()) == 0)
			return false;
		if (!cur->GetNext())
			break;
		cur = cur->GetNext();
	}
	cur->SetNext(item);
	return true;
}

CChunkInfoItem *CChunkInfoList::GetNext(const CChunkInfoItem *cur) const
{
	if (!cur)
		return mHead;
	return cur->GetNext();
}

/* ---------------------------------------------------------------------- *
 * CChunk
 * ---------------------------------------------------------------------- */

CChunk::CChunk(const SChkHeader &hdr)
	: CChunkBase(hdr), mFather(0), mRankNumber(0)
{
	if ((mFlags & 0x6) != 0 && (mFlags & 0x6) != 2)
		ApiAssertFileChunkCpp(0x1b2);
}

CChunk::~CChunk()
{
	if (mFather)
		mFather->OnChildDestroy();
}

bool CChunk::Close()
{
	bool result = CChunkBase::Close();
	mFather = 0;
	return result;
}

bool CChunk::OnWriteLenAndFlags(unsigned long a, unsigned long b,
                                 unsigned long c, unsigned char flags)
{
	if (!mFather) {
		ApiAssertChunkH(0xb4);
		return false;
	}
	return mFather->OnWriteLenAndFlags(a, b, c, flags);
}

bool CChunk::PostClose()
{
	mStatus = eClosed;
	return true;
}

bool CChunk::OnSetInfo(CChunkInfoItem *item)
{
	bool skipRank = (mType == 4) && (mSubtype == 0 && mId == 0 && mFlags == 0x10);
	if (!skipRank)
		item->SetRankNum((unsigned char)mRankNumber);

	if (!mFather) {
		ApiAssertChunkH(0xec);
		return false;
	}
	return mFather->OnSetInfo(item);
}

unsigned int CChunk::Skip(unsigned int n)
{
	if ((mFlags & 0x8) == 0) {
		mStatus = eError;
		return 0;
	}
	if (mStatus == eError)
		return 0;
	if (mStatus != eRead) {
		ApiAssertFileChunkCpp(0x1e1);
		if (mStatus != eRead)
			return 0;
	}
	if (!mParent)
		return 0;

	unsigned long pos = (unsigned long)mParent->Tell();
	unsigned long remaining = (mBasePos + mDeclaredLen > pos) ? (mBasePos + mDeclaredLen - pos) : 0;
	unsigned int want = (n <= remaining) ? n : (unsigned int)remaining;

	mParent->Seek((long)want, CStream::eSeekCur);
	if (mParent->HasIoError())
		mStatus = eError;
	return want;
}

void CChunk::Read(void *buf, unsigned int n)
{
	if ((mFlags & 0x8) == 0)
		mStatus = eError;
	ReadBinary(buf, n);
}

void CChunk::Write(const void *buf, unsigned int n)
{
	if ((mFlags & 0x8) == 0)
		mStatus = eError;
	WriteBinary(buf, n);
}

bool CChunk::Get(unsigned char &b)
{
	if ((mFlags & 0x8) == 0) {
		mStatus = eError;
		return false;
	}
	if (mStatus == eError)
		return false;
	if (mStatus == eClosed)
		return false;
	if (mStatus != eRead)
		ApiAssertFileChunkCpp(0x20f);

	unsigned int got = ReadBinary(&b, 1);
	return got == 1;
}

bool CChunk::Put(unsigned char b)
{
	if ((mFlags & 0x8) == 0) {
		mStatus = eError;
		return false;
	}
	if (mStatus == eError)
		return false;
	if (mStatus == eClosed)
		return false;
	if (mStatus != eWrite)
		ApiAssertFileChunkCpp(0x23c);

	WriteBinary(&b, 1);
	return mStatus != eError;
}

void CChunk::SetInfo(unsigned char a, unsigned char b, unsigned char c,
                      unsigned char d, char *name)
{
	if (mStatus != eWrite)
		ApiAssertFileChunkCpp(0x256);

	CChunkInfoItem *item = new CChunkInfoItem(mRelSonNestLev, a, b, c, d, name);
	bool owned = OnSetInfo(item);
	if (!owned)
		delete item;
}

void CChunk::WriteBE16(unsigned short v)
{
	unsigned char b[2];
	b[0] = (unsigned char)(v >> 8);
	b[1] = (unsigned char)v;
	WriteBinary(b, 2);
}

unsigned short CChunk::ReadBE16()
{
	unsigned char b[2] = {0, 0};
	ReadBinary(b, 2);
	return (unsigned short)(((unsigned short)b[0] << 8) | b[1]);
}

void CChunk::WriteBE32(unsigned long v)
{
	unsigned char b[4];
	b[0] = (unsigned char)(v >> 24);
	b[1] = (unsigned char)(v >> 16);
	b[2] = (unsigned char)(v >> 8);
	b[3] = (unsigned char)v;
	WriteBinary(b, 4);
}

unsigned long CChunk::ReadBE32()
{
	unsigned char b[4] = {0, 0, 0, 0};
	ReadBinary(b, 4);
	return ((unsigned long)b[0] << 24) | ((unsigned long)b[1] << 16) |
	       ((unsigned long)b[2] << 8) | (unsigned long)b[3];
}

CChunk &CChunk::operator>>(unsigned char &v) { Get(v); return *this; }
CChunk &CChunk::operator>>(char &v) { unsigned char b = 0; Get(b); v = (char)b; return *this; }
CChunk &CChunk::operator>>(signed char &v) { unsigned char b = 0; Get(b); v = (signed char)b; return *this; }
CChunk &CChunk::operator>>(unsigned short &v) { v = ReadBE16(); return *this; }
CChunk &CChunk::operator>>(short &v) { v = (short)ReadBE16(); return *this; }
CChunk &CChunk::operator>>(unsigned int &v) { v = (unsigned int)ReadBE32(); return *this; }
CChunk &CChunk::operator>>(unsigned long &v) { v = ReadBE32(); return *this; }
CChunk &CChunk::operator>>(int &v) { v = (int)ReadBE32(); return *this; }
CChunk &CChunk::operator>>(long &v) { v = (long)ReadBE32(); return *this; }

CChunk &CChunk::operator<<(unsigned char v) { Put(v); return *this; }
CChunk &CChunk::operator<<(char v) { Put((unsigned char)v); return *this; }
CChunk &CChunk::operator<<(signed char v) { Put((unsigned char)v); return *this; }
CChunk &CChunk::operator<<(unsigned short v) { WriteBE16(v); return *this; }
CChunk &CChunk::operator<<(short v) { WriteBE16((unsigned short)v); return *this; }
CChunk &CChunk::operator<<(unsigned int v) { WriteBE32((unsigned long)v); return *this; }
CChunk &CChunk::operator<<(unsigned long v) { WriteBE32(v); return *this; }
CChunk &CChunk::operator<<(int v) { WriteBE32((unsigned long)v); return *this; }
CChunk &CChunk::operator<<(long v) { WriteBE32((unsigned long)v); return *this; }

/* CZ interop -- see this file's own header comment on operator>>(CZ&)'s reduced
 * confidence level.
 */
CChunk &CChunk::operator<<(const CZ &z)
{
	unsigned long len = z.RawFlagField();
	const char *p = (const char *)(uintptr_t)z.RawPtrField();
	for (unsigned long i = 0; i < len; ++i)
		Put((unsigned char)p[i]);
	return *this;
}

CChunk &CChunk::operator>>(CZ &z)
{
	unsigned long len = z.RawFlagField();
	char *p = (char *)(uintptr_t)z.RawPtrField();
	for (unsigned long i = 0; i < len; ++i) {
		unsigned char b = 0;
		Get(b);
		p[i] = (char)b;
	}
	return *this;
}

/* ---------------------------------------------------------------------- *
 * CChunkBlock
 * ---------------------------------------------------------------------- */

CChunkBlock::CChunkBlock(const SChkHeader &hdr)
	: CChunk(hdr), mChild(0)
{
	if ((mFlags & 0x6) != 2)
		ApiAssertFileChunkCpp(0x428);
	if ((mFlags & 0x8) != 0)
		ApiAssertFileChunkCpp(0x429);
}

CChunkBlock::~CChunkBlock()
{
}

/* .text+0x080ad510. Real body: (mStatus must be eRead, FileChunk.cpp 0x44f=
 * 1103 soft-assert otherwise) if mChild is set, DELEGATES via a real tail
 * call to `mChild->GetNextSubChunk(sub)` (mChild's own vtable slot 0x14,
 * genuine virtual dispatch -- if mChild is itself a CChunkBlock this chains
 * correctly) -- i.e. "fetch the next sibling UNDER my one child", not a
 * sibling of mChild itself. If mChild is unset, `sub=NULL; return false`.
 */
bool CChunkBlock::GetNextSubChunk(CChunk *&sub)
{
	if (mStatus != eRead) {
		ApiAssertFileChunkCpp(0x44f);
		sub = 0;
		return false;
	}
	if (!mChild) {
		sub = 0;
		return false;
	}
	return mChild->GetNextSubChunk(sub);
}

/* .text+0x080ad5a0. Real body: (mStatus must be eWrite, FileChunk.cpp
 * 0x45b=1115 soft-assert otherwise) if mChild is set, DELEGATES via a real
 * tail call to `mChild->AddSubChunk(sub, id)` (mChild's own vtable slot
 * 0x18, virtual). If mChild is unset, `sub=NULL; return false` -- AddSubChunk
 * can NEVER create mChild itself; only Init()'s own DIRECT (non-virtual)
 * `CChunkBase::AddSubChunk()` call, below, bootstraps it.
 */
bool CChunkBlock::AddSubChunk(CChunk *&sub, SIdVRF id)
{
	if (mStatus != eWrite)
		ApiAssertFileChunkCpp(0x45b);
	if (!mChild) {
		sub = 0;
		return false;
	}
	return mChild->AddSubChunk(sub, id);
}

/* .text+0x080ad630. Real body: does NOT close `sub` itself -- delegates via a
 * real tail call to `mChild->CloseSubChunk(sub)` (mChild's own vtable slot
 * 0x1c, virtual) if mChild is set, i.e. "ask my one child to close WHATEVER
 * grandchild it currently has open"; if mChild is unset, just clears `sub`
 * and returns false. Genuinely different from the base's own "close my
 * caller-identified child" meaning -- transcribed exactly as disassembled,
 * not reinterpreted to match the more obvious-looking name.
 */
bool CChunkBlock::CloseSubChunk(CChunk *&sub)
{
	if (!mChild) {
		sub = 0;
		return false;
	}
	return mChild->CloseSubChunk(sub);
}

bool CChunkBlock::PostClose()
{
	mStatus = eClosed;
	return true;
}

unsigned int CChunkBlock::GetRelSonNumber() const
{
	if (!mChild)
		ApiAssertFileChunkCpp(0xf8);
	return mChild ? mChild->GetRelSonNumber() : 0;
}

bool CChunkBlock::OnSetInfo(CChunkInfoItem *item)
{
	if ((mFlags & 0x6) != 2)
		ApiAssertFileChunkCpp(0x472);
	return mInfoList.Add(item);
}

/* .text+0x080b1390. Note: unlike the public AddSubChunk()/GetNextSubChunk()/
 * CloseSubChunk() overrides above (which delegate to mChild's own methods),
 * PreClose()/Init()/GetAllInfo() below all call `CChunkBase::` versions
 * DIRECTLY (confirmed: real disassembly targets `CChunkBase::AddSubChunk`/
 * `CChunkBase::GetNextSubChunk`/`CChunkBase::CloseSubChunk` literally, not a
 * virtual dispatch) -- they need the GENERIC tree-linking behavior operating
 * on `this`'s own mOpenChild bookkeeping (with `mChild` as the by-reference
 * storage), not the specialized "delegate to mChild" pass-through the public
 * overrides provide to external callers.
 */
bool CChunkBlock::PreClose()
{
	if (mStatus == eWrite) {
		if (mChild)
			CChunkBase::CloseSubChunk(mChild);

		SIdVRF infoId;
		infoId.type = 3;
		infoId.subtype = 0;
		infoId.id = 0;
		infoId.flags = 0x18;
		CChunkBase::AddSubChunk(mChild, infoId);
		mInfoList.Serialize(mChild);
		if (mChild)
			CChunkBase::CloseSubChunk(mChild);
		return true;
	}
	if (mStatus == eRead) {
		if (mChild)
			CChunkBase::CloseSubChunk(mChild);
		return true;
	}
	ApiAssertFileChunkCpp(0x4ae);
	return true;
}

bool CChunkBlock::Init()
{
	if (!CChunkBase::Init())
		return false;

	if (mStatus == eWrite) {
		SIdVRF anchorId;
		anchorId.type = 4;
		anchorId.subtype = 0;
		anchorId.id = 0;
		anchorId.flags = 0x10;
		return CChunkBase::AddSubChunk(mChild, anchorId);
	}
	if (mStatus == eRead) {
		if (!CChunkBase::GetNextSubChunk(mChild))
			return false;
		if (!mChild->MatchesTypeOrWildcard(4, 0x10)) {
			bool ok = CChunkBase::CloseSubChunk(mChild);
			if (!ok)
				ApiAssertFileChunkCpp(0x489);
		}
		return true;
	}
	return true;
}

bool CChunkBlock::GetAllInfo()
{
	if (mChild && !mChild->GetFather())
		return true;

	if (mChild)
		CChunkBase::CloseSubChunk(mChild);
	if (!CChunkBase::GetNextSubChunk(mChild))
		return true;

	if (mChild->MatchesTypeOrWildcard(3, 0x18)) {
		mInfoList.DeSerialize(mChild);
	} else {
		bool ok = CChunkBase::CloseSubChunk(mChild);
		if (!ok)
			ApiAssertFileChunkCpp(0x445);
	}
	return true;
}

/* ---------------------------------------------------------------------- *
 * CChunkOrphan
 * ---------------------------------------------------------------------- */

CChunkOrphan::CChunkOrphan(const SChkHeader &hdr, unsigned char *buf, int len)
	: CChunk(hdr)
{
	/* `len` unused: real ground truth's CMemory size argument comes from
	 * `hdr.length` (read via `mov 0x4(%edi),%edx` off the header pointer),
	 * not from this separate `len` parameter -- every real caller happens to
	 * pass matching values, but the ctor itself never reads `len`.
	 */
	(void)len;
	CMemory *mem = new CMemory(buf, (unsigned long)hdr.length, 1);
	mParent = mem;
	mem->Open(0, CStream::eRead);
}

CChunkOrphan::~CChunkOrphan()
{
	PreClose();
	Close();
	PostClose();
	delete mParent;
	mParent = 0;
}
