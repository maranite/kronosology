/*
 * test_batch_disk_man.cpp  -  host-side known-answer test for CBatchDiskMan
 * (src/editor/batch_disk_man.cpp), CEditTask (src/editor/edit_task.cpp), and the
 * CBatchDiskManConstructor factory wiring (mains.cpp), Eva Stage 6 CBatchDiskMan
 * unlock batch, 2026-07-26. See batch_disk_man.h/edit_task.h/batch_disk_main_task.h's
 * own header comments for the full ground-truth writeup.
 *
 * Checks:
 *   [1] CBatchDiskMan ctor: own real vtable installed (both groups, raw offset-0/
 *       offset-0x2c reads, matches PTR__CBatchDiskMan_08eac048/_08eac06c); mParam
 *       constructed only when param2 != 0.
 *   [2] CBatchDiskMan::Setup(): mMainTask/mEditTask both become non-null, both
 *       registered into CModule's own mTasks via CModule::Add() (count == 2), mParam
 *       consumed+freed (nulled) afterward.
 *   [3] CBatchDiskMan::IsBusy()/IsPreloadRunning() forward to mMainTask's own
 *       same-named methods.
 *   [4] CBatchDiskMan::Config()/Start(): real dispatch (Config() through the fake
 *       Api+0x44 slot) does not crash; both return 0.
 *   [5] CBatchDiskManConstructor::Create() factory wiring (mains.cpp's
 *       PTR__CBatchDiskManConstructor_08eabe08[2]): calling the installed slot the
 *       way CConfigManager::CreateUserModules() does yields a real, non-null
 *       CBatchDiskMan* with the correct vtable.
 *   [6] PTR__CBatchDiskMan_08eac048's own Setup slot -- real CModuleManager-shape raw
 *       vtbl+8 dispatch genuinely runs CBatchDiskMan::Setup() (not EvaVTableStub).
 *   [7] CEditTask, constructed standalone: real vtable installed, DoPreload()/
 *       GetOutLinkName() don't crash and return the "Internal" outlink name.
 */

#include <cstdio>
#include <cstring>

#include "batch_disk_man.h"
#include "omega_vtables.h"
#include "system_api.h"

struct BatchDiskManTestHooks {
	static CBatchDiskMainTask *MainTask(const CBatchDiskMan &m) { return m.mMainTask; }
	static CParameterString *Param(const CBatchDiskMan &m) { return m.mParam; }
	static CEditTask *EditTask(const CBatchDiskMan &m) { return m.mEditTask; }
	static void SetMainTask(CBatchDiskMan &m, CBatchDiskMainTask *t) { m.mMainTask = t; }
};

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

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* --- fake Api global, same shape/idiom as test_panel.cpp's own -------------- */

extern CSystemApi *Api;

extern "C" void *PTR__CBatchDiskManConstructor_08eabe08[3];

extern "C" void FakeApiNoOp() {}

static void *g_fakeApiVtbl[96];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void setup_fake_api()
{
	for (int i = 0; i < 96; i++)
		g_fakeApiVtbl[i] = (void *)FakeApiNoOp;
	/* Api+0x44 -- CBatchDiskMan::Config()'s own registration slot (batch_disk_man.h).
	 * A no-arg no-op is safe to call cdecl-style with extra unread args (same
	 * "caller cleans the stack, callee ignoring extra pushed args is harmless"
	 * reasoning already used throughout this project's own fake-Api test setups). */
	g_fakeApiObj.vtbl = g_fakeApiVtbl;
	Api = (CSystemApi *)&g_fakeApiObj;
}

int main()
{
	printf("CBatchDiskMan known-answer test\n");
	printf("=========================\n");
	setup_fake_api();

	printf("[1] CBatchDiskMan ctor -- real vtable installed, mParam gated on param2\n");
	{
		CBatchDiskMan withParam("BatchDiskMan",
			"PRELOAD=EditResources.Unlocalized;EditResources.Localized.ENG");
		void *vtbl0 = *(void **)(void *)&withParam;
		check("vtbl(+0) == PTR__CBatchDiskMan_08eac048",
		      vtbl0 == (void *)PTR__CBatchDiskMan_08eac048);
		void *vtbl2c = *(void **)((char *)&withParam + 0x2c);
		check("vtbl(+0x2c) == PTR__CBatchDiskMan_08eac06c",
		      vtbl2c == (void *)PTR__CBatchDiskMan_08eac06c);
		check("mParam != 0 (param2 was non-null)",
		      BatchDiskManTestHooks::Param(withParam) != 0);

		CBatchDiskMan noParam("BatchDiskMan", 0);
		check("mParam == 0 (param2 was null)",
		      BatchDiskManTestHooks::Param(noParam) == 0);
	}

	printf("[2] CBatchDiskMan::Setup() -- mMainTask/mEditTask constructed + registered\n");
	{
		CBatchDiskMan man("BatchDiskMan",
			"PRELOAD=EditResources.Unlocalized;EditResources.Localized.ENG");
		int rc = man.Setup();
		check("Setup() returns 0", rc == 0);

		CBatchDiskMainTask *mainTask = BatchDiskManTestHooks::MainTask(man);
		CEditTask *editTask = BatchDiskManTestHooks::EditTask(man);
		check("mMainTask != 0", mainTask != 0);
		check("mEditTask != 0", editTask != 0);
		check("mParam nulled after Setup() consumes it",
		      BatchDiskManTestHooks::Param(man) == 0);

		check("CModule::mTasks count == 2 (CModule::Add() ran twice)",
		      ModuleTestHooks::TaskCount(man) == 2);
		check("CModule::mTasks[0] == mMainTask",
		      ModuleTestHooks::TaskAt(man, 0) == (void *)mainTask);
		check("CModule::mTasks[1] == mEditTask",
		      ModuleTestHooks::TaskAt(man, 1) == (void *)editTask);

		printf("[3] CBatchDiskMan::IsBusy()/IsPreloadRunning() forward to mMainTask\n");
		{
			check("IsBusy() == mMainTask->IsBusy() (both false, Tier-B substitute)",
			      man.IsBusy() == mainTask->IsBusy());
			check("IsPreloadRunning() == mMainTask->IsPreloadRunning()",
			      man.IsPreloadRunning() == mainTask->IsPreloadRunning());
		}

		printf("[4] CBatchDiskMan::Config()/Start()\n");
		{
			int rc2 = man.Config();
			check("Config() returns 0", rc2 == 0);
			check("Config() did not crash dispatching through the fake Api", true);
			check("Start() returns 0", man.Start() == 0);
		}
	}

	printf("[5] CBatchDiskManConstructor::Create() factory wiring (mains.cpp)\n");
	{
		typedef void *(*CreateFn)(void *, void *, void *, int);
		CreateFn create = (CreateFn)PTR__CBatchDiskManConstructor_08eabe08[2];
		void *obj = create(0, (void *)"BatchDiskMan",
			(void *)"PRELOAD=EditResources.Unlocalized;EditResources.Localized.ENG", 0);
		check("Create() returns non-null", obj != 0);
		void *vtbl = *(void **)obj;
		check("created object's vtbl == PTR__CBatchDiskMan_08eac048 (genuinely a "
		      "CBatchDiskMan)", vtbl == (void *)PTR__CBatchDiskMan_08eac048);
	}

	printf("[6] PTR__CBatchDiskMan_08eac048's own Setup slot -- CModuleManager-shape raw dispatch\n");
	{
		CBatchDiskMan man("BatchDiskMan", 0);
		CBatchDiskMainTask *sentinel = (CBatchDiskMainTask *)0xdeadbeef;
		BatchDiskManTestHooks::SetMainTask(man, sentinel);
		check("mMainTask forced to sentinel", BatchDiskManTestHooks::MainTask(man) == sentinel);

		typedef void (*VCallFn)(void *);
		void *vtbl = *(void **)(void *)&man;
		VCallFn setupSlot = *(VCallFn *)((char *)vtbl + 8); /* CModuleManager::Setup()'s
		                                                      * own CallVSlot(module, 8) */
		setupSlot(&man);
		check("raw vtbl+8 dispatch genuinely ran CBatchDiskMan::Setup() (mMainTask no "
		      "longer the sentinel -- an EvaVTableStub no-op would have left it unchanged)",
		      BatchDiskManTestHooks::MainTask(man) != sentinel &&
		          BatchDiskManTestHooks::MainTask(man) != 0);
	}

	printf("[7] CEditTask standalone -- real vtable + DoPreload()/GetOutLinkName()\n");
	{
		CBatchDiskMan owner("BatchDiskMan", 0);
		CEditTask task(owner);
		void *vtbl = *(void **)(void *)&task;
		check("vtbl(+0) == PTR__CEditTask_08eac1c8",
		      vtbl == (void *)PTR__CEditTask_08eac1c8);
		task.DoPreload();
		check("DoPreload() did not crash", true);
		const char *name = task.GetOutLinkName();
		check("GetOutLinkName() == \"Internal\"", name != 0 && strcmp(name, "Internal") == 0);
	}

	printf("=========================\n");
	if (g_fail == 0) {
		printf("ALL TESTS PASSED\n");
		return 0;
	}
	printf("%d check(s) FAILED\n", g_fail);
	return 1;
}
