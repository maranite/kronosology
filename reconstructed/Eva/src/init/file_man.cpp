/*
 * file_man.cpp  -  see include/file_man.h.
 */

#include "file_man.h"
#include "omega_vtables.h"
#include "cz_util.h"
#include "system_api.h"

#include <cstdlib>

extern CSystemApi *Api; /* mains.cpp */

/* Real class-static global (symbols.csv: CFileMan::SysName, 4 bytes, a `const
 * char*`) -- same "opaque, content not decoded, non-null is all CModule::CModule()
 * needs" treatment as mains.cpp's own CEditMan_SysName/CViewBase_SysName/
 * CChunkMan_SysName.
 */
extern "C" const char *CFileMan_SysName = "FileMan";

CFileMan::CFileMan()
	: CModule(CFileMan_SysName)
{
	/* Manual vtable-swap idiom: install this class' own real vtable now that
	 * CModule's base ctor has finished (matches ground truth exactly -- unlike
	 * every other MMainXxx(void) module in mains.cpp, CFileMan's own real ctor
	 * does this swap itself rather than leaving it to its caller).
	 */
	*reinterpret_cast<void **>(this) = (void *)PTR__CFileMan_08e86e48;

	for (int i = 0; i < 128; i++) {
		mUnitTable[i].mField0 = 1;
		mUnitTable[i].mField4 = 0;
	}
	for (int i = 0; i < 128; i++) {
		mHandleTable[i].mField0 = 1;
		mHandleTable[i].mField4 = 0;
	}
	for (int i = 0; i < 32; i++) {
		mIOCTLDevTable[i].mField0 = 1;
		mIOCTLDevTable[i].mField4 = 0;
		mIOCTLDevTable[i].mField8 = 0;
		mIOCTLDevTable[i].mFieldC = 0;
	}

	/* mDriverConstructors already default-constructed (member init list); real
	 * ctor installs its own vtable-swap target right after (same order as ground
	 * truth).
	 */
	*reinterpret_cast<void **>(&mDriverConstructors) = (void *)PTR__TNamedPtrArray_08e86fc8;
	mUnknownA44 = 0x400;
	mUnknownA48 = 0x2000;
	mUnknownA4c = 0;

	mBackgroundJobsEnabled = 0;

	typedef char *(*GetConfigStringFn)(void *, const char *);
	void *api = Api;
	GetConfigStringFn getCfg = *reinterpret_cast<GetConfigStringFn *>(
		(char *)*reinterpret_cast<void **>(api) + 0x38);

	char *val = getCfg(api, "FMBackGroundJobs");
	if (val != 0 && CZ::StrCmpIgnoreCase(val, "Enabled") == 0)
		mBackgroundJobsEnabled = 1;

	mMinIdleTimeToStartBGJobs = 1000;
	val = getCfg(Api, "FMMinIdleTimeToStartBGJobs");
	if (val != 0)
		mMinIdleTimeToStartBGJobs = strtol(val, 0, 10);

	mDeltaTimeBetweenBGJobs = 800;
	val = getCfg(Api, "FMDeltaTimeBetweenBGJobs");
	if (val != 0)
		mDeltaTimeBetweenBGJobs = strtol(val, 0, 10);
}

void CFileMan::Setup()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

void CFileMan::Config()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

void CFileMan::Start()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

int CFileMan::IsBackgroundJobsEnabled() const
{
	return mBackgroundJobsEnabled;
}

void CFileMan::EnableBackgroundJobs(int enabled)
{
	mBackgroundJobsEnabled = enabled;
}

extern "C" void CFileManSetupVSlot(void *obj)
{
	static_cast<CFileMan *>(obj)->Setup();
}

extern "C" void CFileManConfigVSlot(void *obj)
{
	static_cast<CFileMan *>(obj)->Config();
}

extern "C" void CFileManStartVSlot(void *obj)
{
	static_cast<CFileMan *>(obj)->Start();
}

/* Real vtable definition -- 55 slots (omega_vtables.h). Slots 2/3/4 (Setup/Config/
 * Start) wired to the real forwarders above; every other slot (the ~50 real
 * FDisk/RegisterDriver/... virtuals this pass doesn't reconstruct, plus CModule's
 * own dtor/Destroy/GetErrorMsg pair) stays EvaVTableStub.
 */
void *PTR__CFileMan_08e86e48[55] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CFileManSetupVSlot, (void *)CFileManConfigVSlot, (void *)CFileManStartVSlot,
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
