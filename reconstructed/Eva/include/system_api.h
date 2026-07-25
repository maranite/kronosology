/*
 * system_api.h  -  CSystemApi, Eva's central "system API" object (opaque, Stage 3+).
 *
 * CKernel::GetSysApi() (ckernel.cpp) returns the real SysApiInstance global as void*;
 * every caller traced so far (Mains(), the MMainXxx family, mains.cpp) casts it to
 * this type. Real layout is not reconstructed -- only the individual virtual-dispatch
 * slots actually exercised by reconstructed code are documented, each at its own call
 * site, since the vtable itself isn't recovered:
 *   +0x40  register a named module descriptor (mains.cpp's RegisterModuleDescriptor)
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
 */

#ifndef SYSTEM_API_H
#define SYSTEM_API_H

class CSystemApi {
};

#endif /* SYSTEM_API_H */
