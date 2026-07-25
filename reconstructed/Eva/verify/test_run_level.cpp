/*
 * test_run_level.cpp  -  host-side known-answer test for CLevelManager::RunLevel()
 * (src/base/scheduler.cpp, Stage 6 breadth sweep, 2026-07-25).
 *
 * Drives the reconstructed RunLevel() directly against a synthetic CLevelManager-shaped
 * byte buffer (same technique as test_level_manager_array.cpp) plus a handful of fake
 * CTask-shaped byte blocks, checking the real per-tick behavior traced from the
 * decompile:
 *   - a task whose mask/flags byte (+0x4c) has either low bit set is skipped
 *     entirely -- its countdown is never touched
 *   - an unmasked task's countdown (+0x7a) decrements each call; only when it reaches
 *     0 does it reload from the task's own period (+0x78) and dispatch vtable slot+8
 *   - the level's own missed-tick counter (+0x1c) is unconditionally cleared every
 *     call, regardless of what happened above
 *   - CTaskBuffer::SendBuffer() (the level's own embedded +0x04 object) runs first;
 *     with mHead == NULL this is a real no-op (see test_task_buffer.cpp for that
 *     method's own direct coverage)
 */

#include <cstdio>
#include <cstring>
#include "level_manager_array.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static int g_execCount;
extern "C" void FakeTaskExec(void *)
{
	g_execCount++;
}

/* Fake CTask -- CallVSlot(task, 8) reads *(void**)task as the object's own vtable
 * POINTER (a separate array elsewhere in memory), same real C++ object-model
 * CModule/CTask etc. all use throughout this project -- NOT an inline vtable array
 * embedded in the object itself. First member here is that pointer; the 3-slot array
 * it points at is a separate static below. Sized well past +0x7a to stay a safe,
 * plausible stand-in for the real (much larger) CTask object.
 */
static void *g_fakeTaskVtbl[3] = { 0, 0, (void *)FakeTaskExec };

struct FakeTask {
	void          *vtblPtr;
	unsigned char  pad[0x7c - sizeof(void *)];
	/* task+0x4c, task+0x78, task+0x7a all land inside pad[]; accessed by raw
	 * byte offset from the object's own base, matching RunLevel()'s own real
	 * `(char*)task + 0xNN` arithmetic -- not through named members, since CTask
	 * itself isn't reconstructed as a typed class (see level_manager_array.h).
	 */
};

static FakeTask *make_task(unsigned char maskFlags, short period, short countdown)
{
	FakeTask *t = new FakeTask();
	memset(t, 0, sizeof(*t));
	t->vtblPtr = g_fakeTaskVtbl;
	*((unsigned char *)t + 0x4c) = maskFlags;
	*(short *)((char *)t + 0x78) = period;
	*(short *)((char *)t + 0x7a) = countdown;
	return t;
}

/* Fake CLevelManager -- 0x40 bytes, real fields per level_manager_array.h /
 * scheduler.cpp's own header comments. Only the fields RunLevel() itself reads/writes
 * are set up meaningfully; the rest stay zeroed.
 */
struct FakeLevelManager {
	unsigned char raw[0x40];
};

static FakeLevelManager *make_level_manager(FakeTask **tasks, int count)
{
	FakeLevelManager *lm = new FakeLevelManager();
	memset(lm, 0, sizeof(*lm));
	/* CTaskBuffer embedded at +0x04: mHead = NULL -> SendBuffer() no-ops. */
	*(void **)(lm->raw + 4) = 0;
	*(int *)(lm->raw + 8) = 0;
	/* missed-tick counter, deliberately pre-set to a nonzero sentinel so the
	 * test can confirm RunLevel() really clears it.
	 */
	*(int *)(lm->raw + 0x1c) = 0x7777;
	/* embedded COmegaPtrArray-shaped task queue at +0x20: count at +0x2c
	 * (relative +0xc), array pointer at +0x34 (relative +0x14).
	 */
	*(int *)(lm->raw + 0x2c) = count;
	*(void ***)(lm->raw + 0x34) = (void **)tasks;
	return lm;
}

int main(void)
{
	printf("CLevelManager::RunLevel() known-answer test\n");
	printf("=============================================\n");

	printf("[1] Unmasked task, countdown reaches 0 on this call -> dispatches "
	       "Exec() and reloads countdown from period\n");
	{
		FakeTask *t = make_task(0, 4, 1); /* period 4, countdown at 1 -> hits 0 this call */
		FakeTask *tasks[1] = { t };
		FakeLevelManager *lm = make_level_manager(tasks, 1);

		g_execCount = 0;
		CLevelManager::RunLevel(lm);

		check("Exec() dispatched once", g_execCount == 1);
		check("countdown reloaded from period (4)",
		      *(short *)((char *)t + 0x7a) == 4);
	}

	printf("[2] Unmasked task, countdown NOT yet at 0 -> just decrements, no "
	       "dispatch\n");
	{
		FakeTask *t = make_task(0, 4, 3); /* countdown 3 -> becomes 2, no fire */
		FakeTask *tasks[1] = { t };
		FakeLevelManager *lm = make_level_manager(tasks, 1);

		g_execCount = 0;
		CLevelManager::RunLevel(lm);

		check("Exec() NOT dispatched", g_execCount == 0);
		check("countdown decremented to 2", *(short *)((char *)t + 0x7a) == 2);
	}

	printf("[3] Masked task (flags bit0 set) -- countdown untouched, never "
	       "dispatched, even though it would have hit 0\n");
	{
		FakeTask *t = make_task(1, 4, 1);
		FakeTask *tasks[1] = { t };
		FakeLevelManager *lm = make_level_manager(tasks, 1);

		g_execCount = 0;
		CLevelManager::RunLevel(lm);

		check("Exec() NOT dispatched", g_execCount == 0);
		check("countdown left at 1 (untouched -- short-circuited before the "
		      "decrement)",
		      *(short *)((char *)t + 0x7a) == 1);
	}

	printf("[4] Masked task (flags bit1 set, matching the '& 3' mask) also "
	       "skipped\n");
	{
		FakeTask *t = make_task(2, 4, 1);
		FakeTask *tasks[1] = { t };
		FakeLevelManager *lm = make_level_manager(tasks, 1);

		g_execCount = 0;
		CLevelManager::RunLevel(lm);

		check("Exec() NOT dispatched", g_execCount == 0);
	}

	printf("[5] Mixed queue: masked task skipped, unmasked task fires -- only "
	       "one dispatch\n");
	{
		FakeTask *t0 = make_task(1, 4, 1); /* masked -- would fire if not masked */
		FakeTask *t1 = make_task(0, 2, 1); /* unmasked -- fires */
		FakeTask *tasks[2] = { t0, t1 };
		FakeLevelManager *lm = make_level_manager(tasks, 2);

		g_execCount = 0;
		CLevelManager::RunLevel(lm);

		check("exactly one dispatch", g_execCount == 1);
	}

	printf("[6] Empty task queue (count == 0) -- no dispatch, no crash\n");
	{
		FakeLevelManager *lm = make_level_manager(0, 0);

		g_execCount = 0;
		CLevelManager::RunLevel(lm);

		check("no dispatch", g_execCount == 0);
	}

	printf("[7] Missed-tick counter (+0x1c) is unconditionally cleared, "
	       "regardless of task states above\n");
	{
		FakeTask *t = make_task(1, 4, 1); /* masked, irrelevant to this check */
		FakeTask *tasks[1] = { t };
		FakeLevelManager *lm = make_level_manager(tasks, 1);

		check("pre-seeded sentinel is nonzero before the call",
		      *(int *)(lm->raw + 0x1c) == 0x7777);
		CLevelManager::RunLevel(lm);
		check("missed-tick counter cleared to 0 after RunLevel()",
		      *(int *)(lm->raw + 0x1c) == 0);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail != 0;
}
