/*
 * omega_vtables.h  -  real PTR__ClassName_<addr> vtable-slot arrays for classes this
 * project hasn't reconstructed the methods of, but whose objects get manually
 * vtable-swapped into existence throughout ckernel.cpp/scheduler.cpp/module_manager.cpp/
 * mains.cpp (the "base-construct then overwrite this+0" idiom -- see
 * omega_ptr_array.h's header comment for why).
 *
 * Real slot counts recovered directly from symbols.csv's own vtable/typeinfo layout
 * (Decomp/EVA_Decomp/eva_export/symbols.csv, the 08e80a80..08e814a0 cluster where GCC
 * laid out every Itanium-ABI vtable back-to-back, immediately followed by every
 * typeinfo object back-to-back) -- for each class, slot count = (next "vtable"-labeled
 * symbol's address - this class's own PTR_~ClassName address) / 4, i.e. the real
 * distance to the next class's vtable header or (for the last class in a contiguous
 * run) to the first typeinfo object, both being real, load-bearing boundaries in the
 * actual binary:
 *
 *   CHostInterfaceBase  08e80b68 -> 08e80bc0 (TPtrArray<CGlobalObjectBase>)  = 22 slots
 *   CHostInterface      08e80b08 -> 08e80b60 (CHostInterfaceBase)            = 22 slots
 *   COmegaPtrArray      08e80be0 -> 08e80bf0 (TNamedPtrArray<CModuleConstructor>) = 4 slots
 *   TNamedPtrArray<CModuleConstructor> 08e80bf8 -> 08e80c08 (TNamedPtrArray<CModule>) = 4 slots
 *   TNamedPtrArray<CModule>            08e80c10 -> 08e80c20 (CLevelManagerArray) = 4 slots
 *   TPtrArray<CLevelManager>           08e80c40 -> 08e80c50 (TVector<...>)   = 4 slots
 *   TVector<CTimerObject*,1>           08e80c58 -> 08e80c60 (CMessageInput)  = 2 slots
 *   CDummyMsgInput      08e80c80 -> 08e80c8c (start of typeinfo cluster)     = 3 slots
 *   CNamedObjectBase    08e81378 -> 08e81380 (start of typeinfo cluster)     = 2 slots
 *   CTracer             08e81468 -> 08e81474 (start of typeinfo cluster)    = 3 slots
 *   CLevelManagerArray  08e80c28 -> 08e80c38 (TPtrArray<CLevelManager>)     = 4 slots
 *   CLevelManager       08e80e50 -> 08e80ea0 (TNamedPtrArray<CTask>)        = 20 slots
 *   TNamedPtrArray<CTask> 08e80ea8 -> 08e80eb4 (start of typeinfo cluster)  = 3 slots
 *
 * Added in the Api/SysApiInstance pass (2026-07-23), same methodology, plus one direct
 * raw-byte read (08e81008+0xa4, confirmed == CSysApiInstance::RegisterApi's own real
 * .text address 0806bab0 -- see sysapi_instance.h) that pins down CSysApiInstance's own
 * vtable identity (it's the one Api's runtime type actually installs), not just its slot
 * count:
 *
 *   TPtrArray<CGlobalObjectBase> 08e80bc8 -> 08e80bd8 (COmegaPtrArray)      = 4 slots
 *     (this is sm_poGlobalObjectList's own real vtable -- CKernel::AddGlobalObject
 *     installs it, see global_object_base.h/ckernel.cpp; never dispatched through by
 *     any code on this pass's own traced boot path, same "install-only" status as
 *     everything else in this file)
 *   CSysApiInstance     08e81008 -> 08e81180 (typeinfo-name, no further vtable
 *     symbol in this run) = 94 slots -- Api's own real installed vtable once
 *     SysApiInstance's static constructor runs (sysapi_instance.cpp). Genuinely
 *     dispatched through by Mains()'s existing 17-member MMainXxx family (mains.cpp,
 *     CallVSlot at +0x40/+0xa0/+0xb4) -- unlike the opaque single-symbol vtables
 *     mains.cpp installs locally for classes nothing dispatches through, this one
 *     needs the real slot count so those raw offset reads stay in-bounds. +0xa4 is
 *     confirmed (not just "shape-sized") to be RegisterApi -- mains.cpp's own 8-member
 *     MMainXxx(void) family (ckernel.cpp's InitSystemLayer()) calls it by name
 *     directly rather than through this array, now that the identity is known. +0x40
 *     (slot 16) is confirmed (direct byte read of the ground-truth binary's own
 *     installed vtable) to be CSysApiInstance::AddConstructor() -- wired to the real
 *     AddConstructorVSlot forwarder (omega_vtables.cpp) instead of EvaVTableStub,
 *     Stage 6 breadth sweep, 2026-07-25 -- see sysapi_instance.h/module_manager.h.
 *     +0xac (slot 43) is likewise wired to a real forwarder (LookupResourceStub,
 *     omega_vtables.cpp) instead of EvaVTableStub, found 2026-07-26 during the
 *     CEditor vtable-fix's own live-boot verification: CPoller::CPoller()
 *     (poller.cpp) calls this slot as a named-resource lookup and dereferences
 *     a non-null return value through IT'S OWN vtbl+0x10 -- with the old
 *     EvaVTableStub no-op leaving EAX as post-call garbage, this was a live,
 *     reproducible segfault the very first time CPanel::Setup() (and therefore
 *     CPoller's ctor) ran via CModuleManager's real dispatch on an actual VM
 *     boot. See LookupResourceStub's own header comment for the full story.
 *   TNamedPtrArray<CDriverBase>    08e811a8 -> 08e811b8 (next vtable header) = 4 slots
 *   TNamedPtrArray<CApiDescriptor> 08e811c0 -> 08e811e0 (CSystemApi)         = 8 slots
 *     (SysApiInstance's own 2 embedded COmegaPtrArray sub-objects install these --
 *     see sysapi_instance.h's corrected +4/+0x1c field mapping. Install-only, like
 *     the TPtrArray<CGlobalObjectBase> entry above.)
 *
 * Added in the Stage 6 breadth-sweep pass (2026-07-25), CModule/CTaskBuffer/RunLevel
 * batch. **Correction to this file's own stated methodology**: the boundary used for
 * slot-count arithmetic is each class's own installed vtable POINTER address (the
 * `PTR_~ClassName_<addr>` label Ghidra emits 8 bytes into most classes' raw vtable
 * object, past the 2-word offset-to-top/typeinfo-ptr Itanium-ABI header -- e.g.
 * CModule's raw `vtable` label sits at 08e81fe0, but the address every ctor actually
 * writes into `this+0` is `PTR__CModule_08e81fe8`, +8 later) to the NEXT symbol of any
 * kind (matches this file's original wording "next class's vtable header or... the
 * first typeinfo object" -- for CModule specifically, that next symbol is CModule's
 * OWN typeinfo, not another class's vtable, since nothing else sits between):
 *
 *   CModule  08e81fe8 -> 08e82004 (CModule's own typeinfo) = 7 slots. Confirmed against
 *     5 known dispatches (0/4=dtor pair, +8=Setup, +0xc=Config, +0x10=Start -- see
 *     module_manager.cpp) plus 2 further real named methods this pass didn't trace
 *     individually (`Destroy`@08181c10, `GetErrorMsg`@08181c20) that exactly fill the
 *     remaining 2 slots (+0x14/+0x18) -- a clean match, not a coincidence.
 *
 * Added in the Stage 6 breadth-sweep pass (2026-07-25), AddModule()/EnableUpdate()
 * batch. **Safety-critical fix, not just documentation**: mains.cpp's 6 real derived-
 * module vtable placeholders (PTR__CEditMan_08e85ea8 and 5 siblings) were each a bare
 * scalar `void *` (always NULL, since never assigned) rather than a slot array --
 * harmless while CModuleManager::AddModule() was a Tier-B no-op (mModules stayed
 * permanently empty, so CModuleManager::Setup/Config/Start() never actually dispatched
 * through any module's vtable), but AddModule() is now Tier A (module_manager.h) and
 * mModules is genuinely populated on the real boot path -- a NULL vtbl there would be
 * a live NULL-pointer-call crash the first time InitSystemLayer()'s own
 * Setup()/Config() run (ckernel.cpp calls MMainEditMan() then Setup()/Config()
 * immediately after). Upgraded to real EvaVTableStub-backed arrays, same
 * symbols.csv-boundary methodology as everything else in this file:
 *
 *   CEditMan      08e85ea8 -> 08e85ec4 (own typeinfo-name)        =  7 slots
 *   CMessagePort  08e88468 -> 08e8849c (CReceiveFromModules typeinfo-name) = 13 slots
 *     (real extra virtuals beyond CModule's own 7 -- CViewBase/CMessagePort is a
 *     message-handling base with its own additional dispatch surface; not decoded
 *     individually, just sized correctly so any in-range dispatch is safe)
 *   CSeqTimer     08e892a8 -> 08e892c4 (own typeinfo-name)        =  7 slots
 *   CSysEx        08e899e8 -> 08e89a04 (own typeinfo-name)        =  7 slots
 *   CChunkMan     08e85968 -> 08e85984 (own typeinfo-name)        =  7 slots
 *   CDumpManMod   08e85ca8 -> 08e85cc4 (own typeinfo-name)        =  7 slots
 *
 * Added in the Stage 6 breadth-sweep pass (2026-07-25), CTask::CTask()/CLimiterMan
 * batch -- same installed-pointer-to-next-symbol methodology, confirmed against
 * symbols.csv's 08e81d78..08e821e7 cluster:
 *
 *   CTask        08e82128 -> DAT_08e82144 (opaque data blob, not a vtable) = 7 slots.
 *     Matches CModule's own 7-slot count -- plausible (dtor pair + Setup-analog +
 *     Exec() (RunLevel()'s own +8 dispatch, confirmed elsewhere) + further named
 *     methods this pass didn't trace individually, e.g. CTask::SetMask() (found via
 *     CPoller::CPoller(), see task.h)).
 *   TNamedPtrArray<COutLink>  08e82198 -> 08e821a4 (TVector<SRegisteredIfc,1>
 *     typeinfo) = 3 slots. Installed into BOTH of CTask's own embedded
 *     COmegaPtrArray sub-objects (+0xc and +0x24) -- confirmed via the real ctor
 *     writing this same address into both offsets (task.cpp).
 *   TVector<CTask::SRegisteredIfc,1>  08e82188 -> 08e82190 (TNamedPtrArray<COutLink>
 *     vtable) = 2 slots. Installed at CTask+0x50 (the RegisteredIfc vector
 *     RegisterIfc() would grow -- RegisterIfc itself stays Tier B, see task.h).
 *   CLimiterMan  08e81ee8 -> 08e81ef8 (CMarshaller<ILimiterNotify> vtable) = 4 slots.
 *     Installed at CTask+0x60 (the embedded CLimiterMan sub-object every CTask
 *     ctor constructs unconditionally).
 *   TVector<CLimiterBase*,1>  08e81f78 -> 08e81f80 (own typeinfo) = 2 slots.
 *     Installed at CLimiterMan+0x08 (its own embedded TVector). Same 2-slot count
 *     as every other TVector<T,1> instantiation seen in this file -- a real,
 *     cross-confirming pattern, not a coincidence.
 *
 * None of these 5 are ever actually dispatched through by any code in this
 * reconstruction (CTask's own vtable slots would only fire via CLevelManager::
 * RunLevel(), which stays real-but-unreached pending a live task-scheduling boot
 * path -- see level_manager_array.h; CLimiterMan's own vtable would only fire via
 * ~CLimiterMan(), not reconstructed since nothing in this reconstruction calls
 * ~CTask()) -- same "install-only, real slot count" status as this file's other
 * entries. DAT_08e82144 itself (CTask+0x08, a plain data symbol, not a class
 * vtable) is represented by EvaDataPlaceholder_08e82144 below -- its own contents
 * are never decoded or dereferenced by any reconstructed code, only its address is
 * ever stored.
 *
 * Added in the Stage 6 breadth-sweep pass (2026-07-25), CSysApiInstance::RegisterApi()
 * batch -- direct raw-byte read (not the usual next-symbol heuristic, since a second
 * class's own vtable header immediately follows, same trap already hit once for
 * CResMan -- see the agent-memory writeup for that batch):
 *
 *   CApiDescriptor  08e81368 -> 08e81370 (own offset-to-top; this next word is
 *     itself CNamedObjectBase's OWN vtable header, confirmed via a direct
 *     `objdump -s -j .rodata` byte read of 08e81360..08e813a4: word0/word1 at
 *     08e81360/08e81364 are CApiDescriptor's own offset-to-top(0)/typeinfo-ptr,
 *     word2/word3 at 08e81368/08e8136c are its 2 real dtor slots (complete-object,
 *     deleting), and word4 at 08e81370 is ALREADY the next class's offset-to-top(0)
 *     -- so CApiDescriptor's own installed-vtable-pointer range is genuinely just
 *     2 slots, not sized by proximity to CNamedObjectBase's install point
 *     (08e81378, 2 words further still) the way a naive "next PTR__ label" grep
 *     would suggest. This is the real vtable CSysApiInstance::RegisterApi()
 *     (sysapi_instance.cpp) installs on every fresh API-descriptor object it
 *     builds -- both slots are the real ~CApiDescriptor() dtor pair, neither
 *     reconstructed (genuinely out of scope), so both stay EvaVTableStub-backed;
 *     safe because RegisterApi()'s own real dtor-invoking path (replacing an
 *     already-registered API under a different pointer) is never exercised by
 *     this pass's own 7 real boot-path callers, each of which registers a
 *     distinct, never-repeated name (mains.cpp).
 *
 * Every slot points at the same no-op stub (EvaVTableStub, cdecl, zero declared
 * parameters) -- safe under the real cdecl calling convention regardless of how many
 * args/regs a caller's own Fn-typedef pushes, since cdecl callees never pop caller-
 * pushed arguments themselves. This is "shape faithful" (real slot COUNT, matching the
 * real binary's own vtable size) but explicitly NOT "behavior faithful" -- every slot
 * is a safe no-op, not the real virtual method. Anything that ever actually dispatches
 * through one of these at runtime silently does nothing rather than reading garbage
 * memory or crashing; still not something to rely on for correctness.
 */

#ifndef OMEGA_VTABLES_H
#define OMEGA_VTABLES_H

extern "C" {
void EvaVTableStub();
void *GetFMApiStub(void *);
extern void *PTR__CHostInterfaceBase_08e80b68[22];
extern void *PTR__CHostInterface_08e80b08[22];
extern void *PTR__COmegaPtrArray_08e80be0[4];
extern void *PTR__TNamedPtrArray_08e80bf8[4];
extern void *PTR__TNamedPtrArray_08e80c10[4];
extern void *PTR__TPtrArray_08e80c40[4];
extern void *PTR__TVector_08e80c58[2];
extern void *PTR__CDummyMsgInput_08e80c80[3];
extern void *PTR__CNamedObjectBase_08e81378[2];
extern void *PTR__CTracer_08e81468[3];
extern void *PTR__CLevelManagerArray_08e80c28[4];
/* Verified 2026-07-25 (Eva full-state re-verification pass): ground truth's real
 * CLevelManager vtable is only 3 slots (dtor-complete @0817ccb0, dtor-deleting
 * @0817cd20, ExecMsg @0805e7d0) before RTTI data (_ZTS/_ZTI13CLevelManager) begins
 * at 08e80e5c -- confirmed via objdump -s .rodata byte read. StopForMessage/
 * ResumeAfterMessage/RunLevel are real CLevelManager methods but NOT virtual (no
 * vtable slot backs them -- called directly). The array was previously declared
 * [20], which happened to still end exactly on the next class's real vtable
 * boundary (08e80ea0) but wrongly modeled 17 bytes of RTTI/next-header data as
 * placeholder vtable slots. Harmless in practice (only ever used as
 * `*(void**)lm = PTR__CLevelManager_08e80e50` in scheduler.cpp, an address-only
 * assignment, never indexed past slot 0), but corrected to [3] to match the real
 * boundary and avoid misleading future indexed access.
 */
extern void *PTR__CLevelManager_08e80e50[3];
extern void *PTR__TNamedPtrArray_08e80ea8[3];
extern void *PTR__TPtrArray_08e80bc8[4];
extern void *PTR__CSysApiInstance_08e81008[94];
extern void *PTR__TNamedPtrArray_08e811a8[4];
extern void *PTR__TNamedPtrArray_08e811c0[8];
extern void *PTR__CApiDescriptor_08e81368[2];
extern void *PTR__CModule_08e81fe8[7];
extern void *PTR__CEditMan_08e85ea8[7];
extern void *PTR__CMessagePort_08e88468[13];
extern void *PTR__CSeqTimer_08e892a8[7];
extern void *PTR__CSysEx_08e899e8[7];
extern void *PTR__CChunkMan_08e85968[7];
extern void *PTR__CDumpManMod_08e85ca8[7];
extern void *PTR__CTask_08e82128[7];
extern void *PTR__TNamedPtrArray_08e82198[3];
extern void *PTR__TVector_08e82188[2];
extern void *PTR__CLimiterMan_08e81ee8[4];
extern void *PTR__TVector_08e81f78[2];
extern int   EvaDataPlaceholder_08e82144;

/* Added for CTask::~CTask()/CLimiterMan::~CLimiterMan() (Stage 6, 2026-07-25). All
 * three identified by reading `vtable for X` / `typeinfo for X` symbols directly out
 * of .rodata via nm -C (the object's own real vptr value is always vtable-symbol+8,
 * standard Itanium ABI layout: [offset-to-top][typeinfo-ptr][vfunc0]...) -- none are
 * ever dispatched through by any reconstructed code, only ever INSTALLED as the final
 * mVtbl identity once destruction reaches that level, so a 1-slot array is enough:
 *
 *   CObjectBase   (08e79d60 = vtable for CObjectBase, +8 = 08e79d68) -- the ultimate
 *     root base beneath CNamedObjectBase (task.h/module.h's own base), confirmed via
 *     its typeinfo string "11CObjectBase"; genuinely 0 own virtual function slots
 *     (RTTI-only root).
 *   CIfcUnknown   (08e81d78 = vtable for CIfcUnknown, +8 = 08e81d80) -- CLimiterMan's
 *     own further base (limiter_man.h); confirms CLimiterMan IS-A CIfcUnknown, matching
 *     CTask::CTask()'s own `RegisterIfc(reinterpret_cast<CIfcUnknown*>(mLimiterMan))`
 *     call (task.cpp).
 *   CMessageInput (08e80c60 = vtable for CMessageInput, +8 = 08e80c68) -- the real
 *     identity CTask::~CTask() installs into CTask's own +0x08 field (task.h's
 *     "mIfcThunk") right before finishing -- i.e. mIfcThunk is genuinely a secondary
 *     (multiple-inheritance, this-adjusted) vtable slot for a CMessageInput-derived
 *     interface CTask itself implements, not generic opaque data. CMessageInput itself
 *     is not reconstructed (out of scope, unrelated subsystem -- CDummyMsgInput/
 *     CMessageInput cluster, see this header's own slot-count survey above), so this
 *     stays a raw identity marker like the other two.
 */
extern void *PTR__CObjectBase_08e79d68[1];
extern void *PTR__CIfcUnknown_08e81d80[1];
extern void *PTR__CMessageInput_08e80c68[1];

/* CSysExMsgTaskBase's own real vtable pair (Stage 6 SetMask/~CTask batch,
 * 2026-07-25, sysex_msg_task_base.h) -- primary at 08e84c20+8=08e84c28 (13 slots,
 * boundary = own typeinfo-name symbol 08e84c5c per this file's established
 * methodology), secondary (the CTask+0x8-equivalent, this-adjusted, multiple-
 * inheritance slot -- same shape as CTask's own EvaDataPlaceholder_08e82144) at
 * 08e84c50, treated as an opaque scalar placeholder like CTask's own secondary vtable
 * already is, not a named array -- never dispatched through by any reconstructed
 * code.
 */
extern void *PTR__CSysExMsgTaskBase_08e84c28[13];

/* CAlphaKeybIfcTask's own real 3-vtable cluster (Stage 6 breadth sweep,
 * 2026-07-25) -- see omega_vtables.cpp's own comment for the full byte-read
 * derivation. alpha_keyb_ifc_task.h/.cpp are the only consumers.
 */
extern void *PTR__CAlphaKeybIfcTask_08f25ae8[6];
extern void *PTR__CAlphaKeybIfcTask_08f25b08[3];
extern void *PTR__CAlphaKeybIfcTask_08f25b1c[4];
extern int   EvaDataPlaceholder_08e84c50;

/* CEditServer's own real vtable (edit_server.h, Stage 6 breadth sweep,
 * 2026-07-25 -- MMainESCommon/MMainESGlobal survey batch): primary at
 * 08e817a8+8=08e817b0 (7 slots, boundary = the next real symbol,
 * typeinfo-name for CEditServer at 08e817c4, per this file's established
 * methodology). The embedded COmegaPtrArray subobject's own vtable-swap
 * target, TPtrArray<SDescriptorTable> (_ZTV9TPtrArrayI16SDescriptorTableE,
 * 08e817e0+8=08e817e8, 3 slots -- same size as every other
 * TPtrArray<T>/TNamedPtrArray<T> flavor already declared below, e.g.
 * PTR__TNamedPtrArray_08e80ea8).
 */
extern void *PTR__CEditServer_08e817b0[7];
extern void *PTR__TPtrArray_08e817e8[3];

/* COutLink/COutLinkMono/CSysExMsgOutLink/CSysExMsgClientOutLink family (Eva Stage 6
 * follow-up, 2026-07-25 -- client_comm_server.h/sysex_msg_task_base.h's own
 * CSysExMsgClientOutLink batch: CTask::Add(COutLink*)'s own real dependency chain,
 * COutLinkMono::OutMono() reconstructed for real this same pass). All 4 confirmed via
 * `nm -C Eva`'s own "vtable for X" symbol (X's PTR__ address = that symbol + 8, same
 * Itanium-ABI tell already used for CObjectBase/CIfcUnknown/CMessageInput above), slot
 * counts by the same next-vtable-or-typeinfo-boundary methodology as the rest of this
 * file:
 *   COutLinkMono   08e82040 -> 08e82060 (COutLink)                    = 6 slots
 *   COutLink       08e82060 -> 08e82080 (typeinfo cluster)            = 6 slots
 *   TPtrArray<CLink> 08e820d0 -> 08e820e4 (typeinfo cluster)          = 3 slots
 *     (COutLink's own embedded COmegaPtrArray sub-object -- confirmed real class name
 *     `CLink`, i.e. the descriptor COutLinkMono::OutMono() writes through via its own
 *     `this+0x34` pointer is a `CLink*`, out of scope, see client_comm_server.h)
 *   CSysExMsgClientOutLink 08e84b00 -> 08e84b20 (CSysExMsgOutLink)    = 8 slots
 *   CSysExMsgOutLink       08e84b20 -> 08e84b40 (CSysExClientOutLink) = 8 slots
 * All install-only, same status as this file's other entries -- no reconstructed code
 * dispatches through any of them.
 */
extern void *PTR__COutLinkMono_08e82048[6];
extern void *PTR__COutLink_08e82068[6];
extern void *PTR__TPtrArray_08e820d8[3];
extern void *PTR__CSysExMsgClientOutLink_08e84b08[8];
extern void *PTR__CSysExMsgOutLink_08e84b28[8];

/* COutLinkMulti's own real vtable (out_link.h, Eva "size is not depth" re-check
 * batch, 2026-07-26). 08e82028 -> 08e82040 (COutLinkMono) = 6 slots, matching
 * COutLink's own family-wide 6-slot size exactly. Install-only -- CBatchDiskMainTask
 * only constructs+CTask::Add()s this object, never calls OutMulti().
 */
extern void *PTR__COutLinkMulti_08e82028[6];

/* CCircByteBuffer/CDumpBuffer/CDumpManStateMachine/CDumpMachine/CDumpTask/
 * CBufferingTask (Stage 6 breadth sweep, 2026-07-25, DumpManager cluster batch --
 * circ_byte_buffer.h/dump_buffer.h/dump_man_state_machine.h/dump_task.h/
 * buffering_task.h). All 6 confirmed via `nm -C Eva`'s own "vtable for X" symbol
 * (X's PTR__ address = that symbol + 8, same Itanium-ABI tell used throughout this
 * file), slot counts by the same next-symbol-boundary methodology as the rest of
 * this file (all 6 sit in one contiguous 08e85aa0..08e85d90 run):
 *   CBufferingTask         08e85aa0 -> 08e85ae0 (vtable for CDumpApi)         = 14 slots
 *   CCircByteBuffer        08e85b60 -> 08e85b74 (typeinfo name)              =  3 slots
 *   CDumpBuffer            08e85c08 -> 08e85c1c (typeinfo name)             =  3 slots
 *   CDumpMachine           08e85c40 -> 08e85c70 (typeinfo name)             = 10 slots
 *   CDumpManMod            08e85ca0 -> 08e85cc4 (typeinfo name)             =  7 slots
 *     (already declared above, Stage 6 CModule/CTaskBuffer/RunLevel batch -- this
 *     batch changes its DEFINITION, not its declaration, see omega_vtables.cpp/
 *     dump_man_mod.cpp: slots 2/3/4 (Setup/Config/Start) now wired to real
 *     forwarders, direct .rodata byte read confirms slots 0/1/5/6 are CModule's own
 *     dtor pair/Destroy/GetErrorMsg -- see dump_man_mod.cpp's own header comment)
 *   CDumpManStateMachine   08e85ce0 -> 08e85d10 (typeinfo name)             = 10 slots
 *   CDumpTask              08e85d40 -> 08e85d80 (typeinfo name)             = 14 slots
 * Every one except CDumpManMod's slots 2/3/4 is install-only (EvaVTableStub-backed),
 * same status as every other entry in this file -- none of the deep ~30-method
 * `CDumpManStateMachine` state-handler overrides these arrays would need to be
 * individually correct for are dispatched through anywhere in this reconstruction.
 * Secondary (this-adjusted, multiple-inheritance) vtable placeholders for
 * `CBufferingTask`/`CDumpTask` (their own `this+8` field, same pattern as CTask's own
 * `EvaDataPlaceholder_08e82144`/CSysExMsgTaskBase's own `EvaDataPlaceholder_08e84c50`):
 *   EvaDataPlaceholder_08e85ac4  (CBufferingTask, real address per its own ctor)
 *   EvaDataPlaceholder_08e85d74  (CDumpTask, real address per its own ctor)
 */
extern void *PTR__CBufferingTask_08e85aa8[14];
extern void *PTR__CCircByteBuffer_08e85b68[3];
extern void *PTR__CDumpBuffer_08e85c10[3];
extern void *PTR__CDumpMachine_08e85c48[10];
extern void *PTR__CDumpManStateMachine_08e85ce8[10];
extern void *PTR__CDumpTask_08e85d48[14];
extern int   EvaDataPlaceholder_08e85ac4;
extern int   EvaDataPlaceholder_08e85d74;

/* CEditMan::CMainTask/CChkBaseTask/CChkCmd (Stage 6 breadth sweep, 2026-07-25,
 * small-derived-module follow-up batch -- edit_man.h/chunk_man.h). Same
 * installed-pointer-to-next-symbol methodology as the rest of this file:
 *   CMainTask     08e85ee8 -> 08e85f04 (own opaque +8 data blob)         =  7 slots
 *     (adds no new virtuals beyond CTask's own 7 -- every CMainTask method is a
 *     plain non-virtual thiscall function, confirmed by direct-call, not
 *     vtable-indirect, shape in every one of its own decompiles)
 *   TPtrArray<CEditClient> 08e85f40 -> 08e85f4c (typeinfo)               =  3 slots
 *     (CMainTask's own embedded client-list COmegaPtrArray, +0x27c)
 *   CChkBaseTask  08e85648 -> 08e85668 (own opaque +8 data blob)         =  8 slots
 *     (1 more than CTask's own 7 -- CChkBaseTask genuinely adds one new virtual;
 *     never dispatched through by any reconstructed code)
 *   TPtrArray<CRegistrationEntry> 08e85698 -> 08e856a4 (typeinfo)        =  3 slots
 *     (CChkBaseTask's own embedded COmegaPtrArray(10,5,1), +0x7c)
 *   CChkCmd       08e85708 -> 08e85728 (own opaque +8 data blob)         =  8 slots
 *     (same 8-slot count as its own CChkBaseTask base -- CChkCmd adds no further
 *     new virtuals of its own)
 * All install-only (EvaVTableStub-backed), same status as every other entry in
 * this file.
 *
 * CChkCmdBG (2026-07-25 follow-up, now fully reconstructed -- see chunk_man.h):
 *   CChkCmdBG     08e85768 -> 08e85788 (own this-adjusted secondary vtable)  = 8 slots
 *     (direct .rodata byte read confirms the 8-dword size exactly: 6 real
 *     function slots -- non-deleting dtor, deleting dtor, Exec@08180950,
 *     ExecMsg@0807e170, Exec(CMessage&)-shaped@080c6d50, AcceptDuplicate@08185d60,
 *     all out of scope per chunk_man.h -- followed immediately by the
 *     this-adjusted (-8) secondary vtable's own [offset_to_top][RTTI] preamble;
 *     `08e85788` is thus the genuine start of that secondary vfunc array, not a
 *     plain opaque data blob by coincidence -- still install-only/EvaVTableStub-
 *     backed here since nothing dispatches through it on the traced boot path)
 */
extern void *PTR__CMainTask_08e85ee8[7];
extern void *PTR__TPtrArray_08e85f40[3];
extern int   EvaDataPlaceholder_08e85f04;
extern void *PTR__CChkBaseTask_08e85648[8];
extern void *PTR__TPtrArray_08e85698[3];
extern int   EvaDataPlaceholder_08e85668;
extern void *PTR__CChkCmd_08e85708[8];
extern int   EvaDataPlaceholder_08e85728;
extern void *PTR__CChkCmdBG_08e85768[8];
extern int   EvaDataPlaceholder_08e85788;

/* CTimerEngine/CWheelsContainer/CExternalClock/CInternalClock/CSyncRXInterface/
 * CSyncTXInterface/CClockBase cluster (Stage 6 breadth sweep, 2026-07-25 follow-up
 * batch -- timer_engine.h, unblocks CSeqTimer::Setup()). Direct .rodata dword reads
 * (the naive "next symbol" heuristic is actively wrong here -- typeinfo/typeinfo-name
 * objects for MULTIPLE classes are grouped together in .rodata immediately after a
 * short run of adjacent vtables, same trap as CApiDescriptor/CResMan):
 *
 *   CTimerEngine       08e896c8 -> DAT_08e896e4 (opaque secondary-vtable target,
 *                      same idiom as CTask's own +0x08 identity)       =  7 slots
 *   CExternalClock     08e897a8 -> 08e897c0 (own typeinfo-name)        =  6 slots
 *   CInternalClock     08e89888 -> 08e898a0 (own typeinfo-name)        =  6 slots
 *   CClockBase         08e891e8 -> 08e89200 (typeinfo cluster start)   =  6 slots
 *                      (2 real dtor slots + 4 IDENTICAL __cxa_pure_virtual-shaped
 *                      PLT-stub entries at 0x0804c6ac -- CExternalClock/
 *                      CInternalClock each override those same 4 slots)
 *   CSyncRXInterface   08e89748 -> 08e8975c (typeinfo cluster start)   =  5 slots
 *   CSyncTXInterface   08e89198 -> 08e891c0 (CVirtualClock's own vtable header) =
 *                      10 slots (4 real function pointers + 6 literal zero words
 *                      whose exact cause isn't resolved -- not the CClockBase-style
 *                      repeated-PLT-stub pattern; sized to the confirmed real
 *                      boundary regardless, per this file's "avoid an undersized
 *                      array" discipline)
 * All 6 confirmed install-only (never dispatched through by any reconstructed code
 * on this pass's own traced boot path) -- same status as everything else in this
 * file. CVirtualClock (08e891c8, 6 slots) and CWheel (08e897e8, 5 slots) are also
 * visible in this same .rodata run but NOT declared here -- nothing this pass
 * reconstructs installs either of their vtables (CWheelsContainer only ever stores
 * null CWheel* pointers on the traced path; CVirtualClock is never referenced).
 */
extern void *PTR__CTimerEngine_08e896c8[7];
extern int   EvaDataPlaceholder_08e896e4;
extern void *PTR__CExternalClock_08e897a8[6];
extern void *PTR__CInternalClock_08e89888[6];
extern void *PTR__CClockBase_08e891e8[6];
extern void *PTR__CSyncRXInterface_08e89748[5];
extern void *PTR__CSyncTXInterface_08e89198[10];

/* CFileMan/CResMan/CChunkOnDemand cluster (Stage 6 breadth sweep, 2026-07-25 --
 * file_man.h/res_man.h/chunk_on_demand.h, the "What's still open" CFileMan/CResMan
 * ctor batch). Unlike every other MMainXxx(void) module in mains.cpp, these two
 * derived-class ctors ARE real and ARE reconstructed here -- the note 2 batches above
 * ("CFileMan/CResMan's own precedent... of not declaring a vtable for a derived class
 * whose own ctor is never really called") describes mains.cpp's OLD, now-superseded
 * state; both now install real vtables from inside their own real ctor bodies (matching
 * ground truth exactly -- see file_man.cpp/res_man.cpp). Same installed-pointer-to-
 * next-symbol methodology as the rest of this file:
 *   CFileMan       08e86e40 -> 08e86f24 (typeinfo name for CFMDriverInterface) = 55
 *     slots. A genuinely huge god-object vtable (CFileMan itself has ~60 further
 *     methods -- FDisk/RegisterDriver/ScanPartitionTable/... -- none reconstructed,
 *     same "CForm/Peg-scale, indefinitely deferred" boundary as the ES-family
 *     CXxxTask god-objects). Slots 2/3/4 (Setup/Config/Start) wired to real
 *     forwarders (file_man.cpp: all 3 are confirmed genuinely empty `return 0;`
 *     bodies in the real binary) -- everything else stays EvaVTableStub.
 *   TNamedPtrArray<CFMDriverConstructor>  08e86fc0 -> 08e86fe0 (vtable for
 *     CFileManVirtual) = 6 slots. CFileMan's own embedded driver-constructor
 *     registry (its ctor installs this at its `mDriverConstructors` member,
 *     file_man.h) -- confirmed real element type by name, matching
 *     AddDriverConstructor()/RemoveDriverConstructor()/FindDriverConstructor()
 *     (none reconstructed).
 *   TPtrArray<CChunkOnDemand::STripletOnDemand>  08e88598 -> 08e885ac (own
 *     typeinfo) = 3 slots. CChunkOnDemand's own embedded array (chunk_on_demand.h).
 *   CResMan  -- CORRECTED after a direct raw `.rodata` byte read (not just the
 *     next-symbol-boundary heuristic, since this one turned out to have real
 *     internal structure the heuristic alone would have gotten wrong): the
 *     08e88b00..08e88b5c region is NOT one flat 21-slot array. It's a genuine
 *     Itanium-ABI primary+secondary vtable pair sharing one typeinfo:
 *       - primary header  08e88b00/04 = {offset-to-top=0, typeinfo="CResMan"}
 *       - primary vfuncs  08e88b08..08e88b38 = 12 slots, ALL individually
 *         confirmed by address: {~CResMan, ~CResMan(deleting), Setup, Config,
 *         Start, CModule::Destroy, CModule::GetErrorMsg, OnSave, OnDelete,
 *         OnLoad, OnSetRes, OnLoadRes} -- 7 CModule-shaped slots (matching
 *         module.h's own layout exactly) plus 5 real CResMan-specific
 *         overrides. Slot 4 (Start) wired to a real forwarder (res_man.cpp:
 *         confirmed genuinely empty `return 0;`). Setup()/Config() (slots 2/3)
 *         are real but genuinely deeper (Setup constructs CResChkServer/
 *         CResChkClient/CRMMainTask; Config depends on ChkApi + 2 undecoded
 *         `sm_pkcTaskName` statics) -- out of scope, stay EvaVTableStub.
 *         OnSave/OnDelete/OnLoad/OnSetRes/OnLoadRes (slots 7-11, 272-2023
 *         bytes each) are real CResMan callback handlers, also genuinely out
 *         of scope for this ctor-focused pass -- stay EvaVTableStub too.
 *       - secondary header  08e88b38/3c = {offset-to-top=-0x2c, typeinfo=
 *         same "CResMan"} -- the -0x2c confirms this is CResMan's own +0x2c
 *         `CRMApiCallBack`-interface sub-object's this-adjustment, exactly
 *         matching res_man.h's own `mCallbackVtbl` field offset.
 *       - secondary vfuncs  08e88b40..08e88b5c = 7 slots, all confirmed
 *         `non-virtual thunk to CResMan::Xxx` entries (this-adjusted dtor +
 *         OnSetRes/OnLoadRes/OnLoad/OnSave/OnDelete) -- never dispatched
 *         through by any reconstructed code either.
 *     Declared as ONE 21-element `void*` array anyway (`PTR__CResMan_08e88b08`,
 *     indices 0-20 spanning the full 08e88b08..08e88b5c byte range) purely so
 *     the ADDRESS arithmetic for `&PTR__CResMan_08e88b08[14]` (== 0x08e88b40,
 *     the real secondary vfunc0 -- res_man.cpp's own `mCallbackVtbl` final
 *     write) stays correct and self-documenting; indices 12/13 are NOT
 *     function pointers (they hold the secondary header's own offset-to-top/
 *     typeinfo words in the real binary) and are declared `0`, not
 *     EvaVTableStub, precisely so nothing mistakes them for callable slots
 *     (same "opaque, not a vtable" precedent as this file's own
 *     `EvaDataPlaceholder_*` scalars).
 *   TPtrArray<CRMResult::SSingleError>  08e88bb0 -> 08e88bc4 (typeinfo for
 *     TVector<CResEntryEx,1>) = 3 slots. CResMan's own embedded `mResults` array.
 *   TVector<CResEntryEx,1>  08e88ba0 -> 08e88bb0 (the TPtrArray entry above) =
 *     2 slots -- matches every other `TVector<T,1>` instantiation already in this
 *     file. Installed 10 times into CResMan's own trailing region (res_man.cpp).
 *
 * BUG FIX (same batch): `PTR__CRMApiCallBack_08e886e8` (mains.cpp,
 * ConstructRMApiInstance's own transient +4 slot) was a bare scalar `void* = 0` --
 * the same undersized-vtable-array bug class found repeatedly elsewhere in this
 * project (WORKAROUND #1/#2/#3, mains.cpp). Harmless there today (RMApiInstance
 * overwrites it before anything could dispatch through it), but CResMan::CResMan()
 * needed this exact vtable's own real slot count anyway (08e886e0 -> 08e88704 =
 * 7 slots, confirmed by the same next-symbol methodology) to confirm the CResMan
 * analysis, so it's fixed to a real, properly-sized array here at zero extra
 * cost -- moved from a `void*` scalar to a 7-slot array, still install-only
 * (this is CResMan's OWN transient value before its final vtable install, an
 * entirely separate object from the secondary-vtable derivation above).
 */
extern void *PTR__CFileMan_08e86e48[55];
extern void *PTR__TNamedPtrArray_08e86fc8[6];
extern void *PTR__TPtrArray_08e885a0[3];
extern void *PTR__CResMan_08e88b08[21];
extern void *PTR__CRMApiCallBack_08e886e8[7];
extern void *PTR__TPtrArray_08e88bb8[3];
extern void *PTR__TVector_08e88ba8[2];

/* CEditor's own real vtable cluster (editor.h, Stage 6 CEditor batch, 2026-07-25):
 * a real GCC multiple-inheritance layout with ONE primary vtable plus TWO
 * this-adjustment secondary ("virtual thunk") vtables, all three confirmed by a
 * direct .rodata dword read at 0x08f29b80 (Eva) -- NOT inferred from call sites,
 * per this project's own recurring-bug-class discipline:
 *
 *   Primary          08f29b80 -> +8 = 08f29b88 install target, 7 slots (same
 *                     CModule-shape dtor/dtor-deleting/Setup/Config/Start/Destroy/
 *                     GetErrorMsg as every other CModule-derived vtable in this
 *                     project): {~CEditor, ~CEditor(deleting), CEditor::Setup,
 *                     CEditor::Config, CEditor::Start, CModule::Destroy,
 *                     CModule::GetErrorMsg} -- the last 2 are CModule's own
 *                     UN-overridden real implementations (module.h), not stubs.
 *   CEditClient thunk 08f29ba4 -> +8 = 08f29bac install target (this-adjust
 *                     -0x2c, matching the CEditClient sub-object's real offset
 *                     within CEditor), 3 slots: {~CEditor thunk, ~CEditor
 *                     (deleting) thunk, CEditClient::OnNotify} -- OnNotify itself
 *                     is a Tier-B stub (edit_man.h), consistent with slot content
 *                     rather than a fabricated one.
 *   CEditServer thunk 08f29bb8 -> +8 = 08f29bc0 install target (this-adjust
 *                     -0x38, matching the CEditServer sub-object's real offset),
 *                     5 slots: {~CEditor thunk, ~CEditor (deleting) thunk,
 *                     CEditServer::Get, CEditServer::Set,
 *                     CEditServer::SetDefault} -- Get/Set/SetDefault are
 *                     CEditServer's OWN already-real methods (edit_server.h),
 *                     confirmed identical addresses to CEditServer's own
 *                     stand-alone vtable content (PTR__CEditServer_08e817b0
 *                     above), just re-exposed here through CEditor's own
 *                     this-adjusted view -- same object, same functions, two
 *                     vtables per the Itanium ABI's multiple-inheritance rule.
 *
 * FIX (2026-07-26, follow-up to the CPanel unlock batch): the primary array
 * (PTR__CEditor_08f29b88) is NOT install-only -- `CModuleManager::Setup()`/
 * `Config()`/`Start()` (module_manager.cpp, Tier A/real) genuinely
 * raw-dispatch through slots 2/3/4 (byte offsets 8/0xc/0x10) for every module
 * in the real `mModules` array, and `CConfigManager::CreateUserModules()`
 * (also Tier A/real) adds a freshly-constructed `CEditor` straight into that
 * array. Slots 2/3/4 are wired to real forwarders (CEditorSetupVSlot/
 * CEditorConfigVSlot/CEditorStartVSlot, omega_vtables.cpp) instead of
 * EvaVTableStub -- see PTR__CPanel_08f7c328's own header comment below for
 * the full precedent this follows (same fix, same reasoning, found to apply
 * here too via a dedicated ground-truth re-check rather than assumed). Slots
 * 0/1/5/6 (dtor/dtor-deleting/Destroy/GetErrorMsg) stay EvaVTableStub for the
 * same "genuinely never dispatched by any reconstructed code" reason
 * CPanel's own dtor/Destroy/GetErrorMsg slots do.
 *
 * The two secondary (this-adjustment thunk) arrays below, CEditClient thunk
 * (08f29bac) and CEditServer thunk (08f29bc0), remain confirmed install-only
 * -- nothing in this reconstruction dispatches through CEditor via either of
 * its non-primary base-class views.
 */
extern void *PTR__CEditor_08f29b88[7];
extern void *PTR__CEditor_08f29bac[3];
extern void *PTR__CEditor_08f29bc0[5];

/* CEditor::CPanelIfcTask + CPanelCfg (Stage 6 dedicated CPanelIfcTask batch,
 * 2026-07-25 -- panel_ifc_task.h/.cpp). Both confirmed byte-exact via direct
 * `.rodata` dword read (not inferred), cross-checked against the next real
 * symbol's own address (typeinfo-name strings in both cases):
 *   CPanelIfcTask primary   08f29ce0 -> 08f29d10 (typeinfo name)   = 7 slots
 *     (5 real function slots -- D1/D0 dtor, Exec() 0-arg override, Exec
 *     (CMessage&) override, ExecMsg(CMessage&) copied through unchanged from
 *     CTask's own vtable -- plus 2 header/this+8-install slots, same
 *     5-real+2-header bucketing convention as CTask's own
 *     PTR__CTask_08e82128[7] above)
 *   CPanelCfg               08f29d40 -> 08f29d60 (typeinfo)        = 6 slots
 *     (CPanelCfg overrides only its own D1/D0 dtor; CheckDestinationFamily/
 *     OnCreateLink/OnConnect/OnDisconnect are copied through from
 *     COutLinkMono's own vtable unchanged -- confirmed via direct byte
 *     comparison against PTR__COutLinkMono_08e82048 above)
 * `EvaDataPlaceholder_08f29d04` is CPanelIfcTask's own this+8 install target
 * (real address &DAT_08f29d04, i.e. exactly one past the primary array's own
 * end) -- never dereferenced by any reconstructed code, same status as
 * CTask's own EvaDataPlaceholder_08e82144. Both vtables install-only
 * (EvaVTableStub-backed), same convention as every other entry in this file.
 */
extern void *PTR__CPanelIfcTask_08f29ce8[7];
extern void *PTR__CPanelCfg_08f29d48[6];
extern int   EvaDataPlaceholder_08f29d04;

/* CChunkServer / CEditor::CChunkServerTask -- CEditor::Setup()'s own LAST
 * previously-deferred fan-out target (editor.h/chunk_server.h, 2026-07-26 --
 * the "not tractable this pass" note the original dedicated CEditor batch
 * left has been revisited and closed). Both classes share the SAME real
 * 16-slot primary group (2 dtor + CTask::Exec()/Exec(CMessage&)/
 * CTask::ExecMsg() (5-slot CTask-family header, matching PTR__CTask_08e82128's
 * own established shape) + CChunkServer's own 11 new virtuals:
 * GetSaveBuffSize/OnUnlock/OnRelock/OnBegin/OnEnd/OnSave(CChunk*)/
 * OnSave(ulong&,uchar const*&)/OnLoad(CChunk*)/OnLoad(ulong,uchar*)/OnAbort/
 * OnStoppedByUser) plus a 3-slot secondary ("mIfcThunk", this+8) group (2
 * dtor thunks + an ExecMsg thunk, same shape as CTask/CPanelIfcTask/
 * CAlphaKeybIfcTask's own secondary groups) -- ALL confirmed by a direct
 * `.rodata` dword read (not inferred), CChunkServer at 0x08e859a0 (install
 * points 0x08e859a8/0x08e859f0) and CEditor::CChunkServerTask at 0x08f25b78
 * (install points 0x08f25b88/0x08f25bd0). CChunkServerTask's own vtable
 * re-emits the FULL 16-slot group (standard GCC single-inheritance-derived-
 * class vtable shape) with only GetSaveBuffSize/OnSave(CChunk*)/OnLoad(CChunk*)
 * replaced by its own overrides (slots 5/10/12) -- every other slot is the
 * SAME address as CChunkServer's own (byte-for-byte confirmed, not just
 * assumed from "not overridden"). Both vtables install-only (EvaVTableStub-
 * backed, same convention as every other array in this file) -- nothing in
 * this reconstruction's own call graph dispatches through either vtable yet
 * (CEditor::Setup() only constructs+Add()s a CChunkServerTask, it never calls
 * any of its chunk-protocol methods).
 */
extern void *PTR__CChunkServer_08e859a8[16];
extern void *PTR__CChunkServer_08e859f0[3];
extern void *PTR__CChunkServerTask_08f25b88[16];
extern void *PTR__CChunkServerTask_08f25bd0[3];

/* CPoller / CPoller::CIfcClient -- see poller.h's own header comment for the
 * full byte-exact derivation (direct .rodata dword reads at 0x8f7c360/
 * 0x8f7c3a8/0x8f7c3c0, same technique as the CChunkServer entry above).
 * CPoller's own primary group is the SAME 5-slot CTask-family shape
 * (2 dtor + Exec()/Exec(CMessage&)/ExecMsg(CMessage&)) every CTask-derived
 * class in this project shares; its 3-slot secondary group ("mIfcThunk",
 * this+8) is likewise the same CTask-family shape. CIfcClient's own 6-slot
 * group (2 dtor + the 4 COutLinkMono virtuals it inherits unchanged --
 * CheckDestinationFamily/OnConnect/OnCreateLink/OnDisconnect, out_link.h)
 * matches PTR__COutLinkMono_08e82048's own established 6-slot size exactly
 * (same shape, since CIfcClient only overrides the 2 dtor slots). TVector<
 * CPoller::CIfcClient*,1>'s own 2-slot group (D1/D0 only) matches
 * PTR__TVector_08e82188's own established 2-slot size for the same reason
 * (task.h). All EvaVTableStub-backed, install-only.
 */
extern void *PTR__CPoller_08f7c368[5];
extern void *PTR__CPoller_08f7c384[3];
extern void *PTR__TVector_08f7c3b0[2];
extern void *PTR__CIfcClient_08f7c3c8[6];

/* CPanel's own real per-instance vtable (panel.h/panel.cpp, Eva Stage 6 CPanel unlock
 * batch, 2026-07-26). Confirmed byte-exact via direct `.rodata` dword read at
 * 0x08f7c320 -> +8 = 0x08f7c328 install target, 7 slots, the SAME CModule-shape
 * dtor/dtor-deleting/Setup/Config/Start/Destroy/GetErrorMsg layout as every other
 * CModule-derived per-instance vtable in this file (CEditor's PTR__CEditor_08f29b88
 * above, same 7-slot shape): {~CPanel, ~CPanel(deleting), CPanel::Setup,
 * CPanel::Config, CPanel::Start, CModule::Destroy, CModule::GetErrorMsg} -- the last
 * 2 are CModule's own un-overridden real implementations (module.h), not stubs,
 * confirmed identical to CModule's own PTR__CModule_08e81fe8[5]/[6] addresses.
 *
 * DEPARTURE FROM THIS FILE'S OWN "install-only, EvaVTableStub-backed" CONVENTION,
 * documented explicitly because it's a real, deliberate exception: slots 2/3/4
 * (Setup/Config/Start) are wired to real forwarders
 * (CPanelSetupVSlot/CPanelConfigVSlot/CPanelStartVSlot, omega_vtables.cpp), NOT
 * EvaVTableStub. Reason: `CModuleManager::Setup()/Config()/Start()`
 * (module_manager.cpp, already Tier A/real) genuinely dispatch through exactly
 * these 3 byte offsets (`CallVSlot(module, 8/0xc/0x10)`) for every module in the
 * real, now-populated `mModules` array -- and `CConfigManager::CreateUserModules()`
 * (config_manager.cpp, already Tier A/real) adds freshly-`CPanelConstructorCreate()`d
 * `CPanel` instances straight into `mModules`. Leaving these 3 slots as
 * EvaVTableStub (matching CEditor's own sibling array above) would make
 * `CPanel::Setup()` -- the one real, definitive ground-truth caller of
 * `CPoller::CPoller()`, per poller.h's own header comment -- silently unreachable
 * via the real dispatch path even after `CPanelConstructor::Create()` itself was
 * fixed, defeating the entire point of this batch. Dtor slots (0/1) stay
 * EvaVTableStub: nothing in `CModuleManager`'s own reconstructed methods ever
 * dispatches through a module's own dtor slot, and no module is ever destroyed on
 * Eva's own short-lived boot-and-exit run (same "not on this reconstruction's own
 * live boot path" status as `~CPanel()`/`~CPoller()` themselves). Destroy/
 * GetErrorMsg (slots 5/6) also stay EvaVTableStub for the same "genuinely never
 * dispatched by any reconstructed code" reason CModule's own base array leaves them
 * stubbed.
 *
 * FOLLOW-UP, FIXED (2026-07-26, same day): `PTR__CEditor_08f29b88` above had the
 * IDENTICAL real gap and has now been fixed the same way -- see its own header
 * comment above for the ground-truth re-verification (byte-identical vtable shape,
 * confirmed via a fresh `objdump -dr -M intel` read, not assumed from this comment).
 */
extern void *PTR__CPanel_08f7c328[7];

/* CBatchDiskMan's own real per-instance vtables (batch_disk_man.h/.cpp, Eva Stage 6
 * CBatchDiskMan unlock batch, 2026-07-26). Confirmed byte-exact via direct `.rodata`
 * dword reads at 0x08eac040 (`vtable for CBatchDiskMan`): primary group install
 * 0x08eac048 (7 slots, the SAME CModule-shape as CPanel/CEditor above: {~CBatchDiskMan,
 * ~CBatchDiskMan(deleting), CBatchDiskMan::Setup, CBatchDiskMan::Config,
 * CBatchDiskMan::Start, CModule::Destroy, CModule::GetErrorMsg}); secondary group
 * install 0x08eac06c (5 slots, the CEditServer-subobject shape: {dtor thunk pair,
 * CEditServer::Get, CEditServer::Set, CEditServer::SetDefault} -- same shape as
 * PTR__CEditServer_08e817b0, edit_server.h).
 *
 * SAME DEPARTURE as PTR__CPanel_08f7c328/PTR__CEditor_08f29b88 above, for the identical
 * reason: `CModuleManager::Setup()/Config()/Start()` genuinely dispatch through byte
 * offsets 8/0xc/0x10 of the PRIMARY group for every module in `mModules`, and
 * `CConfigManager::CreateUserModules()`'s own real "BatchDiskManClass" row (row 0,
 * config_info.cpp) adds a freshly-`CBatchDiskManConstructorCreate()`d `CBatchDiskMan`
 * straight into that array (once mains.cpp's own factory slot is wired -- see
 * batch_disk_man.cpp). Primary slots 2/3/4 wired to real forwarders
 * (CBatchDiskManSetupVSlot/CBatchDiskManConfigVSlot/CBatchDiskManStartVSlot,
 * omega_vtables.cpp); slots 0/1/5/6 stay EvaVTableStub for the same "never dispatched
 * by any reconstructed code" reason as CPanel/CEditor. The SECONDARY group (CEditServer
 * subobject) stays entirely EvaVTableStub-backed -- `CModuleManager` never dispatches
 * through a module's secondary vtable, matching CESCommon's own established treatment
 * of the identical CEditServer-shaped group (es_common.cpp).
 */
extern void *PTR__CBatchDiskMan_08eac048[7];
extern void *PTR__CBatchDiskMan_08eac06c[7]; /* 5 real slots, +2 headroom, same
                                                * "generously-sized" convention as
                                                * PTR__CEditServer_08e817b0. */

/* CEditTask's own real per-instance vtables (edit_task.h/.cpp, same batch). Confirmed
 * byte-exact via direct `.rodata` dword reads at 0x08eac1c0 (`vtable for CEditTask`):
 * primary group install 0x08eac1c8 (5 slots: {~CEditTask, ~CEditTask(deleting),
 * CTask::Exec(), CTask::Exec(CMessage&), CTask::ExecMsg(CMessage&)} -- the SAME 5-slot
 * CTask-family shape as PTR__CPoller_08f7c368[5]); secondary group ("mIfcThunk", this+8,
 * task.h) install 0x08eac1e4 (3 slots: {dtor thunk pair, non-virtual thunk to
 * CTask::ExecMsg(CMessage&)} -- the SAME 3-slot shape as PTR__CPoller_08f7c384[3]).
 * Both install-only, EvaVTableStub-backed -- nothing in this reconstruction's own call
 * graph dispatches through a CTask-derived task's own vtable (CEditTask is only ever
 * `CModule::Add()`ed, never handed to CLevelManager/scheduler on this reconstruction's
 * currently-wired boot path), same status as every other CTask-derived per-instance
 * vtable in this file.
 */
extern void *PTR__CEditTask_08eac1c8[5];
extern void *PTR__CEditTask_08eac1e4[3];

/* CAlphaKeybCtrl's own real per-instance vtable (alpha_keyb_ctrl.h/.cpp, Eva
 * CAlphaKeybCtrl/CAlphaKeybCtrlTask batch, 2026-07-26). Confirmed byte-exact via
 * direct `.rodata` dword read at 0x08eabb68, 7 slots, the SAME CModule-shape as
 * every other per-instance vtable in this file: {~CAlphaKeybCtrl, ~CAlphaKeybCtrl
 * (deleting), CAlphaKeybCtrl::Setup, CAlphaKeybCtrl::Config, CAlphaKeybCtrl::Start,
 * CModule::Destroy, CModule::GetErrorMsg}. SAME DEPARTURE as
 * PTR__CPanel_08f7c328/PTR__CEditor_08f29b88/PTR__CBatchDiskMan_08eac048 above, same
 * reason: slots 2/3/4 wired to real forwarders
 * (CAlphaKeybCtrlSetupVSlot/CAlphaKeybCtrlConfigVSlot/CAlphaKeybCtrlStartVSlot,
 * omega_vtables.cpp) so `CModuleManager::Setup()/Config()/Start()`'s own real
 * vtbl+8/0xc/0x10 dispatch (module_manager.cpp) actually reaches
 * `CAlphaKeybCtrl::Setup()` -- CAlphaKeybCtrlTask's own real, definitive
 * construction site. Slots 0/1/5/6 stay EvaVTableStub, same "never dispatched by
 * any reconstructed code" reason as every sibling array.
 */
extern void *PTR__CAlphaKeybCtrl_08eabb68[7];

/* CAlphaKeybCtrlTask's own real per-instance vtable (alpha_keyb_ctrl_task.h/.cpp,
 * same batch). Confirmed byte-exact via direct `.rodata` dword read at
 * 0x08eabcc8, 7 slots, the SAME CTask-family shape as PTR__CPoller_08f7c368
 * extended by 2 more slots: {~CAlphaKeybCtrlTask, ~CAlphaKeybCtrlTask(deleting),
 * CAlphaKeybCtrlTask::Exec, CTask::Exec(CMessage&), CTask::ExecMsg(CMessage&),
 * CAlphaKeybCtrlTask::ProcessEvent}. UNLIKE the CModule-shaped arrays above, this
 * one needs a real slot for a different reason: `CAlphaKeybCtrlTask::Exec()`'s own
 * real body self-dispatches `ProcessEvent()` THROUGH slot 5 (`(**(code**)(*this+
 * 0x14))(this,...)`, not a direct call -- same self-dispatch-through-its-own-vtable
 * shape `CHIDDriver::GetKeyboardEvent()` already established for slot 5,
 * hid_driver.h), so slot 5 is wired to a real forwarder
 * (CAlphaKeybCtrlTaskProcessEventVSlot). Slot 2 (Exec itself) is also wired real
 * (CAlphaKeybCtrlTaskExecVSlot) for consistency/potential future
 * `CLevelManager::RunLevel()` dispatch (level_manager_array.h -- that array is
 * real but never populated in this reconstruction's own call graph, same
 * "ground-truth-reachable is the bar" precedent as task.h's own CTask::CTask()
 * note). Slots 0/1 (dtor) and 3/4 (inherited CTask::Exec(CMessage&)/ExecMsg(),
 * matching PTR__CTask_08e82128's own currently-all-stub state) stay
 * EvaVTableStub.
 */
extern void *PTR__CAlphaKeybCtrlTask_08eabcc8[7];

/* CAlphaKeybCtrlTask's own this+8 install target (CTask's own "mIfcThunk" field,
 * task.h -- real ground truth: `&DAT_08eabce8`, never dereferenced by any
 * reconstructed code, same status as CTask's own EvaDataPlaceholder_08e82144 /
 * CPanelIfcTask's EvaDataPlaceholder_08f29d04 above). Any addressable object is a
 * safe stand-in.
 */
extern int   EvaDataPlaceholder_08eabce8;

/* The "AlphaKeybCode" interface-link sub-object CAlphaKeybCtrlTask's own ctor
 * builds at +0x80 (mCodeIfc) -- see alpha_keyb_ctrl_task.h's header comment for
 * the full "why this is opaque, not a real class" derivation. 2 placeholder
 * vtables, both install-only/EvaVTableStub-backed, named after their real
 * ground-truth addresses for traceability:
 *   - primary (buf+0x00, real identity `PTR__COutLinkIfc_08eabd48`): sized 10
 *     slots to match the real `COutLinkIfcBase` primary vtable's own slot count
 *     (dtor pair + SendWithAnswer/SendNoAnswer/OnConnect-family, confirmed via a
 *     direct `.rodata` read at 0x08e81e28..0x08e81e4c) -- never dispatched
 *     through by any of this class's own reconstructed methods.
 *   - CMarshaller<IAlphaKeybCode> sub-object (buf+0x48, real identity
 *     `PTR__CMarshaller_08e89f18`, confirmed via its own adjacent typeinfo-name
 *     string "11CMarshallerI14IAlphaKeybCodeE" to be this exact specialization):
 *     4 slots (dtor pair, GetId, ProcessCode -- matching functions.csv's own
 *     08196670/08196750/08196650/08196410). `ProcessEvent()`'s own real
 *     dispatch through slot 3 (ProcessCode) resolves to EvaVTableStub -- safe,
 *     the real caller discards the return value unconditionally.
 */
extern void *PTR__COutLinkIfc_AlphaKeybCode_08eabd48[10];
extern void *PTR__CMarshaller_AlphaKeybCode_08e89f18[4];

/* CDirEntry's own real vtable (dir_entry.h/.cpp, Eva "size is not depth"
 * re-check batch, 2026-07-26). Confirmed byte-exact via direct `.rodata`
 * reads at 0x08e81900 (`vtable for CDirEntry`): slot0 at 0x08e81908, next
 * symbol (`typeinfo-name`) at 0x08e81924 -> (0x924-0x908)/4 = 7 slots (dtor
 * D1/D0 + 5 real virtual overrides).
 *
 * UPDATE (`PreloadDir()` investigation, 2026-07-26): slot 2
 * (HasValidLongNameExt(), .text+0x08071500) is REAL and genuinely dispatched
 * through by `GetName()`/`GetExt()` (dir_entry.cpp) -- fixed, see
 * `CDirEntryHasValidLongNameExtVSlot`'s own comment in omega_vtables.cpp.
 * Slots 3-6 (OnShortNameChanged/OnShortExtChanged/OnLongNameChanged/
 * OnLongExtChanged, .text+0x081806a0..0x081806d0) ARE confirmed
 * byte-identical empty (`ret` only) -- correctly stay EvaVTableStub-backed,
 * not merely assumed safe.
 */
extern void *PTR__CDirEntry_08e81908[7];

/* CBatchDiskMainTask's own real per-instance vtables (batch_disk_main_task.h/
 * .cpp, same batch). Confirmed byte-exact via direct `.rodata` reads at
 * 0x08eabec0 (`vtable for CBatchDiskMainTask`): PRIMARY group (CTask shape)
 * install 0x08eabec8, 8 slots (0x8eabee8-0x8eabec8)/4; SECONDARY group
 * (CEditable-thunk) install 0x08eabee8, 3 slots
 * ((0x8eabefc-8)-0x8eabee8)/4 -- the this-adjusting thunk header itself
 * occupies the 8 bytes right before 0x8eabefc; TERTIARY group
 * (CRMApiCallBack-thunk) install 0x08eabefc, 7 slots
 * (0x8eabf18-0x8eabefc)/4, matching CRMApiCallBack's own 7 named methods
 * exactly (rm_api_callback.h). All 3 groups entirely EvaVTableStub-backed --
 * same "never CModuleManager-dispatched, only ever CModule::Add()ed" status
 * as every other CTask-derived per-instance vtable in this project
 * (CChunkServerTask, CPanelIfcTask, CEditTask above).
 *
 * UPDATE (`PreloadDir()` investigation, 2026-07-26): the tertiary group's
 * `OnSetRes`/`OnLoadRes`/`OnSave`/`OnDelete` slots being EvaVTableStub is
 * confirmed correct (ground truth's own real bodies ARE empty no-ops,
 * byte-for-byte). `OnLoad` is NOT -- this class genuinely overrides it with
 * real logic at 0x08242e30 (thunk 0x08242e90) -- left EvaVTableStub-backed
 * anyway (no reachable caller, real body needs a populated container this
 * project doesn't build yet) but no longer under the old, disproven blanket
 * claim. See rm_api_callback.h's own corrected header comment for the full
 * writeup.
 */
extern void *PTR__CBatchDiskMainTask_08eabec8[8];
extern void *PTR__CBatchDiskMainTask_08eabee8[3];
extern void *PTR__CBatchDiskMainTask_08eabefc[7];

/* TVector<int,1>'s own real vtable -- the opaque raw buffer at
 * `CBatchDiskMainTask::mUnknownVec` (+0xc8, batch_disk_main_task.h) is
 * vtable-installed to this identity by ground truth (confirmed via direct
 * `.rodata` read: slot0 08e86f78 -> next vtable header 08e86f80
 * (TVector<CMBR::CPartitionInfo,1>) = 2 slots). Install-only, never
 * dispatched through (the vector is always empty in this reconstruction).
 */
extern void *PTR__TVectorInt_08e86f78[2];

/*
 * CLimiterBase's own real vtables (limiter_base.h, Eva "size is not depth"
 * re-check batch, 2026-07-27) -- confirmed byte-exact via a direct `.rodata`
 * dword read at 0x08e81c90..0x08e81ca8 ("vtable for CLimiterBase::
 * CWrProtCircularQueue" begins immediately after CLimiterBase's own last real
 * slot, at 0x8e81ca0, so CLimiterBase's own vtable is exactly 4 slots -- dtor
 * D1/D0 + SendNoAnswer + SendWithAnswer -- not the ~16 nm -C lists across the
 * whole class, most of which are non-virtual). Both install-only/EvaVTableStub-
 * backed: nothing in this reconstruction's own call graph dispatches through
 * either vtable (CLimiterBase has zero callers anywhere in ground truth itself,
 * see limiter_base.h's own REACHABILITY note) -- unlike CAlphaKeybCtrlTask's own
 * per-instance vtable above, no real self-dispatch forwarder is needed.
 */
extern void *PTR__CLimiterBase_08e81c90[4];
extern void *PTR__CWrProtCircularQueue_08e81ca8[2];

/* The "ILimiterNotify" interface-link sub-object CLimiterBase::Init() would build
 * at mIfcLink (limiter_base.h) -- 3 placeholder vtables, same shape and status as
 * PTR__COutLinkIfc_AlphaKeybCode_08eabd48/PTR__CMarshaller_AlphaKeybCode_08e89f18
 * above, just for the ILimiterNotify specialization instead. Declared here (even
 * though Init() itself stays Tier B -- limiter_base.h's own header comment) so a
 * future COutLinkIfcBase reconstruction has real, byte-confirmed addresses ready
 * to wire up: primary vtable @0x8e81d28 (10 slots, matching COutLinkIfcBase's own
 * primary vtable slot count established by AlphaKeybCode's array), secondary/
 * thunk vtable @0x8e81d50 (same 10-slot convention), CMarshaller<ILimiterNotify>
 * sub-object vtable @0x8e81f00 (4 slots: dtor pair, an unidentified slot 2, and
 * `CMarshaller<ILimiterNotify>::WakeUp(unsigned int)` at slot 3 -- confirmed via
 * `nm -C` to be this specialization's real 4th vtable entry, matching
 * ProcessCode's own slot-3 position in the AlphaKeybCode specialization).
 */
extern void *PTR__COutLinkIfc_ILimiterNotify_08e81d28[10];
extern void *PTR__COutLinkIfc_ILimiterNotify_08e81d50[10];
extern void *PTR__CMarshaller_ILimiterNotify_08e81f00[4];
}

#endif /* OMEGA_VTABLES_H */
