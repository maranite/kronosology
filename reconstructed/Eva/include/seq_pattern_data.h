/*
 * seq_pattern_data.h  -  CSeqEvent (minimal slice) / CSeqPat / CPatternDataHolder /
 * CDrumTrackPatternDataHolder -- a real, fully self-contained sequencer
 * pattern-event-storage family.
 *
 * Fresh nm -C class-inventory sweep (2026-07-28), looking for the next
 * CStorageConverterBase/CSpecialFuncCCMap/CRamSample/CPartitionData-shaped
 * mechanical cluster. Several promising-looking candidates were traced with
 * objdump call-xref scans and REJECTED before this one:
 *   - CFATEntry/CFatMap/CShortDirEntry/CVFATEntry (FAT/VFAT directory-entry
 *     family, 89 functions, contiguous .text): looked ideal at first (small,
 *     coherent, filesystem value-class shape identical to the already-landed
 *     CPartitionData/CMBR/CPBR batch) but real per-function call-xref tracing
 *     found several of its own members (CFATEntry::Serialize/Deserialize,
 *     CShortDirEntry::SetLongNameAndExt/FreeLongNameAndExt,
 *     CVFATEntry::AddCharToLongName/EndAddingCharToLongName/
 *     GenerateShortNameExt) call real `CZ` container growth/search primitives
 *     (`CZ::Insert`/`RFind`/`Remove`/`Sprintf`/growable `operator=(const
 *     char*)`) -- the exact ~55-method-deep, project-wide-out-of-scope `CZ`
 *     container surface cz_util.h already documents as deliberately NOT
 *     modeled (only the opaque-capacity ctor/dtor + 2 raw-offset peeks are
 *     real). `CDirEntry::Copy()` (needed by several ctors in this family)
 *     itself turned out to call the same growable `CZ::operator=(const
 *     char*)`, not a raw-field copy. This is exactly the "deep heterogeneous
 *     logic against an unmodeled class" trap this project's own methodology
 *     warns against -- rejected as a batch (its `CFatMap` sub-piece alone,
 *     17 methods with a real zero-dependency call surface, stays a valid
 *     future candidate but wasn't dense enough alone).
 *   - The `CLoadSoundFontMgr`/`CLoadKontaktBankMgr`/`CLoadKontaktMultiMgr`/
 *     `CLoadKontaktInstrumentMgr`/`CLoadKontaktSampleMgr` family (97
 *     functions): call-xref scan found a dense web of `CFMBrowseForm`/
 *     `CPCMManager`/`CDiskUtil`/`CDiskModeManager`/`CSmplMemManager`/
 *     `CFileOperation`/`CStorage` collaborators -- classic god-object-network
 *     business logic, not mechanical data-class shape. Rejected.
 *
 * The real find: `CSeqPat` (a fixed-layout 0x20-byte sequencer-pattern
 * metadata record) and `CPatternDataHolder` (the event-array container that
 * owns a run of `CSeqPat` records plus a shared `CSeqEvent[]` area), 27
 * functions total, living contiguously at .text 0x08e17c90..0x08e19210. A
 * full per-function objdump call-xref scan (script identical to the one used
 * to reject the two candidates above) found **zero** external call targets
 * anywhere in either class -- not even `CZ`, `HAL_*`, or libc. Every method
 * is pure raw-offset field access, fixed-width space-padded name-field
 * copying (GCC-unrolled `movdqa`/byte-store loops -- reproduced here as plain
 * C loops, not literal unrolled transcriptions, matching this project's usual
 * "reproduce ground-truth behavior, not compiler artifacts" convention), or
 * linear scans of an in-object event array for a sentinel byte. This is a
 * strictly BETTER match for "mechanical, not entangled with unmodeled
 * classes" than the rejected FAT family, despite CSeqPat/CPatternDataHolder
 * having real (if simple) algorithmic content of their own.
 *
 * `CDrumTrackPatternDataHolder::Initialize()` (1 function, .text+0x08e18590,
 * immediately adjacent in the same .text run) is a trivial subclass that
 * just writes 4 fixed-literal dwords into the SAME 4 fields
 * (mNumPatterns/mNumEventSlots/mPatternAreaOffset/mEventAreaOffset,
 * confirmed by matching offsets +0x8/+0xc/+0x10/+0x14) -- included here as a
 * real `public CPatternDataHolder` subclass rather than skipped, since it's
 * free given the fields are already being modeled.
 *
 * === CSeqPat real layout (0x20 bytes, confirmed via CDirEntry-style direct
 * literal-immediate transcription in Initialize()/SetName()) ===
 *   +0x00  char mName[0x18]      (24 bytes, fixed-width, space-padded)
 *   +0x18  unsigned char  = 1    (const, written by both Initialize overloads)
 *   +0x19  unsigned char  = 0x13 (const)
 *   +0x1a  unsigned short = 0    (const)
 *   +0x1c  SEventOffset mEventOffset (4 bytes -- see below)
 *
 * `SEventOffset` (mEventOffset, +0x1c..+0x1f) is treated by ground truth as a
 * single packed 32-bit value everywhere it's read or written as a whole
 * (`GetEventOffset()`/`SetEventOffset(unsigned long)` -- confirmed via
 * matching byte-for-byte memcpy-style read/write in both -- and the
 * `== 0xFFFFFFFF` sentinel test in both `GetEvent()` overloads), even though
 * `Initialize()` writes byte +0x1c on its own (offset field, `0xff`) and the
 * other 3 bytes come from a local dword temp the compiler happens to
 * initialize to `0xffffffff` right before slicing it into 3 bytes -- i.e.
 * BOTH `Initialize()` overloads set the WHOLE packed value to `0xFFFFFFFF`
 * (confirmed by tracing the exact stack-slot reuse in the disassembly, not
 * garbage/uninitialized-stack reads as first appeared). "Invalid/unset
 * pattern" is represented by `mEventOffset == 0xFFFFFFFF`.
 *
 * === CPatternDataHolder real layout (fields confirmed by the 6 trivial
 * one-line accessors GetEventAreaTop/GetEventAreaEnd/GetFreeEventTop/
 * GetPatternTop/GetPatternEventTop plus SetInfo/ClearUnusedArea/GetPat) ===
 *   +0x00  int mUsedPatternCount   (SetInfo() output: count of patterns whose
 *                                   GetEvent(patIndex) != null)
 *   +0x04  int mTotalEventCount    (SetInfo() output: total event count
 *                                   across all patterns; doubles as the
 *                                   "free area" event-slot index)
 *   +0x08  int mNumPatterns        (capacity of the CSeqPat[] array)
 *   +0x0c  int mNumEventSlots      (capacity of the CSeqEvent[] area, in
 *                                   8-byte slots)
 *   +0x10  int mPatternAreaOffset  (byte offset from `this` to the embedded
 *                                   CSeqPat[] array -- real object embeds
 *                                   both arrays inline, not via pointers)
 *   +0x14  int mEventAreaOffset    (byte offset from `this` to the embedded
 *                                   CSeqEvent[] area)
 * `CSeqPat` entries are found at `(char*)this + mPatternAreaOffset +
 * index*sizeof(CSeqPat)` (confirmed stride 0x20 in GetPat()); `CSeqEvent`
 * entries at `(char*)this + mEventAreaOffset + index*sizeof(CSeqEvent)`
 * (confirmed stride 8 in GetEventDirect()/ClearUnusedArea()/
 * GetEventAreaEnd()).
 *
 * `CSeqEvent` itself is a genuinely separate, much larger subsystem (used
 * throughout the real sequencer engine) -- only the 3 fields this batch's
 * own traced call graph actually touches are modeled here: `mType` (+0x0,
 * `3` = end-of-pattern sentinel every scan loop below stops on, `1` = a
 * "link/chain" event `CSeqPat::GetEvent(unsigned long, int)` follows via 2
 * big-endian-on-disk 16-bit fields) and the 2 link fields at +0x4/+0x6. Real
 * `sizeof(CSeqEvent)` is confirmed 8 bytes by every stride above. The
 * remaining bytes (+0x1..+0x3) are NOT decoded by this batch and are left as
 * plain padding.
 */

#ifndef SEQ_PATTERN_DATA_H
#define SEQ_PATTERN_DATA_H

#include <stdint.h>
#include <string.h>

/*
 * Minimal, honest slice of a much larger real class -- see header comment.
 * Real sizeof == 8 (confirmed by every *8 stride throughout this file).
 */
class CSeqEvent {
public:
	unsigned char mType;       /* +0x0 -- 3 = end-of-pattern sentinel, 1 = link event */
	unsigned char mUnknown1;   /* +0x1 .. +0x3 -- not decoded by this batch */
	unsigned char mUnknown2;
	unsigned char mUnknown3;
	unsigned char mLinkA[2];   /* +0x4 -- big-endian on-disk 16-bit field (raw bytes,
	                             * byte-swapped at the point of use -- see .cpp) */
	unsigned char mLinkB[2];   /* +0x6 -- same shape */
};

/*
 * One 0x20-byte sequencer-pattern metadata record -- see header comment for
 * full field layout. No constructor/destructor exist in ground truth (real
 * symbol table has none) -- it's a plain embedded-array POD, initialized
 * in place via Initialize().
 */
class CSeqPat {
public:
	/* .text+0x08e17c90, 62 bytes. Sets mEventOffset = 0xFFFFFFFF and the 4
	 * fixed metadata bytes; does NOT touch the name field.
	 */
	void Initialize();

	/* .text+0x08e17cd0, 918 bytes (GCC-unrolled space-pad loop). Same as
	 * Initialize() plus a fixed-width (24-byte) name copy -- see
	 * CopyPaddedName() below for the shared idiom.
	 */
	void Initialize(const char *name);

	/* .text+0x08e18070, 864 bytes. Fixed-width name copy only (metadata
	 * fields untouched).
	 */
	void SetName(const char *name);

	/* .text+0x08e183e0, 54 bytes. Fills the name field with 24 spaces. */
	void SetName();

	/* .text+0x08e18420/0x08e18450, 37/47 bytes. Plain read/write of the
	 * whole 4-byte mEventOffset field as one packed value -- see header
	 * comment.
	 */
	void SetEventOffset(unsigned long v);
	unsigned long GetEventOffset() const;

	/* .text+0x08e18480, 74 bytes. Real body (the 2nd param is used purely
	 * as an integer value in ground truth -- never dereferenced -- despite
	 * its real `CSeqEvent*` mangled type, transcribed as-is via a numeric
	 * cast at the point of use):
	 *   uintptr_t p = (uintptr_t)ptr;
	 *   mEventOffset = (p != 0 && p >= index) ? (uint32_t)(p - index)
	 *                                         : 0xFFFFFFFFu;
	 */
	void SetEvent(unsigned long index, CSeqEvent *ptr);

	/* .text+0x08e184d0, 61 bytes. Real body: return (mEventOffset ==
	 * 0xFFFFFFFF) ? 0 : (index + mEventOffset). NOTE: despite the real
	 * mangled/demangled signature `unsigned long`, this is used as a
	 * plain slot INDEX by every caller in this file, not a pointer.
	 */
	unsigned long GetEvent(unsigned long index) const;

	/* .text+0x08e18510, 110 bytes. Real body: if mEventOffset==0xFFFFFFFF
	 * or (eventAreaBase + mEventOffset) == 0, return 0. Otherwise walk a
	 * `CSeqEvent` "link chain" starting at (eventAreaBase +
	 * mEventOffset), following byte-swapped 16-bit link fields while
	 * mType==1, until either the link's high field matches `matchId` (the
	 * `int` parameter) or the chain hits a null step. Real semantics of
	 * the chain (loop/goto-style pattern events, plausibly) are not
	 * further decoded -- transcribed byte-exact.
	 */
	CSeqEvent *GetEvent(unsigned long eventAreaBase, int matchId) const;

private:
	char mName[0x18];              /* +0x00 */
	unsigned char mUnknown18;      /* +0x18 */
	unsigned char mUnknown19;      /* +0x19 */
	unsigned short mUnknown1a;     /* +0x1a */
	unsigned char mEventOffset[4]; /* +0x1c -- packed as one uint32_t, see above */

	friend struct CPatternDataHolderTestHooks;
};

/*
 * Owns an embedded CSeqPat[] array plus a shared CSeqEvent[] area -- see
 * header comment for full field layout.
 */
class CPatternDataHolder {
public:
	/* .text+0x08e185c0, 1 byte (bare `ret`) -- confirmed genuinely empty. */
	CPatternDataHolder();

	/* .text+0x08e185d0, 420 bytes. Real body: mUsedPatternCount = count of
	 * patterns (0..mNumPatterns) whose GetEvent(patIndex) != null;
	 * mTotalEventCount = sum, over the same patterns, of the number of
	 * CSeqEvent slots from that pattern's start event up to (but not
	 * including) the next mType==3 sentinel -- same scan idiom as
	 * GetNumOfEvent() below, run once per pattern and accumulated.
	 */
	void SetInfo();

	/* .text+0x08e18780, 665 bytes (GCC-unrolled double memset). Real body:
	 * zero-fills the CSeqEvent slots from GetFreeEventTop() through
	 * GetEventAreaEnd() inclusive (the unused tail of the event area).
	 */
	void ClearUnusedArea();

	/* .text+0x08e18a20, 25 bytes. Real body: return &CSeqPat[index], or
	 * null if index >= mNumPatterns.
	 */
	CSeqPat *GetPat(int index) const;

	/* .text+0x08e18a40, 56 bytes. Real body: bounds-check index against
	 * mNumPatterns, then tail-call CSeqPat::GetEvent(unsigned long) on
	 * that pattern, passing the holder's own event-area BASE POINTER
	 * (GetEventAreaTop()) as the generic "value to add mEventOffset to"
	 * -- CSeqPat::GetEvent(unsigned long) is used generically by this
	 * whole family for both plain-index and pointer arithmetic, see its
	 * own doc above.
	 */
	unsigned long GetEvent(int index) const;

	/* .text+0x08e18a80, 59 bytes. Same bounds-check/dispatch shape as the
	 * 1-arg overload, tail-calling CSeqPat::GetEvent(unsigned long, int)
	 * (the link-chain walk) with the holder's own event-area base
	 * pointer.
	 */
	CSeqEvent *GetEvent(int index, int matchId) const;

	/* .text+0x08e18ad0, 15 bytes. Real body: return
	 * &EventArea[eventIndex] -- no bounds check.
	 */
	CSeqEvent *GetEventDirect(int eventIndex) const;

	/* .text+0x08e18ae0, 57 bytes. Same bounds-check/dispatch shape as
	 * GetEvent(int) but tail-calls CSeqPat::SetEvent().
	 */
	void SetEvent(int index, CSeqEvent *ev);

	/* .text+0x08e18b20, 539 bytes. Real body: given a starting CSeqEvent*
	 * (or null) and a `bool` mode flag, scans forward counting slots. In
	 * "simple" mode (flag false): counts slots until mType==3. In the
	 * other mode (flag true): counts slots whose mType is 1 or 9 only
	 * (skipping others), still stopping at mType==3 -- transcribed
	 * byte-exact, real higher-level meaning of type 1/9 not decoded.
	 */
	int GetNumOfEvent(CSeqEvent *start, bool countOnlyType1Or9) const;

	/* .text+0x08e18d50, 307 bytes. Real body: same "count slots until
	 * mType==3" scan as the simple mode above, but starting from
	 * GetEvent(patIndex) (i.e. pattern-index entry point, not a raw
	 * CSeqEvent*).
	 */
	int GetNumOfEvent(int patIndex) const;

	/* .text+0x08e18e90, 343 bytes. Real body: sum of GetNumOfEvent(i) for
	 * i in [patIndex, mNumPatterns).
	 */
	int GetNumOfEventsToEnd(int patIndex) const;

	/* .text+0x08e18ff0, 343 bytes. Real body is the same loop shape as
	 * GetNumOfEventsToEnd() with a fixed start of pattern 0 -- ground
	 * truth duplicates the whole loop as a separate symbol (no real call
	 * to GetNumOfEventsToEnd(0) exists in the binary), but the two are
	 * behaviorally identical, so this reconstruction implements it as
	 * `return GetNumOfEventsToEnd(0);` rather than duplicating the loop.
	 */
	int GetTotalNumOfEvents() const;

	/* .text+0x08e19150, 86 bytes. Real body: scan patterns starting at
	 * patIndex+1 for the first one whose GetEvent() != null; return that
	 * pattern's resolved event index, or 0 if none found before
	 * mNumPatterns.
	 */
	unsigned long GetNextTopEvent(int patIndex) const;

	/* .text+0x08e191b0..0x08e19200, 8-17 bytes each. Plain raw-offset
	 * accessors -- see header comment for field meanings.
	 */
	CSeqEvent *GetEventAreaTop() const;
	CSeqEvent *GetEventAreaEnd() const;
	CSeqEvent *GetFreeEventTop() const;
	CSeqPat   *GetPatternTop() const;
	CSeqEvent *GetPatternEventTop() const;

protected:
	int mUsedPatternCount;  /* +0x00 */
	int mTotalEventCount;   /* +0x04 */
	int mNumPatterns;       /* +0x08 */
	int mNumEventSlots;     /* +0x0c */
	int mPatternAreaOffset; /* +0x10 */
	int mEventAreaOffset;   /* +0x14 */

	friend struct CPatternDataHolderTestHooks;
};

/*
 * Trivial subclass -- see header comment. .text+0x08e18590, 33 bytes.
 */
class CDrumTrackPatternDataHolder : public CPatternDataHolder {
public:
	void Initialize();
};

#endif /* SEQ_PATTERN_DATA_H */
