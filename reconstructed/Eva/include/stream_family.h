/*
 * stream_family.h  -  CStream / CIn / COut / CInOut / CNullStr / CMemory, Eva's
 * abstract byte-stream I/O framework and its two simplest concrete leaf backends.
 *
 * FOUND 2026-07-28, fresh `nm -C` class-inventory sweep for the next dense,
 * previously-untouched, well-defined cluster (following the CBitMaskL/CVFATEntry
 * closure -- see PROJECT_BRAIN/status.md). The obvious first candidate, `CBigEndObj`
 * (13 methods, big-endian byte<->int helpers, `.text+0x0809fd00`) was traced and
 * REJECTED: it is ALREADY documented as rejected in `partition_table.h`'s own header
 * comment -- every one of its real, non-inlined callers lives inside the
 * already-excluded CD-ROM/Joliet virtual-driver cluster (`file_io_base.h`'s
 * "OUT OF SCOPE" `CDDriverIO`/`CScsiDriverBase` family). Re-confirmed here via a
 * fresh `objdump -d` xref sweep before moving on, not just taken on faith.
 *
 * This cluster instead: `nm -C` alone shows `CNullStr`/`CMemory`/`CSysExStr` sharing
 * an unmistakable virtual-interface method set (`Open`/`Read`/`Write`/`Seek`/`Close`/
 * `Flush`/`AreUsingTheNext`/`CurrBufferCapacity`/`IsReady`, all against a `CStream`
 * base whose `EAccessMode`/`ESeekType` nested types appear in every signature) with
 * NO existing coverage anywhere in this project (`grep -rl "class CStream"` was
 * empty). Traced the real class graph via `nm -C "vtable for ..."` +
 * `objdump -s` on the raw vtable bytes + reading every concrete method body:
 *
 *   CStream                              (abstract base, 7 real methods)
 *     |          \
 *   CIn (virtual)  COut (virtual)        (2 real methods each, "leaf" single-byte
 *     |          /                        Get()/Put() convenience wrappers)
 *   CInOut : CIn, COut                   (classic diamond; 1 real method, Open())
 *     |         \
 *   CNullStr    CMemory                  (14 real methods each)
 *
 * (`CSysExStr` was deliberately EXCLUDED from this batch: its methods are named
 * `EOpen`/`ERead`/`ESeek`/`EClose`/`EFlush`, each taking an extra `CWithRef*`
 * argument -- a different, `CWithRef`-mediated adapter shape, not a literal
 * `CStream::Open`-signature override, and genuinely more complex/out of scope.
 * Likewise `CFinal`/`CImageStr`/`CSubBuff`/`CWithRef`/`CSyn`/`CAsyn` -- real,
 * larger `CStream`-family leaves seen in the same `nm -C` symbol range -- are
 * real-file/SysEx-backed and were NOT pulled in; only `CNullStr`/`CMemory`, the two
 * self-contained non-file-backed leaves, are reconstructed here.)
 *
 * SELF-CONTAINMENT: every method in this cluster was read from real disassembly
 * (`objdump -dr -M intel`, `.text+0x0804cf60..0x0804d130` for CStream/CIn/COut's
 * mechanical dtor/Open bodies, `.text+0x080a1070..0x080a2270` for CMemory/CNullStr/
 * CInOut). The ONLY external dependencies anywhere in this cluster are: `memcpy`,
 * `operator new[]`/`operator delete[]`/`operator delete` (`_Znaj`/`_ZdaPv`/`_ZdlPv`),
 * `_Unwind_Resume` (standard EH cleanup tail, not reproduced -- same convention as
 * every other class in this project), `HAL_DisableInterrupts()`/`HAL_EnableInterrupts()`
 * bracketing `free()` in `COut::~COut()`'s deleting-destructor variant (the SAME
 * already-established critical-section idiom used project-wide around malloc/free,
 * see limiter_base.h/task.h/etc -- not a red flag here despite superficially
 * resembling the "raw HAL_* = deep hardware code" pattern flagged elsewhere, since
 * this is just the standard heap-safety bracket), and the `Api`+0x94 soft-assert
 * (same "log-only, never enforcing" convention as everywhere else in this project,
 * `ApiAssert()` below, reusing partition_table.cpp's exact helper shape). No calls
 * into any other Eva subsystem.
 *
 * CIn/COut/CInOut REAL C++ DIAMOND, not hand-modeled: confirmed genuine `virtual`
 * multiple inheritance from `CMemory::CMemory(unsigned char*, unsigned long, int)`'s
 * own real ctor body, which calls `CStream::CStream()` DIRECTLY (bypassing CIn/COut/
 * CInOut) -- exactly the Itanium-ABI rule that only the most-derived class
 * constructs virtual bases. The base-object-constructor variant (`C2`, called only
 * from `CImageStr`'s own ctor, which multiply-inherits `CMemory`) reads its extra
 * implicit VTT (construction-vtable-table) parameter to install intermediate
 * sub-vtable pointers -- 100% mechanical, GCC-synthesized boilerplate; NOT
 * hand-transcribed here since writing ordinary `virtual`-inheriting C++ below makes
 * the real compiler regenerate the equivalent C1/C2/D0/D1/D2 variants itself. Every
 * dtor at the CStream/CIn/COut/CInOut level does NOTHING but reset vtable pointers
 * (confirmed empty-bodied from disassembly) -- reproduced here as plain empty
 * `virtual ~X() {}`, letting the compiler synthesize the mechanical parts.
 *
 * FIELD LAYOUT (CStream virtual-base subobject, confirmed from CStream::CStream()/
 * ::Open()/::GetLength()/::Tell()/::IsSought() and every subclass access via the
 * vbase-offset dance `eax = this + vtbl[-3]`):
 *   +0x04 (long)          mPosition    -- current file position (Tell())
 *   +0x08 (unsigned long) mLength      -- current stream length (GetLength())
 *   +0x0c (unsigned long) mLastOpLen   -- scratch: last Read/Write's byte count
 *                          (CMemory/CNullStr's own Read/Write write this, nothing
 *                          else reads it back -- real ground-truth field, exact
 *                          purpose beyond bookkeeping unconfirmed)
 *   +0x10 (unsigned long) mUnknown10   -- reset to 0 by every successful Open()
 *                          variant, set to 1 by CInOut::Open()'s "invalid mode"
 *                          fallback; never read anywhere in this cluster's own
 *                          bodies. TODO: verify (only CFinal/CImageStr/CSubBuff,
 *                          out of scope here, might consume it)
 *   +0x14 (int)            mState       -- 2 = "just opened", 4 = "read-armed"
 *                          (CMemory/CNullStr::Read require this), 5 = "write-armed"
 *                          (CMemory/CNullStr::Write require this)
 *   +0x18 (int)            mAccessMode  -- the EAccessMode last passed to Open()
 *                          (CIn::Open stores it verbatim; CInOut::Open's own
 *                          mode==1/mode-in-{2,3} branches instead store the FIXED
 *                          values 1/2 regardless of which of {2,3} was passed --
 *                          transcribed as-is, not "fixed")
 *
 * CMemory's OWN direct fields (confirmed from CMemory::CMemory()/~CMemory()):
 *   +0x08 unsigned char *mBuf       -- external or self-owned backing buffer
 *   +0x0c unsigned long  mCapacity  -- buffer size in bytes
 *   +0x10 int            mOwnMode   -- 3rd ctor arg verbatim: ==1 means "allocate
 *                         `new unsigned char[size]` and memcpy the caller's data
 *                         in" (mCapacity, mBuf now owned, freed by dtor); anything
 *                         else means "wrap the caller's buffer directly, caller
 *                         keeps ownership"
 *
 * `CMemory::Open()`'s real tail was flagged in an earlier pass as an unresolved
 * CFG ambiguity ("looks like two blocks unconditionally jumping to each other at
 * .text+0x080a1199/0x80a11c0"). RE-TRACED 2026-07-28 via `objdump -dr -M intel`
 * against .text+0x080a1160..0x080a11ef, definitively resolved (not a guess): the
 * earlier reading mis-attributed the jmp target of the instruction at 0x80a11cd.
 * Its real target, per objdump's own symbolic annotation and independently
 * verified by hand (`eb d2` at 0x80a11cd = 0x80a11cf + (int8_t)0xd2 = 0x80a11a1),
 * is 0x80a11a1 -- NOT back to 0x80a1199. There is no loop. The real CFG is two
 * ordinary diamonds: mode-branch -> mState-set -> converge at 0x80a1199 (checks
 * mState==4), then -> converge at 0x80a11a1 (checks mAccessMode==3) -> epilogue.
 * Every edge is taken at most once per call; fully acyclic, fully deterministic,
 * zero ambiguity for any of the 3 real EAccessMode values:
 *   eRead:      mState=4; mLength=mCapacity
 *   eWrite:     mState=5; (mLength/mPosition untouched)
 *   eReadWrite: mState=5; (mLength/mPosition untouched -- see below, this is
 *               provably DEAD in practice, not merely "untouched by coincidence")
 *
 * Bonus finding made while re-tracing: CMemory::Open()'s own `mAccessMode==3`
 * tail check (guarding `mPosition=mLength`) can NEVER fire for any real caller.
 * CMemory::Open() calls CInOut::Open() FIRST, and the original `mode` argument
 * arrives in a caller-saved register (eax) that call clobbers -- every later
 * branch in CMemory::Open() reads the POST-call `mAccessMode` field, never the
 * original argument. CInOut::Open() (see its class below, .text+0x080a1fd0)
 * ALWAYS stores a FIXED eWrite(2) for either eWrite or eReadWrite input, never
 * eReadWrite(3) itself -- confirmed from its own disassembly, and the identical
 * remap is independently corroborated by CNullStr::Open() below (already
 * documented in an earlier pass as "only the ==2 case is live in practice").
 * So `mAccessMode` is 1 or 2 by the time CMemory::Open()'s tail runs, NEVER 3:
 * both of CMemory::Open()'s own `mAccessMode==3` checks (the mid-function
 * mState=5 branch and the final mPosition=mLength branch) are real,
 * ground-truth-confirmed DEAD CODE in the shipped binary, not a defect
 * introduced by this reconstruction. Transcribed as-is below, matching real
 * dead code rather than "fixing" it into something that would actually execute.
 *
 * `CIn::Get`/`COut::Put` are single-byte convenience wrappers dispatching through
 * the SAME virtual slot Read()/Write() occupy (confirmed: `call [vtbl+0xc]` where,
 * for a `CIn`-typed vtable, slot+0xc is CIn's own unfilled/pure-virtual Read slot,
 * overridden by CMemory::Read/CNullStr::Read/etc in the real dynamic type) --
 * modeled directly as `Read(&b, 1)`/`Write(&v, 1)` calls, matching the real
 * dispatch target exactly without needing to hand-place vtable slots.
 *
 * `Read()`/`Write()`'s real return values (`this` unmodified for Read, `this+4` for
 * Write, confirmed identical in both CMemory and CNullStr) are NOT reproduced --
 * declared `void` here. No real external caller of either method was found
 * anywhere in the binary (see below), so the return value is unobservable from any
 * in-scope call site; the `this+4` adjustment likely reflects a covariant return
 * type (`COut&` vs `CIn&`) tied to this cluster's own multiple-inheritance layout,
 * not a data value worth chasing for a function nothing calls.
 *
 * REACHABILITY: real, non-inlined callers exist (`objdump -d` xref sweep) for the
 * ctors: `CMemoryChunk`/`CBackupChunk`/`CChunkOrphan::CChunkOrphan()` construct a
 * `CMemory`, `CNullChunk::CNullChunk()` constructs a `CNullStr`, and `CImageStr`'s
 * own ctor constructs a `CMemory` base subobject via the `C2` variant. All of those
 * caller classes are themselves unreconstructed (out of scope, part of the
 * CChunk-family chunked-storage subsystem already documented elsewhere in this
 * project, e.g. chunk_man.h) -- same "out-of-scope CALLER, in-scope DATA/UTILITY
 * class" split already established repeatedly in this project (CBitMaskL,
 * CLittleEndObj/partition_table.h, etc).
 */

#ifndef STREAM_FAMILY_H
#define STREAM_FAMILY_H

#include <cstring>

/* CStream -- abstract base for all byte-stream backends in this cluster.
 * Real ground truth vtable has 10 slots (D1, D0, GetLength, Tell, Open, + 5 pure
 * slots for Read/Write/Seek/Close/Flush -- AreUsingTheNext/CurrBufferCapacity/
 * IsReady are introduced further down the hierarchy, see CIn/COut's own extra
 * slots). Declared here as ordinary abstract virtuals; the real vtable SHAPE
 * doesn't need hand-reproduction, only the real dispatch RESULT for each override,
 * which is captured by writing normal C++ virtual inheritance and letting the
 * compiler generate its own (behaviorally equivalent) layout.
 */
class CStream {
public:
	enum EAccessMode { eRead = 1, eWrite = 2, eReadWrite = 3 };
	enum ESeekType { eSeekSet = 0, eSeekCur = 1, eSeekEnd = 2 };

	virtual ~CStream() {}

	/* .text+0x080a1df0, 40 bytes. Real body: unconditional field reset (mPosition,
	 * mUnknown10, mLastOpLen = 0; mAccessMode = mode; mState = 2). This is
	 * CStream's OWN base-level Open(), overridden at every derived level below --
	 * kept here as the shared "just opened" reset shape it clearly represents,
	 * though no reconstructed subclass calls it directly (CIn/COut/CInOut each
	 * have their own real override, see below).
	 */
	virtual void Open(const char *path, EAccessMode mode)
	{
		(void)path;
		mPosition = 0;
		mAccessMode = mode;
		mUnknown10 = 0;
		mLastOpLen = 0;
		mState = 2;
	}

	virtual void Read(void *buf, unsigned int n) = 0;
	virtual void Write(const void *buf, unsigned int n) = 0;
	virtual void Seek(long offset, ESeekType type) = 0;
	virtual void Close() = 0;
	virtual void Flush() = 0;
	virtual void AreUsingTheNext(unsigned int n) = 0;
	virtual unsigned int CurrBufferCapacity() const = 0;
	virtual bool IsReady() volatile = 0;

	/* .text+0x0804cf70, 8 bytes. */
	unsigned long GetLength() const volatile { return mLength; }
	/* .text+0x0804cf80, 8 bytes. */
	long Tell() const volatile { return mPosition; }

	/* Added 2026-07-28 for chunk_family.h's own CChunkBase family -- the real
	 * consumer this file's own header comment anticipated ("only CFinal/
	 * CImageStr/CSubBuff... might consume it"). CChunkBase::ReadHeader()/
	 * WriteHeader()/ReadBinary()/WriteBinary()/Init() all read mUnknown10
	 * (through the same this-adjusted CStream-view pointer used everywhere
	 * else in that cluster) as an "I/O error occurred" flag after a Read()/
	 * Write() call, and mLastOpLen as the byte count that operation actually
	 * moved -- both confirmed via direct field-offset cross-check against this
	 * class's own already-documented layout above (+0x0c/+0x10), not guessed.
	 */
	unsigned long GetLastOpLen() const volatile { return mLastOpLen; }
	bool HasIoError() const volatile { return mUnknown10 != 0; }

protected:
	CStream() : mPosition(0), mLength(0), mLastOpLen(0), mUnknown10(0), mState(0),
		mAccessMode(0) {}

	/* .text+0x080a2130, 253 bytes. Real body: resolves a pending seek request
	 * (`ref`, in/out) against the current position (eSeekCur), stream length
	 * (eSeekEnd, clamped to >=0), or literal target (eSeekSet, clamped to >=0),
	 * then confirms Tell() now matches the resolved target (or, for the
	 * mState==4/"read-armed" + target-past-EOF case, clamps to GetLength() and
	 * retries the confirm). Returns true if Tell()==target after resolution, or
	 * unconditionally true (with a soft-assert) for an out-of-range ESeekType.
	 * Real Api+0x94 soft-assert on invalid ESeekType: "DMStream.cpp" line 0x3d
	 * (61).
	 */
	bool IsSought(long &ref, ESeekType type)
	{
		long target;
		if (type == eSeekCur) {
			target = Tell() + ref;
		} else if (type == eSeekEnd) {
			target = (long)GetLength() + ref;
			if (target < 0)
				target = 0;
		} else if (type == eSeekSet) {
			target = ref;
			if (target < 0)
				target = 0;
		} else {
			ApiAssertStream(0x3d);
			mUnknown10 = 1;
			return true;
		}
		ref = target;

		if (mState == 4 && (unsigned long)target > GetLength()) {
			target = (long)GetLength();
			ref = target;
		}
		return Tell() == target;
	}

	static void ApiAssertStream(int line);

	long mPosition;            /* +0x04 */
	unsigned long mLength;     /* +0x08 */
	unsigned long mLastOpLen;  /* +0x0c */
	unsigned long mUnknown10;  /* +0x10 */
	int mState;                /* +0x14 */
	int mAccessMode;           /* +0x18 (really EAccessMode, stored as plain int) */
};

/* CIn -- virtual base CStream, adds the single-byte Get() convenience wrapper.
 * .text+0x0804cf90 (dtor)/0x080a1e20 (Open)/0x0804cfc0 (Get).
 */
class CIn : public virtual CStream {
public:
	virtual ~CIn() {}

	/* .text+0x080a1e20, 114 bytes. Real body: soft-assert if mode != eRead
	 * ("DMStream.cpp" line 0x55 = 85), then the same field-reset CStream::Open()
	 * does (mAccessMode stores the ACTUAL mode passed, not a fixed value --
	 * unlike CInOut::Open() below).
	 */
	void Open(const char *path, EAccessMode mode)
	{
		(void)path;
		if (mode != eRead)
			ApiAssertStream(0x55);
		mPosition = 0;
		mAccessMode = mode;
		mUnknown10 = 0;
		mLastOpLen = 0;
		mState = 2;
	}

	/* .text+0x0804cfc0, 37 bytes. Real body: this->Read(&b, 1) through the real
	 * dynamic type's Read() override (dispatches through the same vtable slot
	 * CStream::Read occupies).
	 */
	void Get(unsigned char &b) { Read(&b, 1); }
};

/* COut -- virtual base CStream, adds the single-byte Put() convenience wrapper.
 * .text+0x0804cff0 (D1)/0x0804d0c0 (D0)/0x080a1ec0 (Open)/0x0804d020 (Put).
 */
class COut : public virtual CStream {
public:
	/* Real ground truth: the deleting-destructor (D0) variant does
	 * `HAL_DisableInterrupts(); free(this); HAL_EnableInterrupts();` -- the
	 * standard project-wide malloc/free critical-section bracket (see this
	 * file's header comment), reproduced automatically by a plain `virtual`
	 * dtor + ordinary `delete`. Not hand-written here since it's the default
	 * C++ behavior for a class with no other cleanup.
	 */
	virtual ~COut() {}

	/* .text+0x080a1ec0, 128 bytes. Real body: soft-assert if mode is neither
	 * eWrite nor eReadWrite ("DMStream.cpp" line 0x5d = 93), then the same
	 * field-reset shape as CIn::Open (mAccessMode stores the actual mode).
	 */
	void Open(const char *path, EAccessMode mode)
	{
		(void)path;
		if (mode != eWrite && mode != eReadWrite)
			ApiAssertStream(0x5d);
		mPosition = 0;
		mAccessMode = mode;
		mUnknown10 = 0;
		mLastOpLen = 0;
		mState = 2;
	}

	/* .text+0x0804d020, 45 bytes. Real body: this->Write(&v, 1). */
	void Put(unsigned char v) { Write(&v, 1); }
};

/* CInOut : CIn, COut -- classic diamond join back to the shared CStream virtual
 * base. Adds exactly one real method: a unified Open() used by both CNullStr and
 * CMemory (each calls THIS Open(), not CIn's or COut's own).
 * .text+0x080a1f60 (D1)/0x080a2090 (D0)/0x080a2230 (D2)/0x080a1fd0 (Open).
 */
class CInOut : public CIn, public COut {
public:
	virtual ~CInOut() {}

	/* .text+0x080a1fd0, 133 bytes. Real body (see header comment): mode==eRead
	 * -> full reset with mAccessMode FIXED to eRead; mode in {eWrite,
	 * eReadWrite} -> full reset with mAccessMode FIXED to eWrite (even for
	 * eReadWrite -- transcribed as observed, not "fixed"); any other mode ->
	 * ONLY mUnknown10=1, nothing else touched.
	 */
	void Open(const char *path, EAccessMode mode)
	{
		(void)path;
		if (mode == eRead) {
			mPosition = 0;
			mAccessMode = eRead;
			mUnknown10 = 0;
			mLastOpLen = 0;
			mState = 2;
		} else if (mode == eWrite || mode == eReadWrite) {
			mPosition = 0;
			mAccessMode = eWrite;
			mUnknown10 = 0;
			mLastOpLen = 0;
			mState = 2;
		} else {
			mUnknown10 = 1;
		}
	}
};

/* CNullStr -- a "null sink" stream: tracks position/length bookkeeping exactly
 * like a real stream but never touches caller buffers or any real backing store
 * (used for a dry-run size-computation pass before a real write, matching its one
 * real caller, CNullChunk -- out of scope here, chunk_man.h's own family).
 * .text+0x080a18f0..0x080a1e20.
 */
class CNullStr : public CInOut {
public:
	CNullStr() {}
	virtual ~CNullStr() {}

	/* .text+0x080a1b10, 90 bytes. Real body: calls CInOut::Open(), then sets
	 * mState=4 if mAccessMode==eRead, or mState=5 if mAccessMode is 2 or 3
	 * (checked directly, not via the enum -- CInOut::Open() already collapsed
	 * eReadWrite to eWrite, so only the ==2 case is live in practice, but
	 * ground truth's own comparison range is {2,3}, transcribed as-is).
	 */
	void Open(const char *path, EAccessMode mode)
	{
		CInOut::Open(path, mode);
		if (mAccessMode == eRead)
			mState = 4;
		else if (mAccessMode == 2 || mAccessMode == 3)
			mState = 5;
	}

	/* .text+0x080a1920, 135 bytes. Real body: soft-assert if mState!=4
	 * ("NullStr.cpp" line 0x35=53). Advances mPosition by n, clamped so it
	 * never exceeds mLength; NEVER touches `buf` (a real null-sink: computes
	 * how many bytes WOULD be read without producing any data).
	 */
	void Read(void *buf, unsigned int n)
	{
		(void)buf;
		if (mState != 4)
			ApiAssertNullStr(0x35);
		unsigned long avail = mLength - (unsigned long)mPosition;
		unsigned long taken = (n > avail) ? avail : n;
		mLastOpLen = taken;
		mPosition += (long)taken;
	}

	/* .text+0x080a19b0, 130 bytes. Real body: soft-assert if mState!=5
	 * ("NullStr.cpp" line 0x43=67). Advances mPosition by n unconditionally
	 * (no clamping, unlike Read), growing mLength if the new position exceeds
	 * it. NEVER touches `buf`.
	 */
	void Write(const void *buf, unsigned int n)
	{
		(void)buf;
		if (mState != 5)
			ApiAssertNullStr(0x43);
		mLastOpLen = n;
		mPosition += (long)n;
		if ((unsigned long)mPosition > mLength)
			mLength = (unsigned long)mPosition;
	}

	/* .text+0x080a1b90, 78 bytes. Real body: CStream::IsSought() resolves the
	 * target into `ref`; if it disagreed (returned false), mPosition is force-set
	 * to the resolved `ref` value regardless.
	 */
	void Seek(long offset, ESeekType type)
	{
		long ref = offset;
		if (!IsSought(ref, type))
			mPosition = ref;
	}

	/* .text+0x080a18f0, 32 bytes. Real body: resets mPosition and mState to 0. */
	void Close() { mPosition = 0; mState = 0; }

	/* .text+0x080a1a50, 75 bytes. Real body: soft-assert if mState!=5
	 * ("NullStr.cpp" line 0x4f=79); no other effect (nothing to flush). */
	void Flush()
	{
		if (mState != 5)
			ApiAssertNullStr(0x4f);
	}

	/* .text+0x080a1ab0/0x080a1ad0/0x080a1af0, 3/3/6 bytes. Real bodies: fixed
	 * constants, no real logic. */
	void AreUsingTheNext(unsigned int) {}
	unsigned int CurrBufferCapacity() const { return 0; }
	bool IsReady() volatile { return true; }

private:
	static void ApiAssertNullStr(int line);
};

/* CMemory -- a fixed-buffer-backed stream: either wraps a caller-owned buffer
 * directly, or (mOwnMode==1) allocates and owns a private copy.
 * .text+0x080a1070..0x080a1880.
 */
class CMemory : public CInOut {
public:
	/* .text+0x080a1630 (C2, base-object variant, e.g. CImageStr's CMemory
	 * subobject)/0x080a1750 (C1, complete-object variant). Both share the same
	 * real business logic (see header comment re: the VTT-based mechanical
	 * differences, not reproduced). Real Api+0x94 soft-assert if buf==NULL
	 * ("Memory.cpp" line 0x16=22) -- log-only, the mode==1 path still runs
	 * memcpy(newBuf, NULL, size) exactly as ground truth does (no early return).
	 */
	CMemory(unsigned char *buf, unsigned long size, int mode) : mCapacity(size),
		mOwnMode(mode)
	{
		if (!buf)
			ApiAssertMemory(0x16);
		if (mode == 1) {
			mBuf = new unsigned char[size];
			memcpy(mBuf, buf, size);
		} else {
			mBuf = buf;
		}
	}

	/* .text+0x080a1520 (D1)/0x080a15a0 (D0)/0x080a1880 (D2). Real body: if
	 * mOwnMode==1 and mBuf!=NULL, `delete[] mBuf`. (Ground truth's own D0
	 * deleting-destructor variant calls `operator delete[]` on `this` too --
	 * an observed but functionally-inert quirk, see header comment; not
	 * hand-reproduced, a plain `delete` at any real call site is equivalent
	 * since nothing here overloads class-level operator delete/delete[].)
	 */
	virtual ~CMemory()
	{
		if (mOwnMode == 1 && mBuf)
			delete[] mBuf;
	}

	/* .text+0x080a1160, 158 bytes. Real body: CInOut::Open() first, then the
	 * mState/mLength/mPosition tail. CFG fully resolved 2026-07-28 (see header
	 * comment) -- byte-exact match to ground truth, not an approximation. The
	 * `mAccessMode == 3` branch below is confirmed-dead ground-truth code
	 * (CInOut::Open() already collapsed eReadWrite to eWrite by this point);
	 * kept as a literal transcription rather than removed. */
	void Open(const char *path, EAccessMode mode)
	{
		CInOut::Open(path, mode);
		if (mAccessMode == 1)
			mState = 4;
		else if (mAccessMode == 2 || mAccessMode == 3)
			mState = 5;
		if (mState == 4)
			mLength = mCapacity;
		else if (mAccessMode == 3)
			mPosition = (long)mLength;
	}

	/* .text+0x080a1350, 288 bytes. Real body: soft-asserts if mState!=4
	 * ("Memory.cpp" line 0x53=83) or if mCapacity!=mLength ("Memory.cpp" line
	 * 0x54=84, i.e. real ground truth expects the buffer to be exactly full
	 * before any Read -- log-only, does not block the read). Clamps the read
	 * to available bytes (mLength-mPosition); if mBuf is non-NULL, memcpy's
	 * from mBuf+mPosition into the caller buffer and advances mPosition, else
	 * soft-asserts ("Memory.cpp" line 0x5e=94) and does nothing further.
	 */
	void Read(void *buf, unsigned int n)
	{
		if (mState != 4)
			ApiAssertMemory(0x53);
		if (mCapacity != mLength)
			ApiAssertMemory(0x54);

		unsigned long avail = mLength - (unsigned long)mPosition;
		unsigned long taken = (n > avail) ? avail : n;
		mLastOpLen = taken;

		if (mBuf) {
			memcpy(buf, mBuf + mPosition, taken);
			mPosition += (long)taken;
		} else {
			ApiAssertMemory(0x5e);
		}
	}

	/* .text+0x080a1220, 280 bytes. Real body: soft-assert if mState!=5
	 * ("Memory.cpp" line 0x67=103). If mPosition+n would exceed mCapacity,
	 * clamps the write length to what fits (real Api+0x90 vtable "warning"
	 * call on truncation -- a DIFFERENT slot than the usual +0x94 assert,
	 * also log-only, not reproduced as a distinct call here since its own
	 * semantics are undocumented elsewhere in this project). If mBuf is
	 * non-NULL, memcpy's from the caller buffer into mBuf+mPosition, advances
	 * mPosition, and grows mLength if needed; else soft-asserts ("Memory.cpp"
	 * line 0x72=114) and does nothing further.
	 */
	void Write(const void *buf, unsigned int n)
	{
		if (mState != 5)
			ApiAssertMemory(0x67);

		unsigned long taken = n;
		if ((unsigned long)mPosition + n > mCapacity)
			taken = mCapacity - (unsigned long)mPosition;
		mLastOpLen = taken;

		if (mBuf) {
			memcpy(mBuf + mPosition, buf, taken);
			mPosition += (long)taken;
			if ((unsigned long)mPosition > mLength)
				mLength = (unsigned long)mPosition;
		} else {
			ApiAssertMemory(0x72);
		}
	}

	/* .text+0x080a1470, 141 bytes. Real body: same shape as CNullStr::Seek,
	 * plus a soft-assert ("Memory.cpp" line 0x4a=74) if the resolved position
	 * exceeds mCapacity. */
	void Seek(long offset, ESeekType type)
	{
		long ref = offset;
		if (!IsSought(ref, type)) {
			mPosition = ref;
			if ((unsigned long)mPosition > mCapacity)
				ApiAssertMemory(0x4a);
		}
	}

	/* .text+0x080a1070, 32 bytes. Real body: resets mPosition and mState to 0
	 * (same as CNullStr::Close). */
	void Close() { mPosition = 0; mState = 0; }

	/* .text+0x080a10a0, 75 bytes. Real body: soft-assert if mState!=5
	 * ("Memory.cpp" line 0x7f=127); no other effect. */
	void Flush()
	{
		if (mState != 5)
			ApiAssertMemory(0x7f);
	}

	/* .text+0x080a1100/0x080a1120/0x080a1140, 3/3/6 bytes. Real bodies: fixed
	 * constants, same as CNullStr's. */
	void AreUsingTheNext(unsigned int) {}
	unsigned int CurrBufferCapacity() const { return 0; }
	bool IsReady() volatile { return true; }

private:
	static void ApiAssertMemory(int line);

	unsigned char *mBuf;       /* +0x08 */
	unsigned long mCapacity;   /* +0x0c */
	int mOwnMode;              /* +0x10 */
};

#endif /* STREAM_FAMILY_H */
