/*
 * ctrl_and_param_tracer.h  -  CCtrlAndParamTracer : public CControllerTracer, adding
 * RPN/NRPN parameter-number tracking (via two embedded CParamTracer subobjects) on top
 * of CControllerTracer's plain CC/pressure/pitch-bend/program state. Closes out the
 * "Tracer" family cluster found in the 2026-07-28 `nm -C` sweep alongside
 * controller_tracer.h -- see that file's header for the shared background.
 *
 * Confirmed `: public CControllerTracer` via a real `__si_class_type_info` at
 * .rodata+0x08e8269c pointing at CControllerTracer's own typeinfo (already noted in
 * param_tracer.h). Real call-xref-traced the same way as CControllerTracer: only
 * CParamTracer's own (already-reconstructed) methods, CEventsPool::GetNewEvent,
 * operator new/delete/new[] -- nothing pulling in any out-of-scope subsystem.
 *
 * OBJECT LAYOUT (0xc8 bytes, confirmed field-by-field from the ctor/copy-ctor/
 * operator= bodies, which independently agree on every offset):
 *   +0x00..0x8b  CControllerTracer base subobject (own vtable ptr installed first,
 *                 then overwritten with this class's own vtable -- standard C++
 *                 base-then-derived construction order, confirmed real, not manually
 *                 modeled).
 *   +0x8c        CParamTracer mRpnParams   ctor stamps mCtrlChangeType=0x64 (100=RPN)
 *   +0xa8        CParamTracer mNrpnParams  ctor stamps mCtrlChangeType=0x62 (98=NRPN)
 *                 (resolves param_tracer.h's own "NRPN vs RPN, unconfirmed" note)
 *   +0xc4        CParamTracer *mLastUpdated   NULL initially; set by UpdateCtrl() to
 *                 &mRpnParams or &mNrpnParams whenever a valid RPN/NRPN
 *                 Parameter-Number CC selects one (see below). Real real type is a
 *                 bare `void*`/subobject-address, exposed here as a typed pointer for
 *                 clarity -- ground truth never dereferences it as anything but a
 *                 CParamTracer* (via SetDataMSB/SetDataLSB/DataInc/DataDec calls).
 * (sizeof(CParamTracer)==0x1c, confirmed by param_tracer.h; 0x8c+0x1c=0xa8,
 * 0xa8+0x1c=0xc4 -- the two subobjects are laid out back-to-back with zero padding.)
 *
 * CParamTracer EAGER-CAPACITY DIVERGENCE: ground truth's own ctors additionally
 * pre-`operator new(0x80)`-allocate BOTH mRpnParams.mParams and mNrpnParams.mParams up
 * to a fixed 32-SParam capacity immediately, via ~300 lines of inlined
 * TVector-insert-growth boilerplate per subobject (the same generic "insert an empty
 * range to force a capacity bump" shape CParamTracer's own real callers never actually
 * need). NOT reproduced here: composing two plain `CParamTracer` members (each
 * default/value-constructed via its own already-reconstructed ctor, which leaves
 * mParams empty/unallocated) is observably identical after any real sequence of
 * operations -- TVector::MakeCapacity() (tvector.h) grows lazily on first real
 * Insert() regardless. Pure eager-vs-lazy allocation-timing difference, not a
 * behavioral one; same class of simplification already established project-wide for
 * compiler-regenerable boilerplate (see e.g. stream_family.h's own "GCC-synthesized
 * boilerplate; NOT hand-transcribed" note).
 *
 * UpdateCtrl(unsigned char ctrlNum, unsigned char value) OVERRIDE (.text+0x08092c70,
 * ~0x1e0 bytes, confirmed via the real vtable slot-4 target at .rodata+0x8e82650+0x10):
 * always calls CControllerTracer::UpdateCtrl(ctrlNum, value) first (writes mCtrl[]
 * unconditionally, base's own body), then additionally dispatches on ctrlNum:
 *   6      (Data Entry MSB)     mLastUpdated->SetDataMSB(value), if mLastUpdated set
 *   0x26   (Data Entry LSB)     mLastUpdated->SetDataLSB(value), if mLastUpdated set
 *   0x60   (Data Increment)     mLastUpdated->DataInc(), if mLastUpdated set
 *   0x61   (Data Decrement)     mLastUpdated->DataDec(), if mLastUpdated set
 *   0x62   (NRPN Param# LSB)    mNrpnParams.mCurAddr.b1 = value directly (bypassing
 *                                 CParamTracer's own public API -- see the `friend`
 *                                 grant added to param_tracer.h this pass); if the
 *                                 OLD mCurAddr.b0 and new value are both < 0x80 and
 *                                 not BOTH == 0x7f (the standard MIDI RPN/NRPN "Null"
 *                                 deselect sentinel), sets mLastUpdated = &mNrpnParams
 *   0x63   (NRPN Param# MSB)    mNrpnParams.mCurAddr.b0 = value directly; same
 *                                 range/deselect check against the OLD b1, same
 *                                 mLastUpdated update
 *   0x64   (RPN Param# LSB)     mRpnParams.mCurAddr.b1 = value directly; same shape
 *   0x65   (RPN Param# MSB)     mRpnParams.mCurAddr.b0 = value directly; same shape
 *   other  no extra effect beyond the base mCtrl[] write
 * (CC-to-field mapping confirmed via a direct byte dump of the real 5-entry jump
 * table at .rodata+0x8e7a228+0x61*4..+0x65*4, not inferred -- b0=Parameter-Number MSB,
 * b1=Parameter-Number LSB, matching CParamTracer's own mCtrlChangeType-as-wire-LSB-CC
 * convention: RPN's LSB CC (0x64) is literally `(unsigned char)eRPN`.)
 *
 * Reset() OVERRIDE (.text+0x0808f000, real vtable slot 2): base CControllerTracer::
 * Reset() logic (mCtrl[]/mChnPressure/mProgramNumber/pitch-bend/bank) PLUS
 * mRpnParams.Reset() and mNrpnParams.Reset() (each subobject's own real Clear()-the-
 * array + re-seed-mCurAddr-from-kInvalidBytePair logic, inlined in ground truth,
 * modeled here as ordinary calls into the already-reconstructed CParamTracer::
 * Reset()), plus mLastUpdated = 0.
 *
 * InitAfterDefaultCtor(unsigned char) OVERRIDE (.text+0x0808ef70, weak, real vtable
 * slot 3): base's own mChannel = channel, PLUS re-stamps BOTH subobjects' mChannel
 * (via CParamTracer::InitAfterDefaultCtor(channel, eRPN)/eNRPN) -- does NOT touch
 * mCurAddr/mParams/mLastUpdated (matches CParamTracer::InitAfterDefaultCtor's own
 * "channel/type only" real body).
 *
 * AppendAllParams()/AppendParams() OVERRIDEs: ground truth processes mRpnParams and
 * mNrpnParams in an order chosen by mLastUpdated (whichever was most recently touched
 * goes through the address-CARING `CParamTracer::AppendParams`, the other through
 * `AppendParamsDontCareAddr`; NULL mLastUpdated uses DontCareAddr for both), then sums
 * both counts. param_tracer.h's own header already established
 * `AppendParams`/`AppendParamsDontCareAddr` are behaviorally identical (same real
 * messages, one unused extra local) -- so which variant is called, and in which
 * order, has NO effect on the resulting SET of appended messages (only their
 * relative position in the linked chain, which no in-scope caller observes). Modeled
 * here as a fixed mRpnParams-then-mNrpnParams order using AppendParamsDontCareAddr for
 * both, for the identical total effect with far less code; flagged rather than
 * silently presented as literal.
 */

#ifndef CTRL_AND_PARAM_TRACER_H
#define CTRL_AND_PARAM_TRACER_H

#include "controller_tracer.h"
#include "param_tracer.h"

class CCtrlAndParamTracer : public CControllerTracer {
public:
	/* .text+0x080918c0. Channel 0. */
	CCtrlAndParamTracer();

	/* .text+0x08091450. */
	explicit CCtrlAndParamTracer(unsigned char channel);

	/* .text+0x08092240. Deep-copies both embedded CParamTracer subobjects (real
	 * per-subobject TVector::Insert calls); rebases mLastUpdated onto this object's
	 * own subobjects rather than `other`'s. */
	CCtrlAndParamTracer(const CCtrlAndParamTracer &other);

	/* .text+0x08092e80. Self-assignment-safe (real `if (this==&other) return`
	 * guard). Same per-subobject deep-copy + mLastUpdated-rebasing as the copy
	 * ctor. */
	CCtrlAndParamTracer &operator=(const CCtrlAndParamTracer &other);

	/* .text+0x08092c70. See file header for the full CC-to-subobject dispatch
	 * table. */
	virtual void UpdateCtrl(unsigned char ctrlNum, unsigned char value);

	/* .text+0x0808f000. See file header. */
	virtual void Reset();

	/* .text+0x0808ef70 (weak). See file header. */
	virtual void InitAfterDefaultCtor(unsigned char channel);

	/* .text+0x08091e00. Sum of both subobjects' AppendAllParams(). */
	int AppendAllParams(CLinkedEvent *&cursor) const;

	/* .text+0x08091d20. Sum of both subobjects' AppendParamsDontCareAddr(), one
	 * address list per subobject (rpnAddrList for mRpnParams, nrpnAddrList for
	 * mNrpnParams). See file header for the AppendParams-vs-DontCareAddr
	 * simplification. */
	int AppendParams(CLinkedEvent *&cursor, const SBytePair *rpnAddrList,
	                  const SBytePair *nrpnAddrList) const;

private:
	CParamTracer   mRpnParams;    /* +0x8c, mCtrlChangeType == eRPN */
	CParamTracer   mNrpnParams;   /* +0xa8, mCtrlChangeType == eNRPN */
	CParamTracer  *mLastUpdated;  /* +0xc4, NULL or &mRpnParams/&mNrpnParams */
};

#endif /* CTRL_AND_PARAM_TRACER_H */
