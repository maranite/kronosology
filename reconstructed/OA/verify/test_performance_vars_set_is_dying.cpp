// SPDX-License-Identifier: GPL-2.0
/*
 * test_performance_vars_set_is_dying.cpp  -  KAT for batch 19's
 * src/engine/performance_vars_set_is_dying.cpp: CSTGPerformanceVars::
 * SetIsDying(), CSTGSlotVoiceData::SetIsDying(), CSTGMIDIClockSync::
 * DisableActivePerfClock(), and CSTGPerformance::SetIsDying(
 * CSTGPerformanceVars*).
 *
 * Mocks CSTGSlotVoiceData::AreAllKeysAndPedalsReleased() const (real
 * elsewhere, in slot_voice_data_free.cpp, NOT linked into this test) as
 * a controllable-per-object stub, and the four confirmed-real,
 * deliberately-deferred externs CSTGPerformance::SetIsDying() calls
 * (CSTGFrontPanelSmoothers::OnPerformanceDeactivate/CSTGControllerInfo::
 * OnPerformanceDeactivate/CSTGAudioInput::OnPerformanceDeactivate/
 * CSTGMessageProcessor::ClearUnsolicitedMessages) as simple call
 * recorders, keeping this KAT focused on the four newly-real functions'
 * own logic.
 *
 * Every object whose own address is round-tripped through a packed
 * 32-bit field (list nodes, slot-voice-data payloads) is mmap32()/
 * MAP_32BIT-backed. CSTGGlobal::sInstance's own backing buffer uses the
 * project's established plain `calloc(1, 0x29c9fc0)` convention instead
 * (its OWN address is never itself stored into a packed 32-bit field
 * anywhere in this call chain, only dereferenced directly through the
 * native `CSTGGlobal::sInstance` pointer) -- same reasoning already
 * used in test_slot_voice_data_free.cpp/test_load_balancer_static.cpp.
 */

#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"

#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>

static int g_fail;
static void check_eq(const char *what, long got, long want)
{
	if (got == want) {
		printf("  ok    %-60s %ld\n", what, got);
	} else {
		printf("  FAIL  %-60s got=%ld want=%ld\n", what, got, want);
		g_fail = 1;
	}
}

static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return (unsigned char *)p;
}

static void local_zero(void *p, unsigned long n)
{
	unsigned char *b = (unsigned char *)p;
	for (unsigned long i = 0; i < n; i++)
		b[i] = 0;
}

static unsigned int addr32(void *p) { return (unsigned int)(unsigned long)p; }

/* ---- local storage for every static this TU's real code references ---- */
CSTGGlobal *CSTGGlobal::sInstance;
unsigned char CSTGPerformanceVarsManager::sInstance[12];
unsigned char *STGAPIFrontPanelStatus::sInstance;
CSTGFrontPanelSmoothers *CSTGFrontPanelSmoothers::sInstance;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance;
CSTGMIDIClockSync *CSTGMIDIClockSync::sInstance;

/* ---- mocks ---- */

/* PushUnsolicitedMessage() is only reachable from the alloc-count
 * helper's own confirmed-UNREACHABLE inner guard (see
 * performance_vars_set_is_dying.cpp) -- defined here purely to satisfy
 * the linker, with its own call counter asserted to stay zero. */
static int g_pushMsgCalls;
extern "C" void PushUnsolicitedMessage(void *) { g_pushMsgCalls++; }

static int g_onPerfDeactivateFPS, g_onPerfDeactivateCI, g_onPerfDeactivateAI;
static int g_clearUnsolicited;
static void *g_lastCIThis, *g_lastAIThis;

void CSTGFrontPanelSmoothers::OnPerformanceDeactivate() { g_onPerfDeactivateFPS++; }
void CSTGControllerInfo::OnPerformanceDeactivate() { g_onPerfDeactivateCI++; g_lastCIThis = this; }
void CSTGAudioInput::OnPerformanceDeactivate() { g_onPerfDeactivateAI++; g_lastAIThis = this; }
void CSTGMessageProcessor::ClearUnsolicitedMessages() { g_clearUnsolicited++; }

/* Controllable per-object mock: a scratch byte at +0x2000 (well clear of
 * every confirmed-real CSTGSlotVoiceData field this project has
 * documented so far, matching test_load_balancer_static.cpp's own
 * scratch-offset convention) selects the return value. */
static int g_areAllKeysCalls;
bool CSTGSlotVoiceData::AreAllKeysAndPedalsReleased() const
{
	g_areAllKeysCalls++;
	return ((const unsigned char *)this)[0x2000] != 0;
}

int main(void)
{
	printf("[1] CSTGSlotVoiceData::SetIsDying() -- idempotent transition\n");
	{
		unsigned char *buf = mmap32(0x100);
		local_zero(buf, 0x100);
		CSTGSlotVoiceData *v = (CSTGSlotVoiceData *)buf;

		buf[0x40] = 0;
		buf[0x41] = 0xcc;	/* poison -- must be cleared on first call */
		v->SetIsDying();
		check_eq("first call: +0x40 set to 1", buf[0x40], 1);
		check_eq("first call: +0x41 cleared to 0", buf[0x41], 0);

		buf[0x41] = 0x55;	/* re-poke -- second call must be a no-op */
		v->SetIsDying();
		check_eq("second call: +0x40 still 1", buf[0x40], 1);
		check_eq("second call: +0x41 untouched (no-op)", buf[0x41], 0x55);
	}

	printf("\n[2] CSTGMIDIClockSync::DisableActivePerfClock()\n");
	{
		unsigned char *buf = mmap32(0x200);
		local_zero(buf, 0x200);
		*(int *)(buf + 0xc8) = 0x1234;
		CSTGMIDIClockSync *m = (CSTGMIDIClockSync *)buf;
		m->DisableActivePerfClock();
		check_eq("+0xc8 set to -1", *(int *)(buf + 0xc8), -1);
	}

	printf("\n[3] CSTGPerformance::SetIsDying(CSTGPerformanceVars*) -- 4 unconditional calls, arg ignored\n");
	{
		unsigned char *perfBuf = mmap32(0x1000);
		local_zero(perfBuf, 0x1000);
		CSTGPerformance *perf = (CSTGPerformance *)perfBuf;

		unsigned char *fpsBuf = mmap32(0x100);
		local_zero(fpsBuf, 0x100);
		CSTGFrontPanelSmoothers::sInstance = (CSTGFrontPanelSmoothers *)fpsBuf;

		unsigned char *mpBuf = mmap32(0x100);
		local_zero(mpBuf, 0x100);
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)mpBuf;

		g_onPerfDeactivateFPS = g_onPerfDeactivateCI = g_onPerfDeactivateAI = 0;
		g_clearUnsolicited = 0;
		g_lastCIThis = g_lastAIThis = 0;

		/* Pass a bogus, never-dereferenced CSTGPerformanceVars* -- the
		 * real body confirmed never touches it. */
		perf->SetIsDying((CSTGPerformanceVars *)0x41414141);

		check_eq("FrontPanelSmoothers::OnPerformanceDeactivate called once", g_onPerfDeactivateFPS, 1);
		check_eq("ControllerInfo::OnPerformanceDeactivate called once", g_onPerfDeactivateCI, 1);
		check_eq("AudioInput::OnPerformanceDeactivate called once", g_onPerfDeactivateAI, 1);
		check_eq("MessageProcessor::ClearUnsolicitedMessages called once", g_clearUnsolicited, 1);
		check_eq("ControllerInfo this == perf+0xad3", (long)((unsigned char *)g_lastCIThis - perfBuf), 0xad3);
		check_eq("AudioInput this == perf+0xae7", (long)((unsigned char *)g_lastAIThis - perfBuf), 0xae7);
	}

	printf("\n[4] CSTGPerformanceVars::SetIsDying() -- entry guard, list walk, state transition\n");
	{
		unsigned char *fpsBuf = mmap32(0x100);
		local_zero(fpsBuf, 0x100);
		CSTGFrontPanelSmoothers::sInstance = (CSTGFrontPanelSmoothers *)fpsBuf;
		unsigned char *mpBuf = mmap32(0x100);
		local_zero(mpBuf, 0x100);
		CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)mpBuf;
		unsigned char *mcsBuf = mmap32(0x100);
		local_zero(mcsBuf, 0x100);
		CSTGMIDIClockSync::sInstance = (CSTGMIDIClockSync *)mcsBuf;

		unsigned char *globalBuf = (unsigned char *)calloc(1, 0x29c9fc0);
		CSTGGlobal::sInstance = (CSTGGlobal *)globalBuf;

		unsigned char *ownerBuf = mmap32(0x1000);

		/* [4a] entry guard: +0x23d1 != 2 -> immediate no-op */
		{
			local_zero(ownerBuf, 0x1000);
			unsigned char *pv = mmap32(0x3000);
			local_zero(pv, 0x3000);
			pv[0x23d1] = 0;	/* not 2 */
			*(unsigned int *)(pv + 0x23d4) = addr32(ownerBuf);
			*(unsigned int *)(globalBuf + 0x29c9900) = 0;	/* empty list */

			g_onPerfDeactivateFPS = 0;
			g_clearUnsolicited = 0;
			((CSTGPerformanceVars *)pv)->SetIsDying();

			check_eq("no-op: state left at 0", pv[0x23d1], 0);
			check_eq("no-op: owner->SetIsDying() never called", g_onPerfDeactivateFPS, 0);
			check_eq("no-op: DisableActivePerfClock never called (+0xc8 untouched)",
				 *(int *)(mcsBuf + 0xc8), 0);
		}

		/* [4b] state==2, empty list -> vacuously "all released": state
		 * becomes 4, +0x23f8 = old +0x23e0, DisableActivePerfClock
		 * called, owner notified. The alloc-count/pushmsg block's own
		 * `oldState <= 1` guard is confirmed unreachable here (oldState
		 * is always 2, re-read right after the already-passed
		 * `== 2` entry guard) -- verified by confirming
		 * STGAPIFrontPanelStatus::sInstance+0x1094 is NOT touched. */
		{
			local_zero(ownerBuf, 0x1000);
			unsigned char *pv = mmap32(0x3000);
			local_zero(pv, 0x3000);
			pv[0x23d1] = 2;
			*(unsigned int *)(pv + 0x23d4) = addr32(ownerBuf);
			*(unsigned int *)(pv + 0x23e0) = 0xdeadbeef;
			*(unsigned int *)(pv + 0x23f8) = 0;
			*(unsigned int *)(globalBuf + 0x29c9900) = 0;	/* empty list */

			unsigned char *panelBuf = mmap32(0x2000);
			local_zero(panelBuf, 0x2000);
			STGAPIFrontPanelStatus::sInstance = panelBuf;
			*(unsigned int *)(panelBuf + 0x1094) = 0x77777777;

			g_onPerfDeactivateFPS = g_onPerfDeactivateCI = g_onPerfDeactivateAI = 0;
			g_clearUnsolicited = 0;
			g_pushMsgCalls = 0;
			*(int *)(mcsBuf + 0xc8) = 0;

			((CSTGPerformanceVars *)pv)->SetIsDying();

			check_eq("empty list: owner->SetIsDying -> all 4 externs fired",
				 g_onPerfDeactivateFPS + g_onPerfDeactivateCI + g_onPerfDeactivateAI + g_clearUnsolicited, 4);
			check_eq("empty list: state committed to 4 (vacuously all released)", pv[0x23d1], 4);
			check_eq("empty list: +0x23f8 = saved +0x23e0", *(unsigned int *)(pv + 0x23f8), 0xdeadbeef);
			check_eq("empty list: DisableActivePerfClock called (+0xc8 == -1)", *(int *)(mcsBuf + 0xc8), -1);
			check_eq("empty list: alloc-count block confirmed UNREACHABLE (front panel count untouched)",
				 *(unsigned int *)(panelBuf + 0x1094), 0x77777777);
			check_eq("empty list: PushUnsolicitedMessage confirmed UNREACHABLE (never called)",
				 g_pushMsgCalls, 0);
		}

		/* [4c] state==2, one matching payload, AreAllKeysAndPedalsReleased()==true
		 * -> allReleased stays true -> state=4, +0x23f8 saved, payload's
		 * own SetIsDying() fired. */
		{
			local_zero(ownerBuf, 0x1000);
			unsigned char *pv = mmap32(0x3000);
			local_zero(pv, 0x3000);
			pv[0x23d0] = 7;	/* groupId */
			pv[0x23d1] = 2;
			*(unsigned int *)(pv + 0x23d4) = addr32(ownerBuf);
			*(unsigned int *)(pv + 0x23e0) = 0x11223344;

			unsigned char *payload = mmap32(0x3000);
			local_zero(payload, 0x3000);
			payload[0x28c8] = 7;	/* matches groupId */
			payload[0x2000] = 1;	/* AreAllKeysAndPedalsReleased() -> true */

			unsigned char *node = mmap32(0x10);
			*(unsigned int *)(node + 0x0) = 0;
			*(unsigned int *)(node + 0x8) = addr32(payload);
			*(unsigned int *)(globalBuf + 0x29c9900) = addr32(node);

			g_areAllKeysCalls = 0;
			*(int *)(mcsBuf + 0xc8) = 0;

			((CSTGPerformanceVars *)pv)->SetIsDying();

			check_eq("matching+released: AreAllKeysAndPedalsReleased called once", g_areAllKeysCalls, 1);
			check_eq("matching+released: payload->SetIsDying fired (+0x40==1)", payload[0x40], 1);
			check_eq("matching+released: state committed to 4", pv[0x23d1], 4);
			check_eq("matching+released: +0x23f8 = saved +0x23e0", *(unsigned int *)(pv + 0x23f8), 0x11223344);
			check_eq("matching+released: DisableActivePerfClock called", *(int *)(mcsBuf + 0xc8), -1);
		}

		/* [4d] state==2, one matching payload, AreAllKeysAndPedalsReleased()==false
		 * -> allReleased becomes false -> state=3, NO +0x23f8 save. */
		{
			local_zero(ownerBuf, 0x1000);
			unsigned char *pv = mmap32(0x3000);
			local_zero(pv, 0x3000);
			pv[0x23d0] = 7;
			pv[0x23d1] = 2;
			*(unsigned int *)(pv + 0x23d4) = addr32(ownerBuf);
			*(unsigned int *)(pv + 0x23e0) = 0x99999999;
			*(unsigned int *)(pv + 0x23f8) = 0x55555555;	/* poison -- must survive untouched */

			unsigned char *payload = mmap32(0x3000);
			local_zero(payload, 0x3000);
			payload[0x28c8] = 7;
			payload[0x2000] = 0;	/* AreAllKeysAndPedalsReleased() -> false */

			unsigned char *node = mmap32(0x10);
			*(unsigned int *)(node + 0x0) = 0;
			*(unsigned int *)(node + 0x8) = addr32(payload);
			*(unsigned int *)(globalBuf + 0x29c9900) = addr32(node);

			((CSTGPerformanceVars *)pv)->SetIsDying();

			check_eq("matching+not-released: payload->SetIsDying still fired", payload[0x40], 1);
			check_eq("matching+not-released: state committed to 3", pv[0x23d1], 3);
			check_eq("matching+not-released: +0x23f8 NOT touched (still poison)",
				 *(unsigned int *)(pv + 0x23f8), 0x55555555);
		}

		/* [4e] non-matching payload (different groupId) -> filtered out,
		 * never called, allReleased stays vacuously true -> state=4. */
		{
			local_zero(ownerBuf, 0x1000);
			unsigned char *pv = mmap32(0x3000);
			local_zero(pv, 0x3000);
			pv[0x23d0] = 7;
			pv[0x23d1] = 2;
			*(unsigned int *)(pv + 0x23d4) = addr32(ownerBuf);

			unsigned char *payload = mmap32(0x3000);
			local_zero(payload, 0x3000);
			payload[0x28c8] = 9;	/* does NOT match groupId 7 */
			payload[0x2000] = 0;	/* would be "not released" if it mattered */

			unsigned char *node = mmap32(0x10);
			*(unsigned int *)(node + 0x0) = 0;
			*(unsigned int *)(node + 0x8) = addr32(payload);
			*(unsigned int *)(globalBuf + 0x29c9900) = addr32(node);

			g_areAllKeysCalls = 0;
			((CSTGPerformanceVars *)pv)->SetIsDying();

			check_eq("non-matching: AreAllKeysAndPedalsReleased never called", g_areAllKeysCalls, 0);
			check_eq("non-matching: payload->SetIsDying never fired (+0x40==0)", payload[0x40], 0);
			check_eq("non-matching: state committed to 4 (vacuous)", pv[0x23d1], 4);
		}

		/* [4f] two matching payloads, first "not released", second
		 * "released" -- proves the AND-fold does NOT short-circuit:
		 * BOTH payloads' own SetIsDying()/AreAllKeysAndPedalsReleased()
		 * must fire regardless of the first result. */
		{
			local_zero(ownerBuf, 0x1000);
			unsigned char *pv = mmap32(0x3000);
			local_zero(pv, 0x3000);
			pv[0x23d0] = 3;
			pv[0x23d1] = 2;
			*(unsigned int *)(pv + 0x23d4) = addr32(ownerBuf);

			unsigned char *payloadA = mmap32(0x3000);
			local_zero(payloadA, 0x3000);
			payloadA[0x28c8] = 3;
			payloadA[0x2000] = 0;	/* not released */

			unsigned char *payloadB = mmap32(0x3000);
			local_zero(payloadB, 0x3000);
			payloadB[0x28c8] = 3;
			payloadB[0x2000] = 1;	/* released */

			/* List order: node for A is head, next -> node for B
			 * (matching this project's own confirmed singly-linked
			 * "+0x0=next" convention). */
			unsigned char *nodeB = mmap32(0x10);
			*(unsigned int *)(nodeB + 0x0) = 0;
			*(unsigned int *)(nodeB + 0x8) = addr32(payloadB);
			unsigned char *nodeA = mmap32(0x10);
			*(unsigned int *)(nodeA + 0x0) = addr32(nodeB);
			*(unsigned int *)(nodeA + 0x8) = addr32(payloadA);
			*(unsigned int *)(globalBuf + 0x29c9900) = addr32(nodeA);

			g_areAllKeysCalls = 0;
			((CSTGPerformanceVars *)pv)->SetIsDying();

			check_eq("no-short-circuit: AreAllKeysAndPedalsReleased called TWICE", g_areAllKeysCalls, 2);
			check_eq("no-short-circuit: payloadA->SetIsDying fired", payloadA[0x40], 1);
			check_eq("no-short-circuit: payloadB->SetIsDying fired", payloadB[0x40], 1);
			check_eq("no-short-circuit: AND-fold (false&true) -> state=3", pv[0x23d1], 3);
		}
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
