/*
 * job_stack.h  -  CJobStack, construction/destruction-only reconstruction.
 *
 * BACKGROUND: `RMApiInstance`'s own `CGlobalObjectBase` phase-hook slot 2
 * (`PreKernelConstructor`, .text+0x08165490, 24 bytes, mains.cpp) lazily
 * heap-constructs a `CJobStack` into `RMApiInstance+0x24`. This was left as
 * `EvaVTableStub` in the 10th-16th vtable-dispatch-stub-gap sweep
 * (2026-07-27) with the class flagged "genuinely deep, out of scope" --
 * the same `CResMan`/`CJobStack` "god object" family already correctly left
 * unmodeled in `batch_disk_main_task.h`/`rm_api_callback.h` for its 8
 * `AddLoadRes`/`AddLoadFile`/`AddSetRes`/`AddSave`/`AddDelete`/
 * `ExecutePendingCmds`-family business-logic methods (real `TVector<...>`/
 * `TPtrArray<SLoadBankOffset>`-driven job-queue machinery, genuinely large
 * and genuinely unreachable on this project's traced boot path).
 *
 * RE-TRACE (Eva "size is not depth" re-check, 2026-07-27): direct disasm of
 * `CJobStack::CJobStack()` (.text+0x0814da30, 0814da30-0814da97) and
 * `~CJobStack()` (D1 .text+0x0814d6c0, D0 .text+0x0814d870) shows the
 * CONSTRUCTION/DESTRUCTION PATH ALONE is small and fully self-contained --
 * it never touches the job-queue's `Add*`/`ExecutePendingCmds` machinery at
 * all. This is exactly the same "size is not depth" pattern already found
 * for `VoiceModelMsgHandler`/`CombiMsgHandler`/`ProgramMsgHandler` and
 * `CRTRouterApiInstance`'s own phase hooks (mains.cpp WORKAROUND #4): the
 * class as a WHOLE stays out of scope (its 8 job-queue methods are real,
 * large, and unreached), but the narrow slice `RMApiInstance`'s slot 2
 * actually needs -- just standing the object up and tearing it back down --
 * is tractable and is reconstructed here for real.
 *
 * REAL LAYOUT (from `CJobStack::CJobStack()`/`~CJobStack()`'s own
 * disassembly, confirmed against `CRMApiCallBack`'s already-reconstructed
 * own 8-byte layout, rm_api_callback.h):
 *   +0x00  mVtbl        primary vtable ptr. Transiently `PTR__CRMApiCallBack_
 *          08e886e8` (the base subobject's own vtable) during construction,
 *          before being overwritten with `PTR__CJobStack_08e88608` (this
 *          class's own final vtable) -- same "base-construct-then-vtable-
 *          swap" idiom as every other class in this project
 *          (global_object_base.h's own header comment).
 *   +0x04  mJob         CRMJob* (rm_job.h) -- CRMApiCallBack's OWN inherited
 *          field (CJobStack : public CRMApiCallBack, confirmed by the ctor
 *          writing the transient base vtable into +0x00 before this field is
 *          set, and the dtor re-writing +0x00 back to the base vtable
 *          immediately before destructing/freeing this same field -- the
 *          standard Itanium ABI "vptr reset before base-subobject
 *          destruction" pattern). Owns a single heap `CRMJob`,
 *          malloc(0x54)'d + placement-constructed here, matching every
 *          other `CRMJob` owner in this project (res_man.cpp,
 *          batch_disk_main_task.cpp, mains.cpp's own
 *          `ConstructRMApiInstance()`).
 *   +0x08  mIfcVtbl     secondary (multiple-inheritance/IFC) vtable ptr,
 *          `PTR__CJobStack_secondary_08e886c0` -- 2 real, unidentified
 *          functions (.text+0x0818f8b0/0x0818fb00). Install-only: nothing
 *          in this project's own traced call graph (RMApiInstance's own
 *          PreKernelConstructor/PostKernelDestructor) ever dispatches
 *          through it -- same "confirmed real, deliberately not wired"
 *          precedent as `CBatchDiskMainTask`'s own vtable clusters
 *          (batch_disk_main_task.h).
 *   +0x0c  mJobVecBegin CRMJob* -- TVector<CRMJob> begin. Always == End in
 *          this reconstruction: the only reachable ctor path is the empty
 *          one (no `AddLoadRes`/`AddSave`/etc. call is ever reachable), so
 *          this stays a real, faithful "always empty" state rather than a
 *          silently-wrong non-empty one.
 *   +0x10  mJobVecEnd   CRMJob*
 *   +0x14  mJobVecCapEnd CRMJob* (capacity end; unused since the vector
 *          never grows here, but zeroed for byte-fidelity)
 * Total 0x18 (24) bytes -- matches the real `malloc(0x18)` call site in
 * `CRMApiInstance::PreKernelConstructor` (mains.cpp) exactly.
 *
 * The 8 `AddLoadRes`/`AddLoadFile`/`AddLoadSingleRes` (x2)/`AddSave`/
 * `AddDelete`/`AddSetRes`/`ExecutePendingCmds` methods (.text+0x0814dac0..
 * 0x0814f800+) are NOT reconstructed -- real, large (0814dac0..0814f800+
 * spans ~0x2d00 bytes total), genuinely out-of-scope job-queue business
 * logic with zero reachable caller on this project's traced boot path
 * (same verdict already reached for the sibling `CResMan::LoadRes()`-family,
 * rm_api_callback.h). `CJobStack` is declared here as a plain, non-
 * polymorphic-in-C++-terms data holder (raw `void *` vtable fields, manual
 * pokes) rather than a real virtual C++ class, matching this project's
 * established convention for every other manually-vtable-swapped class
 * (global_object_base.h) -- the real ground-truth object IS polymorphic,
 * but nothing in this reconstruction's own call graph ever dispatches
 * through `CJobStack`'s OWN vtable except the single opaque D0-dtor call
 * already wired in `CRMApiInstance::PostKernelDestructor` (mains.cpp).
 */

#ifndef JOB_STACK_H
#define JOB_STACK_H

extern "C" {

/* CJobStack::CJobStack(), .text+0x0814da30. Placement-constructs a real
 * CJobStack into caller-provided, malloc(0x18)'d storage at `self` -- the
 * caller does the allocation itself, matching this project's established
 * "malloc then placement-construct" idiom (res_man.cpp's own
 * `mJob = new (malloc(0x54)) CRMJob();`), not the ctor.
 */
void CJobStack_Construct(void *self);

/* ~CJobStack(), D0 (deleting) variant, .text+0x0814d870 -- the only variant
 * this project's own call graph (CRMApiInstance::PostKernelDestructor's
 * opaque vtable+4 dispatch, mains.cpp) actually invokes. Destructs the
 * (always-empty, in this reconstruction's own reachable call graph) job
 * queue, then the inherited CRMApiCallBack base's owned CRMJob, then frees
 * the CJobStack block itself. Signature matches mains.cpp's own
 * `OpaqueDtorFn` (`void (*)(void *)`) exactly.
 */
void CJobStack_DeletingDtor(void *self);

/* Real CJobStack vtable (7 usable slots, matching ground truth
 * 0x8e88608..0x8e88624): {~CJobStack D1, ~CJobStack D0, OnSetRes, OnLoadRes,
 * OnLoad, OnSave, OnDelete}. Slot 1 (D0) is real and load-bearing (the one
 * PostKernelDestructor's opaque dispatch actually calls). Slot 0 (D1) is
 * real for fidelity but never dispatched through here. Slots 2-6 stay
 * EvaVTableStub: real, confirmed-empty no-op ground truth bodies
 * (rm_api_callback.h's own CRMApiCallBack::OnXxx, inherited unchanged) with
 * no reachable caller in this reconstruction.
 */
extern void *PTR__CJobStack_08e88608[7];

/* Secondary (multiple-inheritance/IFC) vtable, install-only -- see header
 * comment. Never dispatched through by any reconstructed caller.
 */
extern void *PTR__CJobStack_secondary_08e886c0[2];

} // extern "C"

#endif /* JOB_STACK_H */
