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
 * `CChkCmdBG` (.text+0x080c1380, 260 bytes) is now ALSO fully reconstructed
 * (2026-07-25 follow-up): its real ctor chains `CChkBaseTask(owner, sm_pkcTaskName,
 * 5, 0)` (confirmed byte-exact match for the earlier plausible-guess level/
 * scheduleFlag), installs its own vtable + opaque +0x08 identity, default-
 * constructs TWO embedded `CHeap` objects at +0x98/+0xa8 (`CHeap(growBy=10,
 * capacity=5)` each -- see `heap.h`), sets an inferred `mState` int at +0x94 to 2
 * and an inferred `mPendingCount` int at +0xb8 to 0 (the dtor asserts this is
 * still 0 before tearing down, `Api` vtable slot +0x94, "Assertion failed in
 * module %s, line %i.\n" / "ChkCmdBG.cpp" / line 0x3f), then mallocs a real
 * `COutLinkMono` (already reconstructed, out_link.h) via the same
 * `HAL_DisableInterrupts()`/`HAL_EnableInterrupts()`-bracketed `malloc(0x38)` as
 * `CEvBuffersPool`/`out_link.cpp` (brackets dropped, same established userspace
 * no-op precedent) and registers it via `CTask::Add(COutLink*)`. Object size
 * confirmed 0xc0 bytes (matches `CChunkMan::Setup()`'s own `malloc(0xc0)`).
 * `CHeap` itself is a brand-new, tractable ctor/dtor-only reconstruction (see
 * `heap.h`) -- NOT the unrelated `CSTGHeapManager` from the OA.ko project.
 * `CChkCmdBG`'s own real vtable (`PTR__CChkCmdBG_08e85768`) is confirmed by a
 * direct .rodata byte read: 6 real function slots (non-deleting dtor, deleting
 * dtor, `Exec`@08180950 [confirmed elsewhere a real 3-byte `return;`],
 * `ExecMsg`@0807e170, `Exec(CMessage&)`-shaped @080c6d50, `AcceptDuplicate`
 * @08185d60) followed immediately by the this-adjusted (-8) secondary vtable's
 * own `[offset_to_top][RTTI]` preamble at slots 6/7 -- i.e. the 8-dword size
 * already used for the sibling `PTR__CChkBaseTask`/`PTR__CChkCmd` arrays is
 * CONFIRMED correct (not a heuristic guess): the "next symbol" boundary
 * (`DAT_08e85788`) is genuinely the START of the secondary (CTask-interface)
 * vtable's own vfunc array, exactly where `this+8` is supposed to point -- so
 * treating it as an opaque, never-dereferenced placeholder (same as
 * `CChkBaseTask`/`CChkCmd`) is faithful, not a bug. Slots 2-5 are left as
 * `EvaVTableStub` -- `Exec`/`ExecMsg`/`AcceptDuplicate` are `CChkCmd`'s own
 * "further methods" already deferred as out of scope in this same file header.
 */

#ifndef CHUNK_MAN_H
#define CHUNK_MAN_H

#include "module.h"
#include "task.h"
#include "heap.h"

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

class CChkCmdBG : public CChkBaseTask {
public:
	/* .text+0x080c1380, 260 bytes. Real body -- see file header. */
	explicit CChkCmdBG(const CModule &owner);

	/* .text+0x080c11b0, 115 bytes (non-deleting D2 dtor). Real body: tears
	 * down the 2 CHeap sub-objects then chains to CChkBaseTask's own dtor.
	 * (Also asserts mPendingCount == 0 first -- see file header. Assertion
	 * path itself not modeled, matches this project's usual "assert is a
	 * real call, condition is faithfully checked, string content
	 * transcribed" treatment.)
	 */
	~CChkCmdBG();

private:
	int    mState;         /* +0x94, ctor sets 2, inferred name */
	CHeap  mHeap1;          /* +0x98 */
	CHeap  mHeap2;          /* +0xa8 */
	int    mPendingCount;   /* +0xb8, ctor sets 0, dtor asserts == 0, inferred name */
	void  *mOutLinkMono;    /* +0xbc */

	friend struct ChkCmdBGTestHooks;
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
