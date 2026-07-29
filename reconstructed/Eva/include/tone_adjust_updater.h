// SPDX-License-Identifier: GPL-2.0
/*
 * tone_adjust_updater.h  -  CToneAdjustUpdater: a stateless collection of
 * tone-adjust value-formatting/conversion helpers (real class has NO
 * per-instance data at all -- ctor/dtor are both real, literal no-ops,
 * every landed method below is `static`-shaped, cc=__cdecl, no `this`
 * touched anywhere).
 *
 * FOUND 2026-07-29 (round 48, solo). Fresh class -- no prior header
 * existed. 14/69 pending methods landed this round (63/69 total pending
 * are decompiler-clean; this round intentionally landed only the
 * smallest, fully self-contained subset -- the class as a whole ranges
 * up to an 11884-byte single method, `UpdateRelatedParamsForPCM`, far
 * outside a single round's scope).
 *
 * Landed methods split into 3 shapes:
 * (1) `CToneAdjustUpdater()`/`~CToneAdjustUpdater()` -- both real,
 *     literal 1-byte no-op bodies (ground truth: `return;`).
 * (2) `GetProgSwitchValueBuffer(int)`/`SetProgSwitchValueBuffer(int,
 *     short)` -- plain indexed get/set into a single real `short`
 *     array, `s_swProgSwitchValueBuffer` (real size not independently
 *     confirmed beyond what these 2 methods themselves prove -- opaque,
 *     content/size not read).
 * (3) `ConvertDKitNumToBank(int)`/`ConvertWSeqNumToBank(int)` -- pure
 *     arithmetic, no tables, no `this`.
 * (4) The `GetFormatterForXxx`/`GetAssignTypeForXxx` family -- each a
 *     bounds-checked index into its OWN distinct real `.rodata` lookup
 *     table (Ghidra's own `CSWTCH_165`/`168`/`171`/`174`/`177`/`184`/
 *     `187` placeholder names), out-of-range falls back to a fixed
 *     literal constant. Table CONTENTS are real but not independently
 *     confirmed (same "confirmed real, content unread" treatment
 *     already established project-wide for this class of opaque
 *     `.rodata` table, e.g. OA.ko's `STGAPIOutToPhysBusId` round 55) --
 *     each declared here with a safe, not-independently-confirmed size
 *     covering every real index this round's own callers use.
 * `IsAssignAvailable(EAlgorithm, int)` -- pure 2-arg boolean logic, no
 * table, no `this`.
 *
 * === Deferred (55/69 methods) ===
 * Explicitly NOT attempted this round: the remaining 55 methods range
 * from ~50 bytes up to 11884 bytes and cover genuinely large, separate
 * sub-areas (MOSS updater construction, control-surface value
 * round-tripping, min/max range calculation, assign-list management) --
 * each individually tractable but far outside a single round's
 * intended scope; left for dedicated future rounds. 6 are additionally
 * decompiler-flagged (`in_stack_`/`unaff_`): `UpdateSWOnValue`,
 * `ConvertParamToValue`, `ConvertValueToParam`, `UpdateFader`,
 * `UpdateKnob`, `UpdateAssignList`.
 */

#ifndef TONE_ADJUST_UPDATER_H
#define TONE_ADJUST_UPDATER_H

class CToneAdjustUpdater {
public:
	CToneAdjustUpdater();
	~CToneAdjustUpdater();

	static short GetProgSwitchValueBuffer(int index);
	static void SetProgSwitchValueBuffer(int index, short value);

	static int ConvertDKitNumToBank(int dkitNum);
	static int ConvertWSeqNumToBank(int wseqNum);

	static unsigned short GetFormatterForPCM(int paramIndex);
	static unsigned short GetFormatterForCommon(int paramIndex);
	static unsigned short GetFormatterForPCMStoredValueForProg(int paramIndex);
	static unsigned short GetFormatterForPCMStoredValueForTimbre(int paramIndex);
	static unsigned short GetFormatterForCommonStoredValue(int paramIndex);

	static unsigned char GetAssignTypeForPCM(unsigned char index);
	static unsigned char GetAssignTypeForCommon(unsigned char index);

	static bool IsAssignAvailable(int algorithm, int arg2);
};

#endif /* TONE_ADJUST_UPDATER_H */
