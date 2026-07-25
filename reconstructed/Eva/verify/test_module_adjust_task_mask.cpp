/*
 * test_module_adjust_task_mask.cpp  -  host-side known-answer test for
 * CModule::AdjustTaskMask() (src/base/module.cpp, Stage 6 breadth sweep,
 * 2026-07-25 -- upgraded from a Tier-B link-stub to Tier A).
 *
 * Builds a raw CModule-shaped buffer (same "poke the real offsets directly,
 * skip the real ctor" pattern verify/test_module_manager_add_module.cpp already
 * uses for CModuleManager) with a synthetic mTasks array of fake CTask-shaped
 * blobs, then checks:
 *   - count == 0: no-op, no crash (real early-out on `iVar4 != 0`)
 *   - bit 0x02 is cleared on every task in the array, regardless of its
 *     starting value (0x00/0x02/0x03/0xff)
 *   - other bits (e.g. 0x01, the OTHER "masked" bit RunLevel() checks) are left
 *     untouched -- AdjustTaskMask() only ever un-masks its OWN bit
 *   - walk direction doesn't matter to the *outcome* (real code walks
 *     backwards, an implementation detail with no externally-observable
 *     difference here) -- checked by using an array where each slot's
 *     starting byte is distinct, so any index-computation bug would show up
 *     as a wrong final value at the wrong slot
 *   - a larger (9-element, past the single-iteration Duff's-device remainder
 *     cutoff of 8) array exercises the loop's -8 wraparound path too
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "module.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* Real CModule layout (module.h): vtbl(+0)/mName(+4)/mTasks(+8, 0x18 bytes)/
 * mUnknown20(+0x20)/mState(+0x24)/mScopeId(+0x28). mTasks' own count/array
 * fields land at absolute +0x14/+0x1c (COmegaPtrArray-relative +0xc/+0x14).
 */
struct FakeModule {
	void  *vtbl;        /* +0x00 */
	char  *name;        /* +0x04 */
	char   tasksPad0[0xc]; /* +0x08 .. +0x14, mTasks' own vtbl/mUnknown04/mCapacity */
	int    taskCount;   /* +0x14 */
	int    taskGrowBy;  /* +0x18, unused here */
	void **taskArray;   /* +0x1c */
	int    unknown20;   /* +0x20 */
	int    state;       /* +0x24 */
	int    scopeId;     /* +0x28 */
};

/* Minimal fake CTask-shaped blob: only +0x4c (the mask/flags byte) matters to
 * AdjustTaskMask(). Sized generously past 0x4c so the write is never OOB.
 */
struct FakeTask {
	unsigned char pad[0x50];
};

int main()
{
	printf("test_module_adjust_task_mask:\n");

	/* Case 1: count == 0 -- must be a clean no-op (real `if (iVar4 != 0)`
	 * early-out), not a crash from dereferencing a null/garbage array.
	 */
	{
		FakeModule m;
		memset(&m, 0, sizeof(m));
		m.taskCount = 0;
		m.taskArray = 0;
		((CModule *)&m)->AdjustTaskMask();
		check("count==0 is a no-op", true /* reaching here at all is the check */);
	}

	/* Case 2: 4 tasks (fewer than the 8-way unroll width), distinct starting
	 * mask bytes, confirms bit 0x02 clears everywhere and nothing else moves.
	 */
	{
		const int N = 4;
		FakeTask tasks[N];
		void *arr[N];
		unsigned char start[N] = { 0x00, 0x02, 0x03, 0xff };
		for (int i = 0; i < N; i++) {
			memset(&tasks[i], 0, sizeof(tasks[i]));
			tasks[i].pad[0x4c] = start[i];
			arr[i] = &tasks[i];
		}

		FakeModule m;
		memset(&m, 0, sizeof(m));
		m.taskCount = N;
		m.taskArray = arr;
		((CModule *)&m)->AdjustTaskMask();

		check("4-task[0] 0x00 -> 0x00", tasks[0].pad[0x4c] == 0x00);
		check("4-task[1] 0x02 -> 0x00", tasks[1].pad[0x4c] == 0x00);
		check("4-task[2] 0x03 -> 0x01 (bit 0x01 untouched)", tasks[2].pad[0x4c] == 0x01);
		check("4-task[3] 0xff -> 0xfd (only bit 0x02 cleared)", tasks[3].pad[0x4c] == 0xfd);
	}

	/* Case 3: 9 tasks -- exercises the real function's -8 unrolled-loop
	 * wraparound (its own while() tail past the first Duff's-device dispatch).
	 * All start fully masked (0xff); every one must end up with only bit 0x02
	 * cleared (0xfd), including index 0 after the wraparound.
	 */
	{
		const int N = 9;
		FakeTask tasks[N];
		void *arr[N];
		for (int i = 0; i < N; i++) {
			memset(&tasks[i], 0, sizeof(tasks[i]));
			tasks[i].pad[0x4c] = 0xff;
			arr[i] = &tasks[i];
		}

		FakeModule m;
		memset(&m, 0, sizeof(m));
		m.taskCount = N;
		m.taskArray = arr;
		((CModule *)&m)->AdjustTaskMask();

		bool allOk = true;
		for (int i = 0; i < N; i++) {
			if (tasks[i].pad[0x4c] != 0xfd)
				allOk = false;
		}
		check("9-task wraparound: all slots 0xff -> 0xfd", allOk);
	}

	printf("%s\n", g_fail ? "FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
