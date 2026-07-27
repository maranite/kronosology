/*
 * batch_disk_man.cpp  -  see include/batch_disk_man.h.
 *
 * CBatchDiskMan::CBatchDiskMan/~CBatchDiskMan/Setup/Config/Start/IsBusy/
 * IsPreloadRunning transcribed from CBatchDiskMan@082436e0.c/
 * ~CBatchDiskMan@082433c0.c,082434c0.c/Setup@082435d0.c/Config@08243330.c/
 * Start@08243320.c/IsBusy@082437c0.c/IsPreloadRunning@082437e0.c.
 */

#include "batch_disk_man.h"
#include "omega_vtables.h"
#include "system_api.h"

#include <cstdlib>
#include <new>

extern CSystemApi *Api; /* mains.cpp */

CBatchDiskMan::CBatchDiskMan(const char *name, const char *param2)
	: CModule(name), CEditServer(name), mMainTask(0), mEditTask(0)
{
	/* Real: overwrites both base sub-object vtable pointers with
	 * CBatchDiskMan's own combined vtables, right after both base ctors
	 * already installed their own generic base vtables -- same manual
	 * vtable-swap idiom as CESCommon::CESCommon() (es_common.cpp), using raw
	 * pointer arithmetic rather than named-member access so this class needs
	 * no new `friend` grant from module.h/edit_server.h.
	 */
	*(void ***)this = PTR__CBatchDiskMan_08eac048;
	*(void **)((char *)this + 0x2c) = PTR__CBatchDiskMan_08eac06c;

	if (param2 != 0) {
		void *raw = malloc(0xc);
		mParam = new (raw) CParameterString(param2);
	} else {
		mParam = 0;
	}
}

CBatchDiskMan::~CBatchDiskMan()
{
	*(void ***)this = PTR__CBatchDiskMan_08eac048;
	*(void **)((char *)this + 0x2c) = PTR__CBatchDiskMan_08eac06c;

	if (mParam != 0) {
		mParam->~CParameterString();
		free(mParam);
	}
	/* CEditServer::~CEditServer() (already real, edit_server.h) runs
	 * automatically via the base-destructor cascade. Base CModule teardown
	 * is a known, already-precedented gap (no ~CModule() exists in this
	 * project) -- same as CESCommon::~CESCommon()/CPanel::~CPanel().
	 */
}

int CBatchDiskMan::Setup()
{
	const char *preloadList = 0;
	if (mParam != 0)
		preloadList = mParam->GetParamStr("PRELOAD");

	void *raw1 = malloc(sizeof(CBatchDiskMainTask));
	mMainTask = new (raw1) CBatchDiskMainTask(*this, preloadList);
	CModule::Add(mMainTask);

	void *raw2 = malloc(sizeof(CEditTask));
	mEditTask = new (raw2) CEditTask(*this);
	CModule::Add(mEditTask);

	if (mParam != 0) {
		mParam->~CParameterString();
		free(mParam);
	}
	mParam = 0;

	return 0;
}

int CBatchDiskMan::Config()
{
	/* Real: registers this module + mEditTask + mMainTask's own names/
	 * outlink name through a global Api registration slot (Api+0x44,
	 * `ds:0x930a1f4` = `Api`, sysapi_instance.cpp) -- see header comment.
	 * That slot is not named/reconstructed anywhere else in this project
	 * yet, so it's dispatched here the same raw `CallVSlot`-style way
	 * ckernel.cpp/module_manager.cpp already use elsewhere, rather than
	 * inventing a new named wrapper for a single, previously-unseen call
	 * site. `mMainTask`/`mEditTask`'s own `mName` fields are CTask's own
	 * private +0x04 member (task.h) -- read here via the same raw-offset
	 * convention module_manager.cpp already uses for CModule's own private
	 * fields, rather than adding a new public accessor to task.h for a
	 * single caller.
	 *
	 * Exact argument order transcribed directly from the real disassembly
	 * (own `mName` passed twice, at arg1 and arg4 -- preserved verbatim,
	 * not simplified, since the real Api+0x44 slot's own semantics are
	 * undecoded and might genuinely use 2 separate name/outlink pairs
	 * sharing one owner-name argument):
	 *   arg0 = Api (the call's own "this")
	 *   arg1 = this->mName (CModule's own +0x04 field, i.e. CBatchDiskMan's
	 *          own name)
	 *   arg2 = mEditTask->mName (CTask's own +0x04 field)
	 *   arg3 = mEditTask->GetOutLinkName()
	 *   arg4 = this->mName again
	 *   arg5 = mMainTask->mName (CTask's own +0x04 field)
	 *   arg6 = 0 (constant)
	 */
	typedef void (*RegisterFn)(void *, const char *, const char *, const char *,
	                            const char *, const char *, int);
	void *apiVtbl = *(void **)Api;
	RegisterFn registerFn = *(RegisterFn *)((char *)apiVtbl + 0x44);
	const char *ownName = *(const char **)((char *)this + 4);
	const char *editTaskName = *(const char **)((char *)mEditTask + 4);
	const char *mainTaskName = *(const char **)((char *)mMainTask + 4);
	registerFn(Api, ownName, editTaskName, mEditTask->GetOutLinkName(),
	           ownName, mainTaskName, 0);
	return 0;
}

int CBatchDiskMan::Start()
{
	return 0;
}

bool CBatchDiskMan::IsBusy() const
{
	return mMainTask->IsBusy();
}

bool CBatchDiskMan::IsPreloadRunning() const
{
	return mMainTask->IsPreloadRunning();
}

bool CBatchDiskMan::IsPreloadRunning(unsigned char group, const char *name) const
{
	return mMainTask->IsPreloadRunning(group, name);
}
