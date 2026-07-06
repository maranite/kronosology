// SPDX-License-Identifier: GPL-2.0
/*
 * performance_vars_set_is_dying.cpp  -  batch 19: CSTGPerformanceVars::
 * SetIsDying() (`.text+0xbad40`, 478 bytes) plus its three newly-
 * discovered real dependencies: CSTGSlotVoiceData::SetIsDying()
 * (`.text+0xb3c50`, 15 bytes), CSTGMIDIClockSync::DisableActivePerfClock()
 * (`.text+0x675b0`, 11 bytes), and CSTGPerformance::SetIsDying(
 * CSTGPerformanceVars*) (`.text+0xb9a40`, 64 bytes). See oa_engine_init.h/
 * oa_global.h for the full confirmed per-method shape; this file's own
 * comments focus on implementation detail.
 *
 * Deliberately its own dedicated TU, not global.cpp: CSTGPerformanceVars::
 * SetIsDying() already has THREE separate pre-existing mocks
 * (test_engine.cpp/test_global_ctor.cpp/test_global.cpp, the last one a
 * load-bearing call-counter, sec 3474) -- giving the real body its own
 * file keeps all three untouched, matching the established "dedicated
 * TU" precedent (WriteSTGMidiOutQueue, SetControllerValue, FreeSlotVoiceData,
 * etc).
 *
 * Confirmed regparm(3) throughout: this=eax, first explicit arg=edx.
 */

#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"	/* pulls oa_global.h + oa_engine.h; also gives STGAPIFrontPanelStatus, matching slot_voice_data_free.cpp's own established precedent for combining these three ecosystems */

extern "C" void PushUnsolicitedMessage(void *msg);

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}

/*
 * CSTGSlotVoiceData::SetIsDying() -- see oa_global.h for the full
 * confirmed shape. Idempotent: only the FIRST call (while +0x40 == 0)
 * has any effect.
 */
void CSTGSlotVoiceData::SetIsDying()
{
	unsigned char *self = (unsigned char *)this;
	if (self[0x40] == 0) {
		self[0x40] = 1;
		self[0x41] = 0;
	}
}

/*
 * CSTGMIDIClockSync::DisableActivePerfClock() -- see oa_engine_init.h
 * for the full confirmed shape. Trivial unconditional field write.
 */
void CSTGMIDIClockSync::DisableActivePerfClock()
{
	unsigned char *self = (unsigned char *)this;
	*(int *)(self + 0xc8) = -1;
}

/*
 * CSTGPerformance::SetIsDying(CSTGPerformanceVars*) -- see
 * oa_engine_init.h for the full confirmed shape. The passed
 * CSTGPerformanceVars* is confirmed unused in the real body.
 */
void CSTGPerformance::SetIsDying(CSTGPerformanceVars *)
{
	unsigned char *self = (unsigned char *)this;

	CSTGFrontPanelSmoothers::sInstance->OnPerformanceDeactivate();
	((CSTGControllerInfo *)(self + 0xad3))->OnPerformanceDeactivate();
	((CSTGAudioInput *)(self + 0xae7))->OnPerformanceDeactivate();
	CSTGMessageProcessor::sInstance->ClearUnsolicitedMessages();
}

/*
 * Shared "update front-panel active-manager count, maybe
 * PushUnsolicitedMessage" block -- bit-for-bit the same computation
 * `CSTGPerformanceVarsManager::AllocPerformanceVars()` (global.cpp) and
 * `CSTGPerformanceVars::NotifyAllKeysAndPedalsReleased()`
 * (slot_voice_data_free.cpp) already use, duplicated at TWO call sites
 * in SetIsDying()'s own real disassembly (byte-for-byte identical
 * instruction sequences at .text+0xbaea1 and .text+0xbadf3) -- factored
 * into one helper here for readability, since both use sites are
 * provably identical with no ordering difference between them.
 *
 * CONFIRMED UNREACHABLE within SetIsDying() specifically: this helper's
 * own `oldState <= 1` guard re-reads `self[0x23d1]` immediately after
 * SetIsDying()'s own entry guard already established `self[0x23d1] == 2`
 * on entry, and nothing between that entry check and either call site
 * writes to `self[0x23d1]` -- so `oldState` is always exactly `2` here
 * and the guard is always false. The THIRD confirmed instance of the
 * "unconditional pre-write makes a later guard unreachable" quirk in
 * this cluster (see AllocPerformanceVars()/NotifyAllKeysAndPedalsReleased()
 * for the first two) -- preserved faithfully as dead code rather than
 * special-cased away, in case a not-yet-reconstructed callee (any of
 * the four OnPerformanceDeactivate()/ClearUnsolicitedMessages() externs
 * above) turns out to alias back into this object's own state.
 */
static void UpdatePerfVarsManagerActiveCountAndMaybeNotify(unsigned char *self)
{
	unsigned char *slots = CSTGPerformanceVarsManager::sInstance;
	unsigned char *mgrSlot1 = FromU32(*(unsigned int *)(slots + 4));
	unsigned char *mgrSlot0 = FromU32(*(unsigned int *)(slots + 0));
	unsigned int count = (unsigned int)((signed char)mgrSlot1[0x23d1] > 1) +
			     (unsigned int)((signed char)mgrSlot0[0x23d1] > 1);
	*(unsigned int *)((unsigned char *)STGAPIFrontPanelStatus::sInstance + 0x1094) = count;

	if ((signed char)self[0x23d1] <= 1) {
		unsigned char oldFlag = self[0x240c];
		unsigned char msg[0x10];
		*(unsigned short *)(msg + 0x0) = 0x10;
		*(unsigned short *)(msg + 0x2) = 1;
		*(unsigned int *)(msg + 0x4) = 0;
		*(unsigned int *)(msg + 0x8) = 0x20;
		*(unsigned int *)(msg + 0xc) = oldFlag;
		PushUnsolicitedMessage(msg);
		self[0x240c] = 0;
	}
}

/*
 * CSTGPerformanceVars::SetIsDying() -- see oa_engine_init.h for the
 * full confirmed shape.
 */
void CSTGPerformanceVars::SetIsDying()
{
	unsigned char *self = (unsigned char *)this;

	if (self[0x23d1] != 2)
		return;

	/* Notify the owning CSTGPerformance (fieldAt(0x23d4), a packed
	 * 32-bit pointer). The `this` argument passed to the callee is
	 * confirmed unused there. */
	CSTGPerformance *owner = (CSTGPerformance *)FromU32(*(unsigned int *)(self + 0x23d4));
	owner->SetIsDying(this);

	unsigned char *global = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char groupId = self[0x23d0];
	unsigned int node = *(unsigned int *)(global + 0x29c9900);

	/* AND-fold AreAllKeysAndPedalsReleased() over every matching
	 * payload, calling SetIsDying() on each one unconditionally (no
	 * early exit -- both calls always happen for every match,
	 * regardless of the accumulated result so far). Vacuously true if
	 * the list is empty or no payload matches the group-id filter. */
	bool allReleased = true;
	while (node != 0) {
		unsigned char *n = FromU32(node);
		unsigned char *payload = FromU32(*(unsigned int *)(n + 0x8));
		node = *(unsigned int *)(n + 0x0);

		if (payload[0x28c8] == groupId) {
			((CSTGSlotVoiceData *)payload)->SetIsDying();
			bool released = ((CSTGSlotVoiceData *)payload)->AreAllKeysAndPedalsReleased();
			allReleased = allReleased & released;
		}
	}

	signed char oldState = (signed char)self[0x23d1];
	if (allReleased) {
		/* All matching slot-voice-data finished releasing (or none
		 * qualified) -- commit +0x23d1 = 4 and also snapshot/restore
		 * +0x23e0 into +0x23f8 (NOT done on the state==3 branch
		 * below). */
		unsigned int savedField23e0 = *(unsigned int *)(self + 0x23e0);
		self[0x23d1] = 4;
		if (oldState <= 1)
			UpdatePerfVarsManagerActiveCountAndMaybeNotify(self);
		*(unsigned int *)(self + 0x23f8) = savedField23e0;
	} else {
		/* Still waiting on at least one matching payload -- commit
		 * +0x23d1 = 3, no +0x23f8 save. */
		self[0x23d1] = 3;
		if (oldState <= 1)
			UpdatePerfVarsManagerActiveCountAndMaybeNotify(self);
	}

	CSTGMIDIClockSync::sInstance->DisableActivePerfClock();
}
