/*
 * vfat_entry.h  -  CVFATEntry, the VFAT long-filename directory-entry helper
 * (`.text+0x08142510..0x08147...` in the real `Eva` binary, 44 raw symbols per
 * `nm -C`). Continues this project's filesystem-metadata series alongside
 * `CDirEntry` (dir_entry.h) and `CZ` (cz_util.h) -- `CVFATEntry` is a real
 * subclass of `CFATEntry` (itself a subclass of `CDirEntry`, confirmed via its
 * own dtor's tail-`jmp` into `CFATEntry`/`CDirEntry`'s own dtor chain, and via
 * `CVFATEntry::operator=(CFATEntry const&)`/`(CDirEntry const&)` overloads),
 * but this pass does NOT model that inheritance in C++: `CFATEntry` itself is
 * not yet reconstructed as a class (only one of its own methods,
 * `GetFirstDataByte()`, has been read -- see below), and doing real
 * inheritance correctly would require establishing `CFATEntry`'s full layout
 * first. Instead, per this project's established "opaque raw-offset PEEK"
 * convention (`CZ`'s own `RawPtrField()`/`RawFlagField()`, `cz_util.h`),
 * `CVFATEntry` here is a self-contained value class over ONE opaque raw byte
 * buffer, with named accessor METHODS (not named struct fields -- deliberately,
 * to avoid asserting any C++ struct layout/overlap the real disassembly
 * doesn't actually establish) reading/writing at the real confirmed offsets
 * below.
 *
 * PARTIAL RECONSTRUCTION, real scope decision: of the class's 44 raw symbols,
 * this pass reconstructs the 16 self-contained methods below -- zero external
 * calls, zero `.rodata`/`.data` references (confirmed via `objdump -dr`, no
 * `call`/`R_386` relocations anywhere in any of their 16 own instruction
 * ranges), only plain field reads/writes and one small byte-checksum
 * algorithm. Deliberately NOT reconstructed, each for a distinct documented
 * reason:
 *   - `GetSlotIndex() const` (.text+0x08143b30): calls `CFATEntry::
 *     GetFirstDataByte() const` (.text+0x080fae80), which itself calls
 *     `CDirEntry::GetName() const` (already real, dir_entry.h) -- fully
 *     traceable, but needs `CFATEntry` to exist as a real base/member first;
 *     deferred rather than duplicating `CDirEntry::GetName()`'s body inline.
 *   - `OnShortNameChanged()`/`OnShortExtChanged()` (.text+0x08142df0/
 *     0x08143080, 0x28e/0x28f bytes): NOT the same trivial "set flag dword to
 *     1" shape as `OnLongNameChanged()`/`OnLongExtChanged()` below (confirmed
 *     by size alone -- ~15x larger) -- genuinely different, unread bodies.
 *   - `Serialize()`/`Deserialize()`/`GenerateShortNameExt()`/
 *     `AddCharToLongName()`/`BeginAddingCharToLongName()`/
 *     `EndAddingCharToLongName()`/`ResetLongNameExt()`/
 *     `ResetLongNameExtBitArray()`/`ResetVFATEntryData()`/`SetShortNameExt()`/
 *     `NeedShortNameExt()`/`GetWideChar()`/`operator=()` (3 overloads)/ctors/
 *     dtor: genuinely deep -- real calls into `CCodePage::ConvertToUnicode()`,
 *     `CLittleEndObj::SetWord()`, `CZ`'s own real (not-yet-modeled) `Insert`/
 *     ctor-with-content methods, and a per-instance vtable dispatch through
 *     `this`'s own vtable slot `+0x8` (confirmed via `Serialize()`'s own
 *     `call DWORD PTR [eax+0x8]`) -- same "real container/subsystem
 *     dependency, out of scope" class as `CZ`'s own 55 un-reconstructed
 *     container methods (cz_util.h).
 *
 * REAL CONFIRMED FIELDS (from the 16 reconstructed methods' own disassembly --
 * every offset below is a literal immediate transcribed directly from the real
 * instruction stream, not inferred):
 *   +0x04 (u8*) raw ptr of the short-NAME field (SAME offset as CDirEntry::
 *               mShortName's own RawPtrField() -- dir_entry.h; read by
 *               GetAliasChecksum())
 *   +0x0c (u32) raw len of the short-NAME field (SAME offset as CDirEntry::
 *               mShortName's own RawFlagField())
 *   +0x14 (u8*) raw ptr of the short-EXT field (== CDirEntry::mShortExt's own
 *               RawPtrField() offset)
 *   +0x1c (u32) raw len of the short-EXT field (== CDirEntry::mShortExt's own
 *               RawFlagField() offset)
 *   +0x74 + 0x20*i (u32), i in [0,20): "slot i populated" field read by
 *               IsLongNameBitArrayEmpty(count)'s own real loop (stride 0x20,
 *               20 == GetMaxNumEntryForLongName()'s own real constant;
 *               13 chars/slot * 20 slots == GetMaxCharForLongName()'s own
 *               0x104 == 260, an independent cross-check both the stride and
 *               count are real, not guessed). Nonzero == that VFAT
 *               long-name-continuation directory-entry slot is in use.
 *               NOTE: slot 19's own computed offset range, [0x2d4,0x2f4),
 *               OVERLAPS the 5 named fields below (0x2ec/0x2ed/0x2f0/0x2f4/
 *               0x2f8 all fall inside or right at that range) -- real ground
 *               truth, not a modeling bug on this pass's part: whatever
 *               "slot 19" conceptually is, its own trailing bytes are shared
 *               storage with these named fields. Kept as-is (raw offsets into
 *               ONE shared buffer, see design note below) rather than forcing
 *               a non-overlapping C++ struct that ground truth itself doesn't
 *               have.
 *   +0x2ec (u8)  mCurrentSlotIndex     (GetCurrentSlotIndex())
 *   +0x2ed (u8)  mCurrentAliasChecksum (GetCurrentAliasChecksum())
 *   +0x2f0 (u32) mOutputCodePage       (GetOutputCodePage(), a CCodePage::
 *               EPage value -- kept as a plain u32 here, CCodePage isn't
 *               modeled)
 *   +0x2f4 (u32) mHasValidLongNameExt  (HasValidLongNameExt()/
 *               OnLongNameChanged()/OnLongExtChanged() -- a plain int flag,
 *               NOT the same field as CDirEntry's own same-named VIRTUAL
 *               method; this is CVFATEntry's own concrete non-virtual
 *               accessor over its own field, confirmed by direct
 *               disassembly, not vtable dispatch)
 *   +0x2f8 (u32) mCurrentNumForShortNameExt (GetCurrentNumForShortNameExt())
 * Real total sizeof(CVFATEntry) is NOT confirmed by this pass (no ctor/full
 * field-map reconstructed) -- the raw buffer below is sized to comfortably
 * cover every offset actually read/written (up to +0x2f8+4 = 0x2fc), not a
 * claim about the true object size.
 *
 * DESIGN NOTE: every field above is exposed as an accessor METHOD over one
 * shared `unsigned char mRaw[]` buffer (matching CZ's own `RawPtrField()`/
 * `RawFlagField()` convention) rather than as named struct members -- the
 * slot-19/named-field overlap noted above means these fields do NOT occupy
 * disjoint C++ storage, so ordinary non-overlapping struct members would
 * misrepresent ground truth's real layout.
 *
 * VERIFICATION: `GetAliasChecksum() const` and both `IsLongNameBitArrayEmpty()`
 * overloads are genuinely branchy (GCC-unrolled length-dispatch cascades and an
 * 8-way Duff's-device remainder-entry loop respectively) -- confirmed
 * self-contained (no `call`/relocation anywhere in their own byte ranges) and
 * verified via this project's direct-execution-oracle technique (mmap+
 * PROT_EXEC on the real extracted machine code, see re-decompiler memory's
 * `x86-direct-execution-oracle-technique`): 60000 randomized trials (0
 * mismatches) across all three functions, including name/ext lengths swept
 * past their real 8/3-char bounds (clamping-to-space-padding confirmed) and
 * slot-array pokes/counts swept past the real 20-slot bound (bounds
 * confirmed, no OOB access assumed beyond what `count` explicitly requests).
 * `ComputeChecksum()`/`GetAliasChecksum(const u8*)` reduce to the same
 * recurrence (`sum = ror(sum,1) + nextByte`, seeded by the first byte with no
 * rotate) -- the classic DOS/VFAT short-name alias-checksum algorithm --
 * confirmed by direct inspection alone (a 13-byte and a 0x44-byte fully
 * linear function, no branches to get wrong).
 */

#ifndef VFAT_ENTRY_H
#define VFAT_ENTRY_H

#include <stdint.h>
#include <string.h>

class CVFATEntry {
public:
	/* .text+0x081462d0, 13 bytes (_ZN10CVFATEntry15ComputeChecksumEhh). Real
	 * body: no `this` touched (only 2 stack args referenced) -- genuinely a
	 * static helper. `sum = ror(prevSum, 1) + nextByte` (mod 256), the
	 * classic DOS/VFAT short-name alias-checksum recurrence.
	 */
	static unsigned char ComputeChecksum(unsigned char prevSum, unsigned char nextByte)
	{
		unsigned char rotated = (unsigned char)((prevSum >> 1) | (prevSum << 7));
		return (unsigned char)(rotated + nextByte);
	}

	/* .text+0x08146180, 0x44 bytes (_ZNK10CVFATEntry16GetAliasChecksumEPKh).
	 * Real body: `this` unused (const-qualified but the compiled body never
	 * reads it -- confirmed from disassembly, only [esp+8], the `name`
	 * pointer, is referenced). Applies ComputeChecksum()'s recurrence across
	 * exactly 11 bytes (an 8.3 short name + extension), seeded by name[0]
	 * with no rotate.
	 */
	unsigned char GetAliasChecksum(const unsigned char *name11) const
	{
		unsigned char sum = name11[0];
		for (int i = 1; i < 11; i++)
			sum = ComputeChecksum(sum, name11[i]);
		return sum;
	}

	/* .text+0x081461d0, 0xff bytes (_ZNK10CVFATEntry16GetAliasChecksumEv).
	 * Real body: builds the same 11-byte (8+3) buffer GetAliasChecksum(const
	 * u8*) consumes, but sourced from this object's OWN short-name/short-ext
	 * raw fields (+0x04/+0x0c, +0x14/+0x1c -- see header comment), padding
	 * any position past the real length with a literal space (0x20).
	 * Verified via the direct-execution oracle across lengths swept 0..11
	 * (past the real 8/3 clamp bounds), 0 mismatches.
	 */
	unsigned char GetAliasChecksum() const
	{
		const unsigned char *namePtr = RawShortNamePtr();
		uint32_t nameLen = RawShortNameLen();
		const unsigned char *extPtr = RawShortExtPtr();
		uint32_t extLen = RawShortExtLen();

		unsigned char buf11[11];
		for (int i = 0; i < 8; i++)
			buf11[i] = ((uint32_t)i < nameLen) ? namePtr[i] : (unsigned char)0x20;
		for (int i = 0; i < 3; i++)
			buf11[8 + i] = ((uint32_t)i < extLen) ? extPtr[i] : (unsigned char)0x20;
		return GetAliasChecksum(buf11);
	}

	/* .text+0x08145970/0x08145980/0x08145990/0x081459a0, 6 bytes each
	 * (mov eax,imm32; ret). Real hardcoded constants -- no `this` touched.
	 */
	static int GetMaxNumEntryForLongName() { return 0x14; }  /* 20 */
	static int GetMaxCharPerEntry()        { return 0x0d; }  /* 13 */
	static int GetMaxCharForLongName()     { return 0x104; } /* 260 == 20*13 */
	static int GetLongNameMark()           { return 0x40; }

	/* .text+0x08146920/0x08146930, 0xc bytes each. Real body: movzx byte
	 * field read (zero-extended, not sign-extended).
	 */
	unsigned char GetCurrentSlotIndex() const     { return ReadU8(0x2ec); }
	unsigned char GetCurrentAliasChecksum() const { return ReadU8(0x2ed); }

	/* .text+0x08146940/0x08146950/0x08142510, 0xb-0xc bytes each. Real body:
	 * plain dword field read.
	 */
	int GetCurrentNumForShortNameExt() const { return (int)ReadU32(0x2f8); }
	int GetOutputCodePage() const            { return (int)ReadU32(0x2f0); }
	int HasValidLongNameExt() const          { return (int)ReadU32(0x2f4); }

	/* .text+0x08142520/0x08142530, 0xf bytes each. Real body: unconditional
	 * `mov DWORD PTR [this+0x2f4], 1` -- both distinct symbols, identical
	 * bodies (confirmed byte-for-byte via disassembly), both write the SAME
	 * field HasValidLongNameExt() reads.
	 */
	void OnLongNameChanged() { WriteU32(0x2f4, 1); }
	void OnLongExtChanged()  { WriteU32(0x2f4, 1); }

	/* .text+0x081437c0, 0xfc bytes (_ZNK10CVFATEntry23IsLongNameBitArrayEmptyEv).
	 * Real body: `return IsLongNameBitArrayEmpty(GetMaxNumEntryForLongName());`
	 * -- a real GCC-unrolled 9-slots-at-a-time loop over the same 20-slot
	 * array the (unsigned int) overload below walks explicitly. Confirmed
	 * behaviorally identical via the direct-execution oracle, not just by
	 * this reduction.
	 */
	bool IsLongNameBitArrayEmpty() const
	{
		return IsLongNameBitArrayEmpty((unsigned int)GetMaxNumEntryForLongName());
	}

	/* .text+0x081438c0, 0x1d9 bytes
	 * (_ZNK10CVFATEntry23IsLongNameBitArrayEmptyEj). Real body: an 8-way
	 * Duff's-device-unrolled scan of the first `count` long-name-slot
	 * "populated" fields (+0x74 + 0x20*i, see header comment), returning
	 * false the instant any slot in range is nonzero, true if all `count`
	 * are zero (count==0 vacuously true, confirmed real -- ground truth's
	 * own `test esi,esi; je <return-true>` at entry). Verified via the
	 * direct-execution oracle across counts swept 0..25 (past the real
	 * 20-slot bound) and pokes at slot positions swept 0..24, 0 mismatches.
	 */
	bool IsLongNameBitArrayEmpty(unsigned int count) const
	{
		for (unsigned int i = 0; i < count; i++) {
			if (ReadU32(0x74 + 0x20 * i) != 0)
				return false;
		}
		return true;
	}

private:
	friend struct CVFATEntryTestHooks;

	unsigned char ReadU8(unsigned off) const { return mRaw[off]; }
	void WriteU8(unsigned off, unsigned char v) { mRaw[off] = v; }
	uint32_t ReadU32(unsigned off) const
	{
		uint32_t v;
		memcpy(&v, mRaw + off, sizeof v);
		return v;
	}
	void WriteU32(unsigned off, uint32_t v) { memcpy(mRaw + off, &v, sizeof v); }
	const unsigned char *RawShortNamePtr() const
	{
		return (const unsigned char *)(uintptr_t)ReadU32(0x04);
	}
	uint32_t RawShortNameLen() const { return ReadU32(0x0c); }
	const unsigned char *RawShortExtPtr() const
	{
		return (const unsigned char *)(uintptr_t)ReadU32(0x14);
	}
	uint32_t RawShortExtLen() const { return ReadU32(0x1c); }

	/* Real total object size unconfirmed by this pass -- sized to cover
	 * every offset actually read/written above (up to +0x2f8+4 = 0x2fc).
	 * See header comment.
	 */
	unsigned char mRaw[0x2fc];
};

#endif /* VFAT_ENTRY_H */
