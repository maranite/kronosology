/*
 * tvector.h  -  TVector<T,N>, the small growable-array template used throughout this
 * binary (symbols.csv shows dozens of instantiations: TVector<CZ,1>, TVector<CLogicUnit,1>,
 * TVector<CLimiterBase*,1>, TVector<CTimerObject*,1>, TVector<CPool::SPool,1>, etc -- every
 * one seen so far uses N=1, EXCEPT `TVector<CParamTracer::SParam, 0>` (param_tracer.h,
 * CParamTracer family pass, 2026-07-28) -- N is never referenced anywhere in either the
 * ctor or the new Insert() body below regardless of its value, confirming it really is
 * an unused tag parameter, not a capacity/layout knob.
 *
 * Traced 2026-07-27 (Eva CPool/CSlotPool follow-up, see pool.h) against the richest real
 * instantiation available, TVector<CPool::SPool,1>:
 *   ~TVector() D1                       .text+0x08182c10, 26 bytes
 *   ~TVector() D0 (deleting)            .text+0x08182c60, 36 bytes
 *   MakeCapacity(unsigned)              .text+0x081859c0, ~530 bytes
 * cross-checked for general shape (not individually re-disassembled) against
 * TVector<CZ,1>'s own MakeCapacity/Append/Insert/Erase (.text+0x0817ba40/0x0817d350/
 * 0x0817d8c0/0x08180ae0) and TVector<CLogicUnit,1>'s/TVector<CLimiterBase*,1>'s own
 * MakeCapacity (.text+0x08180f70/0x081819a0) -- same doubling-growth reserve() shape in
 * every one.
 *
 * Real object layout (4 dwords, confirmed by TVector<CPool::SPool,1>'s own ctor-embedding
 * site in CPool::CPool(), pool.cpp):
 *   +0x00  mVtbl          real Itanium-ABI vtable ptr (TVector<T,N> genuinely declares a
 *                          virtual ~TVector(), unlike most manually-vtable-swapped classes
 *                          elsewhere in this project) -- but this reconstruction is a real
 *                          C++ template with a real (non-virtual, non-manually-dispatched)
 *                          destructor: every real ground-truth instance this project has
 *                          ever needed to model is embedded as a plain member (CPool::mChunks),
 *                          never heap-allocated/dispatched-through-base-pointer, so normal
 *                          C++ member-destruction order already produces the identical
 *                          observable free() ground truth's own manually-inlined
 *                          "reassert-vtable-then-free" dtor sequence does. mVtbl kept as a
 *                          real field (not modeled as `virtual`) purely for layout fidelity;
 *                          nothing in this project ever reads it back.
 *   +0x04  mBegin (T*)     malloc'd backing buffer start, NULL when empty
 *   +0x08  mEnd (T*)       one-past-the-last-USED element
 *   +0x0c  mEndCapacity (T*)  one-past-the-last-ALLOCATED element
 *
 * MakeCapacity(unsigned minCapacity): real ground truth is `reserve(minCapacity)` --
 * no-op if capacity already >= minCapacity; otherwise grows to the smallest capacity in
 * the doubling sequence 10, 20, 40, 80, ... that is >= minCapacity, mallocs a fresh
 * buffer that size, copies the currently-USED range across (raw dword-by-dword copies in
 * the real disassembly -- no placement-new/copy-ctor calls, valid because every T this
 * project has ever instantiated TVector over through this method, CPool::SPool, is POD),
 * frees the old buffer, and updates all 3 pointers. Real growth-size computation is
 * `newCapacity * sizeof(T)` bytes exactly (confirmed: TVector<CPool::SPool,1>'s own
 * MakeCapacity computes `eax*3 << 2` = `eax*12` = `eax*sizeof(SPool)`, sizeof(SPool)==12).
 *
 * PushBack(): real ground truth does NOT have a single shared "Append one element" method
 * for this specific instantiation -- CPool::CPool()'s own initial chunk-descriptor insert
 * and CPool::Alloc()'s exhausted-vector slow path each hand-inline an equivalent
 * "MakeCapacity(size+1) then write the new element at mEnd, then mEnd++" sequence
 * separately (ordinary GCC duplication of an inlined small helper, same as this project's
 * own established "one shared C++ method, license to collapse" convention -- see
 * omega_ptr_array.h's own header note for the reused-idiom precedent).
 *
 * Element-copy semantics generalize this specific instantiation's memcpy-based copy to
 * every T -- correct for CPool::SPool (the only instantiation this project currently
 * needs); NOT verified against the sibling TVector<CZ,1>/TVector<CLogicUnit,1>/etc
 * instantiations' own Append()/Insert()/Erase() bodies (those manage non-POD element
 * types -- CZ, CLogicUnit -- and are themselves unreconstructed, out of scope for this
 * pass; if a future pass needs one of THEM, MakeCapacity here should still be directly
 * reusable, but Append/Insert/Erase would need their own real bodies, not memcpy).
 */

#ifndef TVECTOR_H
#define TVECTOR_H

#include <cstdlib>
#include <cstring>

template <class T, int N>
class TVector {
public:
	TVector() : mVtbl(0), mBegin(0), mEnd(0), mEndCapacity(0) {}

	~TVector()
	{
		if (mBegin)
			free(mBegin);
	}

	unsigned Size() const { return (unsigned)(mEnd - mBegin); }
	unsigned Capacity() const { return (unsigned)(mEndCapacity - mBegin); }
	T *Begin() const { return mBegin; }
	T *End() const { return mEnd; }

	/* .text+0x081859c0 (TVector<CPool::SPool,1> specialization) -- see file header. */
	void MakeCapacity(unsigned minCapacity)
	{
		if (minCapacity <= Capacity())
			return;

		unsigned newCapacity = 10;
		while (newCapacity < minCapacity)
			newCapacity *= 2;

		T *newBuf = (T *)malloc((size_t)newCapacity * sizeof(T));
		unsigned usedCount = Size();
		if (usedCount)
			memcpy(newBuf, mBegin, (size_t)usedCount * sizeof(T));

		if (mBegin)
			free(mBegin);

		mBegin = newBuf;
		mEnd = newBuf + usedCount;
		mEndCapacity = newBuf + newCapacity;
	}

	/* See file header -- collapses 2 separately-inlined real call sites into one
	 * shared helper. */
	void PushBack(const T &item)
	{
		MakeCapacity(Size() + 1);
		*mEnd = item;
		++mEnd;
	}

	/* .text+0x08182f40 (TVector<CParamTracer::SParam,0> specialization, ~0xc80
	 * bytes -- CParamTracer family pass, 2026-07-28, param_tracer.h). Real ground
	 * truth is `insert(pos, first, last)`: grow if needed, shift [pos,mEnd) up by
	 * (last-first) elements to make room, copy [first,last) into the gap, advance
	 * mEnd. `pos` is taken (and updated) by reference because it is normalized to
	 * an index across any reallocation the growth triggers, same reason
	 * MakeCapacity() itself needs no such handling (it never moves an
	 * externally-held iterator) -- verified real callers (CParamTracer::SetData/
	 * SetDataLSB/SetDataMSB) always pass a single-element range (first+1==last)
	 * and either `pos==End()` (fast-path append) or a genuine mid-array binary-
	 * search insertion point; both are exercised by the real disassembly's own
	 * `if (*pos == mEnd) ... else ...` split, collapsed here into one memmove-based
	 * body since the two real branches are observably equivalent for POD T (same
	 * "generalize the memcpy-based copy" reasoning as MakeCapacity's own header
	 * note -- correct for CParamTracer::SParam, this project's only real
	 * instantiation so far). */
	void Insert(T *&pos, const T *first, const T *last)
	{
		unsigned insertOffset = (unsigned)(pos - mBegin);
		unsigned count = (unsigned)(last - first);
		if (count == 0) {
			pos = mBegin + insertOffset;
			return;
		}

		MakeCapacity(Size() + count);

		T *insertPos = mBegin + insertOffset;
		unsigned tailCount = (unsigned)(mEnd - insertPos);
		if (tailCount)
			memmove(insertPos + count, insertPos, (size_t)tailCount * sizeof(T));
		memcpy(insertPos, first, (size_t)count * sizeof(T));

		mEnd += count;
		pos = insertPos;
	}

	/* Generic helpers backing CParamTracer::Reset()/Erase() (param_tracer.h) --
	 * both real ground-truth operations (`mEnd = mBegin` to clear without
	 * releasing capacity; memmove-down-then-shrink to erase one element) are
	 * plain field manipulation with no per-instantiation logic of their own, so
	 * pulled up here as ordinary shared TVector operations rather than exposing
	 * mBegin/mEnd as public fields to every future caller. */
	void Clear() { mEnd = mBegin; }

	void Erase(T *pos)
	{
		memmove(pos, pos + 1, (size_t)(mEnd - (pos + 1)) * sizeof(T));
		--mEnd;
	}

private:
	void *mVtbl;
	T *mBegin;
	T *mEnd;
	T *mEndCapacity;
};

#endif /* TVECTOR_H */
