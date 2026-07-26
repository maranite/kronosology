/*
 * batch_disk_main_task.h  -  CBatchDiskMainTask. Eva "size is not depth"
 * re-check batch, 2026-07-26 -- direct follow-up to
 * [[eva_batchdiskman_unlock_2026-07-26]], which deferred this class's own
 * ctor as a Tier-B substitute because it directly placement-constructs a real
 * `CZ` member (the 247-method string-set container, project-wide out of
 * scope, config_manager.h's `CreateResourceFamilies()`). Re-traced via a
 * fresh `objdump -dr -M intel` read of `CBatchDiskMainTask::CBatchDiskMainTask()`
 * (.text+0x08241920, 450 bytes) with the specific question "is the ctor's OWN
 * logic tractable if CZ stays opaque?" -- answer: YES. Every single
 * instruction in the real ctor is either (a) a subobject constructor call
 * (CTask, CEditable, CRMApiCallBack [inlined, trivial], CRMJob, CDirEntry, CZ,
 * COutLinkMulti -- rm_job.h/rm_api_callback.h/dir_entry.h/out_link.h, all new
 * or extended this batch, all themselves confirmed equally mechanical/small
 * on their own re-trace) or (b) a literal field-immediate store -- no loops,
 * no branches outside the standard EH landing pads. The ONLY genuinely
 * un-followed dependency is `CZ`'s own internals, exactly as opaque here as
 * everywhere else in this project.
 *
 * CORRECTED PRIOR CLAIM: [[eva_batchdiskman_unlock_2026-07-26]] characterized
 * this class as "confirmed via 3 real vtable-pointer stores at this+0/
 * this+8/this+0x80" therefore "genuinely IS triple-inheritance, unlike
 * CEditTask" -- half right, half wrong. The this+0x80 store IS a real 3rd
 * base (`CRMApiCallBack`, rm_api_callback.h, confirmed via matching
 * `_ZThn128_` dtor thunks). But the this+8 store is NOT a `CEditable` vtable
 * slot -- `CEditable` has no vtable of its own (editable.h's own header
 * comment, already established). It is `CTask`'s OWN internal `mIfcThunk`
 * field (task.h, +0x08) getting re-installed to this class's own identity --
 * the EXACT SAME "size is not depth" mistake `CEditTask`'s own header comment
 * (edit_task.h) already corrected for itself ("that '+8' is CTask's OWN
 * internal 'mIfcThunk' secondary vtable slot... NOT a separate base class").
 * `CEditable` (0x7c..0x80) IS still a real base here (confirmed: ground truth
 * calls `CEditable::CEditable(this+0x7c, ...)` and later
 * `CEditable::AddDescriptorsMap((CObjectBase*)this, ...)`, same as CEditTask)
 * -- so the real inheritance is `CTask, CEditable, CRMApiCallBack` (3 bases,
 * declaration order), giving the compiler's own natural sequential MI layout
 * CTask(0x7c) + CEditable(4) + CRMApiCallBack(8) = 0x88, matching every real
 * offset below with zero manual offset trickery -- same "let real multiple
 * inheritance do the layout work" convention `CBatchDiskMan`'s own `CModule,
 * CEditServer` bases already use (batch_disk_man.h).
 *
 * REAL LAYOUT (from CBatchDiskMainTask::CBatchDiskMainTask()/~CBatchDiskMainTask()):
 *   +0x00..0x7c   CTask base (task.h) -- primary vtable group, install
 *                 0x08eabec8 (8 slots)
 *   +0x7c..0x80   CEditable base (editable.h, no vtable of its own) --
 *                 secondary group install 0x08eabee8 (CTask's own mIfcThunk
 *                 identity, 3 slots -- see above)
 *   +0x80..0x88   CRMApiCallBack base (rm_api_callback.h) -- tertiary group
 *                 install 0x08eabefc (7 slots, matching CRMApiCallBack's own
 *                 7 named methods exactly); owns the heap `CRMJob`
 *                 (rm_job.h, malloc(0x54)) at its own +4 (absolute +0x84)
 *   +0x88..0x8c   unknown (never touched by the ctor)
 *   +0x8c  int  = 0
 *   +0x90  int  = 0
 *   +0x94  byte = 0xff
 *   +0x95  byte = 0
 *   +0x96..0xaa   unknown (never touched by the ctor)
 *   +0xaa  byte = 0
 *   +0xab..0xc0   unknown (never touched by the ctor)
 *   +0xc0  mOutLink       COutLinkMulti* (out_link.h) -- heap `malloc(0x34)`,
 *                 `COutLinkMulti::COutLinkMulti(*this, "Signals"
 *                 (0x8eabe7d), direction=0, mode=0x8031)`, registered via
 *                 `CTask::Add()`. Ground truth then overwrites the freshly
 *                 installed vtable with `vtable for CBatchDiskSignals + 8`
 *                 (0x08eabf88) -- i.e. the real runtime type is
 *                 `CBatchDiskSignals*` (a COutLinkMulti-derived class, own
 *                 further state never touched by anything in this
 *                 reconstruction's own traced call graph). Modeled as plain
 *                 `COutLinkMulti` -- the SAME documented simplification
 *                 `CEditTask::mOutLink`'s own header comment already
 *                 establishes for the analogous `CBatchDiskCmds` case
 *                 (edit_task.h).
 *   +0xc4  mGroupListHead  int, = 0 (a job/group-list head pointer read back
 *                 by `IsPreloadRunning(unsigned char, const char*)`'s own
 *                 real body -- see below; stays 0 here since the methods
 *                 that would populate it, PreloadDir()/PreloadGroup()/
 *                 AddItemToPreload(), remain deferred)
 *   +0xc8  mUnknownVec     opaque `TVector<int,1>`-shaped raw buffer (0x10
 *                 bytes: vtbl + 3 zeroed pointers -- confirmed real identity
 *                 via `.rodata` value 0x8e86f78 == "vtable for
 *                 TVector<int,1>"'s own slot0, symbols.csv). Modeled the same
 *                 "raw offset, install vtable pointer, never dereferenced"
 *                 convention as `COutLink::mLinks` (out_link.h) -- the real
 *                 template container is out of scope, only its
 *                 default-construct-to-empty ctor behavior is reproduced.
 *                 UPDATE (`PreloadDir()`/`OnLoad()` investigation,
 *                 2026-07-26): the 3 zeroed pointers at +4/+8/+12 within this
 *                 buffer (absolute +0xcc/+0xd0/+0xd4) are confirmed to be a
 *                 real `{begin, end, capacity_end}` triple -- `PreloadDir()`
 *                 and `OnLoad()` both read/write `+0xcc`/`+0xd0` directly
 *                 (bypassing any real `TVector` method) as an int-handle
 *                 push/pop stack of open `FMApi::FindFirstFile()` handles.
 *                 Still correctly left as an opaque zeroed buffer here --
 *                 nothing reconstructed ever pushes a real handle onto it
 *                 (that would require `PreloadDir()`'s own genuinely deep
 *                 body, see below), so `begin == end == 0` always holds.
 *   +0xd8  mState          int, = 0 -- THE real field `IsBusy()`/
 *                 `IsPreloadRunning()` (0-arg) read back (see below).
 *   +0xdc  int  = 0
 *   +0xe0  int  = -1 (set LAST, after PrepareGroupsForPreload())
 *   +0xe4  mDirEntry       CDirEntry (dir_entry.h, 0x68 bytes)
 *   +0x14c mCZ             CZ (cz_util.h, opaque, 0x10 bytes) -- the
 *                 out-of-scope dependency this class was originally deferred
 *                 for
 *   +0x15c byte = 0xff (set LAST, after PrepareGroupsForPreload())
 * Total 0x160 (352) bytes, confirmed via `CBatchDiskMan::Setup()`'s own real
 * `malloc(sizeof(CBatchDiskMainTask) = 0x160)` call site (batch_disk_man.h)
 * -- matches exactly (0x15c + 1 byte field, rounds to the next 4-byte
 * boundary = 0x160, zero slack).
 *
 * `CBatchDiskMainTask::CBatchDiskMainTask(const CModule &owner, const char
 * *preloadList)` real body, in order: `CTask::CTask(owner, ...)`;
 * `CEditable::CEditable((CEditServer*)((const char*)&owner + 0x2c))` (the
 * SAME "+0x2c to reach the owner's own CEditServer subobject" trick
 * CEditTask's own ctor uses, edit_task.h -- `owner` here is really the
 * embedded `CModule` subobject of the caller's own `CBatchDiskMan`,
 * batch_disk_man.h); `CRMApiCallBack()` (trivial, inlined); heap `CRMJob()`
 * (rm_job.h); 3 vtable installs; `mUnknownVec` vtable-only init; `CDirEntry()`;
 * `CZ(1)`; heap `COutLinkMulti()` + `CTask::Add()`; `mState = 0`;
 * `CEditable::AddDescriptorsMap((CObjectBase*)this,
 * descCBatchDiskMainTask, false)` (own real data table,
 * `.data+0x091b7300` -- NOT transcribed, same "single real well-formed
 * sentinel-only SDescriptor[1]" placeholder convention CEditTask's own
 * `descCEditTask` already established, edit_task.cpp); the field-init tail
 * above; `PrepareGroupsForPreload(preloadList)` (own real ctor-time call,
 * still deferred -- see below).
 *
 * `PrepareGroupsForPreload(const char*)` (.text+0x08241340, 1336 bytes),
 * `PreloadGroup()` (.text+0x08241d10, 1148 bytes), `PreloadDir()`
 * (.text+0x082421f0, 0xc40 = 3136 bytes), `AddItemToPreload()`
 * (.text+0x08241b80, 359 bytes), `Exec(CMessage&)` (.text+0x08243020,
 * 703 bytes) are the real, genuinely deep, CZ/CRMJob-driven business logic
 * this class was originally deferred for -- STILL Tier-B stubs (real
 * signatures declared, empty bodies), unchanged verdict. `Exec()` (0-arg,
 * .text+0x08242ea0, 377 bytes) is the per-tick task body, same status.
 *
 * UPDATE (dedicated `PreloadDir()` reconstruction attempt, 2026-07-26): fully
 * disassembled (`objdump -dr -M intel --start-address=0x082421f0
 * --stop-address=0x08242e30`, the next real symbol) to answer "is this
 * genuinely deep, or just big" -- **genuinely deep, confirmed with concrete
 * evidence, not deferred on suspicion**. Real dependencies, each independently
 * cross-checked via a direct `.rodata` vtable byte read + `nm -C` resolve
 * (same method as every other Api-vtable slot identification in this
 * project):
 *   - `CZ::Insert(char, unsigned)` / `Insert(const char*, unsigned)` /
 *     `RFind(char, unsigned) const` / `Remove(unsigned, unsigned)`, plus the
 *     `CZ(const char*, unsigned)` and `CZ(const CZ&, unsigned)` constructors
 *     (all `.text+0x80ba6xx-0x80bcxxx` range) -- real string-building logic
 *     (appending `\`, trimming a leading `.`/extension, path-joining) that
 *     needs actual `CZ` container semantics, not just the 2 raw-offset reads
 *     `CDirEntry::GetName()`/`GetExt()`/`HasValidLongNameExt()` needed (see
 *     dir_entry.h, cz_util.h) -- this is exactly the 247-method container
 *     `config_manager.h`'s `CreateResourceFamilies()` already declared out of
 *     scope project-wide.
 *   - `FMApi`'s own vtable slots `+0x270`/`+0x274`/`+0x278` -- confirmed (via
 *     `CFMApiInstance`'s real vtable at 0x08e87200, address-point
 *     0x08e87208) to be `FindFirstFile(char const*, int*, CDirEntry*)`
 *     (0x0811fd50), `FindNextFile(int, CDirEntry*)` (0x0811d420), and
 *     `FindClose(int)` (0x0810e660) -- a real, stateful directory-scan
 *     iterator this function drives (open handle stored/popped from
 *     `mUnknownVec`'s own raw begin/end pointer pair, see below).
 *   - `RMApi`'s own vtable slots `+0xf0`/`+0xf4`/`+0xf8`/`+0xfc`/`+0x100`/
 *     `+0x110` -- confirmed (via `CRMApiInstance`'s real vtable at
 *     0x08e88c40, address-point 0x08e88c48, cross-checked against each
 *     call site's own real argument count) to be, respectively,
 *     `GetNumPositions(uchar,uint)` (0x08164e80), `GetMinAbsBank(uchar,uint)`
 *     (0x081624e0), `GetBankOffset(uchar,uint)` (0x08165080),
 *     `GetFileName(uchar,uint,int&)` (0x081652e0), `GetExt(uchar,uint)`
 *     (0x08162f30), and `GetAbsBank(uchar,uint,uchar)` (0x08164cf0, not
 *     directly seen called in `PreloadDir()` itself but resolved from the
 *     same vtable read) -- real resource-manager bank/position lookups used
 *     to check whether the
 *     current `CDirEntry` is already a known loaded item, tied to the exact
 *     same `CResMan`/`CJobStack` god-object subsystem `CRMJob`'s own recheck
 *     already flagged out of scope (see `rm_job.h`'s header comment).
 *   - `TPtrArray<SLoadBankOffset>` (own real mangled dtor
 *     `_ZN9TPtrArrayI15SLoadBankOffsetED1Ev`, 0x0818f2c0) and
 *     `COmegaPtrArray`/`COmegaPtrArray::Add`/`::Destroy` (0x080a6c10/
 *     0x080a6da0/0x080a6ca0) -- a real grow-by-doubling array-of-3-byte-
 *     records container (`0xaaaaaaab`-reciprocal-multiply divide-by-3
 *     pattern, .text+0x8242a63-0x8242cfe) built to accumulate matched
 *     preload-group entries -- genuinely new container machinery, not
 *     mechanical repetition of anything already reconstructed.
 *   - `TVector<int,1>::Append(int const*, int const*)` (0x0818b1d0) on
 *     `mUnknownVec` (+0xc8) -- confirmed this field's own real identity: a
 *     3-pointer `{vtbl, begin, end}`-shaped buffer (not just "vtbl + 3
 *     zeroed pointers" as originally guessed) -- `begin`/`end` are this
 *     function's OWN open-`FindFirstFile()`-handle stack, read/written via
 *     raw pointer arithmetic at `this+0xcc`/`this+0xd0` (bypassing any real
 *     `TVector` method call for push/pop, only `Append()` for the one
 *     "insert result" case) -- still correctly modeled as opaque, since nothing
 *     reconstructed populates it (see `mUnknownVec`'s own field comment
 *     below).
 * All of the above are genuinely new, real business logic depending on 2
 * subsystems (the `CZ` string container, the `CResMan`/`CJobStack`
 * resource-manager god-object) BOTH already independently declared
 * project-wide out of scope elsewhere -- this is not a "size is not depth"
 * case; the size comes from real, non-repetitive logic, not
 * table-driven/mechanical duplication. Left as Tier-B stubs, matching every
 * other CZ-container-blocked function in this project.
 *
 * What WAS extracted from this investigation (not wasted): the first ~0x120
 * bytes of `PreloadDir()`, before it reaches any `CZ`-internal-writing call,
 * make 9 plain (non-virtual `call`, not vtable-dispatched) `CDirEntry`
 * predicate/accessor calls on `this->mDirEntry`, all of which turned out to
 * be self-contained raw-offset reads needing no `CZ` semantics --
 * `IsEmpty()`/`IsDeleted()`/`IsReserved()`/`IsLabel()`/`IsDir()`/
 * `IsParentDir()`/`IsCurrentDir()`/`HasValidLongNameExt()`/`GetName()`/
 * `GetExt()`, now reconstructed (dir_entry.h/.cpp) -- see that header's own
 * updated comment.
 *
 * Also found and FIXED while tracing `PreloadDir()`'s own neighbor
 * `CBatchDiskMainTask::OnLoad(int)` (.text+0x08242e30, `_ZThn128_` variant
 * 0x08242e90 -- the `CRMApiCallBack` virtual override for THIS class): a
 * stale documentation claim in `rm_api_callback.h` that "ground truth's own
 * real bodies for all 5 OnXxx slots ARE the empty no-op EvaVTableStub
 * already provides, confirmed byte-for-byte" -- true for 4 of 5
 * (`OnSetRes`/`OnLoadRes`/`OnSave`/`OnDelete`) but NOT `OnLoad`, which this
 * class genuinely overrides with real logic (increments `mUnknown8c`/
 * `mUnknown90` depending on the result, then calls `FMApi->FindNextFile()`
 * on the last handle in `mUnknownVec` and tail-calls `PreloadDir()` again).
 * `OnLoad()` itself is NOT reconstructed here -- it has no reachable caller
 * in this project's own call graph (only `CResMan`/`CJobStack`'s real
 * `LoadRes()`-family, out of scope, would ever dispatch it), and its real
 * body reads `mUnknownVec`'s always-zero handle pointer with a `[ptr - 4]`
 * dereference that would only be valid once `PrepareGroupsForPreload()`/
 * `PreloadGroup()` have actually populated it -- writing that logic now
 * would add a genuinely dereference-a-null-derived-address hazard for zero
 * present benefit. `PTR__CBatchDiskMainTask_08eabefc[4]` stays
 * `EvaVTableStub`-backed with a corrected comment (see rm_api_callback.h)
 * rather than the previous, now-disproven blanket claim.
 *
 * `IsBusy() const` (.text+0x08241230, 18 bytes): real, literal
 * `return mState != 0;` -- confirmed byte-exact match to `mState`'s own
 * offset (+0xd8).
 *
 * `IsPreloadRunning() const` (0-arg, .text+0x08241250, 18 bytes): real,
 * literal `return mState == 1;`.
 *
 * `IsPreloadRunning(unsigned char, const char*) const` (.text+0x08241270,
 * 194 bytes) stays a Tier-B stub (real signature declared) -- real body scans
 * a `mGroupListHead`-rooted linked structure (42-byte `SGroupElem` stride,
 * `CBatchDiskCmds::SGroupElem` per symbols.csv) that
 * `PrepareGroupsForPreload()`/`PreloadGroup()` populate; since those stay
 * deferred, `mGroupListHead` is always 0 here, and ground truth's own real
 * behavior on an empty list is to fire a soft Api-vtable diagnostic call then
 * (in ground truth) retry against whatever the diagnostic handler may have
 * populated -- a real, out-of-scope assert-style tail this pass does not
 * attempt to reproduce (no other reconstructed code calls this 2-arg
 * overload).
 */

#ifndef BATCH_DISK_MAIN_TASK_H
#define BATCH_DISK_MAIN_TASK_H

#include "task.h"
#include "editable.h"
#include "rm_api_callback.h"
#include "dir_entry.h"
#include "cz_util.h"
#include "out_link.h"

class CModule;

class CBatchDiskMainTask : public CTask, public CEditable, public CRMApiCallBack {
public:
	/* .text+0x08241920, 450 bytes. Real body -- see header comment. */
	CBatchDiskMainTask(const CModule &owner, const char *preloadList);

	/* .text+0x08241040 (D1)/0x082411d0 (D0), plus this-adjusting
	 * `_ZThn8_`/`_ZThn128_` variants (matching the 3-base layout above) --
	 * same real "re-install all 3 vtable groups, tail-call CTask::~CTask()"
	 * shape as every other multi-base dtor in this project.
	 */
	~CBatchDiskMainTask();

	/* .text+0x08241340, 1336 bytes. Tier-B stub -- see header comment.
	 * Real body parses a ';'-separated resource-family list via `CZ`.
	 */
	void PrepareGroupsForPreload(const char * /*groupList*/) {}

	/* .text+0x08241230, 18 bytes. Real -- see header comment. */
	bool IsBusy() const { return mState != 0; }

	/* .text+0x08241250, 18 bytes. Real -- see header comment. */
	bool IsPreloadRunning() const { return mState == 1; }

	/* .text+0x08241270, 194 bytes. Tier-B stub -- see header comment
	 * (depends on the still-deferred CZ-driven group list). */
	bool IsPreloadRunning(unsigned char /*group*/, const char * /*name*/) const { return false; }

private:
	unsigned char  mGap88[4];      /* +0x88..0x8c, unknown */
	int            mUnknown8c;     /* +0x8c */
	int            mUnknown90;     /* +0x90 */
	unsigned char  mUnknown94;     /* +0x94 */
	unsigned char  mUnknown95;     /* +0x95 */
	unsigned char  mGap96[0x14];   /* +0x96..0xaa, unknown */
	unsigned char  mUnknownAA;     /* +0xaa */
	unsigned char  mGapAB[0x15];   /* +0xab..0xc0, unknown */
	COutLinkMulti *mOutLink;       /* +0xc0 */
	int            mGroupListHead; /* +0xc4 */
	unsigned char  mUnknownVec[0x10]; /* +0xc8, opaque TVector<int,1> */
	int            mState;         /* +0xd8 */
	int            mUnknownDC;     /* +0xdc */
	int            mUnknownE0;     /* +0xe0 */
	CDirEntry      mDirEntry;      /* +0xe4 */
	CZ             mCZ;            /* +0x14c */
	unsigned char  mUnknown15C;    /* +0x15c */

	friend struct BatchDiskMainTaskTestHooks;
};

#endif /* BATCH_DISK_MAIN_TASK_H */
