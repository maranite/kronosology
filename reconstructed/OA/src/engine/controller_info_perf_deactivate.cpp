// SPDX-License-Identifier: GPL-2.0
/*
 * controller_info_perf_deactivate.cpp -- batch 36:
 * CSTGControllerRTData::ResetPerfSwitches() (`.text+0xd430`, 57 bytes) and
 * CSTGControllerInfo::OnPerformanceDeactivate() (`.text+0x92a90`, 106
 * bytes), the last of the four batch-19-flagged OnPerformanceDeactivate
 * siblings that wasn't blocked by a real vtable/callback dispatch inside
 * its OWN body (see bar2_stubs.cpp's own updated note -- the sibling
 * `CSTGFrontPanelSmoothers::OnPerformanceDeactivate()` remains deferred for
 * exactly that reason).
 *
 * ResetPerfSwitches() is a pure straight-line run of field zeroing on
 * `this` -- no branches, no calls -- confirmed via direct disassembly
 * (`.text+0xd430`..`0xd468`, ends in `ret` with no relocations of its own):
 *   +0x27/+0x2a/+0x29/+0x28/+0x15/+0x14/+0x1d/+0x1e/+0x1f  all zeroed (byte)
 *   +0x20                                                  set to 0x40 (byte)
 *   +0x18/+0x1a                                            set to 0x0200 (word each)
 *   +0x1c                                                  zeroed (byte)
 * Field order transcribed exactly as emitted (not sorted), matching this
 * project's own convention for straight-line zeroing sequences (e.g.
 * CCostProfile's own unrolled zero region, sec 10.60).
 *
 * OnPerformanceDeactivate()'s real control flow (confirmed instruction-by-
 * instruction against `.text+0x92a90`..`0x92ae7`, using
 * `CSTGControllerRTData::sInstance` throughout -- NOT the embedded
 * `CSTGGlobal+0x10` sub-object convention used elsewhere in this file):
 *
 *   CSTGControllerRTData *rt = CSTGControllerRTData::sInstance;
 *   if (rt->fieldAt(0x14) != 0) {
 *       this->SetPerfSwitch(0, false);
 *       rt = CSTGControllerRTData::sInstance;   // real compiler reload
 *   }
 *   if (rt->fieldAt(0x15) != 0)
 *       this->SetPerfSwitch(1, false);
 *   rt = CSTGControllerRTData::sInstance;        // real compiler reload
 *   rt->ResetPerfSwitches();
 *   rt = CSTGControllerRTData::sInstance;        // real compiler reload
 *   rt->fieldAt(0x21) &= 0xfc;
 *   rt->fieldAt(0x26) = 0;
 *   *(u16 *)rt->fieldAt(0x24) = 0;
 *   *(u16 *)rt->fieldAt(0x22) = 0;
 *
 * (The real disassembly reloads `sInstance` from the global after every
 * call that could clobber `eax` rather than caching it in a callee-saved
 * register -- transcribed faithfully as repeated reads of the same
 * pointer, not hoisted into one local, matching this project's own
 * "don't over-model compiler register reuse as a data dependency"
 * lesson from batch 6/sec 10.152.)
 *
 * `SetPerfSwitch(int, bool)`'s own first-arg literals (0 and 1) are
 * transcribed verbatim from the two call sites' `edx` values -- the real
 * `ePerfSwitch` enum's names aren't independently recovered, matching this
 * project's established "represent unrecovered enums as plain int"
 * convention (oa_global.h's own comment on this same struct). Calling
 * SetPerfSwitch() itself is safe: its real body (539 bytes, vtable
 * dispatch + jump table) is a SEPARATE, still-deliberately-deferred stub
 * in bar2_stubs.cpp -- a safe no-op stand-in, not a wild call. Fields
 * +0x14/+0x15 on CSTGControllerRTData aren't independently named
 * elsewhere; accessed via raw offset arithmetic per this project's
 * established convention for classes whose full layout isn't recovered.
 */

#include "oa_global.h"

void CSTGControllerRTData::ResetPerfSwitches()
{
	unsigned char *self = (unsigned char *)this;

	self[0x27] = 0;
	self[0x2a] = 0;
	self[0x29] = 0;
	self[0x28] = 0;
	self[0x15] = 0;
	self[0x14] = 0;
	self[0x1d] = 0;
	self[0x1e] = 0;
	self[0x1f] = 0;
	self[0x20] = 0x40;
	*(unsigned short *)(self + 0x18) = 0x0200;
	*(unsigned short *)(self + 0x1a) = 0x0200;
	self[0x1c] = 0;
}

void CSTGControllerInfo::OnPerformanceDeactivate()
{
	unsigned char *rt = (unsigned char *)CSTGControllerRTData::sInstance;

	if (rt[0x14] != 0) {
		SetPerfSwitch(0, false);
		rt = (unsigned char *)CSTGControllerRTData::sInstance;
	}
	if (rt[0x15] != 0)
		SetPerfSwitch(1, false);

	rt = (unsigned char *)CSTGControllerRTData::sInstance;
	((CSTGControllerRTData *)rt)->ResetPerfSwitches();

	rt = (unsigned char *)CSTGControllerRTData::sInstance;
	rt[0x21] &= 0xfc;
	rt[0x26] = 0;
	*(unsigned short *)(rt + 0x24) = 0;
	*(unsigned short *)(rt + 0x22) = 0;
}
