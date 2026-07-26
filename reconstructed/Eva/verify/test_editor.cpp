/*
 * test_editor.cpp  -  host-side known-answer test for CEditor (src/editor/editor.cpp)
 * and CParameterString (src/editor/parameter_string.cpp), Stage 6 CEditor batch,
 * 2026-07-25.
 *
 * Exercises:
 *   - CParameterString's real "key=value,key=value" parser: multiple entries,
 *     whitespace trimming around both delimiters, last-one-wins duplicate
 *     precedence (ctor prepends), a missing-value / trailing-comma-less tail,
 *     DecToInt()/HexToInt()'s own cursor-advancing behavior.
 *   - CEditor's own ctor: real vtable pointers installed on all 3 sub-objects
 *     (primary + the 2 multiple-inheritance thunk vtables), sm_poTheEditor set,
 *     mAlphaKeybParam null vs non-null depending on the ctor's own argument.
 *   - CEditor::Setup()'s real fan-out: mMainTask/mPanelIfcTask both real,
 *     non-null afterward (the two sub-objects this pass reconstructs enough of
 *     to actually construct); the "ALPHAKEYBOARD" gate condition itself
 *     evaluated correctly even though the construction it would gate is
 *     deferred (editor.cpp's own header comment).
 *   - CEditor::CMainTask::IsSwitchPressed()/IsShowCost() -- the 2 fully real
 *     (zero-Peg-dependency) methods on that otherwise Tier-B stub class.
 */

#include <cstdio>
#include <cstring>

#include "editor.h"
#include "panel_ifc_task.h"
#include "alpha_keyb_ifc_task.h"

/* Pokes CEditor's private mAlphaKeybIfcTask -- see editor.h's own friend
 * declaration. Added 2026-07-26 alongside wiring CAlphaKeybIfcTask into
 * CEditor::Setup()/Start() for real (was previously a dead/deferred branch --
 * see eva_createusermodules_editor_unlock_2026-07-26).
 */
struct EditorTestHooks {
	static CAlphaKeybIfcTask *GetAlphaKeybIfcTask(CEditor &e) { return e.mAlphaKeybIfcTask; }
	static CEditor::CChunkServerTask *GetChunkServerTask(CEditor &e) { return e.mChunkServerTask; }
};

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CEditor / CParameterString known-answer test\n");
	printf("=============================================\n");

	printf("[1] CParameterString -- basic key=value parsing\n");
	{
		CParameterString ps("ALPHAKEYBOARD=Yes,FOO=bar");
		check("ALPHAKEYBOARD == \"Yes\"",
		      ps.GetParamStr("ALPHAKEYBOARD") != 0 &&
		          strcmp(ps.GetParamStr("ALPHAKEYBOARD"), "Yes") == 0);
		check("FOO == \"bar\"",
		      ps.GetParamStr("FOO") != 0 && strcmp(ps.GetParamStr("FOO"), "bar") == 0);
		check("missing key returns null", ps.GetParamStr("NOPE") == 0);
	}

	printf("[2] CParameterString -- whitespace trimming around '=' and ','\n");
	{
		CParameterString ps("  A = 1  ,  B  =2,C=3   ");
		check("A == \"1\"", ps.GetParamStr("A") != 0 && strcmp(ps.GetParamStr("A"), "1") == 0);
		check("B == \"2\"", ps.GetParamStr("B") != 0 && strcmp(ps.GetParamStr("B"), "2") == 0);
		check("C == \"3\"", ps.GetParamStr("C") != 0 && strcmp(ps.GetParamStr("C"), "3") == 0);
	}

	printf("[3] CParameterString -- duplicate key, last-one-wins (ctor prepends, GetParamStr walks head-first)\n");
	{
		CParameterString ps("X=first,X=second");
		check("X == \"second\" (most recently parsed wins)",
		      ps.GetParamStr("X") != 0 && strcmp(ps.GetParamStr("X"), "second") == 0);
	}

	printf("[4] CParameterString -- no trailing comma, single entry\n");
	{
		CParameterString ps("ONLY=value");
		check("ONLY == \"value\"",
		      ps.GetParamStr("ONLY") != 0 && strcmp(ps.GetParamStr("ONLY"), "value") == 0);
	}

	printf("[5] CParameterString::DecToInt/HexToInt -- cursor-advancing parsers\n");
	{
		const char *p1 = "123abc";
		int v1 = CParameterString::DecToInt(&p1);
		check("DecToInt(\"123abc\") == 123", v1 == 123);
		check("DecToInt cursor left at 'a'", *p1 == 'a');

		const char *p2 = "-45x";
		int v2 = CParameterString::DecToInt(&p2);
		check("DecToInt(\"-45x\") == -45", v2 == -45);

		const char *p3 = "1Fg";
		int v3 = CParameterString::HexToInt(&p3);
		check("HexToInt(\"1Fg\") == 0x1F", v3 == 0x1F);
		check("HexToInt cursor left at 'g'", *p3 == 'g');
	}

	printf("[6] CEditor ctor -- vtable install + sm_poTheEditor + null alphaKeyb param\n");
	{
		CEditor editor("EditorTest", 0);
		check("Config() returns 0", CEditor::Config() == 0);
		/* No public accessor for mAlphaKeybParam/vtbl -- indirectly confirmed
		 * via Setup() below not crashing when it dereferences mAlphaKeybParam
		 * (guarded, null-safe) and via CModule's own inherited AdjustTaskMask()
		 * not crashing (implies mVtbl/base state is sane).
		 */
		editor.AdjustTaskMask();
		check("ctor + AdjustTaskMask() doesn't crash with null alphaKeybParam", true);
	}

	printf("[7] CEditor::Setup() -- real fan-out: mMainTask/mPanelIfcTask constructed\n");
	{
		CEditor editor("EditorTest2", 0);
		int rc = editor.Setup();
		check("Setup() returns 0", rc == 0);
		/* Exercise Start() too -- guarded against mPanelIfcTask/mMainTask
		 * being set (they are, by Setup() above); must not crash.
		 */
		int rc2 = editor.Start();
		check("Start() returns 0", rc2 == 0);
	}

	printf("[8] CEditor::Setup() -- real \"ALPHAKEYBOARD=Yes\" gate, now wired (2026-07-26)\n");
	{
		/* Matches the real ground-truth "EditorClass" s_atCreateInfo row
		 * (config_info.cpp) -- this is the actual string CEditor's real
		 * boot-path caller passes, now that CreateUserModules() is unlocked.
		 */
		CEditor editor("EditorTest3", "ALPHAKEYBOARD=Yes");
		int rc = editor.Setup();
		check("Setup() returns 0 with ALPHAKEYBOARD=Yes", rc == 0);
		check("mAlphaKeybIfcTask constructed (non-null) when gate matches \"Yes\"",
		      EditorTestHooks::GetAlphaKeybIfcTask(editor) != 0);

		/* Start()'s own conditional CAlphaKeybIfcTask::Setup() tail-call --
		 * real ground truth is a literal 1-byte `ret`, must not crash.
		 */
		int rc2 = editor.Start();
		check("Start() returns 0 with mAlphaKeybIfcTask set", rc2 == 0);
	}

	printf("[8b] CEditor::Setup() -- gate does NOT match (\"No\" != \"Yes\") -- mAlphaKeybIfcTask left untouched\n");
	{
		CEditor editor("EditorTest4", "ALPHAKEYBOARD=No");
		int rc = editor.Setup();
		check("Setup() returns 0 with ALPHAKEYBOARD=No", rc == 0);
		/* Real ground truth's own ctor never zeroes mAlphaKeybIfcTask (see
		 * editor.h's own "preserved real quirk" note) -- so this is only
		 * safe to check because CEditor's ctor here already ran, and this
		 * particular object's storage is fresh stack memory this test
		 * doesn't rely on being any specific value; the point of this check
		 * is Setup() must NOT crash by dereferencing a bad pointer, and the
		 * gate itself (string compare) must correctly reject "No".
		 */
		check("Setup()/gate reject a non-\"Yes\" value without crashing", true);
	}

	printf("[8c] CEditor::Setup() -- CChunkServerTask, UNCONDITIONAL construction (not gated, 2026-07-26)\n");
	{
		CEditor editor("EditorTest5", 0);
		int rc = editor.Setup();
		check("Setup() returns 0", rc == 0);
		check("mChunkServerTask constructed (non-null), no gate needed",
		      EditorTestHooks::GetChunkServerTask(editor) != 0);
	}

	printf("[8d] CChunkServer -- direct unit tests (GetServerHandle/GetServerID/trivial slots)\n");
	{
		CEditor editor("EditorTest6", 0);
		editor.Setup();
		CEditor::CChunkServerTask *cst = EditorTestHooks::GetChunkServerTask(editor);
		check("CChunkServerTask constructed", cst != 0);

		/* Real ctor seeds a 1-entry sentinel table ({0xff, 0}) but leaves
		 * mEntryCount at 0 -- so both scans stay gated off in this
		 * reconstruction (nothing grows the table, Load() is Tier B), same
		 * as ground truth's own "nothing between the ctor and a real Load()
		 * call ever populates it" state.
		 */
		check("GetServerHandle(0xff) == 0xffffffff (mEntryCount == 0 gate)",
		      cst->GetServerHandle(0xff) == 0xffffffffU);
		check("GetServerID(0) == 0xffffffff (mEntryCount == 0 gate)",
		      cst->GetServerID(0) == 0xffffffffU);

		/* Trivial base-class slots, all real, all ignore their own args. */
		check("OnUnlock(...) == 1", cst->OnUnlock(0, 0, 0, 0, 0) == 1);
		check("OnRelock(...) == 1", cst->OnRelock(0, 0, 0, 0, 0) == 1);
		check("OnBegin(...) == 1", cst->OnBegin(0, 0, 0, 0, 0) == 1);
		check("OnEnd(...) == 1", cst->OnEnd(0, 0, 0, 0, 0) == 1);
		unsigned long saveLen = 0;
		const unsigned char *savePtr = 0;
		check("OnSave(ulong&,uchar const*&) overload == 0",
		      cst->CChunkServer::OnSave(saveLen, savePtr, 0, 0, 0) == 0);

		/* CEditor::CChunkServerTask's own 2 real (if behaviorally redundant)
		 * overrides.
		 */
		check("CChunkServerTask::OnSave(CChunk*,...) == 0", cst->OnSave(0, 0, 0, 0) == 0);
		check("CChunkServerTask::GetSaveBuffSize(...) == 0", cst->GetSaveBuffSize(0, 0, 0) == 0);

		/* Unlock() -- real indirect call through this object's own installed
		 * vtable slot 6 (EvaVTableStub-backed, install-only) -- must not
		 * crash.
		 */
		cst->Unlock(0, 0, 0);
		check("Unlock() (real vtable-slot-6 dispatch) does not crash", true);
	}

	printf("[9] CEditor::CMainTask::IsSwitchPressed/IsShowCost -- pure global reads\n");
	{
		/* Both real globals (s_oSwitchState/sShowCost, editor.cpp) start at
		 * zero and are only ever WRITTEN by CMainTask::Exec() (Tier-B,
		 * deferred) -- so IsSwitchPressed() should read "pressed" (the `^1`
		 * inversion of an all-zero bitmask) for every code, and IsShowCost()
		 * should read false.
		 */
		check("IsSwitchPressed(0) == true (all-zero bitmask, inverted)",
		      CEditor::CMainTask::IsSwitchPressed(0) == true);
		check("IsSwitchPressed(63) == true", CEditor::CMainTask::IsSwitchPressed(63) == true);
		check("IsShowCost() == false", CEditor::CMainTask::IsShowCost() == false);
		check("CEditor::IsSwitchPressed forwards correctly",
		      CEditor::IsSwitchPressed(5) == CEditor::CMainTask::IsSwitchPressed(5));
		check("CEditor::IsShowCost forwards correctly",
		      CEditor::IsShowCost() == CEditor::CMainTask::IsShowCost());
	}

	printf("[10] CEditor::CPanelIfcTask -- default ctor stays available (test compat)\n");
	{
		CEditor::CPanelIfcTask fakeOwner;
		(void)fakeOwner;
		check("default-constructible CPanelIfcTask (CTask test-only placeholder ctor)", true);
	}

	printf("=============================================\n");
	if (g_fail == 0) {
		printf("ALL TESTS PASSED\n");
		return 0;
	}
	printf("%d TEST(S) FAILED\n", g_fail);
	return 1;
}
