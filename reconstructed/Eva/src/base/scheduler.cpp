/*
 * scheduler.cpp  -  see include/scheduler.h.
 *
 * Transcribed from the Ghidra decompile export:
 *   CScheduler::CScheduler()   .text+0x08062380, 87 bytes
 *   CScheduler::InsertLevel()  .text+0x08062b40, 316 bytes
 *   CScheduler::Enable()       .text+0x08063120, 119 bytes
 *   CScheduler::Exec()         .text+0x080623e0, 1025 bytes  (Stage 6, 2026-07-25)
 *   CLevelManagerArray::Find() .text+0x0805ee90, 258 bytes   (Stage 6, 2026-07-25)
 *   CLevelManagerArray::Add()  .text+0x0805ec70, 522 bytes   (Stage 6, 2026-07-25)
 *
 * (Find/Add's addresses were mistranscribed in this file's own comment before the
 * 2026-07-25 pass -- off by an inserted digit, 0x08062ee90/0x08062ec70 -- corrected
 * against functions.csv while reconstructing them for real.)
 *
 * CSysApiInstance::WriteMessageToHost(int,int) (.text+0x0806aa00, 64 bytes) stays a
 * Tier-B link-stub (implemented once in sysapi_instance.h/cpp, not here) -- genuinely
 * out of scope, a host-message-queue substrate nothing on this pass's own boot path
 * needs to actually deliver.
 *
 * CLevelManagerArray IS-A COmegaPtrArray (vtable-swapped, no new fields -- same
 * manual-vtable-swap idiom used throughout this project) with element layout confirmed
 * from InsertLevel()'s own field writes (already reconstructed below) cross-checked
 * against Exec()'s/RunLevel()'s own reads:
 *   +0x0c  level number (ETaskLevel) -- the sort/search key both Find() and Add()'s own
 *          sift-up step compare on
 *   +0x10  "currently running" reentrancy guard -- set 1/cleared 0 by Exec() itself
 *          around each RunLevel() call; nothing in this reconstruction sets it
 *          concurrently (single-threaded per-tick dispatch), so Exec()'s own "bail the
 *          whole tick" branch on this field stays unreached, faithfully preserved
 *          rather than dropped
 *   +0x14  "still being configured" flag -- InsertLevel() briefly sets 1 during its own
 *          critical section, then resets 0 right before returning; always 0 by the time
 *          Exec() ever sees a level, so Exec()'s own gate on this field is always taken
 *   +0x18  (short) countdown-to-next-run tick counter
 *   +0x1a  (short) reload period -- InsertLevel() sets both +0x18/+0x1a to 1 (run every
 *          tick); nothing in this reconstruction changes the period afterward
 *   +0x1c  (int) missed-tick counter -- incremented by Exec()'s own reentrancy-bail
 *          path, cleared by RunLevel()'s own real tail (see CLevelManager::RunLevel()
 *          below)
 *   +0x38  "disabled" bail flag -- InsertLevel() zeroes it; Exec() bails the *whole*
 *          tick (not just this level) if it's ever nonzero, unreached here for the same
 *          reason as +0x10 above
 *
 * CScheduler::Exec()'s real body is GCC's classic Duff's-device-unrolled (4-way) linear
 * walk over CLevelManagerArray's flat array -- collapsed to a plain per-index loop here,
 * same license/verification method as omega_ptr_array.cpp's 5 methods (index-by-index
 * checked against the real decompile while writing this). The real disassembly brackets
 * each level's readyCounter read/decrement with HAL_DisableInterrupts()/
 * HAL_EnableInterrupts() -- dropped here, same established reason as every other
 * occurrence of that pair in this project (ckernel.cpp/omega_interface.cpp): a
 * kernel-side critical-section shim, not a real userspace primitive, and this
 * reconstruction's own Exec()/RunLevel() calls are never actually reentrant.
 */

#include "scheduler.h"
#include "level_manager_array.h"
#include "omega_ptr_array.h"
#include "omega_vtables.h"
#include "sysapi_instance.h"

#include <cstdlib>
#include <new>

/* CLevelManagerArray/CLevelManager themselves now live in level_manager_array.h (their
 * own header, so verify/test_level_manager_array.cpp can drive Add()/Find() directly) --
 * see that header and this file's own top comment for the full field-offset writeup.
 * CSysApiInstance itself is shared (sysapi_instance.h) -- WriteMessageToHost(int,int)
 * is implemented once there, not redeclared/redefined here.
 */

/* Real static CLevelManager member the ctor sets to point at this scheduler's own
 * level array -- CLevelManager's own (unreconstructed) methods presumably use it to
 * find their owning array; nothing in this pass calls those methods.
 */
namespace {
void *sm_poLevels_CLevelManager = 0;
}

CLevelManager *CLevelManagerArray::Find(void *arrayThis, int level)
{
	int count = *(int *)((char *)arrayThis + 0xc);
	void **arr = *(void ***)((char *)arrayThis + 0x14);

	for (int i = 0; i < count; i++) {
		void *lm = arr[i];
		if (*(int *)((char *)lm + 0xc) == level)
			return (CLevelManager *)lm;
	}
	return 0;
}

void CLevelManagerArray::Add(void *arrayThis, CLevelManager *level)
{
	/* CLevelManagerArray IS-A COmegaPtrArray (vtable swap only) -- append via the
	 * real base method, same object.
	 */
	unsigned idx = (unsigned)((COmegaPtrArray *)arrayThis)->Add(level);
	if (idx == 0x7fffffffu || idx == 0)
		return; /* growth failure, or first element (nothing to sift) */

	/* Sift the newly appended element left while its own level number is smaller
	 * than its predecessor's -- keeps the array sorted ascending by level.
	 */
	void **arr = *(void ***)((char *)arrayThis + 0x14);
	while (idx > 0) {
		int curLevel = *(int *)((char *)arr[idx] + 0xc);
		int prevLevel = *(int *)((char *)arr[idx - 1] + 0xc);
		if (curLevel >= prevLevel)
			break;
		void *tmp = arr[idx];
		arr[idx] = arr[idx - 1];
		arr[idx - 1] = tmp;
		idx--;
	}
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
	int count = *(int *)((char *)this + 0x10);
	if (count <= 0)
		return;

	void **arr = *(void ***)((char *)this + 0x18);

	for (int i = 0; i < count; i++) {
		unsigned char *lm = (unsigned char *)arr[i];

		if (*(int *)(lm + 0x38) != 0)
			return; /* real: bails the whole tick, not just this level */

		if (*(int *)(lm + 0x10) != 0) {
			*(int *)(lm + 0x1c) += 1;
			return; /* real: also bails the whole tick */
		}

		if (*(int *)(lm + 0x14) == 0) {
			short *readyCounter = (short *)(lm + 0x18);
			if (*readyCounter == 1) {
				*(int *)(lm + 0x10) = 1;
				*readyCounter = *(short *)(lm + 0x1a);
				CLevelManager::RunLevel(lm);
				*(int *)(lm + 0x10) = 0;
			} else {
				*readyCounter = (short)(*readyCounter - 1);
			}
		}
	}
}

void CScheduler::EnableUpdate(int /*enable*/)
{
	/* Tier-B link-stub -- .text+0x080631c0, not measured. See scheduler.h. */
}
