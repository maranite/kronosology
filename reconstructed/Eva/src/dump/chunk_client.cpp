/*
 * chunk_client.cpp  -  see include/chunk_client.h for full provenance/design notes.
 *
 * Real vtable content confirmed by a direct .rodata byte read of
 * PTR__CChunkClient_08e857c8 (26 slots), PTR__CDumpHeaderDescr_08e85a20 (5 slots)
 * and PTR__CDumpReqDescr_08e85a60 (5 slots) -- see chunk_client.h's own slot map.
 *
 * `HAL_DisableInterrupts()`/`HAL_EnableInterrupts()` brackets around every real
 * malloc/free call are dropped -- same established userspace no-op precedent as
 * chunk_man.cpp/out_link.cpp. Every "Assertion failed in module %s, line %i.\n"
 * (Api vtbl+0x94) soft-assert call is likewise not modeled (log-only, no
 * control-flow effect on any of these methods -- same treatment as
 * tempo.cpp/edit_server.cpp/config_manager.cpp's own Api+0x94 asserts); the real
 * data movement/state transitions each one guards are reproduced faithfully.
 */

#include "chunk_client.h"
#include "module.h"
#include "out_link.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <cstring>
#include <new>

extern void *ChkApi; /* mains.cpp */

/* Real vtables + opaque mIfcThunk placeholder -- defined at the bottom of this
 * file (see the "Real vtable definitions" section), forward-declared here so
 * the ctor/dtor/Save.../Load... methods above can reference them.
 */
extern void *PTR__CChunkClient_08e857c8[26];
extern int   EvaDataPlaceholder_08e85838;
extern void *PTR__TPtrArray_08e858b0[3];
extern void *PTR__TPtrArray_08e85898[3];
extern void *PTR__TPtrArray_08e85880[3];
extern void *PTR__TPtrArray_08e85868[3];

/* ===================================================================== *
 * CChkItem
 * ===================================================================== */

CChkItem::CChkItem() : mType(0xff), mLen(0), mData(0)
{
}

CChkItem::CChkItem(unsigned char type, unsigned char len, const unsigned char *data)
	: mType(type), mLen(len), mData(0)
{
	mData = new unsigned char[len];
	memcpy(mData, data, len);
}

CChkItem::~CChkItem()
{
	if (mData)
		delete[] mData;
}

int CChkItem::Serialize(unsigned char *out) const
{
	out[0] = mType;
	out[1] = mLen;
	memcpy(out + 2, mData, mLen);
	return (int)mLen + 2;
}

void CChkItem::DeSerialize(const unsigned char *in)
{
	mType = in[0];
	mLen = in[1];
	mData = new unsigned char[mLen];
	memcpy(mData, in + 2, mLen);
}

/* ===================================================================== *
 * CDumpReqDescr
 * ===================================================================== */

CDumpReqDescr::CDumpReqDescr() : mType(0), mResourceId(0), mMicroId(0xff), mLen(0), mData(0)
{
}

CDumpReqDescr::~CDumpReqDescr()
{
	if (mData)
		free(mData);
}

void CDumpReqDescr::Reset()
{
	mType = 0;
	mResourceId = 0;
	mMicroId = 0xff;
	mLen = 0;
	if (mData) {
		free(mData);
		mData = 0;
	}
}

unsigned CDumpReqDescr::Serialize(unsigned char *out, unsigned char maxLen) const
{
	(void)maxLen;
	out[0] = (unsigned char)mType;
	if (mType == 1) {
		out[1] = mMicroId;
	} else if (mType == 2) {
		out[1] = (unsigned char)mResourceId;
	} else {
		return 0;
	}
	out[2] = mLen;
	memcpy(out + 3, mData, mLen);
	return (unsigned)mLen + 3;
}

unsigned CDumpReqDescr::DeSerialize(const unsigned char *in, unsigned char len)
{
	(void)len;
	Reset(); /* real: dispatched through vtbl+0x10 -- a plain call here reaches
	          * the same (possibly derived) override, since CChunkClient never
	          * calls this on anything but a real CDumpReqDescr/CDumpHeaderDescr
	          * object. */
	unsigned type = in[0];
	mType = (int)type;
	if (type == 1) {
		mMicroId = in[1];
	} else if (type == 2) {
		mResourceId = in[1];
	} else {
		return 0;
	}
	mLen = in[2];
	mData = static_cast<unsigned char *>(malloc(mLen));
	memcpy(mData, in + 3, mLen);
	return (unsigned)mLen + 3;
}

void CDumpReqDescr::SetMicro(unsigned char id, unsigned char len, const unsigned char *data)
{
	mMicroId = id;
	mType = 1;
	mResourceId = 0;
	mLen = len;
	if (len != 0) {
		mData = static_cast<unsigned char *>(malloc(len));
		memcpy(mData, data, len);
	}
}

void CDumpReqDescr::SetSingle(int resource, unsigned char len, const unsigned char *data)
{
	mResourceId = resource;
	mType = 2;
	mMicroId = 0xff;
	mLen = len;
	if (len != 0) {
		mData = static_cast<unsigned char *>(malloc(len));
		memcpy(mData, data, len);
	}
}

CDumpReqDescr &CDumpReqDescr::operator=(const CDumpReqDescr &other)
{
	if (this != &other) {
		unsigned char *old = mData;
		mType = other.mType;
		mResourceId = other.mResourceId;
		mMicroId = other.mMicroId;
		mLen = other.mLen;
		if (old)
			free(old);
		mData = 0;
		if (mLen != 0) {
			mData = static_cast<unsigned char *>(malloc(mLen));
			memcpy(mData, other.mData, mLen);
		}
	}
	return *this;
}

/* ===================================================================== *
 * CDumpHeaderDescr
 * ===================================================================== */

CDumpHeaderDescr::CDumpHeaderDescr() : CDumpReqDescr(), mByteCount(0)
{
}

CDumpHeaderDescr::~CDumpHeaderDescr()
{
	/* CDumpReqDescr::~CDumpReqDescr() runs automatically after this body. */
}

void CDumpHeaderDescr::Reset()
{
	CDumpReqDescr::Reset();
	mByteCount = 0;
}

unsigned CDumpHeaderDescr::Serialize(unsigned char *out, unsigned char maxLen) const
{
	unsigned n = CDumpReqDescr::Serialize(out, (unsigned char)(maxLen - 4));
	if (n == 0)
		return 0;
	unsigned char *p = out + n;
	p[0] = (unsigned char)(mByteCount >> 24);
	p[1] = (unsigned char)(mByteCount >> 16);
	p[2] = (unsigned char)(mByteCount >> 8);
	p[3] = (unsigned char)mByteCount;
	return n + 4;
}

unsigned CDumpHeaderDescr::DeSerialize(const unsigned char *in, unsigned char len)
{
	unsigned n = CDumpReqDescr::DeSerialize(in, (unsigned char)(len - 4));
	if (n == 0)
		return 0;
	const unsigned char *p = in + n;
	mByteCount = ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
	             ((unsigned long)p[2] << 8) | (unsigned long)p[3];
	return n + 4;
}

void CDumpHeaderDescr::SetMicro(unsigned char id, unsigned char len, const unsigned char *data,
                                 unsigned long byteCount)
{
	CDumpReqDescr::SetMicro(id, len, data);
	mByteCount = byteCount;
}

void CDumpHeaderDescr::SetSingle(int resource, unsigned char len, const unsigned char *data,
                                  unsigned long byteCount)
{
	CDumpReqDescr::SetSingle(resource, len, data);
	mByteCount = byteCount;
}

CDumpHeaderDescr &CDumpHeaderDescr::operator=(const CDumpReqDescr &other)
{
	if (this != &other) {
		CDumpReqDescr::operator=(other);
		mByteCount = 0;
	}
	return *this;
}

CDumpHeaderDescr &CDumpHeaderDescr::operator=(const CDumpHeaderDescr &other)
{
	if (this != &other) {
		CDumpReqDescr::operator=(other);
		mByteCount = other.mByteCount;
	}
	return *this;
}

/* ===================================================================== *
 * CChunkClient -- local helpers
 * ===================================================================== */

namespace {

/* Real ground truth's +0x8c scratch block: a `TObjArray<unsigned char>`-shaped
 * value array (NOT a COmegaPtrArray -- no vtable slot at this shape), always
 * constructed with the SAME literal {growBy=5, count=0, capacity=5} header and a
 * 5-byte data blob (chunk_client.h's own file header). Meaning of the constant 5
 * not further decoded; malloc/free lifecycle reproduced faithfully.
 */
struct SScratch5 {
	int mGrowBy;
	int mCount;
	int mCapacity;
	unsigned char *mData;
};

void *NewScratch5()
{
	SScratch5 *s = static_cast<SScratch5 *>(malloc(sizeof(SScratch5)));
	s->mGrowBy = 5;
	s->mCount = 0;
	s->mCapacity = 5;
	s->mData = static_cast<unsigned char *>(malloc(5));
	return s;
}

void FreeScratch5(void *raw)
{
	if (!raw)
		return;
	SScratch5 *s = static_cast<SScratch5 *>(raw);
	if (s->mData)
		free(s->mData);
	free(s);
}

/* Lazily allocates the +0x88 "work list", vtable-swapped to whichever
 * `TPtrArray<T>` flavor matches the in-flight operation -- all 4 real flavors
 * are install-only placeholders (EvaVTableStub), same established convention as
 * every other TPtrArray<T> in this project (omega_vtables.cpp, chunk_man.cpp).
 */
COmegaPtrArray *NewPtrArray(void **vtbl)
{
	void *raw = malloc(sizeof(COmegaPtrArray));
	COmegaPtrArray *arr = new (raw) COmegaPtrArray(5, 5, 1);
	*reinterpret_cast<void **>(arr) = (void *)vtbl;
	return arr;
}

/* Real: calls the array's own vtbl+4 ("delete self + every live element")
 * slot -- our vtable flavors are all EvaVTableStub, so (matching the
 * established "vtable-slot dispatch modeled abstractly" precedent) this is a
 * real, faithfully-reproduced call shape that is a documented no-op in this
 * reconstruction (leaks `arr`, same imprecision chunk_man.cpp's own
 * TPtrArray<CRegistrationEntry> already carries).
 */
void DeleteSelfViaVSlot(void *arr)
{
	if (!arr)
		return;
	typedef void (*DeleteSelfFn)(void *);
	void **vt = *reinterpret_cast<void ***>(arr);
	reinterpret_cast<DeleteSelfFn>(vt[1])(arr);
}

/* LoadRes()/SaveRes()/MergeRes()'s shared "copy each element into a fresh
 * fixed-size record, appended to a new work-list array" loop. Ground truth's
 * own per-field copy (with an always-true assert-guarded field re-read, see
 * chunk_client.h) collapses exactly to a flat memcpy of `recordSize` bytes --
 * confirmed field-by-field for all 3 real record shapes (0x14/0x18/0x38 bytes).
 */
COmegaPtrArray *CopyElemArray(const COmegaPtrArray &src, unsigned recordSize, void **vtbl)
{
	COmegaPtrArray *dst = NewPtrArray(vtbl);
	unsigned n = src.Count();
	for (unsigned i = 0; i < n; ++i) {
		void *rec = malloc(recordSize);
		memcpy(rec, src.Get(i), recordSize);
		dst->Add(rec);
	}
	return dst;
}

void PutBE32(unsigned char *p, unsigned long v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

} /* namespace */

/* ===================================================================== *
 * CChunkClient
 * ===================================================================== */

CChunkClient::CChunkClient(const CModule &owner)
	: CTask(owner, "ChunkClient", 4, 2, 0x8003), mCommId(0xff), mOutLinkMono(0),
	  mByteCount(0), mChkItemArray(0), mScratch(0), mState(0), mPendingCount(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CChunkClient_08e857c8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) = &EvaDataPlaceholder_08e85838;

	void *raw = malloc(sizeof(COutLinkMono));
	COutLinkMono *link = new (raw) COutLinkMono(*this, "CmdToChunkMan", 1, 0x8003);
	mOutLinkMono = link;
	Add(link);
}

CChunkClient::~CChunkClient()
{
	/* Matches the real D1 (non-deleting) destructor body -- see file header
	 * for why the D0 ("free(this)") variant + its 2 real non-virtual
	 * this-8 thunks are not separately modeled.
	 */
	*reinterpret_cast<void **>(this) = (void *)PTR__CChunkClient_08e857c8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) = &EvaDataPlaceholder_08e85838;

	if (mChkItemArray)
		DeleteSelfViaVSlot(mChkItemArray);
	FreeScratch5(mScratch);
	/* mHeader (CDumpHeaderDescr) and the CTask base subobject are torn down
	 * automatically right after this body runs -- matches ground truth's own
	 * trailing CDumpHeaderDescr::~CDumpHeaderDescr()/CTask::~CTask() calls.
	 */
}

bool CChunkClient::Abort()
{
	if (IsAbortToBeExecuted() && mState != 0) {
		mState = 7;
		return mOutLinkMono->OutMono(5, 0) == 0;
	}
	return false;
}

bool CChunkClient::StoppedByUser()
{
	if (IsStoppedByUserToBeExecuted() && mState != 0) {
		mState = 8;
		return mOutLinkMono->OutMono(6, 0) == 0;
	}
	return false;
}

void CChunkClient::Reset()
{
	COmegaPtrArray *chkItems = mChkItemArray;
	mByteCount = 0;
	if (chkItems)
		DeleteSelfViaVSlot(chkItems);
	void *scratch = mScratch;
	mChkItemArray = 0;
	FreeScratch5(scratch);
	mScratch = 0;
	mHeader.Reset();
}

void CChunkClient::FailAndReset()
{
	int pending = mPendingCount - 1; /* real: asserted mPendingCount >= 1 first, omitted */
	int state = mState;
	mPendingCount = pending;
	if (pending == 0) {
		mState = 0;
		if (state == 8)
			OnStoppedByUser(mHeader);
		else if (state == 7)
			OnExternalAbort(mHeader);
		else if (state != 0)
			OnInternalAbort(mHeader);
		Reset();
	}
}

int CChunkClient::PrepareList(const CDumpReqDescr *req, COmegaPtrArray &out)
{
	out.RemoveAll(1); /* real: reads `out`'s own "own" flag -- always 1 at every
	                    * real call site here (see file header). */
	if (req->GetType() == 1) {
		void *raw = malloc(sizeof(CChkItem));
		CChkItem *item = new (raw) CChkItem(req->GetMicroId(), req->GetLen(), req->GetData());
		out.Add(item);
		return 1;
	}
	if (req->GetType() == 2) {
		/* real: tail-calls back through this object's own vtbl+0x14
		 * (OnPrepareSingle) -- direct call here, single concrete class.
		 */
		return OnPrepareSingle(req, out);
	}
	/* real: "CChunkClient::PrepareList : undefined descriptor type" trace-only,
	 * not modeled (no control-flow effect).
	 */
	return 0;
}

int CChunkClient::OnPrepareMicro(const CDumpReqDescr *req, COmegaPtrArray &out)
{
	void *raw = malloc(sizeof(CChkItem));
	CChkItem *item = new (raw) CChkItem(req->GetMicroId(), req->GetLen(), req->GetData());
	out.Add(item);
	return 1;
}

bool CChunkClient::SaveDump(const CDumpReqDescr &req)
{
	if (mState == 0 && IsSaveDumpToBeExecuted(req)) {
		mChkItemArray = NewPtrArray(PTR__TPtrArray_08e858b0);
		mScratch = NewScratch5();
		PrepareList(&req, *mChkItemArray);
		mPendingCount = mPendingCount + 1; /* real: asserted 0 beforehand, omitted */
		mHeader = req;
		mState = 2;

		unsigned long param = OnGetParamForSaveDump();
		unsigned char buf[13];
		buf[0] = mCommId;
		PutBE32(buf + 1, reinterpret_cast<unsigned long>(mChkItemArray));
		PutBE32(buf + 5, reinterpret_cast<unsigned long>(mScratch));
		PutBE32(buf + 9, param);
		if (mOutLinkMono->OutMono(4, buf, sizeof(buf)) == 0)
			return true;
	}
	OnInternalAbort(req);
	return false;
}

bool CChunkClient::LoadDump(const CDumpHeaderDescr &req)
{
	if (mState == 0 && IsLoadDumpToBeExecuted(req)) {
		mChkItemArray = NewPtrArray(PTR__TPtrArray_08e858b0);
		mScratch = NewScratch5();
		mByteCount = req.GetByteCount();
		PrepareList(&req, *mChkItemArray);
		mPendingCount = mPendingCount + 1;
		mHeader = req;
		mState = 5;

		unsigned long param = OnGetParamForLoadDump();
		unsigned char buf[17];
		buf[0] = mCommId;
		PutBE32(buf + 1, reinterpret_cast<unsigned long>(mChkItemArray));
		PutBE32(buf + 5, reinterpret_cast<unsigned long>(mScratch));
		PutBE32(buf + 9, mByteCount);
		PutBE32(buf + 13, param);
		if (mOutLinkMono->OutMono(3, buf, sizeof(buf)) == 0)
			return true;
	}
	OnInternalAbort(req);
	return false;
}

bool CChunkClient::SaveFile(const char *name, const CDumpReqDescr &req)
{
	if (mState == 0 && IsSaveFileToBeExecuted(name, req)) {
		size_t nameLen = strlen(name);
		mChkItemArray = NewPtrArray(PTR__TPtrArray_08e858b0);
		mScratch = NewScratch5();
		PrepareList(&req, *mChkItemArray);
		size_t size = nameLen + 0x13;
		if (size < 0x10000) {
			mPendingCount = mPendingCount + 1;
			mHeader = req;
			mState = 1;

			unsigned char *buf = static_cast<unsigned char *>(malloc(size));
			unsigned long param = OnGetParamForSaveFile();
			unsigned char *p = buf;
			*p++ = mCommId;
			memcpy(p, name, nameLen + 1);
			p += nameLen + 1;
			p[0] = 'K';
			p[1] = 'O';
			p[2] = 'R';
			p[3] = 'F';
			p[4] = 0;
			PutBE32(p + 5, reinterpret_cast<unsigned long>(mChkItemArray));
			PutBE32(p + 9, reinterpret_cast<unsigned long>(mScratch));
			PutBE32(p + 13, param);
			int rc = mOutLinkMono->OutMono(2, buf, (unsigned short)size);
			free(buf);
			if (rc == 0)
				return true;
		}
	}
	OnInternalAbort(req);
	return false;
}

bool CChunkClient::LoadFile(const char *name, const CDumpHeaderDescr &req)
{
	if (mState == 0 && IsLoadFileToBeExecuted(name, req)) {
		size_t nameLen = name ? strlen(name) : 0;
		mChkItemArray = NewPtrArray(PTR__TPtrArray_08e858b0);
		mScratch = NewScratch5();
		mByteCount = req.GetByteCount();
		PrepareList(&req, *mChkItemArray);
		size_t size = nameLen + 0x12;
		if (size < 0x10000) {
			mPendingCount = mPendingCount + 1;
			mHeader = req;
			mState = 4;

			unsigned char *buf = static_cast<unsigned char *>(malloc(size));
			unsigned long param = OnGetParamForLoadFile();
			unsigned char *p = buf;
			*p++ = mCommId;
			if (name) {
				memcpy(p, name, nameLen + 1);
				p += nameLen + 1;
			} else {
				*p++ = 0;
			}
			PutBE32(p, reinterpret_cast<unsigned long>(mChkItemArray));
			PutBE32(p + 4, reinterpret_cast<unsigned long>(mScratch));
			PutBE32(p + 8, mByteCount);
			PutBE32(p + 12, param);
			int rc = mOutLinkMono->OutMono(1, buf, (unsigned short)size);
			free(buf);
			if (rc == 0)
				return true;
		}
	}
	OnInternalAbort(req);
	return false;
}

bool CChunkClient::LoadRes(CResourceChunk *chunk, const COmegaPtrArray &elems, int extra)
{
	if (mState != 6 && mState != 0)
		return false;
	OnBegin();
	COmegaPtrArray *copy = CopyElemArray(elems, 0x14, PTR__TPtrArray_08e85898);
	mPendingCount = mPendingCount + 1; /* real: asserted >=0 beforehand, omitted */
	mState = 6;

	unsigned char buf[10];
	buf[0] = mCommId;
	PutBE32(buf + 1, reinterpret_cast<unsigned long>(chunk));
	PutBE32(buf + 5, reinterpret_cast<unsigned long>(copy));
	buf[9] = (unsigned char)extra;
	return mOutLinkMono->OutMono(7, buf, sizeof(buf)) == 0;
}

void CChunkClient::LoadResSync(CResourceChunk *chunk, const COmegaPtrArray &elems)
{
	OnBegin();
	typedef void (*ChkApiFn)(void *, unsigned char, CResourceChunk *, const COmegaPtrArray *);
	void **vtbl = *reinterpret_cast<void ***>(ChkApi);
	reinterpret_cast<ChkApiFn>(vtbl[0x40 / 4])(ChkApi, mCommId, chunk, &elems);
}

bool CChunkClient::SaveRes(const char *name, const COmegaPtrArray &elems)
{
	if (mState != 3 && mState != 0)
		return false;
	OnBegin();
	COmegaPtrArray *copy = CopyElemArray(elems, 0x18, PTR__TPtrArray_08e85880);
	mPendingCount = mPendingCount + 1;
	mState = 3;

	size_t nameLen = strlen(name);
	size_t size = nameLen + 0xb;
	unsigned char *buf = static_cast<unsigned char *>(malloc(size));
	unsigned char *p = buf;
	*p++ = mCommId;
	memcpy(p, name, nameLen + 1);
	p += nameLen + 1;
	p[0] = 'K';
	p[1] = 'O';
	p[2] = 'R';
	p[3] = 'F';
	p[4] = 0;
	PutBE32(p + 5, reinterpret_cast<unsigned long>(copy));
	int rc = mOutLinkMono->OutMono(8, buf, (unsigned short)size);
	free(buf);
	return rc == 0;
}

bool CChunkClient::MergeRes(CResourceChunk *a, CResourceChunk *b, const COmegaPtrArray *elems,
                             unsigned long flags)
{
	if (mState != 0) {
		/* real: "CChunkClient::MergeRes(.) fails : the client is busy",
		 * trace-only, not modeled. */
		return false;
	}
	OnBegin();
	COmegaPtrArray *copy = CopyElemArray(*elems, 0x38, PTR__TPtrArray_08e85868);
	mByteCount = flags;
	mState = 9;
	mPendingCount = mPendingCount + 1; /* real: asserted >=0 beforehand, omitted */

	unsigned char buf[13];
	buf[0] = mCommId;
	PutBE32(buf + 1, reinterpret_cast<unsigned long>(a));
	PutBE32(buf + 5, reinterpret_cast<unsigned long>(b));
	PutBE32(buf + 9, reinterpret_cast<unsigned long>(copy));
	return mOutLinkMono->OutMono(9, buf, sizeof(buf)) == 0;
}

int CChunkClient::Exec(CMessage &msg)
{
	const unsigned char *raw = reinterpret_cast<const unsigned char *>(&msg);
	unsigned short flagsAndCmd = *reinterpret_cast<const unsigned short *>(raw + 8);
	if (!(flagsAndCmd & 0x100))
		return -1;
	unsigned long payload = *reinterpret_cast<const unsigned long *>(raw + 0x10);

	switch (flagsAndCmd & 0xff) {
	case 0xe0:
		/* real: asserts mState in {4,5} first, omitted */
		OnAcceptedHd(mHeader);
		return 0;
	case 0xe1:
		/* real: asserts mState in {1,2} first, omitted */
		mByteCount = payload;
		mHeader.SetByteCount(payload); /* real: also stores at this+0xa4 ==
		                                 * &mHeader + CDumpHeaderDescr's own
		                                 * +0x14 mByteCount field */
		OnAcceptedRq(mHeader);
		return 0;
	case 0xe2: {
		COmegaPtrArray *list = reinterpret_cast<COmegaPtrArray *>(payload);
		OnSingleEnd(list);
		FailAndReset();
		DeleteSelfViaVSlot(list);
		return 0;
	}
	case 0xe3:
		if (mState != 0) {
			/* real: asserts mByteCount >= payload first, omitted */
			OnByteCount(payload);
		}
		return 0;
	case 0xe4:
		mCommId = (unsigned char)payload;
		return 0;
	case 0xe5: {
		mPendingCount = mPendingCount - 1; /* real: asserted >=1 first, omitted */
		if (mState != 3 && mState != 6 && mState != 9) {
			OnEnd(mScratch, mHeader);
			mState = 0;
			Reset();
			return 0;
		}
		COmegaPtrArray *list = reinterpret_cast<COmegaPtrArray *>(payload);
		OnSingleEnd(list);
		if (mPendingCount == 0) {
			mState = 0;
			OnEnd();
			if (mState == 0)
				mByteCount = 0;
		}
		DeleteSelfViaVSlot(list);
		return 0;
	}
	default:
		return -1;
	}
}

int CChunkClient::OpenSubChunk(CResourceChunk *chunk, unsigned char type)
{
	/* Tier B -- see file header. Real body compares a 5-byte name field at
	 * chunk+0x44 against a fixed magic (chunk_client.h), then walks
	 * CChunkRootWithSeek::SeekToSubChunk()/GetNextSubChunk() (chunk_family.h's
	 * own already-documented DEFERRED sibling) looking for a sub-chunk whose
	 * own +0x10 byte matches `type`. Not modeled -- CChunkRootWithSeek isn't
	 * reconstructed anywhere in this project yet.
	 */
	(void)chunk;
	(void)type;
	return 0;
}

bool CChunkClient::CloseSubChunk(CResourceChunk *chunk, CChunk *sub)
{
	/* Tier B -- see file header. Real body calls `chunk`'s own vtbl+0x1c
	 * (CloseSubChunk-shaped) then CChunkRootWithSeek::SeekToSubChunk(0,0) --
	 * same DEFERRED-sibling dependency as OpenSubChunk() above.
	 */
	(void)chunk;
	(void)sub;
	return false;
}

/* ---- Virtual hook defaults (see chunk_client.h) ---- */

int CChunkClient::OnPrepareSingle(const CDumpReqDescr *req, COmegaPtrArray &list)
{
	(void)req;
	(void)list;
	return 0;
}

bool CChunkClient::IsLoadFileToBeExecuted(const char *name, const CDumpHeaderDescr &req) const
{
	(void)name;
	(void)req;
	return false;
}

bool CChunkClient::IsSaveFileToBeExecuted(const char *name, const CDumpReqDescr &req) const
{
	(void)name;
	(void)req;
	return false;
}

bool CChunkClient::IsLoadDumpToBeExecuted(const CDumpHeaderDescr &req) const
{
	(void)req;
	return false;
}

bool CChunkClient::IsSaveDumpToBeExecuted(const CDumpReqDescr &req) const
{
	(void)req;
	return false;
}

bool CChunkClient::IsAbortToBeExecuted() const
{
	return false;
}

bool CChunkClient::IsStoppedByUserToBeExecuted() const
{
	return false;
}

void CChunkClient::OnAcceptedHd(const CDumpHeaderDescr &req)
{
	(void)req;
}

void CChunkClient::OnAcceptedRq(const CDumpHeaderDescr &req)
{
	(void)req;
}

void CChunkClient::OnByteCount(unsigned long count)
{
	(void)count;
}

void CChunkClient::OnInternalAbort(const CDumpReqDescr &req)
{
	(void)req;
}

void CChunkClient::OnExternalAbort(const CDumpReqDescr &req)
{
	(void)req;
}

void CChunkClient::OnStoppedByUser(const CDumpReqDescr &req)
{
	(void)req;
}

void CChunkClient::OnEnd(const void *list, const CDumpReqDescr &req)
{
	/* real: tail-calls Api's own trace slot with "CChunkClient::OnEnd(.)" --
	 * logging only, not modeled (see file header). */
	(void)list;
	(void)req;
}

void CChunkClient::OnBegin()
{
	/* real: Api trace "CChunkClient::OnBegin()" -- logging only, not modeled. */
}

void CChunkClient::OnEnd()
{
	/* real: Api trace "CChunkClient::OnEnd()" -- logging only, not modeled. */
}

void CChunkClient::OnSingleEnd(COmegaPtrArray *list)
{
	(void)list;
}

unsigned long CChunkClient::OnGetParamForSaveFile() const
{
	/* real: asserts mState==1 first, omitted */
	return 0;
}

unsigned long CChunkClient::OnGetParamForSaveDump() const
{
	/* real: asserts mState==2 first, omitted */
	return 0;
}

unsigned long CChunkClient::OnGetParamForLoadFile() const
{
	/* real: asserts mState==4 first, omitted */
	return 0;
}

unsigned long CChunkClient::OnGetParamForLoadDump() const
{
	/* real: asserts mState==5 first, omitted */
	return 0;
}

/* ===================================================================== *
 * Real vtable definitions -- install-only, see chunk_client.h's slot map.
 * Slots 2 (0x08) and 4 (0x10) are real inherited-CTask virtuals this class
 * does NOT override (ground truth: 0x08180950/0x0807e170) -- out of scope,
 * same "inherited, not exercised by reconstructed code" treatment
 * PTR__CTask_08e82128 itself already gets (omega_vtables.cpp).
 * ===================================================================== */

extern "C" void CChunkClientDtorD1VSlot(void *obj)
{
	static_cast<CChunkClient *>(obj)->~CChunkClient();
}
extern "C" int CChunkClientExecVSlot(void *obj, CMessage *msg)
{
	return static_cast<CChunkClient *>(obj)->Exec(*msg);
}
extern "C" int CChunkClientOnPrepareSingleVSlot(void *obj, const CDumpReqDescr *req,
                                                 COmegaPtrArray *list)
{
	return static_cast<CChunkClient *>(obj)->OnPrepareSingle(req, *list);
}
extern "C" int CChunkClientIsLoadFileVSlot(void *obj, const char *name,
                                            const CDumpHeaderDescr *req)
{
	return static_cast<CChunkClient *>(obj)->IsLoadFileToBeExecuted(name, *req) ? 1 : 0;
}
extern "C" int CChunkClientIsSaveFileVSlot(void *obj, const char *name, const CDumpReqDescr *req)
{
	return static_cast<CChunkClient *>(obj)->IsSaveFileToBeExecuted(name, *req) ? 1 : 0;
}
extern "C" int CChunkClientIsLoadDumpVSlot(void *obj, const CDumpHeaderDescr *req)
{
	return static_cast<CChunkClient *>(obj)->IsLoadDumpToBeExecuted(*req) ? 1 : 0;
}
extern "C" int CChunkClientIsSaveDumpVSlot(void *obj, const CDumpReqDescr *req)
{
	return static_cast<CChunkClient *>(obj)->IsSaveDumpToBeExecuted(*req) ? 1 : 0;
}
extern "C" int CChunkClientIsAbortVSlot(void *obj)
{
	return static_cast<CChunkClient *>(obj)->IsAbortToBeExecuted() ? 1 : 0;
}
extern "C" int CChunkClientIsStoppedByUserVSlot(void *obj)
{
	return static_cast<CChunkClient *>(obj)->IsStoppedByUserToBeExecuted() ? 1 : 0;
}
extern "C" void CChunkClientOnAcceptedHdVSlot(void *obj, const CDumpHeaderDescr *req)
{
	static_cast<CChunkClient *>(obj)->OnAcceptedHd(*req);
}
extern "C" void CChunkClientOnAcceptedRqVSlot(void *obj, const CDumpHeaderDescr *req)
{
	static_cast<CChunkClient *>(obj)->OnAcceptedRq(*req);
}
extern "C" void CChunkClientOnByteCountVSlot(void *obj, unsigned long count)
{
	static_cast<CChunkClient *>(obj)->OnByteCount(count);
}
extern "C" void CChunkClientOnInternalAbortVSlot(void *obj, const CDumpReqDescr *req)
{
	static_cast<CChunkClient *>(obj)->OnInternalAbort(*req);
}
extern "C" void CChunkClientOnExternalAbortVSlot(void *obj, const CDumpReqDescr *req)
{
	static_cast<CChunkClient *>(obj)->OnExternalAbort(*req);
}
extern "C" void CChunkClientOnStoppedByUserVSlot(void *obj, const CDumpReqDescr *req)
{
	static_cast<CChunkClient *>(obj)->OnStoppedByUser(*req);
}
extern "C" void CChunkClientOnEnd2VSlot(void *obj, const void *list, const CDumpReqDescr *req)
{
	static_cast<CChunkClient *>(obj)->OnEnd(list, *req);
}
extern "C" void CChunkClientOnBeginVSlot(void *obj)
{
	static_cast<CChunkClient *>(obj)->OnBegin();
}
extern "C" void CChunkClientOnEnd0VSlot(void *obj)
{
	static_cast<CChunkClient *>(obj)->OnEnd();
}
extern "C" void CChunkClientOnSingleEndVSlot(void *obj, COmegaPtrArray *list)
{
	static_cast<CChunkClient *>(obj)->OnSingleEnd(list);
}
extern "C" unsigned long CChunkClientOnGetParamForSaveFileVSlot(void *obj)
{
	return static_cast<CChunkClient *>(obj)->OnGetParamForSaveFile();
}
extern "C" unsigned long CChunkClientOnGetParamForSaveDumpVSlot(void *obj)
{
	return static_cast<CChunkClient *>(obj)->OnGetParamForSaveDump();
}
extern "C" unsigned long CChunkClientOnGetParamForLoadFileVSlot(void *obj)
{
	return static_cast<CChunkClient *>(obj)->OnGetParamForLoadFile();
}
extern "C" unsigned long CChunkClientOnGetParamForLoadDumpVSlot(void *obj)
{
	return static_cast<CChunkClient *>(obj)->OnGetParamForLoadDump();
}

void *PTR__CChunkClient_08e857c8[26] = {
	(void *)CChunkClientDtorD1VSlot,             /* 0x00 ~CChunkClient (D1) */
	(void *)EvaVTableStub,                        /* 0x04 ~CChunkClient (D0), not modeled */
	(void *)EvaVTableStub,                        /* 0x08 inherited CTask virtual, stub */
	(void *)CChunkClientExecVSlot,                 /* 0x0c Exec(CMessage&) */
	(void *)EvaVTableStub,                        /* 0x10 inherited CTask virtual, stub */
	(void *)CChunkClientOnPrepareSingleVSlot,      /* 0x14 */
	(void *)CChunkClientIsLoadFileVSlot,           /* 0x18 */
	(void *)CChunkClientIsSaveFileVSlot,           /* 0x1c */
	(void *)CChunkClientIsLoadDumpVSlot,           /* 0x20 */
	(void *)CChunkClientIsSaveDumpVSlot,           /* 0x24 */
	(void *)CChunkClientIsAbortVSlot,              /* 0x28 */
	(void *)CChunkClientIsStoppedByUserVSlot,      /* 0x2c */
	(void *)CChunkClientOnAcceptedHdVSlot,         /* 0x30 */
	(void *)CChunkClientOnAcceptedRqVSlot,         /* 0x34 */
	(void *)CChunkClientOnByteCountVSlot,          /* 0x38 */
	(void *)CChunkClientOnInternalAbortVSlot,      /* 0x3c */
	(void *)CChunkClientOnExternalAbortVSlot,      /* 0x40 */
	(void *)CChunkClientOnStoppedByUserVSlot,      /* 0x44 */
	(void *)CChunkClientOnEnd2VSlot,               /* 0x48 */
	(void *)CChunkClientOnBeginVSlot,              /* 0x4c */
	(void *)CChunkClientOnEnd0VSlot,               /* 0x50 */
	(void *)CChunkClientOnSingleEndVSlot,          /* 0x54 */
	(void *)CChunkClientOnGetParamForSaveFileVSlot, /* 0x58 */
	(void *)CChunkClientOnGetParamForSaveDumpVSlot, /* 0x5c */
	(void *)CChunkClientOnGetParamForLoadFileVSlot, /* 0x60 */
	(void *)CChunkClientOnGetParamForLoadDumpVSlot, /* 0x64 */
};

/* Opaque data placeholder CChunkClient's own ctor/dtor store the ADDRESS of at
 * +0x08 (mIfcThunk, CTask's own field) -- never dereferenced by any
 * reconstructed code, same treatment as EvaDataPlaceholder_08e82144
 * (omega_vtables.cpp).
 */
int EvaDataPlaceholder_08e85838;

/* The 4 real `TPtrArray<T>` flavors CChunkClient's own work-list array gets
 * vtable-swapped to (CChkItem for Save/LoadFile/Dump, CLoadResElem for LoadRes,
 * CSaveResElem for SaveRes, CMergeElem for MergeRes) -- all install-only, same
 * "vtable-slot dispatch modeled abstractly" convention as every other
 * TPtrArray<T> in this project.
 */
void *PTR__TPtrArray_08e858b0[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e85898[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e85880[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e85868[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
