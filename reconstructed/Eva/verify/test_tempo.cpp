/*
 * test_tempo.cpp  -  host-side known-answer test for BPM/MPQN (src/base/tempo.cpp,
 * Stage 6 breadth sweep, 2026-07-25).
 *
 * Checks:
 *   - real cross-wiring: SetLowerLimit() writes MPQN::sm_UpperLimit,
 *     SetUpperLimit() writes MPQN::sm_LowerLimit (not the same-named field --
 *     a real, if surprising, ground-truth detail)
 *   - real formula: MPQN = 60000000 / bpm
 *   - real self-healing branch: setting a lower limit above the current upper
 *     limit forces the upper limit up to match (and vice versa), rather than
 *     rejecting the call
 *   - the real static initializer's own values (250000/1500000) are in effect
 *     before any Set call, matching BPM::_GLOBAL__I_sm_LowerLimit
 */

#include <cstdio>

#include "tempo.h"
#include "system_api.h"

/* Real global, defined in mains.cpp, linked into every verify binary since `make
 * verify` links all of $(OBJ) into each test -- same pattern as test_task.cpp. Not
 * exercised by this test directly (BPM's own assert-report path just needs *Api to
 * be dereferenceable to fetch its vtable; mains.cpp's own SysApiInstance-backed real
 * vtable is safely EvaVTableStub-filled).
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
	printf("BPM/MPQN known-answer test\n");
	printf("===========================\n");

	/* Real __attribute__((constructor)) already ran by main() -- confirm its
	 * own values are in effect (250000us/1500000us <-> 240/40 BPM).
	 */
	check("static ctor: MPQN::sm_LowerLimit == 250000", MPQN::sm_LowerLimit == 250000);
	check("static ctor: MPQN::sm_UpperLimit == 1500000", MPQN::sm_UpperLimit == 1500000);
	/* Real .data initial values (readelf byte read): {40, 0}. */
	check("real .data default: BPM::sm_LowerLimit == 40", BPM::sm_LowerLimit == 40);
	check("real .data default: BPM::sm_UpperLimit == 0", BPM::sm_UpperLimit == 0);

	/* SetLowerLimit(40): sm_LowerLimit(40) <= sm_UpperLimit(0) is FALSE -- takes
	 * the real self-healing/assert branch: sm_UpperLimit forced to 40, both MPQN
	 * fields set from 40 (60000000/40 = 1500000).
	 */
	BPM::SetLowerLimit(40);
	check("SetLowerLimit(40): sm_LowerLimit == 40", BPM::sm_LowerLimit == 40);
	check("SetLowerLimit(40) self-heal: sm_UpperLimit forced to 40", BPM::sm_UpperLimit == 40);
	check("SetLowerLimit(40) self-heal: MPQN::sm_LowerLimit == 1500000", MPQN::sm_LowerLimit == 1500000);
	check("SetLowerLimit(40) self-heal: MPQN::sm_UpperLimit == 1500000", MPQN::sm_UpperLimit == 1500000);

	/* SetUpperLimit(240): sm_LowerLimit(40) <= sm_UpperLimit(240) TRUE -- normal
	 * branch, writes MPQN::sm_LowerLimit = 60000000/240 = 250000.
	 */
	BPM::SetUpperLimit(240);
	check("SetUpperLimit(240): sm_UpperLimit == 240", BPM::sm_UpperLimit == 240);
	check("SetUpperLimit(240) normal branch: MPQN::sm_LowerLimit == 250000", MPQN::sm_LowerLimit == 250000);
	/* MPQN::sm_UpperLimit untouched by the normal SetUpperLimit branch. */
	check("SetUpperLimit(240) normal branch: MPQN::sm_UpperLimit unchanged (1500000)",
	      MPQN::sm_UpperLimit == 1500000);

	/* Cross-check: SetLowerLimit() with a value within range takes the normal
	 * branch and writes MPQN::sm_UpperLimit only.
	 */
	BPM::SetLowerLimit(60);
	check("SetLowerLimit(60) normal branch: sm_LowerLimit == 60", BPM::sm_LowerLimit == 60);
	check("SetLowerLimit(60) normal branch: MPQN::sm_UpperLimit == 60000000/60 == 1000000",
	      MPQN::sm_UpperLimit == 1000000);
	check("SetLowerLimit(60) normal branch: MPQN::sm_LowerLimit unchanged (250000)",
	      MPQN::sm_LowerLimit == 250000);

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
