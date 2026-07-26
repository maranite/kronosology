/*
 * res_man.h  -  CResMan, Eva's resource (bank/sample/etc.) manager module
 * (`MMainResMan()`, mains.cpp). Real ctor + `Start()` reconstructed here (Stage 6
 * breadth sweep, 2026-07-25 -- the "What's still open" CFileMan/CResMan ctor batch);
 * the rest of the class (~50 further methods -- Save() alone is 16175 bytes;
 * LoadFile/DeleteOldPrepareNew/ScanPartitionTable-scale siblings throughout) is a
 * genuine god-object, out of scope for this pass, same boundary as CFileMan
 * (file_man.h) and the ES-family CXxxTask god-objects.
 *
 * Real layout confirmed from CResMan@081523a0.c (the only reconstructed method,
 * ctor, 1333 bytes) -- 0x21a0 (8608) bytes total, base CModule (0x2c) + 0x2174 of
 * its own fields:
 *   +0x2c  mCallbackVtbl   secondary (this-adjusted, multiple-inheritance) vtable
 *          slot for a `CRMApiCallBack`-interface sub-object CResMan itself
 *          implements. Transiently `PTR__CRMApiCallBack_08e886e8` (a real,
 *          independent 7-slot vtable, shared with mains.cpp's own RMApiInstance
 *          ctor), then overwritten with the class' own final identity -- see
 *          res_man.cpp / omega_vtables.h.
 *   +0x30  mJob    `CRMJob*`, `malloc(0x54)`'d then real-constructed via
 *          `CRMJob::CRMJob()` (confirmed via direct disassembly of
 *          CResMan::CResMan(), .text+0x081523a0: `call malloc@plt` immediately
 *          followed by `call CRMJob::CRMJob()` on the fresh block, HAL_Disable/
 *          EnableInterrupts-wrapped, dropped per this project's usual
 *          convention). UPDATE (Eva "size is not depth" re-check batch,
 *          2026-07-26): previously left as a raw untyped `void*`/`malloc`-only
 *          Tier-B stub because `CRMJob::CRMJob()` itself depended on the
 *          247-method `CZ` string-set container -- that blocker was resolved
 *          the same batch (`cz_util.h` gained an opaque `CZ(unsigned)`/`~CZ()`
 *          sufficient for placement, `rm_job.h`'s `CRMJob::CRMJob()` is now a
 *          real reconstructed body). This ctor now matches ground truth
 *          exactly: malloc, then placement-construct the real `CRMJob`. Note
 *          `mains.cpp`'s own `RMApiInstance` ctor still uses the old raw-blob
 *          treatment for its own, separate `CRMJob*` -- that one is unrelated
 *          to this fix and was not touched this pass.
 *   +0x34..+0x74  mUnknown34[0x40]  13 undecoded int/byte scalars the real ctor
 *          writes at fixed offsets within this range (-1 at +0x34, 3 bytes of
 *          0xff at +0x68..+0x6a, 0 everywhere else observed) -- real per-field
 *          meaning not decoded; see res_man.cpp for the exact byte-for-byte
 *          writes.
 *   +0x74  mResultCount   ctor zeroes; grouped immediately before mResults,
 *          real meaning not decoded.
 *   +0x78..+0x90  mResults   embedded COmegaPtrArray (3-int ctor: growBy=2,
 *          initialCapacity=0, ownFlag=1), vtable-swapped to the real
 *          `TPtrArray<CRMResult::SSingleError>` (confirmed via `nm -C`), then
 *          immediately `RemoveAll(1)`'d -- a real but redundant defensive clear
 *          (the array is already empty at that point; see omega_ptr_array.h's
 *          own header comment on `RemoveAll()`).
 *   +0x90..+0x20b0  mChunks[257]   embedded `CChunkOnDemand` records
 *          (chunk_on_demand.h), default-constructed in order (implicit array
 *          member construction matches the real ctor's own sequential
 *          construction order exactly).
 *   +0x20b0..+0x21a0  mTail[0xf0]   10x `TVector<CResEntryEx,1>`-vtable-swapped
 *          sub-regions (each nominally 0x18 bytes apart, but only vtbl+3 ints
 *          -- 0x10 of the 0x18 -- are ever written; the object's own real total
 *          size (0x21a0) doesn't leave room for a full 0x18-byte envelope on the
 *          10th one, so this whole tail is modeled as one flat, exactly-sized
 *          byte buffer written via raw offset casts in the ctor rather than a
 *          fixed-stride array of sub-objects -- see res_man.cpp for the exact
 *          10 installs).
 */

#ifndef RES_MAN_H
#define RES_MAN_H

#include "module.h"
#include "omega_ptr_array.h"
#include "chunk_on_demand.h"
#include "rm_job.h"

class CResMan : public CModule {
public:
	CResMan();

	/* .text+0x08151300, 3 bytes -- confirmed genuinely empty (`return 0;`) in the
	 * real binary. Wired into this class' own real vtable slot 4 (omega_vtables.h/
	 * .cpp, byte-verified 12-slot primary layout: dtor pair, Setup, Config, Start,
	 * Destroy, GetErrorMsg, then 5 further real CResMan-specific overrides --
	 * OnSave/OnDelete/OnLoad/OnSetRes/OnLoadRes). Setup()/Config() (slots 2/3) are
	 * real methods too but genuinely deeper (Setup constructs CResChkServer/
	 * CResChkClient/CRMMainTask; Config depends on ChkApi + 2 undecoded
	 * `sm_pkcTaskName` statics); OnSave/OnDelete/OnLoad/OnSetRes/OnLoadRes (slots
	 * 7-11, 272-2023 bytes each) are real resource-load/save callback handlers --
	 * all out of scope for this ctor-focused pass, not declared here.
	 */
	void Start();

private:
	void            *mCallbackVtbl;
	CRMJob          *mJob;
	unsigned char    mUnknown34[0x40];
	int              mResultCount;
	COmegaPtrArray   mResults;
	CChunkOnDemand   mChunks[257];
	unsigned char    mTail[0x21a0 - 0x20b0];

	friend struct ResManTestHooks;
};

#endif /* RES_MAN_H */
