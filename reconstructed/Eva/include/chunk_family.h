/*
 * chunk_family.h  -  CChunkBase/CChunk/CChunkBlock/CChunkOrphan/CChunkInfoItem/
 * CChunkInfoList, Eva's hierarchical binary "chunk" (RIFF/IFF-style) file-format
 * primitive. Fresh `nm -C` class-inventory sweep (2026-07-28), following the
 * CKorgPath/CKorgLinuxPath/UKontaktOposPath batch (commit e9965d8).
 *
 * FOUND VIA: grouped Eva's pending manifest rows by class, sorted by count. The
 * "Chunk" family clustered together immediately once traced: `nm -C` shows
 * `CChunkBase`(21)/`CChunk`(39)/`CChunkBlock`(13)/`CChunkOrphan`(3)/
 * `CChunkInfoItem`(6)/`CChunkInfoList`(7) = 89 methods, ALL previously untouched
 * (manifest status "pending", 0 "reconstructed" in every one of the 6 classes
 * before this batch). Confirmed real ground-truth file names via `objdump -s -j
 * .rodata`: "Chunk.h" (.rodata+0x8e7fa81, inline-in-header asserts), "FileChunk.cpp"
 * (.rodata+0x8e7fba3, out-of-line-body asserts), "ChunkInfo.cpp" (.rodata+0x8e7faef).
 *
 * SIBLING SURVEY (found while tracing this cluster's own vtable, NOT pulled in --
 * deliberately deferred to a future batch, all confirmed via direct `.rodata`
 * string reads at the SAME address range as the strings above):
 *   CBackupChunk               (34 methods, "BackupChunk.cpp") -- CChunk-derived
 *     (vtable-diffed byte-for-byte against CChunk's own: identical at slots the
 *     comment below calls Init/Close/GetAbsSonNumber/CloseSubChunk/GetRelSonNumber
 *     (unmodified from CChunk), overridden at dtor/OnWriteLenAndFlags-equivalent/
 *     PreClose/PostClose/GetFather-adjacent slots, plus 3 NEW virtual slots CChunk
 *     doesn't have at all) -- adds index/backup-restore/CRC bookkeeping
 *     ("CBackupChunk CRC check: %d", "Restoring: pack=%d, bytes=%d", "Backupping:
 *     pack=%d, bytes=%d", all found in the same .rodata range).
 *   CChunkRootWithSeek         (16 methods, "ChunkRootWithSeek.cpp") -- adds a
 *     seekable sub-chunk index on top of CChunk.
 *   CChunkRootWithSeekWithCRC  (18 methods, "ChunkRootWithSeekWithCRC.cpp") --
 *     further derives from the above, adding CRC verification.
 *   CChunkRootBase             (present at .text+0x080b1740 immediately after this
 *     cluster's own address range; not surveyed at all this batch.)
 * These four form a natural, self-contained follow-up batch (real base is THIS
 * header's own `CChunk`, so a future session can pick up directly) -- NOT pulled in
 * here to keep this batch's own scope and time-per-method sane; a family this deep
 * ("index/seek/CRC on top of chunked I/O") is a genuinely separate feature, not
 * core chunked-I/O primitives.
 *
 * ARCHITECTURE (confirmed by direct disassembly, `objdump -dr -M intel`, of every
 * method in this header -- .text+0x0804d050..0x0804d0bf and .text+0x080ace90..
 * 0x080b2600):
 *
 *   This is a RIFF/IFF-style hierarchical binary chunk format. One shared physical
 *   byte stream (typed here as `CStream*`, Eva's own already-reconstructed
 *   stream_family.h -- CONFIRMED not invented: stream_family.h's own header
 *   comment already documents "`CChunkOrphan::CChunkOrphan()` construct[s] a
 *   `CMemory`" as a real, non-inlined, then-out-of-scope caller, which is
 *   PRECISELY this header's own `CChunkOrphan` ctor, closing that cross-reference)
 *   backs every `CChunkBase` object in a tree; each node tracks only its OWN
 *   region's baseline position and declared length within that ONE shared stream --
 *   `CChunkBase::LinkSubChunk()` propagates the SAME `mParent` stream pointer down
 *   to every descendant (`newChild->mParent = this->mParent`, NOT `= this`),
 *   confirmed from the real disassembly.
 *
 *   Each header on the wire (`SChkHeader`, 8 bytes) is [type][subtype][id][flags]
 *   [length, BIG-ENDIAN u32] -- confirmed byte-for-byte from `CChunkBase::
 *   WriteHeader()`'s own bit-twiddling (computes byte0(L)<<24|byte1(L)<<16|
 *   byte2(L)<<8|byte3(L) before a single 4-byte `Write()` call) and `ReadHeader()`'s
 *   own mirror-image byte unpacking (`out.length = buf[4]<<24|buf[5]<<16|
 *   buf[6]<<8|buf[7]`). This is the SAME big-endian convention already established
 *   project-wide for EXs auth's Blowfish (see /home/share/CLAUDE.md).
 *
 *   `SIdVRF` (4 bytes: type/subtype/id/flags, no length) is the SAME identity
 *   tuple as `SChkHeader`'s first 4 bytes, passed BY VALUE in a single register at
 *   every real call site (`AddSubChunk`'s own byte-unpacking of its 2nd argument
 *   confirms the field order/width). `flags` bits 1-2 (mask 0x6) select the
 *   concrete sub-chunk KIND when `AddSubChunk`/`GetNextSubChunk` construct a new
 *   node: 0 = plain `CChunk` (leaf value chunk), 2 = `CChunkBlock` (single-child
 *   container). `flags` bit 3 (0x8) is a permanent, construction-time "is a
 *   leaf/payload chunk" marker, NOT a dynamic open/closed runtime flag as first
 *   assumed -- re-derived after finding `CChunkBase::LinkSubChunk()` and
 *   `CChunkBlock`'s own ctor both require this SAME bit CLEAR (opposite polarity
 *   from every other check on it), which only makes sense as "must be a
 *   container, not a leaf, to receive children"/"a freshly-built container
 *   should never be constructed with the leaf bit set" respectively. `CChunk::
 *   Read/Write/Get/Put/Skip/operator>>/operator<<` all require it SET (only leaf
 *   chunks support raw byte I/O).
 *
 *   Status (`mStatus`, CChunkBase+0xc) is a 4-value state machine, confirmed from
 *   every read-vs-write gate in this cluster (Get/Read/Skip require status==0;
 *   Put/Write/operator<< require status==1; both classes of accessor return a
 *   soft-assert-and-fail result for status==3, and treat any OTHER unexpected
 *   value as a soft-assert-then-recover): 0=Read(open for reading), 1=Write(open
 *   for writing), 2=Closed(finalized), 3=Error(aborted).
 *
 *   `Api`+0x94 soft-assert calls throughout are the SAME project-wide "log-only,
 *   never enforcing" convention documented in partition_table.cpp/stream_family.cpp/
 *   etc -- reproduced as real calls (not dropped), matching real (file,line)
 *   evidence pairs, but NEVER short-circuit the surrounding control flow beyond
 *   what the real disassembly does (several call sites literally assert-then-
 *   continue down the SAME path that "should" have been guarded against, e.g.
 *   `CChunkBase::CChunkBase()`'s own invalid-flags branch re-reads and uses the
 *   SAME header data it just flagged as invalid -- transcribed as-is).
 *
 * `CChunkInfoItem`/`CChunkInfoList`: a small singly-linked-list "rank path"
 * bookkeeping mechanism. `CChunk::SetInfo(a,b,c,d,name)` builds a `CChunkInfoItem`
 * whose "data" buffer is sized to `this->mRelSonNestLev` (confirmed by direct
 * register tracing at the SetInfo/ctor call boundary -- NOT the more obvious-
 * looking "c" 3rd SetInfo argument as first read) and hands it to the virtual
 * `OnSetInfo()`, which walks UP the `GetFather()` chain writing each ancestor's own
 * `mRankNumber` into one slot of that buffer via `CChunkInfoItem::SetRankNum()`
 * (a decrementing countdown filling the buffer back-to-front) -- i.e. building a
 * root-to-leaf path of rank numbers as the call bubbles upward. `Serialize()`/
 * `DeSerialize()` write/read that item to/from a `CChunk` via `Put()`/`Get()`/
 * `Write()` in a fixed field order (byte0..byte6 individually, data buffer,
 * NUL-terminated name last) -- `mTotalLen` (byte0) is a SELF-EXCLUSIVE length
 * (`6 + dataSize + nameLen`, confirmed: 7 total header/meta bytes are actually
 * individually Put() including byte0 itself, so real bytes-on-wire =
 * mTotalLen + 1).
 *
 * FIELDS BEYOND THE CORE STATE MACHINE whose exact real-world MEANING (not byte
 * layout, which is disassembly-confirmed) is not independently corroborated:
 * `SetInfo()`'s own `a`/`b`/`c`/`d` u8 parameters (stored as `CChunkInfoItem`'s
 * own +0x3..+0x6 fields, passed through opaquely by every caller traced so far);
 * the `type==4`/`(subtype=0,id=0,flags=0x10)` wildcard sentinel `OnSetInfo()`/
 * `CChunkBlock::Init()` both check against (an "any/self-identity" marker, exact
 * semantic purpose unconfirmed); `CChunkBlock`'s own two DISTINCT sub-chunk-kind
 * sentinels (type=4/flags=0x10, added unconditionally by `Init()` on write-open,
 * vs type=3/flags=0x18, added by `PreClose()` specifically to hold the serialized
 * `CChunkInfoList` -- confirmed to be two DIFFERENT sub-chunks via the literal
 * `SIdVRF` constants `0x10000004`/`0x18000003` each call site passes, not a
 * transcription slip).
 *
 * `CZ` interop (`CChunk::operator<<(CZ const&)`/`operator>>(CZ&)`): `CZ` itself is
 * an intentionally opaque, out-of-scope 247-real-method container project-wide
 * (cz_util.h's own header comment). `operator<<` is fully traced (loops
 * `Put()`-ing `RawFlagField()` bytes starting at `RawPtrField()`, a Duff's-device-
 * unrolled real body collapsed to a plain loop here -- same license as
 * `CParameterString`'s own ALPHAKEYBOARD compare). `operator>>` is a best-effort
 * SYMMETRIC read (reads `RawFlagField()` bytes back into the SAME already-owned
 * buffer at `RawPtrField()`) since `CZ`'s own opaque model here exposes no growable
 * mutator (`operator+=(char)` exists in ground truth, `0818c260`, but is NOT part
 * of cz_util.h's current opaque surface) -- NOT independently confirmed against
 * `CChunk::operator>>(CZ&)`'s own real disassembly the way every other method in
 * this header is.
 */

#ifndef CHUNK_FAMILY_H
#define CHUNK_FAMILY_H

#include "stream_family.h"
#include "cz_util.h"

/* Wire header, 8 bytes: [type][subtype][id][flags][length, BIG-ENDIAN u32]. */
struct SChkHeader {
	unsigned char type;
	unsigned char subtype;
	unsigned char id;
	unsigned char flags;
	unsigned long length;
};

/* Sub-chunk identity tuple, 4 bytes -- same field order as SChkHeader's first 4
 * bytes, passed by value (packed into one register at every real call site).
 * flags bits 1-2 (0x6) select the concrete kind AddSubChunk()/GetNextSubChunk()
 * construct: 0 = CChunk, 2 = CChunkBlock. flags bit 3 (0x8) = "open for I/O".
 */
struct SIdVRF {
	unsigned char type;
	unsigned char subtype;
	unsigned char id;
	unsigned char flags;
};

class CChunk;
class CChunkInfoItem;

/* CChunkBase -- abstract root of the chunk-tree node hierarchy. */
class CChunkBase {
public:
	enum EStatus { eRead = 0, eWrite = 1, eClosed = 2, eError = 3 };

	virtual ~CChunkBase();

	/* .text+0x080acf60, real body: snapshot mParent's current position into
	 * mBasePos via a virtual "position" call (a SEPARATE 0-arg virtual slot from
	 * Read/Write on the real ground-truth `CStream`-family object -- modeled
	 * here as `CStream::Tell()`, already exposed by stream_family.h), then
	 * return (mParent's own "error" field == 0).
	 */
	virtual bool Init();

	/* .text+0x080acfd0, real body: unconditional `return true`. Overridden by
	 * CChunk/CChunkBlock below with real behavior.
	 */
	virtual bool PreClose() { return true; }

	/* .text+0x080acfe0, real body: mStatus = eClosed; return true. */
	virtual bool PostClose();

	/* .text+0x080ad1d0. Real body: soft-assert if mOpenChild is non-NULL
	 * (unclosed child at Close() time -- FileChunk.cpp line 0x6f=111). Reads
	 * mParent->Tell(); if that position is somehow < mBasePos, soft-asserts
	 * (FileChunk.cpp 0x72=114) and retries the read (a real, disassembly-
	 * confirmed dead-code landmine -- nothing between the two reads can change
	 * mParent's position, so this branch cannot terminate if ever actually
	 * entered; no real caller does). Otherwise, on mStatus==eWrite, calls
	 * `this->OnWriteLenAndFlags(mBasePos-5, pos-mBasePos, mDeclaredLen,
	 * mFlags)` through THIS OBJECT'S OWN vtable slot 0x20 (self-dispatch, NOT
	 * a call on mParent -- confirmed from the real disassembly's own `mov
	 * (%ebx),%edx; call *0x20(%edx)`) and, only if that call's own result is
	 * non-zero, updates mDeclaredLen to the actual bytes written
	 * (pos-mBasePos). For mStatus==eRead, instead seeks mParent forward to
	 * mBasePos+mDeclaredLen (skip any unread trailing payload) via mParent's
	 * own Seek()-shaped virtual slot. Returns (mParent's own HasIoError()==
	 * false) after settling mStatus.
	 *
	 * GROUND-TRUTH LIMITATION (confirmed, not a reconstruction gap): every
	 * AddSubChunk() unconditionally writes a fresh sub-chunk header with
	 * length=0 (hardcoded, not derived from the caller's SIdVRF at all).
	 * `LinkSubChunk()` DOES automatically wire up `mFather` (`sub->
	 * SetFather(this)`, real ground truth, re-verified) -- but the ONLY
	 * mechanism that could then patch a sub-chunk's real final length back
	 * into its already-written wire header is this OnWriteLenAndFlags chain,
	 * whose base `CChunk::OnWriteLenAndFlags()` (below) does nothing but
	 * forward the call to mFather. Since NO class in this 89-method batch
	 * (only the deferred CBackupChunk/CChunkRootWithSeek layer, presumably)
	 * ever overrides `OnWriteLenAndFlags()` to do real work, the forwarding
	 * chain always bottoms out at a true root chunk's own NULL mFather --
	 * i.e. the length patch-back genuinely cannot complete within this
	 * batch's own scope, REGARDLESS of mFather being correctly wired.
	 * Round-trip correctness for GetNextSubChunk()'s own length-based
	 * boundary detection therefore requires either (a) a top-level chunk
	 * constructed with its real final length known upfront (no patch-back
	 * needed), or (b) reading sub-chunk payloads by absolute byte position
	 * rather than relying on the (always-0) declared length.
	 */
	virtual bool Close();

	/* .text+0x080ad330. Real body: if mOpenChild, forcibly walk it through
	 * PreClose()/Close()/PostClose() (mOpenChild's own status is stamped
	 * eError=3 first, matching a forced/abnormal shutdown), then clear
	 * mOpenChild. Soft-asserts (Chunk.h 0x109/0x10f/0x115) if any of the 3
	 * calls found mOpenChild unexpectedly NULL mid-sequence.
	 */
	virtual void OnChildDestroy();

	/* .text+0x080acf20, real body: soft-assert if mOpenChild non-NULL (Chunk.h
	 * 0x3a=58), return false. Overridden by CChunkBlock with real behavior.
	 */
	virtual bool GetAllInfo();

	/* .text+0x080ae710. Real body: validates mFlags bit 0x8 (open) and mStatus,
	 * checks the incoming sub's own identity flags for the reserved bits 0x6==6
	 * or ==4 fast path vs the (subtype=0,id=0,flags=0x10) wildcard, calls the
	 * sub's own Init() and (on success) GetAllInfo(), sets sub->mBasePos ==
	 * this->mBasePos's own read-byte flag on sub (mirrored 1-byte flag, real
	 * purpose unconfirmed beyond "propagate mParent's own byte" -- see .cpp),
	 * sets sub->mParent = this->mParent (the SAME physical stream, not `this`),
	 * asserts sub doesn't already have an mOpenChild set (Chunk.h 0x17f=383),
	 * then sub->mAbsSonNumber = this->mStatus (real ground truth: literally
	 * copies the CONTAINER's status word into the new child's abs-son-number
	 * field -- transcribed as-is, not "corrected"). Increments
	 * this->mAbsSonNumber (used as a running "how many children linked" counter
	 * -- despite ALSO being the destination of the odd copy above on the CHILD
	 * side; these are the SAME field name/offset on different objects, not a
	 * self-contradiction). Returns true.
	 */
	virtual bool LinkSubChunk(CChunk *&sub);

	/* .text+0x080b0d60 (CChunkBase's own generic version -- CChunkBlock below
	 * overrides with single-child pass-through semantics). Real body:
	 * allocates a new CChunk or CChunkBlock (per `id.flags & 0x6`) with header
	 * {id.type, id.subtype, id.id, id.flags, length=0}, LinkSubChunk()s it
	 * under `this`, and (only if this->mStatus==eWrite) WriteHeader()s its
	 * fresh 0-length header out to the stream, THEN calls `sub->Init()`
	 * (vtable offset 0x30 -- snapshots mBasePos right after the header, so
	 * later Read/Write/Get/Put on `sub` clamp against the correct baseline).
	 * On either WriteHeader() or Init() failure, `sub` is deleted and cleared.
	 * Re-verified 2026-07-28: an earlier draft omitted the Init() call.
	 */
	virtual bool AddSubChunk(CChunk *&sub, SIdVRF id);

	/* .text+0x080b1000 (CChunkBase's own generic version). Real body:
	 * (mStatus must be eRead) checks mParent position vs mBasePos+mDeclaredLen
	 * for "no more bytes" (<=7 remaining -> no next sub-chunk, return false),
	 * ReadHeader()s the next 8-byte header in place, builds a CChunk or
	 * CChunkBlock per the header's own kind bits, LinkSubChunk()s it, and (on
	 * success) calls `sub->Init()` (vtable offset 0x30, NOT GetAllInfo() as an
	 * earlier draft had it) -- GetNextSubChunk()'s own return value IS Init()'s
	 * result; on failure `sub` is deleted and cleared. Re-verified 2026-07-28.
	 */
	virtual bool GetNextSubChunk(CChunk *&sub);

	/* .text+0x080ad000. Real body: validates `sub` matches this->mOpenChild
	 * (Chunk.h 0x55/0x56 soft-asserts otherwise), walks it through PreClose()/
	 * PostClose() (stamping mStatus=eError first if PreClose's own predicate
	 * came back false-ish), then Close()s it and clears mOpenChild. Returns
	 * the AND of the PreClose-derived flag and Close()'s own result.
	 */
	virtual bool CloseSubChunk(CChunk *&sub);

	/* .text+0x080ad790 (CChunk's own override; CChunkBase itself has no real
	 * body traced independently -- CChunkBlock overrides separately below).
	 * Real return value confirmed non-void: `CChunk::SetInfo()`'s own inlined
	 * copy of this same dispatch checks the father-call's result
	 * (`test eax,eax`), and `CChunkBlock::OnSetInfo()` is a genuine tail call
	 * into `CChunkInfoList::Add()`, which itself returns `bool` ("did I take
	 * ownership of this item", false for a rejected duplicate) -- so this is
	 * an ownership-transfer flag propagated up the whole ancestor chain, not
	 * a void notification. Declared pure here; CChunk provides the real,
	 * disassembly-confirmed default.
	 */
	virtual bool OnSetInfo(CChunkInfoItem *item) = 0;

	/* Declared here (not in CChunk) purely so `CChunk::mFather`, typed
	 * `CChunkBase*` (the tree father may be any concrete node in principle),
	 * can dispatch through it -- the real slot only exists on CChunk's own
	 * expanded vtable in ground truth (CChunkBase itself is never a real
	 * father in practice, only CChunk/CChunkBlock/CChunkOrphan instances
	 * are), but C++ needs a consistent static type to call through.  Base
	 * default: soft-assert (there is no real base-level body to call) and
	 * return false. CChunk overrides with the real forward-to-mFather chain.
	 */
	virtual bool OnWriteLenAndFlags(unsigned long a, unsigned long b,
	                                 unsigned long c, unsigned char flags)
	{
		(void)a; (void)b; (void)c; (void)flags;
		ApiAssertChunkH(0xb4);
		return false;
	}

	/* .text+0x080ad070 (CChunk's own thunk into this SAME virtual slot,
	 * defaulting to GetAbsSonNumber() unless a derived class -- CBackupChunk,
	 * out of scope -- overrides it). Declared as an ordinary virtual with the
	 * base's own real default body.
	 */
	virtual unsigned int GetRelSonNumber() const { return GetAbsSonNumber(); }

	/* .text+0x0804d060, real body: return mAbsSonNumber. */
	virtual unsigned int GetAbsSonNumber() const { return mAbsSonNumber; }

	/* .text+0x0804d050, real body: return mRelSonNestLev + 1. */
	unsigned int GetRelSonNestLev() const { return (unsigned int)mRelSonNestLev + 1; }

	/* .text+0x080aea20. Real body: reads mParent's own status-ish field
	 * (through the SAME this-adjusted secondary vtable view used by the
	 * position query) and maps it onto mStatus: parent==eWrite -> self=eRead;
	 * parent==eClosed or eError -> self=eWrite; anything else -> soft-assert
	 * (Chunk.h 0x1a6=422), self=eError. Returns the new mStatus. Real deep
	 * "why" of this specific remap not independently confirmed -- transcribed
	 * exactly as the real branch structure, not reinterpreted.
	 */
	virtual int SetStatus();

	int GetStatus() const { return mStatus; }

	/* NOT ground truth -- a bootstrap hook. No method in this 89-method batch
	 * ever transitions a chunk from the ctor's own default mStatus==eClosed
	 * to eRead/eWrite for a ROOT-level node (LinkSubChunk() propagates
	 * mStatus from an EXISTING container to a new child, but nothing sets it
	 * for the container itself) -- confirmed real ground-truth gap, not an
	 * oversight: that responsibility belongs to the deferred CChunkRootWithSeek/
	 * CBackupChunk layer's own (out-of-scope) root-open logic. Exposed here so
	 * a root chunk can be exercised at all within this batch's own scope
	 * (used by verify/test_chunk_family.cpp and by any real future caller
	 * that plays the role of that deferred layer).
	 */
	void SetRootStatus(int status) { mStatus = status; }

	/* NOT ground truth -- companion bootstrap hook for setting mParent (the
	 * shared physical stream) on a freshly-constructed root chunk, mirroring
	 * what LinkSubChunk() does for a child but with no container to inherit
	 * it from. Real CChunkOrphan sets its OWN mParent to a private CMemory it
	 * allocates itself (see CChunkOrphan's ctor below); a plain root CChunk
	 * backed by an EXTERNAL CStream (as opposed to CChunkOrphan's PRIVATE one)
	 * has no in-scope ground-truth ctor path at all.
	 */
	void SetRootParent(CStream *parent) { mParent = parent; }

	/* Public accessor for the position-patch-back a real caller in the
	 * deferred CChunkRootWithSeek/CBackupChunk layer would need to perform
	 * (see Close()'s own header comment on the ground-truth sub-chunk-length
	 * finalization gap) -- the header's own length field sits 4 bytes before
	 * mBasePos on the wire (SChkHeader is [type][subtype][id][flags][length],
	 * mBasePos is snapshotted by Init() AFTER the 8-byte header has already
	 * been read/written).
	 */
	unsigned long GetBasePos() const { return mBasePos; }

	/* Not an independently-traced ground-truth method -- a small helper
	 * exposing the SAME "bytes left in this chunk's own declared region"
	 * computation `GetNextSubChunk()`/`ReadBinary()` already do internally
	 * (`mBasePos + mDeclaredLen - mParent->Tell()`), so `CChunkInfoList::
	 * DeSerialize()` (chunk_family.cpp) can use it as its own loop-termination
	 * predicate without needing that computation duplicated a third time.
	 */
	unsigned long GetRemainingBytes() const
	{
		if (!mParent)
			return 0;
		unsigned long pos = (unsigned long)mParent->Tell();
		unsigned long end = mBasePos + mDeclaredLen;
		return (pos < end) ? (end - pos) : 0;
	}

	unsigned char GetType() const { return mType; }
	unsigned char GetSubtype() const { return mSubtype; }
	unsigned char GetId() const { return mId; }
	unsigned char GetHdrFlags() const { return mFlags; }

	/* Not an independently-traced ground-truth method -- consolidates the
	 * SAME "type==X, or the (subtype=0,id=0,flags=wildcardFlags) wildcard"
	 * identity check that recurs, byte-for-byte identical in shape, at 4 real
	 * call sites in this cluster (`LinkSubChunk()`, `CChunk::OnSetInfo()`,
	 * `CChunkBlock::Init()`, `CChunkBlock::GetAllInfo()` -- see chunk_family.cpp),
	 * each with its own literal `type`/`wildcardFlags` constants. Avoids 4-way
	 * duplication of the exact same comparison shape.
	 */
	bool MatchesTypeOrWildcard(unsigned char type, unsigned char wildcardFlags) const
	{
		if (mType == type)
			return true;
		return mSubtype == 0 && mId == 0 && mFlags == wildcardFlags;
	}

protected:
	CChunkBase(const SChkHeader &hdr);

	/* .text+0x080ae100. Real body: single 8-byte Read() through mParent into a
	 * local buffer, then unpacks: out.type/subtype/id/flags = buf[0..3]
	 * (identity order), out.length = buf[4]<<24|buf[5]<<16|buf[6]<<8|buf[7]
	 * (BIG-ENDIAN). Validates mParent's own "bytes actually read" count == 8
	 * and its own "error" field == 0 (Chunk.h 0xbb=187 soft-assert otherwise,
	 * self mStatus=eError, return false), then validates the just-read flags
	 * byte's reserved bits (same 0x41/sign-bit check as the ctor) before
	 * returning true.
	 */
	bool ReadHeader(SChkHeader &out);

	/* .text+0x080ae220. Real body: validates hdr.flags reserved bits (FileChunk
	 * 0xbb soft-assert path, self mStatus=eError, return false on failure),
	 * then writes type/subtype/id as 3 individual 1-byte Write() calls through
	 * mParent, followed by ONE 4-byte Write() of hdr.length re-packed
	 * BIG-ENDIAN (byte0(L)<<24 | byte1(L)<<16 | byte2(L)<<8 | byte3(L), as a
	 * native little-endian u32 so the memory bytes come out MSB-first).
	 * Returns true only if mParent's own running byte-count reaches exactly 8
	 * AND its own "error" field is still 0.
	 */
	bool WriteHeader(const SChkHeader &hdr);

	/* .text+0x080ae500. Real body: (mStatus must be eRead or already
	 * closed/erroring, i.e. NOT eWrite) clamps `n` to the bytes actually
	 * remaining in this chunk's own declared region (mBasePos+mDeclaredLen -
	 * mParent->Tell()), Read()s that many bytes through mParent, and marks
	 * mStatus=eError if mParent's own "error" field comes back set after the
	 * read.
	 */
	unsigned int ReadBinary(void *buf, unsigned int n);

	/* .text+0x080ae650. Real body: (mStatus must be eWrite) Write()s the full
	 * `n` bytes through mParent, then marks mStatus=eError if mParent's own
	 * "error" field is set AND the actual bytes-written count didn't equal the
	 * requested `n` (a short write).
	 */
	void WriteBinary(const void *buf, unsigned int n);

	static void ApiAssertChunkH(int line);
	static void ApiAssertFileChunkCpp(int line);

	unsigned long mBasePos;       /* +0x04: mParent->Tell() snapshot at Init() */
	unsigned char mRelSonNestLev; /* +0x08 */
	int mStatus;                  /* +0x0c: EStatus */
	unsigned char mType;          /* +0x10 */
	unsigned char mSubtype;       /* +0x11 */
	unsigned char mId;            /* +0x12 */
	unsigned char mFlags;         /* +0x13 */
	unsigned long mDeclaredLen;   /* +0x14: SChkHeader.length */
	CStream *mParent;             /* +0x18: shared physical stream, propagated
	                                  to every descendant unchanged */
	CChunkBase *mOpenChild;       /* +0x1c: the ONE currently-linked/active
	                                  sub-chunk under this node, or NULL */
	unsigned int mAbsSonNumber;   /* +0x20 */

private:
	CChunkBase(const CChunkBase &);
	CChunkBase &operator=(const CChunkBase &);
};

/* CChunkInfoItem -- one "rank path" record: a name string, a small opaque
 * (a,b,c,d) tuple, and a data buffer sized to the originating chunk's own
 * mRelSonNestLev, filled back-to-front by SetRankNum() as OnSetInfo() bubbles up
 * the tree. Singly-linked (mNext) by CChunkInfoList below.
 */
class CChunkInfoItem {
public:
	/* .text+0x080b1cf0. See header comment for the real param->field mapping
	 * (pathDepth is NOT SetInfo's own 3rd argument, despite the natural
	 * reading -- it's the ORIGINATING CHUNK's mRelSonNestLev, threaded through
	 * as this ctor's own first real parameter by CChunk::SetInfo()).
	 */
	CChunkInfoItem(unsigned char pathDepth, unsigned char a, unsigned char b,
	               unsigned char c, unsigned char d, const char *name);
	/* .text+0x080b1de0. Real body: empty name (1-byte ""), zero path depth
	 * (mData left NULL, mPathRemaining=0), mTotalLen=7 (6 + 0 + 1).
	 */
	CChunkInfoItem();
	~CChunkInfoItem();

	/* .text+0x080b1e90. Real body: if mPathRemaining>0, decrement it and write
	 * `r` at mData[mPathRemaining] (post-decrement index -- fills back-to-
	 * front across repeated calls). Returns true if a write happened.
	 */
	bool SetRankNum(unsigned char r);

	void Serialize(CChunk *out);
	void DeSerialize(CChunk *in);

	CChunkInfoItem *GetNext() const { return mNext; }
	void SetNext(CChunkInfoItem *n) { mNext = n; }
	/* Real ground truth: CChunkInfoList::Add()'s own dedup scan reads THIS
	 * field (item's own byte offset +0x2, i.e. mPathDepth by this header's
	 * field map) as its comparison key -- not a more "sensible"-looking
	 * identity field. Transcribed exactly as disassembled, not reinterpreted.
	 */
	unsigned char GetDedupKey() const { return mPathDepth; }
	const char *GetName() const { return mName; }

private:
	CChunkInfoItem(const CChunkInfoItem &);
	CChunkInfoItem &operator=(const CChunkInfoItem &);

	unsigned char mTotalLen;   /* +0x00: 6 + mPathDepth + (strlen(name)+1) */
	unsigned char mReserved1;  /* +0x01: always 0 */
	unsigned char mPathDepth;  /* +0x02: also mData's own allocated size */
	unsigned char mA;          /* +0x03 */
	unsigned char mB;          /* +0x04 */
	unsigned char mC;          /* +0x05 */
	unsigned char mD;          /* +0x06 */
	char *mName;               /* +0x08: heap, NUL-terminated */
	unsigned char *mData;      /* +0x0c: heap, mPathDepth bytes, NULL if 0 */
	CChunkInfoItem *mNext;     /* +0x10 */
	unsigned char mPathRemaining; /* +0x14: countdown, init = mPathDepth */
};

/* CChunkInfoList -- singly-linked list of CChunkInfoItem, head pointer only. */
class CChunkInfoList {
public:
	CChunkInfoList() : mHead(0) {}
	~CChunkInfoList();

	void DestroyAllItem();

	/* .text+0x080b2290. Walks the list calling item->Serialize(out) on every
	 * node (Duff's-device-unrolled real body, collapsed to a plain loop).
	 */
	void Serialize(CChunk *out);

	/* .text+0x080b2350. Real body: soft-asserts (ChunkInfo.cpp 0xad=173) if
	 * mHead is already non-NULL (list must be empty), then repeatedly
	 * DeSerialize()s a fresh CChunkInfoItem and Add()s it until `in`'s own
	 * remaining-bytes predicate says stop.
	 */
	void DeSerialize(CChunk *in);

	/* .text+0x080b24c0. Real body: dedup-scans the list by GetDedupKey() (item's
	 * own +0x2 field, i.e. mPathDepth by this header's own field map -- NOT a
	 * more "sensible"-looking identity field; transcribed exactly as
	 * disassembled) AND by full NUL-terminated name compare; appends at the
	 * tail if no duplicate is found. Returns false (and does NOT append) if a
	 * duplicate matched.
	 */
	bool Add(CChunkInfoItem *item);

	/* .text+0x080b2530. cur==NULL means "start of list" (returns mHead);
	 * otherwise returns cur->GetNext().
	 */
	CChunkInfoItem *GetNext(const CChunkInfoItem *cur) const;

private:
	CChunkInfoList(const CChunkInfoList &);
	CChunkInfoList &operator=(const CChunkInfoList &);

	CChunkInfoItem *mHead;
};

/* CChunk -- concrete leaf/value chunk: raw byte payload plus typed Get/Put and
 * operator>>/operator<< helpers, all funneling through CChunkBase::ReadBinary()/
 * WriteBinary(). Multi-byte values are BIG-ENDIAN on the wire (same convention as
 * SChkHeader.length).
 */
class CChunk : public CChunkBase {
public:
	CChunk(const SChkHeader &hdr);
	virtual ~CChunk();

	virtual bool Close();
	/* .text+0x080ad440. Real body: if mFather is set, forward the call
	 * (unmodified args) to `mFather->OnWriteLenAndFlags(a,b,c,flags)` (virtual,
	 * own vtable slot 0x20 -- a derived class, e.g. the deferred CBackupChunk,
	 * could override this to actually patch a wire header's length field in
	 * place). If mFather is NULL, the real body soft-asserts (Chunk.h
	 * 0xb4=180) and retries the SAME check forever -- a real, disassembly-
	 * confirmed dead-code landmine (nothing changes mFather between the assert
	 * and the retry). Bounded here to assert-once-then-return-false instead of
	 * looping: `LinkSubChunk()` DOES wire up mFather for every real sub-chunk
	 * (`sub->SetFather(this)`), so this path is only genuinely reachable at a
	 * true ROOT chunk (mFather never set for the top of a tree) -- an actual
	 * infinite loop there would still hang any real caller, so the bound is
	 * kept as a deliberate, disclosed safety measure, not a behavior change
	 * for any deeper, REACHABLE real path.
	 */
	virtual bool OnWriteLenAndFlags(unsigned long a, unsigned long b,
	                                 unsigned long c, unsigned char flags);
	virtual bool PreClose() { return true; }
	virtual bool PostClose();

	virtual bool OnSetInfo(CChunkInfoItem *item);

	CChunkBase *GetFather() const { return mFather; }
	void SetFather(CChunkBase *f) { mFather = f; }
	void SetRankNumber(unsigned int r) { mRankNumber = r; }

	/* .text+0x080aec10. Advances mParent's read position by up to `n` bytes
	 * (clamped to this chunk's own remaining declared length), same open/
	 * status gating as Read().
	 */
	unsigned int Skip(unsigned int n);

	void Read(void *buf, unsigned int n);
	void Write(const void *buf, unsigned int n);
	bool Get(unsigned char &b);
	bool Put(unsigned char b);

	/* .text+0x080af060. Builds a CChunkInfoItem sized to this->mRelSonNestLev
	 * (see header comment) carrying (a,b,c,d,name) and dispatches it through
	 * OnSetInfo(). Real ground truth: `name` is `char*` (non-const) though
	 * never mutated.
	 */
	void SetInfo(unsigned char a, unsigned char b, unsigned char c,
	             unsigned char d, char *name);

	CChunk &operator>>(unsigned char &v);
	CChunk &operator>>(char &v);
	CChunk &operator>>(signed char &v);
	CChunk &operator>>(unsigned short &v);
	CChunk &operator>>(short &v);
	CChunk &operator>>(unsigned int &v);
	CChunk &operator>>(unsigned long &v);
	CChunk &operator>>(int &v);
	CChunk &operator>>(long &v);
	CChunk &operator>>(CZ &z);

	CChunk &operator<<(unsigned char v);
	CChunk &operator<<(char v);
	CChunk &operator<<(signed char v);
	CChunk &operator<<(unsigned short v);
	CChunk &operator<<(short v);
	CChunk &operator<<(unsigned int v);
	CChunk &operator<<(unsigned long v);
	CChunk &operator<<(int v);
	CChunk &operator<<(long v);
	CChunk &operator<<(const CZ &z);

protected:
	/* Shared by every 4-byte-and-wider operator<< (big-endian pack) / operator>>
	 * (big-endian unpack), matching WriteHeader()'s own byte-order idiom.
	 */
	void WriteBE32(unsigned long v);
	unsigned long ReadBE32();
	void WriteBE16(unsigned short v);
	unsigned short ReadBE16();

	CChunkBase *mFather;      /* +0x24: logical parent in the chunk TREE (may
	                              differ from the LinkSubChunk() bookkeeping
	                              father, set separately via SetFather()) */
	unsigned int mRankNumber; /* +0x28 */
};

/* CChunkBlock -- single-child container chunk (flags&0x6==2). Embeds its own
 * CChunkInfoList (for the type=3/flags=0x18 info sub-chunk PreClose() writes) and
 * tracks exactly one linked child chunk at a time (mChild), unlike CChunk's
 * position-relative multi-sibling model.
 */
class CChunkBlock : public CChunk {
public:
	CChunkBlock(const SChkHeader &hdr);
	virtual ~CChunkBlock();

	virtual bool GetNextSubChunk(CChunk *&sub);
	virtual bool AddSubChunk(CChunk *&sub, SIdVRF id);
	virtual bool CloseSubChunk(CChunk *&sub);
	virtual bool PostClose();
	virtual unsigned int GetRelSonNumber() const;
	virtual bool OnSetInfo(CChunkInfoItem *item);
	virtual bool PreClose();
	virtual bool Init();
	virtual bool GetAllInfo();
	/* .text+0x08185360, thunk to CChunkBase::GetRelSonNestLev() -- no real
	 * override beyond the base's own body (kept only because a real weak
	 * symbol exists at this address in ground truth).
	 */
	unsigned int GetRelSonNestLev() const { return CChunkBase::GetRelSonNestLev(); }

private:
	CChunkInfoList mInfoList; /* +0x2c: embedded, real ground truth is a single
	                              head-pointer-sized member */
	CChunk *mChild;           /* +0x30 */
};

/* CChunkOrphan -- a CChunk whose backing store is a private, self-owned CMemory
 * buffer rather than the tree's shared physical stream. Used to preserve raw
 * sub-chunk bytes verbatim when a sub-chunk's own type isn't recognized by the
 * reader (confirmed real caller pattern from stream_family.h's own header
 * comment: "CChunkOrphan::CChunkOrphan() construct[s] a CMemory").
 */
class CChunkOrphan : public CChunk {
public:
	/* .text+0x080b2600. Real body: base-constructs CChunk(hdr), then
	 * `new CMemory(buf, hdr.length, /mode=/1)` (owning copy -- CMemory's own
	 * mode==1 means "allocate and memcpy the caller's data in", see
	 * stream_family.h) and installs it as mParent, then Open()s it for reading
	 * (mode=eRead=1). A soft-assert (FileChunk.cpp) fires if mParent's own
	 * post-adjust status field is neither 4 nor 5 (CMemory's own "state"
	 * values, see stream_family.h) after Open(). Confirmed by re-checking the
	 * real disassembly directly (not an assumption): this ctor NEVER writes
	 * its OWN mStatus -- it stays at the CChunkBase ctor default (eClosed).
	 * A real caller must separately arm it (SetRootStatus(eRead), the same
	 * bootstrap hook every other root-level chunk in this batch needs) before
	 * Get()/Read() will do anything.
	 */
	CChunkOrphan(const SChkHeader &hdr, unsigned char *buf, int len);

	/* .text+0x080b2550/0x080b25e0. Real body: PreClose()/Close()/PostClose()
	 * sequence (mirroring CChunkBase::~CChunkBase()'s own base-class teardown
	 * order), then a soft-assert-guarded call through mParent's own vtable
	 * slot +0x14 unless mParent's state is 4 or 5 (same CMemory state check as
	 * the ctor), finally deletes mParent (the owned CMemory).
	 */
	virtual ~CChunkOrphan();

private:
	CChunkOrphan(const CChunkOrphan &);
	CChunkOrphan &operator=(const CChunkOrphan &);
};

#endif /* CHUNK_FAMILY_H */
