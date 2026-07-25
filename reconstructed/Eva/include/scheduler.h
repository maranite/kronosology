/*
 * scheduler.h  -  CScheduler, Eva's task-level scheduler (Stage 4).
 *
 * CKernel::CKernel() placement-constructs one of these (g_poScheduler, ckernel.cpp) and
 * CKernel::InitSystemLayer() drives its level bring-up (7x InsertLevel + Enable(1)).
 * Real layout confirmed from CScheduler@08062380.c (ctor) + InsertLevel/Enable's own
 * field offsets:
 *   +0x00  mBusy          "currently processing" scratch, always written 0 by every
 *                          reconstructed method that touches it; never read back
 *   +0x04  mLevels         embedded COmegaPtrArray (0x18 bytes), vtable-swapped to
 *                          CLevelManagerArray by the ctor -- this is the array
 *                          InsertLevel()'s own CLevelManagerArray::Find()/Add() work on
 *   +0x1c  mUnusedA        real field is "mEnabled" (InsertLevel() saves/zeroes/
 *                          restores it around the level-insert critical section;
 *                          Enable(int) is literally `mEnabled = enable`). Kept the
 *                          original placeholder field name since neither reconstructed
 *                          method needed a better one internally -- see scheduler.cpp.
 *   +0x20  mUnusedB        cleared by Enable() on the 0->1 transition; not otherwise
 *                          read/written by any reconstructed method
 *   +0x24  mReady          set 1 once Enable(1) has run for the first time; gates the
 *                          WriteMessageToHost(3, 0x1c) notification in both
 *                          InsertLevel() and Enable()
 *   +0x28  mNotifyHost     checked (never written by any reconstructed method) before
 *                          the WriteMessageToHost(3, 0x1c) call fires -- real setter
 *                          not traced
 *
 * CScheduler::Exec() (.text+0x080623e0, 1025 bytes) is now reconstructed (Stage 6
 * breadth sweep, 2026-07-25): a faithful per-tick walk over CLevelManagerArray's now-
 * real, sorted-by-level array, decrementing each CLevelManager's own countdown and
 * calling RunLevel() when it reaches 0. See scheduler.cpp for the full field-offset
 * writeup (also fixes this file's own former mistranscribed CLevelManagerArray::Add/
 * Find addresses -- were off by an inserted digit, real addresses 0x0805ec70/
 * 0x0805ee90, corrected against functions.csv). CLevelManager::RunLevel() itself stays
 * Tier-B (declared below) -- its own real body depends on CTaskBuffer (a wholly new,
 * unintroduced class) and dispatches through each queued CModule's own vtable slot +8
 * ("Update"), which would pull in this project's entire per-module task-queue
 * substrate -- out of scope for this pass, deferred to a future breadth-sweep batch.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

class CLevelManager;

class CScheduler {
public:
	CScheduler();

	/* .text+0x08062b40, 316 bytes. Real return type is CLevelManager* (the newly
	 * inserted level, or the existing one if already present) -- collapsed to void
	 * since no reconstructed caller (CKernel::InitSystemLayer) uses the result.
	 */
	void InsertLevel(int level);

	/* .text+0x08063120, 119 bytes. */
	void Enable(int enable);

	/* .text+0x080623e0, 1025 bytes -- reconstructed (see header comment + scheduler.cpp).
	 * Called once per scheduling-signal wakeup from CKernel::Exec().
	 */
	void Exec();

	/* .text+0x080631c0, size not measured -- Tier-B link-stub, not reconstructed.
	 * Called once from CKernel::InitUserLayer().
	 */
	void EnableUpdate(int enable);

private:
	int   mBusy;      /* +0x00 */
	char  mLevels[0x18]; /* +0x04, embedded COmegaPtrArray -> CLevelManagerArray */
	int   mUnusedA;   /* +0x1c */
	int   mUnusedB;   /* +0x20 */
	int   mReady;     /* +0x24 */
	int   mNotifyHost; /* +0x28 */
};

#endif /* SCHEDULER_H */
