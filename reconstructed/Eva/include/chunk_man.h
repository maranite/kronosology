/*
 * chunk_man.h  -  CChunkMan : public CModule, CChkBaseTask : public CTask,
 * CChkCmd : public CChkBaseTask, CChkCmdBG (Tier-B stub) : public CChkBaseTask.
 * Stage 6 breadth sweep, 2026-07-25 (small-derived-module follow-up batch, see
 * dump_man_mod.h / edit_man.h for the shared "MMainXxx 9-member family" context).
 *
 * GROUND TRUTH: `MMainChunkMan()` (mains.cpp, already Tier A) builds the base
 * `CModule("ChunkMan")` and vtable-swaps in `PTR__CChunkMan_08e85968` -- unchanged,
 * same "mains.cpp already produces the correct object" precedent as CDumpManMod/
 * CEditMan. `CChunkMan::Setup()` (.text+0x080cb880, 118 bytes) mallocs and
 * registers TWO real sibling tasks via the already-real `CModule::Add(CTask*)`:
 * `CChkCmd` (0x9c bytes) and `CChkCmdBG` (0xc0 bytes) -- same "construct + Add() a
 * sibling CTask pair" idiom as `CDumpManMod::Setup()`. `CChunkMan::Config()`
 * (.text+0x080cb6a0, 160 bytes) is real and NOT empty (unlike every other sibling
 * in this batch) -- see .cpp. `CChunkMan::Start()` (.text+0x080cb740, 3 bytes) is
 * confirmed genuinely empty.
 *
 * `CChkBaseTask` (.text+0x080bfec0, 118 bytes) is a real, fully tractable
 * intermediate base both `CChkCmd`/`CChkCmdBG` derive from: chains into
 * `CTask::CTask(owner, name, level, scheduleFlag, 0x8003)`, installs its own
 * vtable, overwrites CTask's own `mIfcThunk` (+0x08) with a class-specific opaque
 * identity, and default-constructs an embedded `COmegaPtrArray(growBy=10,cap=5,
 * own=1)` at +0x7c (vtable-swapped to `TPtrArray<CRegistrationEntry>`) -- no new
 * out-of-scope dependency (`COmegaPtrArray` is already real, omega_ptr_array.h).
 * Its own further real methods (`FindFirstModule`/`AddNewModule`/
 * `FindRegisteredModules`/`GetIDServerModule`/`CheckAllDefaultModules`/
 * `AdjustAllDefaultModules`, 118-515 bytes each) are NOT reconstructed here --
 * genuinely deeper chunk-server-registry logic this batch didn't need (nothing
 * calls them on the currently-wired boot path); only the ctor is real.
 *
 * `CChkCmd` (.text+0x080c0ea0, 159 bytes) is ALSO real and fully tractable: chains
 * to `CChkBaseTask::CChkBaseTask(owner, sm_pkcTaskName, 4, 2)`, sets its own vtable
 * + opaque +0x08 identity, sets a `mCommId`-shaped sentinel byte at +0x98 to 0xff,
 * then mallocs a real `COutLinkMono` (already reconstructed, out_link.h) and
 * registers it via the already-real `CTask::Add(COutLink*)` (task.h) -- the SAME
 * two already-real dependencies `CDumpTask`'s own ctor exercises (dump_task.h).
 * `CChkCmd`'s own further real methods (`Exec`/`Exec(CMessage&)`/`OnAccepted`/
 * `OnEnd`/`OnFail`/`OnMergeBegin`/`OnAfterPreSave`/`ResetSchedulingParams`/
 * `AcceptDuplicate`) are NOT reconstructed -- genuinely out of scope (the actual
 * "chunk save/load command" state machine, a `CClientCommServer`-scale dependency
 * this batch correctly defers, same boundary as `CDumpManStateMachine`).
 *
 * `CChkCmdBG` (.text+0x080c1380, 260 bytes) is the ONE genuinely-too-deep sibling
 * in this pair: beyond the same `CChkBaseTask`/opaque-identity/mCommId-shaped-
 * sentinel pattern as `CChkCmd`, its real ctor also constructs TWO embedded `CHeap`
 * objects (`CHeap::CHeap(this+0x98, 10, 5)` / `CHeap::CHeap(this+0xa8, 10, 5)`) --
 * `CHeap` is a brand-new, wholly unreconstructed memory-heap-allocator class (not
 * the unrelated `CSTGHeapManager` from the OA.ko project) this pass has no other
 * reason to build. Modeled as a Tier-B stub deriving from the now-real
 * `CChkBaseTask` (not `CModule`/`CTask` directly, since a valid `CChkBaseTask`
 * base is now cheap and more faithful than skipping straight to `CTask`) with a
 * plausible (real level/scheduleFlag, unfaithful name) ctor -- same
 * "chain to nearest real base, no real derived body" precedent as
 * `CFileMan`/`CResMan` (mains.cpp) and `CESCommonTask` (es_common.h). Its own
 * `COutLinkMono`/`CTask::Add(COutLink*)` construction and `CHeap` sub-objects are
 * NOT modeled, matching that same license.
 */

#ifndef CHUNK_MAN_H
#define CHUNK_MAN_H

#include "module.h"
#include "task.h"

class CChkBaseTask : public CTask {
public:
	/* .text+0x080bfec0, 118 bytes. */
	CChkBaseTask(const CModule &owner, const char *name, int level, int scheduleFlag);

private:
	unsigned char mRegistrations[0x18]; /* +0x7c, embedded COmegaPtrArray(10,5,1) */
};

class CChkCmd : public CChkBaseTask {
public:
	/* .text+0x080c0ea0, 159 bytes. */
	explicit CChkCmd(const CModule &owner);

private:
	unsigned char mCommId;        /* +0x98, ctor sets 0xff */
	void         *mOutLinkMono;   /* +0x94 */
};

/* Tier-B stub -- see file header. Real name/CHeap sub-objects/COutLinkMono not
 * modeled.
 */
class CChkCmdBG : public CChkBaseTask {
public:
	CChkCmdBG(const CModule &owner)
		: CChkBaseTask(owner, "ChkCmdBG", 5, 0) {}
};

class CChunkMan : public CModule {
public:
	/* .text+0x080cb930, 38 bytes. Ground truth's own real boot-path caller
	 * (mains.cpp's MMainChunkMan()) builds an equivalent object by hand
	 * instead of calling this -- same "provided for structural completeness"
	 * status as CDumpManMod::CDumpManMod().
	 */
	CChunkMan();

	/* .text+0x080cb880, 118 bytes. Real body -- see file header. */
	void Setup();

	/* .text+0x080cb6a0, 160 bytes. Real, NOT empty -- see .cpp. */
	bool Config();

	/* .text+0x080cb740, 3 bytes. Confirmed genuinely `return 0;`. */
	void Start();

private:
	CChkCmdBG *mChkCmdBG; /* +0x2c */

	friend struct ChunkManTestHooks;
};

extern "C" void CChunkManSetupVSlot(void *obj);
extern "C" void CChunkManConfigVSlot(void *obj);
extern "C" void CChunkManStartVSlot(void *obj);

#endif /* CHUNK_MAN_H */
