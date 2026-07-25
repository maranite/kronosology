/*
 * test_config_manager_boot_slice.cpp  -  host-side smoke test chaining
 * SetConfigInfo() (config_info.cpp) into the 5 CConfigManager methods upgraded to
 * Tier A this batch (config_manager.cpp, Stage 6 breadth sweep, 2026-07-25):
 * SetupRouting/MakeConnections/RegisterChunkServer/LinkRTRouterTracks/
 * ConfigureSeqTimer -- exactly the same order CKernel::InitUserLayer() (ckernel.cpp)
 * calls them in on the real boot path.
 *
 * This is the actual point of the batch: config_info.cpp's own SeqTimerInfo
 * placeholder had to be given sane non-zero BPM defaults (40/240) instead of the
 * all-zero convention used elsewhere, because ConfigureSeqTimer() unconditionally
 * dereferences one field as a pointer and unconditionally divides by two others
 * (BPM::SetLowerLimit()/SetUpperLimit(), tempo.cpp) -- a real divide-by-zero /
 * NULL-deref hazard this test exists specifically to catch a regression of. All 5
 * functions are real, boot-path-reachable code with today's placeholder table data
 * -- not dead code -- so "runs to completion without crashing" is a meaningful,
 * non-trivial check here, not a tautology.
 */

#include <cstdio>

#include "config_manager.h"
#include "tempo.h"
#include "system_api.h"

/* Real global, defined in mains.cpp, linked into every verify binary (make verify
 * links all of $(OBJ) into each test) -- same pattern as test_task.cpp/test_tempo.cpp.
 */
extern CSystemApi *Api;

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CConfigManager boot-slice smoke test\n");
	printf("=====================================\n");

	/* Real call, .text+0x0804cb70 -- populates all 13 sm_ptXxx fields, including
	 * the SeqTimerInfo placeholder this test cares about.
	 */
	SetConfigInfo();
	check("SetConfigInfo(): sm_ptSeqTimerInfo is non-null", CConfigManager::sm_ptSeqTimerInfo != 0);
	check("SetConfigInfo(): sm_ptConnectInfo is non-null", CConfigManager::sm_ptConnectInfo != 0);
	check("SetConfigInfo(): sm_ptChunkInfo is non-null", CConfigManager::sm_ptChunkInfo != 0);
	check("SetConfigInfo(): sm_ptRTRouterInfo is non-null", CConfigManager::sm_ptRTRouterInfo != 0);

	/* Real CKernel::InitUserLayer() call order (ckernel.cpp) for these 5 --
	 * exercised here without crashing is the actual test.
	 */
	CConfigManager::SetupRouting();
	check("SetupRouting() returned without crashing", true);

	CConfigManager::MakeConnections();
	check("MakeConnections() returned without crashing", true);

	CConfigManager::RegisterChunkServer();
	check("RegisterChunkServer() returned without crashing", true);

	CConfigManager::LinkRTRouterTracks();
	check("LinkRTRouterTracks() returned without crashing", true);

	CConfigManager::ConfigureSeqTimer();
	check("ConfigureSeqTimer() returned without crashing (the real divide-by-zero/"
	      "NULL-deref hazard this batch found and avoided)", true);

	/* ConfigureSeqTimer()'s own real tail: BPM::SetLowerLimit(40)/SetUpperLimit(240)
	 * from config_info.cpp's fixed placeholder -- confirms the real values actually
	 * flowed through the real call chain, not just "didn't crash".
	 */
	check("ConfigureSeqTimer() -> BPM::sm_LowerLimit == 40", BPM::sm_LowerLimit == 40);
	check("ConfigureSeqTimer() -> BPM::sm_UpperLimit == 240", BPM::sm_UpperLimit == 240);
	check("ConfigureSeqTimer() -> MPQN::sm_LowerLimit == 60000000/240 == 250000",
	      MPQN::sm_LowerLimit == 250000);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
