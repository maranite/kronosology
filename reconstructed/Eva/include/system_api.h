/*
 * system_api.h  -  CSystemApi, Eva's central "system API" object (opaque, Stage 3+).
 *
 * CKernel::GetSysApi() (ckernel.cpp) returns the real SysApiInstance global as void*;
 * every caller traced so far (Mains(), the MMainXxx family, mains.cpp) casts it to
 * this type. Real layout is not reconstructed -- only the individual virtual-dispatch
 * slots actually exercised by reconstructed code are documented, each at its own call
 * site, since the vtable itself isn't recovered:
 *   +0x40  register a named module descriptor (mains.cpp's RegisterModuleDescriptor)
 *          -- confirmed (2026-07-25, direct byte read of the ground-truth vtable) to
 *          be CSysApiInstance::AddConstructor(CModuleConstructor*) (sysapi_instance.h),
 *          forwarding to CModuleManager::AddConstructor() (module_manager.h). Was a
 *          dead EvaVTableStub no-op until this batch (omega_vtables.cpp) -- fixed.
 *   +0xb4  register a fully-constructed driver object (MMainPanelDriver/MMainHIDDriver)
 *   +0x7c  exit-requested query (COmegaInterface::ExitRequested, omega_interface.cpp)
 *   +0xa0  fetch a named sub-API, e.g. FMApi (MMainLinuxDriver, mains.cpp)
 *   +0x3c  per-object "scope id" assignment, called from both CModule::CModule() and
 *          CTask::CTask() at construction time (module.cpp/task.cpp) -- real meaning
 *          not decoded, return value just stored (mScopeId)
 *   +0x12c CModule::Add(CTask*)'s own second notification: `(Api, CModule*)` --
 *          real meaning not decoded (module.cpp, Stage 6 CTask::CTask() batch,
 *          2026-07-25)
 *   +0x134 CModule::Add(CTask*)'s own first notification: `(Api, CTask*)` -- real
 *          meaning not decoded (module.cpp, same batch)
 *   +0x140 CTask::~CTask()'s own entry notification: `(Api, CTask*)` -- fired once,
 *          at the very start of destruction, before anything else is torn down;
 *          real meaning not decoded (task.cpp, Stage 6 SetMask/~CTask batch,
 *          2026-07-25)
 *   +0x38  named config-string getter: `char *(Api, const char *key)` -- returns
 *          NULL if `key` is absent, else a `char*` value (CFileMan::CFileMan(),
 *          file_man.cpp, Stage 6 breadth sweep, 2026-07-25, called 3 times for
 *          "FMBackGroundJobs"/"FMMinIdleTimeToStartBGJobs"/
 *          "FMDeltaTimeBetweenBGJobs"). Real identity not decoded, only this call
 *          shape (same per-key config lookup pattern as CConfigManager's own
 *          config-table reads elsewhere in this project).
 *   +0x058 CTask::~CTask()'s own per-outlink notification: `(Api, COutLink*)` --
 *          fired once for each element of mOutLinks (task.h) as it's drained, right
 *          before that element's own COmegaPtrArray::RemoveAtIndex(0, true) call;
 *          real meaning not decoded (task.cpp, same batch). A DIFFERENT slot from the
 *          per-element "free element" callback COmegaPtrArray::RemoveAtIndex/Destroy
 *          already dispatch on their own (omega_ptr_array.cpp's CallFreeElement) --
 *          this is an ADDITIONAL, explicit notification straight to Api.
 *   +0x044 CPoller::RegisterClient()'s own tail call (poller.cpp, 2026-07-26): `int
 *          (Api, const char *ownerModuleName, const char *taskName, const char
 *          *clientName, const char *nameA, const char *nameB, int)` -- 7 args, real
 *          meaning not decoded (a generic "register/notify named client pair"
 *          shape, by call-site inference only). Result `<= 0` is treated as failure
 *          by the caller (RegisterClient() returns 11 and resets its own out-handle
 *          to -1 in that case), `> 0` as success (returns 0).
 */

#ifndef SYSTEM_API_H
#define SYSTEM_API_H

class CSystemApi {
};

#endif /* SYSTEM_API_H */
