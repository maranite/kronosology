/*
 * es_common.cpp  -  see include/es_common.h.
 *
 * CESCommon::CESCommon/~CESCommon/Setup/Start/Config transcribed from
 * CESCommon@08bd1e00.c/_CESCommon@08bd1be0.c/_CESCommon@08bd1cc0.c/
 * Setup@08bd1d80.c/Start@08bd1bd0.c/Config@08bd1bc0.c.
 */

#include "es_common.h"
#include "omega_vtables.h"

#include <cstdlib>
#include <new>

/* Real combined vtable (_ZTV9CESCommon, 0x40 bytes total for both the
 * CModule-subobject primary vtable and the CEditServer-subobject secondary
 * vtable, Itanium multiple-inheritance layout) -- never dispatched through
 * by any reconstructed code (Setup/Start/Config are called directly as named
 * C++ methods here, not via CModuleManager's vtable+8 convention, since this
 * class isn't wired into CModuleManager's own factory array -- see
 * edit_server.h). Declared as 2 separate, generously-sized EvaVTableStub
 * arrays (headroom past each real sub-vtable's own measured slot count),
 * same "safe regardless of real slot count" precedent as every other
 * undecoded vtable in this project.
 */
extern "C" void *PTR__CESCommon_08fbafc8[9] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
extern "C" void *DAT_08fbafec[9] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

CESCommon::CESCommon(const char *name, int /*unusedExtra*/)
	: CModule(name), CEditServer(name)
{
	/* Real: overwrites both base sub-object vtable pointers with CESCommon's
	 * own combined vtable, right after both base ctors have already
	 * installed their own generic base vtables -- same manual-vtable-swap
	 * idiom used throughout this project. CModule's own base ctor already
	 * ran as this class's first base-initializer above (installing
	 * PTR__CModule_08e81fe8 at `this+0`); CEditServer's own base ctor ran
	 * second (installing PTR__CEditServer_08e817b0 at `this+0x2c`). Both
	 * get overwritten here with CESCommon's own real vtables, matching the
	 * ctor's own final 2 stores.
	 */
	*(void ***)this = PTR__CESCommon_08fbafc8;
	*(void **)((char *)this + 0x2c) = DAT_08fbafec;
}

CESCommon::~CESCommon()
{
	/* Real: both dtor overloads (deleting + complete-object) re-install the
	 * same 2 CESCommon vtables at entry, before tearing down bases -- matches
	 * every other reconstructed dtor in this project doing the same. The
	 * actual base teardown (CEditServer::~CEditServer(), which reinstalls
	 * CEditServer's own base vtable and calls COmegaPtrArray::Destroy() on
	 * mData) runs automatically afterward via real C++'s own base-destructor
	 * cascade -- CModule has no reconstructed dtor of its own (module.h),
	 * same as every other real CModule-derived class in this project
	 * (mains.cpp's CFileMan/CResMan). The complete-object dtor (reconstructed
	 * here) stops short of the final `free(this)` the deleting-dtor overload
	 * adds -- same "only the complete-object dtor is reconstructed" convention
	 * already used throughout this project (e.g. CSysExMsgTaskBase,
	 * CClientCommServer).
	 */
	*(void ***)this = PTR__CESCommon_08fbafc8;
	*(void **)((char *)this + 0x2c) = DAT_08fbafec;
}

int CESCommon::Setup()
{
	CESCommonTask *task = new CESCommonTask(*(CModule *)this);
	CModule::Add(task);
	return 0;
}

int CESCommon::Start()
{
	return 0;
}

int CESCommon::Config()
{
	return 0;
}
