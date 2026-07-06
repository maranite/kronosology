// SPDX-License-Identifier: GPL-2.0
/*
 * load_balancer_static.cpp  -  batch 18: the "cost-accounting cluster"
 * first flagged (and rejected as a group) in sec 10.164 --
 * CLoadBalancer::BalanceStaticLoad() (`.text+0x61cb0`, 401 bytes),
 * CLoadBalancer::BalanceStaticLoadHelper(...) (`.text+0x61b80`, 297
 * bytes), and CSTGSlotVoiceData::EnableSlot() (`.text+0xb3b80`, 101
 * bytes). Its own fourth sibling, CSTGSlotVoiceData::
 * GetPatchStaticCosts(unsigned int, unsigned long*, unsigned long*)
 * const (`.text+0xb5650`, 170 bytes) is STILL genuinely blocked --
 * real vtable DISPATCH through a not-yet-reconstructed `CIFXEffectSlot`/
 * `CMFXEffectSlot` per-slot table (the same cluster blocking
 * `CSTGProgram::CSTGProgram()`, sec 10.157) -- left as a bare no-op
 * stub in bar2_stubs.cpp; calling into it from BalanceStaticLoadHelper
 * is safe, matching this project's established "calling a still-
 * deferred stub is fine" precedent. See oa_engine.h/oa_global.h for the
 * full per-method confirmed shape; this file's own comments focus on
 * implementation detail.
 *
 * Deliberately its own dedicated TU, not global.cpp/slot_voice_data_free.cpp:
 * `CLoadBalancer::BalanceStaticLoad()` already has FOUR separate
 * pre-existing mocks (test_engine.cpp/test_global_ctor.cpp trivial
 * no-ops; test_global.cpp/test_slot_voice_data_free.cpp load-bearing
 * call counters) -- giving the real body its own file keeps all four
 * untouched, matching the established "dedicated TU" precedent
 * (WriteSTGMidiOutQueue, SetControllerValue, SetControllerAssignment,
 * CSTGControllerRTData::SetControllerAssignment, etc). `EnableSlot()`/
 * `BalanceStaticLoadHelper()` have no pre-existing mocks anywhere and
 * could have lived elsewhere, but are kept alongside `BalanceStaticLoad()`
 * since all three are tightly coupled by design (one function's own
 * correctness depends on the other two's real bodies, not stubs).
 *
 * Confirmed regparm(3) throughout: CSTGSlotVoiceData::EnableSlot() takes
 * only `this` (eax). CLoadBalancer::BalanceStaticLoad() takes only
 * `this` (eax, never dereferenced -- see below). CLoadBalancer::
 * BalanceStaticLoadHelper(candidate, distArrayA, distArrayB, sumOut1,
 * sumOut2): this=eax (never dereferenced), candidate=edx,
 * distArrayA=ecx, distArrayB/sumOut1/sumOut2 on the stack (in that
 * order).
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"	/* pulls oa_global.h; also gives STGAPIFrontPanelStatus / CSTGAudioManager consistently, matching engine_startup_bits2.cpp's own established precedent for combining these ecosystems */

extern "C" void PushUnsolicitedMessage(void *msg);

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}

/*
 * CSTGSlotVoiceData::EnableSlot() -- confirmed: no-op unless
 * `fieldAt(0x28c4)` (dword) is nonzero. Otherwise: clears it to 0, sets
 * bit 0x80 on `fieldAt(0x34)`'s own sub-object `+0x45` byte, and sends a
 * 20-byte tagged PushUnsolicitedMessage (opcode 0x14, param1 =
 * `fieldAt(0x34)`'s own `+0x4` byte zero-extended, param2 = 1) -- the
 * same message shape already established in
 * OnExtModeKnobAssignChange/OnExtModeSliderAssignChange (sec 10.161).
 */
void CSTGSlotVoiceData::EnableSlot()
{
	unsigned char *self = (unsigned char *)this;

	if (*(unsigned int *)(self + 0x28c4) == 0)
		return;

	unsigned char *program = FromU32(*(unsigned int *)(self + 0x34));
	*(unsigned int *)(self + 0x28c4) = 0;
	program[0x45] |= 0x80;

	unsigned int patchByte = program[0x4];

	unsigned char msg[0x14];
	*(unsigned short *)(msg + 0x0) = 0x14;
	*(unsigned short *)(msg + 0x2) = 1;
	*(unsigned int *)(msg + 0x4) = 0;
	*(unsigned int *)(msg + 0x8) = 0x14;
	*(unsigned int *)(msg + 0xc) = patchByte;
	*(unsigned int *)(msg + 0x10) = 1;
	PushUnsolicitedMessage(msg);
}

/*
 * CLoadBalancer::BalanceStaticLoadHelper(...) -- a literal, instruction-
 * level transliteration of .text+0x61b80 (not a hand-simplified
 * re-derivation): the exact higher-level PURPOSE of the two-signal
 * (`distArrayA` vs `distArrayB`) split below is not independently
 * determined. For EACH of exactly two fixed "cost dimensions" (loop
 * runs for busIndex 0 then 1 -- NOT a loop over `CSTGAudioManager::
 * sInstance`'s own confirmed real `+0x18` field, which is used only as
 * the INNER scan's own bound, i.e. it gates how many entries of
 * distArrayA/distArrayB are considered, not how many times the outer
 * loop runs):
 *
 *   1. out1/out2 = candidate->GetPatchStaticCosts(busIndex).
 *   2. If busCount==0: bestIdx = scanIdx = 0, chosenPtr = &distArrayA[0],
 *      esiFinal = distArrayB (i.e. &distArrayB[0]).
 *   3. Else, scan indices 0..busCount-1: `bestIdx` tracks the argmin of
 *      distArrayA[] (ties favor the higher index, confirmed via
 *      `cmovbe`/`cmova` on a direct `distArrayA[i]` vs
 *      `distArrayA[bestIdx]` comparison). INDEPENDENTLY, `scanIdx`
 *      tracks whichever index the SAME scan happens to have settled on
 *      when distArrayB[]'s own value (re-read directly from
 *      distArrayB, NOT distArrayA) stops being STRICTLY LESS than the
 *      previous index's value (or busCount is reached) -- confirmed via
 *      a hand-traced host KAT (busCount 0/1/2) that `bestIdx` and
 *      `scanIdx` genuinely diverge whenever busCount > 1 and
 *      distArrayB happens to be monotonically decreasing all the way to
 *      its last element.
 *   4. Writes `candidate->fieldAt(0x28dc + busIndex) = (byte)bestIdx`
 *      and `candidate->fieldAt(0x28de + busIndex) = (byte)scanIdx`.
 *   5. Unconditionally: distArrayA[bestIdx] += out1; distArrayB[scanIdx]
 *      += out2; *sumOut1 += out1; *sumOut2 += out2.
 */
void CLoadBalancer::BalanceStaticLoadHelper(CSTGSlotVoiceData *candidate,
					     unsigned long *distArrayA,
					     unsigned long *distArrayB,
					     unsigned long *sumOut1,
					     unsigned long *sumOut2)
{
	unsigned char *slot = (unsigned char *)candidate;
	unsigned int busCount = *(unsigned int *)((char *)CSTGAudioManager::sInstance + 0x18);

	for (unsigned int busIndex = 0; busIndex < 2; busIndex++) {
		unsigned long out1 = 0, out2 = 0;
		candidate->GetPatchStaticCosts(busIndex, &out1, &out2);

		unsigned int bestIdx, scanIdx;
		unsigned long *chosenPtr, *esiFinal;

		if (busCount == 0) {
			chosenPtr = &distArrayA[0];
			esiFinal  = &distArrayB[0];
			bestIdx = 0;
			scanIdx = 0;
		} else {
			unsigned int eax = 0, ecx = 0;
			unsigned int v34 = 0;
			unsigned long v38 = distArrayB[0];
			unsigned long v1c = distArrayB[0];
			unsigned long *v3c = &distArrayA[0];
			unsigned long *scanEsi = distArrayB;

			for (;;) {
				unsigned long *bestPtr = &distArrayA[ecx];
				unsigned long *candPtr = &distArrayA[eax];
				v3c = candPtr;
				if (distArrayA[eax] > *bestPtr)
					v3c = bestPtr;
				if (distArrayA[eax] <= *bestPtr)
					ecx = eax;

				unsigned long edx = v1c;
				if (edx < v38) {
					/* "advance" (.text+0x61be8): the scan
					 * keeps going because distArrayB is
					 * still strictly decreasing. scanEsi
					 * uses the OLD v34 (the last index the
					 * scan actually settled at below, or 0
					 * if it never has yet). */
					scanEsi = distArrayB + v34;
					eax = eax + 1;
					if (eax >= busCount)
						break;
					v1c = distArrayB[eax];
					continue;
				}

				/* "settle" (.text+0x61c38): distArrayB stopped
				 * decreasing at this index -- record it. */
				v34 = eax;
				scanEsi = distArrayB + v34;
				eax = eax + 1;
				v38 = edx;
				if (eax < busCount) {
					v1c = distArrayB[eax];
					continue;
				}
				break;
			}

			bestIdx = ecx;
			scanIdx = v34;
			chosenPtr = v3c;
			esiFinal = scanEsi;
		}

		slot[0x28dc + busIndex] = (unsigned char)bestIdx;
		slot[0x28de + busIndex] = (unsigned char)scanIdx;

		*chosenPtr += out1;
		*esiFinal  += out2;
		*sumOut1   += out1;
		*sumOut2   += out2;
	}
}

/*
 * CLoadBalancer::BalanceStaticLoad() -- `this` is never dereferenced
 * except `fieldAt(0x8c)` (the budget threshold, second phase only).
 * Two phases:
 *
 *   Phase 1 -- walks CSTGGlobal::sInstance+0x29c9900 (the same list
 *   RunVoiceModelFeedback/FreeSlotVoiceData use). For each payload
 *   whose own +0x42 byte is clear, whose +0x28c4 mode is NEITHER 1 nor
 *   2, and whose +0x34-program sub-object's +0xd subType IS 1 or 2:
 *   calls BalanceStaticLoadHelper(payload, &busTotalsA, &busTotalsB,
 *   &committedA, &committedB) -- this phase only ACCUMULATES into the
 *   running totals, never enables anything.
 *
 *   Phase 2 -- walks the SAME 16-entry, 12-byte-stride
 *   CSTGGlobal::sInstance+0x29c990c active-voice-data-node table
 *   (UpdateAllActiveMIDIFilters/ResolveActiveVoiceDataNode, sec
 *   10.142/10.163). For each qualifying payload (not dying, mode
 *   EXACTLY 1, subType 1 or 2): computes payload->GetTotalStaticCosts(),
 *   and if committedA+committedB+candA+candB <= fieldAt(0x8c), calls
 *   payload->EnableSlot() then feeds this payload's own cost into the
 *   SAME running totals via another BalanceStaticLoadHelper() call (so
 *   later candidates in this same pass see the now-higher committed
 *   total) -- a genuine greedy, budget-limited slot-enabling loop.
 */
void CLoadBalancer::BalanceStaticLoad()
{
	unsigned int busCount = *(unsigned int *)((char *)CSTGAudioManager::sInstance + 0x18);

	/* busTotalsA/busTotalsB: per-bus running totals, sized generously
	 * (real busCount is confirmed small -- a handful of audio buses --
	 * this array size is not independently confirmed as an exact bound,
	 * only large enough for any plausible real busCount). */
	unsigned long busTotalsA[64];
	unsigned long busTotalsB[64];
	for (unsigned int i = 0; i < busCount && i < 64; i++) {
		busTotalsA[i] = 0;
		busTotalsB[i] = 0;
	}

	unsigned long committedA = 0, committedB = 0;

	unsigned char *node = FromU32(*(unsigned int *)((unsigned char *)CSTGGlobal::sInstance + 0x29c9900));
	while (node != 0) {
		unsigned char *payload = FromU32(*(unsigned int *)(node + 0x8));
		unsigned char *next = FromU32(*(unsigned int *)(node + 0x0));

		if (payload[0x42] == 0) {
			unsigned int mode = *(unsigned int *)(payload + 0x28c4);
			if (mode != 1 && mode != 2) {
				unsigned char *program = FromU32(*(unsigned int *)(payload + 0x34));
				unsigned char subType = program[0xd];
				if (subType == 1 || subType == 2) {
					this->BalanceStaticLoadHelper(
						(CSTGSlotVoiceData *)payload,
						busTotalsA, busTotalsB,
						&committedA, &committedB);
				}
			}
		}
		node = next;
	}

	/* fieldAt(0x8c) is a real 32-bit dword on the actual target -- must
	 * be read as unsigned int (4 bytes), not the host's 8-byte unsigned
	 * long, per this project's established pointer/field-width
	 * convention. */
	unsigned char *budgetSelf = (unsigned char *)this;
	unsigned int budget = *(unsigned int *)(budgetSelf + 0x8c);

	for (unsigned int i = 0; i < 16; i++) {
		unsigned char *tableBase = (unsigned char *)CSTGGlobal::sInstance;
		unsigned char *entry = FromU32(*(unsigned int *)(tableBase + 0x29c990c + i * 12));
		if (entry == 0)
			continue;

		unsigned char *payload = FromU32(*(unsigned int *)(entry + 0x8));
		if (payload[0x40] != 0)
			continue;
		if (*(unsigned int *)(payload + 0x28c4) != 1)
			continue;

		unsigned char *program = FromU32(*(unsigned int *)(payload + 0x34));
		unsigned char subType = program[0xd];
		if (subType != 1 && subType != 2)
			continue;

		unsigned long candA = 0, candB = 0;
		((CSTGSlotVoiceData *)payload)->GetTotalStaticCosts(&candA, &candB);

		unsigned long total = committedA + committedB + candA + candB;
		if (total > budget)
			continue;

		((CSTGSlotVoiceData *)payload)->EnableSlot();
		this->BalanceStaticLoadHelper(
			(CSTGSlotVoiceData *)payload,
			busTotalsA, busTotalsB,
			&committedA, &committedB);
	}
}
