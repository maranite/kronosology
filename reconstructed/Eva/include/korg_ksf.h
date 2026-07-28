/*
 * korg_ksf.h  -  CKorgKsf, the Korg .KSF single-sample file class. Partial
 * reconstruction -- see korg_kmp.h's own file header for the full "shared
 * root + siblings" writeup, the corrected CSTGMultisampleBank/name-collision
 * finding, and why this whole family is a deep on-disk-format leaf rather
 * than a CSTGMultisampleBank-blocked one. Same batch, same conventions.
 *
 * WHAT IS RECONSTRUCTED HERE:
 *   - CKorgKsf(name, displayName, field16a, type, field154) -- the REAL
 *     5-argument ctor (.text+0x089d0580, 526 bytes). Chains
 *     CKorgRiff(name, ".KSF"); stores `field16a`/`type`/`field154` verbatim
 *     (real offsets 0x16a/0x17c/0x154 -- no independently-recoverable
 *     semantic meaning, none of this pass's methods read them back except
 *     `mBuffer`/`mDataSize`, see below); zero-inits `mBuffer`/`mDataSize`.
 *     Ground truth's own ctor ALSO builds a type-suffixed short display-name
 *     copy into an undocumented, partially CNameChunk-overlapping field
 *     (same idiom as korg_kmp.h's own "mShortDisplayName" -- see that file's
 *     header) -- not reproduced here, since nothing reconstructed in this
 *     pass reads it. Several other real ctor fields are unconditionally
 *     zeroed with no ctor-argument origin and no reconstructed reader
 *     (0x138/0x13c/0x140/0x144/0x148/0x150/0x158/0x166/0x168/0x16e) -- also
 *     not modeled, harmless (their real ground-truth value is always 0
 *     immediately after construction regardless).
 *   - ~CKorgKsf() -- .text+0x089cff50 (D1) / 0x089cff90 (D0). Real body
 *     empty.
 *   - TypeString(KorgType) -- .text+0x089d09a0. Byte-identical body/string
 *     table to CKorgKmp::TypeString() (korg_kmp.h) -- transcribed as its own
 *     distinct symbol per this project's "distinct ground-truth symbols stay
 *     distinct" convention.
 *   - IsBigEndian() const -- .text+0x089da350. Real body: unconditional
 *     `return true`, same as CKorgKmp's own override.
 *   - MakeSampleFileName(a, b, c, dest, maxLen) -- .text+0x089d0950. Real
 *     body: `sprintf(dest, "MS%03u%03u", a, c+1)` (format string confirmed
 *     via direct `.rodata` byte dump at 0x08f781f1) then a TAIL CALL to
 *     `CKorgFile::MakeFileName(dest, 0xd, ".KSF")` with a HARDCODED
 *     `maxLen=0xd` -- ground truth's own `b` and `maxLen` parameters are
 *     genuinely never read by this function's real body (confirmed: neither
 *     stack slot is ever loaded), preserved faithfully as unused parameters
 *     rather than dropped, matching this project's "preserve real quirks"
 *     convention.
 *   - SetSampleDataSize(size, alloc) -- .text+0x089d0850. Real body: frees
 *     any existing `mBuffer` and stores `mDataSize = size` unconditionally,
 *     then `mBuffer = malloc(size)` iff `size != 0 && alloc`.
 *
 * NESTED VALUE TYPES (own `objdump` reads, same bounded-accessor idiom as
 * `CKorgRiff::CNameChunk` / `CKorgKmp::CMultisampleChunk`):
 *   - CSampleChunk: GetName/SetName over a 17-byte mName (SetName writes 16,
 *     GetName reads 17+force-null, same idiom as CMultisampleChunk).
 *     GetStartOffsetSamples(idx)/SetStartOffsetSamples(idx,val) over a
 *     2-element `mStartOffsetSamples[2]` array, real ground truth bounds-
 *     checks `idx <= 1` (returns 0 / no-ops otherwise).
 *   - CSampleFileNameChunk: GetSampleFileName/SetSampleFileName over a
 *     13-byte mSampleFileName (SetSampleFileName writes 12, but ALSO
 *     real-ground-truth-clears the destination to an empty string if the
 *     source pointer is NULL -- confirmed via its own explicit NULL check,
 *     not present in the sibling GetName/SetName pairs above).
 *   - CSampleDataChunk: SetOneShot(bool) -- real body sets/clears bit 0x80
 *     of a single `mFlags` byte (`this[+4]` in ground truth; modeled here as
 *     this class's own dedicated field, not literally overlapping anything
 *     else, per this batch's general "behavior not byte-exact ABI" policy).
 *
 * NOT RECONSTRUCTED THIS PASS: Read()/Write()/ReadChunk()/WriteFile() (the
 * real per-chunk-tag on-disk record parser, same genuine depth as
 * CKorgKmp's own ReadChunk()), and every other CSampleChunk field beyond
 * mName/mStartOffsetSamples (real per-instance sample-rate/loop-point/etc
 * fields, only touched by the deferred ReadChunk()/WriteFile()).
 */

#ifndef KORG_KSF_H
#define KORG_KSF_H

#include "korg_riff.h"

class CKorgKsf : public CKorgRiff {
public:
	/* Real ground-truth values 0/1/2 -- CKorgKsf has its own, separate
	 * nested KorgType (confirmed via CKorgKmp::AddSample()'s own mangled
	 * signature referencing `CKorgKsf::KorgType` as a distinct type from
	 * `CKorgKmp::KorgType`), same Mono/Left/Right semantics per
	 * TypeString()'s own identical string table.
	 */
	enum KorgType { Mono = 0, Left = 1, Right = 2 };

	/* .text+0x089d07b0 (GetName) / 0x089d07e0 (SetName) /
	 * 0x089d0810 (GetStartOffsetSamples) / 0x089d0830 (SetStartOffsetSamples).
	 */
	class CSampleChunk {
	public:
		void GetName(char *dest, unsigned int maxLen) const;
		void SetName(const char *name);

		/* idx must be 0 or 1; returns 0 / no-ops otherwise (real
		 * ground-truth bounds check).
		 */
		unsigned int GetStartOffsetSamples(unsigned int idx) const;
		void SetStartOffsetSamples(unsigned int idx, unsigned int value);

	private:
		char mName[0x11];
		unsigned int mStartOffsetSamples[2];
	};

	/* .text+0x089d08c0 (GetSampleFileName) / 0x089d08f0 (SetSampleFileName). */
	class CSampleFileNameChunk {
	public:
		void GetSampleFileName(char *dest, unsigned int maxLen) const;
		void SetSampleFileName(const char *name);

	private:
		char mSampleFileName[0xd];
	};

	/* .text+0x089d0930 (SetOneShot). */
	class CSampleDataChunk {
	public:
		void SetOneShot(bool oneShot);

	private:
		unsigned char mFlags;
	};

	/* .text+0x089d0580, 526 bytes. See file header for full provenance. */
	CKorgKsf(const char *name, const char *displayName, unsigned int field16a,
	         KorgType type, bool field154);

	/* .text+0x089cff50 (D1) / 0x089cff90 (D0). Real body empty. */
	virtual ~CKorgKsf();

	/* .text+0x089d09a0. Same string table as CKorgKmp::TypeString(). */
	static const char *TypeString(KorgType type);

	/* .text+0x089da350. Real body: unconditional `return true`. */
	virtual bool IsBigEndian() const;

	/* .text+0x089d0950. See file header: `b` and `maxLen` are real,
	 * ground-truth-confirmed unused parameters.
	 */
	static void MakeSampleFileName(unsigned int a, unsigned int b, unsigned int c,
	                                char *dest, unsigned int maxLen);

	/* .text+0x089d0850. */
	void SetSampleDataSize(unsigned int size, bool alloc);

private:
	unsigned int mField16a; /* real offset 0x16a, ctor arg, purpose unknown */
	KorgType mType;          /* real offset 0x17c */
	bool mField154;          /* real offset 0x154, ctor arg, purpose unknown */
	void *mBuffer;           /* real offset 0x15c, malloc'd by SetSampleDataSize */
	unsigned int mDataSize;  /* real offset 0x160 */

	friend struct KorgKsfTestHooks;
};

#endif /* KORG_KSF_H */
