/*
 * param_tracer.h  -  CParamTracer, a sorted-array MIDI NRPN/RPN "parameter changed
 * since last send" tracker. 2026-07-28 class-inventory sweep: `nm -C` over the current
 * Eva static export (59656 symbols) surfaced a tight, previously-untouched cluster --
 * `CParamTracer`/`CControllerTracer`/`CCtrlAndParamTracer`/`CNoteTracer` (a related
 * family sharing the CEventsPool dependency below) plus `CNoteTracerTransposer` (a
 * sibling pulling in an unmodeled `CNoteTransposerOwner` interface -- excluded, see
 * below). Real-call-xref-traced before committing: CParamTracer's own call graph is
 * `CEventsPool::GetNewEvent`/`CEvBuffersPool::Free`/`CEvent::~CEvent` (both
 * reconstructed, ev_buffers_pool.h/event.h), `HAL_Disable/EnableInterrupts` (already a
 * real extern, prog_converter.cpp), `TVector<SParam,0>::Insert` (added to tvector.h
 * this pass), and plain libc (malloc/free/realloc/memcpy/operator new/delete) -- zero
 * touch on CZ/CStorage/the ES-family task god-objects/the virtual-driver subsystem/
 * Peg GUI, confirmed by extracting every `call` target inside CParamTracer's own
 * disassembled bytes (objdump -dr -M intel) and checking each callee by name.
 *
 * SCOPE NOTE: this pass reconstructs `CParamTracer` (19 real methods) + the
 * `CEventsPool` support class (events_pool.h, 3 methods) + `TVector<SParam,0>::Insert`
 * only. The sibling classes seen via the same nm sweep are deliberately deferred, each
 * for a documented reason, not a blanket "ran out of time":
 *   `CControllerTracer` -- simpler (fixed 128-byte mCtrl[] array, no TVector), single
 *     inheritance base of `CCtrlAndParamTracer`; genuinely self-contained, a clean
 *     follow-up.
 *   `CCtrlAndParamTracer` -- : public CControllerTracer (confirmed via typeinfo, a
 *     real __si_class_type_info at .rodata+0x08e8269c pointing at CControllerTracer's
 *     own typeinfo), COMPOSES two embedded CParamTracer-shaped subobjects (confirmed
 *     by its own dtor freeing two independent TVector-vtable-stamped buffers at
 *     `this`+0x8c and `this`+0xa8) -- needs CParamTracer's field layout (this pass)
 *     plus a bit more tracing of which subobject is which (NRPN vs RPN, unconfirmed).
 *   `CNoteTracer` -- unrelated data (own `CBufferedNote` dynamic array via
 *     SwapBuffer/CreateBuffer/DestroyBuffer/ReallocBuffer, not TVector-based), but DOES
 *     independently confirm the same CEventsPool dependency reconstructed here; a
 *     same-size follow-up on its own merits.
 *   `CNoteTracerTransposer` -- : public CNoteTracer, ctor takes a `CNoteTransposerOwner&`
 *     (an unmodeled callback-owner interface, real .rodata typeinfo string
 *     "20CNoteTransposerOwner" seen nearby but never traced) -- excluded from any
 *     follow-up estimate until that interface's real implementer(s) are identified as
 *     shallow or deep.
 *
 * CParamTracer OBJECT LAYOUT (0x1c bytes, confirmed field-by-field from both real
 * ctors @.text+0x0808fe10/0x08090000):
 *   +0x00  unsigned char mChannel        MIDI channel (ctor param; default ctor: 0)
 *   +0x04  ECtrlChange   mCtrlChangeType NRPN(98) or RPN(100) -- IS the wire CC base
 *                         (ctor param; default ctor: 100/RPN)
 *   +0x08  SBytePair     mCurAddr        "currently selected" NRPN/RPN address --
 *                         default-inits (both ctors) AND Reset() re-inits from the
 *                         same real global `_ZL16kInvalidBytePair` (.bss+0x0930a390,
 *                         confirmed value {0,0} -- .bss NOBITS, so a zero-initialized
 *                         "static const SBytePair") this file names kInvalidBytePair.
 *                         DataInc()/DataDec() operate on this field directly.
 *   +0x0c  void*         (TVector<SParam,0> mParams.mVtbl, layout-fidelity only)
 *   +0x10  SParam*       mParams.mBegin
 *   +0x14  SParam*       mParams.mEnd
 *   +0x18  SParam*       mParams.mEndCapacity
 * (no user-declared dtor exists in the real symbol table -- ~CParamTracer() is never
 * ODR-used by any real caller this pass traced, so GCC never emitted one; every real
 * caller only ever destroys a CParamTracer as a subobject inside something else, whose
 * own dtor frees the TVector buffer directly rather than calling through a CParamTracer
 * dtor -- see CCtrlAndParamTracer's own dtor, deferred above).
 *
 * SParam (4 bytes, sorted-array element, confirmed via Next()'s own `+0x4` stride and
 * Find()'s own `(end-begin)>>2` element count):
 *   +0x0  SBytePair mAddr  NRPN/RPN address (7-bit MSB, 7-bit LSB) -- the sort/search
 *                            key (binary search compares mAddr.b0 then mAddr.b1).
 *   +0x2  SBytePair mData  14-bit value (7-bit MSB, 7-bit LSB); either byte can be
 *                            0xff, meaning "not set" -- AppendSingleParam() skips
 *                            emitting a Data Entry MSB/LSB CC for a 0xff byte, and
 *                            SetDataLSB()/SetDataMSB() insert a fresh entry with the
 *                            OTHER data byte defaulted to 0xff when no existing entry
 *                            matches the current mCurAddr.
 *
 * REAL MIDI PROTOCOL (AppendSingleParam, confirmed from its own disassembly's exact CC
 * numbers/shift pattern): up to 4 CC messages per SParam, each packed directly into a
 * fresh CLinkedEvent's own mTag word (no mBuf allocation -- these events carry their
 * payload inline, matching event.h's own documented "class-code in low byte" tag
 * scheme, class-code 0x3 here = "raw 3-byte MIDI message"): byte0=0x3 (tag), byte1=
 * mChannel, byte2=CC number, byte3=7-bit value. Address-MSB/LSB CCs are ONLY emitted
 * when they differ from the caller-supplied `lastAddr` (redundant-address elision);
 * `lastAddr` is unconditionally overwritten with the SParam's own address at the very
 * end of the call regardless of which messages were actually built. Data-MSB/LSB CCs
 * are emitted unless that data byte is the 0xff "not set" sentinel. CC numbers: address
 * LSB = mCtrlChangeType itself (98 NRPN / 100 RPN), address MSB = mCtrlChangeType+1 (99
 * / 101), data MSB = 6 (Data Entry MSB, always), data LSB = 38/0x26 (Data Entry LSB,
 * always) -- standard MIDI NRPN/RPN wire protocol. Return value is the real int count
 * of messages this call actually appended (0-4), summed by every Append* caller.
 *
 * Several Api+0x90/+0x94 "soft assert" call sites inside AppendSingleParam/
 * AppendAllParams are additionally gated behind `cmp DWORD ds:0x8e7a0ec,0` -- that
 * .rodata global is a compile-time constant 0 (confirmed by reading the file bytes
 * directly), so every one of those gated branches is provably dead in the real binary;
 * omitted here along with the diagnostic calls themselves, same established
 * Api+0x90/+0x94 convention as every other class in this project.
 *
 * `AppendParams` vs `AppendParamsDontCareAddr`: both iterate a caller-supplied
 * SBytePair address list (terminated by kInvalidBytePair, same sentinel convention as
 * Erase(SBytePair const*) below), binary-search each address in mParams, and call
 * AppendSingleParam with a FRESH per-call local `lastAddr` seeded from kInvalidBytePair
 * (NOT from `this->mCurAddr` -- confirmed identical in both functions). One extra
 * always-zero local (`[esp+0x24]`) exists only in AppendParams's own disassembly with
 * no isolated observable effect found this pass; both are reconstructed here as
 * behaviorally identical pending a closer look, flagged rather than silently
 * guessed away.
 */

#ifndef PARAM_TRACER_H
#define PARAM_TRACER_H

#include "event.h"
#include "tvector.h"

/* NRPN(98)/RPN(100) -- the enum's own underlying value IS the real wire CC base for
 * the address-LSB message (address-MSB is always base+1). Confirmed by
 * CParamTracer's default ctor storing literal 100 into mCtrlChangeType. */
enum ECtrlChange {
	eNRPN = 98,
	eRPN  = 100
};

/* A raw 7-bit MIDI data-byte pair -- used both as an NRPN/RPN address and as a 14-bit
 * (MSB,LSB) value. 2 bytes, no padding (confirmed: CParamTracer::SParam is exactly 4
 * bytes = 2x SBytePair). */
struct SBytePair {
	unsigned char b0;
	unsigned char b1;
};

/* .bss+0x0930a390, real mangled name `_ZL16kInvalidBytePair` (internal linkage). In
 * .bss (NOBITS) rather than .rodata/.data, so the real value is the zero-initialized
 * default {0,0} -- confirmed by reading the file directly (no static initializer
 * exists anywhere in the binary for this symbol). Used both as the "no address
 * selected" default (both CParamTracer ctors, Reset()) and as a list-terminator
 * sentinel (Erase(SBytePair const*), AppendParams/AppendParamsDontCareAddr). */
extern const SBytePair kInvalidBytePair;

class CParamTracer {
public:
	/* One (addr, 14-bit value) entry in the sorted array. See file header. */
	struct SParam {
		SBytePair mAddr;
		SBytePair mData;
	};

	/* .text+0x0808fe10. Channel 0, mCtrlChangeType=RPN(100) default. */
	CParamTracer();

	/* .text+0x08090000. */
	CParamTracer(unsigned char channel, ECtrlChange ctrlChangeType);

	/* .text+0x080901f0, 18 bytes -- re-stamps mChannel/mCtrlChangeType only (used
	 * when a CParamTracer is default-constructed in bulk, then individually
	 * initialized -- same "InitAfterDefaultCtor" convention already established
	 * elsewhere in this project, e.g. CControllerTracer). */
	void InitAfterDefaultCtor(unsigned char channel, ECtrlChange ctrlChangeType);

	/* .text+0x08090210. Clears the tracked-params array (Size()->0, capacity kept)
	 * and re-seeds mCurAddr from kInvalidBytePair. */
	void Reset();

	/* .text+0x08090230/0x080903d0. Binary-search-erase one entry, or every entry
	 * named in a kInvalidBytePair-terminated list, respectively. No-op if not
	 * found. */
	void Erase(const SBytePair &addr);
	void Erase(const SBytePair *addrList);

	/* .text+0x080905e0. Binary-search for `addr`; if found, overwrites its data
	 * (both bytes at once) with `data`. No-op (does NOT insert) if not found --
	 * the find-or-insert upsert is SetData() below, not this. */
	void ModifyData(SBytePair addr, SBytePair data);

	/* .text+0x08090690/0x080907a0. 14-bit (MSB*128+LSB) saturating increment/
	 * decrement of the entry at the CURRENT mCurAddr cursor (no-op if mCurAddr is
	 * out of the valid 0-126/0-127 range, if no matching entry exists, or -- for
	 * DataInc -- already at the 0x7f/0x7f maximum; symmetric for DataDec at 0). */
	void DataInc();
	void DataDec();

	/* .text+0x080908b0/0x080908c0. Raw forward-iteration over the sorted array
	 * (First() returns the first entry or 0 if empty; Next(p) returns the entry
	 * after `p`, or 0 at the end -- a soft Api+0x94 assert, omitted per this
	 * project's convention, fires if `p` is null or already out of range). */
	const SParam *First() const;
	const SParam *Next(const SParam *p) const;

	/* .text+0x08090970. Binary search; returns the matching entry or 0. */
	const SParam *Find(const SBytePair &addr) const;

	/* .text+0x080909e0. Lower-bound binary search: first entry with
	 * mAddr >= addr, or 0 if none. */
	const SParam *FindEqualOrNext(const SBytePair &addr) const;

	/* .text+0x08090a60. Builds up to 4 CC messages for one SParam and prepends
	 * them to `listHead` (real ground truth pushes each new CLinkedEvent onto the
	 * FRONT via `newNode->mNext = *listHead; *listHead = newNode`, i.e. reverses
	 * per-call emission order within a single param -- observably fine since every
	 * real caller only cares about the resulting set of CC messages, not their
	 * relative order within one SParam). `lastAddr` is the redundant-address-
	 * elision cursor, updated in place. Returns the real count of messages
	 * actually appended (0-4). See file header for the full protocol. */
	int AppendSingleParam(CLinkedEvent *&listHead, SBytePair &lastAddr, const SParam &param) const;

	/* .text+0x08090df0. Dumps every tracked entry as CC messages (used for full
	 * resync, e.g. after a program change): walks the sorted array with a local
	 * lastAddr seeded from kInvalidBytePair (same as AppendParams below), summing
	 * AppendSingleParam's return. Real disassembly opens with `if (listHead == 0)
	 * return 0;` -- i.e. it refuses to run against an ALREADY-null incoming list
	 * pointer. This reads as defensive/unreachable-in-practice (no real caller
	 * this pass traced exercises it, and skipping all work on an empty starting
	 * list would be an odd "dump everything" contract) rather than a normal-case
	 * short-circuit; reproduced as-is since it's real, unambiguous control flow,
	 * flagged rather than silently dropped. */
	int AppendAllParams(CLinkedEvent *&listHead) const;

	/* .text+0x08091140/0x08091010. Like AppendAllParams but restricted to the
	 * addresses named in a kInvalidBytePair-terminated `addrList`; both seed a
	 * FRESH local lastAddr from kInvalidBytePair rather than `this->mCurAddr` --
	 * see file header's AppendParams/AppendParamsDontCareAddr note. */
	int AppendParams(CLinkedEvent *&listHead, const SBytePair *addrList) const;
	int AppendParamsDontCareAddr(CLinkedEvent *&listHead, const SBytePair *addrList) const;

	/* .text+0x08091e90. Binary-search upsert: if `addr` already has an entry,
	 * overwrites its data (like ModifyData); otherwise inserts a new SParam{addr,
	 * data} in sorted position via TVector::Insert. No-op if `addr.b0`/`addr.b1`
	 * is out of the valid 0-126/0-127 range. */
	void SetData(SBytePair addr, SBytePair data);

	/* .text+0x08092430/0x08092850. Upsert just one data byte at the CURRENT
	 * mCurAddr cursor: if an entry already exists there, overwrites only that one
	 * byte; otherwise inserts a new entry with the OTHER data byte defaulted to
	 * 0xff ("not set"). No-op if mCurAddr or the new value is out of range. */
	void SetDataLSB(unsigned char lsb);
	void SetDataMSB(unsigned char msb);

private:
	unsigned char mChannel;        /* +0x00 */
	ECtrlChange   mCtrlChangeType; /* +0x04 */
	SBytePair     mCurAddr;        /* +0x08 */
	TVector<SParam, 0> mParams;    /* +0x0c */
};

#endif /* PARAM_TRACER_H */
