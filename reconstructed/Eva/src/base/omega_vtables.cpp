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
void *PTR__CTracer_08e81468[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CLevelManagerArray_08e80c28[4] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CLevelManager_08e80e50[20] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
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
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
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
/* 6 real derived-module vtables (mains.cpp) -- see omega_vtables.h's Stage 6
 * "AddModule()/EnableUpdate() batch" note for why these must be real slot arrays
 * (not a bare NULL scalar) now that CModuleManager::AddModule() is Tier A.
 */
void *PTR__CEditMan_08e85ea8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CMessagePort_08e88468[13] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub,
};
void *PTR__CSeqTimer_08e892a8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CSysEx_08e899e8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CChunkMan_08e85968[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CDumpManMod_08e85ca8[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
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
}
