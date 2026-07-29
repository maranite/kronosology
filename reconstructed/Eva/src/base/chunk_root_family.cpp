/*
 * chunk_root_family.cpp  -  see include/chunk_root_family.h.
 *
 * Real per-class file-name literals confirmed via `objdump -s -j .rodata`:
 *   CChunkRootBase's own out-of-line asserts   -> "Chunk.cpp"    (.rodata,
 *     confirmed from ~CChunkRootBase()/SetPath()'s own real literal reads)
 *   CChunkRootBase's inline-in-header asserts  -> "Chunk.h"      (same
 *     literal chunk_family.cpp already established)
 *   CChunkRootWithSeek's own asserts           -> "ChunkRootWithSeek.cpp"
 *     (.rodata+0x8e7fb10)
 *   CChunkRootWithSeekWithCRC's own asserts    -> "ChunkRootWithSeekWithCRC.cpp"
 *     (.rodata+0x8e7fb20)
 * Shared format string "Assertion failed in module %s, line %i.\n" -- same
 * project-wide string chunk_family.cpp already uses.
 */

#include "chunk_root_family.h"
#include "system_api.h"

#include <cstdlib>
#include <cstring>

extern CSystemApi *Api; /* mains.cpp */

namespace {

inline void ApiAssertImpl(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}

inline SChkHeader MakeHeader(unsigned char type, unsigned char subtype, unsigned char id,
                              unsigned char flags, unsigned long length)
{
	SChkHeader h;
	h.type = type;
	h.subtype = subtype;
	h.id = id;
	h.flags = flags;
	h.length = length;
	return h;
}

inline SIdVRF MakeId(unsigned char type, unsigned char subtype, unsigned char id,
                      unsigned char flags)
{
	SIdVRF v;
	v.type = type;
	v.subtype = subtype;
	v.id = id;
	v.flags = flags;
	return v;
}

} // namespace

void CChunkRootBase::ApiAssertChunkCpp(int line) { ApiAssertImpl("Chunk.cpp", line); }

static void ApiAssertChunkRootWithSeekCpp(int line)
{
	ApiAssertImpl("ChunkRootWithSeek.cpp", line);
}
static void ApiAssertChunkRootWithSeekWithCrcCpp(int line)
{
	ApiAssertImpl("ChunkRootWithSeekWithCRC.cpp", line);
}

/* ---------------------------------------------------------------------- *
 * CChunkRootBase
 * ---------------------------------------------------------------------- */

CChunkRootBase::CChunkRootBase(const SChkHeader &hdr) : CChunkBase(hdr), mPath(0)
{
}

CChunkRootBase::CChunkRootBase()
	: CChunkBase(MakeHeader(0, 0, 0, 0x14, 0)), mPath(0)
{
}

CChunkRootBase::~CChunkRootBase()
{
	ResetPath();
}

bool CChunkRootBase::PostClose()
{
	mStatus = eClosed;
	return true;
}

bool CChunkRootBase::Close()
{
	bool preOk = PreClose();
	unsigned long trueEndPos = mParent ? (unsigned long)mParent->Tell() : 0;
	bool baseOk = CChunkBase::Close();
	/* NOT ground truth -- CChunkBase::Close()'s own real body (mStatus==
	 * eWrite) self-dispatches OnWriteLenAndFlags() to patch THIS root's own
	 * header length field in place -- CChunkRootBase's own override of that
	 * method (above) seeks mParent BACKWARD to do so and leaves it there
	 * (at this root's own mBasePos), same "patch-back leaves position
	 * mid-stream" consequence documented in PreClose(). Restored here so
	 * PostClose()'s own Tell()-based "end of stream" capture (used to
	 * locate/patch the CRC value) is correct.
	 */
	if (mParent)
		mParent->Seek((long)trueEndPos, CStream::eSeekSet);
	bool postOk = PostClose();
	CloseStream();
	return postOk && baseOk && preOk;
}

bool CChunkRootBase::Init()
{
	if (!mParent) {
		ApiAssertChunkH(0xbb);
		return false;
	}
	mBasePos = (unsigned long)mParent->Tell();
	return !mParent->HasIoError();
}

bool CChunkRootBase::OnWriteLenAndFlags(unsigned long a, unsigned long b,
                                         unsigned long /*c*/, unsigned char flags)
{
	if (!mParent)
		return false;

	mParent->Seek((long)a, CStream::eSeekSet);
	mParent->Write(&flags, 1);

	unsigned char be[4];
	be[0] = (unsigned char)(b >> 24);
	be[1] = (unsigned char)(b >> 16);
	be[2] = (unsigned char)(b >> 8);
	be[3] = (unsigned char)b;
	mParent->Write(be, 4);

	return !mParent->HasIoError();
}

void CChunkRootBase::SetPath(const char *path)
{
	unsigned int n = path ? (unsigned int)strlen(path) : 0;
	char *dest = new char[n + 1];
	mPath = dest;
	if (!dest) {
		ApiAssertChunkCpp(0x4ff);
		dest = mPath;
	}
	if (n > 0)
		strncpy(dest, path, n);
	dest[n] = '\0';
}

void CChunkRootBase::ResetPath()
{
	if (mPath)
		delete[] mPath;
	mPath = 0;
}

void CChunkRootBase::CloseStream()
{
	if (mParent && (mParent->IsOpenForRead() || mParent->IsOpenForWrite()))
		mParent->Close();
}

bool CChunkRootBase::OpenStreamInWrite()
{
	mParent->Open(mPath, CStream::eWrite);

	if (mParent->IsOpenForRead() || mParent->IsOpenForWrite()) {
		int mode = mParent->GetAccessMode();
		bool proceed = true;
		if (mode == 1) {
			mStatus = eRead;
			/* Real ground truth soft-asserts here even on this "successful"
			 * path -- transcribed as-is, not reinterpreted.
			 */
			ApiAssertChunkCpp(0x50f);
		} else if (mode == 2 || mode == 3) {
			mStatus = eWrite;
		} else {
			mStatus = eError;
			ApiAssertChunkCpp(0x1a6);
			proceed = false; /* real ground truth: only takes this path if
			                     mStatus==eWrite, which it never is here */
		}

		if (proceed) {
			if ((mFlags & 6) != 6 && (mFlags & 6) != 4)
				ApiAssertChunkCpp(0x510);

			SChkHeader hdr = MakeHeader(mType, mSubtype, mId, mFlags, mDeclaredLen);
			if (CChunkBase::WriteHeader(hdr) && mParent->Tell() == 8) {
				/* See header comment: `Tell()==8` is this session's own
				 * best-effort reading of an ambiguous single-arg indirect
				 * call ground truth compares against 8 here.
				 */
				return Init();
			}
		}
	}

	mStatus = eClosed;
	ResetPath();
	return false;
}

bool CChunkRootBase::OpenStreamInRead()
{
	mParent->Open(mPath, CStream::eRead);

	if (mParent->IsOpenForRead() || mParent->IsOpenForWrite()) {
		int mode = mParent->GetAccessMode();
		if (mode == 1) {
			mStatus = eRead;
		} else {
			if (mode == 2 || mode == 3)
				mStatus = eWrite;
			else {
				mStatus = eError;
				ApiAssertChunkCpp(0x1a6);
			}
			/* Real ground truth: this assert fires for BOTH the eWrite and
			 * eError cases above (falls through into the same line) --
			 * transcribed as-is, not reinterpreted.
			 */
			ApiAssertChunkCpp(0x529);
		}

		/* Real ground truth passes `this`'s own header fields (mType..
		 * mDeclaredLen, contiguous per CChunkBase's own layout, matching
		 * SChkHeader byte-for-byte) DIRECTLY as ReadHeader()'s output --
		 * i.e. this root's own identity is overwritten in place by whatever
		 * is physically at the start of the stream BEFORE CheckHeader() is
		 * consulted. Mirrored here via a local temp + explicit copy so
		 * CheckHeader() still observes the freshly-read fields, same order.
		 */
		SChkHeader hdr;
		if (CChunkBase::ReadHeader(hdr)) {
			mType = hdr.type;
			mSubtype = hdr.subtype;
			mId = hdr.id;
			mFlags = hdr.flags;
			mDeclaredLen = hdr.length;

			if (CheckHeader() && mParent->Tell() == 8 && mDeclaredLen > 0xe) {
				/* See header comment: same `Tell()==8` reading as
				 * OpenStreamInWrite().
				 */
				return Init();
			}
		}
	}

	/* Real ground truth: Api+0x90 WARNING-level log (not the usual +0x94
	 * soft-assert slot), same "log-only" convention.
	 */
	typedef void (*WarnFn)(void *, const char *);
	void *vtbl = *(void **)Api;
	WarnFn warn = *(WarnFn *)((char *)vtbl + 0x90);
	warn(Api, "Warning : CChunkRootBase::OpenStreamInRead(.) fails : "
	          "ChunkRoot doesn't exist or corrupted!");

	mStatus = eClosed;
	ResetPath();
	return false;
}

/* ---------------------------------------------------------------------- *
 * CChunkRootWithSeek
 * ---------------------------------------------------------------------- */

const char CChunkRootWithSeek::sm_pkcBeginIndex[5] = "KCIX";
const char CChunkRootWithSeek::sm_pkcEndIndex[5] = "KCEX";

CChunkRootWithSeek::CChunkRootWithSeek(const SChkHeader &hdr)
	: CChunkRootBase(hdr), mFirstSubChunkPos(0), mIndexCapacity(kInitialCapacity),
	  mIndexCount(0), mIndexGrowBy(kInitialCapacity), mIndexData(0)
{
	mIndexData = (unsigned long *)malloc(kInitialCapacity * sizeof(unsigned long));
}

CChunkRootWithSeek::CChunkRootWithSeek()
	: CChunkRootBase(), mFirstSubChunkPos(0), mIndexCapacity(kInitialCapacity),
	  mIndexCount(0), mIndexGrowBy(kInitialCapacity), mIndexData(0)
{
	mIndexData = (unsigned long *)malloc(kInitialCapacity * sizeof(unsigned long));
}

CChunkRootWithSeek::~CChunkRootWithSeek()
{
	if (mIndexData)
		free(mIndexData);
}

void CChunkRootWithSeek::IndexAdd(unsigned long v)
{
	if (mIndexCount >= mIndexCapacity) {
		unsigned long newCap = mIndexCapacity + mIndexGrowBy;
		unsigned long *newData =
		    (unsigned long *)realloc(mIndexData, newCap * sizeof(unsigned long));
		if (newData) {
			mIndexData = newData;
			mIndexCapacity = newCap;
		}
	}
	if (mIndexCount < mIndexCapacity)
		mIndexData[mIndexCount++] = v;
}

void CChunkRootWithSeek::IndexClear()
{
	unsigned long *ptr = mIndexData;
	mIndexCapacity = 0;
	mIndexCount = 0;
	if (ptr) {
		free(ptr);
		mIndexData = 0;
	}
}

void CChunkRootWithSeek::CopySubChunkIndex(const unsigned long *src, unsigned long count)
{
	if (mIndexCount != 0)
		IndexClear();
	for (unsigned long i = 0; i < count; i++)
		IndexAdd(src[i]);
}

bool CChunkRootWithSeek::GetNextSubChunk(CChunk *&sub)
{
	bool ok = CChunkBase::GetNextSubChunk(sub);
	if (ok && HasIndex()) {
		if (sub->GetType() == 0xfe && sub->GetSubtype() == 0 &&
		    sub->GetId() == 0 && sub->GetHdrFlags() == 0x18) {
			ok = false;
			CloseSubChunk(sub);
		}
	}
	return ok;
}

bool CChunkRootWithSeek::AddSubChunk(CChunk *&sub, SIdVRF id)
{
	if (!mParent)
		ApiAssertChunkH(0xbb);

	unsigned long pos = mParent ? (unsigned long)mParent->Tell() : 0;
	bool ok = CChunkBase::AddSubChunk(sub, id);
	if (ok && !ExcludeFromIndex(id))
		IndexAdd(pos);
	return ok;
}

unsigned long CChunkRootWithSeek::ComputeIndexDataSize(unsigned long entryCount)
{
	return (unsigned long)(strlen(sm_pkcBeginIndex) + strlen(sm_pkcEndIndex)) +
	       4 + entryCount * 4;
}

unsigned long CChunkRootWithSeek::GetSizeWhenRewrite()
{
	unsigned long size = mDeclaredLen;
	if (!HasIndex()) {
		if (BuildSubChunkIndex())
			size = mDeclaredLen + ComputeIndexDataSize(mIndexCount);
	}
	return size;
}

bool CChunkRootWithSeek::SeekToSubChunk(unsigned int index)
{
	if (mStatus == eError)
		return false;
	if (mStatus != eRead)
		ApiAssertChunkRootWithSeekCpp(0x81);

	if (index == 0) {
		if (mFirstSubChunkPos == 0)
			ApiAssertChunkRootWithSeekCpp(0x85);
		mParent->Seek((long)mFirstSubChunkPos, CStream::eSeekSet);
	} else {
		if (!BuildSubChunkIndex())
			return false;
		if (index >= mIndexCount)
			return true; /* real ground truth: silently "succeeds" without
			                actually seeking for an out-of-range index */
		mParent->Seek((long)mIndexData[index], CStream::eSeekSet);
	}
	return !mParent->HasIoError();
}

bool CChunkRootWithSeek::PreClose()
{
	bool ok;

	if (mStatus == eWrite) {
		CChunk *idxChunk = 0;
		ok = CChunkBase::AddSubChunk(idxChunk, MakeId(0xfe, 0, 0, 0x18));
		if (ok) {
			if (!idxChunk)
				ApiAssertChunkRootWithSeekCpp(0x4d);
			idxChunk->Write(sm_pkcBeginIndex, 4);
			for (unsigned long i = 0; i < mIndexCount; i++)
				(*idxChunk) << mIndexData[i];
			idxChunk->Write(sm_pkcEndIndex, 4);
			(*idxChunk) << (unsigned long)mIndexCount;

			unsigned long lastOpLenBeforeClose = mParent->GetLastOpLen();
			unsigned long trueEndPos = (unsigned long)mParent->Tell();
			bool closeOk = CloseSubChunk(idxChunk);
			/* NOT ground truth -- CloseSubChunk()'s own patch-back chain
			 * (OnWriteLenAndFlags(), now real for the first time in this
			 * batch -- see chunk_family.h's own "genuinely cannot complete"
			 * note, written when no class implemented it for real yet)
			 * seeks mParent BACKWARD to patch the just-closed sub-chunk's
			 * own header in place, and leaves the stream positioned there
			 * (at that sub-chunk's own mBasePos) rather than at the true
			 * end of written data. Restored here so a FOLLOWING write (the
			 * CRC sub-chunk in CChunkRootWithSeekWithCRC::PreClose(), or any
			 * future sibling) continues from the correct position.
			 */
			mParent->Seek((long)trueEndPos, CStream::eSeekSet);
			if (!closeOk) {
				mStatus = eError;
				return false;
			}
			ok = (lastOpLenBeforeClose == 4);
		}
	} else {
		if (mStatus != eRead) {
			mStatus = eError;
			return false;
		}
		ok = true;
		if (HasIndex())
			ok = BuildSubChunkIndex();
	}

	if (!ok) {
		mStatus = eError;
		return false;
	}
	return CChunkRootBase::PreClose();
}

bool CChunkRootWithSeek::BuildSubChunkIndex()
{
	if (mStatus != eRead)
		return false;
	if (mIndexCount != 0)
		return true;

	/* DEFERRED: the real eRead-mode body re-reads the tail of this root's
	 * own region (via a CImageStr-backed scratch buffer) to parse a
	 * previously-written index sub-chunk back into mIndexData/mIndexCount/
	 * mFirstSubChunkPos. See header comment -- needs CImageStr, out of
	 * scope this batch. A conservative `false` ("no index available") is
	 * the correct, safe answer for every real caller in this batch.
	 */
	return false;
}

/* ---------------------------------------------------------------------- *
 * CChunkRootWithSeekWithCRC
 * ---------------------------------------------------------------------- */

CChunkRootWithSeekWithCRC::CChunkRootWithSeekWithCRC(const SChkHeader &hdr, int crcMode)
	: CChunkRootWithSeek(hdr), mCrcMode(crcMode)
{
	if (crcMode == 1)
		mFlags = (unsigned char)(mFlags | 6);
	else
		mFlags = (unsigned char)((mFlags & 0xf9) | 4);
}

CChunkRootWithSeekWithCRC::CChunkRootWithSeekWithCRC()
	: CChunkRootWithSeek(), mCrcMode(1)
{
}

bool CChunkRootWithSeekWithCRC::ExcludeFromIndex(SIdVRF id)
{
	if (CChunkRootWithSeek::ExcludeFromIndex(id))
		return true;
	return id.type == (unsigned char)0xf7 && id.subtype == 0 && id.id == 0 &&
	       id.flags == 0x18;
}

unsigned int CChunkRootWithSeekWithCRC::GetNumByteAfterIndex() const
{
	unsigned int base = CChunkRootWithSeek::GetNumByteAfterIndex();
	if (mStatus == eRead) {
		if ((mFlags & 6) != 6)
			return base;
	} else if (mStatus == eWrite) {
		if (mCrcMode != 1)
			return base;
	} else {
		return base;
	}
	return base + 0xc;
}

bool CChunkRootWithSeekWithCRC::PreClose()
{
	bool baseOk = CChunkRootWithSeek::PreClose();
	if (!baseOk || (mParent && mParent->HasIoError()))
		return false;

	/* Real ground truth: reaching this point (base PreClose + no I/O error)
	 * always yields "ok" as a side effect of the original condition chain,
	 * UNLESS the CRC sub-chunk write below is attempted and fails.
	 */
	bool ok = true;

	if (mStatus == eWrite && mCrcMode == 1) {
		CChunk *crcChunk = 0;
		bool added = AddSubChunk(crcChunk, MakeId(0xf7, 0, 0, 0x18));
		if (added) {
			if (!crcChunk)
				ApiAssertChunkRootWithSeekWithCrcCpp(0x49);
			(*crcChunk) << (unsigned long)0; /* placeholder, patched by PostClose() */
			unsigned long lastOpLenBeforeClose = mParent->GetLastOpLen();
			unsigned long trueEndPos = (unsigned long)mParent->Tell();
			bool closeOk = CloseSubChunk(crcChunk);
			/* NOT ground truth -- see the identical comment in
			 * CChunkRootWithSeek::PreClose(). Restores the stream to the
			 * true end of written data (PostClose()'s own `Tell()` capture
			 * depends on this) after the patch-back chain's own backward
			 * seek.
			 */
			mParent->Seek((long)trueEndPos, CStream::eSeekSet);
			ok = closeOk && (lastOpLenBeforeClose == 4);
		} else {
			ok = false;
		}
	}
	return ok;
}

bool CChunkRootWithSeekWithCRC::PostClose()
{
	bool crcOk = false;

	if (mParent && !mParent->HasIoError() && mStatus == eWrite && mCrcMode == 1) {
		unsigned long endPos = (unsigned long)mParent->Tell();
		unsigned long computedCrc = 0, storedCrc = 0;
		int hadCrc = 0;
		bool got = GetCRC(&computedCrc, &storedCrc, &hadCrc, 0);
		if (got && hadCrc == 1 && storedCrc == 0) {
			mParent->Seek((long)(endPos - 4), CStream::eSeekSet);
			if (!mParent->HasIoError()) {
				unsigned char be[4];
				be[0] = (unsigned char)(computedCrc >> 24);
				be[1] = (unsigned char)(computedCrc >> 16);
				be[2] = (unsigned char)(computedCrc >> 8);
				be[3] = (unsigned char)computedCrc;
				mParent->Write(be, 4);
				if (!mParent->HasIoError())
					crcOk = (mParent->GetLastOpLen() == 4);
			}
		}
	}

	bool baseOk = CChunkRootWithSeek::PostClose();
	return baseOk && crcOk;
}

bool CChunkRootWithSeekWithCRC::CheckCRC(const CChunkCallback *cb)
{
	if (mStatus != eRead || (mFlags & 6) != 6 || (mParent && mParent->HasIoError()))
		return false;

	unsigned long computed = 0, stored = 0;
	int hadCrc = 0;
	bool got = GetCRC(&computed, &stored, &hadCrc, cb);
	if (!got || hadCrc != 1)
		return false;
	return stored == computed;
}

bool CChunkRootWithSeekWithCRC::GetCRC(unsigned long *outCrc, unsigned long *outStoredCrc,
                                        int *outHadCrcSubChunk, const CChunkCallback *cb)
{
	*outStoredCrc = 0;
	*outCrc = 0;
	*outHadCrcSubChunk = 0;

	if (!mParent) {
		ApiAssertChunkH(0xbb);
		return false;
	}

	/* MYSTERY-A/MYSTERY-B: see header comment -- this session reads these
	 * as mParent->Tell()/mParent->GetLength() (a position and a total
	 * length, matched against how the values are used below), not
	 * independently confirmed against a named virtual slot.
	 */
	unsigned long startPos = (unsigned long)mParent->Tell();
	if (startPos < mBasePos)
		ApiAssertChunkRootWithSeekWithCrcCpp(0x94);
	unsigned long totalLen = (unsigned long)mParent->GetLength();

	if (mParent->HasIoError())
		return false;

	bool reopenedForCrc;
	if (mStatus == eWrite) {
		mParent->Close();
		mParent->Open(mPath, CStream::eRead);
		CChunkBase::SetStatus();
		if (mParent->IsOpenForRead() || mParent->IsOpenForWrite()) {
			reopenedForCrc = true;
		} else {
			return false;
		}
	} else {
		reopenedForCrc = false;
	}

	CCrc32 crc(0xedb88320UL, 0);
	mParent->Seek(0, CStream::eSeekSet);

	char cancelByte = 0;
	if (cb)
		cb->OnProgress((unsigned int)totalLen, 0, &cancelByte);
	bool cancelled = (cancelByte != 0);

	unsigned int scratchSize = 0x100000;
	unsigned char *scratch = 0;
	do {
		scratchSize >>= 2;
		scratch = (unsigned char *)malloc(scratchSize);
	} while (!scratch && scratchSize != 0);

	bool streamedOk = false;
	if (!mParent->HasIoError()) {
		unsigned long remaining = totalLen - 4;
		if (scratch && remaining >= 5 && !cancelled) {
			while (true) {
				mParent->Read(scratch, scratchSize);
				unsigned long got = mParent->GetLastOpLen();
				unsigned long toProcess, newRemaining;
				if (remaining < got) {
					if (got >= remaining + 4)
						ApiAssertChunkRootWithSeekWithCrcCpp(0xca);
					newRemaining = 0;
					toProcess = remaining;
				} else {
					newRemaining = remaining - got;
					toProcess = got;
				}
				crc.PutBuffer(scratch, toProcess);
				if (cb) {
					if (totalLen < newRemaining)
						ApiAssertChunkRootWithSeekWithCrcCpp(0xd2);
					char cb2 = 0;
					cb->OnProgress((unsigned int)totalLen,
					               (unsigned int)(totalLen - newRemaining), &cb2);
					cancelled = (cb2 != 0);
				}
				if (mParent->HasIoError() || cancelled)
					break;
				remaining = newRemaining;
				if (remaining == 0) {
					streamedOk = true;
					break;
				}
			}
		}
	}
	if (scratch)
		free(scratch);

	bool result;
	if (streamedOk) {
		mParent->Seek((long)(totalLen - GetNumByteAfterIndex()), CStream::eSeekSet);
		CChunk *sub = 0;
		bool got = GetNextSubChunk(sub);
		result = true;
		if (got && sub) {
			if (sub->GetType() == (unsigned char)0xf7 && sub->GetSubtype() == 0 &&
			    sub->GetId() == 0 && sub->GetHdrFlags() == 0x18) {
				*outHadCrcSubChunk = 1;
				unsigned char be[4];
				mParent->Read(be, 4);
				*outStoredCrc = ((unsigned long)be[0] << 24) | ((unsigned long)be[1] << 16) |
				                ((unsigned long)be[2] << 8) | (unsigned long)be[3];
				result = !mParent->HasIoError() && (mParent->GetLastOpLen() == 4);
			} else {
				*outHadCrcSubChunk = 0;
			}
			CloseSubChunk(sub);
		}
	} else {
		result = false;
	}

	if (reopenedForCrc) {
		mParent->Close();
		mParent->Open(mPath, CStream::eReadWrite);
		CChunkBase::SetStatus();
		if (!mParent->IsOpenForRead())
			result = mParent->IsOpenForWrite();
	}
	if (!result)
		return false;

	mParent->Seek((long)startPos, CStream::eSeekSet);
	if (mParent->HasIoError())
		return false;

	*outCrc = crc.GetCrc();
	return true;
}
