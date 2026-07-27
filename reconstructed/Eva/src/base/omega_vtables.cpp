/*
 * omega_vtables.cpp  -  see include/omega_vtables.h.
 */

#include "omega_vtables.h"
#include "sysapi_instance.h"
#include "global_object_base.h"
#include "panel.h"
#include "editor.h"
#include "batch_disk_man.h"
#include "alpha_keyb_ctrl.h"
#include "alpha_keyb_ctrl_task.h"

extern "C" void EvaVTableStub()
{
	/* Real cdecl no-op. Safe as a stand-in for any of these slots' real virtual
	 * method regardless of the real method's own arg count -- cdecl callees never
	 * pop caller-pushed stack args, so a mismatched (zero) parameter count here
	 * cannot corrupt the caller's stack.
	 */
}

/*
 * WORKAROUND (2026-07-24): PTR__CSysApiInstance_08e81008's own slot 40 (byte
 * offset 0xa0) is installed at this specific index instead of the generic
 * EvaVTableStub -- a live kronos_vm boot proved this is the one slot on
 * Api's own vtable this reconstruction genuinely DISPATCHES THROUGH AND
 * CONSUMES THE RETURN VALUE OF: mains.cpp's MMainLinuxDriver fetches "FMApi"
 * via a virtual call through this exact slot (see that file's own header
 * comment), then immediately dispatches through FMApi's own vtable at
 * +0x24. EvaVTableStub's own no-op body leaves EAX holding whatever
 * happened to be there before the call (observed live: the call target's
 * OWN address, an accidental byproduct of the calling sequence, not
 * meaningful data) -- fine for slots whose return value is discarded (the
 * documented, until-now-universal assumption this whole array's own header
 * comment states), but this ONE consumed return value then gets
 * (mis)interpreted as a CSystemApi* and dereferenced, landing on
 * uninitialized/arbitrary memory -- BUG: segfault, "ip" a valid Eva code
 * address but the faulting data address pure garbage. Returns `SysApiInstance`
 * itself instead: a real, valid, already-fully-stubbed (this very array)
 * CSystemApi-shaped object, safe for anything downstream to dispatch through
 * again. Not a claim about what the real FMApi object actually is --
 * MMainLinuxDriver's own real behavior (a distinct secondary sub-API) stays
 * unreconstructed either way; this only prevents the crash.
 */
extern "C" void *GetFMApiStub(void *)
{
	return SysApiInstance;
}

/*
 * FIX (2026-07-25): PTR__CSysApiInstance_08e81008's own slot 16 (byte offset 0x40)
 * is installed as this real forwarder instead of the generic EvaVTableStub. Direct
 * byte read of the ground-truth binary's own installed vtable (Eva, file offset
 * 0xE39048 = VA 08e81008+0x40) confirms the real pointer there is exactly
 * 0x0806b530 = CSysApiInstance::AddConstructor(CModuleConstructor*)
 * (sysapi_instance.h). mains.cpp's RegisterModuleDescriptor() dispatches every one
 * of its 15 real module descriptors through this exact slot; under the old
 * EvaVTableStub no-op none of them ever reached CModuleManager::AddConstructor(),
 * so mConstructors (module_manager.h) stayed permanently empty -- same "Tier-B stub
 * leaves a real array dead" bug class as CModuleManager::AddModule()/mModules
 * (Stage 6 batch 3).
 */
extern "C" void AddConstructorVSlot(void *obj, void *ctor)
{
	((CSysApiInstance *)obj)->AddConstructor((CModuleConstructor *)ctor);
}

/*
 * FIX (2026-07-25, small-derived-module follow-up batch): PTR__CSysApiInstance_
 * 08e81008's own slot 17 (byte offset 0x44) is installed as this dedicated stub
 * instead of the generic EvaVTableStub. `CChunkMan::Config()` (chunk_man.cpp) is
 * the one caller that genuinely CONSUMES this slot's return value
 * (`if (0 < result) { ...second call...; return result < 1; } return true;`) --
 * same "EvaVTableStub leaves EAX as stale, meaningless garbage" hazard class
 * already fixed for the +0x40/+0xa0 slots above (AddConstructorVSlot/
 * GetFMApiStub's own header comments). Real meaning not decoded -- judging from
 * CChunkMan::Config()'s own 6-string-argument call shape (SysName, a task name, an
 * outlink name, SysName again, another task name, 0), plausibly an Api-mediated
 * "link two named outlinks together" registration call -- returns 0
 * unconditionally, which keeps both of CChunkMan::Config()'s own `0 < result`
 * checks false, i.e. the whole function deterministically returns `true` on its
 * first call, matching its own real early-return shape on any non-positive first
 * result. A safe, documented placeholder, not a claim about the real return value.
 */
extern "C" int ChunkLinkRegisterVSlotStub()
{
	return 0;
}

/*
 * FIX (2026-07-26, found during the CEditor vtable-fix's own live-boot
 * verification): PTR__CSysApiInstance_08e81008's own slot 43 (byte offset 0xac,
 * `Api+0xac`) is installed as this real forwarder instead of the generic
 * EvaVTableStub. Same "EvaVTableStub leaves EAX as stale, meaningless garbage"
 * hazard class already fixed for the +0x40/+0xa0 slots above
 * (AddConstructorVSlot/GetFMApiStub) -- CPoller::CPoller() (poller.cpp,
 * CPanel::Setup()'s own real construction target, itself unlocked by this same
 * batch's CModuleManager-dispatch fix) calls this slot as a named-resource
 * lookup and immediately dereferences whatever it returns: `if (resource != 0)
 * { ...call through resource's own vtbl+0x10... }`. Ground truth's own comment
 * in poller.h already documents this as "Api+0xac resolves ... tractable now"
 * (i.e. the CALL SITE was already known-real) but never wired a real target for
 * the slot itself. With the old EvaVTableStub no-op, EAX held leftover garbage
 * from the call sequence, `resource != 0` was true, and the ctor dereferenced
 * that garbage as a vtable pointer -- a real, live segfault (`Eva[pid]: segfault
 * ... in CPoller::CPoller`), first observed on THIS session's live-boot test,
 * the first live boot to ever reach CPanel::Setup()'s real dispatch via
 * CModuleManager (see PTR__CPanel_08f7c328's own header comment for why that
 * dispatch itself was only unlocked earlier the same day). Fixed by returning
 * NULL here -- the ctor's own well-tested, KAT-covered "lookup failed" fallback
 * path (`mResource = 0; SetMask(1);`, matching test_panel.cpp's own fake-Api
 * setup, which deliberately modeled exactly this NULL-return case). Not a claim
 * that the real Api+0xac lookup for "PanelDriver" genuinely fails on real
 * hardware -- only that returning a safe, well-defined NULL here is strictly
 * better than the previous undefined-garbage behavior, same spirit as
 * GetFMApiStub's own "safe placeholder, not a claim about the real object"
 * disclaimer.
 */
extern "C" void *LookupResourceStub(void *, const char *)
{
	return 0;
}

/*
 * FIX (2026-07-26, Eva Stage 6 CPanel unlock batch): PTR__CPanel_08f7c328's own
 * Setup/Config/Start slots (byte offsets 8/0xc/0x10) are installed as these 3 real
 * forwarders instead of the generic EvaVTableStub -- see omega_vtables.h's own
 * header comment on PTR__CPanel_08f7c328 for the full "why" (CModuleManager::
 * Setup()/Config()/Start(), module_manager.cpp, genuinely dispatch through exactly
 * these offsets for every module in the real mModules array, and this is the one
 * real path that makes CPoller's own construction, panel.cpp's CPanel::Setup(),
 * live-reachable). `CallVSlot`'s own caller (module_manager.cpp) treats every
 * slot as `void(*)(void*)` regardless of the real method's own return type --
 * same cdecl-safe "extra return value in EAX is simply ignored by the caller"
 * reasoning EvaVTableStub's own header comment already establishes.
 */
extern "C" void CPanelSetupVSlot(void *obj)
{
	((CPanel *)obj)->Setup();
}

extern "C" void CPanelConfigVSlot(void *obj)
{
	((CPanel *)obj)->Config();
}

extern "C" void CPanelStartVSlot(void *obj)
{
	((CPanel *)obj)->Start();
}

/*
 * FIX (2026-07-26, Eva Stage 6 CEditor unlock follow-up): PTR__CEditor_08f29b88's own
 * Setup/Config/Start slots (byte offsets 8/0xc/0x10) get the same real-forwarder
 * treatment as PTR__CPanel_08f7c328 above, for the identical reason -- see
 * omega_vtables.h's header comment on PTR__CEditor_08f29b88. Ground truth
 * (`objdump -dr -M intel` on the real, unstripped `Decomp/EVA_Decomp/Eva`) confirms the
 * real vtable at 0x08f29b88 has slot2=0x08249b60 (CEditor::Setup), slot3=0x082498a0
 * (CEditor::Config), slot4=0x082498b0 (CEditor::Start) -- byte-identical shape to
 * CPanel's own vtable, down to slot5/slot6 being CModule::Destroy/GetErrorMsg
 * (0x08181c10/0x08181c20, same weak symbols CPanel's vtable also points at).
 *
 * `CEditor::Config()` is declared `static int Config()` (editor.h) -- real ground
 * truth is a literal `xor eax,eax; ret`, 3 bytes, that never touches `this` at all.
 * The forwarder still receives `obj` (CModuleManager::Setup()/Config()/Start()'s own
 * `CallVSlot` always passes it) but discards it, matching the real callee's own
 * shape exactly instead of pretending it's a normal member call.
 */
extern "C" void CEditorSetupVSlot(void *obj)
{
	((CEditor *)obj)->Setup();
}

extern "C" void CEditorConfigVSlot(void *obj)
{
	(void)obj;
	CEditor::Config();
}

extern "C" void CEditorStartVSlot(void *obj)
{
	((CEditor *)obj)->Start();
}

/*
 * FIX (2026-07-26, broad Tier-B recheck sweep): PTR__CSysApiInstance_08e81008's own
 * slot 62 (byte offset 0xf8) is installed as this real forwarder instead of the
 * generic EvaVTableStub. Direct byte read of the ground-truth binary's own installed
 * vtable (Eva, VA 08e81008+0xf8) confirms the real pointer there is exactly
 * 0x0806aa00 = CSysApiInstance::WriteMessageToHost(int,int) (sysapi_instance.h).
 * CErrorHandler::EnableUpdate() (error_handler.cpp) is the one reconstructed caller
 * that dispatches through this exact slot on the global Api object -- under the old
 * EvaVTableStub no-op, that notification silently vanished. Same
 * "LESSON_vtable_dispatch_stub_gap" bug class as the +0x40/+0xa0/+0x44 slots above.
 */
extern "C" void WriteMessageToHostVSlot(void *obj, int a, int b)
{
	((CSysApiInstance *)obj)->WriteMessageToHost(a, b);
}

extern "C" {
void *PTR__CHostInterfaceBase_08e80b68[22] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CHostInterface_08e80b08[22] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__COmegaPtrArray_08e80be0[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e80bf8[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e80c10[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e80c40[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TVector_08e80c58[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CDummyMsgInput_08e80c80[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CNamedObjectBase_08e81378[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CApiDescriptor_08e81368[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CTracer_08e81468[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CLevelManagerArray_08e80c28[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
/* Real boundary is 3 slots -- see omega_vtables.h's header comment on this symbol. */
void *PTR__CLevelManager_08e80e50[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e80ea8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e80bc8[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
/*
 * FIX (2026-07-27, live vtable-dispatch sweep on kronos_vm): PTR__CSysApiInstance_
 * 08e81008's own slots 2/3/4/5 (byte offsets 8/0xc/0x10/0x14 -- the 4 inherited
 * CGlobalObjectBase "phase hook" slots, global_object_base.h) were still the generic
 * EvaVTableStub, even though global_object_base.h's own header comment already
 * documented (from an earlier ground-truth raw-byte read) that these 4 slots are
 * confirmed identical to CGlobalObjectBase_Pre/PostKernelConstructor/Pre/
 * PostKernelDestructor in the real binary -- the write-up was done but the array
 * below was never actually updated to match it. Direct byte read of the ground-truth
 * binary (Eva, VA 08e81010/08e81014/08e81018/08e8101c) confirms
 * 0x0804cc10/0x0804cc20/0x0804cc30/0x0804cc40 -- exactly global_object_base.cpp's own
 * 4 no-ops, reused here verbatim (not re-wrapped) since ground truth uses the literal
 * same code address for the inherited, unoverridden slot.
 *
 * Confirmed live and reachable: a kronos_vm gdbstub trace of CKernel::CKernel(int)'s
 * own sm_poGlobalObjectList bring-up loop (ckernel.cpp) caught two real hits landing
 * on EvaVTableStub -- CallVSlot2(SysApiInstance, 8, 0) and CallVSlot2(SysApiInstance,
 * 12, 0) -- confirming this dispatch is genuinely exercised on the real boot path, not
 * theoretical. (Slots 0/1, the dtor pair, are NOT touched by this fix -- ground truth
 * shows CSysApiInstance has its own distinct, still-unreconstructed destructor there,
 * a separate documented gap, not this bug class.) Same
 * "LESSON_vtable_dispatch_stub_gap" bug class as the +0x40/+0xa0/+0x44/+0xf8 slots
 * above -- 9th confirmed instance this project has found.
 */
void *PTR__CSysApiInstance_08e81008[94] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CGlobalObjectBase_PreKernelConstructor, (void *)CGlobalObjectBase_PostKernelConstructor,
	(void *)CGlobalObjectBase_PreKernelDestructor, (void *)CGlobalObjectBase_PostKernelDestructor,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)AddConstructorVSlot, (void *)ChunkLinkRegisterVSlotStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)GetFMApiStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)LookupResourceStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)WriteMessageToHostVSlot, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e811a8[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e811c0[8] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
/* CModule's own real vtable -- see header comment (7 slots: dtor pair, Setup(+8),
 * Config(+0xc), Start(+0x10), Destroy(+0x14), GetErrorMsg(+0x18)). Never actually
 * dispatched through by any code in this reconstruction (every real MMainXxx(void)
 * caller in mains.cpp overwrites `this+0` with the derived module's own vtable
 * immediately after CModule::CModule() runs, and CModuleManager::Setup/Config/Start()
 * only ever iterate mModules -- always empty, since CModuleManager::AddModule() is a
 * Tier-B stub) -- same "install-only, real slot count" status as this file's other
 * entries.
 */
void *PTR__CModule_08e81fe8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
/* CSysEx's own vtable stays here -- its own Setup/Config/Start are all confirmed
 * genuinely empty (not yet transcribed as real forwarders; the class itself has no
 * further tractable methods pursued this pass), unlike its 4 siblings below.
 */
void *PTR__CSysEx_08e899e8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
/* CDumpManMod's own vtable definition now lives in dump_man_mod.cpp (Stage 6
 * breadth sweep, 2026-07-25, DumpManager cluster batch) -- slots 2/3/4 (Setup/
 * Config/Start) are wired to real forwarders there, same "define locally where the
 * real forwarders live" precedent as es_common.cpp's own PTR__CESCommon_08fbafc8.
 *
 * CEditMan/CSeqTimer/CChunkMan/CMessagePort's own vtable definitions now live in
 * edit_man.cpp/seq_timer.cpp/chunk_man.cpp/message_port.cpp respectively (Stage 6
 * breadth sweep, 2026-07-25, small-derived-module follow-up batch) -- same
 * "define locally where the real forwarders live" precedent, now that all 4 have
 * real Setup()/Config()/Start() bodies (or, for CMessagePort, confirmed-empty ones)
 * to wire in.
 */
/* CTask's own real vtable (7 slots) + its 2 embedded sub-object vtables + the
 * embedded CLimiterMan's own vtable + ITS embedded TVector's vtable -- see
 * header comment's "CTask::CTask()/CLimiterMan batch" note. All install-only,
 * same status as this file's other entries.
 */
void *PTR__CTask_08e82128[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TNamedPtrArray_08e82198[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TVector_08e82188[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CLimiterMan_08e81ee8[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TVector_08e81f78[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
/* Opaque data blob CTask+0x08 stores the ADDRESS of (never dereferences) -- see
 * header comment. Any addressable object is a safe stand-in.
 */
int EvaDataPlaceholder_08e82144 = 0;

/* CTask::~CTask()/CLimiterMan::~CLimiterMan() teardown identities -- see
 * omega_vtables.h's own header comment for what each really is in ground truth. Never
 * dispatched through, so a single EvaVTableStub slot each is enough.
 */
void *PTR__CObjectBase_08e79d68[1]   = { (void *)EvaVTableStub };
void *PTR__CIfcUnknown_08e81d80[1]   = { (void *)EvaVTableStub };
void *PTR__CMessageInput_08e80c68[1] = { (void *)EvaVTableStub };

/* CSysExMsgTaskBase's own vtable pair -- see header comment. */
void *PTR__CSysExMsgTaskBase_08e84c28[13] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};
int EvaDataPlaceholder_08e84c50 = 0;

/* CEditServer's own vtable pair -- see header comment. */
void *PTR__CEditServer_08e817b0[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e817e8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* COutLink/COutLinkMono/CSysExMsgOutLink/CSysExMsgClientOutLink vtables -- see
 * header comment. All install-only.
 */
void *PTR__COutLinkMono_08e82048[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__COutLink_08e82068[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__COutLinkMulti_08e82028[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e820d8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CSysExMsgClientOutLink_08e84b08[8] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CSysExMsgOutLink_08e84b28[8] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CBufferingTask/CCircByteBuffer/CDumpBuffer/CDumpMachine/CDumpManStateMachine/
 * CDumpTask vtables -- see header comment. All install-only.
 */
void *PTR__CBufferingTask_08e85aa8[14] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CCircByteBuffer_08e85b68[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CDumpBuffer_08e85c10[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CDumpMachine_08e85c48[10] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CDumpManStateMachine_08e85ce8[10] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CDumpTask_08e85d48[14] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
int EvaDataPlaceholder_08e85ac4 = 0;
int EvaDataPlaceholder_08e85d74 = 0;

/* CFileMan/CResMan/CChunkOnDemand cluster's simple, no-real-forwarder vtables --
 * see header comment. PTR__CFileMan_08e86e48/PTR__CResMan_08e88b08 themselves are
 * defined in file_man.cpp/res_man.cpp instead (real Setup/Config/Start forwarders
 * live there, same "define locally where the real forwarders live" precedent as
 * dump_man_mod.cpp's own PTR__CDumpManMod_08e85ca8).
 */
void *PTR__TNamedPtrArray_08e86fc8[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e885a0[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
/* BUG FIX: was `void *PTR__CRMApiCallBack_08e886e8 = 0;` (a bare scalar) in
 * mains.cpp -- see header comment. Moved here, real 7-slot size.
 */
void *PTR__CRMApiCallBack_08e886e8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e88bb8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TVector_08e88ba8[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CEditor's own real vtable cluster -- see header comment. The 2 sibling
 * (nested-class) vtables below are confirmed install-only, same treatment as
 * PTR__CModule_08e81fe8 (whose own real Destroy()/GetErrorMsg() slots stay
 * EvaVTableStub for the same reason: real, named, ground-truth methods that
 * are simply never dispatched through by any reconstructed caller).
 * PTR__CEditor_08f29b88 itself is the DEPARTURE (2026-07-26 fix, see
 * omega_vtables.h's own header comment): slots 2/3/4 (Setup/Config/Start) are
 * wired to real forwarders (CEditorSetupVSlot/CEditorConfigVSlot/
 * CEditorStartVSlot, above), not EvaVTableStub -- CModuleManager::Setup()/
 * Config()/Start() (module_manager.cpp, Tier A/real) genuinely dispatch
 * through exactly these offsets for every module in the real mModules array,
 * same as PTR__CPanel_08f7c328's own fix.
 */
void *PTR__CEditor_08f29b88[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CEditorSetupVSlot, (void *)CEditorConfigVSlot, (void *)CEditorStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CEditor_08f29bac[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CEditor_08f29bc0[5] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CAlphaKeybIfcTask's own real 3-vtable cluster (Stage 6 breadth sweep,
 * 2026-07-25 -- CEditor::Setup()'s "ALPHAKEYBOARD=Yes" fan-out target,
 * reconstructed standalone/unwired to avoid touching editor.cpp/editor.h
 * during the concurrent dedicated CEditor batch -- see alpha_keyb_ifc_task.h).
 * All 3 groups confirmed by a direct .rodata dword read at 0x08f25ae0
 * (offset-to-top/typeinfo/slots, repeated per this-adjustment group), same
 * discipline as CEditor's own 3-vtable cluster immediately above:
 *   primary   (this+0,    offset-to-top 0)     08f25ae8..08f25afc = 6 slots
 *   secondary (this+0x08, offset-to-top -8)    08f25b08..08f25b10 = 3 slots
 *             -- this is CTask's own "mIfcThunk" field (task.h +0x08),
 *             specialized per-class instead of the generic
 *             EvaDataPlaceholder_08e82144 identity
 *   tertiary  (this+0x80, offset-to-top -0x80) 08f25b1c..08f25b28 = 4 slots
 *             -- the real CIfcUnknown-adjusted sub-object CTask::RegisterIfc()
 *             is called against (CTask::CTask()'s own +0x80 install)
 * All 3 install-only in this reconstruction (CAlphaKeybIfcTask is not wired
 * into any reconstructed caller yet), same EvaVTableStub treatment as every
 * other undecoded vtable in this file.
 */
void *PTR__CAlphaKeybIfcTask_08f25ae8[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CAlphaKeybIfcTask_08f25b08[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CAlphaKeybIfcTask_08f25b1c[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};

/* CEditor::CPanelIfcTask + CPanelCfg -- see omega_vtables.h's own header
 * comment for the full byte-read derivation.
 */
void *PTR__CPanelIfcTask_08f29ce8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};
void *PTR__CPanelCfg_08f29d48[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
int EvaDataPlaceholder_08f29d04 = 0;

/* CChunkServer / CEditor::CChunkServerTask -- see omega_vtables.h's own
 * header comment for the full 16+3 byte-exact derivation.
 */
void *PTR__CChunkServer_08e859a8[16] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};
void *PTR__CChunkServer_08e859f0[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CChunkServerTask_08f25b88[16] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};
void *PTR__CChunkServerTask_08f25bd0[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CPoller / CPoller::CIfcClient -- see omega_vtables.h's own header comment
 * for the full byte-exact derivation.
 */
void *PTR__CPoller_08f7c368[5] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CPoller_08f7c384[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TVector_08f7c3b0[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CIfcClient_08f7c3c8[6] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CPanel's own real per-instance vtable -- see omega_vtables.h's own header comment
 * for the full derivation and the deliberate departure from this file's usual
 * install-only convention (slots 2/3/4 are real, not EvaVTableStub).
 */
void *PTR__CPanel_08f7c328[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CPanelSetupVSlot, (void *)CPanelConfigVSlot, (void *)CPanelStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

/*
 * FIX (2026-07-26, Eva Stage 6 CBatchDiskMan unlock batch): PTR__CBatchDiskMan_08eac048's
 * own Setup/Config/Start slots get the same real-forwarder treatment as
 * PTR__CPanel_08f7c328/PTR__CEditor_08f29b88 above, for the identical reason -- see
 * omega_vtables.h's own header comment on PTR__CBatchDiskMan_08eac048.
 */
extern "C" void CBatchDiskManSetupVSlot(void *obj)
{
	((CBatchDiskMan *)obj)->Setup();
}

extern "C" void CBatchDiskManConfigVSlot(void *obj)
{
	((CBatchDiskMan *)obj)->Config();
}

extern "C" void CBatchDiskManStartVSlot(void *obj)
{
	((CBatchDiskMan *)obj)->Start();
}

/* CBatchDiskMan's own real per-instance vtables -- see omega_vtables.h's own header
 * comment for the full derivation. */
void *PTR__CBatchDiskMan_08eac048[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CBatchDiskManSetupVSlot, (void *)CBatchDiskManConfigVSlot,
	(void *)CBatchDiskManStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CBatchDiskMan_08eac06c[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};

/* CEditTask's own real per-instance vtables -- install-only, see omega_vtables.h's own
 * header comment. */
void *PTR__CEditTask_08eac1c8[5] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CEditTask_08eac1e4[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/*
 * FIX (2026-07-26, Eva CAlphaKeybCtrl/CAlphaKeybCtrlTask batch): PTR__CAlphaKeybCtrl_
 * 08eabb68's own Setup/Config/Start slots get the same real-forwarder treatment as
 * PTR__CPanel_08f7c328/PTR__CEditor_08f29b88/PTR__CBatchDiskMan_08eac048 above, for
 * the identical reason -- see omega_vtables.h's own header comment.
 */
extern "C" void CAlphaKeybCtrlSetupVSlot(void *obj)
{
	((CAlphaKeybCtrl *)obj)->Setup();
}

extern "C" void CAlphaKeybCtrlConfigVSlot(void *obj)
{
	((CAlphaKeybCtrl *)obj)->Config();
}

extern "C" void CAlphaKeybCtrlStartVSlot(void *obj)
{
	((CAlphaKeybCtrl *)obj)->Start();
}

/* CAlphaKeybCtrl's own real per-instance vtable -- see omega_vtables.h's own header
 * comment for the full derivation. */
void *PTR__CAlphaKeybCtrl_08eabb68[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CAlphaKeybCtrlSetupVSlot, (void *)CAlphaKeybCtrlConfigVSlot,
	(void *)CAlphaKeybCtrlStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CAlphaKeybCtrlTask's own self-dispatch forwarders -- see omega_vtables.h's own
 * header comment for why slot 5 (ProcessEvent) needs to be real (Exec()'s own real
 * body dispatches through it, not a direct call).
 */
extern "C" void CAlphaKeybCtrlTaskExecVSlot(void *obj)
{
	((CAlphaKeybCtrlTask *)obj)->Exec();
}

extern "C" void CAlphaKeybCtrlTaskProcessEventVSlot(void *obj, void *evt)
{
	((CAlphaKeybCtrlTask *)obj)->ProcessEvent((const SKeyboardEvt *)evt);
}

/* CAlphaKeybCtrlTask's own real per-instance vtable -- see omega_vtables.h's own
 * header comment. */
void *PTR__CAlphaKeybCtrlTask_08eabcc8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CAlphaKeybCtrlTaskExecVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CAlphaKeybCtrlTaskProcessEventVSlot,
	(void *)EvaVTableStub,
};

/* The "AlphaKeybCode" interface-link sub-object's own 2 placeholder vtables -- see
 * omega_vtables.h's own header comment. Both install-only, EvaVTableStub-backed.
 */
void *PTR__COutLinkIfc_AlphaKeybCode_08eabd48[10] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};
void *PTR__CMarshaller_AlphaKeybCode_08e89f18[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};

int EvaDataPlaceholder_08eabce8 = 0;

/*
 * FIX (CBatchDiskMainTask::PreloadDir() investigation, 2026-07-26):
 * PTR__CDirEntry_08e81908's own slot 2 (byte offset 0x8) is installed as this
 * real forwarder instead of the generic EvaVTableStub. Direct `.rodata` byte
 * read of ground truth's own vtable (0x08e81908+0x8 = 0x08071500) confirms the
 * real function there is CDirEntry::HasValidLongNameExt() (dir_entry.h/.cpp) --
 * and unlike this vtable's other 4 real virtual slots (OnShortNameChanged/
 * OnShortExtChanged/OnLongNameChanged/OnLongExtChanged, confirmed
 * byte-identical empty `ret`-only bodies, correctly left EvaVTableStub-backed
 * below), THIS one is genuinely dispatched through and its return value
 * consumed: CDirEntry::GetName()/GetExt() (dir_entry.cpp) both call
 * `(*(this->mVtbl + 0x8))(this)` to decide short-vs-long name/ext. Under the
 * old EvaVTableStub no-op this would have returned stale/meaningless EAX
 * garbage instead of a real true/false answer -- same "EvaVTableStub leaves a
 * consumed return value as garbage" hazard class as every other confirmed
 * instance of this bug in this project (see AddConstructorVSlot/GetFMApiStub's
 * own comments above).
 */
extern "C" int CDirEntryHasValidLongNameExtVSlot(void *obj)
{
	return ((const CDirEntry *)obj)->HasValidLongNameExt() ? 1 : 0;
}

/* CDirEntry's own real vtable -- see omega_vtables.h's own header comment. */
void *PTR__CDirEntry_08e81908[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CDirEntryHasValidLongNameExtVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};

/* CBatchDiskMainTask's own real per-instance vtables -- see omega_vtables.h's
 * own header comment. */
void *PTR__CBatchDiskMainTask_08eabec8[8] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CBatchDiskMainTask_08eabee8[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CBatchDiskMainTask_08eabefc[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};

/* TVector<int,1>'s own real vtable -- see omega_vtables.h's own header
 * comment. */
void *PTR__TVectorInt_08e86f78[2] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};
}
