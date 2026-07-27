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
 *   [9] CBDApiInstance (bd_api_instance.h, 2026-07-27 batch, correcting the prior
 *       "RegisterLoader has zero call sites" false verdict): RegisterLoader()
 *       push_back semantics (append, growth across the 0x20-element boundary,
 *       NULL rejected with -1 and no append), IsBusy()/IsPreloadRunning() x2
 *       forwarding to the FIRST registered loader, and CBatchDiskManConstructor
 *       Create()'s own real wiring (mains.cpp) growing the SAME global
 *       BDApiInstance singleton [5] already exercises.
 */

#include <cstdio>
#include <cstring>

#include "batch_disk_man.h"
#include "batch_disk_main_task.h"
#include "bd_api_instance.h"
#include "omega_vtables.h"
#include "system_api.h"

struct BDApiInstanceTestHooks {
	static int Count(const CBDApiInstance &b)
	{
		return (int)(b.mEnd - b.mBegin);
	}
	static CBatchDiskMan *First(const CBDApiInstance &b) { return b.mBegin[0]; }
	static CBatchDiskMan *Last(const CBDApiInstance &b) { return b.mEnd[-1]; }
};

struct BatchDiskMainTaskTestHooks {
	static int State(const CBatchDiskMainTask &t) { return t.mState; }
};

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
			check("IsBusy() == mMainTask->IsBusy() (both false, real mState field)",
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

	printf("[8] CBatchDiskMainTask standalone -- real ctor (Eva \"size is not depth\"\n"
	       "    re-check batch, 2026-07-26): all 3 vtable groups, mState, no crash\n");
	{
		CBatchDiskMan owner("BatchDiskMan", 0);
		CBatchDiskMainTask task(owner, "EditResources.Unlocalized");

		void *vtbl0 = *(void **)(void *)&task;
		check("vtbl(+0) == PTR__CBatchDiskMainTask_08eabec8 (CTask group)",
		      vtbl0 == (void *)PTR__CBatchDiskMainTask_08eabec8);
		void *vtbl8 = *(void **)((char *)&task + 8);
		check("vtbl(+8) == PTR__CBatchDiskMainTask_08eabee8 (CTask's own mIfcThunk, "
		      "NOT a separate CEditable vtable -- corrects the prior unlock batch's claim)",
		      vtbl8 == (void *)PTR__CBatchDiskMainTask_08eabee8);
		void *vtbl80 = *(void **)((char *)&task + 0x80);
		check("vtbl(+0x80) == PTR__CBatchDiskMainTask_08eabefc (CRMApiCallBack group, "
		      "the genuine 3rd base)", vtbl80 == (void *)PTR__CBatchDiskMainTask_08eabefc);

		check("mState == 0 initially", BatchDiskMainTaskTestHooks::State(task) == 0);
		check("IsBusy() == false", task.IsBusy() == false);
		check("IsPreloadRunning() == false", task.IsPreloadRunning() == false);
		check("sizeof(CBatchDiskMainTask) == 0x160 (matches CBatchDiskMan::Setup()'s "
		      "own real malloc size)", sizeof(CBatchDiskMainTask) == 0x160);
		check("ctor/dtor did not crash (CTask+CEditable+CRMApiCallBack bases, heap "
		      "CRMJob, embedded CDirEntry/CZ, heap COutLinkMulti all constructed)", true);
	}

	printf("[9] CBDApiInstance -- RegisterLoader()/IsBusy()/IsPreloadRunning() (bd_api_instance.h)\n");
	{
		/* A fresh, LOCAL CBDApiInstance -- deliberately NOT the global BDApiInstance
		 * singleton (which [5]/[10] already populate via the real factory wiring, in
		 * an order this file doesn't want this check to depend on). Same real class,
		 * just an independent instance for isolated testing.
		 */
		CBDApiInstance local;

		check("RegisterLoader(NULL) returns -1", local.RegisterLoader(0) == -1);

		CBatchDiskMan first("BDApi-First", 0);
		first.Setup(); /* IsBusy()/IsPreloadRunning() forward through mMainTask --
		                 * must be non-NULL, same precondition CBatchDiskMan's own
		                 * ground truth requires. */
		int rc1 = local.RegisterLoader(&first);
		check("RegisterLoader() returns the new count (1st real append)", rc1 == 1);
		check("RegisterLoader(NULL) above did not append", rc1 == 1);

		check("IsBusy() forwards to the first (only) registered loader",
		      local.IsBusy() == first.IsBusy());
		check("IsPreloadRunning() forwards to the first (only) registered loader",
		      local.IsPreloadRunning() == first.IsPreloadRunning());
		check("IsPreloadRunning(group,name) forwards to the first (only) registered "
		      "loader", local.IsPreloadRunning(3, "grp") ==
		                    first.IsPreloadRunning(3, "grp"));

		/* Grow past the real 0x20-element initial capacity (bd_api_instance.h's own
		 * MakeCapacity() growth-policy writeup) to exercise the real doubling path,
		 * not just the fast "room available" append. Reusing &first 40 more times is
		 * fine -- this exercises RegisterLoader()'s/MakeCapacity()'s own pointer-
		 * vector growth mechanics, not per-element uniqueness.
		 */
		for (int i = 0; i < 40; i++)
			local.RegisterLoader(&first);
		check("40 more RegisterLoader() calls grew the count by exactly 40 "
		      "(real MakeCapacity() growth, crossing the 0x20-element boundary)",
		      BDApiInstanceTestHooks::Count(local) == 41);
		check("index 0 is UNCHANGED after growth (MakeCapacity() copied existing "
		      "elements into the new block, first-ever registration untouched)",
		      BDApiInstanceTestHooks::First(local) == &first);
		check("index 40 (the last append) also == &first",
		      BDApiInstanceTestHooks::Last(local) == &first);
	}

	printf("[10] CBatchDiskManConstructor::Create() wiring RegisterLoader() for "
	       "real (mains.cpp, corrects the prior omission)\n");
	{
		int countBefore = BDApiInstanceTestHooks::Count(BDApiInstance);

		typedef void *(*CreateFn)(void *, void *, void *, int);
		CreateFn create = (CreateFn)PTR__CBatchDiskManConstructor_08eabe08[2];
		void *obj = create(0, (void *)"BDApi-ViaFactory", 0, 0);
		check("Create() returns non-null", obj != 0);
		check("Create() also registered the new CBatchDiskMan with BDApiInstance "
		      "(count grew by 1 -- was silently omitted before this batch)",
		      BDApiInstanceTestHooks::Count(BDApiInstance) == countBefore + 1);
		check("the newly-appended loader IS the exact object Create() just returned",
		      (void *)BDApiInstanceTestHooks::Last(BDApiInstance) == obj);
	}

	printf("=========================\n");
	if (g_fail == 0) {
		printf("ALL TESTS PASSED\n");
		return 0;
	}
	printf("%d check(s) FAILED\n", g_fail);
	return 1;
}
