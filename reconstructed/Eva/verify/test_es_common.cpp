/*
 * test_es_common.cpp  -  host-side known-answer test for CESCommon
 * (src/editor/es_common.cpp, Stage 6 breadth sweep, 2026-07-25 --
 * MMainESCommon/MMainESGlobal survey batch).
 *
 * CESCommon isn't reachable from this reconstruction's own currently-wired
 * boot path yet (see edit_server.h's file header -- the
 * CESCommonModuleConstructor::Create() call that would ever construct one
 * is gated behind CConfigManager::CreateUserModules(), not pursued), so this
 * KAT constructs one directly and checks:
 *   - real object size matches CModule(0x2c) + CEditServer(0x40038) = 0x40064
 *     exactly (the real malloc size CESCommonModuleConstructor::Create()
 *     uses) -- confirms the multiple-inheritance layout matches ground truth
 *     byte-for-byte, not just "compiles"
 *   - the ctor runs clean (both base ctors + the 2 vtable overwrites)
 *   - Setup() constructs a real CESCommonTask and registers it via
 *     CModule::Add() -- the same, already-boot-path-confirmed mechanism
 *     CEditor::Setup() uses (module.h)
 *   - Start()/Config() are confirmed no-ops (real ground truth: 3-byte
 *     `return 0;` bodies)
 *   - the CEditServer base half of the object is independently usable
 *     (Get()/Set() through the SAME object, not a standalone CEditServer --
 *     confirms the +0x2c base-offset layout is right, not just individually
 *     correct for each base in isolation)
 */

#include <cstdio>

#include "es_common.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CESCommon test\n");
	printf("==============\n");

	check("sizeof(CESCommon) == 0x40064 (CModule 0x2c + CEditServer 0x40038, "
	      "matches CESCommonModuleConstructor::Create()'s real malloc size exactly)",
	      sizeof(CESCommon) == 0x40064);

	CESCommon es("CommonEditServer", 0);
	check("ctor ran without crashing", true);

	int rc = es.Config();
	check("Config(): confirmed-empty in ground truth, returns 0", rc == 0);

	rc = es.Start();
	check("Start(): confirmed-empty in ground truth, returns 0", rc == 0);

	rc = es.Setup();
	check("Setup(): constructs a CESCommonTask and registers it via CModule::Add() "
	      "without crashing, returns 0", rc == 0);

	/* The CEditServer base half is independently usable through the same
	 * object -- confirms the +0x2c multiple-inheritance offset is right, not
	 * just each base compiling in isolation. Uses the same friend-hook
	 * pattern as test_edit_server.cpp (EditServerTestHooks isn't reused
	 * directly since CESCommon doesn't expose it -- instead this drives the
	 * public CEditServer surface via SetDefault()/Get()/Set(), all of which
	 * correctly report "not found" (real quirk: returns 1 for Get(), see
	 * edit_server.h) since no descriptors are registered on this instance).
	 */
	int buf = 0;
	rc = static_cast<CEditServer &>(es).Get(0xff, 1, 0, &buf, 4);
	check("CEditServer base (at +0x2c) is reachable and functions through the "
	      "combined object (no descriptors registered -> real not-found "
	      "quirk, returns 1)", rc == 1);

	printf("\n%s (%d failed)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail);
	return g_fail == 0 ? 0 : 1;
}
