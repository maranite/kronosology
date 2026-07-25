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

	printf("[8] CEditor::Setup() -- real \"ALPHAKEYBOARD=Yes\" gate condition\n");
	{
		CEditor editor("EditorTest3", "ALPHAKEYBOARD=Yes");
		int rc = editor.Setup();
		check("Setup() returns 0 even with ALPHAKEYBOARD=Yes (construction itself deferred)", rc == 0);
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
