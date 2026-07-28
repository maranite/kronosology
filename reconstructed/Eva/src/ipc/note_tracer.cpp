/*
 * note_tracer.cpp  -  CNoteTracer. See note_tracer.h for the full derivation.
 */

#include "note_tracer.h"
#include "event.h"
#include <cstring>
#include <cstdlib>

extern void HAL_DisableInterrupts();
extern void HAL_EnableInterrupts();

/* ---- mNoteIndex[]/mNotes[] invariant helpers -------------------------------------- */

void CNoteTracer::ClearEntries()
{
	for (unsigned i = 0; i < mNotes.mSize; ++i)
		mNoteIndex[mNotes.mBegin[i].mNote] = 0xff;
}

void CNoteTracer::RefreshEntries()
{
	for (unsigned i = 0; i < mNotes.mSize; ++i)
		mNoteIndex[mNotes.mBegin[i].mNote] = (unsigned char)i;
}

void CNoteTracer::ResetPendingNotes()
{
	ClearEntries();
	mNotes.mSize = 0;
}

/* ---- construction / destruction --------------------------------------------------- */

CNoteTracer::CNoteTracer() : mChannel(0)
{
	memset(mNoteIndex, 0xff, sizeof(mNoteIndex));
	HAL_DisableInterrupts();
	mNotes.mBegin = (CBufferedNote *)malloc(0x80);
	HAL_EnableInterrupts();
	mNotes.mCapacity = mNotes.mBegin ? 0x20 : 0;
	mNotes.mSize = 0;
	/* real: soft Api+0x94 assert (code 0xa4) on malloc failure, omitted per this
	 * project's established convention -- see file header. */
}

CNoteTracer::CNoteTracer(unsigned char channel) : mChannel(channel)
{
	memset(mNoteIndex, 0xff, sizeof(mNoteIndex));
	HAL_DisableInterrupts();
	mNotes.mBegin = (CBufferedNote *)malloc(0x80);
	HAL_EnableInterrupts();
	mNotes.mCapacity = mNotes.mBegin ? 0x20 : 0;
	mNotes.mSize = 0;
}

CNoteTracer::CNoteTracer(const CNoteTracer &other) : mChannel(other.mChannel)
{
	HAL_DisableInterrupts();
	mNotes.mBegin = (CBufferedNote *)malloc(other.mNotes.mSize * sizeof(CBufferedNote));
	HAL_EnableInterrupts();
	mNotes.mCapacity = mNotes.mBegin ? other.mNotes.mSize : 0;
	mNotes.mSize = 0;
	if (mNotes.mBegin) {
		memcpy(mNotes.mBegin, other.mNotes.mBegin, other.mNotes.mSize * sizeof(CBufferedNote));
		mNotes.mSize = other.mNotes.mSize;
	}
	/* real: soft Api+0x94 assert (code 0xa4) on malloc failure, omitted. */
	memcpy(mNoteIndex, other.mNoteIndex, sizeof(mNoteIndex));
}

CNoteTracer::~CNoteTracer()
{
	HAL_DisableInterrupts();
	free(mNotes.mBegin);
	HAL_EnableInterrupts();
	mNotes.mBegin = 0;
	mNotes.mSize = 0;
	mNotes.mCapacity = 0;
}

CNoteTracer &CNoteTracer::operator=(const CNoteTracer &other)
{
	ClearEntries();
	mChannel = other.mChannel;

	if (other.mNotes.mSize > mNotes.mCapacity) {
		HAL_DisableInterrupts();
		CBufferedNote *p = (CBufferedNote *)realloc(mNotes.mBegin, other.mNotes.mSize * sizeof(CBufferedNote));
		HAL_EnableInterrupts();
		if (p) {
			mNotes.mBegin = p;
			mNotes.mCapacity = other.mNotes.mSize;
		}
		/* real: soft Api+0x94 assert (code 0xb7) on realloc failure, then falls
		 * through to the same best-effort copy into the stale buffer -- omitted,
		 * ground truth behavior preserved below. */
	}
	mNotes.mSize = other.mNotes.mSize;
	memcpy(mNotes.mBegin, other.mNotes.mBegin, other.mNotes.mSize * sizeof(CBufferedNote));

	RefreshEntries();
	return *this;
}

/* ---- RendundantInsertion hook ------------------------------------------------------ */

void CNoteTracer::RendundantInsertion(CBufferedNote &existing, CBufferedNote /*incoming*/)
{
	/* real: raw 32-bit `*(unsigned*)&existing += 1`; byte-identical for realistic
	 * mCount values, see file header. */
	existing.mCount++;
}

/* ---- insert / remove ---------------------------------------------------------------- */

void CNoteTracer::Insert(CBufferedNote note)
{
	unsigned char idx = mNoteIndex[note.mNote];
	if (idx != 0xff) {
		RendundantInsertion(mNotes.mBegin[idx], note); /* virtual dispatch */
		return;
	}

	if (mNotes.mSize == mNotes.mCapacity) {
		unsigned newCap = mNotes.mCapacity * 2;
		HAL_DisableInterrupts();
		CBufferedNote *p = (CBufferedNote *)realloc(mNotes.mBegin, newCap * sizeof(CBufferedNote));
		HAL_EnableInterrupts();
		if (p) {
			mNotes.mBegin = p;
			mNotes.mCapacity = newCap;
		}
		/* real: soft Api+0x94 assert (code 0xb7) on realloc failure, then falls
		 * through to a best-effort insert into the stale (over-capacity) buffer
		 * -- omitted, ground truth behavior preserved below. */
	}

	mNotes.mBegin[mNotes.mSize] = note;
	mNoteIndex[note.mNote] = (unsigned char)mNotes.mSize;
	mNotes.mSize++;
}

void CNoteTracer::Remove(unsigned char note)
{
	unsigned char idx = mNoteIndex[note];
	if (idx == 0xff)
		return;

	CBufferedNote &slot = mNotes.mBegin[idx];
	if (slot.mCount != 0) {
		/* a pending retrigger -- release one stacked instance, keep the slot. */
		slot.mCount--;
		return;
	}

	unsigned lastIdx = mNotes.mSize - 1;
	mNotes.mSize = lastIdx;
	mNoteIndex[note] = 0xff;
	if (idx < lastIdx) {
		CBufferedNote moved = mNotes.mBegin[lastIdx];
		mNotes.mBegin[idx] = moved;
		mNoteIndex[moved.mNote] = idx;
	}
}

/* ---- extremes ------------------------------------------------------------------------ */

signed char CNoteTracer::GetLeftMost() const
{
	unsigned char best = 0xff; /* sentinel: unsigned max, always replaced by a real note */
	for (unsigned i = 0; i < mNotes.mSize; ++i) {
		unsigned char v = mNotes.mBegin[i].mNote;
		if (v < best)
			best = v;
	}
	return (signed char)best;
}

signed char CNoteTracer::GetRightMost() const
{
	signed char best = -1; /* sentinel: signed min relative to real 0-127 note values */
	for (unsigned i = 0; i < mNotes.mSize; ++i) {
		signed char v = (signed char)mNotes.mBegin[i].mNote;
		if (v > best)
			best = v;
	}
	return best;
}

/* ---- event-list emission -------------------------------------------------------------- */

namespace {

/* Same tail-cursor idiom as controller_tracer.cpp's own AppendCCEvent -- grows the
 * chain FORWARD from an already-existing anchor node. */
void AppendNoteEvent(CLinkedEvent *&cursor, unsigned tagWord)
{
	CLinkedEvent *node = CLinkedEvent::sm_oEventsPool.GetNewEvent();
	cursor->SetNext(node);
	node->SetTag((int)tagWord);
	cursor = node;
}

} // namespace

int CNoteTracer::ListNotesOn(CLinkedEvent *&cursor) const
{
	unsigned base = 0x1u | ((unsigned)mChannel << 8);
	for (unsigned i = 0; i < mNotes.mSize; ++i) {
		const CBufferedNote &n = mNotes.mBegin[i];
		unsigned tag = base | ((unsigned)n.mNote << 16) | ((unsigned)n.mVelocity << 24);
		AppendNoteEvent(cursor, tag);
	}
	return (int)mNotes.mSize;
}

int CNoteTracer::ListNotesOn(CLinkedEvent *&cursor, signed char velocityDelta) const
{
	unsigned base = 0x1u | ((unsigned)mChannel << 8);
	for (unsigned i = 0; i < mNotes.mSize; ++i) {
		const CBufferedNote &n = mNotes.mBegin[i];
		int v = (int)n.mVelocity + (int)velocityDelta;
		if (v < 0 || v > 127)
			v = (velocityDelta < 0) ? 1 : 127;
		unsigned tag = base | ((unsigned)n.mNote << 16) | ((unsigned)(unsigned char)v << 24);
		AppendNoteEvent(cursor, tag);
	}
	return (int)mNotes.mSize;
}

int CNoteTracer::ListNotesOff(CLinkedEvent *&cursor) const
{
	unsigned base = 0x40000000u | ((unsigned)mChannel << 8);
	for (unsigned i = 0; i < mNotes.mSize; ++i) {
		const CBufferedNote &n = mNotes.mBegin[i];
		unsigned tag = base | ((unsigned)n.mNote << 16);
		AppendNoteEvent(cursor, tag);
	}
	return (int)mNotes.mSize;
}

int CNoteTracer::ListSoundsOn(CLinkedEvent *&cursor) const
{
	unsigned base = 0xdu | ((unsigned)mChannel << 8);
	for (unsigned i = 0; i < mNotes.mSize; ++i) {
		const CBufferedNote &n = mNotes.mBegin[i];
		unsigned tag = base | ((unsigned)n.mNote << 16) | ((unsigned)n.mVelocity << 24);
		AppendNoteEvent(cursor, tag);
	}
	return (int)mNotes.mSize;
}

int CNoteTracer::ListSoundsOff(CLinkedEvent *&cursor) const
{
	unsigned base = 0x4000000cu | ((unsigned)mChannel << 8);
	for (unsigned i = 0; i < mNotes.mSize; ++i) {
		const CBufferedNote &n = mNotes.mBegin[i];
		unsigned tag = base | ((unsigned)n.mNote << 16);
		AppendNoteEvent(cursor, tag);
	}
	return (int)mNotes.mSize;
}

/* ---- TDynBuffer<CBufferedNote> helpers ------------------------------------------------ */

bool CNoteTracer::CreateBuffer(TDynBuffer<CBufferedNote> &buf, unsigned count)
{
	HAL_DisableInterrupts();
	CBufferedNote *p = (CBufferedNote *)malloc(count * sizeof(CBufferedNote));
	HAL_EnableInterrupts();
	buf.mBegin = p;
	buf.mCapacity = count;
	buf.mSize = 0;
	if (!p) {
		buf.mCapacity = 0;
		/* real: soft Api+0x94 assert (code 0xa4), omitted. */
		return false;
	}
	return true;
}

bool CNoteTracer::ReallocBuffer(TDynBuffer<CBufferedNote> &buf, unsigned count)
{
	HAL_DisableInterrupts();
	CBufferedNote *p = (CBufferedNote *)realloc(buf.mBegin, count * sizeof(CBufferedNote));
	HAL_EnableInterrupts();
	if (!p) {
		/* real: soft Api+0x94 assert (code 0xb7), omitted; buf left unchanged,
		 * matching realloc()'s own failure semantics. */
		return false;
	}
	buf.mBegin = p;
	buf.mCapacity = count;
	return true;
}

void CNoteTracer::DestroyBuffer(TDynBuffer<CBufferedNote> &buf)
{
	HAL_DisableInterrupts();
	free(buf.mBegin);
	HAL_EnableInterrupts();
	buf.mBegin = 0;
	buf.mSize = 0;
	buf.mCapacity = 0;
}

void CNoteTracer::SwapBuffer(TDynBuffer<CBufferedNote> &other)
{
	if (!other.mBegin) {
		HAL_DisableInterrupts();
		other.mBegin = (CBufferedNote *)malloc(0x80);
		HAL_EnableInterrupts();
		other.mCapacity = other.mBegin ? 0x20 : 0;
		other.mSize = 0;
		/* real: soft Api+0x94 assert (code 0xa4) on malloc failure, omitted. */
	}

	ClearEntries();

	TDynBuffer<CBufferedNote> tmp = mNotes;
	mNotes = other;
	other = tmp;

	RefreshEntries();
}

/* ---- friend free function ------------------------------------------------------------- */

void Swap(CNoteTracer &a, CNoteTracer &b)
{
	a.ClearEntries();
	b.ClearEntries();

	TDynBuffer<CNoteTracer::CBufferedNote> tmp = a.mNotes;
	a.mNotes = b.mNotes;
	b.mNotes = tmp;

	a.RefreshEntries();
	b.RefreshEntries();
}
