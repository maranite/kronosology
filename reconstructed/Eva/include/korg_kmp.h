/*
 * korg_kmp.h  -  CKorgKmp, the Korg .KMP multisample-map file class. Partial
 * reconstruction: see file header below for exactly what is/isn't covered.
 *
 * FOUND 2026-07-28, re-investigating a lead korg_file.h/korg_riff.h's own
 * "shared root + siblings" writeup left deferred: `CKorgKmp`/`CKorgKsc`/
 * `CKorgKsf`/`CKorgProgram`, the concrete `CKorgFile`/`CKorgRiff` siblings.
 * korg_file.h's own comment claimed all 3 (Kmp/Ksc/Ksf) "take a
 * `CSTGMultisampleBank*`" and are blocked on the project-wide out-of-scope
 * MOSS voice-model subsystem. A full `objdump -d -C` call-target sweep of
 * this entire family's own .text range (0x089ca550..0x089d1780, all 62
 * methods across the 4 classes) found ZERO calls to CSTGMultisampleBank or
 * any other out-of-scope class -- only libc, STL (list/string/vector), and
 * CKorgFile/CKorgRiff (already real). The "CSTGMultisampleBank*" claim was a
 * CLASS-NAME COLLISION: `nm -C` also lists an entirely unrelated,
 * genuinely-MOSS-dependent `CMultisampleChunk`/`CSampleChunk`/
 * `CSampleDataChunk` family at .text 0x08e30a50-0x08e31c00 (real signatures:
 * `CMultisampleChunk::ImportToBank(CFileStream&, CSTGMultisample*,
 * unsigned long)`, `CSampleChunk::ImportToBank(CFileStream&, CSTGSample*,
 * CSTGDrumSample*)`, etc) that happens to share short nested-class names with
 * THIS family's own (different, MOSS-free) nested types of the same name.
 * Corrected here; that other family remains untouched/out of scope.
 *
 * The REAL reason this family wasn't fully reconstructed in one pass is
 * different: it is a genuinely deep on-disk chunked binary format leaf.
 * `CKorgKmp::ReadChunk()` alone dispatches 5+ distinct chunk tags (real,
 * confirmed via their own `cmp` immediates reversed through the same
 * Bswap32-normalization `korg_riff.h` documents: "RLP1"/"RLP2"/"RLP3"/
 * "MNO1"/"MSP1"), each populating a different `std::list<T*>` with a
 * different fixed-size on-disk record (18/4/6/4-ish bytes respectively),
 * using GCC magic-constant integer division to compute record counts from
 * chunk length. `CKorgKmp`'s own ctor alone is 712 bytes; `WriteFile()` is
 * 1523, `SortSamples()` 1103, `AddSkippedSamples()` 759 -- reconstructing
 * these faithfully would mean independently re-deriving the entire on-disk
 * KMP multisample-map record format from raw disassembly, disproportionate
 * to a single batch (matching this project's "real additional depth"
 * precedent, just for a format-archaeology reason instead of a MOSS one).
 *
 * WHAT IS RECONSTRUCTED HERE (all independently verified via their own
 * `objdump -dr -M intel`, cross-checked against `CKorgKmp`'s ctor for field
 * roles):
 *   - CKorgKmp(name, displayName, ...) -- the REAL 10-argument ctor
 *     (.text+0x089ccaa0, 712 bytes), field-by-field. Several numeric fields
 *     (`mField13a`/`mField150`/`mField154`/`mField158`/`mField15c`/
 *     `mField160`/`mField161`/`mFlagA`/`mFlagB`) are stored verbatim from
 *     ctor arguments with NO independently-recoverable semantic meaning (no
 *     reconstructed method in this pass reads them back) -- named
 *     conservatively per this project's "mark uncertain fields" convention,
 *     not guessed. Two other real ctor side effects are NOT reproduced: a
 *     second, 16-byte truncated copy of the type-suffixed display name into
 *     an otherwise-unread field, and a call that sets the INHERITED
 *     `CKorgRiff::mChunkName` (private, no derived-class accessor exists in
 *     `korg_riff.h` -- adding one was judged out of scope for this pass
 *     rather than editing an already-verified, separately-committed file).
 *     Neither omission affects any method reconstructed here.
 *   - ~CKorgKmp() -- real body is empty (the 4 real member lists are cleaned
 *     up via ordinary list-node deletion in ground truth; modeled here as
 *     real `std::list<T*>` members, whose own destructors already do this).
 *   - TypeString(KorgType) -- .text+0x089cc570, static, indexes a 3-entry
 *     `.rodata` string table (direct byte dump: "Mono"/"Left"/"Right" at
 *     indices 0/1/2, confirmed against the ctor's own `MakeName`/
 *     `MakeNameLeft`/`MakeNameRight` dispatch using the SAME 0/1/2 values),
 *     default "Unknown" for any other value.
 *   - IsBigEndian() const -- .text+0x089d9dd0, real body: `return true;`
 *     unconditionally (KMP chunk payloads are big-endian on disk, per
 *     `korg_riff.h`'s own tag-normalization note -- this is the per-format
 *     override CKorgRiff's own base `return false` anticipates).
 *   - MakeFolder() -- .text+0x089cb720. `CKorgFile::GetFolder(buf, 0x100);
 *     mkdir(buf, 0777);` Byte-identical body to `CKorgKsc::MakeFolder()`
 *     (korg_ksc.h) -- transcribed as its own distinct symbol per this
 *     project's "distinct ground-truth symbols stay distinct even when
 *     bit-identical" convention (CKorgRiff::Swap/SwapBigEndian precedent).
 *   - IsStereoCounterpart(const CKorgKmp*) const -- .text+0x089cc4b0. False
 *     if either object's `mType` is Mono (0); otherwise compares both
 *     objects' `mDisplayName` with a trailing "-<ch>" suffix stripped via
 *     `strrchr(name, '-')`, true iff the two stripped base names match
 *     exactly (`strcmp`). Reads only `mType`/`mDisplayName`, both this
 *     class's OWN members (legal from any `CKorgKmp` method, no base-class
 *     access issue).
 *   - CanAddSample(unsigned char low, unsigned char high) const -- .text+
 *     0x089cb770. Real body: `if (mRelativeChunks.empty()) return true;`
 *     (a genuine, unconditional fast path -- no existing per-zone metadata
 *     means anything can be added), otherwise walks `mRelativeChunks` and
 *     `mRelative3Chunks` IN LOCKSTEP (ground truth advances both list
 *     iterators together, unconditionally, without independently bounds-
 *     checking the second list -- the two lists are evidently always kept
 *     the same length by the deferred `AddSample()`/`ReadChunk()`, preserved
 *     faithfully rather than "fixed"), returning false on the first pair
 *     where `rec3->mUnknownHigh <= high && rec2->mUnknownLow >= low`
 *     (an overlap check), true if no pair overlaps.
 *
 * NESTED VALUE TYPES (simple bounded-buffer accessors, same idiom as
 * `CKorgRiff::CNameChunk` -- own dedicated `objdump` read each):
 *   - CMultisampleChunk: GetName/SetName over a 17-byte `mName` (SetName
 *     writes 16 bytes; GetName reads all 17 + force-nulls the caller's own
 *     `dest[0x10]`, same "N-1 write / N read+force-null" idiom as
 *     `CNameChunk`).
 *   - CMultisampleRelativeChunk: GetName/SetName over a 13-byte `mName` at
 *     its own offset+6 (SetName writes 12, GetName reads 13+force-null).
 *     Real ground-truth per-instance byte at offset+1 is read by
 *     `CanAddSample()` above -- exposed here as `mUnknownLow` (purpose
 *     inferred ONLY from that comparison's own role, not independently
 *     confirmed against the deferred `ReadChunk()`'s real on-disk "RLP1"
 *     record layout).
 *
 * NOT RECONSTRUCTED THIS PASS (genuinely deep on-disk record parsing, real
 * next lead if this family gets revisited): Read()/Write()/ReadChunk()/
 * WriteFile(), AddSample()/GetSample()/SortSamples()/AddSkippedSamples()/
 * MakeMultisampleFileName(), the "RLP2"-tag `CMultisampleRelative2Chunk`
 * nested type + its list (touched by NOTHING reconstructed here), and
 * `mSamples` (the `std::list<CKorgKsf*>` `GetSample()`/`AddSample()` would
 * manage -- also not needed by anything reconstructed here). `CKorgKsc`/
 * `CKorgKsf` siblings: see korg_ksc.h/korg_ksf.h (same-pass partial
 * reconstructions). `CKorgProgram` (velocity-split/oscillator trees,
 * `COscillator::Sort()`/`::Add()` alone 946/1197 bytes): not attempted this
 * pass, real next lead, deferred for the same "genuinely deep" reason.
 */

#ifndef KORG_KMP_H
#define KORG_KMP_H

#include "korg_riff.h"

#include <list>

class CKorgKmp : public CKorgRiff {
public:
	/* Real ground-truth values 0/1/2, confirmed against the ctor's own
	 * MakeName/MakeNameLeft/MakeNameRight dispatch and TypeString()'s own
	 * "Mono"/"Left"/"Right" string-table order.
	 */
	enum KorgType { Mono = 0, Left = 1, Right = 2 };

	/* .text+0x089cb660 (GetName) / 0x089cb690 (SetName). 17-byte mName. */
	class CMultisampleChunk {
	public:
		void GetName(char *dest, unsigned int maxLen) const;
		void SetName(const char *name);

	private:
		char mName[0x11];
	};

	/* .text+0x089cb6c0 (GetName) / 0x089cb6f0 (SetName) / real per-instance
	 * byte read by CanAddSample() below. 6-byte unknown prefix (not touched
	 * by any reconstructed method) + 13-byte mName at offset+6.
	 */
	class CMultisampleRelativeChunk {
	public:
		void GetName(char *dest, unsigned int maxLen) const;
		void SetName(const char *name);

		/* Real field at this class's own offset+1 (before mUnknownPrefix's
		 * remaining bytes and mName). Purpose inferred only from
		 * CanAddSample()'s own comparison role -- see file header.
		 */
		unsigned char mUnknownLow;

	private:
		unsigned char mUnknownPrefix[5]; /* offsets +2..+6, real purpose unknown */
		char mName[0xd];
	};

	/* Real field at this class's own offset+5 (the last byte of its
	 * 6-byte on-disk "RLP3" record) read by CanAddSample() below. No other
	 * field of this real nested type is reconstructed this pass.
	 */
	class CMultisampleRelative3Chunk {
	public:
		unsigned char mUnknownPrefix[4]; /* offsets +0..+4, real purpose unknown */
		unsigned char mUnknownHigh;      /* real offset+5 */
	};

	/* .text+0x089ccaa0, 712 bytes. See file header for the full field-by-
	 * field provenance and the two real side effects NOT reproduced here.
	 */
	CKorgKmp(const char *name, const char *displayName, unsigned int field13a,
	         KorgType type, unsigned int field150, unsigned int field154,
	         unsigned int field158, unsigned int field15c,
	         unsigned char field160, unsigned char field161);

	/* .text+0x089cc590 (D1) / 0x089cca80 (D0). Real body empty. */
	virtual ~CKorgKmp();

	/* .text+0x089cc570. Static, indexes a 3-entry Mono/Left/Right string
	 * table; "Unknown" default for any other value.
	 */
	static const char *TypeString(KorgType type);

	/* .text+0x089d9dd0. Real body: unconditional `return true`. */
	virtual bool IsBigEndian() const;

	/* .text+0x089cb720. GetFolder(buf,0x100); mkdir(buf,0777). */
	void MakeFolder();

	/* .text+0x089cc480. strncpy(dest, mDisplayName, 0x19); dest[0x18]=0. */
	void GetName(char *dest, unsigned int maxLen) const;

	/* .text+0x089cc4b0. See file header for the full comparison logic. */
	bool IsStereoCounterpart(const CKorgKmp *other) const;

	/* .text+0x089cb770. See file header for the lockstep-list logic. */
	bool CanAddSample(unsigned char low, unsigned char high) const;

private:
	unsigned char mFlagA;  /* real offset 0x138, ctor always stores 0 */
	unsigned char mFlagB;  /* real offset 0x139, ctor always stores 1 */
	unsigned int mField13a;  /* real offset 0x13a, ctor arg, purpose unknown */
	KorgType mType;           /* real offset 0x140 */
	unsigned int mField144;  /* real offset 0x144, ctor always stores 0 */
	unsigned int mField148;  /* real offset 0x148, ctor always stores 0 */
	unsigned int mField14c;  /* real offset 0x14c, ctor always stores 0 */
	unsigned int mField150;  /* real offset 0x150, ctor arg, purpose unknown */
	unsigned int mField154;  /* real offset 0x154, ctor arg, purpose unknown */
	unsigned int mField158;  /* real offset 0x158, ctor arg, purpose unknown */
	unsigned int mField15c;  /* real offset 0x15c, ctor arg, purpose unknown */
	unsigned char mField160; /* real offset 0x160, ctor arg, purpose unknown */
	unsigned char mField161; /* real offset 0x161, ctor arg, purpose unknown */

	/* Real offset 0x164 ("RLP1" chunk, 18-byte on-disk records). */
	std::list<CMultisampleRelativeChunk *> mRelativeChunks;

	/* Real offset 0x174 ("RLP3" chunk, 6-byte on-disk records). Ground
	 * truth's own CanAddSample() always advances this in lockstep with
	 * mRelativeChunks -- see file header.
	 */
	std::list<CMultisampleRelative3Chunk *> mRelative3Chunks;

	/* Real offset 0x184, 25-byte buffer (0x18 chars written by the ctor's
	 * own MakeName/MakeNameLeft/MakeNameRight call, byte 0x18 force-
	 * available for NUL like every other bounded name buffer in this
	 * family). Read by GetName() and IsStereoCounterpart() (the latter reads
	 * this field directly rather than calling GetName(), matching ground
	 * truth).
	 */
	char mDisplayName[0x19];

	friend struct KorgKmpTestHooks;
};

#endif /* KORG_KMP_H */
