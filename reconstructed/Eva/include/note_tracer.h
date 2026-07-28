/*
 * note_tracer.h  -  CNoteTracer, a per-channel "currently sounding MIDI notes" tracker
 * with a small polymorphic hook (RendundantInsertion) for handling repeated note-on
 * messages. Real sibling of the already-reconstructed CParamTracer/CControllerTracer/
 * CCtrlAndParamTracer family (param_tracer.h/controller_tracer.h/
 * ctrl_and_param_tracer.h) -- surfaced in the same 2026-07-28 `nm -C` class-inventory
 * sweep, and CControllerTracer's own header comment already flagged this one as "a
 * same-size follow-up on its own merits". Closed this pass. `CNoteTracerTransposer :
 * public CNoteTracer` (real sibling, same source region 0x080971f0-0x08098700) is
 * DELIBERATELY DEFERRED -- see the note at the end of this comment.
 *
 * Real call-xref-traced (objdump -dr -M intel over .text 0x08093050-0x080971ea):
 * `CEventsPool::GetNewEvent` (already reconstructed, events_pool.h), `HAL_Disable/
 * EnableInterrupts` (real externs), plain libc (malloc/free/realloc). Zero touch on
 * CZ/CStorage/the ES-family task god-objects/the virtual-driver subsystem/Peg GUI --
 * same clean dependency footprint as the rest of this Tracer family.
 *
 * OBJECT LAYOUT (>= 0x91 bytes, confirmed field-by-field from both real ctors
 * @.text+0x08093ff0/0x080940b0 and cross-checked against every method that touches
 * these offsets):
 *   +0x00  vtable ptr        real Itanium vtable @.rodata+0x08e82720 (5-slot: D1/D0
 *                             dtor, RendundantInsertion -- a genuine polymorphic base,
 *                             overridden by CNoteTracerTransposer's own vtable
 *                             @.rodata+0x08e82708).
 *   +0x04  unsigned char mNoteIndex[128]  one slot-index byte per MIDI note number
 *                             (0-127); 0xff = "note not currently tracked". O(1)
 *                             note-number -> mNotes[] slot lookup, confirmed by
 *                             Insert()/Remove()'s own direct indexed reads/writes.
 *   +0x84  TDynBuffer<CBufferedNote> mNotes  malloc-backed unordered array of
 *                             currently-active notes (mBegin/mCapacity/mSize, 12
 *                             bytes -- see TDynBuffer<T> below). Initial capacity 32
 *                             entries (both ctors malloc(0x80)=32*4 bytes, filled with
 *                             0xffffffff sentinel dwords -- dead once mSize starts
 *                             tracking real content; not otherwise meaningful).
 *   +0x90  unsigned char mChannel  ctor param; default ctor: 0.
 *
 * CBufferedNote (4 bytes, one mNotes[] element, confirmed from Insert()'s own
 * `(rawValue>>0x10)&0xff` note-number extraction, Remove()'s own byte-2 read for
 * re-indexing after a compacting removal, and the base RendundantInsertion's own
 * "increment the low byte" body):
 *   +0x0  unsigned char mCount     retrigger/duplicate-insertion counter, only ever
 *                          touched by RendundantInsertion (see below); a genuinely
 *                          new note-on always inserts with whatever mCount the caller
 *                          packed in (every real caller this pass reached leaves it
 *                          0, by construction convention, not enforced by CNoteTracer
 *                          itself).
 *   +0x1  unsigned char mChannel   only read by CNoteTracerTransposer's own override
 *                          (out of scope this pass); never read by any CNoteTracer
 *                          method itself.
 *   +0x2  unsigned char mNote      the mNoteIndex[] key -- MIDI note number.
 *   +0x3  unsigned char mVelocity  note-on velocity; consumed by ListNotesOn/
 *                          ListSoundsOn (release-velocity fields are unused, see
 *                          ListNotesOff/ListSoundsOff below).
 *
 * TDynBuffer<T> (12 bytes: T* mBegin; unsigned mCapacity; unsigned mSize -- element
 * counts, not byte counts) is a NEW, minimal, malloc-backed dynamic-array helper type
 * this pass introduces (distinct from the already-reconstructed `TVector<T,N>`,
 * tvector.h, which is a real C++ template with its own vtable/dtor and Insert/Erase
 * growth logic) -- confirmed as a genuinely separate, simpler shape by
 * CreateBuffer/ReallocBuffer/DestroyBuffer's own disassembly: all three are STATIC
 * member functions (no `this` load at all -- their sole `TDynBuffer<CBufferedNote>&`
 * parameter is the only pointer ever dereferenced), directly writing raw
 * offsets +0x0/+0x4/+0x8, matching `mNotes` embedded at CNoteTracer+0x84 exactly.
 * `SwapBuffer(TDynBuffer<CBufferedNote>&)` alone IS non-static (uses `this` AND the
 * parameter): it lazily malloc(0x80)-initializes the passed-in buffer if it's still
 * empty (mBegin==0), then swaps {mBegin,mCapacity,mSize} with `this->mNotes`,
 * invalidating+rebuilding `this`'s own mNoteIndex[] cache around the swap (same
 * ClearEntries()/RefreshEntries() idiom the free-function Swap() below reuses).
 * CreateBuffer/ReallocBuffer/DestroyBuffer themselves have no in-scope caller this
 * pass traced (ctors/Insert/dtor all inline their own malloc/realloc/free directly,
 * matching the sibling Tracer classes' own established convention of NOT routing
 * through shared allocation helpers) -- reconstructed anyway since they're real,
 * simple, exported symbols with an unambiguous shape.
 *
 * mNoteIndex[]/mNotes[] INVARIANT: every method that mutates mNotes[] wholesale
 * (Swap, SwapBuffer, operator=, copy ctor, ResetPendingNotes) follows the SAME
 * three-step idiom -- invalidate every currently-tracked note's mNoteIndex[] entry
 * (set to 0xff) using the OLD mNotes[] contents, do the raw mNotes[]/TDynBuffer field
 * mutation, then rebuild mNoteIndex[] from the NEW mNotes[] contents. ClearEntries()/
 * RefreshEntries() are literally the "invalidate" and "rebuild" halves of this exact
 * idiom, exposed as their own public methods (real ground truth: ClearEntries()
 * touches ONLY mNoteIndex[], never mSize; RefreshEntries() is its inverse). The
 * free-function `Swap(CNoteTracer&, CNoteTracer&)` is ground truth's own
 * `ClearEntries(); ClearEntries(); <swap raw TDynBuffer fields>; RefreshEntries();
 * RefreshEntries();` sequence (confirmed instruction-for-instruction) -- reconstructed
 * here as exactly that composition rather than re-transcribing the ~350-line unrolled
 * assembly a second time.
 *
 * Insert(CBufferedNote): O(1) via mNoteIndex[note.mNote]. If the note is NOT already
 * tracked (index==0xff): append to mNotes[] (growing via realloc-double-capacity, on
 * failure a soft Api+0x94 assert then a best-effort insert into the stale
 * over-capacity buffer -- ground truth, preserved), set mNoteIndex[note]=newSlotIndex.
 * If the note IS already tracked (a genuine re-trigger while still sounding): calls
 * the virtual `RendundantInsertion(mNotes[index], note)` instead of touching mNotes[]
 * directly -- this is the ONLY call site of RendundantInsertion in this class,
 * confirmed by direct vtable-slot dispatch (`call [vtable+8]`) rather than a
 * statically-resolved direct call, i.e. genuinely polymorphic (CNoteTracerTransposer's
 * own override fires here when `this` is really a CNoteTracerTransposer).
 *
 * RendundantInsertion(CBufferedNote &existing, CBufferedNote incoming) [virtual,
 * base body @.text+0x08183f60, 8 bytes]: real ground truth is `*(unsigned*)&existing
 * += 1` -- a raw 32-bit increment of the WHOLE 4-byte struct, not just `existing.
 * mCount`. Modeled here as `existing.mCount++` (byte-identical for every mCount value
 * that doesn't wrap 0xff->0x00, which would carry into mChannel -- never observed/
 * expected in practice; `incoming` is genuinely unused in the base body, matching the
 * real disassembly, which never reads the by-value second argument).
 *
 * Remove(unsigned char note): O(1) via mNoteIndex[note]. If mNotes[index].mCount != 0
 * (a pending duplicate/retrigger, set by RendundantInsertion above): just decrements
 * the WHOLE 4-byte struct by 1 (same raw-dword idiom as RendundantInsertion, mirrored
 * here as `mCount--`) and returns WITHOUT removing the slot -- i.e. a note-off only
 * fully releases a note once every stacked re-trigger has had a matching Remove().
 * Otherwise (mCount==0): swap-removes the slot (move the LAST slot's contents into
 * the removed slot, mSize--), re-pointing the moved note's mNoteIndex[] entry to its
 * new (former) slot index. No-op if the note isn't currently tracked (index==0xff).
 *
 * GetLeftMost()/GetRightMost(): linear scans of mNotes[] returning the note-number
 * field only (GCC auto-vectorized GetRightMost() with SSE `pshufb`/`pcmpgtb`
 * reductions -- same "compiler-vectorized simple reduction loop" artifact already
 * catalogued for OA.ko, see agent memory; reconstructed here as the equivalent plain
 * scalar loop). GetLeftMost() = the SMALLEST mNote value present (unsigned min-reduce,
 * sentinel 0xff never survives since it's the unsigned MAXIMUM, so any real note
 * replaces it); returns -1 (0xff as signed char) if mSize==0. GetRightMost() = the
 * LARGEST mNote value (signed max-reduce, sentinel -1 correctly stays "smallest" until
 * a real 0-127 value replaces it); returns -1 if empty. Confirmed by the two
 * functions' opposite `cmova`(unsigned)/`cmovl`(signed) selection conditions in the
 * real scalar tail loops -- not simply "symmetric functions with swapped names".
 *
 * ListNotesOn(cursor)/ListNotesOff(cursor)/ListSoundsOn(cursor)/ListSoundsOff(cursor)
 * [all const]: iterate every active mNotes[] entry, appending one packed MIDI event
 * tag per note via the SAME tail-cursor idiom `CControllerTracer`'s own Append* family
 * uses (`cursor->SetNext(new); cursor = new;`, cursor grows FORWARD -- confirmed
 * identical to controller_tracer.h's own documented idiom, the OPPOSITE of
 * CParamTracer's front-push idiom). Every real per-note diagnostic soft-assert
 * (Api+0x94, code 0x25a, gated on out-of-range channel/note/velocity bit-7 checks --
 * never true for real 0-15/0-127/0-127 MIDI data) is omitted per this project's
 * established Api+0x90/+0x94 convention (param_tracer.h/controller_tracer.h). Real
 * int return value in every case is the iterated note COUNT (mSize at entry), not a
 * "messages appended" count or a bool -- transcribed as observed. Packed tag-word
 * formats (byte0=class-code, byte1=mChannel, byte2=note, byte3=velocity, confirmed
 * per-function by direct shift/or/mask transcription):
 *   ListNotesOn:   0x1        | (mChannel<<8) | (note<<16) | (velocity<<24)
 *   ListNotesOff:  0x40000000 | (mChannel<<8) | (note<<16)              (byte3=0x40,
 *                              byte0=0 -- no release-velocity byte, confirmed real
 *                              `and eax,0xff0000` masking the entry's velocity byte
 *                              to 0 before combining)
 *   ListSoundsOn:  0xd        | (mChannel<<8) | (note<<16) | (velocity<<24)
 *   ListSoundsOff: 0x4000000c | (mChannel<<8) | (note<<16)              (byte0=0xc,
 *                              byte3=0x40, velocity masked out same as ListNotesOff)
 *
 * ListNotesOn(cursor, signed char velocityDelta) [const, 2nd overload]: same
 * tail-cursor iteration, but the emitted velocity is `note.mVelocity + velocityDelta`
 * (real raw 32-bit ADD of the two byte values placed in the tag's byte3 lane, NOT an
 * OR) CLAMPED to [1,127]: whenever the raw sum falls outside [0,127] (real ground
 * truth: `js` on the full 32-bit ADD result, which is exactly bit7-of-((velocity+
 * delta_byte) mod 256), proven equivalent to "sum outside [0,127]" for velocity in
 * [0,127] and delta in [-128,127]), the overflow handler clamps to 1 if the delta was
 * negative or 127 if non-negative (real: `test` the ORIGINAL tag_base's sign bit,
 * which is exactly delta's own sign bit; `cmovns` picks the low or high clamp).
 * Confirmed via direct disassembly of the overflow-handler block, not inferred.
 *
 * ClearEntries()/RefreshEntries(): see the mNoteIndex[]/mNotes[] INVARIANT note
 * above -- these ARE the invalidate/rebuild halves used throughout this class.
 *
 * ResetPendingNotes(): invalidates every mNoteIndex[] entry from the CURRENT mNotes[]
 * contents (same "invalidate" half as ClearEntries()) and ALSO resets mSize to 0 --
 * i.e. this is ClearEntries() plus discarding mNotes[]'s own logical content (the
 * malloc'd buffer/capacity are left alone, only mSize is reset), a genuinely different
 * (more destructive) operation from ClearEntries() despite the similar-looking body.
 *
 * `friend Swap(CNoteTracer&, CNoteTracer&)`, the copy ctor, and operator= are all
 * granted access to the private fields below (same "grant the sibling free function/
 * special member direct field access" convention already used in param_tracer.h for
 * CCtrlAndParamTracer).
 *
 * OUT OF SCOPE THIS PASS:
 *   - `operator<<(CMStream&, const CNoteTracer&)` / `operator>>(CMStream&,
 *     CNoteTracer&)` (.text+0x08093c80/0x08093cf0) -- real, simple bodies (write/read
 *     mSize then the raw mNotes[] buffer, rebuilding mNoteIndex[] on read via the same
 *     RefreshEntries() idiom above), but they pull in `CMStream::Write(const void*,
 *     unsigned)`/`Read(void*, unsigned)`, a class this project has NOT reconstructed
 *     anywhere yet (no existing header declares it). Deferred rather than hand-adding
 *     an under-specified new external dependency's class shape on faith; a clean,
 *     well-scoped follow-up once CMStream itself is reconstructed.
 *   - `CNoteTracerTransposer : public CNoteTracer` (.text+0x080971f0-0x08098700, ~12
 *     methods). Its OWN blocking dependency from the earlier param_tracer.h deferral
 *     note IS NOW RESOLVED: `CNoteTransposerOwner` (the ctor's 2nd argument type) is a
 *     genuinely tiny abstract interface -- real vtable @.rodata+0x08e82818 has exactly
 *     ONE pure-virtual slot beyond D1/D0 (`.rodata+0x8e82828` = the real
 *     `__cxa_pure_virtual`-style stub address, confirmed by direct byte dump), and its
 *     real, unambiguous signature was recovered by finding the one real override:
 *     `CTrackBase`'s own `__vmi_class_type_info` (.rodata+0x8e827fc) lists
 *     `CNoteTransposerOwner` as a real (non-virtual, offset-0) base, and CTrackBase's
 *     own vtable (.rodata+0x8e827d0) has `CTrackBase::OnRejectNotesForTransposeConflict
 *     (CNoteTracerTransposer&, CLinkedEvent*, CLinkedEvent*, unsigned char)` sitting in
 *     EXACTLY the inherited slot -- i.e. `virtual void
 *     OnRejectNotesForTransposeConflict(CNoteTracerTransposer&, CLinkedEvent*,
 *     CLinkedEvent*, unsigned char) = 0;` is CNoteTransposerOwner's entire contract.
 *     BUT `CNoteTracerTransposer::RendundantInsertion` (its own override of the hook
 *     above, .text+0x08093140-0x080936be, ~1400 bytes) is itself a genuinely dense
 *     duplicate-note-conflict resolver -- real channel-mismatch detection, an 8-way
 *     unrolled note-index-invalidation loop, multiple `CEventsPool::GetNewEvent()`+
 *     `CEvent` ctor/dtor sequences building up to 4 synthetic events, and a real
 *     virtual dispatch back out to `mOwner->OnRejectNotesForTransposeConflict(...)`
 *     (confirmed: `this+0x94` stores the `CNoteTransposerOwner&` ctor argument,
 *     dispatched through ITS OWN vtable slot 2). This one function alone is
 *     comparable in size/depth to an entire prior Tracer-family batch; deferred to
 *     keep this pass in scope. Real future pickup: the interface is no longer the
 *     blocker, only this one method's own algorithmic depth is -- everything else in
 *     CNoteTracerTransposer (both ctors, operator=, InsertConstNoteOn,
 *     InsertAndTranspose, RemoveAndTranspose, ListTransposedNotesOn x2,
 *     ListTransposedNotesOff) is ordinary-sized and was NOT independently re-checked
 *     for extra depth this pass, since the RendundantInsertion override alone already
 *     exceeds this pass's remaining budget.
 */

#ifndef NOTE_TRACER_H
#define NOTE_TRACER_H

/* Minimal malloc-backed dynamic array (mBegin/mCapacity/mSize, ELEMENT counts) -- see
 * file header for why this is a distinct, simpler shape from TVector<T,N>
 * (tvector.h). */
template <class T>
struct TDynBuffer {
	T *mBegin;
	unsigned mCapacity;
	unsigned mSize;
};

class CLinkedEvent;

class CNoteTracer {
public:
	/* One mNotes[] element. See file header for the confirmed byte-offset meanings. */
	struct CBufferedNote {
		unsigned char mCount;
		unsigned char mChannel;
		unsigned char mNote;
		unsigned char mVelocity;
	};

	/* .text+0x08093ff0. Channel 0. */
	CNoteTracer();

	/* .text+0x080940b0. */
	explicit CNoteTracer(unsigned char channel);

	/* .text+0x08094170. */
	CNoteTracer(const CNoteTracer &other);

	/* .text+0x08093050 (D1) / 0x080930b0 (D0). */
	virtual ~CNoteTracer();

	/* .text+0x08094290. */
	CNoteTracer &operator=(const CNoteTracer &other);

	/* .text+0x08183f60 (weak, real vtable slot 2). Base behavior: bump the
	 * already-tracked note's retrigger counter. See file header. */
	virtual void RendundantInsertion(CBufferedNote &existing, CBufferedNote incoming);

	/* .text+0x08095200. O(1) via mNoteIndex[]; dispatches to RendundantInsertion()
	 * (virtual) instead of inserting if the note is already tracked. */
	void Insert(CBufferedNote note);

	/* .text+0x080947d0. O(1) via mNoteIndex[]; no-op if not tracked. See file
	 * header for the mCount-pending-retrigger short-circuit. */
	void Remove(unsigned char note);

	/* .text+0x080946b0. Invalidates mNoteIndex[] from the current mNotes[] AND
	 * resets mSize to 0 -- see file header, NOT the same as ClearEntries(). */
	void ResetPendingNotes();

	/* .text+0x08096fb0. Invalidates every mNoteIndex[] entry from the current
	 * mNotes[] contents; does not touch mNotes[]/mSize itself. */
	void ClearEntries();

	/* .text+0x080970d0. Inverse of ClearEntries(): rebuilds mNoteIndex[] from the
	 * current mNotes[] contents. */
	void RefreshEntries();

	/* .text+0x08094860. Smallest mNote value present, -1 if empty. */
	signed char GetLeftMost() const;

	/* .text+0x08094980. Largest mNote value present, -1 if empty. */
	signed char GetRightMost() const;

	/* .text+0x08095330. Appends one Note-On event per active note. Returns the
	 * iterated note count (see file header). */
	int ListNotesOn(CLinkedEvent *&cursor) const;

	/* .text+0x080958d0. Same, with velocity += velocityDelta (clamped to
	 * [1,127] -- see file header for the exact clamp semantics). */
	int ListNotesOn(CLinkedEvent *&cursor, signed char velocityDelta) const;

	/* .text+0x08095eb0. Appends one Note-Off event per active note (no release
	 * velocity -- see file header tag format). */
	int ListNotesOff(CLinkedEvent *&cursor) const;

	/* .text+0x08096460. Appends one "Sound On" event (class-code 0xd) per
	 * active note, WITH velocity. */
	int ListSoundsOn(CLinkedEvent *&cursor) const;

	/* .text+0x08096a00. Appends one "Sound Off" event (class-code 0xc) per
	 * active note, no velocity. */
	int ListSoundsOff(CLinkedEvent *&cursor) const;

	/* .text+0x08094da0. STATIC: malloc(count*sizeof(CBufferedNote)) into buf.
	 * Returns true on success; on failure buf is left {0,0,0}. */
	static bool CreateBuffer(TDynBuffer<CBufferedNote> &buf, unsigned count);

	/* .text+0x08094e40. STATIC: realloc buf's storage to `count` elements.
	 * Returns true on success (buf.mBegin/mCapacity updated); on failure buf is
	 * left unchanged (real: realloc() semantics -- original block still valid). */
	static bool ReallocBuffer(TDynBuffer<CBufferedNote> &buf, unsigned count);

	/* .text+0x08094ed0. STATIC: frees buf's storage and zeroes buf. */
	static void DestroyBuffer(TDynBuffer<CBufferedNote> &buf);

	/* .text+0x08094f20. Non-static: swaps this->mNotes with `other` (lazily
	 * malloc(0x80)-initializing `other` first if it's still empty), rebuilding
	 * this->mNoteIndex[] around the swap. See file header. */
	void SwapBuffer(TDynBuffer<CBufferedNote> &other);

	/* .text+0x08093790. Real ground truth: ClearEntries() on both, swap the raw
	 * mNotes buffers, RefreshEntries() on both -- see file header. */
	friend void Swap(CNoteTracer &a, CNoteTracer &b);

private:
	unsigned char mNoteIndex[128]; /* +0x04 */
	TDynBuffer<CBufferedNote> mNotes; /* +0x84 */
	unsigned char mChannel; /* +0x90 */
};

#endif /* NOTE_TRACER_H */
