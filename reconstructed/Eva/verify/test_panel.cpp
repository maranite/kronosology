/*
 * test_panel.cpp  -  host-side known-answer test for CPanel (src/ui/panel.cpp) and
 * the CPanelConstructor factory wiring (mains.cpp), Eva Stage 6 CPanel unlock batch,
 * 2026-07-26. See panel.h's own header comment for the full ground-truth writeup.
 *
 * Checks:
 *   [1] CPanel ctor: own real vtable installed (raw offset-0 read, matches
 *       PTR__CPanel_08f7c328); mParam parses "PANELDRV=PanelDriver" correctly
 *       (indirectly, via Setup() below -- CPanel has no public GetParamStr forward).
 *   [2] CPanel::Setup(): mPoller becomes non-null, is genuinely a CPoller (CTask's own
 *       vtable-installed identity, checked via a real CPoller-only accessor call), and
 *       is registered into CModule's own mTasks via CModule::Add() (count == 1, the
 *       stored pointer matches mPoller exactly).
 *   [3] CPanel::Config(): with mPoller set, real dispatch through
 *       CPoller::InitButtons()/InitAnalogs() (Tier-B stubs, but the dispatch itself
 *       must not crash); returns 0.
 *   [4] CPanel::Start(): real, unconditional `return 0`.
 *   [5] CPanelConstructor::Create() factory wiring (mains.cpp's
 *       PTR__CPanelConstructor_08f7c2f0[2]): calling the installed "Create" slot
 *       exactly the way CConfigManager::CreateUserModules() does (raw vtbl+8
 *       dispatch, (param1, param2, counter) args) yields a real, non-null CPanel*
 *       whose own vtable matches PTR__CPanel_08f7c328 -- the actual end-to-end path
 *       this batch exists to unlock.
 *   [6] CModuleManager's own real per-module dispatch shape (module_manager.cpp's
 *       `CallVSlot(module, 8/0xc/0x10)`): calling PTR__CPanel_08f7c328's Setup slot
 *       exactly that way on a fresh CPanel genuinely invokes CPanel::Setup() (not
 *       EvaVTableStub) -- confirms the "OPEN FINDING" fix documented in
 *       omega_vtables.h's own header comment on PTR__CPanel_08f7c328.
 */

#include <cstdio>
#include <cstring>

#include "panel.h"
#include "poller.h"
#include "module.h"
#include "omega_vtables.h"
#include "system_api.h"

struct PanelTestHooks {
	static CPoller *Poller(const CPanel &p) { return p.mPoller; }
	static void SetPoller(CPanel &p, CPoller *poller) { p.mPoller = poller; }
};

/* Same local re-declaration convention as test_task.cpp/test_small_modules.cpp/etc --
 * each verify test file keeps its own copy rather than sharing a header, matching the
 * project's established per-file idiom.
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

/* --- fake Api global, same shape/idiom as test_poller.cpp's own -------------- */

extern CSystemApi *Api;

/* Real global, defined in mains.cpp (extern "C" array of 3 void*: dtor/dtor/Create). */
extern "C" void *PTR__CPanelConstructor_08f7c2f0[3];

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
	/* Api+0xac ("named-resource lookup", poller.h) returns NULL -- CPoller's ctor
	 * takes its real, safe "name != NULL but lookup failed" fallback path (same
	 * scenario as test_poller.cpp's own check [2]), avoiding this test needing its
	 * own fake CPoller-resource object (already covered end-to-end by
	 * test_poller.cpp -- out of scope to re-test here).
	 */
	g_fakeApiVtbl[0xac / 4] = (void *)FakeLookupReturnsNull;
	g_fakeApiObj.vtbl = g_fakeApiVtbl;
	Api = (CSystemApi *)&g_fakeApiObj;
}

int main()
{
	printf("CPanel known-answer test\n");
	printf("=========================\n");
	setup_fake_api();

	printf("[1] CPanel ctor -- real vtable installed\n");
	{
		CPanel panel("Panel", "PANELDRV=PanelDriver");
		void *vtbl = *(void **)(void *)&panel; /* offset 0 = CModule::mVtbl */
		check("vtbl == PTR__CPanel_08f7c328", vtbl == (void *)PTR__CPanel_08f7c328);
	}

	printf("[2] CPanel::Setup() -- mPoller constructed + registered via CModule::Add()\n");
	{
		CPanel panel("Panel", "PANELDRV=PanelDriver");
		int rc = panel.Setup();
		check("Setup() returns 0", rc == 0);

		CPoller *poller = PanelTestHooks::Poller(panel);
		check("mPoller != 0", poller != 0);
		check("mPoller->IsValidHandle(0xffffffff) == false (real CPoller method, "
		      "confirms mPoller is a live, correctly-constructed CPoller)",
		      poller != 0 && poller->IsValidHandle(0xffffffff) == false);

		check("CModule::mTasks count == 1 (CModule::Add() really ran)",
		      ModuleTestHooks::TaskCount(panel) == 1);
		check("CModule::mTasks[0] == mPoller",
		      ModuleTestHooks::TaskAt(panel, 0) == (void *)poller);

		printf("[3] CPanel::Config() -- dispatches CPoller::InitButtons()/InitAnalogs()\n");
		{
			int rc2 = panel.Config();
			check("Config() returns 0", rc2 == 0);
			check("Config() did not crash dispatching through mPoller", true);
		}

		printf("[4] CPanel::Start() -- real, unconditional return 0\n");
		{
			check("Start() returns 0", panel.Start() == 0);
		}
	}

	printf("[5] CPanelConstructor::Create() factory wiring (mains.cpp) -- real end-to-end path\n");
	{
		typedef void *(*CreateFn)(void *, void *, void *, int);
		CreateFn create = (CreateFn)PTR__CPanelConstructor_08f7c2f0[2];
		void *obj = create(0, (void *)"Panel", (void *)"PANELDRV=PanelDriver", 0);
		check("Create() returns non-null", obj != 0);
		void *vtbl = *(void **)obj;
		check("created object's vtbl == PTR__CPanel_08f7c328 (genuinely a CPanel)",
		      vtbl == (void *)PTR__CPanel_08f7c328);
	}

	printf("[6] PTR__CPanel_08f7c328's own Setup slot -- real CModuleManager-shape raw dispatch\n");
	{
		CPanel panel("Panel", "PANELDRV=PanelDriver");
		/* mPoller is genuinely uninitialized garbage right after the real ctor
		 * (panel.h's own header comment -- preserved ground-truth quirk, not a
		 * bug), so force a known sentinel here rather than assuming it starts
		 * null, and confirm the real dispatch overwrites it.
		 */
		CPoller *sentinel = (CPoller *)0xdeadbeef;
		PanelTestHooks::SetPoller(panel, sentinel);
		check("mPoller forced to sentinel", PanelTestHooks::Poller(panel) == sentinel);

		typedef void (*VCallFn)(void *);
		void *vtbl = *(void **)(void *)&panel;
		VCallFn setupSlot = *(VCallFn *)((char *)vtbl + 8); /* CModuleManager::Setup()'s
		                                                      * own CallVSlot(module, 8) */
		setupSlot(&panel);
		check("raw vtbl+8 dispatch genuinely ran CPanel::Setup() (mPoller no longer "
		      "the sentinel -- an EvaVTableStub no-op would have left it unchanged)",
		      PanelTestHooks::Poller(panel) != sentinel &&
		          PanelTestHooks::Poller(panel) != 0);
	}

	printf("=========================\n");
	if (g_fail == 0) {
		printf("ALL TESTS PASSED\n");
		return 0;
	}
	printf("%d CHECK(S) FAILED\n", g_fail);
	return 1;
}
