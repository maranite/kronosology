/*
 * omega_vtables.cpp  -  see include/omega_vtables.h.
 */

#include "omega_vtables.h"
#include "sysapi_instance.h"

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
void *PTR__CSysApiInstance_08e81008[94] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)AddConstructorVSlot, (void *)ChunkLinkRegisterVSlotStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)GetFMApiStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
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

/* CEditor's own real vtable cluster -- see header comment. All 3 confirmed
 * install-only, same treatment as PTR__CModule_08e81fe8 above (whose own real
 * Destroy()/GetErrorMsg() slots stay EvaVTableStub for the same reason: real,
 * named, ground-truth methods that are simply never dispatched through by any
 * reconstructed caller).
 */
void *PTR__CEditor_08f29b88[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
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
}
