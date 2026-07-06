// SPDX-License-Identifier: GPL-2.0
/*
 * slot_voice_data_free.cpp  -  batch 17: CSTGSlotVoiceData::
 * FreeSlotVoiceData(bool) (`.text+0xb3fb0`, 394 bytes) plus its three
 * newly-discovered small dependencies: CSTGSmoother::
 * CancelAllSlotVoiceDataCCSmoothers(const CSTGSlotVoiceData*)
 * (`.text+0x2b790`, 83 bytes), CSTGSlotVoiceData::
 * AreAllKeysAndPedalsReleased() const (`.text+0xb3b50`, 33 bytes), and
 * CSTGPerformanceVars::NotifyAllKeysAndPedalsReleased() (`.text+0xbafc0`,
 * 279 bytes). See oa_global.h/oa_engine_init.h for the full confirmed
 * per-method shape; this file's own comments focus on implementation
 * detail.
 *
 * Deliberately its own dedicated TU, not global.cpp: `FreeSlotVoiceData(bool)`
 * already has THREE separate pre-existing mocks (test_engine.cpp/
 * test_global_ctor.cpp/test_global.cpp, the last one load-bearing across
 * many call-count assertions across several sections) -- giving the real
 * body its own file keeps all three untouched, matching the established
 * "dedicated TU" precedent (WriteSTGMidiOutQueue, SetControllerValue,
 * SetControllerAssignment, CSTGControllerRTData::SetControllerAssignment,
 * etc).
 *
 * Confirmed regparm(3) for FreeSlotVoiceData: this=eax, arg(bool)=edx.
 */

#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"	/* pulls oa_global.h + oa_engine.h; also gives STGAPIFrontPanelStatus (matching engine_startup_bits2.cpp's own established precedent for combining these three ecosystems) */

extern "C" void PushUnsolicitedMessage(void *msg);

static unsigned char *FromU32(unsigned int v)
{
	return (unsigned char *)(unsigned long)v;
}
static unsigned int ToU32(unsigned char *p)
{
	return (unsigned int)(unsigned long)p;
}

/*
 * CSTGSmoother::CancelAllSlotVoiceDataCCSmoothers(const CSTGSlotVoiceData*)
 * -- see oa_engine_init.h for the full confirmed shape.
 */
void CSTGSmoother::CancelAllSlotVoiceDataCCSmoothers(const CSTGSlotVoiceData *target)
{
	unsigned char *self = (unsigned char *)this;
	unsigned int targetId = (unsigned int)(unsigned long)target;

	unsigned int node = *(unsigned int *)(self + 0xf010);
	while (node != 0) {
		unsigned char *n = FromU32(node);
		unsigned int next = *(unsigned int *)(n + 0);
		unsigned char *mapping = FromU32(*(unsigned int *)(n + 8));

		if (*(unsigned int *)(mapping + 0x10) == 8 &&
		    *(unsigned int *)(mapping + 0xac) == targetId) {
			FinalizeSmoother(n, false);
		}
		node = next;
	}
}

/*
 * CSTGSlotVoiceData::AreAllKeysAndPedalsReleased() const -- see
 * oa_global.h for the full confirmed shape.
 */
bool CSTGSlotVoiceData::AreAllKeysAndPedalsReleased() const
{
	const unsigned char *self = (const unsigned char *)this;
	if (self[0x2888] != 0)
		return false;
	if (self[0x1790] > 0x4f)
		return false;
	return self[0x17a8] <= 0x3f;
}

/*
 * Shared unlink shape for FreeSlotVoiceData(bool)'s two embedded
 * intrusive doubly-linked lists (list #1: node fields +0x4/+0x8, owner/
 * anchor pointer at +0x10; list #2: node fields +0x14/+0x18, owner/
 * anchor at +0x20 -- confirmed via TWO independently-disassembled,
 * byte-identical-shaped blocks). Identity/head/tail comparisons use the
 * SAME "&node+link1Off as a token" convention already established for
 * CSTGHeapManager's sentinel (oa_heapmanager.h) -- NOT modeled here as
 * named "next"/"prev" fields, since the real disassembly's own head/tail
 * update direction doesn't obviously match either label; transcribed
 * with neutral `link1`/`link2` names instead of asserting an unverified
 * semantic. The real binary duplicates this exact code twice (no shared
 * subroutine call at either site) -- reconstructed here as one static
 * helper purely for readability, with identical net effect.
 */
static void UnlinkFromOwnerList(unsigned char *self, unsigned long link1Off,
				 unsigned long link2Off, unsigned long ownerOff)
{
	unsigned char *owner = FromU32(*(unsigned int *)(self + ownerOff));
	if (!owner)
		return;

	unsigned int identity = ToU32(self + link1Off);

	if (*(unsigned int *)(owner + 0) == identity)
		*(unsigned int *)(owner + 0) = *(unsigned int *)(self + link1Off);
	if (*(unsigned int *)(owner + 4) == identity)
		*(unsigned int *)(owner + 4) = *(unsigned int *)(self + link2Off);

	unsigned int b = *(unsigned int *)(self + link2Off);
	if (b)
		*(unsigned int *)FromU32(b) = *(unsigned int *)(self + link1Off);
	unsigned int a = *(unsigned int *)(self + link1Off);
	if (a)
		*(unsigned int *)(FromU32(a) + 4) = *(unsigned int *)(self + link2Off);

	*(unsigned int *)(self + link1Off) = 0;
	*(unsigned int *)(self + link2Off) = 0;
	*(unsigned int *)(self + ownerOff) = 0;
	(*(int *)(owner + 8))--;
}

/*
 * CSTGSlotVoiceData::FreeSlotVoiceData(bool) -- see oa_global.h for the
 * full confirmed shape.
 */
void CSTGSlotVoiceData::FreeSlotVoiceData(bool arg)
{
	unsigned char *self = (unsigned char *)this;

	CSTGSmoother::sInstance->CancelAllSlotVoiceDataCCSmoothers(this);

	/* +0x28c8: the SAME per-voice-data "group id"/CSTGPerformanceVars
	 * selector byte already confirmed for StealDyingSlotVoiceDatasForCost/
	 * FreeVoicelessDyingSlots (sec 10.94/10.110). Saved before the list
	 * unlinks below, matching the real disassembly's own save-early
	 * ordering (into a stack slot there, a local variable here). */
	unsigned char perfVarsSlot = self[0x28c8];

	/* Confirmed real: the EXACT same 3-field check as the standalone
	 * AreAllKeysAndPedalsReleased() const above -- inlined here with no
	 * `call` instruction at this specific site (matching this project's
	 * own "inlined helper is not the same as a call" finding, sec
	 * 10.163), so reproduced as an inline expression rather than a
	 * method call. */
	bool allKeysReleased;
	if (self[0x2888] != 0)
		allKeysReleased = false;
	else if (self[0x1790] > 0x4f)
		allKeysReleased = false;
	else
		allKeysReleased = self[0x17a8] <= 0x3f;

	UnlinkFromOwnerList(self, 0x4, 0x8, 0x10);
	UnlinkFromOwnerList(self, 0x14, 0x18, 0x20);

	CSTGGlobal::sInstance->FreeSlotVoiceData(this);

	if (arg) {
		/* +0x34: a packed pointer to a "program"-shaped sub-object,
		 * the SAME field CSTGGlobal::StealDyingSlotVoiceDatasForCost
		 * and CSTGSlotVoiceData::UpdateGlobalTune both already read
		 * via the identical +0xd "mode" sub-byte. */
		unsigned char *program = FromU32(*(unsigned int *)(self + 0x34));
		unsigned char subType = program[0xd];
		if ((subType == 1 || subType == 2) &&
		    *(unsigned int *)(self + 0x28c4) == 0) {
			CLoadBalancer::sInstance->BalanceStaticLoad();
		}
		/* Confirmed real: BOTH the "subType matched but locked" and
		 * "subType didn't match at all" sub-cases converge here
		 * unconditionally, same as the arg==false case below. */
	}

	if (!allKeysReleased) {
		unsigned char *slots = CSTGPerformanceVarsManager::sInstance;
		unsigned char *mgr = FromU32(*(unsigned int *)(slots + (unsigned int)perfVarsSlot * 4));
		((CSTGPerformanceVars *)mgr)->NotifyAllKeysAndPedalsReleased();
	}
}

/*
 * CSTGPerformanceVars::NotifyAllKeysAndPedalsReleased() -- see
 * oa_engine_init.h for the full confirmed shape, including the
 * confirmed-real-but-unreachable PushUnsolicitedMessage tail.
 */
void CSTGPerformanceVars::NotifyAllKeysAndPedalsReleased()
{
	unsigned char *self = (unsigned char *)this;

	if (self[0x23d1] != 3)
		return;

	unsigned char *global = (unsigned char *)CSTGGlobal::sInstance;
	unsigned char groupId = self[0x23d0];
	unsigned int node = *(unsigned int *)(global + 0x29c9900);
	while (node != 0) {
		unsigned char *n = FromU32(node);
		unsigned char *payload = FromU32(*(unsigned int *)(n + 0x8));
		node = *(unsigned int *)(n + 0x0);

		if (payload[0x28c8] == groupId) {
			if (!((CSTGSlotVoiceData *)payload)->AreAllKeysAndPedalsReleased())
				return;
		}
	}

	signed char oldState = (signed char)self[0x23d1];
	unsigned int savedField23e0 = *(unsigned int *)(self + 0x23e0);
	self[0x23d1] = 4;

	if (oldState <= 1) {
		unsigned char *slots = CSTGPerformanceVarsManager::sInstance;
		unsigned char *mgrSlot1 = FromU32(*(unsigned int *)(slots + 4));
		unsigned char *mgrSlot0 = FromU32(*(unsigned int *)(slots + 0));
		unsigned int count = (unsigned int)((signed char)mgrSlot1[0x23d1] > 1) +
				     (unsigned int)((signed char)mgrSlot0[0x23d1] > 1);
		*(unsigned int *)((unsigned char *)STGAPIFrontPanelStatus::sInstance + 0x1094) = count;

		/* Confirmed real but UNREACHABLE: self[0x23d1] was just
		 * forcibly set to 4 above, so this guard is always false in
		 * the compiled binary -- reproduced faithfully as dead code,
		 * the same "preserve real quirks" treatment already used for
		 * the identical shape in CSTGPerformanceVarsManager::
		 * AllocPerformanceVars() (see global.cpp). */
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

	*(unsigned int *)(self + 0x23f8) = savedField23e0;
}
