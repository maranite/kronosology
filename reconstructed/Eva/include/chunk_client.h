/*
 * chunk_client.h  -  CChkItem/CDumpReqDescr/CDumpHeaderDescr/CChunkClient, Eva's
 * dump/chunk-transfer request layer. Fresh `nm -C` class-inventory sweep
 * (2026-07-28), following the CKorgKmp/CKorgKsf/CKorgKsc batch (commit a5c7f0d).
 *
 * FOUND VIA: `include/buffering_task.h` (Stage 6, 2026-07-25) already flagged
 * `mChunkClient` (+0xa4, `void*`) as "a genuinely separate, un-reconstructed
 * chunk-transfer client class (out of scope)" and named `CDumpReqDescr`/
 * `CDumpHeaderDescr` as siblings `CBufferingTask::Exec()`/`Put()` pull in but don't
 * need. That's exactly this batch's target: `nm -C` confirms `CChunkClient` (43
 * methods) + `CDumpReqDescr` (9) + `CDumpHeaderDescr` (10) = 62 methods, ALL
 * previously "pending" -- plus `CChkItem` (5 methods, a small TLV blob type
 * `PrepareList()`/`OnPrepareMicro()` both depend on, found while tracing this
 * cluster's own body) = 67 total. `CChunkClient` derives from the ALREADY-real
 * `CTask` (task.h) -- ground truth confirmed real strings "ChunkClient" (task name)
 * and "CmdToChunkMan" (internal outlink name) via direct `.rodata` reads at
 * 0x08e7fdc4/0x08e7fdd0 (the `.data` pointer table at 0x91ae8e0/0x91ae8e4 the ctor's
 * own disassembly loads them from).
 *
 * ARCHITECTURE: `CChunkClient` is an abstract-flavored session/state-machine base
 * for chunk-based save/load transfers (to a file OR to a "dump", Eva's own on-wire
 * resource-transfer format) over a `COutLinkMono` (out_link.h, already real). Every
 * real operation (`SaveFile`/`LoadFile`/`SaveDump`/`LoadDump`/`LoadRes`/`SaveRes`/
 * `MergeRes`) follows the SAME shape: assert idle (`mState==0`, +0xa8), ask the
 * derived class's own `IsXxxToBeExecuted()` hook whether to proceed (vtbl -- base
 * always returns false, i.e. "do nothing" unless overridden), build a small
 * `TPtrArray<CChkItem>`-shaped work list via `PrepareList()`, transition `mState`
 * to an operation-specific tag (1=SaveFile, 2=SaveDump, 3=SaveRes, 4=LoadFile,
 * 5=LoadDump, 6=LoadRes, 7=Abort, 8=StoppedByUser, 9=MergeRes), pack a small
 * big-endian-ish byte message, and hand it to `COutLinkMono::OutMono()` (an ECB
 * code 1-9 matching the state tag, confirmed by direct call-site cross-reference).
 * `Exec(CMessage&)` is the reverse direction: the transfer's remote peer replies
 * with ECB 0xe0-0xe5 messages this dispatches back through the SAME `IsXxx`/`OnXxx`
 * virtual hooks, `FailAndReset()`/`Reset()` tear the session back down.
 *
 * VIRTUAL DISPATCH: `CChunkClient` follows the SAME manual-vtable-as-data-array
 * convention every other `CTask`-derived class in this project uses (task.h/
 * chunk_man.h) -- NOT real C++ `virtual` (that would fight `CTask`'s own raw
 * `mVtbl`/`mIfcThunk` fields, see chunk_man.h's `CChkBaseTask` precedent). Real
 * vtable `PTR__CChunkClient_08e857c8` confirmed by a direct `.rodata` byte read: 26
 * slots (0x00..0x64). Slots 2 (0x08) and 4 (0x10) hold real addresses
 * (0x08180950/0x0807e170) for CTask-family virtuals `CChunkClient` does NOT
 * override -- OUT of this reconstruction's scope (same "inherited, not exercised
 * by any reconstructed code" treatment `PTR__CTask_08e82128` itself already gets,
 * omega_vtables.cpp), modeled as `EvaVTableStub`. Every OTHER slot is a real,
 * fully reconstructed `CChunkClient` method, confirmed by cross-referencing each
 * `vtbl+offset` call site against this table:
 *   0x00 ~CChunkClient (D1)        0x34 OnAcceptedRq
 *   0x04 ~CChunkClient (D0)        0x38 OnByteCount
 *   0x08 [inherited CTask, stub]   0x3c OnInternalAbort
 *   0x0c Exec(CMessage&)           0x40 OnExternalAbort
 *   0x10 [inherited CTask, stub]   0x44 OnStoppedByUser
 *   0x14 OnPrepareSingle           0x48 OnEnd(TObjArray<uchar> const*, CDumpReqDescr const&)
 *   0x18 IsLoadFileToBeExecuted    0x4c OnBegin
 *   0x1c IsSaveFileToBeExecuted    0x50 OnEnd()
 *   0x20 IsLoadDumpToBeExecuted    0x54 OnSingleEnd
 *   0x24 IsSaveDumpToBeExecuted    0x58 OnGetParamForSaveFile
 *   0x28 IsAbortToBeExecuted       0x5c OnGetParamForSaveDump
 *   0x2c IsStoppedByUserToBeExecuted 0x60 OnGetParamForLoadFile
 *   0x30 OnAcceptedHd              0x64 OnGetParamForLoadDump
 * `this+0x08` (`mIfcThunk`, CTask's own field, task.h) is overwritten with a
 * class-specific opaque identity (`&EvaDataPlaceholder_08e857c8x`) the SAME way
 * `CChkBaseTask`/`CChkCmd` already do -- confirmed to be a genuine 3-entry
 * secondary this-adjusted vtable region (2 real non-virtual `this-8` dtor thunks at
 * .text+0x080c8e80/0x080c8f80, both already fully satisfied by the plain D1/D0
 * dtor bodies below, same "not separately modeled" treatment as
 * `CSysExMsgTaskBase::~CSysExMsgTaskBase()`'s own dtor thunk pair -- plus one more
 * inherited-CTask-virtual slot), never dereferenced by any reconstructed code.
 *
 * REAL LAYOUT (0xb0 bytes: CTask's own 0x7c + 0x34 own bytes, confirmed from
 * CChunkClient@080c8f90.c and every real field write across all Tier-A methods):
 *   +0x7c  mCommId       unsigned char, ctor sets 0xff (sentinel) -- the
 *                        `COutLinkMono::OutMono` "ecb"-adjacent identity byte
 *                        packed as the first byte of every outgoing message
 *   +0x80  mOutLinkMono  `COutLinkMono*`, malloc'd+constructed in the ctor
 *                        ("CmdToChunkMan", direction 1, mode 0x8003) and
 *                        registered via the already-real `CTask::Add()`
 *   +0x84  mByteCount    `unsigned long` -- set from a `CDumpHeaderDescr`'s own
 *                        byte-count field in `LoadDump()`, from `CMessage`'s own
 *                        payload dword in `Exec()`'s ECB 0xe1 case, compared
 *                        against in the ECB 0xe3 case (a running transfer's
 *                        "bytes so far" tracker, real meaning of individual reads
 *                        not further decoded beyond "value in progress")
 *   +0x88  mChkItemArray `COmegaPtrArray*` -- lazily malloc'd+constructed
 *                        (growBy=5, cap=5, own=1) then vtable-swapped to whichever
 *                        `TPtrArray<T>` flavor matches the in-flight operation
 *                        (`CChkItem` for Save/LoadFile/Dump, `CLoadResElem` for
 *                        LoadRes, `CSaveResElem` for SaveRes, `CMergeElem` for
 *                        MergeRes -- 4 distinct real `.rodata` vtable addresses,
 *                        all install-only per this project's established
 *                        "vtable-slot dispatch modeled abstractly" convention,
 *                        omega_ptr_array.h). Freed via its own vtbl+4 "self-
 *                        deleting" slot in `Reset()`/`FailAndReset()`/dtor.
 *   +0x8c  mScratch      small malloc'd 4-dword helper block (real ground truth:
 *                        `[5][0][5][malloc(5)]`, i.e. `{growBy=5, count=0,
 *                        capacity=5, data}` -- NOT a `COmegaPtrArray` (no vtable
 *                        slot, different field order), meaning of the constant 5
 *                        not further decoded, but the malloc/free lifecycle is
 *                        faithfully reproduced (see `SScratch5` below)
 *   +0x90  mHeader       embedded `CDumpHeaderDescr` (0x18 bytes), the in-flight
 *                        operation's own request/reply header, copied in via
 *                        `operator=` at the start of every Save/Load method
 *   +0xa8  mState        `int`, the session state tag documented above (0=idle)
 *   +0xac  mPendingCount `int`, ctor sets 0 -- incremented right before every
 *                        `OutMono()` call, decremented (and the whole session torn
 *                        down when it reaches 0) by `FailAndReset()`
 *
 * `OpenSubChunk(CResourceChunk*, unsigned char)`/`CloseSubChunk(CResourceChunk*,
 * CChunk*)` (.text+0x080cb480/0x080cb5f0) are Tier B (real signature, minimal
 * body) -- both genuinely dereference `CResourceChunk`'s own fields (+0xc, +0x44)
 * and call through `CChunkRootWithSeek::SeekToSubChunk`/its own vtbl+0x14/0x1c --
 * `CChunkRootWithSeek` is chunk_family.h's own already-documented, deliberately
 * DEFERRED sibling (real, not yet reconstructed anywhere in this project), and
 * `CResourceChunk` is a brand-new undocumented class this batch didn't chase
 * further. Same "small reconstructed shell, deep out-of-scope dependency"
 * boundary as `CBufferingTask::Exec()`/`Put()` (buffering_task.h). Every OTHER
 * method that takes a `CResourceChunk*` (`LoadRes`/`MergeRes`/`LoadResSync`) only
 * ever passes the pointer through OPAQUELY (packed into an outgoing message buffer
 * or forwarded to `ChkApi`'s own vtbl+0x40 -- already-real global, mains.cpp) --
 * confirmed by direct read of every one of those methods' own bodies, so they stay
 * Tier A with `CResourceChunk` forward-declared and never dereferenced.
 *
 * `CMessage` stays opaque (no reconstructed layout anywhere in this project yet,
 * same status sysex_msg_task_base.h's own `Exec(CMessage&)` already established) --
 * `Exec(CMessage&)` below accesses it via the SAME raw-offset convention as
 * `CSysExMsgTaskBase::Exec(CMessage&)`: +0x08 a 2-byte flags/command field (bit
 * 0x100 = "has an extended command byte", low byte = the command itself), +0x10 a
 * 4-byte payload dword (reused as either an integer value or an aliased pointer
 * depending on the command).
 */

#ifndef CHUNK_CLIENT_H
#define CHUNK_CLIENT_H

#include "task.h"

class CModule;
class CMessage;
class CResourceChunk;
class CChunk;
class COmegaPtrArray;
class COutLinkMono;

/* CChkItem  -  a small TLV blob: [type][len][len bytes of malloc'd payload].
 * Real layout (6 bytes fixed + malloc'd payload), confirmed from
 * CChkItem@080c8b60.c and its 4 sibling methods (all .text+0x080c8b60..0x080c8d60,
 * "ChkItem.cpp" ground-truth filename via direct .rodata Assertion-string reads).
 * `PrepareList()`/`OnPrepareMicro()` below are its only real callers found this
 * batch.
 */
class CChkItem {
public:
	/* .text+0x080c8bf0, 19 bytes. Default ctor: mType=0xff (sentinel), mLen=0,
	 * mData=0.
	 */
	CChkItem();

	/* .text+0x080c8b60, 135 bytes. Real: mallocs+memcpy's a `len`-byte copy of
	 * `data` (asserted non-NULL).
	 */
	CChkItem(unsigned char type, unsigned char len, const unsigned char *data);

	/* .text+0x080c8c10, 33 bytes. */
	~CChkItem();

	/* .text+0x080c8c40, 164 bytes. Writes [type][len][payload] to `out`, returns
	 * len+2.
	 */
	int Serialize(unsigned char *out) const;

	/* .text+0x080c8cf0, 170 bytes. Reads [type][len] from `in`, mallocs+memcpy's
	 * the payload. Real: asserts mData==0 beforehand (fresh/reset object only).
	 */
	void DeSerialize(const unsigned char *in);

private:
	unsigned char  mType; /* +0x00 */
	unsigned char  mLen;  /* +0x01 */
	unsigned char *mData; /* +0x04 */

	friend struct ChunkClientTestHooks;
};

/* CDumpReqDescr  -  a real, tractable single-inheritance hierarchy (CDumpReqDescr
 * -> CDumpHeaderDescr), unlike CChunkClient itself -- uses ordinary C++ `virtual`
 * (dtor/Serialize/DeSerialize/Reset), confirmed by a direct .rodata byte read of
 * PTR__CDumpReqDescr_08e85a60/PTR__CDumpHeaderDescr_08e85a20: both real 5-slot
 * vtables (D1/D0/Serialize/DeSerialize/Reset), CDumpHeaderDescr overriding EVERY
 * slot -- same "plain single-base override chain" shape as chunk_family.h's own
 * CChunkBase/CChunk (which already established real `virtual` as this project's
 * convention for that shape). `SetMicro`/`SetSingle`/`operator=` are NOT in either
 * vtable (never dispatched virtually at any real call site traced this batch) --
 * ordinary non-virtual member functions, each overridden (name-hidden, not
 * polymorphic) in CDumpHeaderDescr with an extra trailing `unsigned long` arg.
 *
 * Real layout (0x14 bytes), confirmed from every field write across all 9 own
 * methods:
 *   +0x00  vtbl
 *   +0x04  mType        int: 0=none, 1=Micro (mMicroId valid), 2=Single
 *                        (mResourceId valid) -- ground-truth enum not decoded,
 *                        real values only
 *   +0x08  mResourceId  int (real: `CDumpReqDescr::EResource`, enum not decoded,
 *                        opaque) -- valid only when mType==2
 *   +0x0c  mMicroId     unsigned char -- valid only when mType==1, ctor/Reset set
 *                        0xff otherwise
 *   +0x0d  mLen         unsigned char
 *   +0x10  mData        unsigned char* -- malloc'd payload, length mLen
 */
class CDumpReqDescr {
public:
	/* .text+0x080cce10, 40 bytes. */
	CDumpReqDescr();

	/* .text+0x080ccb60 (D1)/0x080cca80 (D0), 46/78 bytes. Real: frees mData
	 * (D0 additionally frees `this`, matching every other deleting-destructor
	 * in this project).
	 */
	virtual ~CDumpReqDescr();

	/* .text+0x080ccaf0, 83 bytes. Virtual (vtbl slot 4) -- CDumpHeaderDescr's
	 * own override chains into this via an explicit qualified call, then
	 * clears its own +0x14 field.
	 */
	virtual void Reset();

	/* .text+0x080cc980, 241 bytes (virtual, vtbl slot 2). Real: writes
	 * [mType][mMicroId-or-mResourceId][mLen][payload] to `out` (asserted
	 * non-NULL), returns mLen+3 (soft-capped against `maxLen`). Returns 0 if
	 * mType is neither 1 nor 2.
	 */
	virtual unsigned Serialize(unsigned char *out, unsigned char maxLen) const;

	/* .text+0x080ccbb0, 559 bytes (virtual, vtbl slot 3). Real: calls this
	 * object's own (possibly derived) Reset() first, then reads [mType]
	 * [mMicroId-or-mResourceId][mLen] from `in`, mallocs+memcpy's the payload.
	 * Returns mLen+3, or 0 if the type byte is neither 1 nor 2. Ground truth's
	 * own soft-assert branches (min-length/max-length/mData==NULL checks) are
	 * real but non-diverting (log-only, same "Assertion failed" no-op-on-valid-
	 * input treatment used throughout this project) -- omitted here, the real
	 * data-movement result is identical either way.
	 */
	virtual unsigned DeSerialize(const unsigned char *in, unsigned char len);

	/* .text+0x080cce40, 378 bytes. Real: mType=1, mMicroId=id, mResourceId=0,
	 * mLen=len, mallocs+memcpy's `data` (only if len!=0).
	 */
	void SetMicro(unsigned char id, unsigned char len, const unsigned char *data);

	/* .text+0x080ccfe0, 361 bytes. Real: mType=2, mResourceId=resource,
	 * mMicroId=0xff, mLen=len, mallocs+memcpy's `data` (only if len!=0). Real:
	 * `resource` is CDumpReqDescr::EResource, opaque int here.
	 */
	void SetSingle(int resource, unsigned char len, const unsigned char *data);

	/* .text+0x080cd170, 179 bytes. Real deep copy (frees own mData, mallocs a
	 * fresh copy of `other`'s).
	 */
	CDumpReqDescr &operator=(const CDumpReqDescr &other);

	/* Trivial public accessors -- not separate real methods (ground truth
	 * reads these fields via raw offsets at their one real caller,
	 * CChunkClient::PrepareList()/OnPrepareMicro(), chunk_client.cpp), added
	 * for the same reason COmegaPtrArray::Count()/Get() were (omega_ptr_array.h):
	 * CChunkClient is a sibling, not a subclass, so it cannot reach protected
	 * fields directly.
	 */
	int GetType() const { return mType; }
	unsigned char GetMicroId() const { return mMicroId; }
	int GetResourceId() const { return mResourceId; }
	unsigned char GetLen() const { return mLen; }
	const unsigned char *GetData() const { return mData; }

protected:
	int            mType;       /* +0x04 */
	int            mResourceId; /* +0x08 */
	unsigned char  mMicroId;    /* +0x0c */
	unsigned char  mLen;        /* +0x0d */
	unsigned char *mData;       /* +0x10 */

	friend struct ChunkClientTestHooks;
};

/* CDumpHeaderDescr : public CDumpReqDescr  -  adds a single trailing
 * `unsigned long mByteCount` (+0x14), big-endian-serialized (same convention as
 * SChkHeader/EXs auth, see CLAUDE.md), 0x18 bytes total.
 */
class CDumpHeaderDescr : public CDumpReqDescr {
public:
	/* .text+0x080cc860, 36 bytes. */
	CDumpHeaderDescr();

	/* .text+0x080cc830 (D0)/0x080cc810 (D1), 37/23 bytes. */
	virtual ~CDumpHeaderDescr();

	/* .text+0x080cc7f0, 30 bytes. Chains CDumpReqDescr::Reset(), then clears
	 * mByteCount.
	 */
	virtual void Reset();

	/* .text+0x080cc700, 232 bytes. Chains CDumpReqDescr::Serialize() into a
	 * buffer 4 bytes shorter than `maxLen` (reserving room for mByteCount),
	 * then appends mByteCount big-endian. Returns 0 if the base call does (same
	 * "type byte neither 1 nor 2" early-out).
	 */
	virtual unsigned Serialize(unsigned char *out, unsigned char maxLen) const;

	/* .text+0x080cc610, 232 bytes. Mirror-image of Serialize() above. */
	virtual unsigned DeSerialize(const unsigned char *in, unsigned char len);

	/* .text+0x080cc890, 56 bytes. Chains CDumpReqDescr::SetMicro(), then sets
	 * mByteCount.
	 */
	void SetMicro(unsigned char id, unsigned char len, const unsigned char *data,
	              unsigned long byteCount);

	/* .text+0x080cc8d0, 55 bytes. Chains CDumpReqDescr::SetSingle(), then sets
	 * mByteCount.
	 */
	void SetSingle(int resource, unsigned char len, const unsigned char *data,
	               unsigned long byteCount);

	/* .text+0x080cc910, 44 bytes. Real: `CDumpReqDescr const&` overload --
	 * chains CDumpReqDescr::operator=(), then zeroes mByteCount (NOT copied,
	 * since the source has no such field).
	 */
	CDumpHeaderDescr &operator=(const CDumpReqDescr &other);

	/* .text+0x080cc940, 57 bytes. Real: CDumpHeaderDescr const& overload --
	 * chains CDumpReqDescr::operator=(), then DOES copy mByteCount.
	 */
	CDumpHeaderDescr &operator=(const CDumpHeaderDescr &other);

	/* Trivial public accessors, same justification as CDumpReqDescr's own
	 * (above) -- CChunkClient::LoadDump()/Exec() (chunk_client.cpp) read/write
	 * this field directly at ground-truth's own `this+0x14`/`this+0xa4`
	 * (mHeader's embedded offset) raw offsets.
	 */
	unsigned long GetByteCount() const { return mByteCount; }
	void SetByteCount(unsigned long v) { mByteCount = v; }

private:
	unsigned long mByteCount; /* +0x14 */

	friend struct ChunkClientTestHooks;
};

/* CChunkClient : public CTask  -  see file header for the full design writeup. */
class CChunkClient : public CTask {
public:
	/* .text+0x080c8f90, 248 bytes. */
	explicit CChunkClient(const CModule &owner);

	/* .text+0x080c8db0 (D1)/0x080c8e90 (D0), 143/161 bytes (+2 real non-virtual
	 * `this-8` thunks, .text+0x080c8e80/0x080c8f80, not separately modeled --
	 * see file header).
	 */
	~CChunkClient();

	/* .text+0x080c90d0, 89 bytes. Real: asks IsAbortToBeExecuted() (vtbl+0x28,
	 * base always false); if true and a session is active, sets mState=7 and
	 * sends OutMono(ecb=5).
	 */
	bool Abort();

	/* .text+0x080c9130, 89 bytes. Mirror-image of Abort(), state 8, ecb=6. */
	bool StoppedByUser();

	/* .text+0x080c9330, 824 bytes. */
	bool SaveDump(const CDumpReqDescr &req);

	/* .text+0x080c96d0, 880 bytes. */
	bool LoadDump(const CDumpHeaderDescr &req);

	/* .text+0x080c9aa0, 1101 bytes. */
	bool SaveFile(const char *name, const CDumpReqDescr &req);

	/* .text+0x080c9f50, 1126 bytes. */
	bool LoadFile(const char *name, const CDumpHeaderDescr &req);

	/* .text+0x080caa40, 689 bytes. */
	bool LoadRes(CResourceChunk *chunk, const COmegaPtrArray &elems, int extra);

	/* .text+0x080cad40, 120 bytes. Real: calls OnBegin() then forwards
	 * (mCommId, chunk, elems) opaquely through ChkApi's own vtbl+0x40 (already-
	 * real global, mains.cpp) -- `chunk`/`elems` never dereferenced here.
	 */
	void LoadResSync(CResourceChunk *chunk, const COmegaPtrArray &elems);

	/* .text+0x080cadc0, 719 bytes. */
	bool SaveRes(const char *name, const COmegaPtrArray &elems);

	/* .text+0x080cb0e0, 851 bytes. */
	bool MergeRes(CResourceChunk *a, CResourceChunk *b, const COmegaPtrArray *elems,
	              unsigned long flags);

	/* .text+0x080ca720, 794 bytes. Overrides CTask's own Exec(CMessage&) (vtbl
	 * slot 3, offset 0x0c). Dispatches ECB 0xe0..0xe5 back through the IsXxx/
	 * OnXxx hooks -- see file header.
	 */
	int Exec(CMessage &msg);

	/* .text+0x080ca420, 133 bytes. Real: builds a single CChkItem from `req`'s
	 * own micro id/len/data (same body shape as PrepareList()'s own mType==1
	 * branch, but NOT called from PrepareList() -- a real, separately reachable
	 * method with no traced internal caller in this batch's own call graph
	 * (matches this project's "real and reachable in the actual binary" bar,
	 * task.h). Public since nothing in this class calls it.
	 */
	int OnPrepareMicro(const CDumpReqDescr *req, COmegaPtrArray &out);

	/* .text+0x080cb480, 347 bytes. Tier B -- see file header. Real signature
	 * only; returns 0 (not found/not applicable).
	 */
	int OpenSubChunk(CResourceChunk *chunk, unsigned char type);

	/* .text+0x080cb5f0, 165 bytes. Tier B -- see file header. Real signature
	 * only; returns false.
	 */
	bool CloseSubChunk(CResourceChunk *chunk, CChunk *sub);

	/* ---- Virtual hooks (vtbl-dispatched, see file header's slot map). Base
	 * class bodies are the REAL ground-truth defaults -- every "IsXxx" returns
	 * false (never exercised) and every "OnXxx" either no-ops or asserts a
	 * state-machine invariant (assert calls themselves not modeled, same
	 * established "log-only, no control-flow effect" omission used throughout
	 * this project); a real derived class would override these to do actual
	 * work. Public so a derived class can override / a test can call them
	 * directly. ----
	 */
	int OnPrepareSingle(const CDumpReqDescr *req, COmegaPtrArray &list);
	bool IsLoadFileToBeExecuted(const char *name, const CDumpHeaderDescr &req) const;
	bool IsSaveFileToBeExecuted(const char *name, const CDumpReqDescr &req) const;
	bool IsLoadDumpToBeExecuted(const CDumpHeaderDescr &req) const;
	bool IsSaveDumpToBeExecuted(const CDumpReqDescr &req) const;
	bool IsAbortToBeExecuted() const;
	bool IsStoppedByUserToBeExecuted() const;
	void OnAcceptedHd(const CDumpHeaderDescr &req);
	void OnAcceptedRq(const CDumpHeaderDescr &req);
	void OnByteCount(unsigned long count);
	void OnInternalAbort(const CDumpReqDescr &req);
	void OnExternalAbort(const CDumpReqDescr &req);
	void OnStoppedByUser(const CDumpReqDescr &req);

	/* Real first arg (per the Exec() ECB-0xe5 call site, vtbl+0x48) is `this`'s
	 * own +0x8c scratch block (see mScratch below) -- a `TObjArray<unsigned
	 * char>`-shaped value array, NOT a `COmegaPtrArray`/`TPtrArray<T>` (no
	 * vtable slot at that shape, matching this project's existing
	 * TObjArray/TPtrArray/TNamedPtrArray sibling-template distinction,
	 * omega_vtables.h) -- kept opaque `void*` here since no `TObjArray<T>`
	 * template is reconstructed anywhere in this project yet.
	 */
	void OnEnd(const void *list, const CDumpReqDescr &req);
	void OnBegin();
	void OnEnd();
	void OnSingleEnd(COmegaPtrArray *list);
	unsigned long OnGetParamForSaveFile() const;
	unsigned long OnGetParamForSaveDump() const;
	unsigned long OnGetParamForLoadFile() const;
	unsigned long OnGetParamForLoadDump() const;

	/* NOTE: deliberately NOT `virtual` -- CChunkClient's REAL dispatch is the
	 * manual PTR__CChunkClient_08e857c8 array (chunk_client.cpp), matching
	 * every other CTask-derived class in this project (CTask's own `mVtbl`
	 * field, task.h, stays the single source of truth). Giving these real C++
	 * `virtual` would inject a compiler-generated vptr -- since CTask itself
	 * declares no virtuals, that vptr would land at offset 0 of the WHOLE
	 * object, shifting every one of CTask's own raw-offset fields (and every
	 * field below) by 4 bytes and breaking the byte-exact layout match. Each
	 * vtbl slot below is instead served by an explicit `extern "C"` forwarder
	 * function (chunk_client.cpp) that calls the plain method directly.
	 */

private:
	/* Reset()/FailAndReset()'s shared "tear the in-flight session back down"
	 * tail, and PrepareList()/OnPrepareMicro()'s shared "append one CChkItem"
	 * body -- both real, both used by 2+ public methods above.
	 */
	void Reset();
	void FailAndReset();
	int PrepareList(const CDumpReqDescr *req, COmegaPtrArray &out);

	unsigned char   mCommId;       /* +0x7c */
	COutLinkMono   *mOutLinkMono;  /* +0x80 */
	unsigned long   mByteCount;    /* +0x84 */
	COmegaPtrArray *mChkItemArray; /* +0x88 */
	void           *mScratch;      /* +0x8c, see file header ("SScratch5") */
	CDumpHeaderDescr mHeader;      /* +0x90, 0x18 bytes */
	int             mState;        /* +0xa8 */
	int             mPendingCount; /* +0xac */

	friend struct ChunkClientTestHooks;
};

#endif /* CHUNK_CLIENT_H */
