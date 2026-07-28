/*
 * controller_tracer.h  -  CControllerTracer, a per-channel "last known MIDI controller
 * state" tracker (128 CCs + channel pressure + pitch bend + program/bank) with a
 * "changed since last send" append-to-event-list API. Sibling of the already-
 * reconstructed CParamTracer (param_tracer.h) -- both surfaced in the same 2026-07-28
 * `nm -C` class-inventory sweep and share the same CEventsPool dependency, but
 * CParamTracer's own header deliberately deferred this one for a follow-up pass
 * ("simpler... genuinely self-contained, a clean follow-up"). This pass closes it,
 * plus its real subclass CCtrlAndParamTracer (ctrl_and_param_tracer.h).
 *
 * Real call-xref-traced (objdump -dr -M intel over the whole class's .text range,
 * 0x0808efa0-0x0808fe10, plus the 4 tiny out-of-line weak virtuals at 0x08182de0-
 * 0x08182e20): CEventsPool::GetNewEvent (already reconstructed, events_pool.h),
 * operator new/delete, and nothing else touching CZ/CStorage/the ES-family task
 * god-objects/the virtual-driver subsystem/Peg GUI.
 *
 * OBJECT LAYOUT (0x8c bytes, confirmed field-by-field from Reset()/both ctors'
 * identical field-store sequences, cross-checked against every Append* method's own
 * field reads):
 *   +0x00  vtable ptr           real Itanium vtable at .rodata+0x08e82670 (14-slot:
 *                                D1/D0 dtor, UpdateCtrl -- see below); a genuine
 *                                polymorphic base, not a manually-swapped vtable.
 *   +0x04  unsigned char mCtrl[128]   one byte per MIDI CC# 0-127, 0xff = "not set"
 *   +0x84  unsigned char mChnPressure  0xff = "not set"
 *   +0x85  unsigned char mPitchBendLSB
 *   +0x86  unsigned char mPitchBendMSB
 *   +0x87  unsigned char mProgramNumber  0xff = "not set"
 *   +0x88  unsigned char mBankMSB        (CC0)  0xff = "not set"
 *   +0x89  unsigned char mBankLSB        (CC32) 0xff = "not set"
 *   +0x8a  unsigned char mChannel
 * Reset()/both ctors set mCtrl[]/mChnPressure/mProgramNumber to the explicit 0xff
 * sentinel, but copy {mPitchBendLSB,mPitchBendMSB} and {mBankMSB,mBankLSB} as two
 * raw 16-bit reads of the SAME real global word at .bss+0x0930a390 -- confirmed by
 * param_tracer.h to be `kInvalidBytePair`, a real zero-initialized {0,0} .bss symbol
 * (NOT the 0xff sentinel!). So pitch-bend/bank fields start life as {0,0}, not
 * "unset" -- transcribed faithfully even though it means AppendPitchBend's own
 * `mPitchBendLSB==0xff` gate is NOT satisfied immediately after construction (real
 * ground-truth behavior, not a bug introduced here).
 *
 * VTABLE (`.rodata+0x08e82670`, confirmed via a direct byte dump + symbol lookup of
 * every slot target):
 *   slot 0: ~CControllerTracer() D1   .text+0x08182de0 (11 bytes, vtable-ptr reset only)
 *   slot 1: ~CControllerTracer() D0   .text+0x08182e20 (23 bytes, + operator delete(this))
 *   slot 2: Reset()                    .text+0x0808efa0 (real, own class)
 *   slot 3: InitAfterDefaultCtor(uc)   .text+0x08182df0 (15 bytes: mChannel = arg)
 *   slot 4: UpdateCtrl(uc,uc)          .text+0x08182e00 (18 bytes: mCtrl[a]=b, no bounds
 *                                       check in the base -- every real caller,
 *                                       SetDefCtrls()'s own loop, already guarantees
 *                                       ctrlNum<0x80).
 * `CCtrlAndParamTracer` overrides slot 4 only (own real vtable at .rodata+0x8e82650),
 * inheriting slots 0-3 unchanged in effect (D1/D0 do get real CCtrlAndParamTracer-
 * specific out-of-line bodies too, but they're the mechanical "reset vtable ptr [+
 * delete]" pattern every dtor in this project already uses -- ordinary C++ `virtual`
 * inheritance regenerates them, ctor/dtor not modeled as separate real symbols here).
 *
 * DEFAULT-CC TABLE (`.rodata+0x8e7a160`, 128 bytes, real bytes transcribed below --
 * used by SetDefCtrls()/AppendDefaultCtrl()/AppendDefaultCtrls()): 0xff = "no default
 * value defined for this CC".
 *
 * EVENT-LIST IDIOM: every Append* method takes `CLinkedEvent *&cursor` and, unlike
 * CParamTracer::AppendSingleParam's own front-push idiom (see param_tracer.h), grows
 * the chain FORWARD from an already-existing anchor node: `cursor->SetNext(newNode);
 * cursor = newNode;` -- confirmed from every Append* method's own identical
 * `ebp = *cursorRef; new->something; ebp->mNext(+0x8) = new; *cursorRef = new;`
 * sequence. Every real Append* method starts with `if (cursor == 0) { softAssert;
 * return 0; }` -- a REAL, meaningful guard (unlike the dead-code Api+0x90/+0x94 sites
 * below), preserved here since building on a null cursor would be undefined. This
 * means every real caller must seed `cursor` with a pre-existing sentinel/anchor node
 * before calling any Append* method here -- not traced this pass (no in-scope caller
 * reaches these methods; every real call site lives in the still-out-of-scope
 * ES-family task god-objects).
 *
 * Several Api+0x90/+0x94 "soft assert" call sites inside the Append* family are
 * additionally gated behind `cmp DWORD ds:0x8e7a0ec,0` / `cmp DWORD ds:0x8e7a0f8,0` --
 * both real .rodata addresses, both confirmed compile-time-constant 0 by reading the
 * file bytes directly (same pattern already established in param_tracer.h for a
 * different pair of addresses) -- so every branch they gate is provably dead;
 * omitted, along with the diagnostic calls themselves, per this project's established
 * Api+0x90/+0x94 convention. The remaining `cmp DWORD[event],0xf` post-GetNewEvent()
 * checks (validating the freshly-popped node's tag == CEvent's own "fresh" sentinel)
 * are genuine runtime soft-asserts, but every branch converges on building the SAME
 * message from the SAME source fields regardless of outcome (verified for every
 * Append* method individually) -- so they're modeled as straight-line code too.
 *
 * PACKED TAG-WORD FORMAT (every Append* method, confirmed by literal transcription of
 * each method's own shift/or sequence): byte0(bits0-7)=class-code, byte1(bits8-15)=
 * mChannel, byte2(bits16-23)=CC-number (0 for channel-pressure/pitch-bend, since
 * those aren't CC-numbered), byte3(bits24-31)=7-bit value. Class codes seen: 0x3 =
 * generic CC (bank-select MSB uses CC#0, bank-select LSB uses CC#0x20=32), 0x4 =
 * program change (byte2 unused, byte3=program#), 0x5 = channel pressure (byte2
 * unused), 0x6 = pitch bend (byte2 unused, byte3=MSB, and -- uniquely for this one --
 * the LSB is packed into byte2's own would-be slot... no: re-confirmed from
 * AppendPitchBend's own literal shift chain, pitch bend is `LSB<<24 | MSB<<16 |
 * channel<<8 | 6`, i.e. byte3=LSB, byte2=MSB -- the one class code that carries TWO
 * data bytes instead of one, doubling up what's normally the "CC-number" slot).
 * AppendDefault* variants send class-appropriate "reset to default" messages: default
 * pitch bend is center (LSB=0, MSB=0x40, confirmed real immediate `or ebx,0x40000006`);
 * default channel pressure is 0 (no explicit value byte set, i.e. implicit 0); default
 * CC values come from the DEFAULT-CC TABLE above.
 *
 * RETURN VALUES: no in-scope caller consumes any Append* method's return value (see
 * above), so exact byte-for-byte register-arithmetic fidelity wasn't chased past what
 * a direct read gives for free. AppendCtrl/AppendDefault{ChnPressure,PitchBend,Ctrl}
 * clearly return a 0/1 "was anything appended" boolean (real `mov eax,1` on the
 * success path). AppendCtrls/AppendDefaultCtrls clearly return a real running count
 * (real accumulator local, incremented once per event actually appended).
 * AppendChnPressure returns the channel-pressure VALUE itself on success (real
 * `mov eax,ebp` where ebp is the value, not a boolean -- transcribed as observed, not
 * "corrected" to a boolean). AppendFullProgram's own real counter has a couple of
 * arithmetic quirks not fully isolated (an `edi`/`ebp` register pair whose final value
 * doesn't cleanly reduce to "count of the 3 possible messages" in every combination);
 * modeled here as the straightforward "count of the up-to-3 messages actually sent"
 * rather than chasing the exact quirk, flagged rather than silently guessed away.
 */

#ifndef CONTROLLER_TRACER_H
#define CONTROLLER_TRACER_H

#include "event.h"

class CControllerTracer {
public:
	/* .text+0x0808f090. Channel 0. */
	CControllerTracer();

	/* .text+0x0808f100. */
	explicit CControllerTracer(unsigned char channel);

	/* .text+0x08182de0 (D1) / 0x08182e20 (D0). See file header vtable note. */
	virtual ~CControllerTracer();

	/* .text+0x0808efa0 (real vtable slot 2). Re-inits every tracked field to its
	 * "not set"/default state (mChannel is NOT touched -- only the ctors set it).
	 * See file header. Virtual: CCtrlAndParamTracer overrides it to also reset its
	 * two embedded CParamTracer subobjects (ctrl_and_param_tracer.h). */
	virtual void Reset();

	/* .text+0x08182df0 (weak, out-of-line virtual slot 3 body). mChannel = channel.
	 * Virtual for the same reason as Reset() above. */
	virtual void InitAfterDefaultCtor(unsigned char channel);

	/* .text+0x0808f170. Bounds-checked: no-op if ctrlNum >= 0x80. */
	void EraseCtrl(unsigned char ctrlNum);

	/* .text+0x0808f1c0. Erases every entry named in a `ctrls`, terminated by the
	 * first byte >= 0x80 (0xff by convention). No-op if ctrls is null. */
	void EraseCtrls(const unsigned char *ctrls);

	/* .text+0x0808f2a0. For each ctrlNum named in a >=0x80-terminated `ctrls` list,
	 * looks up the DEFAULT-CC TABLE and, if a default is defined for it, dispatches
	 * through the virtual UpdateCtrl(ctrlNum, defaultValue) -- i.e. this always
	 * writes through the (possibly overridden) virtual hook, never mCtrl[] directly.
	 * No-op if ctrls is null. */
	void SetDefCtrls(const unsigned char *ctrls);

	/* .text+0x0808f320. Appends a channel-pressure message if mChnPressure is set.
	 * See file header for the cursor/return-value conventions. */
	int AppendChnPressure(CLinkedEvent *&cursor) const;

	/* .text+0x0808f410. Appends a pitch-bend message if mPitchBendLSB != 0xff. */
	int AppendPitchBend(CLinkedEvent *&cursor) const;

	/* .text+0x0808f520. Appends up to 3 messages (bank MSB, bank LSB, program
	 * change), each independently gated on its own field being set. */
	int AppendFullProgram(CLinkedEvent *&cursor) const;

	/* .text+0x0808f770. Appends mCtrl[ctrlNum] if set. `ctrlNum` is used to index
	 * mCtrl[] unchecked -- every real caller keeps it < 0x80. */
	int AppendCtrl(CLinkedEvent *&cursor, unsigned char ctrlNum) const;

	/* .text+0x0808f890. Like AppendCtrl but over a >=0x80-terminated `ctrls` list;
	 * returns the count actually appended. */
	int AppendCtrls(CLinkedEvent *&cursor, const unsigned char *ctrls) const;

	/* .text+0x0808f9d0. Appends a channel-pressure message with an implicit value
	 * of 0 if mChnPressure is set (i.e. "reset to default"). */
	int AppendDefaultChnPressure(CLinkedEvent *&cursor) const;

	/* .text+0x0808faa0. Appends a pitch-bend-center (LSB=0,MSB=0x40) message if
	 * mPitchBendLSB is set. */
	int AppendDefaultPitchBend(CLinkedEvent *&cursor) const;

	/* .text+0x0808fb80. If mCtrl[ctrlNum] is set, appends ITS OWN current value
	 * (NOT the default table's); only falls back to the DEFAULT-CC TABLE value if
	 * mCtrl[ctrlNum] is currently unset. No-op if neither is available. */
	int AppendDefaultCtrl(CLinkedEvent *&cursor, unsigned char ctrlNum) const;

	/* .text+0x0808fcc0. Like AppendDefaultCtrl but over a list, EXCEPT it does NOT
	 * fall back per-element -- only ctrls that are BOTH currently tracked
	 * (mCtrl[c] != 0xff) AND have a defined default get a message, always using the
	 * DEFAULT TABLE value (not the tracked one). A genuinely different rule from
	 * AppendDefaultCtrl's own "current value, default as fallback" -- transcribed
	 * as observed, not unified. */
	int AppendDefaultCtrls(CLinkedEvent *&cursor, const unsigned char *ctrls) const;

	/* .text+0x08182e00 (weak, out-of-line virtual slot 4 body, base implementation).
	 * mCtrl[ctrlNum] = value, unconditionally, no bounds check. CCtrlAndParamTracer
	 * overrides this (ctrl_and_param_tracer.h) to additionally forward Parameter-
	 * Number/Data-Entry/Data-Increment CCs into its own embedded CParamTracer
	 * subobjects. */
	virtual void UpdateCtrl(unsigned char ctrlNum, unsigned char value);

private:
	unsigned char mCtrl[128];      /* +0x04 */
	unsigned char mChnPressure;    /* +0x84 */
	unsigned char mPitchBendLSB;   /* +0x85 */
	unsigned char mPitchBendMSB;   /* +0x86 */
	unsigned char mProgramNumber;  /* +0x87 */
	unsigned char mBankMSB;        /* +0x88 */
	unsigned char mBankLSB;        /* +0x89 */
	unsigned char mChannel;        /* +0x8a */
};

#endif /* CONTROLLER_TRACER_H */
