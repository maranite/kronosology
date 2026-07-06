// SPDX-License-Identifier: GPL-2.0
/*
 * test_message_processor.cpp -- known-answer test for batch 20:
 *   CSTGDelayedMsgSender::Clear()  (the intrusive active->free list recycle)
 *   CSTGMessageProcessor::ClearUnsolicitedMessages()  (3x Clear at fixed offsets)
 *
 * Every node object AND the sender/MessageProcessor buffer itself is
 * MAP_32BIT-backed: Clear() stores node addresses into 32-bit link fields
 * and stores the sender's own `&sender[0x10]` address into each node's
 * `+0xc` back-ref -- both truncate to wild pointers on this 64-bit host
 * unless the real addresses fit in 32 bits (sec 10.156/10.164).
 *
 * Expected values below were derived by hand-tracing the 131-byte original
 * on paper (three independent list shapes), NOT read back from the C
 * translation under test.
 */

#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "oa_global.h"
#include "oa_engine.h"

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	bool ok = (got == want);
	if (!ok)
		g_fail++;
	printf("  %s  %-58s 0x%x\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted 0x%x)\n", want);
}

static unsigned char *mmap32(unsigned long size)
{
	void *p = mmap(0, size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	if (p == MAP_FAILED) {
		perror("mmap32");
		return 0;
	}
	memset(p, 0, size);
	return (unsigned char *)p;
}

static inline unsigned int U(void *base, unsigned int off)
{
	return *(unsigned int *)((unsigned char *)base + off);
}
static inline void SetU(void *base, unsigned int off, unsigned int v)
{
	*(unsigned int *)((unsigned char *)base + off) = v;
}
static inline unsigned int A32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

int main(void)
{
	printf("CSTGDelayedMsgSender::Clear / CSTGMessageProcessor::ClearUnsolicitedMessages KAT\n");
	printf("==============================================================================\n");

	printf("[1] Clear(): 3-node active list -> empty free list (seed + special-node + counts)\n");
	{
		unsigned char *snd = mmap32(0x40);
		unsigned char *n0 = mmap32(0x20), *n1 = mmap32(0x20), *n2 = mmap32(0x20);

		/* active doubly-linked list n0<->n1<->n2, head=n0 */
		SetU(snd, 0x4, A32(n0));	/* head */
		SetU(snd, 0x8, A32(n2));	/* "special" node == tail: exercises the clear-special branch */
		SetU(snd, 0xc, 3);		/* active count */
		SetU(snd, 0x10, 0);		/* free list empty */
		SetU(snd, 0x14, 0);
		SetU(snd, 0x18, 0);
		SetU(n0, 0x0, A32(n1)); SetU(n0, 0x4, 0);
		SetU(n1, 0x0, A32(n2)); SetU(n1, 0x4, A32(n0));
		SetU(n2, 0x0, 0);        SetU(n2, 0x4, A32(n1));

		((CSTGDelayedMsgSender *)snd)->Clear();

		unsigned int anchor = A32(snd + 0x10);
		check_eq("active head +0x4 emptied", U(snd, 0x4), 0);
		check_eq("special node +0x8 cleared", U(snd, 0x8), 0);
		check_eq("active count +0xc == 0", U(snd, 0xc), 0);
		check_eq("free count +0x18 == 3", U(snd, 0x18), 3);
		check_eq("free tail anchor +0x10 == n2 (last pushed)", U(snd, 0x10), A32(n2));
		check_eq("free head anchor +0x14 == n0 (first pushed)", U(snd, 0x14), A32(n0));
		/* free chain: n2(prev=0) <-> n1 <-> n0(next=0) */
		check_eq("n2->next == n1", U(n2, 0x0), A32(n1));
		check_eq("n2->prev == 0",  U(n2, 0x4), 0);
		check_eq("n2->backref == &snd[0x10]", U(n2, 0xc), anchor);
		check_eq("n1->next == n0", U(n1, 0x0), A32(n0));
		check_eq("n1->prev == n2", U(n1, 0x4), A32(n2));
		check_eq("n1->backref == &snd[0x10]", U(n1, 0xc), anchor);
		check_eq("n0->next == 0",  U(n0, 0x0), 0);
		check_eq("n0->prev == n1", U(n0, 0x4), A32(n1));
		check_eq("n0->backref == &snd[0x10]", U(n0, 0xc), anchor);
	}

	printf("\n[2] Clear(): 1 active node onto a PRE-EXISTING free list (tprev!=0 splice line)\n");
	{
		unsigned char *snd = mmap32(0x40);
		unsigned char *T = mmap32(0x20), *P = mmap32(0x20), *A = mmap32(0x20);

		/* pre-existing free list: ... P <-> T, with T the tail anchor (+0x10). */
		SetU(T, 0x0, 0);        SetU(T, 0x4, A32(P));
		SetU(P, 0x0, A32(T));   SetU(P, 0x4, 0);
		SetU(snd, 0x10, A32(T));	/* free tail anchor */
		SetU(snd, 0x14, A32(P));
		SetU(snd, 0x18, 1);

		/* one active node A */
		SetU(snd, 0x4, A32(A));		/* head */
		SetU(snd, 0x8, 0);
		SetU(snd, 0xc, 1);
		SetU(A, 0x0, 0); SetU(A, 0x4, 0);

		((CSTGDelayedMsgSender *)snd)->Clear();

		unsigned int anchor = A32(snd + 0x10);
		check_eq("active head emptied", U(snd, 0x4), 0);
		check_eq("active count 0", U(snd, 0xc), 0);
		check_eq("free count incremented to 2", U(snd, 0x18), 2);
		check_eq("free tail anchor now A", U(snd, 0x10), A32(A));
		/* the tprev!=0 line: T->prev(P)->next must be repointed to A */
		check_eq("P->next repointed to A (tprev!=0 splice)", U(P, 0x0), A32(A));
		check_eq("T->prev == A", U(T, 0x4), A32(A));
		check_eq("A->prev == P (old tail->prev)", U(A, 0x4), A32(P));
		check_eq("A->next == T (old free tail)", U(A, 0x0), A32(T));
		check_eq("A->backref == &snd[0x10]", U(A, 0xc), anchor);
	}

	printf("\n[3] ClearUnsolicitedMessages(): all 3 senders (+0x6c/+0x608/+0xb24) cleared\n");
	{
		unsigned char *mp = mmap32(0x1040);
		CSTGMessageProcessor *proc = (CSTGMessageProcessor *)mp;
		const unsigned int offs[3] = { 0x6c, 0x608, 0xb24 };
		unsigned char *nodes[3];

		for (int i = 0; i < 3; i++) {
			unsigned char *snd = mp + offs[i];
			unsigned char *node = mmap32(0x20);
			nodes[i] = node;
			SetU(snd, 0x4, A32(node));	/* 1-node active list */
			SetU(snd, 0x8, 0);
			SetU(snd, 0xc, 1);
			SetU(snd, 0x10, 0);
			SetU(snd, 0x14, 0);
			SetU(snd, 0x18, 0);
			SetU(node, 0x0, 0); SetU(node, 0x4, 0);
		}

		proc->ClearUnsolicitedMessages();

		for (int i = 0; i < 3; i++) {
			unsigned char *snd = mp + offs[i];
			char label[96];
			snprintf(label, sizeof(label), "sender +0x%x: active head cleared", offs[i]);
			check_eq(label, U(snd, 0x4), 0);
			snprintf(label, sizeof(label), "sender +0x%x: active count 0", offs[i]);
			check_eq(label, U(snd, 0xc), 0);
			snprintf(label, sizeof(label), "sender +0x%x: free count 1", offs[i]);
			check_eq(label, U(snd, 0x18), 1);
			snprintf(label, sizeof(label), "sender +0x%x: free tail anchor == node", offs[i]);
			check_eq(label, U(snd, 0x10), A32(nodes[i]));
			snprintf(label, sizeof(label), "sender +0x%x: node backref == &snd[0x10]", offs[i]);
			check_eq(label, U(nodes[i], 0xc), A32(snd + 0x10));
		}
	}

	printf("==============================================================================\n");
	if (g_fail) {
		printf("RESULT: %d check(s) FAILED\n", g_fail);
		return 1;
	}
	printf("RESULT: all checks passed\n");
	return 0;
}
