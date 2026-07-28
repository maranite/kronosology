/*
 * res_entry.h  -  STriplet / CResInfo / CResEntry / CResEntryEx, Eva's resource
 * (bank/sample/etc.) identity + descriptor value classes.
 *
 * FOUND 2026-07-28, fresh `nm -C` class-inventory sweep for the next dense,
 * mechanical, previously-100%-untouched cluster (following the CSpecialFuncCCMap,
 * CRamSample, and CPartitionData/CMBR/CPBR-family batches -- see this project's own
 * `HARDWARE_REVIEW_LOG.md` and `PROJECT_BRAIN/status.md`). Two other promising-
 * looking candidates were traced and REJECTED before settling on this one:
 *   - `CVirtualDriverBase`/`CVirtualDriver` (35 `nm -C` methods, and independently
 *     flagged as a lead by `partition_table.h`'s own header comment) -- real, but its
 *     ctor/`MediaOpen()`/`MediaClose()` call directly into `CSingleSectCache`'s own
 *     `Enable`/`Disable`/`GetSect`/`InsertSect`/`InsertSects`/`Update`/`Read`
 *     (.text+0x08077b60..0x08079170, ~4KB, genuine sector-range-tracking cache
 *     algorithm -- NOT simple accessors) and `CMachineBase`'s own
 *     `PutCommand`/`Add` message-dispatch methods (also non-trivial). Correctly
 *     rejected as too deep for this pass -- real algorithmic dependencies, not just
 *     an out-of-scope caller pulling in a mechanical data class.
 *   - `CShortDirEntry`/`CVFATEntry` (57 `nm -C` methods, FAT short/long directory
 *     entries -- a natural-looking sibling to the just-landed
 *     `CPartitionData`/`CMBR`/`CPBR` batch) -- every real caller found via a full
 *     `objdump -d` xref sweep (`CCDGetHandleElem`/`CGetHandleDir`/`CGetHandleElem`/
 *     `CSetEntryDir`) is itself a `CCommandImp`-derived command object living inside
 *     the same large, deeply interconnected `CVirtualDriverBase`/`CDirectory`
 *     filesystem-driver subsystem rejected above -- one level removed, not a clean
 *     "out-of-scope caller, in-scope data class" split.
 *
 * REACHABILITY (this cluster): traced via a full `objdump -d` xref sweep (not
 * inlined). `CResEntry`'s/`CResEntryEx`'s real callers are `CResMan::FindOpen`
 * (already-reconstructed class, res_man.h -- `mJob`/`Start()` -- ctor only so far),
 * `CResMan::FindNextRes`/`FindNextResEx`/`LoadSingleRes`, `CRMApiInstance::
 * FindNextRes`/`FindFirstRes`/`FindNextResEx`/`FindFirstResEx`/`LoadSingleRes`,
 * `CLoadableResEntry`, and `CJobStack::AddLoadSingleRes` (job_stack.h,
 * ALREADY reconstructed) -- same "out-of-scope god-object caller, in-scope data
 * class" split already established for `CSpecialFuncCCMap`, but even more directly
 * motivated here: `res_man.h`'s own header comment already names `CResEntryEx` by
 * name as the element type of `CResMan::mTail`'s 10 `TVector<CResEntryEx,1>`
 * sub-regions -- a documented gap this batch closes.
 *
 * LAYOUT (confirmed by direct disassembly of every ctor/Reset/Copy/Deserialize/
 * Serialize body at .text+0x081507e0..0x081512c0):
 *
 *   STriplet (3 bytes) -- a plain 3-byte resource identity tuple. Real per-field
 *   meaning not decoded (no reconstructed caller ever reads the 3 bytes back
 *   individually by name, only copies them as a unit) -- modeled as 3 unnamed
 *   bytes, same "opaque, shape-faithful" treatment as `CChunkOnDemand::
 *   STripletOnDemand` (chunk_on_demand.h).
 *
 *   CResInfo (0x18 = 24 bytes, NOT polymorphic):
 *     +0x00  mResName[18]   17-char name + forced NUL at +0x11 (`SetResName()`:
 *            `strncpy(this, src, 0x11); this[0x11] = 0;`). Default ctor/
 *            `ResetResName()` zero all 18 bytes.
 *     +0x12..+0x17  mTail[6]  default ctor sets {0xff,0xff,0xff,0xff,0xff,0x00};
 *            copy-from-buffer ctor/`Deserialize()`/`Serialize()` treat these as
 *            plain opaque bytes, copied verbatim. Real per-byte meaning not
 *            decoded from `CResInfo`'s own methods alone -- see `CResEntry` below,
 *            which reuses these SAME bytes (embedded at `CResEntry`+0x22..+0x26)
 *            as raw storage for ITS OWN `STriplet` id + 2 flag bytes, directly
 *            poking them by absolute offset rather than through any `CResInfo`
 *            accessor (confirmed: `CResEntry::CResEntry()` writes `this+0x22`,
 *            `this+0x23`, `this+0x24`, `this+0x25`, `this+0x26` AFTER
 *            `CResInfo::CResInfo()` already default-inited that whole range --
 *            deliberately overwriting `mTail[0..4]`, `mTail[5]` alone survives
 *            untouched). Modeled here as a plain `unsigned char mTail[6]` so
 *            `CResEntry` can write it directly, same raw-offset-access
 *            convention already used project-wide for undecoded shared regions
 *            (`res_man.h`'s own `mUnknown34`/`mTail`, etc).
 *     `Deserialize(const unsigned char*)`/`Serialize(unsigned char*)`: real
 *     null-pointer soft-assert first (`"Assertion failed in module %s, line
 *     %i.\n"` / `"ResInfo.cpp"` / line 0x21 [ctor+Deserialize] or 0x33
 *     [Serialize] -- ALL 3 sites share literally the same call shape as
 *     `partition_table.cpp`'s own `ApiAssert()` helper, reused verbatim here),
 *     then a straight 24-byte field-by-field copy either direction. The
 *     2-argument `Deserialize(CChunk*)`/`Serialize(CChunk*)` overloads (real
 *     ground truth: .text+0x08151080/0x08151140) are NOT reconstructed --
 *     `CChunk` itself is an unmodeled, genuinely deep class elsewhere in this
 *     project (chunk_man.h and friends); same "declared opaque, body out of
 *     scope" boundary already used for `class CChunk;` in `chunk_server.h`.
 *
 *   CResEntry (0x28 = 40 bytes, POLYMORPHIC -- real vtable at +0x00, confirmed
 *   `vtable for CResEntry`/0x08e88aa0 read by every ctor; no `~CResEntry()`
 *   override exists in ground truth beyond the compiler-synthesized weak D1/D2
 *   pair, so only a plain `virtual ~CResEntry() {}` is declared here -- same
 *   "vtable shape faithful, no real dtor body" treatment as `CResFamily`'s D0):
 *     +0x04  mIndex (unsigned short)  set directly from the `unsigned short`
 *            ctor overload's 3rd arg; forced to 0xffff (sentinel "unused") by
 *            the `unsigned char,unsigned char,int,int` ctor overload and by
 *            `Reset()`.
 *     +0x08  mPos (int)   set directly from the `int,int` ctor overload's 3rd
 *            arg (the two overloads are mutually exclusive: one carries
 *            `mIndex`, the other carries `mPos`/`mSize`); forced to -1 by the
 *            `unsigned short` overload and by `Reset()`.
 *     +0x0c  mSize (int)  same pairing as `mPos`, 4th arg of that overload;
 *            forced to -1 by the other overload and by `Reset()`.
 *     +0x10  mInfo (CResInfo, 0x18 bytes) -- embedded, default-constructed
 *            first, THEN its own trailing `mTail[0..4]` bytes (absolute
 *            +0x22..+0x26) get overwritten directly below.
 *     +0x22..+0x24 (== mInfo.mTail[0..2])  the `STriplet` id argument, copied
 *            byte-by-byte.
 *     +0x25  (== mInfo.mTail[3])  the ctor's 3rd/4th `unsigned char` arg
 *            (`a`).
 *     +0x26  (== mInfo.mTail[4])  the ctor's 4th/5th `unsigned char` arg
 *            (`b`).
 *     `Reset()`: re-zeroes/re-sentinels +0x04/+0x08/+0x0c, calls
 *     `mInfo.ResetResName()`, then forces `mTail[3..5]` (absolute +0x22..+0x26)
 *     to 0xff each -- i.e. it does NOT call the full `CResInfo` default ctor
 *     again, just the name-reset + a manual 5-byte 0xff fill over the id/flag
 *     region.
 *     `Copy(const CResEntry&)`/copy-ctor/`operator=`: copy `mIndex`, `mPos`,
 *     `mSize`, call `mInfo.SetResName()` on the source's name, then copy
 *     `mTail[0..4]` verbatim (5 raw bytes, NOT `mTail[5]`).
 *
 *   CResEntryEx (0x2c = 44 bytes, derives from CResEntry, adds exactly ONE
 *   field):
 *     +0x28  mExtra (unsigned int)  set directly by every ctor overload's
 *            trailing `unsigned int` arg where present, else 0 (the 4
 *            "no unsigned int" overloads + `Reset()` + the `CResEntry`-copy
 *            ctor/`operator=`/`CopyEx(const CResEntry&)` all zero it; the
 *            `CResEntryEx`-copy ctor/`operator=`/`CopyEx(const CResEntryEx&)`
 *            copy it from the source). Real per-field meaning not decoded
 *            (bank-load status? object handle? -- no reconstructed caller
 *            reads it back by name).
 *     `Reset()`: calls `CResEntry::Reset()`, then zeroes `mExtra`.
 *     `CopyEx(const CResEntryEx&)`/`CopyEx(const CResEntry&)`: same shape as
 *     `operator=`, just non-self-check, unconditional (matches the real
 *     disassembly: `operator=` short-circuits on `this == &other`,
 *     `CopyEx()` never does).
 */

#ifndef RES_ENTRY_H
#define RES_ENTRY_H

/* Real per-byte meaning not decoded -- see file header. Shape-only, matching
 * CChunkOnDemand::STripletOnDemand's own "opaque, unreconstructed" treatment.
 */
struct STriplet {
	unsigned char b0, b1, b2;
};

class CResInfo {
public:
	/* .text+0x08150f00, 62 bytes. */
	CResInfo();

	/* .text+0x08150e60, 146 bytes. Real: if `src` is NULL, a soft-assert fires
	 * (ResInfo.cpp:0x21) then falls through to the same copy anyway (matches
	 * ground truth's own `je` landing back on the copy path, not returning
	 * early) -- faithfully preserved, not "fixed" into a real NULL guard.
	 */
	explicit CResInfo(const unsigned char *src);

	/* .text+0x08150f40, 99 bytes. Same NULL-soft-assert-then-copy-anyway shape
	 * as the ctor above (ResInfo.cpp:0x21).
	 */
	void Deserialize(const unsigned char *src);

	/* .text+0x08150fe0, 105 bytes. Same shape (ResInfo.cpp:0x33). */
	void Serialize(unsigned char *dst) const;

	/* .text+0x08151200, 6 bytes -- literally `return 0x18;`. */
	static unsigned SizeOf();

	/* .text+0x08151210, 43 bytes. */
	void SetResName(const char *name);

	/* .text+0x08151240, 38 bytes. */
	void ResetResName();

	char          mResName[18]; /* +0x00, see file header */
	unsigned char mTail[6];     /* +0x12, see file header -- reused by CResEntry */
};

class CResEntry {
public:
	/* Compiler-synthesized weak D1/D2 pair only in ground truth -- see file
	 * header. Vtable shape faithful via this being genuinely virtual.
	 */
	virtual ~CResEntry() {}

	/* NOT a standalone symbol in ground truth -- inlined at its only 2 real
	 * call sites (CResEntryEx's copy-ctor and CResEntry-converting-ctor,
	 * .text+0x08150cc0/0x08150d20: construct mInfo, then a real, separate,
	 * out-of-line `call CResEntry::Reset()`). Reconstructed as a real ctor
	 * so both call sites can share it normally.
	 */
	CResEntry() : mIndex(0), mPos(0), mSize(0), mInfo() { Reset(); }

	/* .text+0x08150830, 147 bytes. */
	CResEntry(STriplet id, const char *name, unsigned char a, unsigned char b, int pos, int size);

	/* .text+0x081508d0, 157 bytes. */
	CResEntry(STriplet id, const char *name, unsigned short index, unsigned char a, unsigned char b);

	/* .text+0x08150970, 128 bytes. */
	CResEntry(const CResEntry &other);

	/* .text+0x081509f0, 100 bytes. */
	CResEntry &operator=(const CResEntry &other);

	/* .text+0x081507e0, 66 bytes. */
	void Reset();

	/* .text+0x08150a60, 94 bytes. */
	void Copy(const CResEntry &other);

protected:
	unsigned short mIndex; /* +0x04 */
	int            mPos;   /* +0x08 */
	int            mSize;  /* +0x0c */
	CResInfo       mInfo;  /* +0x10, 0x18 bytes -- mInfo.mTail[0..4] double as this
	                         * class's own STriplet id + 2 flag bytes, see file header. */

	friend struct ResEntryTestHooks;
};

class CResEntryEx : public CResEntry {
public:
	/* .text+0x08150ae0, 113 bytes. */
	CResEntryEx(STriplet id, const char *name, unsigned char a, unsigned char b, int pos, int size,
	            unsigned extra);

	/* .text+0x08150bd0, 113 bytes. */
	CResEntryEx(STriplet id, const char *name, unsigned char a, unsigned char b, int pos, int size);

	/* .text+0x08150b60, 106 bytes. */
	CResEntryEx(STriplet id, const char *name, unsigned short index, unsigned char a, unsigned char b,
	            unsigned extra);

	/* .text+0x08150c50, 106 bytes. */
	CResEntryEx(STriplet id, const char *name, unsigned short index, unsigned char a, unsigned char b);

	/* .text+0x08150cc0, 96 bytes. */
	CResEntryEx(const CResEntryEx &other);

	/* .text+0x08150d20, 87 bytes. */
	explicit CResEntryEx(const CResEntry &other);

	/* .text+0x08150d80, 45 bytes. */
	CResEntryEx &operator=(const CResEntryEx &other);

	/* .text+0x08150dc0, 44 bytes. */
	CResEntryEx &operator=(const CResEntry &other);

	/* .text+0x08150df0, 51 bytes. */
	void CopyEx(const CResEntryEx &other);

	/* .text+0x08150e30, 38 bytes. */
	void CopyEx(const CResEntry &other);

	/* .text+0x08150ac0, 30 bytes. */
	void Reset();

private:
	unsigned mExtra; /* +0x28, see file header */

	friend struct ResEntryTestHooks;
};

#endif /* RES_ENTRY_H */
