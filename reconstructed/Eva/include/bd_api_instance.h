/*
 * bd_api_instance.h  -  CBDApiInstance, the 9th and final "XxxApiInstance"-family
 * global singleton (see global_object_base.h's own file comment for the other 8).
 *
 * RECONSTRUCTED FOR REAL 2026-07-27, correcting TWO prior "confirmed genuine dead
 * end" verdicts (README.md's Stage 6 breadth-sweep re-check, 2026-07-25, and the
 * CAlphaKeybIfcTask batch's independent re-check the same day). Both concluded
 * `RegisterLoader(CBatchDiskMan*)` -- the one method with a plausible real caller --
 * had "zero call sites anywhere in the 37,795-function export, confirmed by grepping
 * every decompile for the mangled symbol". A from-scratch `objdump -dr -M intel`
 * re-trace (2026-07-27, dispatched to re-examine the project's largest deferred
 * items with the same "size is not depth" lens that had just unlocked CLimiterBase/
 * CKGMsgProcessor) found this was a false negative -- the correct mangled name is
 * `_ZN14CBDApiInstance14RegisterLoaderEP13CBatchDiskMan` (14/13-char length
 * prefixes for "CBDApiInstance"/"CBatchDiskMan"; the prior greps apparently used a
 * mistyped 13/12 prefix and matched nothing). A direct `objdump -dr | grep "call.*
 * 8243980"` finds exactly ONE real call site: `CBatchDiskManConstructor::Create()`
 * (.text+0x08243d80) -- a function THIS PROJECT ALREADY RECONSTRUCTED as
 * `CBatchDiskManConstructorCreate()` (mains.cpp), which deliberately omitted this
 * exact call ("already independently confirmed a genuine dead end... deliberately
 * NOT reproduced here", mains.cpp's own prior comment, now corrected).
 *
 * Worse, that caller is ALSO the one this project's own "CBatchDiskMan unlock
 * batch" (2026-07-26) note in gen_manifest.py already flagged as now
 * boot-path-reachable: `PTR__CBatchDiskManConstructor_08eabe08`'s Create slot
 * routes to this exact function via `CConfigManager::CreateUserModules()`'s
 * "BatchDiskManClass" row (config_info.cpp row 0) -- i.e. `RegisterLoader()` is
 * reachable from the currently-wired boot path, not dead code, and omitting it was
 * a real (if inconsequential -- see below) gap in an already-shipped reconstruction.
 *
 * REAL CLASS SHAPE (from RegisterLoader@08243980.c/IsBusy@08243920.c/
 * IsPreloadRunning@082438c0.c,08243830.c/~CBDApiInstance@08245570.c,082455e0.c --
 * global object `BDApiInstance`, .bss @0x0939c114, sizeof 0x14 confirmed exactly by
 * the very next `.bss` symbol, `BDApi`, at 0x0939c128):
 *   +0x00  CBDApiInstance's own vtable ptr (installed by ~CBDApiInstance() to
 *          0x08eac0c8, base-class CGlobalObjectBase's own vtable 0x08e79768 --
 *          same "written, then overwritten by inherited cleanup" shape documented
 *          elsewhere in this project). Confirmed derived from CGlobalObjectBase
 *          (dtor tail-jmp's into `_ZN17CGlobalObjectBaseD1Ev`, global_object_base.h)
 *          -- same base every other XxxApiInstance shares.
 *   +0x04  the embedded `TVector<CBatchDiskMan*,1> mLoaders` sub-object's OWN
 *          leading vtable-shaped slot (installed by ~CBDApiInstance() to
 *          0x08eac120) -- confirmed via `TVector<CBatchDiskMan*,1>::MakeCapacity()`
 *          (.text+0x082456d0, 536 bytes) itself: it receives `this+4` as its own
 *          `tvec` argument (`lea eax,[ebx+4]` at RegisterLoader's own growth-check
 *          call site) and reads `tvec+4`/`tvec+8`/`tvec+0xc` as begin/end/cap --
 *          i.e. this field IS the TVector's own "+0x0 vtbl (untouched)" slot, same
 *          shape already established for `CTask::mRegisteredIfcs`
 *          (`TVector<CTask::SRegisteredIfc,1>`, task.cpp).
 *   +0x08  mBegin  CBatchDiskMan** (TVector begin)
 *   +0x0c  mEnd    CBatchDiskMan** (TVector end)
 *   +0x10  mCap    CBatchDiskMan** (TVector cap)
 * Neither vtable-shaped slot (+0x00/+0x04) is ever READ by RegisterLoader/IsBusy/
 * IsPreloadRunning (confirmed directly from each one's own disassembly -- only
 * +0x08/+0x0c/+0x10 are touched) -- real virtual dispatch through this object, and
 * the base-class CGlobalObjectBase registration (CKernel::AddGlobalObject(), see
 * global_object_base.h) that would install them, are deliberately NOT modeled here,
 * same "reconstruct only what the traced call graph needs" precedent as
 * `CEditClient` (edit_man.h) and `CLimiterBase`'s own `COutLinkIfcBase` framework
 * (limiter_base.h) -- nothing on this project's traced boot path ever dispatches
 * virtually on `BDApiInstance` or destroys it. Both slots are kept as opaque
 * padding, not silently dropped, so `sizeof(CBDApiInstance)` still matches ground
 * truth's real 0x14 bytes.
 *
 * `RegisterLoader(CBatchDiskMan*)` (.text+0x08243980, 172 bytes): real body is a
 * `TVector<CBatchDiskMan*,1>` push_back -- grow via `MakeCapacity(this+4, used+1)`
 * if `mEnd == mCap`, then `*mEnd++ = loader;` -- returns the new element count (or
 * -1, preserved verbatim from the real disassembly's own `eax = -1` default, if
 * `loader` is NULL, in which case nothing is appended). `MakeCapacity()`'s own real
 * growth policy (0x082456d0): minimum capacity 0x20 (32) elements, doubling
 * thereafter until it covers the requested count -- a DIFFERENT curve from
 * `TVector<CTask::SRegisteredIfc,1>::MakeCapacity()`'s own (base 10, task.cpp) --
 * confirmed from this instantiation's own disassembly, not assumed by analogy. Old
 * elements are copied via GCC's own Duff's-device-unrolled (x8, mod-8) 4-byte-
 * element copy loop -- collapsed to `memcpy()` here, identical result (a
 * `CBatchDiskMan*` is trivially-copyable POD), same license already used for
 * `TVector<CTask::SRegisteredIfc,1>::MakeCapacity()` (task.cpp).
 *
 * `IsBusy() const`/`IsPreloadRunning() const`/`IsPreloadRunning(unsigned char,
 * const char*) const` (.text+0x08243920/0x082438c0/0x08243830, 85/85/129 bytes):
 * all three share one shape -- a soft, non-enforcing diagnostic call through the
 * global `Api` vtable slot+0x94 (`ds:0x930a1f4`, "Assertion failed..." with a real
 * `.rodata` file/line pair) if `mLoaders.size() != 1`, THEN an unconditional,
 * unchecked `return mBegin[0]->IsBusy();` (etc.) regardless of the assert's own
 * outcome -- same established "Api+0x94 log-only, never enforcing" convention this
 * project already omits everywhere else (`CTask::RegisterIfc`, task.cpp;
 * `CLimiterBase`, limiter_base.h; ...). `mBegin[0]` is dereferenced UNCONDITIONALLY
 * even if `mLoaders` is empty -- preserved verbatim (matches ground truth's own
 * fragility exactly, same "no NULL check either" precedent as
 * `CSysApiInstance::WriteMessageToHost()`, sysapi_instance.h). All 3 tail-call the
 * already-real `CBatchDiskMan::IsBusy()`/`IsPreloadRunning()`/`IsPreloadRunning
 * (unsigned char, const char*)` (batch_disk_man.h -- the 3rd overload added by this
 * same pass, forwarding to `CBatchDiskMainTask::IsPreloadRunning(unsigned char,
 * const char*)`, already real, batch_disk_main_task.h, `return false;`).
 *
 * REACHABILITY: `RegisterLoader()` itself IS reachable (see above) and is now wired
 * into `CBatchDiskManConstructorCreate()` (mains.cpp). `IsBusy()`/`IsPreloadRunning()`
 * x2/the dtor have ZERO callers anywhere in the whole 22MB ground-truth binary
 * (confirmed by grepping every `call` target for each address) -- dead code in
 * ground truth itself, not just this reconstruction, same status as
 * `CLimiterBase` (limiter_base.h) -- reconstructed for structural completeness.
 */

#ifndef BD_API_INSTANCE_H
#define BD_API_INSTANCE_H

class CBatchDiskMan;

class CBDApiInstance {
public:
	CBDApiInstance() : mBegin(0), mEnd(0), mCap(0) {}

	/* .text+0x08243980, 172 bytes. Real body -- see header comment. Returns the
	 * new registered-loader count, or -1 (untouched) if `loader` is NULL.
	 */
	int RegisterLoader(CBatchDiskMan *loader);

	/* .text+0x08243920/0x082438c0/0x08243830. Real bodies -- see header comment.
	 * All 3 unconditionally dereference the first registered loader.
	 */
	bool IsBusy() const;
	bool IsPreloadRunning() const;
	bool IsPreloadRunning(unsigned char group, const char *name) const;

private:
	/* +0x00/+0x04 -- CGlobalObjectBase's own vtable ptr + the embedded TVector's
	 * own leading slot. Never read by any method above -- see header comment.
	 */
	unsigned char  mUnknown0[8];
	CBatchDiskMan **mBegin; /* +0x08 */
	CBatchDiskMan **mEnd;   /* +0x0c */
	CBatchDiskMan **mCap;   /* +0x10 */

	friend struct BDApiInstanceTestHooks;
};

/* Real global, .bss @0x0939c114 (BDApiInstance), sizeof 0x14. Real per-instance
 * CGlobalObjectBase registration (global constructor) deliberately not modeled --
 * see header comment.
 */
extern CBDApiInstance BDApiInstance;

#endif /* BD_API_INSTANCE_H */
