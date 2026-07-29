/*
 * chunk_root_family.h  -  CChunkRootBase/CChunkRootWithSeek/
 * CChunkRootWithSeekWithCRC, the "index/seek/CRC on top of chunked I/O" layer
 * chunk_family.h's own 2026-07-28 header comment explicitly deferred as a
 * natural follow-up batch ("SIBLING SURVEY" section). Fresh `nm -C`
 * class-inventory sweep (2026-07-29) picked this up as the next dense,
 * previously-100%-untouched cluster, following the "shared root + siblings
 * via call-xref" pattern: CChunkRootBase (23 methods) derives from the
 * already-real `CChunkBase` (chunk_family.h), and CChunkRootWithSeek/
 * CChunkRootWithSeekWithCRC derive from EACH OTHER in a genuine linear chain
 * -- confirmed by a direct `.rodata` vtable byte dump (see CORRECTION below),
 * not by name similarity alone.
 *
 * CORRECTION to chunk_family.h's own prior speculative note: that header's
 * "SIBLING SURVEY" guessed `CBackupChunk` was "CChunk-derived" with a
 * "vtable-diffed byte-for-byte against CChunk's own" shape. A real vtable
 * dump this session (slots 20-25 across all four classes, `.rodata+0x8e84d60/
 * 0x8e84e00/0x8e85000/0x8e850a0`) shows this was WRONG: `CBackupChunk`'s own
 * vtable slot 25 (`GetNumByteAfterIndex`) is the LITERAL SAME function
 * pointer as `CChunkRootWithSeekWithCRC::GetNumByteAfterIndex()`
 * (0x80b3b00) -- i.e. `CBackupChunk` INHERITS that slot unchanged, which is
 * only possible if `CBackupChunk : public CChunkRootWithSeekWithCRC`. The
 * real hierarchy is a genuine linear chain:
 *   CChunkBase -> CChunkRootBase -> CChunkRootWithSeek ->
 *   CChunkRootWithSeekWithCRC -> CBackupChunk (deferred, see below)
 * not four independent CChunk-derived siblings. This is exactly the
 * "verify entanglement claims via real vtable reads, not name matching"
 * trap this project's own methodology warns about.
 *
 * SCOPE THIS BATCH: CChunkRootBase (23 methods, full), CChunkRootWithSeek (18
 * methods -- 17 full + `BuildSubChunkIndex()`'s own deep READ-mode body
 * deferred, see below), CChunkRootWithSeekWithCRC (15 methods, full), plus
 * the small self-contained `CCrc32` utility (2 methods) `GetCRC()` needs.
 * `CBackupChunk` itself (34 methods) is NOT reconstructed here -- ALL of its
 * ctors plus `GetNextPackSize`/`ReadNextPack`/`SkipNextPack`/`WriteNextPack`/
 * `WriteTailPack` call a real, out-of-scope proprietary compression codec
 * (`COComp`, dispatching into `CBarc`'s own ~3KB LZ-style
 * `m_ifnBCompress`/`m_ifnBDeCompress` routines -- confirmed via a direct
 * call-xref trace, NOT a superficial size guess) -- a genuinely separate
 * DSP-like subsystem, deliberately left for a future batch. `CImageStr`
 * (memory-backed `CStream`, 11 methods) is ALSO deferred -- it is the one
 * remaining dependency of `BuildSubChunkIndex()`'s deep body, and its own
 * `GetLength()`/`Tell()`/`Open()`/`Seek()` overrides implement a genuinely
 * distinct mode-dependent windowing scheme not yet independently traced.
 *
 * ARCHITECTURE (confirmed via the Ghidra static decompile export,
 * /home/share/Decomp/EVA_Decomp/eva_export/functions/ dir (per-function .c files), cross-checked
 * against chunk_family.h's own already-established CChunkBase/CStream
 * conventions):
 *
 *   `CChunkRootBase` turns a `CChunkBase` (which only knows how to read/
 *   write ITS OWN region of an already-open `CStream`) into something that
 *   owns opening and closing the physical stream itself: `SetPath()` stores
 *   a heap copy of a path string (freed/reallocated by `ResetPath()`),
 *   `OpenStreamInRead()`/`OpenStreamInWrite()` call `mParent->Open(path,
 *   mode)` then read/write this root's own `SChkHeader` via the
 *   already-established `CChunkBase::ReadHeader()`/`WriteHeader()`, and
 *   `Close()`/`CloseStream()` close `mParent` if it is still open (mState==
 *   4/"read-armed" or 5/"write-armed", the CStream states stream_family.h's
 *   own CMemory/CNullStr already establish). Two new pure virtuals declared
 *   here, `CheckHeader()`/`HasIndex()`, have NO real implementation until
 *   the deferred `CBackupChunk` -- `CChunkRootWithSeek::GetNextSubChunk()`
 *   calls `HasIndex()` unconditionally, meaning `CChunkRootWithSeek`/
 *   `CChunkRootWithSeekWithCRC` are, in ground truth too, never actually
 *   instantiated directly outside a `CBackupChunk`-shaped derived class
 *   (verify/test_chunk_root_family.cpp uses a small test-only concrete
 *   subclass to exercise them, same technique `CChunkBase::OnSetInfo()==0`
 *   already required for `CChunk`).
 *
 *   `CChunkRootWithSeek` adds a growable index of sub-chunk stream
 *   positions (`TObjArray<unsigned long>`, the SAME opaque
 *   capacity/count/growBy/data 4-field layout chunk_server.h's own
 *   `mUnknown80`/`mEntryCount`/`mUnknown88`/`mTableBuf` group already
 *   documents for `TObjArray<SIDEntry>` -- confirmed identical here from the
 *   ctor's own literal field inits, capacity=0x10, growBy=0x10, data=
 *   `malloc(0x40)`=16*sizeof(unsigned long)). Unlike chunk_server.h's own
 *   `TObjArray<SIDEntry>::Add()` (modeled as an INERT no-op stand-in, since
 *   that class's own round-trip behavior never depends on it), THIS
 *   project's own `AddSubChunk()`/`CopySubChunkIndex()`/`PreClose()`/
 *   `SeekToSubChunk()` all depend on the index actually holding real
 *   appended values for a meaningful round-trip test -- so `IndexAdd()` here
 *   is a genuinely FUNCTIONAL growable-array append (same "genuinely
 *   functional, not inert" spirit as `smpl_mem_manager_stub.cpp`), not a
 *   literal disassembly of `TObjArray<unsigned long>::Add()` itself (that
 *   method's own body is project-wide out of scope, same convention as
 *   `cz_util.h`'s `CZ`).
 *
 *   `AddSubChunk()` snapshots `mParent->Tell()` BEFORE calling the base
 *   `CChunkBase::AddSubChunk()` (which writes the fresh sub-chunk header),
 *   then -- if the new sub-chunk's own identity does NOT match the "excluded
 *   from index" predicate (`ExcludeFromIndex()`, a real virtual hook;
 *   `CChunkRootWithSeek`'s own base body always returns false/"not
 *   excluded"; `CChunkRootWithSeekWithCRC` excludes its own CRC sub-chunk;
 *   `CBackupChunk`, deferred, excludes its own index/src-path sub-chunks
 *   too) -- appends that snapshotted position to the index.
 *
 *   `PreClose()` (eWrite branch): adds a special index sub-chunk (identity
 *   `SIdVRF{0x18,0x00,0x00,0xfe}`, matching `GetNextSubChunk()`'s own
 *   "type=0xfe/subtype=0/id=0/flags=0x18" recognize-and-hide check), writes
 *   a literal 4-byte "begin index" marker, then EVERY recorded position
 *   (big-endian `unsigned long`, `CChunk::operator<<`, already established),
 *   then a literal 4-byte "end index" marker and the index's own entry
 *   count, and finally verifies the sub-chunk's own on-wire length came out
 *   to exactly 4 bytes short of what it should be (`iVar1==4` in the Ghidra
 *   decompile, transcribed as a soft-assert-worthy consistency check rather
 *   than reinterpreted) before deferring to `CChunkRootBase::PreClose()`.
 *   The `sm_pkcBeginIndex`/`sm_pkcEndIndex` literals are the SAME two static
 *   `const char*` ground truth already declares (`0x091ae880`/`0x091ae890`)
 *   -- their own real string CONTENTS were not independently confirmed via
 *   `.rodata` this session (both point at 4-byte regions this batch's own
 *   scope doesn't otherwise touch); modeled here as `"KCIX"`/`"KCEX"`
 *   placeholders of the right length, clearly marked.
 *
 *   `CChunkRootWithSeekWithCRC` layers a `CCrc32`-computed checksum sub-chunk
 *   (identity `SIdVRF{0x18,0x00,0x00,0xf7}`) on top, gated by a new `mCrcMode`
 *   field (the ctor's own `int` argument, stored as-is -- 1 selects the
 *   "flags|=6" branch HasCRCSubChunk()/GetNumByteAfterIndex() both check,
 *   any other value selects "flags=(flags&0xf9)|4"). `GetCRC()` (~1500
 *   bytes, the single largest method in this batch) closes the stream,
 *   reopens it for read, streams the WHOLE region before the CRC sub-chunk
 *   through a `CCrc32` (seeded with the standard reflected IEEE-802.3/zlib
 *   polynomial `0xedb88320`, confirmed literal) via a halving-retry scratch
 *   buffer allocation (starts at 1MB, keeps quartering until `malloc()`
 *   succeeds or hits 0), then reopens for write again to restore the
 *   original stream state. Faithfully transcribed as one real, non-trivial
 *   method (not deferred) since every other method in this class is a thin,
 *   otherwise-untestable wrapper around it.
 */

#ifndef CHUNK_ROOT_FAMILY_H
#define CHUNK_ROOT_FAMILY_H

#include "chunk_family.h"

/* Real, out-of-scope templated container method
 * `TObjArray<unsigned long>::Add(unsigned long)` (.text+0x08185370, mangled
 * `_ZN9TObjArrayImE3AddEm`) -- same "not a real template, a concretely-named
 * stand-in" convention as `TObjArray<CChunkServer::SIDEntry>::Add()`
 * (chunk_server.cpp), but modeled here as a genuinely FUNCTIONAL 4-field
 * (capacity/count/growBy/data) growable array via CChunkRootWithSeek's own
 * IndexAdd()/IndexClear() helpers below, not an inert stand-in -- see header
 * comment.
 */

/* Progress/cancel callback `CChunkRootWithSeekWithCRC::GetCRC()`/`CheckCRC()`
 * accept. Real ground truth calls a single vtable slot (`this,total,done,
 * &cancelFlag`) during the CRC scan; modeled here as one virtual method.
 */
class CChunkCallback {
public:
	virtual ~CChunkCallback() {}
	virtual void OnProgress(unsigned int total, unsigned int done,
	                         char *cancel) const = 0;
};

/* CCrc32 -- table-driven CRC accumulator, parameterized by polynomial and
 * seed. .text+0x080b4890 (ctor, builds the 256-entry table)/0x080b4930
 * (PutBuffer). The ctor's own per-entry table-build loop folds "c = i<<24;
 * for(8 rounds) c = (c<<1) ^ (poly if c's sign bit set else 0)" together
 * with the initial <<24 into a single `shl ecx,0x19` (25) for round 1 --
 * confirmed bit-exact equivalent to the canonical unfolded form by direct
 * emulation of both against every possible byte value (2026-07-29), so
 * modeled here as the canonical, readable loop rather than the folded one
 * ("reproduce behavior, not compiler artifacts").
 */
class CCrc32 {
public:
	CCrc32(unsigned long poly, unsigned long seed) : mPoly(poly), mCrc(seed)
	{
		for (unsigned long i = 0; i < 256; i++) {
			unsigned long c = i << 24;
			for (int bit = 0; bit < 8; bit++) {
				if (c & 0x80000000UL)
					c = (c << 1) ^ mPoly;
				else
					c = c << 1;
			}
			mTable[i] = c;
		}
	}

	/* .text+0x080b4930, 437 bytes (8x Duff's-device-unrolled real body,
	 * collapsed to a plain loop here). Real body: standard table-driven
	 * MSB-first CRC update, `crc = (crc<<8) ^ table[((crc>>24)^byte)&0xff]`
	 * per byte. A `len==0` call is a real no-op (confirmed: ground truth
	 * jumps straight to the store-back with `crc` unchanged).
	 */
	void PutBuffer(unsigned char *buf, unsigned long len)
	{
		unsigned long crc = mCrc;
		for (unsigned long i = 0; i < len; i++)
			crc = (crc << 8) ^ mTable[((crc >> 24) ^ buf[i]) & 0xFF];
		mCrc = crc;
	}

	/* NOT ground truth -- `GetCRC()`'s own real caller reads this object's
	 * +0x4 field directly (no named accessor found in the mangled symbol
	 * list); exposed here so C++ code can read the running accumulator.
	 */
	unsigned long GetCrc() const { return mCrc; }

private:
	unsigned long mPoly;      /* +0x0: stored but only read during table build */
	unsigned long mCrc;       /* +0x4: running accumulator */
	unsigned long mTable[256]; /* +0x8 */
};

/* CChunkRootBase -- owns opening/closing the physical CStream a chunk tree's
 * root sits on. See header comment.
 */
class CChunkRootBase : public CChunkBase {
public:
	CChunkRootBase(const SChkHeader &hdr);
	CChunkRootBase();
	virtual ~CChunkRootBase();

	virtual bool PreClose() { return true; }
	virtual bool PostClose();
	/* .text+0x080ad830. Real body: PreClose(), CChunkBase::Close(),
	 * PostClose(), then CloseStream() -- see each method's own comment.
	 * DISCOVERY this session: CChunkBase::Close()'s own real body (already
	 * established, chunk_family.h/cpp) self-dispatches OnWriteLenAndFlags()
	 * when mStatus==eWrite -- which for THIS class (unlike CChunk, whose
	 * OnWriteLenAndFlags forwards to a father that never existed before this
	 * batch) finally has a REAL implementation, and genuinely patches this
	 * root's own on-wire length field in place, for the first time ever
	 * exercised. That patch seeks mParent BACKWARD and leaves it there (see
	 * OnWriteLenAndFlags()'s own comment) -- a NOT-ground-truth position
	 * restore (`Seek()` back to the position captured just before
	 * CChunkBase::Close() runs) is inserted here so PostClose()'s own
	 * Tell()-based "end of stream" capture stays correct. Same story as the
	 * position-restore PreClose() (below) already needs after each
	 * CloseSubChunk() call.
	 */
	virtual bool Close();
	virtual bool Init();
	virtual bool OnWriteLenAndFlags(unsigned long a, unsigned long b,
	                                 unsigned long c, unsigned char flags);

	/* .text+0x08185200, weak. Real body: GetAbsSonNumber()-1 (NOT just
	 * GetAbsSonNumber() the way CChunkBase's own default does -- transcribed
	 * as-is, the "-1" is real ground truth).
	 */
	virtual unsigned int GetRelSonNumber() const { return GetAbsSonNumber() - 1; }

	/* .text+0x08185220/0x08185260/0x081852a0, all weak. Real bodies: none of
	 * these have any real implementation at THIS level -- each just
	 * soft-asserts (Chunk.h 0x207/0x20d/0x212) and returns a default. A root
	 * chunk has no logical father of its own (it OWNS its stream instead),
	 * so these stay unimplemented all the way up through this batch's own
	 * scope.
	 */
	virtual CChunkBase *GetFather() const
	{
		ApiAssertChunkH(0x207);
		return 0;
	}
	virtual void SetFather(CChunkBase *)
	{
		ApiAssertChunkH(0x20d);
	}
	virtual void SetRankNumber(unsigned int)
	{
		ApiAssertChunkH(0x212);
	}
	/* .text+0x081852e0, weak. Real body: unconditional `return 0` --
	 * different from CChunk::OnSetInfo()'s own real bubble-up-the-tree body,
	 * since a root chunk has no father to bubble up to.
	 */
	virtual bool OnSetInfo(CChunkInfoItem *) { return false; }

	/* .text+0x080ad720, real body: unconditional `return true`. Overridden
	 * by CBackupChunk (out of scope) with real media-presence logic.
	 */
	virtual bool MediaCheck() { return true; }

	/* .text+0x080a74a0 (CBackupChunk's own real override) -- declared PURE
	 * here since CChunkRootBase/CChunkRootWithSeek/CChunkRootWithSeekWithCRC
	 * (confirmed via direct vtable byte dump, all three show the literal
	 * `__cxa_pure_virtual`-style stub address 0x804c6ac at this slot) never
	 * implement it themselves.
	 */
	virtual bool CheckHeader() const = 0;
	/* .text+0x080a74f0 (CBackupChunk's own real override) -- same "pure all
	 * the way up through this batch's own scope" story as CheckHeader().
	 * CChunkRootWithSeek::GetNextSubChunk() calls this UNCONDITIONALLY,
	 * meaning ground truth itself never instantiates a bare
	 * CChunkRootWithSeek/CChunkRootWithSeekWithCRC either -- only via a
	 * CBackupChunk-shaped derived class. See header comment.
	 */
	virtual bool HasIndex() const = 0;

	/* .text+0x080b18c0/0x080b1890. SetPath() heap-copies `path` (or an empty
	 * string if NULL); ResetPath() frees the existing copy and clears it.
	 */
	void SetPath(const char *path);
	void ResetPath();

	/* .text+0x080b1850. Real body: closes mParent if it is still open (read-
	 * or write-armed) -- see header comment. CloseStream() and Close()'s own
	 * tail share this SAME logic; ground truth's own Close() reads as an
	 * inlined duplicate of this method's body, modeled here as Close()
	 * calling CloseStream() directly (same observable behavior, no literal
	 * inlining needed at the C++ source level).
	 */
	void CloseStream();

	/* .text+0x080b1970/0x080b1b30, ~430 bytes each. Real bodies: mParent->
	 * Open(path, mode), then read mParent's own now-current mState/
	 * mAccessMode to derive this root's own mStatus (matching
	 * CChunkBase::SetStatus()'s own remap table: parent write-armed(2/3)
	 * with mode==1 -> self eRead, mode==2/3 -> self eWrite; any other
	 * combination -> self eError, soft-assert), validates mFlags, then reads
	 * or writes this root's OWN 8-byte SChkHeader via the already-
	 * established CChunkBase::ReadHeader()/WriteHeader(). OpenStreamInRead()
	 * additionally requires `this->CheckHeader()` (vtable offset 0x50, pure
	 * at this level -- see above) to return true before proceeding. On
	 * success, both then check a value read off mParent through a single-arg
	 * indirect call this session could not pin to a named virtual slot with
	 * full confidence -- but it gates the ONLY path that returns success at
	 * all, and GetCRC()'s own use of the SAME call shape is directly
	 * compared against mBasePos (a stream POSITION, not a small type/kind
	 * enum), which only makes sense if this is `mParent->Tell()`. Modeled
	 * here as exactly that: `Tell()==8` (OpenStreamInWrite) / `Tell()==8 &&
	 * mDeclaredLen>0xe` (OpenStreamInRead) -- "we ended up exactly 8 bytes
	 * into the stream", i.e. this root's own freshly read/written header was
	 * the very first thing in the stream. On any failure, both close out
	 * cleanly (mStatus=eClosed, path buffer freed) and return false.
	 */
	bool OpenStreamInWrite();
	bool OpenStreamInRead();

protected:
	/* NOT ground truth -- lets CChunkRootWithSeek's own PreClose()/GetCRC()
	 * etc reach the shared header-file-name assert helper without exposing
	 * ApiAssertChunkH's own storage; matches CChunkBase's own protected
	 * static-assert-helper convention.
	 */
	static void ApiAssertChunkCpp(int line);

	char *mPath; /* +0x24: heap copy set by SetPath(), owned */

private:
	CChunkRootBase(const CChunkRootBase &);
	CChunkRootBase &operator=(const CChunkRootBase &);
};

/* CChunkRootWithSeek -- adds a growable index of sub-chunk stream positions
 * on top of CChunkRootBase, written as a trailing special sub-chunk at
 * PreClose() time so a later read-mode open can seek directly to any
 * sub-chunk by index instead of scanning linearly. See header comment.
 */
class CChunkRootWithSeek : public CChunkRootBase {
public:
	CChunkRootWithSeek(const SChkHeader &hdr);
	CChunkRootWithSeek();
	virtual ~CChunkRootWithSeek();

	/* .text+0x080b27d0/0x080b27e0, both 13 bytes: a tail call to the base
	 * version. Ghidra's own decompile types these `void` (the x86 `ret`
	 * leaves EAX == the base call's own return value untouched, which
	 * Ghidra's local analysis doesn't attribute back to the caller) --
	 * modeled here as a real pass-through, not a hardcoded `true`.
	 */
	virtual bool Init() { return CChunkRootBase::Init(); }
	virtual bool PostClose() { return CChunkRootBase::PostClose(); }
	/* .text+0x080b3650, 824 bytes -- see header comment. NOT-ground-truth
	 * addition: restores mParent's own stream position (via `Seek()`) to
	 * the true end of written data right after CloseSubChunk() closes the
	 * index sub-chunk, undoing that call's own patch-back side effect (see
	 * CChunkRootBase::Close()'s own comment) so any FOLLOWING write (a
	 * sibling sub-chunk, or CChunkRootWithSeekWithCRC's own CRC sub-chunk)
	 * continues from the right place.
	 */
	virtual bool PreClose();
	virtual bool GetNextSubChunk(CChunk *&sub);
	virtual bool AddSubChunk(CChunk *&sub, SIdVRF id);

	/* .text+0x080b2720, real body: unconditional `return false` -- "nothing
	 * is excluded from the index" at this level. CChunkRootWithSeekWithCRC
	 * overrides with real behavior.
	 */
	virtual bool ExcludeFromIndex(SIdVRF) { return false; }
	/* .text+0x080b2730, real body: unconditional `return 0`. */
	virtual unsigned int GetNumByteAfterIndex() const { return 0; }

	/* .text+0x080b34e0. Real body: if HasIndex(), returns the CURRENT
	 * mDeclaredLen unchanged (the on-disk root already has an index,
	 * assumed correctly sized); otherwise builds the index (see
	 * BuildSubChunkIndex()) and, on success, returns mDeclaredLen plus the
	 * exact byte cost the index sub-chunk would add when PreClose() writes
	 * it (4 + entryCount*4 + strlen(beginMarker) + strlen(endMarker)).
	 */
	unsigned long GetSizeWhenRewrite();

	/* .text+0x080b3550. Real body: (mStatus must be eRead, soft-assert
	 * otherwise -- log-only, continues anyway) index==0 seeks mParent
	 * directly to this root's own mBasePos (the start of sub-chunk 0, no
	 * index needed); index!=0 first BuildSubChunkIndex()s (returns false on
	 * failure), then -- if `index` is within the built index's own entry
	 * count -- seeks mParent to the recorded position for that entry
	 * (silently leaves the stream position untouched if `index` is out of
	 * range, matching ground truth's own "<=" early return). Returns
	 * whether mParent's own error flag is still clear afterward.
	 */
	bool SeekToSubChunk(unsigned int index);

	/* .text+0x080b29f0. Real body: strlen(beginMarker) + strlen(endMarker) +
	 * 4 (entry count) + entryCount*4 (one BE32 position per entry).
	 */
	static unsigned long ComputeIndexDataSize(unsigned long entryCount);

protected:
	/* .text+0x080b2af0, 335 bytes (Duff's-device-unrolled real body,
	 * collapsed to a plain loop here). Real body: if the index is non-empty,
	 * clears it (frees the old backing buffer); then appends every entry of
	 * `src` into this root's own index via IndexAdd().
	 */
	void CopySubChunkIndex(const unsigned long *src, unsigned long count);

	/* .text+0x080b2c60, 2100 bytes. Real body (fast paths only, faithfully
	 * reproduced): mStatus!=eRead -> return false immediately (an index can
	 * only be BUILT BY READING one back, never while writing); index already
	 * built (count!=0) -> return true immediately. DEFERRED: the actual
	 * eRead-mode body -- allocate a scratch buffer (halving-retry starting
	 * at min(mDeclaredLen clamped to [0x400,0x600], mDeclaredLen)), seek to
	 * the tail of this root's own region via a CImageStr-backed re-read,
	 * and parse the trailing index sub-chunk's own begin/end markers and
	 * BE32 position list back into the index -- needs CImageStr (a memory-
	 * backed CStream with its own not-yet-traced mode-dependent windowing
	 * behavior, see header comment), out of scope this batch. Real callers
	 * reaching this deferred body get a conservative `false` ("could not
	 * build index"), which is the correct, safe answer for every caller in
	 * this batch (PreClose()'s eRead branch, GetSizeWhenRewrite(),
	 * SeekToSubChunk()) -- none of them can silently corrupt data on a
	 * `false` return, they just report "no index available".
	 */
	bool BuildSubChunkIndex();

	/* NOT ground truth -- functional growable-array append backing the
	 * index (capacity/count/growBy/data, matching the real TObjArray<T>
	 * 4-field layout confirmed from the ctor's own literal inits). See
	 * header comment.
	 */
	void IndexAdd(unsigned long v);
	void IndexClear();

	/* +0x28. Real ground truth: SeekToSubChunk(0)'s own fast path seeks
	 * mParent directly to this field's value rather than going through the
	 * index at all -- presumably "the position of sub-chunk 0", captured
	 * once. No WRITER for this field was found anywhere in this batch's own
	 * scope (only the ctor's own zero-init) -- almost certainly populated
	 * by BuildSubChunkIndex()'s own deferred eRead-mode body (see below),
	 * making SeekToSubChunk(0) share that same deferred-until-populated
	 * story as every other read-mode-index consumer in this class.
	 */
	unsigned long mFirstSubChunkPos; /* +0x28 */

	unsigned long mIndexCapacity; /* +0x2c */
	unsigned long mIndexCount;    /* +0x30 */
	unsigned long mIndexGrowBy;   /* +0x34 */
	unsigned long *mIndexData;    /* +0x38: heap, owned */

private:
	CChunkRootWithSeek(const CChunkRootWithSeek &);
	CChunkRootWithSeek &operator=(const CChunkRootWithSeek &);

	static const unsigned long kInitialCapacity = 16;

	/* Real ground-truth statics (.text-adjacent .data, 0x091ae880/0x091ae890)
	 * -- 4-byte "begin index"/"end index" marker strings PreClose() writes
	 * verbatim and GetNextSubChunk()/BuildSubChunkIndex() would need to
	 * recognize on read-back. Real byte CONTENTS not independently
	 * confirmed via .rodata this session (see header comment) -- modeled as
	 * fixed-length 4-character placeholders of the right size.
	 */
	static const char sm_pkcBeginIndex[5];
	static const char sm_pkcEndIndex[5];
};

/* CChunkRootWithSeekWithCRC -- layers a CCrc32 checksum sub-chunk on top of
 * CChunkRootWithSeek's own index. See header comment.
 */
class CChunkRootWithSeekWithCRC : public CChunkRootWithSeek {
public:
	CChunkRootWithSeekWithCRC(const SChkHeader &hdr, int crcMode);
	CChunkRootWithSeekWithCRC();
	virtual ~CChunkRootWithSeekWithCRC() {}

	virtual bool ExcludeFromIndex(SIdVRF id);
	/* .text+0x080b3a00 -- see header comment. Same NOT-ground-truth
	 * position-restore as CChunkRootWithSeek::PreClose() applied to the CRC
	 * sub-chunk's own CloseSubChunk() call.
	 */
	virtual bool PreClose();
	virtual bool PostClose();
	virtual unsigned int GetNumByteAfterIndex() const;

	/* .text+0x080b4460, real body: (mFlags & 6) == 6. */
	bool HasCRCSubChunk() const { return (mFlags & 6) == 6; }

	/* .text+0x080b3c50, ~1500 bytes -- see header comment for the full
	 * mechanism. Computes the CRC of every byte in this root's own region
	 * BEFORE the trailing CRC sub-chunk. On success: *outCrc = the computed
	 * value, *outStoredCrc = the value found on disk (only meaningful if
	 * *outHadCrcSubChunk becomes 1), *outHadCrcSubChunk = whether a CRC
	 * sub-chunk (identity type=0xf7/subtype=0/id=0/flags=0x18) was actually
	 * found. Returns false on any I/O error or (for an eWrite-status root)
	 * if the underlying stream isn't currently write-armed.
	 */
	bool GetCRC(unsigned long *outCrc, unsigned long *outStoredCrc,
	            int *outHadCrcSubChunk, const CChunkCallback *cb);

	/* .text+0x080b43c0. Real body: (mStatus must be eRead and (mFlags&6)==6
	 * and mParent must be error-free) computes GetCRC(); returns whether a
	 * stored CRC sub-chunk was found AND it matches the freshly computed
	 * value.
	 */
	bool CheckCRC(const CChunkCallback *cb);

private:
	CChunkRootWithSeekWithCRC(const CChunkRootWithSeekWithCRC &);
	CChunkRootWithSeekWithCRC &operator=(const CChunkRootWithSeekWithCRC &);

	int mCrcMode; /* +0x3c: ctor's own raw int argument, 1 selects the
	                 "flags|=6" branch every real caller here checks */
};

#endif /* CHUNK_ROOT_FAMILY_H */
