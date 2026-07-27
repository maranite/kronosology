/*
 * test_limiter_base.cpp  -  host-side known-answer test for CLimiterBase +
 * CWrProtCircularQueue (limiter_base.h, Eva "size is not depth" re-check batch,
 * 2026-07-27).
 *
 * CLimiterBase itself has zero real callers anywhere in ground truth (see
 * limiter_base.h's own REACHABILITY note) -- these are pure structural/host KAT
 * checks of the Tier-A pieces (ctors/dtors/Init(int)/IsEmpty/CountIntegers,
 * SendWithAnswer/SendNoAnswer's real forward shape), not a reachability claim.
 *
 * Checks:
 *   [1] CLimiterBase::CLimiterBase(): installs its own vtable, mIfcLink starts
 *       NULL, mInitAttempted forced to 4 iff either callback is null (0 with
 *       both callbacks present)
 *   [2] CWrProtCircularQueue::Init(int): sizeShift<=6 -> fixed 0x80 capacity;
 *       sizeShift>6 -> (1<<sizeShift)&~3; idempotent (2nd call returns false)
 *   [3] CWrProtCircularQueue::IsEmpty(): true right after construction
 *       (mReadPtr==mWritePtr), false once mWritePtr is advanced
 *   [4] CWrProtCircularQueue::CountIntegers(): dword-padded word count
 *   [5] CLimiterBase::SendWithAnswer()/SendNoAnswer(): both are real forwards
 *       into Write() (confirmed observable here since Write() is a Tier-B stub
 *       that always returns false -- SendWithAnswer() must therefore also
 *       return false, and SendNoAnswer() must not crash calling through a const
 *       void* data pointer)
 */

#include <cstdio>
#include "limiter_base.h"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(const char *name, bool cond)
{
	++g_checks;
	if (!cond) {
		++g_failed;
		std::printf("FAIL: %s\n", name);
	}
}

} // namespace

/* The one friend this header grants both CLimiterBase and its private nested
 * CWrProtCircularQueue to (limiter_base.h) -- all private-member access for this
 * whole test file has to go through here, including the queue-level checks
 * (CWrProtCircularQueue itself is a private nested type, so nothing outside this
 * struct can even name it).
 */
struct LimiterBaseTestHooks {
	static void *Vtbl(const CLimiterBase &b)
	{
		return *reinterpret_cast<void *const *>(&b);
	}
	static void *IfcLink(const CLimiterBase &b)
	{
		return *reinterpret_cast<void *const *>(reinterpret_cast<const char *>(&b) + 0x4);
	}
	static int InitAttempted(const CLimiterBase &b)
	{
		return *reinterpret_cast<const int *>(reinterpret_cast<const char *>(&b) + 0x28);
	}

	typedef CLimiterBase::CWrProtCircularQueue Queue;

	static Queue &EmbeddedQueue(CLimiterBase &b) { return b.mQueue; }
	static bool QueueInit(Queue &q, int shift) { return q.Init(shift); }
	static bool QueueIsEmpty(const Queue &q) { return q.IsEmpty(); }
	static void QueueAdvanceWrite(Queue &q) { q.mWritePtr += 1; }

	/* mLimit - mBase, i.e. the real allocated capacity -- CWrProtCircularQueue
	 * is deliberately non-copyable (see its own header comment), so every check
	 * here has to work through a real CLimiterBase's own embedded instance
	 * rather than a standalone by-value one.
	 */
	static long QueueCapacity(const Queue &q) { return q.mLimit - q.mBase; }
};

static bool DummyUnmarshall(unsigned char, void *, unsigned int, CIfcUnknown *)
{
	return true;
}
static bool DummySend(unsigned char, void *, unsigned int, CIfcUnknown *)
{
	return true;
}

int main()
{
	CLimiterBase limNoCallbacks(3, 5, 0, 0);
	check("ctor: vtbl installed", LimiterBaseTestHooks::Vtbl(limNoCallbacks) != 0);
	check("ctor: mIfcLink starts NULL", LimiterBaseTestHooks::IfcLink(limNoCallbacks) == 0);
	check("ctor: mInitAttempted forced to 4 when both callbacks null",
	      LimiterBaseTestHooks::InitAttempted(limNoCallbacks) == 4);

	CLimiterBase limWithCallbacks(2, 9, DummyUnmarshall, DummySend);
	check("ctor: mInitAttempted stays 0 when both callbacks present",
	      LimiterBaseTestHooks::InitAttempted(limWithCallbacks) == 0);

	/* limWithCallbacks's own ctor (sizeShift=2) already built its embedded
	 * queue's buffer at construction time (the ctor duplicates Init()'s own
	 * logic inline, per header comment) -- so a further Init() call on that
	 * SAME queue must be a no-op (idempotency guard: mBase already set).
	 */
	LimiterBaseTestHooks::Queue &q1 = LimiterBaseTestHooks::EmbeddedQueue(limWithCallbacks);
	check("Init(): idempotent, 2nd call (after ctor's own inline build) returns false",
	      !LimiterBaseTestHooks::QueueInit(q1, 2));
	check("IsEmpty(): true right after construction", LimiterBaseTestHooks::QueueIsEmpty(q1));
	LimiterBaseTestHooks::QueueAdvanceWrite(q1);
	check("IsEmpty(): false once mWritePtr advances", !LimiterBaseTestHooks::QueueIsEmpty(q1));

	/* 2 more CLimiterBase instances (their ctor duplicates Init(int)'s own
	 * capacity logic inline -- header comment) to check both real capacity
	 * branches through their own embedded queues. CWrProtCircularQueue is
	 * deliberately non-copyable (own header comment), so this is the only
	 * safe way to exercise a queue with a specific sizeShift on a host KAT.
	 */
	CLimiterBase limSmall(6, 1, DummyUnmarshall, DummySend);
	check("ctor: sizeShift<=6 -> fixed 0x80 capacity",
	      LimiterBaseTestHooks::QueueCapacity(LimiterBaseTestHooks::EmbeddedQueue(limSmall)) == 0x80);

	CLimiterBase limShifted(10, 1, DummyUnmarshall, DummySend);
	check("ctor: sizeShift>6 -> (1<<sizeShift)&~3",
	      LimiterBaseTestHooks::QueueCapacity(LimiterBaseTestHooks::EmbeddedQueue(limShifted)) ==
	          ((1 << 10) & ~3));

	check("CountIntegers(0) == 0", LimiterBaseTestHooks::Queue::CountIntegers(0) == 0);
	check("CountIntegers(4) == 1", LimiterBaseTestHooks::Queue::CountIntegers(4) == 1);
	check("CountIntegers(5) == 2 (dword-padded)",
	      LimiterBaseTestHooks::Queue::CountIntegers(5) == 2);
	check("CountIntegers(8) == 2", LimiterBaseTestHooks::Queue::CountIntegers(8) == 2);

	/* [5] SendWithAnswer()/SendNoAnswer() real-forward shape */
	unsigned char payload[4] = {1, 2, 3, 4};
	check("SendWithAnswer(): real forward into Write() (Tier-B stub, always false)",
	      limWithCallbacks.SendWithAnswer(0x10, payload, sizeof(payload)) == false);
	limWithCallbacks.SendNoAnswer(0x11, payload, sizeof(payload)); /* must not crash */
	check("SendNoAnswer(): completed without crashing", true);

	std::printf("%d checks, %d failed\n", g_checks, g_failed);
	return g_failed != 0 ? 1 : 0;
}
