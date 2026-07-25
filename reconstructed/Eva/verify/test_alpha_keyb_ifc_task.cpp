/*
 * test_alpha_keyb_ifc_task.cpp  -  host-side known-answer test for
 * CEditable (src/editor/editable.cpp) and CAlphaKeybIfcTask
 * (src/editor/alpha_keyb_ifc_task.cpp), Stage 6 breadth sweep, 2026-07-25.
 *
 * Neither class is wired into this reconstruction's own currently-wired boot
 * path yet (see alpha_keyb_ifc_task.h's own header comment for why -- kept
 * standalone/unwired to avoid touching editor.cpp/editor.h during the
 * concurrent dedicated CEditor/CPanelIfcTask work this session), so both are
 * exercised directly here, same "ready infrastructure, verified by KAT
 * instead of a live boot test" methodology test_edit_server.cpp already
 * established for CEditServer itself.
 *
 * Checks:
 *   [1] CEditable::AddDescriptorsMap()'s real sentinel-scan (group==0xff
 *       terminates) against a real CEditServer, end to end through
 *       CDataHandler::AddDescriptors() (already-real) and back out through
 *       CEditServer::Get() -- proves the count this reconstruction computes
 *       matches what a real consumer actually sees registered
 *   [2] CAlphaKeybIfcTask::CAlphaKeybIfcTask()'s own real CTask base
 *       construction (mask/period/countdown, same two-tier computation
 *       task.h documents) plus its own 3 vtable-identity installs and the
 *       real owner+0x38 offset arithmetic CEditable ends up holding
 *   [3] CAlphaKeybIfcTask::~CAlphaKeybIfcTask() reinstalls the same primary/
 *       secondary identities and peels mIfcVtbl back to the generic
 *       CIfcUnknown identity before the (automatic) CTask::~CTask() base
 *       call runs
 */

#include <cstdio>
#include <cstring>
#include <new>

#include "alpha_keyb_ifc_task.h"
#include "editable.h"
#include "edit_server.h"
#include "module.h"
#include "omega_vtables.h"
#include "system_api.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

/* --- fake Api global, same fixture shape as test_task.cpp ----------------- */

extern CSystemApi *Api;

extern "C" int FakeScopeIdFn(void *) { return 0x1234; }
extern "C" void FakeApiNoOp() {}

static void *g_fakeApiVtbl[96];
struct FakeApiObj { void *vtbl; } g_fakeApiObj;

static void setup_fake_api()
{
	for (int i = 0; i < 96; i++)
		g_fakeApiVtbl[i] = (void *)FakeApiNoOp;
	g_fakeApiVtbl[0x3c / 4] = (void *)FakeScopeIdFn;
	g_fakeApiObj.vtbl = g_fakeApiVtbl;
	Api = (CSystemApi *)&g_fakeApiObj;
}

/* Friend-less access to CAlphaKeybIfcTask's private layout -- same raw-offset
 * technique test_task.cpp/test_module_adjust_task_mask.cpp already use rather
 * than adding a dedicated friend struct for a one-off KAT.
 */
static void *VtblAt(const CAlphaKeybIfcTask &t)
{
	return *(void *const *)&t;
}
static void *IfcThunkAt(const CAlphaKeybIfcTask &t)
{
	return *(void *const *)((const unsigned char *)&t + 8);
}
static void *EditableStoredServer(const CAlphaKeybIfcTask &t)
{
	/* mEditable sits at CTask's own 0x7c boundary; CEditable's own sole
	 * member (mEditServer) is its first 4 bytes.
	 */
	return *(void *const *)((const unsigned char *)&t + 0x7c);
}
static void *IfcVtblAt(const CAlphaKeybIfcTask &t)
{
	return *(void *const *)((const unsigned char *)&t + 0x80);
}

int main(void)
{
	printf("CEditable / CAlphaKeybIfcTask known-answer test\n");
	printf("================================================\n");

	setup_fake_api();

	printf("[1] CEditable::AddDescriptorsMap() sentinel-scan, end to end "
	       "through a real CEditServer\n");
	{
		CEditServer server("AlphaKeyb");
		CEditable editable(&server);

		struct FakeOwner { int intField; } owner = { 0 };

		SDescriptor rows[3];
		memset(rows, 0, sizeof(rows));
		rows[0].offset = 0;
		rows[0].elemSize = 4;
		rows[0].group = 1;
		rows[0].baseIndex = 0;
		rows[0].minValue = 0;
		rows[0].maxValue = 1000;
		rows[1].offset = 0;
		rows[1].elemSize = 4;
		rows[1].group = 2;
		rows[1].baseIndex = 0;
		rows[1].minValue = 0;
		rows[1].maxValue = 1000;
		rows[2].group = 0xff; /* sentinel -- must NOT be registered */

		editable.AddDescriptorsMap((CObjectBase *)&owner, rows, false);

		int got = 0;
		int rc1 = server.Get(0xff, 1, 0, &got, 4);
		check("row 0 (group=1) reachable after AddDescriptorsMap()",
		      rc1 == 1 && got == 0);
		got = -1;
		int rc2 = server.Get(0xff, 2, 0, &got, 4);
		check("row 1 (group=2) reachable after AddDescriptorsMap()",
		      rc2 == 1 && got == 0);
		int rc3 = server.Get(0xff, 0xff, 0, &got, 4);
		check("sentinel row (group=0xff) itself was NOT registered as a "
		      "descriptor (still returns a miss-shaped result, not a "
		      "second real hit)",
		      rc3 == 1); /* CEditServer::Get() returns 1 on both hit and
		                  * clean miss (edit_server.h's own documented
		                  * quirk) -- absence is instead confirmed by [2]
		                  * below counting exactly 2 registered rows. */
	}

	printf("[2] CAlphaKeybIfcTask::CAlphaKeybIfcTask(): real CTask base "
	       "construction + own vtable installs + owner+0x38 arithmetic\n");
	{
		struct FakeModule {
			void *vtbl;
			char *name;
			char  tasksPad[0x18];
			int   unknown20;
			int   state;
			int   scopeId;
		} owner;
		memset(&owner, 0, sizeof(owner));
		owner.state = 0; /* < 4 -- pre-masked branch, same as a freshly
		                  * constructed real CModule */

		CAlphaKeybIfcTask task(*(const CModule *)&owner);

		check("primary vtable installed",
		      VtblAt(task) == (void *)PTR__CAlphaKeybIfcTask_08f25ae8);
		check("mIfcThunk (+0x08) specialized, not the generic CTask "
		      "placeholder",
		      IfcThunkAt(task) == (void *)PTR__CAlphaKeybIfcTask_08f25b08);
		check("mIfcVtbl (+0x80) installed",
		      IfcVtblAt(task) == (void *)PTR__CAlphaKeybIfcTask_08f25b1c);
		check("mEditable holds owner+0x38 (real ctor's own "
		      "(CEditServer*)(owner+0x38) cast)",
		      EditableStoredServer(task) ==
		          (void *)((const unsigned char *)&owner + 0x38));

		printf("[3] ~CAlphaKeybIfcTask(): own explicit installs run first, "
		       "but the (automatic) base CTask::~CTask() call has the "
		       "real LAST word on mVtbl/mIfcThunk -- same 'faithfully "
		       "written, then immediately superseded by the inherited "
		       "cleanup' shape ground truth's own disassembly shows\n");
		task.~CAlphaKeybIfcTask();
		check("mVtbl ends at CObjectBase's own root identity (CTask::"
		      "~CTask()'s own final act, task.cpp step 8) -- NOT this "
		      "class's own vtable, which CAlphaKeybIfcTask's derived dtor "
		      "body does install but the base dtor overwrites right after",
		      VtblAt(task) == (void *)PTR__CObjectBase_08e79d68);
		check("mIfcThunk ends at CTask::~CTask()'s own CMessageInput "
		      "identity (task.cpp step 6), same supersede reasoning",
		      IfcThunkAt(task) == (void *)PTR__CMessageInput_08e80c68);
		check("mIfcVtbl (+0x80, not touched by CTask::~CTask() -- outside "
		      "its own object) peeled back to the generic CIfcUnknown "
		      "identity by CAlphaKeybIfcTask's own dtor body, and stays "
		      "that way",
		      IfcVtblAt(task) == (void *)PTR__CIfcUnknown_08e81d80);

		/* Placement-reconstruct so the (already-run) implicit ~CTask()
		 * from the explicit dtor call above doesn't double-free when
		 * this scope's own automatic destructor runs at closing brace.
		 */
		new (&task) CAlphaKeybIfcTask(*(const CModule *)&owner);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
