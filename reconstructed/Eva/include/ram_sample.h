/*
 * ram_sample.h  -  CRamSample / CMultiSample / CRamSampleRelative, a tightly
 * packed cluster of 3 plain (non-virtual, no vtable) sample-metadata value
 * classes, found back-to-back in the binary (`.text+0x08427db0..0x08428340`,
 * zero unrelated code between them -- CMultiSample is even sandwiched inside
 * CRamSample's own address range, right after CRamSample::operator=).
 *
 * Found via a fresh `nm -C` class-inventory sweep (2026-07-28) for the next
 * CStorageConverterBase/CSpecialFuncCCMap-shaped opportunity. Ratio of
 * Get-star/Set-star/Is-star names: CRamSampleRelative 87% (30 nm entries), CRamSample 66%
 * (32 entries, lower % only because of the one real Initialize()/operator=
 * pair), CMultiSample 78% (9 entries) -- all cdecl (`this` at [esp+4], args
 * follow), NOT regparm(3) like OA.ko's STG family.
 *
 * REACHABILITY: real, non-CESxxxTask callers. `CRamSample`'s ctor/Initialize/
 * accessor calls come from `.text+0x0839xxxx`/`0x083c2xxx`/`0x0840c8xx`,
 * which resolve (via `nm -C` on the enclosing function) to `CFileAIF::`/
 * `CFileKSF::SetAddressAndFlagInRamSample`/`CFileSng::` -- real sample/song
 * file-format parsers, not the excluded CSK-model-layer `CESxxxTask` family.
 * (Those parser classes' own disk-I/O bodies are NOT reconstructed here --
 * only the data classes they populate, same division of labor already used
 * for CSpecialFuncCCMap's CESGlobalTask caller.)
 *
 * CRamSample -- REAL LAYOUT (all 29 non-Initialize/operator= accessor bodies
 * are single-field `mov`/`movzx`/`movsx`/`and`/`test+setne` one-liners;
 * natural x86 struct alignment reproduces every offset below with no manual
 * padding or `packed` attribute needed):
 *   +0x00 (u32) mFS              GetFS()/SetFS(unsigned long) -- sample rate
 *   +0x04 (u8)  mLoopTune        GetLoopTune()/SetLoopTune(int)
 *   +0x05 (u8)  mFlags           bitfield, see below
 *   +0x06,+0x07                  compiler alignment padding, never touched
 *   +0x08 (u32) mStartAddress    GetStartAddress()/SetStartAddress(u32)
 *   +0x0c (u32) m2ndStartAddress Get2ndStartAddress()/Set2ndStartAddress(u32)
 *   +0x10 (u32) mLoopStartAddress GetLoopStartAddress()/SetLoopStartAddress(u32)
 *   +0x14 (u32) mEndAddress      GetEndAddress()/SetEndAddress(u32)
 *   +0x18 (u32) mTopAddress      GetTopAddress()/SetTopAddress(u32)
 *   +0x1c (u32) mNumOfByte       GetNumOfByte()/SetNumOfByte(u32)
 *   +0x20       mName            GetName() (non-const `char*`, `return this+0x20;`)
 *
 * mFlags bits (all confirmed by direct bit-test/mask disassembly):
 *   0x04  "NotUse2nd"   IsNotUse2ndStart()/SetNotUse2ndStart(int) -- normalized
 *                       0/1 (real body: `test+setne`).
 *   0x08  "Plus12dB"    IsPlus12dB()/SetPlus12dB(int) -- IsPlus12dB() is NOT
 *                       normalized: ground truth is `return mFlags & 0x08;`,
 *                       i.e. returns literal 0 or 8, not 0/1. Preserved as-is.
 *   0x20  "Reverse"     IsReverse() -- same raw (0 or 0x20), NOT normalized,
 *                       same shape as IsPlus12dB(). No setter exists (no
 *                       method anywhere writes only this bit; SetFlag()
 *                       writes the whole byte).
 *   0x80  "OneShot"     IsOneShot() -- ground truth reads this bit via
 *                       `movsx+shr 0x1f` (sign-extend then take the top bit),
 *                       which DOES normalize to 0/1 -- the one asymmetric
 *                       exception among the 3 single-bit predicates above.
 *   GetFlag()/SetFlag(unsigned) read/write the whole byte verbatim.
 *
 * GetStartAddress(int) overload: `return (flag && !(mFlags & 0x04)) ?
 * m2ndStartAddress : mStartAddress;` -- real, confirmed branch shape.
 *
 * GetBank()/SetBank(int): CONFIRMED DEAD FIELDS. GetBank() is a bare
 * `return 0;` (no field read at all -- `xor eax,eax; ret`) and SetBank(int)
 * is a bare no-op (`ret` only, argument never touched, no field write). Bank
 * is simply not implemented/stored on this class in this build; transcribed
 * verbatim, not "fixed".
 *
 * Initialize(int, unsigned long, unsigned long): the FIRST (int) parameter is
 * a genuine dead argument -- ground truth's own disassembly never reads the
 * stack slot it occupies. Real body (confirmed 0x2d-byte function):
 *   mStartAddress = m2ndStartAddress = mLoopStartAddress = startAddr;
 *   mEndAddress = startAddr + numBytes - 1;
 *   mTopAddress = startAddr * 2;
 *   mNumOfByte  = numBytes * 2;
 *   mFlags = 0xd2;  // = 1101 0010b: NotUse2nd(0x04)=0 (2nd-start considered
 *                    // in use), Plus12dB(0x08)=0, Reverse(0x20)=0,
 *                    // OneShot(0x80)=1; bits 0x02/0x40 are also set but have
 *                    // no accessor anywhere in this class -- ground truth's
 *                    // own literal, transcribed verbatim, not modeled further.
 *
 * DEFERRED (not declared/implemented this batch): `CRamSample::operator=
 * (CUsrSample&)` (.text+0x08427ff0). Real body copies GetSampleRate()/4 real
 * offset-getters from CUsrSample into mFS/mStartAddress family (each right-
 * shifted by 1 -- byte-to-sample-word conversion), a flags byte computed from
 * 2 bytes read out of an undocumented struct at `*(CUsrSample*)this+0x4`
 * (offsets +0x3c/+0x3d), and a verbatim 0x18-byte block copy from a second
 * undocumented struct at `*(CUsrSample*)this+0x8` into mName[0..0x17] -- i.e.
 * mName is genuinely DUAL-USE (plain name buffer in the AIFF/KSF/Sng parser
 * path above, raw scratch storage for 6 CUsrSample-supplied dwords in the
 * USR-sample path). None of CUsrSample's own +0x4/+0x8 internal sub-struct
 * layouts are modeled anywhere in this project yet -- real future-batch lead,
 * not attempted here to avoid fabricating an unverified struct. mName is
 * sized to 0x18 bytes below as a documented lower bound from this deferred
 * method's own known minimum usage, not independently confirmed by any
 * reconstructed caller.
 *
 * CMultiSample -- REAL LAYOUT (all 7 non-ctor/dtor bodies are one-liners):
 *   +0x00 (u16) mTopOfRelative   GetTopOfRelative()/SetTopOfRelative(int)
 *   +0x02 (u8)  mNumOfRelative   GetNumOfRelative()/SetNumOfRelative(int)
 *   +0x03 (u8)  mFlags           bit 0x01 = NotUse2ndStart (IsNotUse2ndStart(),
 *                                 no separate setter -- SetFlag(int) below
 *                                 writes the whole byte)
 *   +0x04       mName            GetName() (`return this+0x4;`)
 * SetTopOfRelative(int) quirk, preserved verbatim: ground truth reads only
 * the LOW BYTE of its `int` argument (`movzx edx, BYTE PTR [esp+0x8]`) before
 * storing the 16-bit field -- any value outside 0-255 is silently truncated,
 * even though the field itself is 16 bits wide and GetTopOfRelative() reads
 * the full word back.
 *
 * CRamSampleRelative -- REAL LAYOUT (all 26 non-ctor/dtor bodies are
 * one-liners, object size 0x10 confirmed by the ctor's own dword write, see
 * below):
 *   +0x00 (u16) mSampleNumber    GetSampleNumber()/SetSampleNumber(int)
 *   +0x02 (u8)  mSampleBank      GetSampleBank()/SetSampleBank(int)
 *   +0x03 (u8)  mTopKey          GetTopKey()/SetTopKey(int)
 *   +0x04 (u8)  mOriginalKey     GetOriginalKey()/SetOriginalKey(int)
 *   +0x05 (u8)  mTune            GetTune()/SetTune(int)
 *   +0x06 (u8)  mLevel           GetLevel()/SetLevel(int)
 *   +0x07 (i8)  mTransposeOrPan  GetTranspose()/SetTranspose(int) AND
 *                                GetPan()/SetPan(int) -- CONFIRMED ALIASED:
 *                                both pairs read/write the exact same byte.
 *                                Get differs only in sign: GetTranspose()
 *                                zero-extends (`movzx`), GetPan() sign-
 *                                extends (`movsx`). Set is byte-identical
 *                                for both names. Ground truth's own choice,
 *                                not a transcription error -- preserved.
 *   +0x08 (i8)  mCutoff          GetCutoff()/SetCutoff(int), signed
 *   +0x09 (i8)  mResonance       GetResonance()/SetResonance(int), signed
 *   +0x0a (i8)  mAttack          GetAttack()/SetAttack(int), signed
 *   +0x0b (i8)  mDecay           GetDecay()/SetDecay(int), signed
 *   +0x0c (u32) mReserved0xc     Zeroed by the ctor only (`mov dword[this+0xc],
 *                                0`); no accessor anywhere in this class reads
 *                                or writes it -- a real opaque tail field
 *                                (likely a pointer/handle populated by an
 *                                unmodeled owner class), included purely to
 *                                match the real 0x10-byte object size the
 *                                ctor's own dword write proves.
 * SetupAsSkipped()/IsSkipped(): `mSampleNumber = 0xffff;` / `return
 * mSampleNumber == 0xffff;` -- the (sample bank, sample number) pair doubles
 * as a "this slot is unused" sentinel via the number field alone.
 */

#ifndef RAM_SAMPLE_H
#define RAM_SAMPLE_H

class CUsrSample; /* forward decl only -- operator= is deferred, see above */

class CRamSample {
public:
	CRamSample();
	~CRamSample();

	unsigned long GetFS() const;
	void SetFS(unsigned long v);

	int GetLoopTune() const;
	void SetLoopTune(int v);

	bool IsNotUse2ndStart() const;
	void SetNotUse2ndStart(int v);

	bool IsOneShot() const;

	/* NOT normalized to 0/1 -- returns raw 0 or 0x08, see file header */
	int IsPlus12dB() const;
	void SetPlus12dB(int v);

	/* NOT normalized to 0/1 -- returns raw 0 or 0x20, see file header */
	int IsReverse() const;

	unsigned GetFlag() const;
	void SetFlag(unsigned v);

	/* always 0; SetBank() is a real ground-truth no-op, see file header */
	int GetBank() const;
	void SetBank(int v);

	unsigned long GetStartAddress(int flag) const;
	unsigned long GetStartAddress() const;
	void SetStartAddress(unsigned long v);

	unsigned long Get2ndStartAddress() const;
	void Set2ndStartAddress(unsigned long v);

	unsigned long GetLoopStartAddress() const;
	void SetLoopStartAddress(unsigned long v);

	unsigned long GetEndAddress() const;
	void SetEndAddress(unsigned long v);

	unsigned long GetTopAddress() const;
	void SetTopAddress(unsigned long v);

	unsigned long GetNumOfByte() const;
	void SetNumOfByte(unsigned long v);

	char *GetName();

	void Initialize(int unusedIndex, unsigned long startAddr, unsigned long numBytes);

	/* CRamSample& operator=(CUsrSample&) deliberately NOT declared -- see
	 * "DEFERRED" note in the file header.
	 */

private:
	unsigned long mFS;             /* +0x00 */
	unsigned char mLoopTune;       /* +0x04 */
	unsigned char mFlags;          /* +0x05 */
	unsigned long mStartAddress;   /* +0x08 */
	unsigned long m2ndStartAddress;/* +0x0c */
	unsigned long mLoopStartAddress;/* +0x10 */
	unsigned long mEndAddress;     /* +0x14 */
	unsigned long mTopAddress;     /* +0x18 */
	unsigned long mNumOfByte;      /* +0x1c */
	char mName[0x18];              /* +0x20, size unconfirmed, see file header */
};

class CMultiSample {
public:
	CMultiSample();
	~CMultiSample();

	unsigned GetTopOfRelative() const;
	void SetTopOfRelative(int v); /* low byte only, see file header */

	unsigned GetNumOfRelative() const;
	void SetNumOfRelative(int v);

	bool IsNotUse2ndStart() const;
	void SetFlag(int v); /* whole byte, not just the NotUse2ndStart bit */

	char *GetName();

private:
	unsigned short mTopOfRelative; /* +0x00 */
	unsigned char mNumOfRelative;  /* +0x02 */
	unsigned char mFlags;          /* +0x03 */
	char mName[8];                 /* +0x04, size unconfirmed, see file header */
};

class CRamSampleRelative {
public:
	CRamSampleRelative();
	~CRamSampleRelative();

	unsigned GetSampleNumber() const;
	void SetSampleNumber(int v);

	unsigned GetSampleBank() const;
	void SetSampleBank(int v);

	unsigned GetTopKey() const;
	void SetTopKey(int v);

	unsigned GetOriginalKey() const;
	void SetOriginalKey(int v);

	unsigned GetTune() const;
	void SetTune(int v);

	unsigned GetLevel() const;
	void SetLevel(int v);

	/* aliased with GetPan()/SetPan() -- see file header */
	unsigned GetTranspose() const;
	void SetTranspose(int v);

	int GetPan() const;
	void SetPan(int v);

	void SetupAsSkipped();
	bool IsSkipped() const;

	int GetCutoff() const;
	void SetCutoff(int v);

	int GetResonance() const;
	void SetResonance(int v);

	int GetAttack() const;
	void SetAttack(int v);

	int GetDecay() const;
	void SetDecay(int v);

private:
	unsigned short mSampleNumber;  /* +0x00 */
	unsigned char mSampleBank;     /* +0x02 */
	unsigned char mTopKey;         /* +0x03 */
	unsigned char mOriginalKey;    /* +0x04 */
	unsigned char mTune;           /* +0x05 */
	unsigned char mLevel;          /* +0x06 */
	signed char mTransposeOrPan;   /* +0x07 */
	signed char mCutoff;           /* +0x08 */
	signed char mResonance;        /* +0x09 */
	signed char mAttack;           /* +0x0a */
	signed char mDecay;            /* +0x0b */
	unsigned long mReserved0xc;    /* +0x0c, see file header */
};

#endif /* RAM_SAMPLE_H */
