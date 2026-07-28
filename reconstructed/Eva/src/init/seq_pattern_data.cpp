/*
 * seq_pattern_data.cpp  -  see include/seq_pattern_data.h.
 *
 * All arithmetic below is transcribed to match ground truth's real
 * byte-for-byte behavior, but loops are written as plain C (not literal
 * GCC-unrolled `movdqa`/byte-store transcriptions) -- matching this
 * project's established convention (cz_util.h's ctor/dtor, etc.) of
 * reproducing real behavior rather than compiler artifacts.
 */

#include "seq_pattern_data.h"

namespace {

/* Real shared idiom used by both CSeqPat::Initialize(const char*) and
 * CSeqPat::SetName(const char*) (.text+0x08e17cd0 / 0x08e18070): copies
 * `src` into `dst[0..len)`, stopping at the first byte that is NOT in the
 * range 0x20..0x7f (confirmed via ground truth's signed `cmp cl,0x1f; jle`
 * test -- true for 0x00..0x1f AND for every byte with the high bit set,
 * i.e. only 0x20..0x7f keeps copying), then space-pads (0x20) the
 * remainder. Also used standalone by SetName() (no args, .text+0x08e183e0)
 * with an implicit all-space source.
 */
void CopyPaddedName(char *dst, const char *src, unsigned len)
{
	unsigned i = 0;
	if (src) {
		for (; i < len; i++) {
			signed char c = (signed char)src[i];
			if (c <= 0x1f)
				break;
			dst[i] = (char)c;
		}
	}
	for (; i < len; i++)
		dst[i] = ' ';
}

/* Real shared scan idiom (CPatternDataHolder::SetInfo()'s 2nd loop,
 * GetNumOfEvent(int), GetNumOfEventsToEnd()/GetTotalNumOfEvents()'s inner
 * per-pattern count): `event` is known non-null. Counts CSeqEvent slots
 * starting at `event` (inclusive) up to and including the first mType==3
 * (end-of-pattern sentinel) slot -- confirmed 1 if `event` itself is
 * already the sentinel.
 */
int CountSlotsThroughSentinel(const CSeqEvent *event)
{
	int n = 1;
	while (event->mType != 3) {
		event++;
		n++;
	}
	return n;
}

inline uint16_t ByteSwap16(const unsigned char *p)
{
	return (uint16_t)((p[0] << 8) | p[1]);
}

} /* namespace */

/* ------------------------------------------------------------------ */
/* CSeqPat                                                             */
/* ------------------------------------------------------------------ */

void CSeqPat::Initialize()
{
	/* Real body sets the whole 4-byte mEventOffset to 0xFFFFFFFF (see
	 * header comment) and the 4 fixed metadata bytes; does NOT touch the
	 * name field.
	 */
	mUnknown18 = 1;
	mUnknown19 = 0x13;
	mUnknown1a = 0;
	SetEventOffset(0xFFFFFFFFu);
}

void CSeqPat::Initialize(const char *name)
{
	CopyPaddedName(mName, name, sizeof mName);
	mUnknown18 = 1;
	mUnknown19 = 0x13;
	mUnknown1a = 0;
	SetEventOffset(0xFFFFFFFFu);
}

void CSeqPat::SetName(const char *name)
{
	CopyPaddedName(mName, name, sizeof mName);
}

void CSeqPat::SetName()
{
	CopyPaddedName(mName, 0, sizeof mName);
}

void CSeqPat::SetEventOffset(unsigned long v)
{
	mEventOffset[0] = (unsigned char)(v & 0xff);
	mEventOffset[1] = (unsigned char)((v >> 8) & 0xff);
	mEventOffset[2] = (unsigned char)((v >> 16) & 0xff);
	mEventOffset[3] = (unsigned char)((v >> 24) & 0xff);
}

unsigned long CSeqPat::GetEventOffset() const
{
	return (unsigned long)mEventOffset[0]
	     | ((unsigned long)mEventOffset[1] << 8)
	     | ((unsigned long)mEventOffset[2] << 16)
	     | ((unsigned long)mEventOffset[3] << 24);
}

void CSeqPat::SetEvent(unsigned long index, CSeqEvent *ptr)
{
	uintptr_t p = (uintptr_t)ptr;
	if (p != 0 && p >= index)
		SetEventOffset((unsigned long)(p - index));
	else
		SetEventOffset(0xFFFFFFFFu);
}

unsigned long CSeqPat::GetEvent(unsigned long index) const
{
	unsigned long packed = GetEventOffset();
	if (packed == 0xFFFFFFFFu)
		return 0;
	return index + packed;
}

CSeqEvent *CSeqPat::GetEvent(unsigned long eventAreaBase, int matchId) const
{
	unsigned long packed = GetEventOffset();
	if (packed == 0xFFFFFFFFu)
		return 0;

	uintptr_t sum = eventAreaBase + packed;
	if (sum == 0)
		return 0;

	CSeqEvent *p = (CSeqEvent *)sum;
	while (p->mType == 1) {
		unsigned b6 = ByteSwap16(p->mLinkB);
		if ((int)b6 == matchId)
			break;
		unsigned b4 = ByteSwap16(p->mLinkA);
		p = (CSeqEvent *)((char *)p + (size_t)b4 * 8);
		if (p == 0)
			return 0;
	}
	return p;
}

/* ------------------------------------------------------------------ */
/* CPatternDataHolder                                                  */
/* ------------------------------------------------------------------ */

CPatternDataHolder::CPatternDataHolder()
{
	/* Confirmed genuinely empty (bare `ret`) in the real binary -- fields
	 * are populated separately (e.g. CDrumTrackPatternDataHolder::
	 * Initialize()), not by this ctor.
	 */
}

void CPatternDataHolder::SetInfo()
{
	CSeqEvent *eventAreaBase = GetEventAreaTop();

	mUsedPatternCount = 0;
	for (int i = 0; i < mNumPatterns; i++) {
		CSeqPat *pat = GetPat(i);
		if (pat && pat->GetEvent((unsigned long)(uintptr_t)eventAreaBase))
			mUsedPatternCount++;
	}

	int total = 0;
	for (int i = 0; i < mNumPatterns; i++) {
		CSeqPat *pat = GetPat(i);
		if (!pat)
			continue;
		unsigned long start = pat->GetEvent((unsigned long)(uintptr_t)eventAreaBase);
		if (start)
			total += CountSlotsThroughSentinel((const CSeqEvent *)start);
	}
	mTotalEventCount = total;
}

void CPatternDataHolder::ClearUnusedArea()
{
	CSeqEvent *freeTop = GetFreeEventTop();
	CSeqEvent *areaEnd = GetEventAreaEnd();
	if ((char *)freeTop > (char *)areaEnd)
		return;
	/* Real length is (areaEnd - freeTop) + 1 bytes -- note this only
	 * zeroes the FIRST byte (the mType field) of the final slot at
	 * areaEnd, not the whole 8-byte slot, confirmed by ground truth's own
	 * length computation. See header comment.
	 */
	size_t len = (size_t)((char *)areaEnd - (char *)freeTop) + 1;
	memset(freeTop, 0, len);
}

CSeqPat *CPatternDataHolder::GetPat(int index) const
{
	if (mNumPatterns <= index)
		return 0;
	return (CSeqPat *)((char *)this + mPatternAreaOffset + (size_t)index * sizeof(CSeqPat));
}

unsigned long CPatternDataHolder::GetEvent(int index) const
{
	if (index >= mNumPatterns)
		return 0;
	CSeqPat *pat = (CSeqPat *)((char *)this + mPatternAreaOffset + (size_t)index * sizeof(CSeqPat));
	CSeqEvent *eventAreaBase = (CSeqEvent *)((char *)this + mEventAreaOffset);
	return pat->GetEvent((unsigned long)(uintptr_t)eventAreaBase);
}

CSeqEvent *CPatternDataHolder::GetEvent(int index, int matchId) const
{
	if (index >= mNumPatterns)
		return 0;
	CSeqPat *pat = (CSeqPat *)((char *)this + mPatternAreaOffset + (size_t)index * sizeof(CSeqPat));
	CSeqEvent *eventAreaBase = (CSeqEvent *)((char *)this + mEventAreaOffset);
	return pat->GetEvent((unsigned long)(uintptr_t)eventAreaBase, matchId);
}

CSeqEvent *CPatternDataHolder::GetEventDirect(int eventIndex) const
{
	return (CSeqEvent *)((char *)this + mEventAreaOffset) + eventIndex;
}

void CPatternDataHolder::SetEvent(int index, CSeqEvent *ev)
{
	if (index >= mNumPatterns)
		return;
	CSeqPat *pat = (CSeqPat *)((char *)this + mPatternAreaOffset + (size_t)index * sizeof(CSeqPat));
	CSeqEvent *eventAreaBase = (CSeqEvent *)((char *)this + mEventAreaOffset);
	pat->SetEvent((unsigned long)(uintptr_t)eventAreaBase, ev);
}

int CPatternDataHolder::GetNumOfEvent(CSeqEvent *start, bool countOnlyType1Or9) const
{
	if (!start)
		return 0;
	if (start->mType == 3)
		return 1;

	if (!countOnlyType1Or9)
		return CountSlotsThroughSentinel(start);

	/* Best-effort transcription: no caller in this reconstructed cluster
	 * exercises this mode (GetNumOfEvent(CSeqEvent*, bool) has zero
	 * in-cluster callers -- it's a public API for an out-of-scope
	 * caller), so this branch was traced but not exhaustively
	 * cross-checked instruction-by-instruction the way the rest of this
	 * file was. Real shape (confirmed): scan forward from `start`,
	 * counting only slots whose mType is 1 or 9, stopping at (not
	 * counting) the first mType==3 sentinel.
	 */
	int n = (start->mType == 1 || start->mType == 9) ? 1 : 0;
	const CSeqEvent *p = start + 1;
	for (;;) {
		if (p->mType == 3)
			return n;
		if (p->mType == 1 || p->mType == 9)
			n++;
		p++;
	}
}

int CPatternDataHolder::GetNumOfEvent(int patIndex) const
{
	if (patIndex >= mNumPatterns)
		return 0;
	CSeqPat *pat = GetPat(patIndex);
	if (!pat)
		return 0;
	unsigned long start = pat->GetEvent((unsigned long)(uintptr_t)GetEventAreaTop());
	if (!start)
		return 0;
	return CountSlotsThroughSentinel((const CSeqEvent *)start);
}

int CPatternDataHolder::GetNumOfEventsToEnd(int patIndex) const
{
	if (patIndex >= mNumPatterns)
		return 0;
	int total = 0;
	for (int i = patIndex; i < mNumPatterns; i++) {
		CSeqPat *pat = GetPat(i);
		if (!pat)
			continue;
		unsigned long start = pat->GetEvent((unsigned long)(uintptr_t)GetEventAreaTop());
		if (start)
			total += CountSlotsThroughSentinel((const CSeqEvent *)start);
	}
	return total;
}

int CPatternDataHolder::GetTotalNumOfEvents() const
{
	return GetNumOfEventsToEnd(0);
}

unsigned long CPatternDataHolder::GetNextTopEvent(int patIndex) const
{
	CSeqEvent *eventAreaBase = GetEventAreaTop();
	for (int i = patIndex + 1; i < mNumPatterns; i++) {
		CSeqPat *pat = GetPat(i);
		if (pat) {
			unsigned long r = pat->GetEvent((unsigned long)(uintptr_t)eventAreaBase);
			if (r != 0)
				return r;
		}
	}
	return 0;
}

CSeqEvent *CPatternDataHolder::GetEventAreaTop() const
{
	return (CSeqEvent *)((char *)this + mEventAreaOffset);
}

CSeqEvent *CPatternDataHolder::GetEventAreaEnd() const
{
	return (CSeqEvent *)((char *)this + mEventAreaOffset) + (mNumEventSlots - 1);
}

CSeqEvent *CPatternDataHolder::GetFreeEventTop() const
{
	return (CSeqEvent *)((char *)this + mEventAreaOffset) + mTotalEventCount;
}

CSeqPat *CPatternDataHolder::GetPatternTop() const
{
	return (CSeqPat *)((char *)this + mPatternAreaOffset);
}

CSeqEvent *CPatternDataHolder::GetPatternEventTop() const
{
	return (CSeqEvent *)((char *)this + mEventAreaOffset);
}

/* ------------------------------------------------------------------ */
/* CDrumTrackPatternDataHolder                                         */
/* ------------------------------------------------------------------ */

void CDrumTrackPatternDataHolder::Initialize()
{
	/* Real literal immediates, .text+0x08e18590. */
	mNumPatterns = 1000;         /* 0x3e8 */
	mNumEventSlots = 0x13880;    /* 80000 */
	mPatternAreaOffset = 0x18;
	mEventAreaOffset = 0x7d18;
}
