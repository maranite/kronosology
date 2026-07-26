/*
 * test_alpha_keyb_ctrl.cpp  -  host-side known-answer test for CAlphaKeybCtrl
 * (src/hw/alpha_keyb_ctrl.cpp), CAlphaKeybCtrlTask (src/hw/alpha_keyb_ctrl_task.cpp),
 * CKeyboardLayout (src/hw/keyboard_layout.cpp), and the CAlphaKeybCtrlConstructor
 * factory wiring (mains.cpp). Eva CAlphaKeybCtrl/CAlphaKeybCtrlTask batch,
 * 2026-07-26. See alpha_keyb_ctrl.h/alpha_keyb_ctrl_task.h/keyboard_layout.h for the
 * full ground-truth writeup.
 *
 * Checks:
 *   [1] CAlphaKeybCtrl ctor: own real vtable installed (raw offset-0 read, matches
 *       PTR__CAlphaKeybCtrl_08eabb68).
 *   [2] CAlphaKeybCtrl::Setup(): mTask becomes non-null, genuinely a
 *       CAlphaKeybCtrlTask (own real vtable check), registered into CModule's own
 *       mTasks via CModule::Add().
 *   [3] CAlphaKeybCtrl::Config()/Start() -- real, don't crash; Start() dispatches
 *       Initialize() (which itself dispatches an Api+0x44 call, checked separately).
 *   [4] CAlphaKeybCtrlConstructor::Create() factory wiring (mains.cpp) -- real
 *       end-to-end path.
 *   [5] PTR__CAlphaKeybCtrl_08eabb68's own Setup slot -- real CModuleManager-shape
 *       raw dispatch (same vtable-dispatch-gap check as test_panel.cpp's own [6]).
 *   [6] CAlphaKeybCtrlTask ctor under a stub Api (LookupResourceStub-equivalent,
 *       Api+0xac returns NULL): mHidResource stays NULL, exactly 1 layout lands in
 *       the primary layout list (Default) and exactly 1 in the ASCII list (Custom
 *       ASCII) -- the 13 international layouts are NOT built (buildExtraLayouts
 *       false), matching this reconstruction's own documented stub behavior.
 *   [7] CAlphaKeybCtrlTask::Exec() with mHidResource == 0 -- real fast path:
 *       SetMask(1), returns -1.
 *   [8] CAlphaKeybCtrlTask::ProcessEvent() with CLocaleManager::GetKeyboardLayout()
 *       stubbed to NULL -- real fast path: returns 0, does not crash.
 *   [9] CAlphaKeybCtrlTask::SetCtrlCondition() -- direct unit test of the real bit
 *       logic (self-contained, no Api dependency), both down=true and down=false
 *       paths for all 4 recognized keycodes plus the default/unrecognized case.
 *   [10] CKeyboardLayout ctor -- constructs without crashing/asserting for a
 *        representative built-in descriptor (kKeyboardLayoutDescs[0], "Default").
 */

#include <cstdio>
#include <cstring>
#include <new>

#include "alpha_keyb_ctrl.h"
#include "alpha_keyb_ctrl_task.h"
#include "keyboard_layout.h"
#include "module.h"
#include "omega_vtables.h"
#include "system_api.h"

/* Same local re-declaration convention as test_panel.cpp/test_task.cpp/etc -- each
 * verify test file keeps its own copy rather than sharing a header.
 */
struct ModuleTestHooks {
	static int TaskCount(const CModule &m)
	{
		return *(const int *)((const unsigned char *)&m + 0x14);
	}
	static void *TaskAt(const CModule &m, int i)
	{
		void **arr = *(void ***)((const unsigned char *)&m + 0x1c);
		return arr[i];
	}
};

struct AlphaKeybCtrlTestHooks {
	static CAlphaKeybCtrlTask *Task(const CAlphaKeybCtrl &c) { return c.mTask; }
	static void SetTask(CAlphaKeybCtrl &c, CAlphaKeybCtrlTask *task) { c.mTask = task; }
};

struct AlphaKeybCtrlTaskTestHooks {
	static void *HidResource(const CAlphaKeybCtrlTask &t) { return t.mHidResource; }
	static int LayoutCount(const CAlphaKeybCtrlTask &t)
	{
		return (int)(t.mLayoutList.mEnd - t.mLayoutList.mBegin);
	}
	static int AsciiLayoutCount(const CAlphaKeybCtrlTask &t)
	{
		return (int)(t.mAsciiLayoutList.mEnd - t.mAsciiLayoutList.mBegin);
	}
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* --- fake Api global, same shape/idiom as test_panel.cpp's own -------------- */

extern CSystemApi *Api;

extern "C" void *PTR__CAlphaKeybCtrlConstructor_08eabb48[3];

extern "C" int FakeScopeIdFn(void *) { return 0x1234; }
extern "C" void FakeApiNoOp() {}
extern "C" void *FakeLookupReturnsNull(void *, const char *) { return 0; }

static void *g_fakeApiVtbl[96];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void setup_fake_api()
{
	for (int i = 0; i < 96; i++)
		g_fakeApiVtbl[i] = (void *)FakeApiNoOp;
	g_fakeApiVtbl[0x3c / 4] = (void *)FakeScopeIdFn;
	/* Api+0xac ("named-resource lookup", same shape as CPoller's own, poller.h)
	 * returns NULL -- CAlphaKeybCtrlTask's ctor takes its real, safe
	 * "lookup failed" fallback path, matching this reconstruction's documented
	 * stub behavior throughout.
	 */
	g_fakeApiVtbl[0xac / 4] = (void *)FakeLookupReturnsNull;
	g_fakeApiObj.vtbl = g_fakeApiVtbl;
	Api = (CSystemApi *)&g_fakeApiObj;
}

int main()
{
	printf("CAlphaKeybCtrl/CAlphaKeybCtrlTask known-answer test\n");
	printf("=====================================================\n");
	setup_fake_api();

	printf("[1] CAlphaKeybCtrl ctor -- real vtable installed\n");
	{
		CAlphaKeybCtrl ctrl("AlphaKeybCtrl", "HIDDRV=HIDDriver");
		void *vtbl = *(void **)(void *)&ctrl;
		check("vtbl == PTR__CAlphaKeybCtrl_08eabb68",
		      vtbl == (void *)PTR__CAlphaKeybCtrl_08eabb68);
	}

	printf("[2] CAlphaKeybCtrl::Setup() -- mTask constructed + registered via CModule::Add()\n");
	{
		CAlphaKeybCtrl ctrl("AlphaKeybCtrl", "HIDDRV=HIDDriver");
		int rc = ctrl.Setup();
		check("Setup() returns 0", rc == 0);

		CAlphaKeybCtrlTask *task = AlphaKeybCtrlTestHooks::Task(ctrl);
		check("mTask != 0", task != 0);
		void *taskVtbl = task != 0 ? *(void **)(void *)task : 0;
		check("mTask's own vtbl == PTR__CAlphaKeybCtrlTask_08eabcc8 (genuinely a "
		      "CAlphaKeybCtrlTask)",
		      taskVtbl == (void *)PTR__CAlphaKeybCtrlTask_08eabcc8);

		check("CModule::mTasks count == 1 (CModule::Add() really ran)",
		      ModuleTestHooks::TaskCount(ctrl) == 1);
		check("CModule::mTasks[0] == mTask",
		      ModuleTestHooks::TaskAt(ctrl, 0) == (void *)task);

		printf("[3] CAlphaKeybCtrl::Config()/Start() -- real, don't crash\n");
		{
			check("Config() returns 0", ctrl.Config() == 0);
			check("Start() returns 0", ctrl.Start() == 0);
			check("Start() did not crash dispatching Initialize() through mTask",
			      true);
		}

		printf("[6] CAlphaKeybCtrlTask ctor under stub Api -- Default+CustomASCII only\n");
		{
			check("mHidResource stays NULL (LookupResourceStub-equivalent path)",
			      AlphaKeybCtrlTaskTestHooks::HidResource(*task) == 0);
			check("mLayoutList holds exactly 1 element (Default only -- the 13 "
			      "international layouts are NOT built)",
			      AlphaKeybCtrlTaskTestHooks::LayoutCount(*task) == 1);
			check("mAsciiLayoutList holds exactly 1 element (Custom ASCII)",
			      AlphaKeybCtrlTaskTestHooks::AsciiLayoutCount(*task) == 1);
		}

		printf("[7] CAlphaKeybCtrlTask::Exec() -- mHidResource == 0 fast path\n");
		{
			int rc2 = task->Exec();
			check("Exec() returns -1", rc2 == -1);
		}

		printf("[8] CAlphaKeybCtrlTask::ProcessEvent() -- GetKeyboardLayout() stub "
		       "returns NULL fast path\n");
		{
			SKeyboardEvt evt;
			memset(&evt, 0, sizeof(evt));
			evt.isKeyDown = 1;
			evt.keycode = 0x1e; /* 'A' scancode-ish, an ordinary (non-sticky) key */
			int rc2 = task->ProcessEvent(&evt);
			check("ProcessEvent() returns 0 (layout lookup failed, real fast path)",
			      rc2 == 0);
		}
	}

	printf("[4] CAlphaKeybCtrlConstructor::Create() factory wiring (mains.cpp) -- real "
	       "end-to-end path\n");
	{
		typedef void *(*CreateFn)(void *, void *, void *, int);
		CreateFn create = (CreateFn)PTR__CAlphaKeybCtrlConstructor_08eabb48[2];
		void *obj = create(0, (void *)"AlphaKeybCtrl", (void *)"HIDDRV=HIDDriver", 0);
		check("Create() returns non-null", obj != 0);
		void *vtbl = obj != 0 ? *(void **)obj : 0;
		check("created object's vtbl == PTR__CAlphaKeybCtrl_08eabb68 (genuinely a "
		      "CAlphaKeybCtrl)",
		      vtbl == (void *)PTR__CAlphaKeybCtrl_08eabb68);
	}

	printf("[5] PTR__CAlphaKeybCtrl_08eabb68's own Setup slot -- real CModuleManager-"
	       "shape raw dispatch\n");
	{
		CAlphaKeybCtrl ctrl("AlphaKeybCtrl", "HIDDRV=HIDDriver");
		/* mTask is genuinely uninitialized garbage right after the real ctor
		 * (alpha_keyb_ctrl.h's own header comment -- preserved ground-truth
		 * quirk), so force a known sentinel and confirm the real dispatch
		 * overwrites it.
		 */
		CAlphaKeybCtrlTask *sentinel = (CAlphaKeybCtrlTask *)0xdeadbeef;
		AlphaKeybCtrlTestHooks::SetTask(ctrl, sentinel);
		check("mTask forced to sentinel", AlphaKeybCtrlTestHooks::Task(ctrl) == sentinel);

		typedef void (*VCallFn)(void *);
		void *vtbl = *(void **)(void *)&ctrl;
		VCallFn setupSlot = *(VCallFn *)((char *)vtbl + 8); /* CModuleManager::Setup()'s
		                                                      * own CallVSlot(module, 8) */
		setupSlot(&ctrl);
		check("raw vtbl+8 dispatch genuinely ran CAlphaKeybCtrl::Setup() (mTask no "
		      "longer the sentinel -- an EvaVTableStub no-op would have left it "
		      "unchanged)",
		      AlphaKeybCtrlTestHooks::Task(ctrl) != sentinel &&
		          AlphaKeybCtrlTestHooks::Task(ctrl) != 0);
	}

	printf("[9] CAlphaKeybCtrlTask::SetCtrlCondition() -- direct bit-logic unit test\n");
	{
		/* s_iStatusBits is a function-local static, so state carries across calls
		 * within this process -- test in a fixed, deliberate sequence rather than
		 * assuming independence between checks.
		 */
		check("down('X') from all-clear: was clear, sets bit 8 -> returns 1",
		      CAlphaKeybCtrlTask::SetCtrlCondition('X', true) == 1);
		/* real ground truth: up('X') READS bit 4 but CLEARS bit 8 -- transcribed
		 * literally, not "fixed" (same asymmetric-bit class as 'L'/';' below).
		 * down('X') only ever sets bit 8, so up('X')'s own bit-4 read is always
		 * 0 here -> returns 1, not 0.
		 */
		check("up('X') after down: reads bit 4 (still clear) -> returns 1, real "
		      "ground-truth bit-4-vs-bit-8 mismatch preserved",
		      CAlphaKeybCtrlTask::SetCtrlCondition('X', false) == 1);
		check("up('X') again: bit 4 still clear -> returns 1",
		      CAlphaKeybCtrlTask::SetCtrlCondition('X', false) == 1);

		check("down(';') from all-clear: returns 1", CAlphaKeybCtrlTask::SetCtrlCondition(';', true) == 1);
		check("down(';') again (bit 1 now set): returns 0",
		      CAlphaKeybCtrlTask::SetCtrlCondition(';', true) == 0);
		/* real ground truth: up(';') READS bit 2 but CLEARS bit 1 -- same
		 * asymmetric-bit class as 'X'/'L'. down(';') only ever sets bit 1, so
		 * up(';')'s own bit-2 read is always 0 here -> returns 1.
		 */
		check("up(';'): reads bit 2 (still clear) -> returns 1, real ground-truth "
		      "bit-1-vs-bit-2 mismatch preserved",
		      CAlphaKeybCtrlTask::SetCtrlCondition(';', false) == 1);

		check("down('L'): returns 1", CAlphaKeybCtrlTask::SetCtrlCondition('L', true) == 1);
		/* real ground truth: up('L') READS bit 8 but CLEARS bit 4 -- transcribed
		 * literally (see .cpp comment), not "fixed". down('L') set bit 4, so
		 * bit 8 is still clear -> up('L') returns 1 (bit8==0), and bit 4 gets
		 * cleared as a side effect.
		 */
		check("up('L'): reads bit 8 (still clear) -> returns 1, real ground-truth "
		      "bit-4-vs-bit-8 mismatch preserved",
		      CAlphaKeybCtrlTask::SetCtrlCondition('L', false) == 1);

		check("down('a') from all-clear: returns 1", CAlphaKeybCtrlTask::SetCtrlCondition('a', true) == 1);
		check("down('a') again (bit 2 now set): returns 0",
		      CAlphaKeybCtrlTask::SetCtrlCondition('a', true) == 0);
		check("up('a'): bit 0 clear -> returns 1 (0^1)",
		      CAlphaKeybCtrlTask::SetCtrlCondition('a', false) == 1);

		check("down(unrecognized keycode): returns 1",
		      CAlphaKeybCtrlTask::SetCtrlCondition(0x1e, true) == 1);
		check("up(unrecognized keycode): returns 1",
		      CAlphaKeybCtrlTask::SetCtrlCondition(0x1e, false) == 1);
	}

	printf("[10] CKeyboardLayout ctor -- constructs without crashing/asserting\n");
	{
		const SKeyboardLayoutDesc &def = kKeyboardLayoutDescs[0];
		unsigned char raw[sizeof(CKeyboardLayout)];
		CKeyboardLayout *layout =
			new (raw) CKeyboardLayout(def.type, def.table, def.name, def.flag);
		check("CKeyboardLayout constructed in place without asserting", layout != 0);
	}

	printf("=====================================================\n");
	if (g_fail == 0) {
		printf("ALL TESTS PASSED\n");
		return 0;
	}
	printf("%d CHECK(S) FAILED\n", g_fail);
	return 1;
}
