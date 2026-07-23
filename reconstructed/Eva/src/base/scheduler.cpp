/*
 * scheduler.cpp  -  see include/scheduler.h.
 *
 * Transcribed from the Ghidra decompile export:
 *   CScheduler::CScheduler()   .text+0x08062380, 87 bytes
 *   CScheduler::InsertLevel()  .text+0x08062b40, 316 bytes
 *   CScheduler::Enable()       .text+0x08063120, 119 bytes
 *
 * CLevelManagerArray::Find()/Add() (.text+0x08062ee90/0x08062ec70, 258/522 bytes) and
 * CSysApiInstance::WriteMessageToHost(int,int) (.text+0x0806aa00, 64 bytes) are
 * Tier-B link-stubs -- real, correctly-mangled, empty bodies, not behaviorally
 * reconstructed (genuinely deeper CLevelManagerArray/host-message-queue substrate,
 * out of scope for this pass). Find() always reports "not found" (its real sentinel
 * return, CLevelManager* NULL), which happens to be exactly right for every level
 * InsertLevel() is asked to insert here since nothing else populates the array in this
 * reconstruction -- so InsertLevel()'s own real "build a new CLevelManager and add it"
 * path is still exercised faithfully even though Add() itself then does nothing with
 * the result (a harmless leak under stub Add(), not a behavioral divergence for
 * anything this pass's boot-path measures).
 */

#include "scheduler.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "sysapi_instance.h"

#include <cstdlib>
#include <new>

/* ---- Tier-B call-contract classes this file needs but doesn't reconstruct.
 * CSysApiInstance itself is shared (sysapi_instance.h) -- WriteMessageToHost(int,int)
 * is implemented once there, not redeclared/redefined here.
 */
class CLevelManager;

class CLevelManagerArray {
public:
	static CLevelManager *Find(void *arrayThis, int level);
	static void Add(void *arrayThis, CLevelManager *level);
};

/* Real static CLevelManager member the ctor sets to point at this scheduler's own
 * level array -- CLevelManager's own (unreconstructed) methods presumably use it to
 * find their owning array; nothing in this pass calls those methods.
 */
namespace {
void *sm_poLevels_CLevelManager = 0;
}

CLevelManager *CLevelManagerArray::Find(void * /*arrayThis*/, int /*level*/)
{
	return 0; /* Tier-B: always "not found". See file header comment. */
}

void CLevelManagerArray::Add(void * /*arrayThis*/, CLevelManager * /*level*/)
{
	/* Tier-B no-op. */
}

CScheduler::CScheduler()
{
	new (mLevels) COmegaPtrArray();

	mBusy = 0;
	*(void **)mLevels = (void *)PTR__CLevelManagerArray_08e80c28;
	mNotifyHost = 0;
	mUnusedA = 0;
	mUnusedB = 0;
	mReady = 0;

	sm_poLevels_CLevelManager = mLevels;
}

void CScheduler::InsertLevel(int level)
{
	CLevelManager *found = CLevelManagerArray::Find(mLevels, level);
	if (found != 0)
		return;

	int savedEnabled = mUnusedA;
	mUnusedA = 0;

	/* Real malloc(0x40) + hand-built CLevelManager, same manual-vtable-swap idiom as
	 * CModule::CModule/CScheduler's own ctor above.
	 */
	unsigned char *lm = (unsigned char *)malloc(0x40);
	*(int *)(lm + 4) = 0;
	*(int *)(lm + 8) = 0;
	*(void **)lm = (void *)PTR__CLevelManager_08e80e50;
	*(int *)(lm + 0xc) = level;
	*(int *)(lm + 0x10) = 0;
	*(int *)(lm + 0x14) = 1;
	*(short *)(lm + 0x18) = 1;
	*(short *)(lm + 0x1a) = 1;
	*(int *)(lm + 0x1c) = 0;

	new (lm + 0x20) COmegaPtrArray();
	*(void **)(lm + 0x20) = (void *)PTR__TNamedPtrArray_08e80ea8;
	*(int *)(lm + 0x38) = 0;
	*(int *)(lm + 0x3c) = 0;

	CLevelManagerArray::Add(mLevels, (CLevelManager *)lm);
	*(int *)(lm + 0x14) = 0;

	mUnusedA = savedEnabled;
	mBusy = 0;

	if (mReady != 0 && mNotifyHost != 0)
		((CSysApiInstance *)SysApiInstance)->WriteMessageToHost(3, 0x1c);
}

void CScheduler::Enable(int enable)
{
	int wasEnabled = mUnusedA;
	mUnusedA = enable;

	if (enable == 0)
		return;

	if (mReady == 0) {
		mReady = 1;
		mBusy = 0;
		if (mNotifyHost != 0) {
			((CSysApiInstance *)SysApiInstance)->WriteMessageToHost(3, 0x1c);
			enable = mUnusedA;
		}
	}

	if (enable != 0 && wasEnabled == 0 && mUnusedB != 0)
		mUnusedB = 0;
}

void CScheduler::Exec()
{
	/* Tier-B link-stub -- .text+0x080623e0, 1025 bytes, real per-tick task
	 * dispatch loop over CLevelManagerArray x CLevelManager's own task queues.
	 * Genuinely out of scope for this pass; see scheduler.h.
	 */
}

void CScheduler::EnableUpdate(int /*enable*/)
{
	/* Tier-B link-stub -- .text+0x080631c0, not measured. See scheduler.h. */
}
