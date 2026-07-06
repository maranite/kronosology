// SPDX-License-Identifier: GPL-2.0
/*
 * test_load_balancer_static.cpp  -  KAT for batch 18's
 * src/engine/load_balancer_static.cpp: CSTGSlotVoiceData::EnableSlot(),
 * CLoadBalancer::BalanceStaticLoadHelper(...), and CLoadBalancer::
 * BalanceStaticLoad().
 *
 * Mocks CSTGSlotVoiceData::GetPatchStaticCosts()/GetTotalStaticCosts()
 * (both still deliberately-deferred stubs elsewhere -- their own real
 * bodies dispatch through a not-yet-reconstructed effect-slot vtable
 * cluster) and PushUnsolicitedMessage() as simple, controllable
 * call-recorders, keeping this KAT focused on the three newly-real
 * functions' own logic. Per-candidate cost values are looked up via a
 * small "candidate id" byte stashed at a scratch offset (+0x2000, well
 * away from every confirmed-real CSTGSlotVoiceData field this project
 * has documented so far) rather than a single global -- lets the
 * BalanceStaticLoad() end-to-end sections exercise more than one
 * distinct candidate at once.
 *
 * Every object whose own address is round-tripped through a packed
 * 32-bit field (list nodes, the CSTGGlobal active-voice-data lists) is
 * mmap32()/MAP_32BIT-backed, matching this project's established
 * pointer-width discipline for a 64-bit host reconstructing a 32-bit
 * target.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_setup_global_resources.h"

#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>

/* No <cstring> here -- it conflicts with oa_internal.h's own `strlen`
 * declaration (different exception specifier), the same reasoning
 * test_slot_voice_data_free.cpp/setup_global_resources.cpp/
 * stgheap_init.cpp already established. */
static void local_copy(void *dst, const void *src, unsigned long n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	for (unsigned long i = 0; i < n; i++)
		d[i] = s[i];
}

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

/* ---- mocks ---- */

CSTGGlobal *CSTGGlobal::sInstance;
CLoadBalancer *CLoadBalancer::sInstance;
CSTGAudioManager *CSTGAudioManager::sInstance;

static int g_pushMsgCalls;
static unsigned char g_lastMsg[0x14];
extern "C" void PushUnsolicitedMessage(void *msg)
{
	g_pushMsgCalls++;
	local_copy(g_lastMsg, msg, sizeof(g_lastMsg));
}

/* Per-candidate-id cost table for the mocked GetPatchStaticCosts()/
 * GetTotalStaticCosts(). Candidate id is stashed at +0x2000. */
#define MAX_CAND 4
static unsigned long g_patchA[MAX_CAND][2];
static unsigned long g_patchB[MAX_CAND][2];
static unsigned long g_totalA[MAX_CAND];
static unsigned long g_totalB[MAX_CAND];
static int g_getPatchStaticCostsCalls;
static int g_getTotalStaticCostsCalls;

static unsigned char CandId(const CSTGSlotVoiceData *cand)
{
	return ((const unsigned char *)cand)[0x2000];
}

void CSTGSlotVoiceData::GetPatchStaticCosts(unsigned int busIndex, unsigned long *out1, unsigned long *out2) const
{
	g_getPatchStaticCostsCalls++;
	unsigned char id = CandId(this);
	*out1 = g_patchA[id][busIndex];
	*out2 = g_patchB[id][busIndex];
}

void CSTGSlotVoiceData::GetTotalStaticCosts(unsigned long *out1, unsigned long *out2) const
{
	g_getTotalStaticCostsCalls++;
	unsigned char id = CandId(this);
	*out1 = g_totalA[id];
	*out2 = g_totalB[id];
}

int main(void)
{
	printf("[1] CSTGSlotVoiceData::EnableSlot() -- no-op vs full effect\n");
	{
		unsigned char *buf = mmap32(0x3000);
		local_zero(buf, 0x3000);
		CSTGSlotVoiceData *v = (CSTGSlotVoiceData *)buf;

		unsigned char *program = mmap32(0x100);
		local_zero(program, 0x100);
		*(unsigned int *)(buf + 0x34) = addr32(program);

		/* [1a] +0x28c4 == 0 -> no-op */
		*(unsigned int *)(buf + 0x28c4) = 0;
		program[0x45] = 0x00;
		g_pushMsgCalls = 0;
		v->EnableSlot();
		check_eq("no-op: PushUnsolicitedMessage not called", g_pushMsgCalls, 0);
		check_eq("no-op: program+0x45 untouched", program[0x45], 0x00);

		/* [1b] +0x28c4 != 0 -> full effect */
		*(unsigned int *)(buf + 0x28c4) = 2;	/* any nonzero value */
		program[0x45] = 0x05;	/* some pre-existing low bits, must survive OR */
		program[0x4] = 0x37;	/* patch byte, becomes msg param1 */
		g_pushMsgCalls = 0;
		v->EnableSlot();
		check_eq("full: +0x28c4 cleared", *(unsigned int *)(buf + 0x28c4), 0);
		check_eq("full: program+0x45 bit 0x80 set, low bits preserved", program[0x45], 0x85);
		check_eq("full: PushUnsolicitedMessage called once", g_pushMsgCalls, 1);
		check_eq("full: msg size word == 0x14", *(unsigned short *)(g_lastMsg + 0x0), 0x14);
		check_eq("full: msg type word == 1", *(unsigned short *)(g_lastMsg + 0x2), 1);
		check_eq("full: msg dword@4 == 0", *(unsigned int *)(g_lastMsg + 0x4), 0);
		check_eq("full: msg opcode dword@8 == 0x14", *(unsigned int *)(g_lastMsg + 0x8), 0x14);
		check_eq("full: msg param1 dword@c == patch byte (0x37)", *(unsigned int *)(g_lastMsg + 0xc), 0x37);
		check_eq("full: msg param2 dword@10 == 1", *(unsigned int *)(g_lastMsg + 0x10), 1);
	}

	printf("\n[2] CLoadBalancer::BalanceStaticLoadHelper -- busCount==0\n");
	{
		CLoadBalancer *lb = (CLoadBalancer *)mmap32(sizeof(CLoadBalancer) + 0x200);
		unsigned char *amBuf = mmap32(0x40);
		local_zero(amBuf, 0x40);
		*(unsigned int *)(amBuf + 0x18) = 0;	/* busCount == 0 */
		CSTGAudioManager::sInstance = (CSTGAudioManager *)amBuf;

		unsigned char *candBuf = mmap32(0x3000);
		local_zero(candBuf, 0x3000);
		candBuf[0x2000] = 0;	/* candidate id 0 */
		CSTGSlotVoiceData *cand = (CSTGSlotVoiceData *)candBuf;
		g_patchA[0][0] = 10; g_patchB[0][0] = 20;
		g_patchA[0][1] = 30; g_patchB[0][1] = 40;

		unsigned long distA[4] = { 0, 0, 0, 0 };
		unsigned long distB[4] = { 0, 0, 0, 0 };
		unsigned long sumA = 0, sumB = 0;
		g_getPatchStaticCostsCalls = 0;

		lb->BalanceStaticLoadHelper(cand, distA, distB, &sumA, &sumB);

		check_eq("busCount==0: GetPatchStaticCosts called twice (busIndex 0,1)", g_getPatchStaticCostsCalls, 2);
		/* busIndex 0: distA[0] += 10 (chosenPtr==&distA[0]), distB[0] += 20 (esiFinal==&distB[0]) */
		/* busIndex 1: distA[0] += 30, distB[0] += 40 (SAME indices both times, per the disassembly's own busCount==0 special case) */
		check_eq("busCount==0: distA[0] == 10+30", (long)distA[0], 40);
		check_eq("busCount==0: distB[0] == 20+40", (long)distB[0], 60);
		check_eq("busCount==0: distA[1..3] untouched", (long)(distA[1] + distA[2] + distA[3]), 0);
		check_eq("busCount==0: sumA == 10+30", (long)sumA, 40);
		check_eq("busCount==0: sumB == 20+40", (long)sumB, 60);
		check_eq("busCount==0: candidate+0x28dc byte == 0 (bestIdx)", candBuf[0x28dc], 0);
		check_eq("busCount==0: candidate+0x28dd byte == 0 (bestIdx, busIndex 1)", candBuf[0x28dd], 0);
		check_eq("busCount==0: candidate+0x28de byte == 0 (scanIdx)", candBuf[0x28de], 0);
		check_eq("busCount==0: candidate+0x28df byte == 0 (scanIdx, busIndex 1)", candBuf[0x28df], 0);
	}

	printf("\n[3] CLoadBalancer::BalanceStaticLoadHelper -- busCount==1 (trivial single-bus)\n");
	{
		CLoadBalancer *lb = (CLoadBalancer *)mmap32(sizeof(CLoadBalancer) + 0x200);
		unsigned char *amBuf = mmap32(0x40);
		local_zero(amBuf, 0x40);
		*(unsigned int *)(amBuf + 0x18) = 1;
		CSTGAudioManager::sInstance = (CSTGAudioManager *)amBuf;

		unsigned char *candBuf = mmap32(0x3000);
		local_zero(candBuf, 0x3000);
		candBuf[0x2000] = 0;
		CSTGSlotVoiceData *cand = (CSTGSlotVoiceData *)candBuf;
		g_patchA[0][0] = 5; g_patchB[0][0] = 7;
		g_patchA[0][1] = 9; g_patchB[0][1] = 11;

		unsigned long distA[4] = { 100, 0, 0, 0 };
		unsigned long distB[4] = { 200, 0, 0, 0 };
		unsigned long sumA = 0, sumB = 0;

		lb->BalanceStaticLoadHelper(cand, distA, distB, &sumA, &sumB);

		/* With only 1 bus, both bestIdx and scanIdx always resolve to
		 * index 0 for every busIndex pass -- both out1 and out2 land
		 * in slot 0 of their respective arrays every time. */
		check_eq("busCount==1: distA[0] == 100+5+9", (long)distA[0], 114);
		check_eq("busCount==1: distB[0] == 200+7+11", (long)distB[0], 218);
		check_eq("busCount==1: sumA == 5+9", (long)sumA, 14);
		check_eq("busCount==1: sumB == 7+11", (long)sumB, 18);
		check_eq("busCount==1: bestIdx byte (busIndex 0) == 0", candBuf[0x28dc], 0);
		check_eq("busCount==1: scanIdx byte (busIndex 0) == 0", candBuf[0x28de], 0);
	}

	printf("\n[4] CLoadBalancer::BalanceStaticLoadHelper -- busCount==2, distB monotonically decreasing (bestIdx != scanIdx)\n");
	{
		CLoadBalancer *lb = (CLoadBalancer *)mmap32(sizeof(CLoadBalancer) + 0x200);
		unsigned char *amBuf = mmap32(0x40);
		local_zero(amBuf, 0x40);
		*(unsigned int *)(amBuf + 0x18) = 2;
		CSTGAudioManager::sInstance = (CSTGAudioManager *)amBuf;

		unsigned char *candBuf = mmap32(0x3000);
		local_zero(candBuf, 0x3000);
		candBuf[0x2000] = 0;
		CSTGSlotVoiceData *cand = (CSTGSlotVoiceData *)candBuf;
		/* Only busIndex 0 exercised meaningfully here -- busIndex 1
		 * re-runs the same scan shape a second time, already covered
		 * by [2]/[3] above, so zero its own contribution to keep this
		 * section's arithmetic simple. */
		g_patchA[0][0] = 1; g_patchB[0][0] = 1;
		g_patchA[0][1] = 0; g_patchB[0][1] = 0;

		/* distA = [100, 50]: bus 1 has the smaller load -> bestIdx == 1.
		 * distB = [10, 3]: strictly decreasing all the way to the last
		 * element -> the scan never "settles" mid-way, so scanIdx stays
		 * at the LAST value the scan settled on before it started
		 * strictly decreasing again, which is index 0 (hand-traced,
		 * see load_balancer_static.cpp's own header comment). */
		unsigned long distA[4] = { 100, 50, 0, 0 };
		unsigned long distB[4] = { 10, 3, 0, 0 };
		unsigned long sumA = 0, sumB = 0;

		lb->BalanceStaticLoadHelper(cand, distA, distB, &sumA, &sumB);

		check_eq("busCount==2: bestIdx byte (busIndex 0) == 1", candBuf[0x28dc], 1);
		check_eq("busCount==2: scanIdx byte (busIndex 0) == 0", candBuf[0x28de], 0);
		check_eq("busCount==2: distA[1] += 1 (bestIdx==1)", (long)distA[1], 51);
		check_eq("busCount==2: distA[0] unchanged", (long)distA[0], 100);
		check_eq("busCount==2: distB[0] += 1 (scanIdx==0)", (long)distB[0], 11);
		check_eq("busCount==2: distB[1] unchanged", (long)distB[1], 3);
	}

	printf("\n[5] CLoadBalancer::BalanceStaticLoad() -- end to end, phase 1 accumulate + phase 2 greedy enable\n");
	{
		CLoadBalancer *lb = (CLoadBalancer *)mmap32(sizeof(CLoadBalancer) + 0x200);
		local_zero((void *)lb, sizeof(CLoadBalancer) + 0x200);
		CLoadBalancer::sInstance = lb;
		*(unsigned int *)((unsigned char *)lb + 0x8c) = 100;	/* budget */

		unsigned char *amBuf = mmap32(0x40);
		local_zero(amBuf, 0x40);
		*(unsigned int *)(amBuf + 0x18) = 1;	/* busCount == 1, keeps the arithmetic simple */
		CSTGAudioManager::sInstance = (CSTGAudioManager *)amBuf;

		CSTGGlobal *g = (CSTGGlobal *)mmap32(0x2a00000);
		local_zero((void *)g, 0x2a00000);
		CSTGGlobal::sInstance = g;
		unsigned char *gBase = (unsigned char *)g;

		/* Phase-1 candidate (id 1): qualifies for the +0x29c9900 list
		 * scan (subType 1, mode 0, +0x42 clear) and contributes
		 * A=20,B=5 into the running totals -- but is NEVER itself a
		 * phase-2 candidate (it's not on the 16-entry table), so
		 * EnableSlot() must never be called on it. */
		unsigned char *cand1 = mmap32(0x3000);
		local_zero(cand1, 0x3000);
		cand1[0x2000] = 1;
		g_patchA[1][0] = 20; g_patchB[1][0] = 5;
		unsigned char *prog1 = mmap32(0x100);
		local_zero(prog1, 0x100);
		prog1[0xd] = 1;
		*(unsigned int *)(cand1 + 0x34) = addr32(prog1);
		cand1[0x42] = 0;
		*(unsigned int *)(cand1 + 0x28c4) = 0;

		unsigned char *node1 = mmap32(0x10);
		*(unsigned int *)(node1 + 0x0) = 0;		/* next == NULL, single-entry list */
		*(unsigned int *)(node1 + 0x8) = addr32(cand1);
		*(unsigned int *)(gBase + 0x29c9900) = addr32(node1);

		/* Phase-2 candidate A (id 2): fits within budget (committed
		 * 20+5=25 so far, own total 30+10=40, sum=65 <= 100) ->
		 * EnableSlot() must fire, and its own cost (30/10) must then
		 * feed into the running totals via a second Helper call. */
		unsigned char *cand2 = mmap32(0x3000);
		local_zero(cand2, 0x3000);
		cand2[0x2000] = 2;
		g_totalA[2] = 30; g_totalB[2] = 10;
		g_patchA[2][0] = 30; g_patchB[2][0] = 10;
		unsigned char *prog2 = mmap32(0x100);
		local_zero(prog2, 0x100);
		prog2[0xd] = 2;
		*(unsigned int *)(cand2 + 0x34) = addr32(prog2);
		cand2[0x40] = 0;
		*(unsigned int *)(cand2 + 0x28c4) = 1;	/* mode MUST be exactly 1 for phase 2 */

		/* Phase-2 candidate B (id 3): would push the running total
		 * over budget (committed becomes 25+40=65 after candidate A;
		 * own total 40+10=50, sum=115 > 100) -> must be SKIPPED,
		 * EnableSlot() must NOT fire, and its own cost must NOT be
		 * added to the running totals either. */
		unsigned char *cand3 = mmap32(0x3000);
		local_zero(cand3, 0x3000);
		cand3[0x2000] = 3;
		g_totalA[3] = 40; g_totalB[3] = 10;
		unsigned char *prog3 = mmap32(0x100);
		local_zero(prog3, 0x100);
		prog3[0xd] = 1;
		*(unsigned int *)(cand3 + 0x34) = addr32(prog3);
		cand3[0x40] = 0;
		*(unsigned int *)(cand3 + 0x28c4) = 1;

		unsigned char *entry2 = mmap32(0x10);
		*(unsigned int *)(entry2 + 0x8) = addr32(cand2);
		unsigned char *entry3 = mmap32(0x10);
		*(unsigned int *)(entry3 + 0x8) = addr32(cand3);
		*(unsigned int *)(gBase + 0x29c990c + 0 * 12) = addr32(entry2);
		*(unsigned int *)(gBase + 0x29c990c + 1 * 12) = addr32(entry3);
		/* Remaining 14 table slots stay NULL (zeroed above) -> skipped. */

		g_getTotalStaticCostsCalls = 0;
		g_pushMsgCalls = 0;
		lb->BalanceStaticLoad();

		check_eq("cand2 (fits budget): +0x28c4 flag cleared by EnableSlot()", *(unsigned int *)(cand2 + 0x28c4), 0);
		check_eq("cand3 (exceeds budget): +0x28c4 flag left untouched (still 1)", *(unsigned int *)(cand3 + 0x28c4), 1);
		check_eq("GetTotalStaticCosts called exactly twice (both phase-2 table entries)", g_getTotalStaticCostsCalls, 2);
		check_eq("EnableSlot's own PushUnsolicitedMessage fired exactly once (cand2 only)", g_pushMsgCalls, 1);
	}

	printf("=========================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
