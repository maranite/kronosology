/*
 * param_tracer.cpp  -  CParamTracer. See param_tracer.h for the full derivation.
 */

#include "param_tracer.h"
#include <cstring>

const SBytePair kInvalidBytePair = { 0, 0 };

/* ---- construction ------------------------------------------------------------- */

CParamTracer::CParamTracer()
	: mChannel(0), mCtrlChangeType(eRPN), mCurAddr(kInvalidBytePair)
{
}

CParamTracer::CParamTracer(unsigned char channel, ECtrlChange ctrlChangeType)
	: mChannel(channel), mCtrlChangeType(ctrlChangeType), mCurAddr(kInvalidBytePair)
{
}

CParamTracer::CParamTracer(const CParamTracer &other)
	: mChannel(other.mChannel), mCtrlChangeType(other.mCtrlChangeType), mCurAddr(other.mCurAddr)
{
	/* mParams starts empty (TVector's own default ctor); Insert() at Begin()==0
	 * on an empty vector is a plain append of the whole source range. */
	SParam *pos = mParams.Begin();
	mParams.Insert(pos, other.mParams.Begin(), other.mParams.End());
}

CParamTracer &CParamTracer::operator=(const CParamTracer &other)
{
	if (this == &other)
		return *this;

	mChannel = other.mChannel;
	mCtrlChangeType = other.mCtrlChangeType;
	mCurAddr = other.mCurAddr;

	mParams.Clear();
	SParam *pos = mParams.Begin();
	mParams.Insert(pos, other.mParams.Begin(), other.mParams.End());

	return *this;
}

void CParamTracer::InitAfterDefaultCtor(unsigned char channel, ECtrlChange ctrlChangeType)
{
	mChannel = channel;
	mCtrlChangeType = ctrlChangeType;
}

void CParamTracer::Reset()
{
	mCurAddr = kInvalidBytePair;
	mParams.Clear(); /* real: Size() -> 0, capacity kept */
}

/* ---- sorted-array binary search helpers (shared by every method below) -------- */

namespace {

/* Lower-bound: first element with mAddr >= key, or `end` if none. Matches every
 * real binary-search body in this class (all hand-unrolled the same std::lower_bound
 * shape: compare mAddr.b0 first, mAddr.b1 to break ties). */
const CParamTracer::SParam *LowerBound(const CParamTracer::SParam *begin,
                                        const CParamTracer::SParam *end,
                                        const SBytePair &key)
{
	const CParamTracer::SParam *first = begin;
	unsigned count = (unsigned)(end - begin);
	while (count > 0) {
		unsigned half = count >> 1;
		const CParamTracer::SParam *mid = first + half;
		if (mid->mAddr.b0 < key.b0 ||
		    (mid->mAddr.b0 == key.b0 && mid->mAddr.b1 < key.b1)) {
			first = mid + 1;
			count -= half + 1;
		} else {
			count = half;
		}
	}
	return first;
}

const CParamTracer::SParam *FindExact(const CParamTracer::SParam *begin,
                                       const CParamTracer::SParam *end,
                                       const SBytePair &key)
{
	const CParamTracer::SParam *p = LowerBound(begin, end, key);
	if (p != end && p->mAddr.b0 == key.b0 && p->mAddr.b1 == key.b1)
		return p;
	return 0;
}

} // namespace

const CParamTracer::SParam *CParamTracer::First() const
{
	return (mParams.Begin() != mParams.End()) ? mParams.Begin() : 0;
}

const CParamTracer::SParam *CParamTracer::Next(const SParam *p) const
{
	/* Real: soft Api+0x94 assert if p==0 or p is already out of [mBegin,mEnd) --
	 * omitted per this project's convention, falls through to the same result
	 * either way. */
	const SParam *next = p + 1;
	return (next != mParams.End()) ? next : 0;
}

const CParamTracer::SParam *CParamTracer::Find(const SBytePair &addr) const
{
	return FindExact(mParams.Begin(), mParams.End(), addr);
}

const CParamTracer::SParam *CParamTracer::FindEqualOrNext(const SBytePair &addr) const
{
	const SParam *p = LowerBound(mParams.Begin(), mParams.End(), addr);
	return (p != mParams.End()) ? p : 0;
}

/* ---- erase ---------------------------------------------------------------------- */

void CParamTracer::Erase(const SBytePair &addr)
{
	SParam *p = (SParam *)FindExact(mParams.Begin(), mParams.End(), addr);
	if (p == 0)
		return;
	mParams.Erase(p);
}

void CParamTracer::Erase(const SBytePair *addrList)
{
	if (addrList == 0)
		return; /* real: soft Api+0x94 assert, then falls through to the same no-op */

	while (!(addrList->b0 == kInvalidBytePair.b0 && addrList->b1 == kInvalidBytePair.b1)) {
		Erase(*addrList);
		++addrList;
	}
}

/* ---- direct data modification ---------------------------------------------------*/

void CParamTracer::ModifyData(SBytePair addr, SBytePair data)
{
	SParam *p = (SParam *)FindExact(mParams.Begin(), mParams.End(), addr);
	if (p != 0)
		p->mData = data;
}

void CParamTracer::DataInc()
{
	if (mCurAddr.b0 >= 0x80 || mCurAddr.b1 >= 0x80)
		return;
	if (mCurAddr.b0 == 0x7f && mCurAddr.b1 == 0x7f)
		return;

	SParam *p = (SParam *)FindExact(mParams.Begin(), mParams.End(), mCurAddr);
	if (p == 0)
		return;

	if (p->mData.b1 <= 0x7e) {
		++p->mData.b1;
	} else if (p->mData.b0 <= 0x7e) {
		++p->mData.b0;
		p->mData.b1 = 0;
	}
	/* else: already at the 0x7f/0x7f maximum, no-op */
}

void CParamTracer::DataDec()
{
	if (mCurAddr.b0 >= 0x80 || mCurAddr.b1 >= 0x80)
		return;
	if (mCurAddr.b0 == 0x7f && mCurAddr.b1 == 0x7f)
		return;

	SParam *p = (SParam *)FindExact(mParams.Begin(), mParams.End(), mCurAddr);
	if (p == 0)
		return;

	if (p->mData.b1 != 0) {
		--p->mData.b1;
	} else if (p->mData.b0 != 0) {
		--p->mData.b0;
		p->mData.b1 = 0x7f;
	}
	/* else: already at 0/0, no-op */
}

/* ---- upsert ----------------------------------------------------------------------*/

void CParamTracer::SetData(SBytePair addr, SBytePair data)
{
	if (addr.b0 >= 0x80 || addr.b1 >= 0x80)
		return;
	if (addr.b0 == 0x7f && addr.b1 == 0x7f)
		return; /* matches DataInc/DataDec's own "already at max" guard */

	SParam *pos = (SParam *)LowerBound(mParams.Begin(), mParams.End(), addr);
	if (pos != mParams.End() && pos->mAddr.b0 == addr.b0 && pos->mAddr.b1 == addr.b1) {
		pos->mData = data;
		return;
	}

	SParam newEntry;
	newEntry.mAddr = addr;
	newEntry.mData = data;
	mParams.Insert(pos, &newEntry, &newEntry + 1);
}

void CParamTracer::SetDataLSB(unsigned char lsb)
{
	if (lsb >= 0x80)
		return;
	if (mCurAddr.b0 >= 0x80 || mCurAddr.b1 >= 0x80)
		return;
	if (mCurAddr.b0 == 0x7f)
		return; /* real: further real-only for b0!=0x7f; the b0==0x7f arm always no-ops here */

	SParam *pos = (SParam *)LowerBound(mParams.Begin(), mParams.End(), mCurAddr);
	if (pos != mParams.End() && pos->mAddr.b0 == mCurAddr.b0 && pos->mAddr.b1 == mCurAddr.b1) {
		pos->mData.b1 = lsb;
		return;
	}

	SParam newEntry;
	newEntry.mAddr = mCurAddr;
	newEntry.mData.b0 = 0xff;
	newEntry.mData.b1 = lsb;
	mParams.Insert(pos, &newEntry, &newEntry + 1);
}

void CParamTracer::SetDataMSB(unsigned char msb)
{
	if (msb >= 0x80)
		return;
	if (mCurAddr.b0 >= 0x80 || mCurAddr.b1 >= 0x80)
		return;
	if (mCurAddr.b0 == 0x7f)
		return;

	SParam *pos = (SParam *)LowerBound(mParams.Begin(), mParams.End(), mCurAddr);
	if (pos != mParams.End() && pos->mAddr.b0 == mCurAddr.b0 && pos->mAddr.b1 == mCurAddr.b1) {
		pos->mData.b0 = msb;
		return;
	}

	SParam newEntry;
	newEntry.mAddr = mCurAddr;
	newEntry.mData.b0 = msb;
	newEntry.mData.b1 = 0xff;
	mParams.Insert(pos, &newEntry, &newEntry + 1);
}

/* ---- MIDI CC message emission ---------------------------------------------------*/

namespace {

/* Pop a fresh node, stamp its tag, and push it onto the front of `listHead`. Real
 * ground truth: `node = CLinkedEvent::sm_oEventsPool.GetNewEvent(); node->mTag =
 * tagWord; node->mNext = listHead; listHead = node;` -- no mBuf allocation, these
 * events carry their payload packed directly into the tag word (see file header). */
void PushCCEvent(CLinkedEvent *&listHead, unsigned tagWord)
{
	CLinkedEvent *ev = CLinkedEvent::sm_oEventsPool.GetNewEvent();
	ev->SetTag((int)tagWord);
	ev->SetNext(listHead);
	listHead = ev;
}

} // namespace

int CParamTracer::AppendSingleParam(CLinkedEvent *&listHead, SBytePair &lastAddr, const SParam &param) const
{
	int appended = 0;
	unsigned char ccBase = (unsigned char)mCtrlChangeType;

	if (lastAddr.b0 != param.mAddr.b0) {
		unsigned tag = 0x3u | ((unsigned)mChannel << 8) | ((unsigned)(ccBase + 1) << 16)
		             | ((unsigned)(param.mAddr.b0 & 0x7f) << 24);
		PushCCEvent(listHead, tag);
		++appended;
	}
	if (lastAddr.b1 != param.mAddr.b1) {
		unsigned tag = 0x3u | ((unsigned)mChannel << 8) | ((unsigned)ccBase << 16)
		             | ((unsigned)(param.mAddr.b1 & 0x7f) << 24);
		PushCCEvent(listHead, tag);
		++appended;
	}
	if (param.mData.b0 != 0xff) {
		unsigned tag = 0x3u | ((unsigned)mChannel << 8) | (6u << 16)
		             | ((unsigned)(param.mData.b0 & 0x7f) << 24);
		PushCCEvent(listHead, tag);
		++appended;
	}
	if (param.mData.b1 != 0xff) {
		unsigned tag = 0x3u | ((unsigned)mChannel << 8) | (0x26u << 16)
		             | ((unsigned)(param.mData.b1 & 0x7f) << 24);
		PushCCEvent(listHead, tag);
		++appended;
	}

	lastAddr = param.mAddr;
	return appended;
}

int CParamTracer::AppendAllParams(CLinkedEvent *&listHead) const
{
	if (listHead == 0)
		return 0; /* see header note -- real, unambiguous, reason not fully isolated */

	int total = 0;
	SBytePair lastAddr = kInvalidBytePair;
	for (const SParam *p = mParams.Begin(); p != mParams.End(); ++p)
		total += AppendSingleParam(listHead, lastAddr, *p);
	return total;
}

int CParamTracer::AppendParams(CLinkedEvent *&listHead, const SBytePair *addrList) const
{
	if (addrList == 0)
		return 0;

	int total = 0;
	SBytePair lastAddr = kInvalidBytePair;
	while (!(addrList->b0 == kInvalidBytePair.b0 && addrList->b1 == kInvalidBytePair.b1)) {
		const SParam *p = FindExact(mParams.Begin(), mParams.End(), *addrList);
		if (p != 0)
			total += AppendSingleParam(listHead, lastAddr, *p);
		++addrList;
	}
	return total;
}

int CParamTracer::AppendParamsDontCareAddr(CLinkedEvent *&listHead, const SBytePair *addrList) const
{
	if (addrList == 0)
		return 0;

	int total = 0;
	SBytePair lastAddr = kInvalidBytePair;
	while (!(addrList->b0 == kInvalidBytePair.b0 && addrList->b1 == kInvalidBytePair.b1)) {
		const SParam *p = FindExact(mParams.Begin(), mParams.End(), *addrList);
		if (p != 0)
			total += AppendSingleParam(listHead, lastAddr, *p);
		++addrList;
	}
	return total;
}
